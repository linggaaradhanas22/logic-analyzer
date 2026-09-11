"""Capture export: streaming sample reconstruction and .sr/VCD/PNG writers.

Raw packed words decode to four 8-bit samples per word (oldest byte
first, la_raw_packer order). Transition words {run[23:0], sample[7:0]}
decode to run repetitions of sample; reconstruction streams sample by
sample so an RLE capture never expands in memory. The .sr writer spools the
decoded byte stream once to learn the sample count and streams it again
into the zip, bounded by a single 64 KiB buffer.
"""

from __future__ import annotations

import io
import struct
import zipfile
from fractions import Fraction

from . import protocol as p

CHANNELS = 8
CHUNK_BYTES = 65536


def iter_samples(data: bytes, encoding: int):
    """Yields one 8-bit sample per timestamp (streaming)."""
    if encoding == 0:
        for value in data:
            yield value
        return
    if encoding != 1:
        raise ValueError(f"unknown encoding {encoding}")
    offset = 0
    while offset + 4 <= len(data):
        word = struct.unpack_from("<I", data, offset)[0]
        offset += 4
        run = word >> 8
        sample = word & 0xFF
        for _ in range(run):
            yield sample


def count_samples(data: bytes, encoding: int) -> int:
    if encoding == 0:
        return len(data)
    if encoding != 1:
        raise ValueError(f"unknown encoding {encoding}")
    total = 0
    for offset in range(0, len(data), 4):
        word = struct.unpack_from("<I", data, offset)[0]
        total += word >> 8
    return total


def _metadata_text(total_samples: int, samplerate: int) -> bytes:
    lines = [
        "[global]",
        "sigrok version=0.2.0",
        "",
        "[device 1]",
        "capturefile=logic-1",
        f"total probes={CHANNELS}",
        f"samplerate={samplerate}",
        "unitsize=1",
        f"total samples={total_samples}",
    ]
    lines.extend(f"probe{index + 1}=CH{index}" for index in range(CHANNELS))
    return ("\n".join(lines) + "\n").encode("ascii")


def _spool_decoded(data: bytes, encoding: int) -> tuple[int, io.BytesIO]:
    """First streaming pass: writes decoded samples into a spool and returns
    the sample count (bounded memory)."""
    total = 0
    spool = io.BytesIO()
    for sample in iter_samples(data, encoding):
        spool.write(bytes((sample,)))
        total += 1
    spool.seek(0)
    return total, spool


def write_sr(path: str, data: bytes, encoding: int, samplerate: int) -> int:
    """Writes a sigrok .sr capture (zip: version, metadata, logic-1-1)."""
    total, spool = _spool_decoded(data, encoding)
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("version", "1\n")
        archive.writestr("metadata", _metadata_text(total, samplerate))
        with archive.open("logic-1-1", "w") as samples:
            while True:
                block = spool.read(CHUNK_BYTES)
                if not block:
                    break
                samples.write(block)
    return total


def _vcd_timescale(samplerate: int) -> tuple[str, Fraction]:
    """Returns (timescale unit string, samples per time unit). The sample
    period in seconds is Fraction(1, samplerate); we choose the smallest
    integral unit >= the period so per-sample times are >= 1."""
    period_ns = Fraction(10**9, samplerate)
    if period_ns >= 1:
        return "1 ns", Fraction(10**9, samplerate)
    return "1 ps", Fraction(10**12, samplerate)


def _rounded_units(n: int, period_units: Fraction) -> int:
    """round(n * period) in integer time units."""
    scaled = n * period_units
    return (2 * scaled.numerator + scaled.denominator) // (2 * scaled.denominator)


def write_vcd(path: str, data: bytes, encoding: int, samplerate: int) -> int:
    """Writes an 8-channel VCD (timescale chosen from the sample rate)."""
    from vcd import writer as vcd_writer

    timescale, period_units = _vcd_timescale(samplerate)
    total = 0
    previous = [0] * CHANNELS
    with open(path, "w", encoding="ascii", newline="\n") as handle:
        vw = vcd_writer.VCDWriter(
            handle,
            timescale=timescale,
            date="la-cli capture",
            comment=f"logic analyzer capture at {samplerate} Hz",
        )
        variables = [
            vw.register_var("la", f"CH{channel}", "wire", size=1, init=0)
            for channel in range(CHANNELS)
        ]
        for sample in iter_samples(data, encoding):
            for channel in range(CHANNELS):
                bit = (sample >> channel) & 1
                if bit != previous[channel]:
                    vw.change(variables[channel], _rounded_units(total, period_units), bit)
                    previous[channel] = bit
            total += 1
        vw.close()
    return total


def write_png(path: str, data: bytes, encoding: int, samplerate: int,
              max_samples: int = 200000) -> int:
    """Renders the first max_samples of all channels into a stacked PNG."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np

    total = count_samples(data, encoding)
    shown = min(total, max_samples)
    samples = np.zeros(shown, dtype=np.uint8)
    taken = 0
    for sample in iter_samples(data, encoding):
        if taken >= shown:
            break
        samples[taken] = sample
        taken += 1
    _, axes = plt.subplots(figsize=(12, 6))
    time_ns = np.arange(shown) * 1e9 / float(samplerate)
    for channel in range(CHANNELS):
        level = (samples >> channel) & 1
        axes.step(time_ns, level.astype(float) + (CHANNELS - 1 - channel) * 1.1,
                  where="post", linewidth=0.8)
    axes.set_xlabel("time (ns)")
    axes.set_ylabel("channel")
    axes.set_yticks([])
    axes.set_title(f"la-cli capture, {shown} of {total} samples at {samplerate} Hz")
    axes.grid(True, linewidth=0.3)
    plt.tight_layout()
    plt.savefig(path, dpi=110)
    plt.close()
    return shown


def export_capture(path: str, data: bytes, encoding: int, samplerate: int) -> int:
    """Writes the capture by extension; returns the exported sample count."""
    lower = path.lower()
    if lower.endswith(".sr"):
        return write_sr(path, data, encoding, samplerate)
    if lower.endswith(".vcd"):
        return write_vcd(path, data, encoding, samplerate)
    if lower.endswith(".png"):
        return write_png(path, data, encoding, samplerate)
    raise ValueError("output must end in .sr, .vcd or .png")
