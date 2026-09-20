"""Capture export helpers for the unified UI (W02).

``.sr`` and ``.vcd`` reuse the CLI's streaming writers unchanged
(``host/la_cli/convert.py``) through a temporary file, so the UI output is
byte-compatible with ``la-cli capture`` and still opens in PulseView. The two
CSV exports carry a ``# la-ui`` metadata line so the offline opener can
recover the sample rate; the CLI's own CSV output is not changed.
"""

from __future__ import annotations

import io
import tempfile
from pathlib import Path

import numpy as np

from ..la_cli import convert

EVENTS_HEADER = "slot,kind,t_sample,channel,flags,data_hex"


def _temp_path(suffix: str) -> Path:
    handle = tempfile.NamedTemporaryFile(prefix="la-ui-", suffix=suffix,
                                         delete=False)
    handle.close()
    return Path(handle.name)


def sr_bytes(capture) -> bytes:
    path = _temp_path(".sr")
    try:
        convert.write_sr(str(path), capture.data, capture.encoding,
                         capture.sample_rate_hz)
        return path.read_bytes()
    finally:
        path.unlink(missing_ok=True)


def vcd_bytes(capture) -> bytes:
    path = _temp_path(".vcd")
    try:
        convert.write_vcd(str(path), capture.data, capture.encoding,
                          capture.sample_rate_hz)
        return path.read_bytes()
    finally:
        path.unlink(missing_ok=True)


def _kind_name(kind: int) -> str:
    from ..la_cli import protocol as p

    return p.EVENT_NAMES.get(kind, str(kind))


def events_csv_bytes(capture) -> bytes:
    """Decoded-event rows for every slot, ordered by sample time."""
    rows: list[tuple[int, int, int, int, int, str]] = []
    for slot, events in capture.annotations.items():
        for event in events:
            rows.append((int(event["t_sample"]), int(slot), int(event["kind"]),
                         int(event.get("channel", 255)),
                         int(event.get("flags", 0)),
                         str(event.get("data", ""))))
    rows.sort(key=lambda row: (row[0], row[1]))
    buffer = io.StringIO()
    buffer.write(f"# la-ui events sample_rate_hz={capture.sample_rate_hz} "
                 f"capture={capture.id}\n")
    buffer.write(EVENTS_HEADER + "\n")
    for t_sample, slot, kind, channel, flags, data in rows:
        name = _kind_name(kind)
        buffer.write(f"{slot},{name},{t_sample},{channel},{flags},{data}\n")
    return buffer.getvalue().encode("utf-8")


def samples_csv_bytes(capture) -> bytes:
    """One row per sample: ``t,CH0,...,CH7``."""
    samples = capture.samples()
    buffer = io.StringIO()
    buffer.write(f"# la-ui samples sample_rate_hz={capture.sample_rate_hz} "
                 f"capture={capture.id}\n")
    buffer.write("t,CH0,CH1,CH2,CH3,CH4,CH5,CH6,CH7\n")
    if samples.size:
        bits = ((samples[:, None] >> np.arange(8)) & 1).astype(np.uint8)
        table = np.column_stack(
            [np.arange(samples.size, dtype=np.int64), bits])
        np.savetxt(buffer, table, delimiter=",", fmt="%d")
    return buffer.getvalue().encode("utf-8")


EXPORTS = {
    "sr": ("capture.sr", "application/zip", sr_bytes),
    "vcd": ("capture.vcd", "text/plain", vcd_bytes),
    "csv-events": ("events.csv", "text/csv", events_csv_bytes),
    "csv-samples": ("samples.csv", "text/csv", samples_csv_bytes),
}
