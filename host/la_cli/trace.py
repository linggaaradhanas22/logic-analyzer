"""P8.3 trace dump parsing and timeline rendering.

The Zynq TRACE_CTRL dump arrives as LOG text lines (protocol.md). This module
parses them into objects and records, resolves object ids to names and renders
a task/queue timeline PNG with matplotlib (Agg). The parse result is also
usable on its own: `summarize()` returns the counters used by the tests and
the `la-cli trace` summary.
"""

from __future__ import annotations

from dataclasses import dataclass, field

# Event tokens emitted by fw-zynq/app/src/la_trace.c.
TASK_TOKENS = {"IN", "OUT"}
QUEUE_TOKENS = {"QS", "QR", "QF", "RX"}
STREAM_TOKENS = {"SS", "SR", "SF", "SE"}
ISR_TOKENS = {"PL", "DM"}
MARK_TOKENS = {"SEG", "DEC", "TX", "OVR"}
KNOWN_TOKENS = TASK_TOKENS | QUEUE_TOKENS | STREAM_TOKENS | ISR_TOKENS | MARK_TOKENS

TOKEN_LABELS = {
    "IN": "task in", "OUT": "task out",
    "QS": "queue send", "QR": "queue recv",
    "QF": "queue send fail", "RX": "queue recv fail",
    "SS": "stream send", "SR": "stream recv",
    "SF": "stream send fail", "SE": "stream recv fail",
    "PL": "PL IRQ", "DM": "DMA IRQ",
    "SEG": "segment published", "DEC": "decoder fed",
    "TX": "UART frame", "OVR": "overrun report",
}


class TraceParseError(ValueError):
    """Raised when a dump line cannot be parsed or is out of order."""


@dataclass
class TraceObject:
    kind: str  # "task" or "queue"
    name: str
    id: int = 0
    number: int = 0


@dataclass
class TraceRecord:
    time: int
    token: str
    object: int
    value: int
    id: int


@dataclass
class TraceDump:
    counts_per_second: int = 0
    objects: dict[int, TraceObject] = field(default_factory=dict)
    records: list[TraceRecord] = field(default_factory=list)
    declared_records: int = 0
    total: int = 0
    dropped: int = 0

    @property
    def duration_s(self) -> float:
        if len(self.records) < 2 or self.counts_per_second == 0:
            return 0.0
        span = self.records[-1].time - self.records[0].time
        return span / self.counts_per_second

    def object_name(self, object_id: int) -> str:
        entry = self.objects.get(object_id)
        if entry is not None:
            return entry.name
        return f"obj_{object_id:08x}"


def parse_trace(lines, require_complete: bool = True) -> TraceDump:
    """Parses LOG dump lines. Raises TraceParseError on malformed input.

    The LOG transport is best-effort: under relay pressure a few records can be
    dropped in flight (reported as EVT_OVERRUN stage 2). With
    require_complete=False a short dump is accepted; the caller checks the
    deficit. A dump that declares fewer records than it contains is always
    malformed."""
    dump = TraceDump()
    saw_header = False
    saw_end = False

    for raw in lines:
        line = raw.strip()
        if not line:
            continue
        if line.startswith("LA TRACE "):
            parts = line.split()
            if len(parts) != 4 or parts[2] != "1":
                raise TraceParseError(f"bad trace header: {line!r}")
            try:
                dump.counts_per_second = int(parts[3])
            except ValueError as exc:
                raise TraceParseError(f"bad trace header: {line!r}") from exc
            if dump.counts_per_second <= 0:
                raise TraceParseError(f"bad trace header: {line!r}")
            saw_header = True
            continue
        if line.startswith("TASK "):
            parts = line.split(maxsplit=3)
            if len(parts) != 4:
                raise TraceParseError(f"bad TASK line: {line!r}")
            try:
                number = int(parts[1])
                object_id = int(parts[2], 16)
            except ValueError as exc:
                raise TraceParseError(f"bad TASK line: {line!r}") from exc
            dump.objects[object_id] = TraceObject("task", parts[3], object_id, number)
            continue
        if line.startswith("QUEUE "):
            parts = line.split(maxsplit=2)
            if len(parts) != 3:
                raise TraceParseError(f"bad QUEUE line: {line!r}")
            try:
                object_id = int(parts[1], 16)
            except ValueError as exc:
                raise TraceParseError(f"bad QUEUE line: {line!r}") from exc
            dump.objects[object_id] = TraceObject("queue", parts[2], object_id, 0)
            continue
        if line.startswith("REC "):
            parts = line.split()
            if len(parts) != 6 or parts[0] != "REC":
                raise TraceParseError(f"bad REC line: {line!r}")
            try:
                record = TraceRecord(
                    time=int(parts[1]), token=parts[2], object=int(parts[3]),
                    value=int(parts[4]), id=int(parts[5], 16),
                )
            except ValueError as exc:
                raise TraceParseError(f"bad REC line: {line!r}") from exc
            if record.token not in KNOWN_TOKENS:
                raise TraceParseError(f"unknown trace token: {line!r}")
            dump.records.append(record)
            continue
        if line.startswith("END "):
            parts = line.split()
            if len(parts) != 4:
                raise TraceParseError(f"bad END line: {line!r}")
            try:
                dump.declared_records = int(parts[1])
                dump.total = int(parts[2])
                dump.dropped = int(parts[3])
            except ValueError as exc:
                raise TraceParseError(f"bad END line: {line!r}") from exc
            saw_end = True
            continue
        raise TraceParseError(f"unexpected trace line: {line!r}")

    if not saw_header:
        raise TraceParseError("trace dump has no LA TRACE header")
    if not saw_end:
        raise TraceParseError("trace dump has no END marker")
    if len(dump.records) > dump.declared_records:
        raise TraceParseError(
            f"END declares {dump.declared_records} records, parsed {len(dump.records)}"
        )
    if require_complete and dump.declared_records != len(dump.records):
        raise TraceParseError(
            f"END declares {dump.declared_records} records, parsed {len(dump.records)}"
        )
    return dump


