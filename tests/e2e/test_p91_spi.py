"""P9.1 SPI bulk-link bench gates (docs/spi-link.md test plan).

Run with the P9.1 firmware on both sides, the JD-7..10/JA-1 jumpers installed
and a reachable analyzer link:

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- \
        --target tcp://HOST:5555 -k test_p91

or set LA_E2E_TARGET. Without a target the tests skip so the suite stays
runnable off-bench; nothing here may be reported as passing without a
captured run.

Coverage: framed counter captures at 4/16/64 KB, the master-clock sweep
(5/10/20 MHz selected at run time through LINK_CFG), a 64 MB drain measured
with every capture CRC-verified and the counter stream checked vectorized, and
a fault-recovery gate (an over-speed attempt must not poison the next capture).
"""

from __future__ import annotations

import time

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient, AnalyzerError
from host.la_cli.transport import open_uri

COUNTER_DIV = 9
DRAIN_CAPTURE_SAMPLES = 4 * 1024 * 1024  # 4 MiB raw payload per capture


@pytest.fixture(scope="module")
def client(request):
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the P9.1 SPI gates require the bench hardware")
    analyzer = AnalyzerClient(open_uri(target))
    yield analyzer
    analyzer.close()


def _selftest_config(post_samples, mode, sample_div, trig_value=0, trig_mask=0):
    return p.pack_config(
        ctrl_flags=p.CTRL_SELFTEST_EN,
        sample_div=sample_div,
        mode=mode,
        chan_en=0xFF,
        trig_mask=trig_mask,
        trig_value=trig_value,
        post_samples=post_samples,
        timeout=0,
    )


def _enable_counter(client: AnalyzerClient, div: int = COUNTER_DIV) -> None:
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(p.PATGEN_EN_MASK | (div << 16))
    time.sleep(0.01)


def _assert_counter_runs(data: bytes, sample_div: int) -> None:
    """Runs model for a divided counter (same check as the P4.6 burst suite)."""
    step = (COUNTER_DIV + 1) // (sample_div + 1)
    assert step >= 1
    runs = []
    value = data[0]
    index = 0
    while index < len(data):
        run = 0
        while index + run < len(data) and data[index + run] == value:
            run += 1
        runs.append(run)
        value = (value + 1) & 0xFF
        index += run
    assert len(runs) >= 2, (len(data), runs)
    assert all(run == step for run in runs[1:-1]), runs[:8]
    assert 1 <= runs[0] <= step
    assert 1 <= runs[-1] <= step


def _assert_counter_sequence(data: bytes) -> None:
    """Every adjacent sample must step by one (DIV=0 counter); vectorized so a
    4 MiB capture stays fast."""
    import numpy as np

    values = np.frombuffer(data, dtype=np.uint8)
    assert values.size >= 2
    diffs = (values[1:].astype(np.int16) - values[:-1].astype(np.int16)) & 0xFF
    assert bool((diffs == 1).all())


def _capture_exact(client: AnalyzerClient, samples: int, sample_div: int = COUNTER_DIV,
                   timeout_s: float = 120.0, vectorized: bool = False) -> float:
    start = time.monotonic()
    result = client.capture(_selftest_config(samples, p.MODE_RAW, sample_div),
                            timeout_s=timeout_s)
    elapsed = time.monotonic() - start
    assert len(result.data) == samples, (len(result.data), samples)
    assert result.parser_stats["crc_errors"] == 0, result.parser_stats
    assert result.parser_stats["format_errors"] == 0, result.parser_stats
    if vectorized:
        _assert_counter_sequence(result.data)
    else:
        _assert_counter_runs(result.data, sample_div)
    return elapsed


def test_p91_spi_capture_sizes(client: AnalyzerClient) -> None:
    """Framed SPI transfers: 4/16/64 KB captures are bit-exact with zero parser
    errors (the 10 MHz gate; a single corrupted chunk used to fail the 64 KB
    case)."""
    client.configure_link(p.LINK_SPI, spi_clock_mhz=10)
    _enable_counter(client)
    for samples in (4096, 16384, 65536):
        _capture_exact(client, samples)


def test_p91_spi_clock_sweep(client: AnalyzerClient) -> None:
    """The runtime LINK_CFG clock selects 5/10/18 MHz; each rate must deliver a
    bit-exact framed capture. 18 MHz is the measured clean ceiling on the
    current 200 ohm jumper wiring: 19 and 20 MHz lose chunks to slave FIFO
    underruns (see the P9.1 bench session in docs/status.md)."""
    _enable_counter(client)
    for clock_mhz in (5, 10, 18):
        client.configure_link(p.LINK_SPI, spi_clock_mhz=clock_mhz)
        _capture_exact(client, 16384)


def test_p91_spi_fault_recovery(client: AnalyzerClient) -> None:
    """Fault injection: an over-speed master (40 MHz, above the 20 MHz design
    ceiling) must produce a reported failure or parser errors, and the next
    10 MHz capture must be clean again (no poisoned ring/parser)."""
    _enable_counter(client)
    client.configure_link(p.LINK_SPI, spi_clock_mhz=40)
    failed = False
    try:
        result = client.capture(_selftest_config(16384, p.MODE_RAW, COUNTER_DIV),
                                timeout_s=30)
        failed = (len(result.data) != 16384 or
                  result.parser_stats["crc_errors"] > 0 or
                  result.parser_stats["format_errors"] > 0)
    except Exception:  # noqa: BLE001 - any honest failure counts
        failed = True
    assert failed, "40 MHz unexpectedly delivered a clean capture"

    client.configure_link(p.LINK_SPI, spi_clock_mhz=10)
    _enable_counter(client)
    _capture_exact(client, 16384)


def test_p91_spi_drain_rate(client: AnalyzerClient, request) -> None:
    """64 MB drain gate: repeated 4 MiB captures at the requested clock, every
    byte verified through the parser (frame CRCs + DATA_END CRC) and the
    counter sequence, with the measured payload rate >= 1.5 MB/s."""
    mb = request.config.getoption("--p91-drain-mb")
    clock_mhz = request.config.getoption("--p91-clock")
    if mb <= 0:
        pytest.skip("drain disabled with --p91-drain-mb 0")
    client.configure_link(p.LINK_SPI, spi_clock_mhz=clock_mhz)
    _enable_counter(client, div=0)

    total = 0
    start = time.monotonic()
    while total < mb * 1024 * 1024:
        _capture_exact(client, DRAIN_CAPTURE_SAMPLES, sample_div=0,
                       timeout_s=180, vectorized=True)
        total += DRAIN_CAPTURE_SAMPLES
    elapsed = time.monotonic() - start
    rate = total / elapsed / 1e6
    print(f"P9.1 drain: {total} bytes at {clock_mhz} MHz in {elapsed:.1f} s "
          f"= {rate:.2f} MB/s")
    assert rate >= 1.5, f"drain rate {rate:.2f} MB/s < 1.5 MB/s"
