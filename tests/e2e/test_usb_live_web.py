"""V04 USB live end-to-end gates with the host web bridge in the loop.

Run against the USB-profile device (patgen UART on CH0, JE->JC loopback):

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- \\
        --target serial://COM13 -k usb_live_web

The bridge is a real `LiveWebServer` + `LiveEventHub`; a minimal SSE reader
thread counts every event and record the browser feed receives. The gates
require the feed to match the bridge's published count exactly, the device to
return to IDLE, and the 1 MHz SPI overload to recover within 3 s while the
bridge keeps serving.
"""

from __future__ import annotations

import http.client
import json
import threading
import time

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri
from host.la_live_web import LiveEventHub, LiveWebServer

FCLK_HZ = 100_000_000
UART_FRAME = b"LA-TEST\n"
EVENT_BYTE = 1
SPI_WORDS = (0x4C, 0x41)
I2C_ADDRESS = 0x50
I2C_BYTES = (0x4C, 0x41)


@pytest.fixture(scope="module")
def target(request) -> str:
    value = request.config.getoption("--target")
    if not value:
        pytest.skip("no --target: the V04 bridge gates require the bench hardware")
    return value


@pytest.fixture(scope="module")
def client(target):
    analyzer = AnalyzerClient(open_uri(target))
    yield analyzer
    analyzer.close()


@pytest.fixture
def bridge():
    """A fresh hub/server per test so the replay ring never leaks between
    gates."""
    hub = LiveEventHub()
    server = LiveWebServer(hub, port=0)
    server.start()
    yield hub, server
    server.stop()


class _SseReader(threading.Thread):
    """Minimal SSE client: counts messages/events and keeps every record."""

    def __init__(self, port: int) -> None:
        super().__init__(name="sse-reader", daemon=True)
        self.port = port
        self.status: int | None = None
        self.error: Exception | None = None
        self.events = 0
        self.messages = 0
        self.records: list[dict] = []
        self._stop_requested = threading.Event()
        self._ready = threading.Event()

    def run(self) -> None:
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        try:
            connection.request("GET", "/events")
            response = connection.getresponse()
            self.status = response.status
            self._ready.set()
            while not self._stop_requested.is_set():
                line = response.readline()
                if not line:
                    break
                if not line.startswith(b"data: "):
                    continue
                arrival = time.monotonic()
                message = json.loads(line[6:])
                if "e" not in message:
                    continue
                self.messages += 1
                for record in message["e"]:
                    self.events += 1
                    self.records.append({
                        "slot": message["s"],
                        "kind": record[1],
                        "t_sample": record[0],
                        "data": bytes.fromhex(record[2] or ""),
                        "arrival": arrival,
                    })
        except Exception as exc:  # noqa: BLE001 - reported by the test
            self.error = exc
        finally:
            self._ready.set()
            connection.close()

    def wait_ready(self, timeout: float = 5.0) -> None:
        assert self._ready.wait(timeout), "SSE reader did not connect"
        assert self.status == 200, f"SSE feed status {self.status}"
        assert self.error is None, self.error

    def stop(self) -> None:
        self._stop_requested.set()
        self.join(timeout=10.0)
        assert not self.is_alive(), "SSE reader did not stop"
        assert self.error is None, self.error


