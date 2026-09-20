"""Offline capture openers for the unified UI (W07).

Supported inputs, detected from the uploaded file name:

- ``.sr``: the project's sigrok zip export (``metadata`` + ``logic-1-1``);
- ``.vcd``: the project's VCD export (a small parser for the subset our
  writer emits: 8 ``CHn`` probes, a ``$timescale`` and binary value changes);
- ``.csv``: the UI's sample CSV (``t,CH0..CH7``) or event CSV
  (``slot,kind,t_sample,...``). The UI writes a ``# la-ui`` metadata line so
  the sample rate survives a round trip; a file without it defaults to
  100 MHz (the analyzer's maximum rate) and says so in its warnings.

An opened file becomes a normal capture in the session store, so the viewer,
decode, measurement and export paths are identical to a device capture.
"""

from __future__ import annotations

import csv
import io
import re
import zipfile
from pathlib import Path

import numpy as np

from ..la_cli import protocol as p
from .store import Capture

DEFAULT_SAMPLE_RATE_HZ = 100_000_000
_UNIT_HZ = {"s": 1, "ms": 10 ** 3, "us": 10 ** 6, "ns": 10 ** 9,
            "ps": 10 ** 12, "fs": 10 ** 15}


class OpenError(ValueError):
    """The uploaded file is not a supported or valid capture."""


def open_capture(name: str, data: bytes) -> Capture:
    suffix = Path(name).suffix.lower()
    if suffix == ".sr":
        return _open_sr(name, data)
    if suffix == ".vcd":
        return _open_vcd(name, data)
    if suffix == ".csv":
        return _open_csv(name, data)
    raise OpenError(f"unsupported file type {suffix or '(none)'}; use .sr, .vcd or .csv")


def _open_sr(name: str, data: bytes) -> Capture:
    try:
        archive = zipfile.ZipFile(io.BytesIO(data))
    except zipfile.BadZipFile as exc:
        raise OpenError(f"{name}: not a valid .sr zip") from exc
    with archive:
        names = set(archive.namelist())
        if "metadata" not in names or "logic-1-1" not in names:
            raise OpenError(f"{name}: .sr is missing metadata or logic-1-1")
        metadata = archive.read("metadata").decode("ascii", errors="replace")
        sample_rate = 0
        total = 0
        probes = 0
        for line in metadata.splitlines():
            key, _, value = line.partition("=")
            key = key.strip().lower()
            try:
                if key == "samplerate":
                    sample_rate = int(value)
                elif key == "total samples":
                    total = int(value)
                elif key == "total probes":
                    probes = int(value)
            except ValueError as exc:
                raise OpenError(f"{name}: bad metadata value {value!r}") from exc
        samples = archive.read("logic-1-1")
    if probes and probes != 8:
        raise OpenError(f"{name}: expected 8 probes, file has {probes}")
    if total and total != len(samples):
        raise OpenError(
            f"{name}: metadata declares {total} samples, file has {len(samples)}")
    if not samples:
        raise OpenError(f"{name}: capture has no samples")
    warnings = []
    if not sample_rate:
        sample_rate = DEFAULT_SAMPLE_RATE_HZ
        warnings.append("metadata had no samplerate; assuming 100 MHz")
    return Capture(encoding=0, sample_rate_hz=sample_rate, data=samples,
                   total_words=(len(samples) + 3) // 4, name=name,
                   source=f"open:{name}", warnings=warnings)


def _open_vcd(name: str, data: bytes) -> Capture:
    text = data.decode("ascii", errors="replace")
    timescale_mag = 1
    timescale_unit = "ns"
    code_to_channel: dict[str, int] = {}
    changes: list[tuple[int, int, int]] = []
    current_time = 0
    saw_timescale = False
    lines = text.splitlines()
    index = 0
    while index < len(lines):
        line = lines[index].strip()
        index += 1
        if not line:
            continue
        if line.startswith("$timescale"):
            parts = [line]
            while "$end" not in parts[-1] and index < len(lines):
                parts.append(lines[index].strip())
                index += 1
            match = re.search(r"(\d+)\s*(fs|ps|ns|us|ms|s)", " ".join(parts))
            if match:
                timescale_mag = int(match.group(1))
                timescale_unit = match.group(2)
                saw_timescale = True
            continue
        if line.startswith("$var"):
            parts = line.split()
            if len(parts) >= 5:
                code, ref = parts[3], parts[4]
                match = re.fullmatch(r"CH(\d+)", ref)
                if match:
                    code_to_channel[code] = int(match.group(1))
            continue
        if line.startswith("$"):
            continue
        if line.startswith("#"):
            try:
                current_time = int(line[1:])
            except ValueError as exc:
                raise OpenError(f"{name}: bad VCD timestamp {line!r}") from exc
            continue
        value, code = line[0], line[1:]
        channel = code_to_channel.get(code)
        if channel is None or value not in "01":
            continue
        changes.append((current_time, channel, int(value)))
    if not saw_timescale:
        raise OpenError(f"{name}: VCD has no $timescale")
    if not code_to_channel:
        raise OpenError(f"{name}: VCD has no CH0..CH7 probes")
    if not changes:
        raise OpenError(f"{name}: VCD has no value changes")

    # Our writer emits integer time units per sample; recover the sample rate
    # from the timescale by choosing a rate whose sample period divides the
    # unit (the writer's own inverse mapping).
    unit_hz = _UNIT_HZ[timescale_unit]
    sample_rate = DEFAULT_SAMPLE_RATE_HZ
    warnings: list[str] = []
    # Reconstruct sample indices from the VCD times. The project's writer uses
    # a timescale whose unit is at most one sample period, so index =
    # round(t * rate / unit_hz) with the requested rate; without metadata we
    # take the smallest power-of-ten rate that makes the last index an
    # integer.
    max_time = changes[-1][0]
    if max_time <= 0:
        raise OpenError(f"{name}: VCD has no positive timestamps")
    for candidate in (100_000_000, 10_000_000, 1_000_000, 100_000, 10_000,
                      1_000, 100, 10, 1):
        scaled = max_time * candidate / unit_hz
        if abs(scaled - round(scaled)) < 1e-9:
            sample_rate = candidate
            break
    warnings.append(
        f"VCD has no sample rate metadata; reconstructed at {sample_rate} Hz")
    samples_per_unit = sample_rate / unit_hz
    last_index = int(round(max_time * samples_per_unit))
    samples = np.zeros(last_index + 1, dtype=np.uint8)
    for when, channel, value in changes:
        sample_index = int(round(when * samples_per_unit))
        if sample_index > last_index:
            continue
        if value:
            samples[sample_index:] |= np.uint8(1 << channel)
        else:
            samples[sample_index:] &= np.uint8(~(1 << channel) & 0xFF)
    return Capture(encoding=0, sample_rate_hz=sample_rate,
                   data=samples.tobytes(), total_words=(samples.size + 3) // 4,
                   name=name, source=f"open:{name}", warnings=warnings)


