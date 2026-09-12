"""P9.2 measurements: burst raw-vs-transition sizes and live event bandwidth.

The step (counter/walking-1/PRBS8) and UART transition counts are computed
from the same bit-exact waveform models that the P6.2 matrix asserts against
hardware, so no large payload has to cross the relay; SPI and I2C are
captured on the bench in both encodings because they have no sample model.
All rows are written to `build/reports/p92-sizes.json` (ignored evidence).

The live comparison runs the same 115200 UART stream once in the v1
EVT_DECODED encoding and once in the compact EVT_BLOCK encoding and compares
the received wire bytes per decoded event; both runs must decode the
identical byte stream.
"""

from __future__ import annotations

import json
import time
from pathlib import Path

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri
from tests.e2e.test_e2e_burst import (
    POST_SAMPLES, RATE_SAMPLE_DIV, RATES, STEP_DIV, UART_BIT_CYCLES,
    UART_DIV, _assert_clean, _patgen_ctrl, _step_sequence, _trigger_flags,
    _uart_waveform,
)
from tests.e2e.test_e2e_live import (
    FCLK_HZ, _aligned_frame_bytes, _configure_patgen_uart, _stream_config,
    _uart_decoder,
)

COMPACT_EVENTS = 600
MODEL_MODES = ("counter", "walking", "prbs", "uart")


def _report_add(request, key: str, entry: dict) -> None:
    report_path = Path(request.config.rootpath) / "build" / "reports" / "p92-sizes.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = {}
    if report_path.is_file():
        report = json.loads(report_path.read_text(encoding="utf-8"))
    report[key] = entry
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def _model_stream(mode: str, rate: str, mask: int) -> bytes:
    if mode == "uart":
        samples_per_bit = (
            (UART_BIT_CYCLES * (UART_DIV[rate] + 1)) // (RATE_SAMPLE_DIV[rate] + 1)
        )
        return _uart_waveform(samples_per_bit)
    step = (STEP_DIV + 1) // (RATE_SAMPLE_DIV[rate] + 1)
    values = [value & mask for value in _step_sequence(mode)]
    return bytes(value for value in values for _ in range(step))


