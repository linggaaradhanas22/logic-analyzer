"""P4.6/P4.10 Zynq burst-path bench tests (E2E, [HUMAN] bench gate).

Run with the P4 firmware loaded and a reachable analyzer link:

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- --target tcp://HOST:5555

or set LA_E2E_TARGET. Without a target the tests skip so the suite stays
runnable off-bench; nothing here may be reported as passing without a
captured run.

Coverage: back-to-back captures (DMA reset/reuse without buffer reuse races),
multi-BD chains (four ordinary 64 KiB SG descriptors), short tails (small
word-aligned raw and data-dependent txn packets), abort while armed and the
capacity-overflow NAK. The counter patgen runs over the internal selftest mux,
so a captured sample stream is verified against its model, which also proves
DMA writes followed by CPU readback of the non-cached capture pool (P4.3).
"""

from __future__ import annotations

import time

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient, AnalyzerError
from host.la_cli.transport import open_uri

POOL_BYTES = 8 * 1024 * 1024
COUNTER_DIV = 99  # patgen counter steps every 100 FCLK cycles


@pytest.fixture(scope="module")
def client(request):
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the E2E burst tests require the bench hardware")
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


def _enable_counter(client: AnalyzerClient) -> None:
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(p.PATGEN_EN_MASK | (COUNTER_DIV << 16))
    time.sleep(0.01)


def _assert_counter(data: bytes, sample_div: int) -> None:
    """The free-running counter holds each value for exactly (DIV+1)/(div+1)
    samples; the first and last runs may be partial because the capture is
    asynchronous to the generator phase."""
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
    assert all(run == step for run in runs[1:-1]), runs
    assert 1 <= runs[0] <= step, runs
    assert 1 <= runs[-1] <= step, runs


def _assert_clean(result, client: AnalyzerClient) -> None:
    assert result.parser_stats["crc_errors"] == 0
    assert result.parser_stats["format_errors"] == 0
    assert not (client.status()["status_reg"] & 0x10), "FIFO overflow set"


def test_ping_and_idle(client: AnalyzerClient) -> None:
    pong = client.ping()
    assert pong["pl_id"] == 0x4C410002, pong
    status = client.status()
    assert status["state"] == 0, status
    assert not (status["status_reg"] & 0x2), "must not be ARMED"


def test_consecutive_runs(client: AnalyzerClient, request) -> None:
    """P4.6 reset/reuse: back-to-back captures stay exact and never reuse a
    buffer while the previous burst is still on the wire."""
    repeat = request.config.getoption("--burst-repeat")
    _enable_counter(client)
    for run in range(repeat):
        result = client.capture(_selftest_config(16000, p.MODE_RAW, 9), timeout_s=30)
        assert result.meta["total_words"] == 4000, (run, result.meta)
        _assert_counter(result.data, sample_div=9)
        _assert_clean(result, client)


def test_multi_bd_capture(client: AnalyzerClient) -> None:
    """P4.6 multi-BD: 200000 samples = 50000 words = 200 KB spread over four
    ordinary 64 KiB SG descriptors, exact counter and no overflow."""
    _enable_counter(client)
    result = client.capture(_selftest_config(200000, p.MODE_RAW, 9), timeout_s=60)
    assert result.meta["total_words"] == 50000, result.meta
    _assert_counter(result.data, sample_div=9)
    _assert_clean(result, client)


def test_short_tail_captures(client: AnalyzerClient) -> None:
    """P4.6 short tail: a small word-aligned raw packet and a data-dependent
    txn packet both complete exactly on a single short BD."""
    _enable_counter(client)
    raw = client.capture(_selftest_config(4096, p.MODE_RAW, 9), timeout_s=30)
    assert raw.meta["total_words"] == 1024, raw.meta
    _assert_counter(raw.data, sample_div=9)
    _assert_clean(raw, client)

    txn = client.capture(_selftest_config(16000, p.MODE_TXN, 9), timeout_s=30)
    runs = [
        int.from_bytes(txn.data[i:i + 4], "little") >> 8
        for i in range(0, len(txn.data), 4)
    ]
    assert sum(runs) == 16000, (len(txn.data), sum(runs))
    _assert_clean(txn, client)


def test_abort_while_armed(client: AnalyzerClient) -> None:
    """P4.6 abort: a never-triggering ARM aborts cleanly (the PL datapath is
    soft-reset) and the next capture still works."""
    client.configure_patgen(0)  # SELFTEST with patgen low: level 0xA5 never fires
    config = _selftest_config(16000, p.MODE_RAW, 9, trig_value=0xA5, trig_mask=0xFF)
    client.arm(config, timeout_s=5.0)
    time.sleep(0.05)
    assert client.status()["status_reg"] & 0x2, "expected ARMED"
    client.abort()
    status = client.status()
    assert status["state"] == 0, status
    _enable_counter(client)
    result = client.capture(_selftest_config(4096, p.MODE_RAW, 9), timeout_s=30)
    assert result.meta["total_words"] == 1024, result.meta
    _assert_counter(result.data, sample_div=9)


def test_abort_during_capture(client: AnalyzerClient) -> None:
    """P4.6 abort mid-capture: the partial PL data is discarded and the next
    capture is exact (no stale FIFO/packer state in the new descriptor
    chain)."""
    _enable_counter(client)
    long_config = _selftest_config(1000000, p.MODE_RAW, 99)  # 1 s at 1M
    client.arm(long_config, timeout_s=5.0)
    time.sleep(0.2)
    client.abort()
    assert client.status()["state"] == 0
    _enable_counter(client)
    result = client.capture(_selftest_config(4096, p.MODE_RAW, 9), timeout_s=30)
    assert result.meta["total_words"] == 1024, result.meta
    _assert_counter(result.data, sample_div=9)


def test_capacity_overflow_nak(client: AnalyzerClient) -> None:
    """P4.6 capacity error: an ARM whose raw plan exceeds the 8 MiB pool is
    rejected with NAK overflow before the DMA is touched; the machine stays
    idle and the next capture works."""
    _enable_counter(client)
    oversize_samples = POOL_BYTES + 4096
    config = _selftest_config(oversize_samples, p.MODE_RAW, 9)
    with pytest.raises(AnalyzerError) as excinfo:
        client.capture(config, timeout_s=10)
    assert excinfo.value.error_code == p.ERROR_OVERFLOW
    status = client.status()
    assert status["state"] == 0, "machine must stay idle after the NAK"
    result = client.capture(_selftest_config(4096, p.MODE_RAW, 9), timeout_s=30)
    assert result.meta["total_words"] == 1024
    _assert_counter(result.data, sample_div=9)
