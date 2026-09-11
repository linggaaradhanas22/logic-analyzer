"""Export writers and streaming RLE reconstruction tests."""

from __future__ import annotations

import io
import struct
import zipfile

from host.la_cli import convert
from host.la_cli import protocol as p
from host.la_cli.convert import count_samples, export_capture, iter_samples


def _raw_words(samples: list[int]) -> bytes:
    """Packs samples into raw words (4 samples per word, oldest byte first)."""
    assert len(samples) % 4 == 0
    return b"".join(struct.pack("<I", samples[i] | (samples[i + 1] << 8) |
                                (samples[i + 2] << 16) | (samples[i + 3] << 24))
                     for i in range(0, len(samples), 4))


def _txn_data(runs: list[tuple[int, int]]) -> bytes:
    return b"".join(struct.pack("<I", (run << 8) | (sample & 0xFF))
                    for run, sample in runs)


def test_reconstruction_raw_round_trip() -> None:
    samples = [0x00, 0xAA, 0xFF, 0x55, 0x01, 0x02, 0x03, 0x04]
    data = _raw_words(samples)
    assert list(iter_samples(data, 0)) == samples
    assert count_samples(data, 0) == 8


def test_reconstruction_txn_round_trip() -> None:
    runs = [(3, 0x00), (1, 0xFF), (2, 0x5A), (5, 0x00)]
    data = _txn_data(runs)
    expected = [0x00, 0x00, 0x00, 0xFF, 0x5A, 0x5A, 0, 0, 0, 0, 0]
    assert list(iter_samples(data, 1)) == expected
    assert count_samples(data, 1) == 11


def test_sr_export_raw(tmp_path) -> None:
    samples = list(range(256)) * 8  # 2048 samples = 512 words
    data = _raw_words(samples)
    path = tmp_path / "capture.sr"
    exported = export_capture(str(path), data, 0, 100_000_000)
    assert exported == 2048
    with zipfile.ZipFile(path) as archive:
        assert archive.read("version") == b"1\n"
        metadata = archive.read("metadata").decode()
        assert "samplerate=100000000" in metadata
        assert "total samples=2048" in metadata
        assert "unitsize=1" in metadata
        assert "capturefile=logic-1\n" in metadata
        assert "total probes=8" in metadata
        assert "probe1=CH0" in metadata and "probe8=CH7" in metadata
        decoded = archive.read("logic-1-1")
    assert decoded == bytes(samples)


def test_sr_export_txn(tmp_path) -> None:
    runs = [(100, 0x00), (5, 0xFF), (40, 0x03)]
    data = _txn_data(runs)
    path = tmp_path / "txn.sr"
    exported = export_capture(str(path), data, 1, 10_000_000)
    assert exported == 145
    with zipfile.ZipFile(path) as archive:
        assert "total samples=145" in archive.read("metadata").decode()
        assert len(archive.read("logic-1-1")) == 145


def test_vcd_export(tmp_path) -> None:
    samples = [0x00, 0xFF, 0x55, 0xAA] * 4
    data = _raw_words(samples)
    path = tmp_path / "capture.vcd"
    exported = export_capture(str(path), data, 0, 100_000_000)
    assert exported == 16
    text = path.read_text()
    assert "$timescale 1 ns $end" in text
    assert text.count("$var wire 1") == 8
    assert "CH0" in text and "CH7" in text
    assert text.count("#") >= 8  # value changes present


def test_png_export(tmp_path) -> None:
    samples = [0x00, 0xFF] * 32
    data = _raw_words(samples)
    path = tmp_path / "capture.png"
    exported = export_capture(str(path), data, 0, 10_000_000)
    assert exported == 64
    assert path.stat().st_size > 500


def test_export_rejects_unknown_extension(tmp_path) -> None:
    path = tmp_path / "capture.txt"
    try:
        export_capture(str(path), b"\x00", 0, 1000)
    except ValueError:
        pass
    else:
        raise AssertionError("unknown extension must raise")
