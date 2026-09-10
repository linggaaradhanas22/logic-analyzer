"""E2E options: every test here needs a running analyzer link.

Select the link explicitly with `--target tcp://host:5555` (or
`serial://COMx`), or set `LA_E2E_TARGET`; without one the E2E tests are
skipped so the suite stays runnable off-bench.
"""

from __future__ import annotations


def pytest_addoption(parser) -> None:
    parser.addoption(
        "--target",
        default=None,
        help="analyzer link URI for the E2E suite, e.g. tcp://192.168.0.10:5555 "
        "or serial://COM7 (without it the E2E tests are skipped)",
    )
    parser.addoption(
        "--burst-repeat",
        type=int,
        default=3,
        help="consecutive burst capture repetitions (P4.6/P6.2)",
    )
    parser.addoption(
        "--loopback-bytes",
        type=int,
        default=10_000_000,
        help="P5.5 loopback payload bytes (default 10 MB)",
    )
    parser.addoption(
        "--loopback-timeout",
        type=float,
        default=900.0,
        help="P5.5 loopback overall deadline in seconds",
    )
    parser.addoption(
        "--console",
        default=None,
        help="ESP32 USB console port (e.g. COM5) for the fifo_ovf= counter",
    )
    parser.addoption(
        "--loopback",
        action="store_true",
        help="run the P5.5 ESP32 loopback gate (requires the TX/RX and "
        "RTS/CTS jumpers and the JD ribbon removed)",
    )
