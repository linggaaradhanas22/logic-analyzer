"""HIL options: every test here needs an explicit bench serial port."""

from __future__ import annotations


def pytest_addoption(parser) -> None:
    parser.addoption(
        "--port",
        default=None,
        help="Zynq analyzer serial port for the HIL link, e.g. COM7 "
        "(without it the HIL tests are skipped)",
    )
    parser.addoption(
        "--repeat",
        type=int,
        default=100,
        help="counter capture repetitions (P4.10 cache/alignment shakeout)",
    )
    parser.addoption(
        "--uart-repeat",
        type=int,
        default=100,
        help="UART patgen transition capture repetitions",
    )
