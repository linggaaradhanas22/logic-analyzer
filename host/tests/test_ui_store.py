"""W01: capture store, lazy sample decoding and envelope decimation."""

from __future__ import annotations

import numpy as np

from host.la_ui.render import (
    ENVELOPE_HEADER,
    ENVELOPE_MAGIC,
    decode_window,
    encode_envelope,
)
from host.la_ui.store import WARN_SAMPLES, Capture, CaptureStore
from host.tests.mock_analyzer import txn_words


def _reference_envelope(samples: np.ndarray, buckets: int):
    total = len(samples)
    buckets = min(buckets, total)
    result = []
    for channel in range(8):
        mins, maxs = [], []
        for bucket in range(buckets):
            start = bucket * total // buckets
            end = (bucket + 1) * total // buckets
            values = [(int(samples[index]) >> channel) & 1
                      for index in range(start, end)]
            mins.append(min(values))
            maxs.append(max(values))
        result.append((mins, maxs))
    return result


def test_raw_capture_samples():
    data = bytes([0b00000001, 0b00000010, 0b00000100])
    capture = Capture(encoding=0, sample_rate_hz=10_000_000, data=data,
                      total_words=1)
    assert capture.total_samples == 3
    assert capture.total_words == 1
    assert list(capture.samples()) == [1, 2, 4]
    assert capture.info()["encoding"] == 0


def test_txn_capture_expands_runs():
    data = txn_words([(3, 0x01), (2, 0xFF)])
    capture = Capture(encoding=1, sample_rate_hz=1000, data=data, total_words=2)
    assert capture.total_samples == 5
    assert list(capture.samples()) == [1, 1, 1, 0xFF, 0xFF]


def test_envelope_matches_reference():
    samples = np.array([0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1],
                       dtype=np.uint8)
    capture = Capture(encoding=0, sample_rate_hz=1, data=samples.tobytes(),
                      total_words=4)
    for buckets in (1, 2, 4, 8, 16):
        actual, blob = encode_envelope(capture, buckets)
        assert actual == buckets
        magic, version, channels, header_buckets, total = \
            ENVELOPE_HEADER.unpack_from(blob, 0)
        assert magic == ENVELOPE_MAGIC
        assert version == 1
        assert channels == 8
        assert header_buckets == buckets
        assert total == len(samples)
        offset = ENVELOPE_HEADER.size
        reference = _reference_envelope(samples, buckets)
        for channel in range(8):
            mins = list(blob[offset:offset + buckets])
            offset += buckets
            maxs = list(blob[offset:offset + buckets])
            offset += buckets
            assert (mins, maxs) == reference[channel], f"channel {channel}"
        assert offset == len(blob)


def test_envelope_clamps_and_handles_empty():
    samples = np.arange(4, dtype=np.uint8)
    capture = Capture(encoding=0, sample_rate_hz=1, data=samples.tobytes(),
                      total_words=1)
    actual, _blob = encode_envelope(capture, 100)
    assert actual == 4
    empty = Capture(encoding=0, sample_rate_hz=1, data=b"", total_words=0)
    actual, blob = encode_envelope(empty, 16)
    assert actual == 0
    assert blob == ENVELOPE_HEADER.pack(ENVELOPE_MAGIC, 1, 8, 0, 0)


def test_decode_window_bounds():
    samples = np.arange(10, dtype=np.uint8)
    capture = Capture(encoding=0, sample_rate_hz=1, data=samples.tobytes(),
                      total_words=3)
    assert decode_window(capture, 2, 3) == bytes([2, 3, 4])
    assert decode_window(capture, 8, 100) == bytes([8, 9])
    assert decode_window(capture, 50, 5) == b""
    assert decode_window(capture, -5, 2) == bytes([0, 1])


def test_store_lifecycle():
    store = CaptureStore()
    first = Capture(encoding=0, sample_rate_hz=1, data=b"\x00", total_words=1)
    second = Capture(encoding=0, sample_rate_hz=1, data=b"\x01", total_words=1)
    store.add(first)
    store.add(second)
    assert len(store) == 2
    assert [item["id"] for item in store.list()] == [second.id, first.id]
    assert store.get(first.id) is first
    assert store.delete(first.id) is True
    assert store.delete(first.id) is False
    try:
        store.get(first.id)
    except KeyError:
        pass
    else:
        raise AssertionError("deleted capture must not resolve")
    store.clear()
    assert len(store) == 0


def test_large_capture_warns():
    data = bytes(WARN_SAMPLES + 1)
    capture = Capture(encoding=0, sample_rate_hz=1, data=data,
                      total_words=len(data) // 4)
    assert capture.warnings
    assert "decimated" in capture.warnings[0]
