"""Click CLI behaviour against the mock analyzer."""

from __future__ import annotations

import zipfile

from click.testing import CliRunner

from host.la_cli import protocol as p
from host.la_cli.commands import cli


def test_ports_reports_candidates_or_none() -> None:
    # The bench machine may have a USB-UART attached (for example the Zybo
    # FTDI console), so accept either an empty report or a candidate list.
    result = CliRunner().invoke(cli, ["ports"])
    assert result.exit_code == 0
    if "no USB-UART" not in result.output:
        assert "COM" in result.output


def test_ping_command(analyzer) -> None:
    mock = analyzer("ping")
    result = CliRunner().invoke(cli, ["ping", "--target", mock.uri])
    assert result.exit_code == 0, result.output
    assert "pong: fw 0x00000001 pl 0x4C410002 uptime 42 ms" in result.output


def test_status_command(analyzer) -> None:
    mock = analyzer("status")
    result = CliRunner().invoke(cli, ["status", "--target", mock.uri])
    assert result.exit_code == 0, result.output
    assert "state done" in result.output
    assert "decoder i2c: events 7 dropped_segments 1" in result.output


def test_capture_command_raw(analyzer, tmp_path) -> None:
    mock = analyzer("capture_raw")
    output = tmp_path / "capture.sr"
    result = CliRunner().invoke(cli, [
        "capture", "--target", mock.uri, "--rate", "10M", "--words", "3056",
        "--mode", "raw", "--trig-rise", "0", "--out", str(output),
    ])
    assert result.exit_code == 0, result.output
    assert "captured 764 words = 3056 samples (rate 10000000 Hz)" in result.output
    assert "exported 3056 samples" in result.output
    with zipfile.ZipFile(output) as archive:
        assert "total samples=3056" in archive.read("metadata").decode()


def test_capture_command_patgen_selftest(analyzer, tmp_path) -> None:
    mock = analyzer("capture_raw")
    output = tmp_path / "selftest.sr"
    result = CliRunner().invoke(cli, [
        "capture", "--target", mock.uri, "--rate", "10M", "--words", "3056",
        "--patgen", "uart", "--out", str(output),
    ])
    assert result.exit_code == 0, result.output
    assert mock.patgen_history == [
        p.pack_patgen_cfg(0),
        p.pack_patgen_cfg(p.PATGEN_EN_MASK | (3 << 4)),
    ]
    assert mock.last_config is not None
    assert mock.last_config[0] & p.CTRL_SELFTEST_EN


def test_capture_command_txn(analyzer, tmp_path) -> None:
    mock = analyzer("capture_txn")
    output = tmp_path / "txn.vcd"
    result = CliRunner().invoke(cli, [
        "capture", "--target", mock.uri, "--rate", "10M", "--mode", "txn",
        "--words", "1310", "--out", str(output),
    ])
    assert result.exit_code == 0, result.output
    assert "5 transition words" in result.output
    assert "exported 1310 samples" in result.output
    assert output.stat().st_size > 100


def test_capture_nak_reports_error(analyzer, tmp_path) -> None:
    mock = analyzer("nak_config")
    result = CliRunner().invoke(cli, [
        "capture", "--target", mock.uri, "--words", "1000", "--out",
        str(tmp_path / "x.sr"),
    ])
    assert result.exit_code == 1
    assert "bad field" in result.output


def test_capture_bad_rate(analyzer, tmp_path) -> None:
    mock = analyzer("capture_raw")
    result = CliRunner().invoke(cli, [
        "capture", "--target", mock.uri, "--rate", "200M", "--out",
        str(tmp_path / "x.sr"),
    ])
    assert result.exit_code != 0
    assert "rate must be within" in result.output


def test_capture_bad_channel(analyzer, tmp_path) -> None:
    mock = analyzer("capture_raw")
    result = CliRunner().invoke(cli, [
        "capture", "--target", mock.uri, "--trig-rise", "9", "--out",
        str(tmp_path / "x.sr"),
    ])
    assert result.exit_code != 0
    assert "channel must be within 0..7" in result.output


def test_capture_unknown_target() -> None:
    result = CliRunner().invoke(cli, [
        "capture", "--target", "carrier://pigeon", "--out", "x.sr",
    ])
    assert result.exit_code == 1
    assert "unknown target" in result.output


def test_abort_command(analyzer) -> None:
    mock = analyzer("ping")
    result = CliRunner().invoke(cli, ["abort", "--target", mock.uri])
    assert result.exit_code == 0, result.output
    assert "aborted" in result.output
