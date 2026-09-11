"""P7.5 host-side live decoding against the mock analyzer."""

from __future__ import annotations

from click.testing import CliRunner

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.commands import cli
from host.la_cli.transport import open_uri


def _collect_all(uri: str) -> list[dict]:
    client = AnalyzerClient(open_uri(uri))
    try:
        events = client.live(p.pack_decoder_cfg_uart(0, 0, 115200, 10_000_000))
    finally:
        client.close()
    return events


def test_decoder_config_pack_layout() -> None:
    payload = p.pack_decoder_cfg_uart(2, 3, 115200, 100_000_000, data_bits=8)
    assert len(payload) == 20
    assert payload[0] == 2 and payload[1] == p.DECODER_UART
    assert payload[4] == 3 and payload[5] == 8
    assert int.from_bytes(payload[12:16], "little") == 115200
    assert int.from_bytes(payload[16:20], "little") == 100_000_000


def test_live_client_collects_events_and_overrun(analyzer) -> None:
    mock = analyzer("live")
    events = _collect_all(mock.uri)
    decoded = [e for e in events if not e.get("overrun")]
    overruns = [e for e in events if e.get("overrun")]
    assert len(decoded) == 24  # three batches of b"LA-TEST\n"
    assert b"".join(e["data"] for e in decoded) == b"LA-TEST\n" * 3
    assert all(e["slot"] == 0 and e["kind"] == 1 for e in decoded)
    assert decoded[0]["t_sample"] == 10_000
    assert decoded[-1]["t_sample"] == 10_000 + 2 * 10_000 + 7 * 100
    assert len(overruns) == 1
    assert overruns[0]["dropped"] == 3 and overruns[0]["stage"] == 1


def test_live_client_count_stops_early(analyzer) -> None:
    mock = analyzer("live")
    client = AnalyzerClient(open_uri(mock.uri))
    try:
        events = client.live(p.pack_decoder_cfg_uart(0, 0, 115200, 10_000_000), count=5)
    finally:
        client.close()
    decoded = [e for e in events if not e.get("overrun")]
    assert len(decoded) == 5


def test_live_command_text(analyzer) -> None:
    mock = analyzer("live")
    result = CliRunner().invoke(cli, [
        "live", "--target", mock.uri, "--decoder", "uart:rx=0,baud=115200", "--count", "8",
    ])
    assert result.exit_code == 0, result.output
    # The count stop leaves the mock's trailing EVT_OVERRUN in flight; the
    # stop-wait now collects it, so the summary reports one overrun.
    assert "live: 8 decoded events, 1 overruns" in result.output
    assert "BYTE 0x4c 'L'" in result.output
    assert "BYTE 0x41 'A'" in result.output


def test_live_command_csv(analyzer) -> None:
    mock = analyzer("live")
    result = CliRunner().invoke(cli, [
        "live", "--target", mock.uri, "--csv", "--count", "8",
    ])
    assert result.exit_code == 0, result.output
    rows = [line for line in result.output.splitlines() if line.startswith("BYTE,")]
    assert len(rows) == 8
    assert rows[0] == "BYTE,0,10000,4c"


def test_live_command_sends_stream_config_and_patgen(analyzer) -> None:
    mock = analyzer("live")
    result = CliRunner().invoke(cli, [
        "live", "--target", mock.uri, "--count", "8", "--patgen", "uart",
    ])
    assert result.exit_code == 0, result.output
    assert mock.last_config is not None
    assert len(mock.last_config) == p.CONFIG_SIZE
    assert mock.last_config[0] & p.CTRL_STREAM
    assert int.from_bytes(mock.last_config[8:12], "little") & 1  # transition encoding
    assert int.from_bytes(mock.last_config[33:37], "little") == 200_000
    assert mock.last_patgen is not None
    ctrl = int.from_bytes(mock.last_patgen, "little")
    assert ctrl & p.PATGEN_EN_MASK
    assert (ctrl & p.PATGEN_MODE_MASK) >> p.PATGEN_MODE_SHIFT == p.PATGEN_MODE_UART_115200


def test_live_command_bad_decoder_spec() -> None:
    result = CliRunner().invoke(cli, [
        "live", "--target", "tcp://127.0.0.1:1", "--decoder", "spi:mosi=1",
    ])
    assert result.exit_code != 0
    assert "decoder must be uart:" in result.output