def summarize(dump: TraceDump) -> dict:
    """Counters used by the CLI summary and the tests."""
    counts: dict[str, int] = {}
    for record in dump.records:
        counts[record.token] = counts.get(record.token, 0) + 1
    return {
        "records": len(dump.records),
        "total": dump.total,
        "dropped": dump.dropped,
        "duration_s": dump.duration_s,
        "tasks": sum(1 for obj in dump.objects.values() if obj.kind == "task"),
        "queues": sum(1 for obj in dump.objects.values() if obj.kind == "queue"),
        "counts": counts,
    }


def timeline_model(dump: TraceDump) -> dict:
    """Serializable timeline model for the host UI canvas (W06).

    Times are milliseconds relative to the first record; lanes are tasks,
    spans are busy intervals, and markers carry queue/stream/mark/ISR events
    with the lane they belong to (or ``None`` for ISRs).
    """
    lanes = _task_lanes(dump)
    lane_index = {object_id: index for index, (object_id, _name) in
                  enumerate(lanes)}
    intervals = _run_intervals(dump)
    running_timeline = _current_task_at(dump)
    t0 = dump.records[0].time if dump.records else 0
    cps = dump.counts_per_second or 1

    def rel_ms(count: int) -> float:
        return (count - t0) * 1000.0 / cps

    def running_task_at(when_ms: float):
        current = None
        for sample_ms, task_id in running_timeline:
            if sample_ms > when_ms:
                break
            current = task_id
        return current

    markers = []
    for record in dump.records:
        when = rel_ms(record.time)
        if record.token in ISR_TOKENS:
            markers.append({"t_ms": when, "token": record.token,
                            "lane": None, "value": record.value,
                            "object": record.id})
            continue
        task_id = running_task_at(when)
        lane = lane_index.get(task_id)
        if lane is None:
            continue
        if record.token in QUEUE_TOKENS:
            group = "queue"
        elif record.token in STREAM_TOKENS:
            group = "stream"
        elif record.token in MARK_TOKENS:
            group = "mark"
        else:
            continue
        markers.append({"t_ms": when, "token": record.token, "lane": lane,
                        "value": record.value, "object": record.id,
                        "group": group})
    duration_ms = rel_ms(dump.records[-1].time) if dump.records else 0.0
    return {
        "counts_per_second": dump.counts_per_second,
        "duration_ms": duration_ms,
        "lanes": [{"id": object_id, "name": name,
                   "kind": dump.objects[object_id].kind
                   if object_id in dump.objects else "task"}
                  for object_id, name in lanes],
        "spans": {str(object_id): spans
                  for object_id, spans in intervals.items()},
        "markers": markers,
        "summary": summarize(dump),
        "objects": {str(object_id): {"kind": obj.kind, "name": obj.name}
                    for object_id, obj in dump.objects.items()},
    }


def _task_lanes(dump: TraceDump) -> list[tuple[int, str]]:
    lanes: dict[int, str] = {}
    for object_id, obj in dump.objects.items():
        if obj.kind == "task":
            lanes[object_id] = obj.name
    for record in dump.records:
        if record.token in TASK_TOKENS and record.id not in lanes:
            lanes[record.id] = dump.object_name(record.id)
    return sorted(lanes.items(), key=lambda item: item[1])


