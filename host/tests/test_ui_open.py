"""W07: offline open of .sr, .vcd and .csv without a device."""

from __future__ import annotations

import numpy as np
import pytest

from host.la_ui.export import samples_csv_bytes, sr_bytes, vcd_bytes
from host.la_ui.open import OpenError, open_capture
from host.la_ui.store import Capture


def _raw_capture(samples: bytes, rate: int = 10_000_000) -> Capture:
    return Capture(encoding=0, sample_rate_hz=rate, data=samples,
                   total_words=(len(samples) + 3) // 4)


def test_open_sr_round_trip_exact():
    samples = bytes(range(64))
    capture = _raw_capture(samples)
    opened = open_capture("capture.sr", sr_bytes(capture))
    assert opened.data == samples
    assert opened.total_samples == 64
    assert opened.sample_rate_hz == capture.sample_rate_hz
    assert opened.source == "open:capture.sr"
    assert opened.has_signal is True


def test_open_vcd_recovers_rate_and_changes():
    samples = bytes([1, 1, 2, 2, 3, 3, 4, 0])
    capture = _raw_capture(samples)
    opened = open_capture("capture.vcd", vcd_bytes(capture))
    assert opened.sample_rate_hz == capture.sample_rate_hz
    assert opened.data == samples
    assert any("changes only" in warning for warning in opened.warnings)


def test_open_vcd_trailing_constant_run_is_reported():
    # The last sample is constant, so the VCD ends at the last change.
    samples = bytes([1, 2, 3, 3, 3, 3])
    opened = open_capture("tail.vcd", vcd_bytes(_raw_capture(samples)))
    assert opened.data == bytes([1, 2, 3])
    assert any("trailing constant run" in warning for warning in opened.warnings)


def test_open_vcd_without_comment_uses_heuristic():
    text = (
        "$timescale 1 ns $end\n"
        "$scope module la $end\n"
        "$var wire 1 ! CH0 $end\n"
        "$upscope $end\n"
        "$enddefinitions $end\n"
        "#0\n0!\n#100\n1!\n#200\n0!\n"
    )
    opened = open_capture("plain.vcd", text.encode("ascii"))
    assert opened.total_samples == 3
    assert opened.data == bytes([0, 1, 0])
    assert any("no sample-rate comment" in warning for warning in opened.warnings)


def test_open_samples_csv_round_trip():
    samples = bytes([0, 1, 2, 3, 4, 5, 6, 7, 8, 9])
    capture = _raw_capture(samples)
    opened = open_capture("samples.csv", samples_csv_bytes(capture))
    assert opened.data == samples
    assert opened.sample_rate_hz == capture.sample_rate_hz
    assert opened.has_signal is True


def test_open_events_csv_builds_annotations():
    text = (
        "# la-ui events sample_rate_hz=10000000 capture=abc\n"
        "slot,kind,t_sample,channel,flags,data_hex\n"
        "0,BYTE,100,0,0,41\n"
        "1,NACK,300,1,0,\n"
    )
    opened = open_capture("events.csv", text.encode("utf-8"))
    assert opened.has_signal is False
    assert opened.total_samples == 301
    assert opened.annotations[0][0]["kind"] == 1
    assert opened.annotations[1][0]["name"] == "NACK"
    assert any("annotations only" in warning for warning in opened.warnings)


def test_open_rejects_bad_input():
    with pytest.raises(OpenError):
        open_capture("file.bin", b"nope")
    with pytest.raises(OpenError):
        open_capture("file.sr", b"not a zip")
    with pytest.raises(OpenError):
        open_capture("file.csv", b"a,b,c\n1,2,3\n")
    with pytest.raises(OpenError):
        open_capture("file.vcd", b"$scope module la $end\n$end\n")
    with pytest.raises(OpenError):
        open_capture("file.sr", sr_bytes(_raw_capture(b"")))


def test_open_sr_rejects_probe_mismatch():
    import io
    import zipfile

    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as archive:
        archive.writestr("version", "1\n")
        archive.writestr("metadata",
                         "[device 1]\ntotal probes=4\nsamplerate=1000\n"
                         "total samples=4\n")
        archive.writestr("logic-1-1", b"\x01\x02\x03\x04")
    with pytest.raises(OpenError):
        open_capture("four.sr", buffer.getvalue())
