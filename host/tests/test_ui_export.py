"""W05: capture exports (.sr/.vcd/CSV) and their offline round trips."""

from __future__ import annotations

import io
import zipfile

from host.la_ui.export import (
    events_csv_bytes,
    samples_csv_bytes,
    sr_bytes,
    vcd_bytes,
)
from host.la_ui.open import open_capture
from host.la_ui.store import Capture


def _raw_capture(samples: bytes, rate: int = 10_000_000) -> Capture:
    return Capture(encoding=0, sample_rate_hz=rate, data=samples,
                   total_words=(len(samples) + 3) // 4)


def _txn_capture() -> Capture:
    from host.tests.mock_analyzer import txn_words

    return Capture(encoding=1, sample_rate_hz=10_000_000,
                   data=txn_words([(900, 0x00), (7, 0x55), (300, 0xFF)]),
                   total_words=3)


def test_sr_bytes_are_a_valid_sigrok_zip():
    capture = _raw_capture(bytes(range(16)))
    blob = sr_bytes(capture)
    with zipfile.ZipFile(io.BytesIO(blob)) as archive:
        names = set(archive.namelist())
        assert {"version", "metadata", "logic-1-1"} <= names
        metadata = archive.read("metadata").decode("ascii")
        assert "samplerate=10000000" in metadata
        assert "total probes=8" in metadata
        assert archive.read("logic-1-1") == capture.data


def test_vcd_bytes_have_probes_and_changes():
    capture = _raw_capture(bytes([1, 1, 2, 2, 3, 3]))
    text = vcd_bytes(capture).decode("ascii")
    assert "$timescale 1 ns $end" in text
    assert "CH0" in text and "CH7" in text
    assert "logic analyzer capture at 10000000 Hz" in text


def test_events_csv_rows_sorted_with_metadata():
    capture = _raw_capture(bytes(range(8)))
    capture.annotations = {
        1: [{"kind": 1, "name": "BYTE", "t_sample": 500, "channel": 0,
             "flags": 0, "data": "41"}],
        0: [{"kind": 7, "name": "ADDRESS", "t_sample": 100, "channel": 1,
             "flags": 0, "data": "50"},
            {"kind": 8, "name": "ACK", "t_sample": 200, "channel": 1,
             "flags": 0, "data": ""}],
    }
    text = events_csv_bytes(capture).decode("utf-8")
    lines = text.splitlines()
    assert lines[0].startswith("# la-ui events sample_rate_hz=10000000")
    assert lines[1] == "slot,kind,t_sample,channel,flags,data_hex"
    assert lines[2] == "0,ADDRESS,100,1,0,50"
    assert lines[3] == "0,ACK,200,1,0,"
    assert lines[4] == "1,BYTE,500,0,0,41"


def test_samples_csv_rows_and_round_trip():
    samples = bytes([0b00000001, 0b00000011, 0b00000010, 0b00000000])
    capture = _raw_capture(samples)
    text = samples_csv_bytes(capture).decode("utf-8")
    lines = text.splitlines()
    assert lines[0].startswith("# la-ui samples sample_rate_hz=10000000")
    assert lines[1] == "t,CH0,CH1,CH2,CH3,CH4,CH5,CH6,CH7"
    assert lines[2] == "0,1,0,0,0,0,0,0,0"
    assert lines[3] == "1,1,1,0,0,0,0,0,0"
    opened = open_capture("samples.csv", text.encode("utf-8"))
    assert opened.data == samples
    assert opened.sample_rate_hz == capture.sample_rate_hz


def test_sr_and_vcd_round_trip_samples():
    samples = bytes([1, 1, 2, 2, 3, 3, 4, 0])  # ends with a change
    capture = _raw_capture(samples)
    opened_sr = open_capture("x.sr", sr_bytes(capture))
    assert opened_sr.data == samples
    opened_vcd = open_capture("x.vcd", vcd_bytes(capture))
    assert opened_vcd.sample_rate_hz == capture.sample_rate_hz
    assert opened_vcd.data == samples


def test_txn_capture_exports_use_reconstructed_samples():
    capture = _txn_capture()
    opened = open_capture("x.sr", sr_bytes(capture))
    assert opened.total_samples == capture.total_samples
    assert opened.data == bytes([0x00] * 900 + [0x55] * 7 + [0xFF] * 300)