def _run_intervals(dump: TraceDump) -> dict[int, list[tuple[float, float]]]:
    """Busy intervals per task from the IN/OUT records (ms since first record)."""
    intervals: dict[int, list[tuple[float, float]]] = {}
    open_at: dict[int, float] = {}
    t0 = dump.records[0].time if dump.records else 0
    cps = dump.counts_per_second
    for record in dump.records:
        when = (record.time - t0) * 1000.0 / cps
        if record.token == "OUT":
            start = open_at.pop(record.id, None)
            if start is not None:
                intervals.setdefault(record.id, []).append((start, when))
        elif record.token == "IN":
            open_at[record.id] = when
    if dump.records:
        end = (dump.records[-1].time - t0) * 1000.0 / cps
        for task_id, start in open_at.items():
            intervals.setdefault(task_id, []).append((start, end))
    return intervals


def _current_task_at(dump: TraceDump) -> list[tuple[float, int | None]]:
    """Timeline of (ms, running_task_id) samples for marker placement."""
    timeline: list[tuple[float, int | None]] = []
    running: int | None = None
    t0 = dump.records[0].time if dump.records else 0
    cps = dump.counts_per_second
    for record in dump.records:
        when = (record.time - t0) * 1000.0 / cps
        if record.token == "IN":
            running = record.id
            timeline.append((when, running))
        elif record.token == "OUT" and running == record.id:
            running = None
            timeline.append((when, running))
    return timeline


def render_timeline(dump: TraceDump, output_path, title: str | None = None,
                    window_ms: tuple[float, float] | None = None) -> None:
    """Renders the task/queue timeline to a PNG (matplotlib Agg).

    Times are milliseconds relative to the first record. `window_ms` limits
    the view to [start, end] ms, which is how the annotated decode burst is
    isolated from the surrounding idle time."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    if not dump.records or dump.counts_per_second <= 0:
        raise TraceParseError("trace has no records to render")
    lanes = _task_lanes(dump)
    if not lanes:
        raise TraceParseError("trace has no tasks to render")
    lane_index = {object_id: index for index, (object_id, _) in enumerate(lanes)}
    intervals = _run_intervals(dump)
    running_timeline = _current_task_at(dump)
    t0 = dump.records[0].time
    cps = dump.counts_per_second

    def rel_ms(count: int) -> float:
        return (count - t0) * 1000.0 / cps

    def running_task_at(when_ms: float) -> int | None:
        current = None
        for sample_ms, task_id in running_timeline:
            if sample_ms > when_ms:
                break
            current = task_id
        return current

    def visible(when_ms: float) -> bool:
        return window_ms is None or window_ms[0] <= when_ms <= window_ms[1]

    fig_height = max(2.4, 0.6 * len(lanes) + 1.2)
    fig, axis = plt.subplots(figsize=(11, fig_height))
    colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]

    for lane, (object_id, name) in enumerate(lanes):
        spans = [span for span in intervals.get(object_id, [])
                 if window_ms is None or
                 (span[1] >= window_ms[0] and span[0] <= window_ms[1])]
        if spans:
            axis.broken_barh(
                spans, (lane - 0.35, 0.7),
                facecolors=colors[lane % len(colors)], alpha=0.55,
            )

    for record in dump.records:
        when = rel_ms(record.time)
        if not visible(when):
            continue
        if record.token in ISR_TOKENS:
            axis.axvline(when, color="crimson", linestyle="--", linewidth=0.8,
                         alpha=0.7)
            axis.annotate(record.token, (when, len(lanes) - 0.4),
                          fontsize=6, color="crimson", rotation=90,
                          ha="right", va="top")
            continue
        task_id = running_task_at(when)
        if task_id not in lane_index:
            continue
        lane = lane_index[task_id]
        if record.token in QUEUE_TOKENS:
            axis.plot(when, lane - 0.35, marker="v", markersize=3,
                      color="black", alpha=0.6)
        elif record.token in STREAM_TOKENS:
            axis.plot(when, lane + 0.35, marker="^", markersize=3,
                      color="navy", alpha=0.6)
        elif record.token in MARK_TOKENS:
            axis.plot(when, lane, marker="*", markersize=5,
                      color="darkgreen", alpha=0.8)

    axis.set_yticks(range(len(lanes)))
    axis.set_yticklabels([name for _id, name in lanes], fontsize=8)
    axis.set_ylim(-0.8, len(lanes) - 0.2)
    axis.set_xlabel("time since first record (ms)")
    axis.grid(True, axis="x", alpha=0.3)
    if window_ms is not None:
        axis.set_xlim(*window_ms)
    if title:
        axis.set_title(title, fontsize=10)
    summary = summarize(dump)
    axis.text(
        0.99, 1.02,
        f"{summary['records']} records, {summary['dropped']} dropped, "
        f"{summary['duration_s'] * 1000:.1f} ms total",
        transform=axis.transAxes, ha="right", va="bottom", fontsize=8,
    )
    fig.tight_layout()
    fig.savefig(output_path, dpi=140)
    plt.close(fig)
