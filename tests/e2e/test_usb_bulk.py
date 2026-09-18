"""U03 direct-USB bulk-OUT loss detector (E2E, [HUMAN] bench gate).

Run with the USB-profile firmware loaded and the CDC port open:

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- --target serial://COMx -k test_usb

Without --target the tests skip so the suite stays runnable off-bench.

Each record is one TEST_RECORD frame with a unique u32 id and a unique frame
sequence. The host records every write result and matches the per-record ACK;
the device-side JTAG trace (`la_usb_trace_ring`, dumped with
`fw-zynq/scripts/usb_trace_dump.tcl`) records the same ids at parser delivery,
so a missing ACK can be classified as host->controller loss or
controller->parser loss. One host write is one transport send; it is not
assumed to be one USB packet or one device completion.
"""

from __future__ import annotations

import pytest

from host.la_cli.client import AnalyzerClient, account_test_records
from host.la_cli.transport import TransportError, open_uri

# The recorded 2026-09-18 baseline used 50 ms between host writes.
BASELINE_GAP_S = 0.05

# A single PING is one bulk OUT frame; under the recorded ~50% loss it can be
# discarded, so the fixture retries before declaring the link dead.
PING_ATTEMPTS = 8


@pytest.fixture(scope="module")
def client(request):
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the U03 bulk tests require the bench hardware")
    if not str(target).lower().startswith("serial"):
        pytest.skip("the U03 bulk tests require the direct-USB serial target")
    analyzer = AnalyzerClient(open_uri(target))
    last_error: Exception | None = None
    for _ in range(PING_ATTEMPTS):
        try:
            analyzer.ping(timeout_s=1.5)
            last_error = None
            break
        except TransportError as exc:
            last_error = exc
    if last_error is not None:
        analyzer.close()
        pytest.fail(
            f"USB device did not answer PING after {PING_ATTEMPTS} attempts: "
            f"{last_error}"
        )
    yield analyzer
    analyzer.close()


def _report(accounting: dict) -> str:
    return (
        f"sent={accounting['sent']} acked={accounting['acked']} "
        f"missing={accounting['missing']} duplicates={accounting['duplicates']} "
        f"reordered={accounting['reordered']} naks={accounting['naks']} "
        f"write_errors={accounting['write_errors']} "
        f"crc={accounting['crc_errors']} fmt={accounting['format_errors']}"
    )


def _run(analyzer: AnalyzerClient, count: int, gap_s: float, settle_s: float) -> dict:
    result = analyzer.test_records(range(count), gap_s=gap_s, settle_s=settle_s)
    return account_test_records(result)


def test_usb_numbered_records_12(client) -> None:
    """The pending 12-write experiment from the U03 bring-up log."""
    accounting = _run(client, 12, BASELINE_GAP_S, 3.0)
    assert accounting["clean"], _report(accounting)


def test_usb_numbered_records_12_repeat(client) -> None:
    """Two more loss-free 12-write bursts from the same open session."""
    for _ in range(2):
        accounting = _run(client, 12, BASELINE_GAP_S, 3.0)
        assert accounting["clean"], _report(accounting)


def test_usb_numbered_records_long(client) -> None:
    """A longer numbered stream (200 records, no artificial gap)."""
    accounting = _run(client, 200, 0.0, 15.0)
    assert accounting["clean"], _report(accounting)
