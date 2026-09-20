"""W06: the trace timeline model used by the UI canvas."""

from __future__ import annotations

import pytest

from host.la_cli import trace as trace_mod

DUMP_LINES = [
    "LA TRACE 1 333333333",
    "TASK 1 00100000 acq",
    "TASK 2 00100001 tx",
    "QUEUE 00200000 seg0",
    "REC 1000 IN 1 0 00100000",
    "REC 1100 QS 0 0 00200000",
    "REC 1200 OUT 1 0 00100000",
    "REC 1150 TX 0 53 00000000",
    "REC 1400 DM 0 1 00000000",
    "REC 1500 IN 2 0 00100001",
    "REC 1600 SS 0 4 00000000",
    "REC 1700 OUT 2 0 00100001",
    "END 8 8 0",
]


def test_timeline_model_shape():
    dump = trace_mod.parse_trace(DUMP_LINES)
    model = trace_mod.timeline_model(dump)
    assert model["counts_per_second"] == 333333333
    assert model["summary"]["records"] == 8
    assert [lane["name"] for lane in model["lanes"]] == ["acq", "tx"]
    # Object ids are the hex values from the TASK lines (0x00100000, 0x00100001).
    assert model["spans"]["1048576"][0][0] == pytest.approx(0.0)
    assert model["spans"]["1048576"][0][1] == pytest.approx(0.0006)
    assert model["spans"]["1048577"][0][0] == pytest.approx(0.0015)
    assert model["spans"]["1048577"][0][1] == pytest.approx(0.0021)
    groups = {marker["group"] for marker in model["markers"]
              if marker["lane"] is not None}
    assert groups == {"queue", "mark", "stream"}
    isr = [marker for marker in model["markers"] if marker["token"] == "DM"]
    assert isr and isr[0]["lane"] is None
    assert model["duration_ms"] > 0.002


def test_timeline_model_marks_are_ordered():
    dump = trace_mod.parse_trace(DUMP_LINES)
    model = trace_mod.timeline_model(dump)
    times = [marker["t_ms"] for marker in model["markers"]]
    assert times == sorted(times)
