"""U06 direct-USB drain gate.

Run with the USB-profile firmware loaded (the profile rejects LINK_CFG, so no
transport switch is sent) and the CDC port free:

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- \\
        --target serial://COMx -k test_usb_drain

Transfers `--usb-drain-mb` MiB (default 64) as repeated 4 MiB raw captures,
verifying every byte against the free-running counter (DIV=0), the frame CRC
statistics and the final DATA_END CRC. Three consecutive trials must each
reach >= 1.5 MB/s measured from before the capture loop to after host
verification (command, acquisition, transport and verification time included).
"""

from __future__ import annotations

import time

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri

DRAIN_CAPTURE_SAMPLES = 4 * 1024 * 1024  # 4 MiB raw payload per capture
DRAIN_MIN_RATE_MBPS = 1.5


@pytest.fixture()
def client(request):
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the U06 drain gate requires the bench hardware")
    if not str(target).lower().startswith("serial"):
        pytest.skip("the U06 drain gate requires the direct-USB serial target")
    analyzer = AnalyzerClient(open_uri(target))
    yield analyzer
    analyzer.close()


def _selftest_config(post_samples: int) -> bytes:
    return p.pack_config(
        ctrl_flags=p.CTRL_SELFTEST_EN,
        sample_div=0,
        mode=p.MODE_RAW,
        chan_en=0xFF,
        trig_mask=0,
        trig_value=0,
        post_samples=post_samples,
        timeout=0,
    )


def _enable_counter(client: AnalyzerClient) -> None:
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(p.PATGEN_EN_MASK)
    time.sleep(0.01)


def _assert_counter_sequence(data: bytes) -> None:
    """Every adjacent sample must step by one (DIV=0 counter); vectorized so a
    4 MiB capture stays fast."""
    import numpy as np

    values = np.frombuffer(data, dtype=np.uint8)
    assert values.size >= 2
    diffs = (values[1:].astype(np.int16) - values[:-1].astype(np.int16)) & 0xFF
    assert bool((diffs == 1).all())


def test_usb_drain_rate(client: AnalyzerClient, request) -> None:
    """64 MiB (16 x 4 MiB) per trial; every trial must reach >= 1.5 MB/s."""
    mb = request.config.getoption("--usb-drain-mb")
    if mb <= 0:
        pytest.skip("drain disabled with --usb-drain-mb 0")
    _enable_counter(client)
    # The drain gate profiles the host pacing: ACK every 4 chunks and poll
    # without a timeout tail, which keeps the device's 16-chunk window full.
    client.ack_batch = 4
    total_target = mb * 1024 * 1024
    rates: list[float] = []
    for trial in range(3):
        total = 0
        start = time.monotonic()
        while total < total_target:
            result = client.capture(_selftest_config(DRAIN_CAPTURE_SAMPLES),
                                    timeout_s=120.0, recv_cap=0.0)
            assert len(result.data) == DRAIN_CAPTURE_SAMPLES, len(result.data)
            assert result.parser_stats["crc_errors"] == 0, result.parser_stats
            assert result.parser_stats["format_errors"] == 0, result.parser_stats
            _assert_counter_sequence(result.data)
            total += len(result.data)
        elapsed = time.monotonic() - start
        rate = total / elapsed / 1_000_000
        rates.append(rate)
        print(f"U06 trial {trial + 1}: {total} bytes in {elapsed:.1f} s "
              f"= {rate:.2f} MB/s")
    assert all(rate >= DRAIN_MIN_RATE_MBPS for rate in rates), rates
