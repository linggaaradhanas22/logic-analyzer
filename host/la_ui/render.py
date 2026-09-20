"""Server-side decimation and sample windows for the viewer (W01).

The page never receives a full capture by default. It asks for a min/max
envelope of roughly one bucket per horizontal pixel and, when zoomed in, for
exact sample windows. Envelopes are built from the lazily decoded numpy
sample array (an RLE capture is never expanded twice) and served as compact
little-endian binary:

    magic "LAEN" | version u8 | channels u8 | buckets u16 | total_samples u32
    then per channel: buckets min bytes, buckets max bytes
"""

from __future__ import annotations

import struct

import numpy as np

from .store import Capture

ENVELOPE_MAGIC = b"LAEN"
ENVELOPE_VERSION = 1
CHANNELS = 8
ENVELOPE_HEADER = struct.Struct("<4sBBHI")

DETAIL_MAX_SAMPLES = 262_144


def encode_envelope(capture: Capture, buckets: int) -> tuple[int, bytes]:
    """Returns ``(actual_buckets, blob)`` for the requested bucket count.

    ``buckets`` is clamped to the sample count so every bucket is non-empty.
    """
    samples = capture.samples()
    total = int(samples.size)
    buckets = int(buckets)
    if total == 0 or buckets <= 0:
        return 0, ENVELOPE_HEADER.pack(ENVELOPE_MAGIC, ENVELOPE_VERSION,
                                       CHANNELS, 0, 0)
    buckets = min(buckets, total)
    starts = (np.arange(buckets, dtype=np.int64) * total) // buckets
    out = bytearray(ENVELOPE_HEADER.pack(ENVELOPE_MAGIC, ENVELOPE_VERSION,
                                         CHANNELS, buckets, total))
    for channel in range(CHANNELS):
        bits = ((samples >> channel) & 1).astype(np.uint8)
        mins = np.minimum.reduceat(bits, starts)
        maxs = np.maximum.reduceat(bits, starts)
        out += mins.tobytes()
        out += maxs.tobytes()
    return buckets, bytes(out)


def decode_window(capture: Capture, start: int, count: int) -> bytes:
    """Returns up to ``count`` exact samples starting at ``start``."""
    samples = capture.samples()
    total = int(samples.size)
    start = max(0, min(int(start), total))
    count = max(0, int(count))
    return samples[start:start + count].tobytes()