def _wait_feed(reader: _SseReader, hub: LiveEventHub, timeout: float = 5.0) -> None:
    """Waits until the SSE reader consumed everything the hub published."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and reader.events < hub.events:
        time.sleep(0.05)


def _stream_config(mode: int) -> bytes:
    return p.pack_config(
        ctrl_flags=p.CTRL_STREAM,
        sample_div=0,
        mode=mode,
        chan_en=0xFF,
        post_samples=0,
        txn_max_run=0x00FFFFFF,
        live_flush_cycles=200_000,
    )


def _configure_patgen(client: AnalyzerClient, mode: int, div: int) -> None:
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(
        p.PATGEN_EN_MASK | (mode << p.PATGEN_MODE_SHIFT) | (div << 16)
    )


def _configure_patgen_uart(client: AnalyzerClient, baud: int) -> None:
    div = max(0, 115200 // baud - 1) if baud <= 115200 else 0
    _configure_patgen(client, p.PATGEN_MODE_UART_115200, div)


def _uart_decoder(baud: int, stop_on: bytes | None = None) -> bytes:
    return p.pack_decoder_cfg_uart(0, 0, baud, FCLK_HZ, stop_on=stop_on)


def _slot_bytes(reader: _SseReader, slot: int = 0) -> bytes:
    return b"".join(
        record["data"]
        for record in reader.records
        if record["kind"] == EVENT_BYTE and record["slot"] == slot
    )


def _aligned_frame_bytes(reader: _SseReader) -> bytes:
    data = _slot_bytes(reader)
    start = data.find(b"L")
    assert start >= 0, f"no frame start in {data[:32]!r}"
    return data[start:]


def _get(port: int, path: str) -> tuple[int, bytes]:
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    connection.request("GET", path)
    response = connection.getresponse()
    body = response.read()
    connection.close()
    return response.status, body


def _assert_bridge_clean(hub: LiveEventHub, reader: _SseReader) -> None:
    """The live feed must be lossless; ring eviction only bounds replay."""
    stats = hub.stats()
    assert stats["client_dropped"] == 0, stats
    assert stats["format_dropped"] == 0, stats
    assert stats["events"] == reader.events, (stats["events"], reader.events)
    assert stats["messages"] == reader.messages, (stats["messages"], reader.messages)


def _publisher(hub: LiveEventHub, count: int | None = None):
    """Mirrors `la-cli live --serve`: caps the feed exactly at `count`."""
    def publish(frame_events) -> None:
        limit = None if count is None else max(0, count - hub.events)
        hub.publish_frame(frame_events, limit=limit)
    return publish


def test_v04_uart_zero_drop_through_the_bridge(client, bridge, request) -> None:
    hub, server = bridge
    seconds = request.config.getoption("--live-seconds")
    baud = request.config.getoption("--live-baud")
    _configure_patgen_uart(client, baud)
    reader = _SseReader(server.port)
    reader.start()
    reader.wait_ready()
    try:
        client.live(_uart_decoder(baud), config=_stream_config(1), count=None,
                    timeout_s=seconds, collect_events=False,
                    on_event=hub.publish_frame)
    finally:
        _wait_feed(reader, hub)
        reader.stop()
    _assert_bridge_clean(hub, reader)
    assert client.live_stats["overruns"] == 0, client.live_stats
    data = _aligned_frame_bytes(reader)
    assert len(data) >= 8 * max(1, int(seconds)), "live stream decoded too few bytes"
    expected = (UART_FRAME * (len(data) // len(UART_FRAME) + 1))[: len(data)]
    assert data == expected, "decoded stream is not the repeating UART frame"
    status = client.status()
    assert status["state"] == 0, "firmware did not return to IDLE after LIVE_STOP"
    assert status["decoders"][0]["dropped_segments"] == 0


def test_v04_isolated_byte_latency_through_the_bridge(client, bridge) -> None:
    hub, server = bridge
    client.configure_patgen(0)
    time.sleep(0.01)
    reader = _SseReader(server.port)
    reader.start()
    reader.wait_ready()

    def trigger() -> None:
        client.raw_send(p.encode(
            p.PATGEN_CFG, 0x42,
            p.pack_patgen_cfg(p.PATGEN_EN_MASK
                              | (p.PATGEN_MODE_UART_SINGLE << p.PATGEN_MODE_SHIFT))))

    try:
        client.live(_uart_decoder(115200), config=_stream_config(1), count=None,
                    timeout_s=5.0, collect_events=False,
                    on_event=hub.publish_frame, on_started=trigger)
    finally:
        _wait_feed(reader, hub, 3.0)
        reader.stop()
    records = [record for record in reader.records
               if record["kind"] == EVENT_BYTE and record["slot"] == 0]
    start = next((index for index, record in enumerate(records)
                  if record["data"] == b"L"), None)
    assert start is not None, [record["data"] for record in records[:16]]
    frame = records[start:start + len(UART_FRAME)]
    assert bytes(b"".join(record["data"] for record in frame)) == UART_FRAME
    wire_gap = (frame[-1]["t_sample"] - frame[0]["t_sample"]) / FCLK_HZ
    extra = (frame[-1]["arrival"] - frame[0]["arrival"]) - wire_gap
    assert extra <= 0.025, (extra, frame[0], frame[-1])


def test_v04_spi_10khz_through_the_bridge(client, bridge) -> None:
    hub, server = bridge
    _configure_patgen(client, 4, 99)
    reader = _SseReader(server.port)
    reader.start()
    reader.wait_ready()
    try:
        client.live(
            p.pack_decoder_cfg_spi(0, 0, mosi_channel=1, cs_channel=2, cs_active=0),
            config=_stream_config(1), count=None, timeout_s=2.0,
            collect_events=False, on_event=hub.publish_frame)
    finally:
        _wait_feed(reader, hub, 3.0)
        reader.stop()
    _assert_bridge_clean(hub, reader)
    kinds = [record["kind"] for record in reader.records]
    assert 10 in kinds and 11 in kinds, f"missing CS events in {kinds}"
    start = kinds.index(11) + 1
    assert kinds[start] == 10, f"no CS assert after the partial frame: {kinds[start:start + 4]}"
    aligned = reader.records[start:]
    assert 2 not in [record["kind"] for record in aligned], "SPI frame error after alignment"
    words = [record["data"][0] for record in aligned if record["kind"] == EVENT_BYTE]
    assert len(words) >= 4, words
    expected = (list(SPI_WORDS) * (len(words) // 2 + 1))[: len(words)]
    assert words == expected, (words, expected)
    status = client.status()
    assert status["state"] == 0
    assert status["decoders"][1]["dropped_segments"] == 0


def test_v04_i2c_10khz_through_the_bridge(client, bridge) -> None:
    hub, server = bridge
    _configure_patgen(client, 5, 39)
    reader = _SseReader(server.port)
    reader.start()
    reader.wait_ready()
    try:
        client.live(p.pack_decoder_cfg_i2c(0, 3, 4), config=_stream_config(1),
                    count=None, timeout_s=2.0, collect_events=False,
                    on_event=hub.publish_frame)
    finally:
        _wait_feed(reader, hub, 3.0)
        reader.stop()
    _assert_bridge_clean(hub, reader)
    kinds = [record["kind"] for record in reader.records]
    assert 9 not in kinds, "I2C NACK"
    assert 4 in kinds, f"no START in {kinds}"
    start = kinds.index(4)
    aligned = kinds[start:]
    pattern = (4, 7, 8, 1, 8, 1, 8, 5)  # START, ADDR, ACK, BYTE, ACK, BYTE, ACK, STOP
    complete = len(aligned) // len(pattern)
    assert complete >= 2, (complete, aligned)
    assert tuple(aligned[: complete * len(pattern)]) == pattern * complete, aligned
    for record in reader.records[start:start + complete * len(pattern)]:
        if record["kind"] == 7:
            assert record["data"] == bytes([I2C_ADDRESS]), record
        elif record["kind"] == EVENT_BYTE:
            assert record["data"][0] in I2C_BYTES, record
    status = client.status()
    assert status["state"] == 0
    assert status["decoders"][2]["dropped_segments"] == 0


def test_v04_stop_on_decoded_pattern_through_the_bridge(client, bridge, request) -> None:
    hub, server = bridge
    baud = request.config.getoption("--live-baud")
    _configure_patgen_uart(client, baud)
    reader = _SseReader(server.port)
    reader.start()
    reader.wait_ready()
    try:
        client.live(_uart_decoder(baud, stop_on=b"LA"), config=_stream_config(1),
                    count=None, timeout_s=10.0, stop_on_match=True,
                    collect_events=False, on_event=hub.publish_frame)
    finally:
        _wait_feed(reader, hub, 3.0)
        reader.stop()
    assert client.live_match_index is not None, "no EVT_TRIGGERED match report"
    _assert_bridge_clean(hub, reader)
    data = _aligned_frame_bytes(reader)
    assert b"LA" in data, f"matched bytes missing from {data[:32]!r}"
    status = client.status()
    assert status["state"] == 0, "firmware did not return to IDLE after stop-on"


def test_v04_compact_events_through_the_bridge(client, bridge, request) -> None:
    hub, server = bridge
    baud = request.config.getoption("--live-baud")
    _configure_patgen_uart(client, baud)
    reader = _SseReader(server.port)
    reader.start()
    reader.wait_ready()
    try:
        client.live(_uart_decoder(baud), config=_stream_config(1), count=None,
                    timeout_s=3.0, collect_events=False,
                    event_format=p.EVT_FORMAT_BLOCK, on_event=hub.publish_frame)
    finally:
        _wait_feed(reader, hub)
        reader.stop()
    _assert_bridge_clean(hub, reader)
    assert hub.events > 0
    data = _aligned_frame_bytes(reader)
    assert data[: len(UART_FRAME)] == UART_FRAME
    status = client.status()
    assert status["state"] == 0


def test_v04_overload_recovers_while_serving(client, bridge) -> None:
    hub, server = bridge
    _configure_patgen(client, 4, 0)
    reader = _SseReader(server.port)
    reader.start()
    reader.wait_ready()
    try:
        client.live(
            p.pack_decoder_cfg_spi(0, 0, mosi_channel=1, cs_channel=2, cs_active=0),
            config=_stream_config(1), count=None, timeout_s=5.0,
            collect_events=False, on_event=hub.publish_frame,
            stop_on_overrun=False)
        assert hub.events > 0, "the bridge received no events during the flood"
        status = None
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            try:
                status = client.status(timeout_s=2.0)
            except Exception:  # noqa: BLE001 - retried within the deadline
                status = None
            if status is not None and status["state"] == 0:
                break
            time.sleep(0.2)
        assert status is not None and status["state"] == 0, \
            "overload did not return to IDLE within 3 s"
        code, body = _get(server.port, "/status")
        assert code == 200
        snapshot = json.loads(body)
        assert snapshot["events"] >= hub.events
    finally:
        reader.stop()


def test_v04_bridge_reopen_does_not_wedge_the_port(client, bridge, request) -> None:
    hub, server = bridge
    baud = request.config.getoption("--live-baud")
    _configure_patgen_uart(client, baud)
    reader = _SseReader(server.port)
    reader.start()
    reader.wait_ready()
    try:
        client.live(_uart_decoder(baud), config=_stream_config(1), count=32,
                    timeout_s=5.0, collect_events=False,
                    on_event=_publisher(hub, 32))
    finally:
        _wait_feed(reader, hub)
        reader.stop()
    assert hub.events == 32, hub.events
    server.stop()
    # The early count stop can leave one stale overrun report in flight; a
    # fresh session must tolerate it and still deliver its own 32 events.
    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline:
        if client.status(timeout_s=2.0)["state"] == 0:
            break
        time.sleep(0.1)

    # A brand-new bridge and session must work after the first one closed.
    # The patgen mode/DIV latches on the EN rising edge, so re-arm it for the
    # new session (the same rule the rest of the live suite follows).
    _configure_patgen_uart(client, baud)
    hub2 = LiveEventHub()
    server2 = LiveWebServer(hub2, port=0)
    server2.start()
    reader2 = _SseReader(server2.port)
    reader2.start()
    reader2.wait_ready()
    try:
        client.live(_uart_decoder(baud), config=_stream_config(1), count=32,
                    timeout_s=5.0, collect_events=False,
                    stop_on_overrun=False, on_event=_publisher(hub2, 32))
    finally:
        _wait_feed(reader2, hub2)
        reader2.stop()
        server2.stop()
    assert hub2.events == 32, hub2.events
    assert hub2.events == reader2.events
    pong = client.ping(timeout_s=5.0)
    assert pong["fw_version"] != 0, pong
