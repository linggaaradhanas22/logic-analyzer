"""End-to-end client flows against the mock analyzer."""

from __future__ import annotations

import time
import threading

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient, AnalyzerError, account_test_records
from host.la_cli.transport import TransportError, open_uri
from host.tests.mock_analyzer import counter_samples


def _client(uri: str) -> AnalyzerClient:
    return AnalyzerClient(open_uri(uri))


def _wait_until(predicate, timeout_s: float = 2.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.005)
    raise AssertionError("condition not reached before timeout")


def test_ping(analyzer) -> None:
    mock = analyzer("ping")
    client = _client(mock.uri)
    try:
        result = client.ping()
    finally:
        client.close()
    assert result["fw_version"] == 1
    assert result["pl_id"] == 0x4C410002
    assert result["uptime_ms"] == 42


def test_ping_resyncs_after_noise(analyzer) -> None:
    mock = analyzer("noise_then_pong")
    client = _client(mock.uri)
    try:
        result = client.ping()
    finally:
        client.close()
    assert result["pl_id"] == 0x4C410002
    assert client.parser.dropped_bytes >= 4


def test_status(analyzer) -> None:
    mock = analyzer("status")
    client = _client(mock.uri)
    try:
        result = client.status()
    finally:
        client.close()
    assert result["state"] == 3
    assert result["status_reg"] == 0x00000008
    assert result["word_count"] == 4096
    assert result["decoders"][0]["events"] == 10
    assert result["decoders"][2]["dropped_segments"] == 1


def test_status_ignores_stale_nak(analyzer) -> None:
    mock = analyzer("stale_nak_status")
    client = _client(mock.uri)
    try:
        result = client.status()
    finally:
        client.close()
    assert result["state"] == 3


def test_patgen_cfg_acknowledged(analyzer) -> None:
    mock = analyzer("ping")
    client = _client(mock.uri)
    try:
        ctrl = p.PATGEN_EN_MASK | (p.PATGEN_MODE_COUNTER << p.PATGEN_MODE_SHIFT)
        client.configure_patgen(ctrl)
    finally:
        client.close()


def test_link_cfg_acknowledged(analyzer) -> None:
    mock = analyzer("ping")
    client = _client(mock.uri)
    try:
        client.configure_link(p.LINK_SPI)
        assert mock.last_link_transport == p.LINK_SPI
        client.configure_link(p.LINK_UART)
        assert mock.last_link_transport == p.LINK_UART
    finally:
        client.close()


def test_arm_acks_config_and_arm(analyzer) -> None:
    mock = analyzer("ping")
    client = _client(mock.uri)
    try:
        client.arm(p.pack_config(post_samples=1024), timeout_s=2.0)
    finally:
        client.close()


def test_capture_raw_transfer(analyzer) -> None:
    mock = analyzer("capture_raw")
    client = _client(mock.uri)
    try:
        result = client.capture(p.pack_config(post_samples=3056), timeout_s=5.0)
    finally:
        client.close()
    assert result.data == counter_samples(3056)
    assert result.encoding == 0
    assert result.meta["total_words"] == 3056 // 4
    assert result.chunks == 1  # one chunk holds all 3056 bytes
    assert result.parser_stats["crc_errors"] == 0
    # Every chunk must be acknowledged (batches of 16 plus the end drain);
    # the mock thread needs a moment to drain its socket.
    _wait_until(lambda: len(mock.received_acks) == 1)
    assert [t for t, _ in mock.received_acks] == [p.DATA_CHUNK]


def test_capture_txn_transfer(analyzer) -> None:
    mock = analyzer("capture_txn")
    client = _client(mock.uri)
    try:
        result = client.capture(p.pack_config(mode=1), timeout_s=5.0)
    finally:
        client.close()
    assert result.encoding == 1
    assert result.meta["total_words"] == 5
    words = [mock.capture_payload[i:i + 4] for i in range(0, len(mock.capture_payload), 4)]
    runs = [(int.from_bytes(w, "little") >> 8) for w in words]
    assert sum(runs) == 1310
    assert result.parser_stats["crc_errors"] == 0


def test_capture_cancel_has_one_reader_and_leaves_connection_usable(analyzer):
    mock = analyzer("ping")  # ACK ARM, then wait without sending capture data.
    client = _client(mock.uri)
    cancel = threading.Event()
    timer = threading.Timer(0.1, cancel.set)
    timer.start()
    try:
        with pytest.raises(AnalyzerError, match="capture cancelled"):
            client.capture(p.pack_config(post_samples=64000), timeout_s=5, cancel_event=cancel)
        assert client.ping()["pl_id"] == 0x4C410002
        assert client.parser.crc_errors == client.parser.format_errors == 0
    finally:
        timer.cancel()
        client.close()


def test_nak_config_rejected(analyzer) -> None:
    mock = analyzer("nak_config")
    client = _client(mock.uri)
    try:
        with pytest.raises(AnalyzerError) as excinfo:
            client.capture(p.pack_config(), timeout_s=5.0)
    finally:
        client.close()
    assert excinfo.value.error_code == p.ERROR_BAD_FIELD
    assert "rejected" in str(excinfo.value)


def test_chunk_gap_detected(analyzer) -> None:
    mock = analyzer("chunk_gap")
    client = _client(mock.uri)
    try:
        with pytest.raises(AnalyzerError) as excinfo:
            client.capture(p.pack_config(), timeout_s=5.0)
    finally:
        client.close()
    assert "gap" in str(excinfo.value)


def test_chunk_bad_crc_detected(analyzer) -> None:
    mock = analyzer("chunk_bad_crc")
    client = _client(mock.uri)
    try:
        with pytest.raises(AnalyzerError) as excinfo:
            client.capture(p.pack_config(), timeout_s=5.0)
    finally:
        client.close()
    assert client.parser.crc_errors == 1
    assert "gap" in str(excinfo.value)


def test_timeout_when_silent(analyzer) -> None:
    mock = analyzer("silent")
    client = _client(mock.uri)
    try:
        with pytest.raises(TransportError) as excinfo:
            client.ping(timeout_s=0.5)
    finally:
        client.close()
    assert "timeout" in str(excinfo.value)


def test_eof_reported(analyzer) -> None:
    mock = analyzer("close_on_ping")
    client = _client(mock.uri)
    try:
        with pytest.raises(TransportError) as excinfo:
            client.ping(timeout_s=2.0)
    finally:
        client.close()
    assert "closed" in str(excinfo.value)


def test_test_record_burst_accounting(analyzer) -> None:
    mock = analyzer("ping")
    client = _client(mock.uri)
    try:
        result = client.test_records(range(6), settle_s=0.3)
    finally:
        client.close()
    accounting = account_test_records(result)
    assert accounting["clean"], accounting
    assert accounting["sent"] == 6
    assert accounting["acked"] == 6
    assert mock.test_record_ids == list(range(6))


def test_test_record_drop_detected(analyzer) -> None:
    mock = analyzer("test_record_drop")
    client = _client(mock.uri)
    try:
        result = client.test_records(range(6), settle_s=0.3)
    finally:
        client.close()
    accounting = account_test_records(result)
    assert not accounting["clean"]
    assert accounting["sent"] == 6
    assert len(accounting["missing"]) == 3
    assert accounting["duplicates"] == []
    assert accounting["naks"] == []
    assert mock.test_record_ids == list(range(6))