def _open_csv(name: str, data: bytes) -> Capture:
    text = data.decode("utf-8", errors="replace")
    lines = text.splitlines()
    sample_rate = 0
    while lines and lines[0].lstrip().startswith("#"):
        match = re.search(r"sample_rate_hz=(\d+)", lines[0])
        if match:
            sample_rate = int(match.group(1))
        lines.pop(0)
    if not lines:
        raise OpenError(f"{name}: CSV is empty")
    header = lines[0].strip().lower()
    if header.startswith("t,") or header.startswith("time,"):
        return _open_samples_csv(name, lines, sample_rate)
    if header.startswith("slot,") or header.startswith("kind,"):
        return _open_events_csv(name, lines, sample_rate)
    raise OpenError(
        f"{name}: unrecognized CSV header {lines[0]!r}; expected "
        f"'t,CH0..CH7' or 'slot,kind,...'")


def _open_samples_csv(name: str, lines: list[str], sample_rate: int) -> Capture:
    warnings: list[str] = []
    if not sample_rate:
        sample_rate = DEFAULT_SAMPLE_RATE_HZ
        warnings.append("CSV had no sample rate; assuming 100 MHz")
    rows = [line for line in lines[1:] if line.strip()]
    if not rows:
        raise OpenError(f"{name}: sample CSV has no rows")
    try:
        table = np.array([[int(value) for value in row.split(",")[:9]]
                          for row in rows], dtype=np.int64)
    except ValueError as exc:
        raise OpenError(f"{name}: sample CSV has a non-numeric row") from exc
    if table.shape[1] < 9:
        raise OpenError(f"{name}: sample CSV needs t,CH0..CH7 columns")
    bits = (table[:, 1:9] & 1).astype(np.uint8)
    packed = (bits * (1 << np.arange(8, dtype=np.int64))).sum(axis=1).astype(np.uint8)
    return Capture(encoding=0, sample_rate_hz=sample_rate, data=packed.tobytes(),
                   total_words=(packed.size + 3) // 4, name=name,
                   source=f"open:{name}", warnings=warnings)


def _open_events_csv(name: str, lines: list[str], sample_rate: int) -> Capture:
    warnings = ["event CSV has no signal samples; annotations only"]
    if not sample_rate:
        sample_rate = DEFAULT_SAMPLE_RATE_HZ
        warnings.append("CSV had no sample rate; assuming 100 MHz")
    annotations: dict[int, list[dict]] = {}
    max_t = 0
    reader = csv.DictReader(lines)
    for row in reader:
        try:
            slot = int(row.get("slot") or 0)
            t_sample = int(row.get("t_sample") or 0)
        except ValueError as exc:
            raise OpenError(f"{name}: event CSV row has bad numbers") from exc
        kind_text = (row.get("kind") or "").strip()
        kind = _kind_from_text(kind_text)
        data_hex = (row.get("data_hex") or "").strip()
        annotations.setdefault(slot, []).append({
            "kind": kind,
            "name": p.EVENT_NAMES.get(kind, kind_text or str(kind)),
            "t_sample": t_sample,
            "channel": int(row.get("channel") or 255),
            "flags": int(row.get("flags") or 0),
            "data": data_hex,
        })
        max_t = max(max_t, t_sample)
    if not annotations:
        raise OpenError(f"{name}: event CSV has no rows")
    capture = Capture(encoding=0, sample_rate_hz=sample_rate, data=b"",
                      total_words=0, name=name, source=f"open:{name}",
                      annotations=annotations, warnings=warnings)
    capture.total_samples = max_t + 1
    return capture


def _kind_from_text(text: str) -> int:
    if not text:
        return 0
    if text.isdigit():
        return int(text)
    for kind, name in p.EVENT_NAMES.items():
        if name == text.upper():
            return kind
    return 0