def _transition_words(stream: bytes, samples: int) -> int:
    """txn words for a window: the initial run plus one word per change."""
    window = (stream * (samples // len(stream) + 2))[:samples]
    changes = sum(1 for index in range(1, samples) if window[index] != window[index - 1])
    return changes + 1


@pytest.mark.parametrize("mode", MODEL_MODES)
@pytest.mark.parametrize("rate", RATES)
def test_model_raw_vs_transition_size(request, mode: str, rate: str) -> None:
    mask = request.config.getoption("--channel-mask")
    samples = POST_SAMPLES[mode][rate]
    stream = _model_stream(mode, rate, mask)
    words = _transition_words(stream, samples)
    raw_bytes = ((samples + 3) // 4) * 4
    txn_bytes = words * 4
    ratio = txn_bytes / raw_bytes
    _report_add(request, f"{mode}@{rate}", {
        "source": "model (P6.2 bit-exact)",
        "samples": samples,
        "raw_words": (samples + 3) // 4,
        "txn_words": words,
        "raw_bytes": raw_bytes,
        "txn_bytes": txn_bytes,
        "txn_over_raw": round(ratio, 3),
    })
    print(
        f"P9.2 size {mode}@{rate}: samples={samples} raw={raw_bytes} B "
        f"txn={txn_bytes} B ({words} transitions) ratio={ratio:.3f}"
    )
    assert words >= 1
    assert ratio > 0.0


@pytest.fixture(scope="module")
def client(request):
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the P9.2 measurements require the bench hardware")
    analyzer = AnalyzerClient(open_uri(target))
    yield analyzer
    analyzer.close()


@pytest.mark.parametrize("mode", ("spi", "i2c"))
@pytest.mark.parametrize("rate", RATES)
def test_burst_raw_vs_transition_size(client: AnalyzerClient, request,
                                      mode: str, rate: str) -> None:
    external = request.config.getoption("--external-loopback")
    channel_mask = request.config.getoption("--channel-mask")
    # The ratio is scale-invariant; keep the window small so no capture
    # exceeds 16 KiB and the ESP32 relay is not stressed (large transfers
    # wedge the relay intermittently, see docs/status.md).
    samples = min(POST_SAMPLES[mode][rate], 4096)
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(_patgen_ctrl(mode, rate))
    time.sleep(0.01)
    common = dict(
        ctrl_flags=0 if external else p.CTRL_SELFTEST_EN,
        sample_div=RATE_SAMPLE_DIV[rate],
        chan_en=channel_mask,
        post_samples=samples,
        timeout=0,
        **_trigger_flags(mode),
    )
    raw = client.capture(p.pack_config(mode=p.MODE_RAW, **common), timeout_s=30)
    _assert_clean(raw, client)
    txn = client.capture(p.pack_config(mode=p.MODE_TXN, **common), timeout_s=30)
    _assert_clean(txn, client)

    raw_bytes = raw.meta["total_words"] * 4
    txn_bytes = txn.meta["total_words"] * 4
    ratio = txn_bytes / raw_bytes
    _report_add(request, f"{mode}@{rate}", {
        "source": "bench capture",
        "samples": samples,
        "raw_words": raw.meta["total_words"],
        "txn_words": txn.meta["total_words"],
        "raw_bytes": raw_bytes,
        "txn_bytes": txn_bytes,
        "txn_over_raw": round(ratio, 3),
    })
    print(
        f"P9.2 size {mode}@{rate}: samples={samples} raw={raw_bytes} B "
        f"txn={txn_bytes} B ({txn.meta['total_words']} transitions) "
        f"ratio={ratio:.3f}"
    )
    assert raw.meta["total_words"] == (samples + 3) // 4
    assert txn.meta["total_words"] > 0


def _run_live(client: AnalyzerClient, decoder: bytes, config: bytes,
              event_format: int, count: int | None = None,
              seconds: float | None = None) -> tuple[list[dict], dict]:
    """One live session with a fresh patgen enable (the P7.6 helper toggles it
    because a session started on an already-running generator streams almost
    nothing)."""
    _configure_patgen_uart(client, 115200)
    events = client.live(
        decoder, config=config, count=count,
        timeout_s=seconds if count is None else 15.0,
        stop_on_overrun=False, collect_events=count is not None,
        event_format=event_format,
    )
    return events, dict(client.live_stats)


def test_compact_events_reduce_live_wire_bytes(client: AnalyzerClient, request) -> None:
    if not request.config.getoption("--live"):
        pytest.skip("P9.2 live comparison requires --live")
    config = _stream_config(1)
    decoder = _uart_decoder(115200)

    # Correctness: the same event window must decode the same bytes.
    # One connection only: the gateway enforces a single client, so this test
    # shares the module client instead of opening a second socket.
    v1_events, _ = _run_live(client, decoder, config,
                             p.EVT_FORMAT_RECORDS, count=COMPACT_EVENTS)
    compact_events, _ = _run_live(client, decoder, config,
                                  p.EVT_FORMAT_BLOCK, count=COMPACT_EVENTS)
    v1_data = _aligned_frame_bytes(v1_events)
    compact_data = _aligned_frame_bytes(compact_events)
    common = min(len(v1_data), len(compact_data))
    assert common >= COMPACT_EVENTS // 4, (len(v1_data), len(compact_data))
    assert v1_data[:common] == compact_data[:common], "different decoded bytes"

    # Bandwidth: fixed 2 s windows at the same offered rate; the in-flight
    # relay data at a count stop otherwise dilutes the comparison.
    _, v1_stats = _run_live(client, decoder, config,
                            p.EVT_FORMAT_RECORDS, seconds=2.0)
    _, compact_stats = _run_live(client, decoder, config,
                                 p.EVT_FORMAT_BLOCK, seconds=2.0)
    assert v1_stats["events"] > 1000 and compact_stats["events"] > 1000
    v1_per_event = v1_stats["wire_bytes"] / v1_stats["events"]
    compact_per_event = compact_stats["wire_bytes"] / compact_stats["events"]
    saving = 100.0 * (1.0 - compact_per_event / v1_per_event)
    print(
        f"P9.2 live UART 115200: v1 {v1_stats['wire_bytes']} B for "
        f"{v1_stats['events']} events ({v1_per_event:.2f} B/event), "
        f"compact {compact_stats['wire_bytes']} B for "
        f"{compact_stats['events']} events ({compact_per_event:.2f} B/event), "
        f"saving {saving:.1f}%"
    )
    assert compact_per_event < v1_per_event * 0.65
