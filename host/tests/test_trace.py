"""P8.3 trace: protocol payload, client dump collection, parser and renderer."""

from __future__ import annotations

import pytest
from click.testing import CliRunner

from host.la_cli import protocol as p
from host.la_cli import trace
from host.la_cli.client import AnalyzerClient
from host.la_cli.commands import cli
from host.la_cli.transport import open_uri

MOCK_LINES = [
    "LA TRACE 1 333333333",
    "TASK 1 00100000 acq",
    "QUEUE 00200000 seg0",
    "REC 1000 IN 1 0 00100000",
    "REC 1100 QS 0 0 00200000",
    "REC 1200 OUT 1 0 00100000",
    "REC 1300 TX 0 53 00000000",
    "END 4 4 0",
]


def test_pack_trace_ctrl_layout() -> None:
    assert p.pack_trace_ctrl(p.TRACE_START) == bytes([2, 0, 0, 0])
    assert p.pack_trace_ctrl(p.TRACE_DUMP) == bytes([3, 0, 0, 0])
    assert p.unpack_trace_ctrl(bytes([1, 0, 0, 0])) == p.TRACE_STOP
    with pytest.raises(ValueError):
        p.pack_trace_ctrl(0)
    with pytest.raises(p.FrameError):
        p.unpack_trace_ctrl(bytes([9, 0, 0, 0]))
    with pytest.raises(p.FrameError):
        p.unpack_trace_ctrl(b"\x01")


def test_trace_parse_and_summary() -> None:
    dump = trace.parse_trace(MOCK_LINES)
    summary = trace.summarize(dump)
    assert summary["records"] == 4
    assert summary["dropped"] == 0
    assert summary["tasks"] == 1
    assert summary["queues"] == 1
    assert summary["counts"] == {"IN": 1, "QS": 1, "OUT": 1, "TX": 1}
    assert dump.object_name(0x00100000) == "acq"
    assert dump.object_name(0x00200000) == "seg0"
    assert dump.object_name(0xDEADBEEF) == "obj_deadbeef"
    assert dump.duration_s == pytest.approx(300 / 333_333_333)


def test_trace_parse_rejects_bad_dumps() -> None:
    with pytest.raises(trace.TraceParseError):
        trace.parse_trace(["REC 1 IN 0 0 0"])  # no header
    with pytest.raises(trace.TraceParseError):
        trace.parse_trace(MOCK_LINES[:-1])  # no END
    with pytest.raises(trace.TraceParseError):
        trace.parse_trace(MOCK_LINES[:-1] + ["END 9 9 0"])  # count mismatch
    with pytest.raises(trace.TraceParseError):
        trace.parse_trace(MOCK_LINES[:1] + ["REC 1 ZZ 0 0 0", *MOCK_LINES[-1:]])
    with pytest.raises(trace.TraceParseError):
        trace.parse_trace(["LA TRACE 2 1"])


def test_trace_client_control_and_dump(analyzer) -> None:
    mock = analyzer("trace")
    client = AnalyzerClient(open_uri(mock.uri))
    try:
        client.trace_control(p.TRACE_START)
        assert mock.last_trace_action == p.TRACE_START
        client.trace_control(p.TRACE_STOP)
        assert mock.last_trace_action == p.TRACE_STOP
        lines = client.trace_dump()
    finally:
        client.close()
    assert lines == MOCK_LINES


def test_trace_render_timeline(tmp_path) -> None:
    dump = trace.parse_trace(MOCK_LINES)
    output = tmp_path / "trace.png"
    trace.render_timeline(dump, output, title="test trace")
    assert output.exists() and output.stat().st_size > 0


def test_trace_render_window(tmp_path) -> None:
    dump = trace.parse_trace(MOCK_LINES)
    output = tmp_path / "zoom.png"
    trace.render_timeline(dump, output, window_ms=(0.0, 0.001))
    assert output.exists() and output.stat().st_size > 0


def test_trace_command_dump(analyzer, tmp_path) -> None:
    mock = analyzer("trace")
    out = tmp_path / "dump.txt"
    png = tmp_path / "timeline.png"
    result = CliRunner().invoke(cli, [
        "trace", "--target", mock.uri, "--dump",
        "--out", str(out), "--png", str(png), "--title", "cli trace",
    ])
    assert result.exit_code == 0, result.output
    assert out.exists() and out.read_text(encoding="utf-8").startswith("LA TRACE 1 ")
    assert png.exists() and png.stat().st_size > 0
    assert "trace: 4 records, 0 dropped" in result.output
    assert "QS=1" in result.output


def test_trace_command_start_and_exclusive_flags(analyzer) -> None:
    mock = analyzer("trace")
    result = CliRunner().invoke(cli, ["trace", "--target", mock.uri, "--start"])
    assert result.exit_code == 0, result.output
    assert "trace started" in result.output
    assert mock.last_trace_action == p.TRACE_START

    result = CliRunner().invoke(cli, [
        "trace", "--target", mock.uri, "--start", "--stop",
    ])
    assert result.exit_code != 0
    assert "choose only one" in result.output
