"""P8.3 live trace capture: start, decode, stop, dump, parse and render.

The whole sequence runs on one connection so the 4096-record ring still holds
the decode burst when TRACE_STOP is sent; the dump is then collected and
checked for the annotated flow (DMA IRQ -> AcqTask -> decoder -> TxTask ->
UART TX) and rendered to `build/traces/p83-live-uart.png`.
"""

from __future__ import annotations

import time
from pathlib import Path

import pytest

from host.la_cli import protocol as p
from host.la_cli import trace as trace_mod
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri

FCLK_HZ = 100_000_000
DECODE_EVENTS = 64
REQUIRED_TOKENS = ("DM", "SEG", "DEC", "SS", "TX", "IN", "OUT")


@pytest.fixture(scope="module")
def client(request):
    if not request.config.getoption("--live"):
        pytest.skip("P8.3 requires --live (P7 firmware and bench wiring)")
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the live gates require the bench hardware")
    analyzer = AnalyzerClient(open_uri(target))
    yield analyzer
    analyzer.close()


def _stream_config() -> bytes:
    return p.pack_config(
        ctrl_flags=p.CTRL_STREAM,
        sample_div=0,
        mode=1,
        chan_en=0xFF,
        post_samples=0,
        txn_max_run=0x00FFFFFF,
        live_flush_cycles=200_000,
    )


def _configure_patgen_uart(client: AnalyzerClient) -> None:
    """UART 115200 source; disable first so the EN edge latches the mode."""
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(p.PATGEN_EN_MASK | (p.PATGEN_MODE_UART_115200 << 4))


def test_p83_live_uart_trace(client: AnalyzerClient, request) -> None:
    client.trace_control(p.TRACE_START)
    _configure_patgen_uart(client)
    events = client.live(
        p.pack_decoder_cfg_uart(0, 0, 115200, FCLK_HZ),
        config=_stream_config(), count=DECODE_EVENTS, timeout_s=10.0,
    )
    client.trace_control(p.TRACE_STOP)
    lines = client.trace_dump(timeout_s=30.0)

    dump = trace_mod.parse_trace(lines)
    summary = trace_mod.summarize(dump)
    decoded = [event for event in events if not event.get("overrun")]
    assert len(decoded) >= DECODE_EVENTS // 2, len(decoded)

    tokens = summary["counts"]
    for token in REQUIRED_TOKENS:
        assert tokens.get(token, 0) > 0, f"trace has no {token} records: {tokens}"
    task_names = {obj.name for obj in dump.objects.values() if obj.kind == "task"}
    assert {"acq", "dec0", "tx"} <= task_names, task_names

    traces_dir = Path(request.config.rootpath) / "build" / "traces"
    traces_dir.mkdir(parents=True, exist_ok=True)
    dump_path = traces_dir / "p83-live-uart.txt"
    dump_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    png_path = traces_dir / "p83-live-uart.png"
    trace_mod.render_timeline(
        dump, png_path, title="P8.3 live UART decode (115200)",
    )

    # Zoom on the first DMA IRQ so the annotated chain (DMA IRQ -> AcqTask ->
    # decoder -> TxTask -> UART TX) is visible at millisecond scale.
    t0 = dump.records[0].time
    first_dm = next(record for record in dump.records if record.token == "DM")
    dm_ms = (first_dm.time - t0) * 1000.0 / dump.counts_per_second
    zoom_path = traces_dir / "p83-live-uart-zoom.png"
    trace_mod.render_timeline(
        dump, zoom_path, window_ms=(max(0.0, dm_ms - 1.0), dm_ms + 4.0),
        title="P8.3 live UART decode: DMA IRQ -> AcqTask -> decoder -> TxTask",
    )
    print(
        f"P8.3 trace: {summary['records']} records, {summary['dropped']} dropped, "
        f"{summary['duration_s'] * 1000:.1f} ms, tokens {tokens}; "
        f"dump {dump_path}, timelines {png_path}, {zoom_path}"
    )
