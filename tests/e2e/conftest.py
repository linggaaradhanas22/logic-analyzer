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
        "--zynq-console",
        default=None,
        help="Zynq FTDI console port (e.g. COM7) for the P8.5 decoder "
        "throughput counters (`LA status: dec words ...`)",
    )
    parser.addoption(
        "--p85-seconds",
        type=float,
        default=20.0,
        help="P8.5 decoder-throughput measurement window in seconds "
        "(default 20; the status line period is 5 s)",
    )
    parser.addoption(
        "--loopback",
        action="store_true",
        help="run the P5.5 ESP32 loopback gate (requires the TX/RX and "
        "RTS/CTS jumpers and the JD ribbon removed)",
    )
    parser.addoption(
        "--external-loopback",
        action="store_true",
        help="P6.3: run the burst matrix over the JE->JC jumper loopback "
        "(SELFTEST_EN=0; the eight JE/JC jumpers must be installed)",
    )
    parser.addoption(
        "--channel-mask",
        type=lambda value: int(value, 0),
        default=0xFF,
        help="P6.3 enabled-channel mask for the external loopback "
        "(0x3F when only JE/JC channels 0-5 are jumpered; default 0xFF)",
    )
    parser.addoption(
        "--soak-count",
        type=int,
        default=0,
        help="P8.4 burst soak: number of back-to-back captures (0 = skipped; "
        "the plan gate uses 500)",
    )
    parser.addoption(
        "--live",
        action="store_true",
        help="P7.6: run the live decode gates (requires the P7 firmware, the "
        "JE-1->JC-1 jumper and a patgen UART 115200 source on CH0)",
    )
    parser.addoption(
        "--live-seconds",
        type=float,
        default=10.0,
        help="P7.6 continuous UART live stream duration in seconds "
        "(default 10; the plan zero-drop gate uses 60)",
    )
    parser.addoption(
        "--live-baud",
        type=int,
        default=115200,
        help="P7.6 live UART baud; the patgen 115200 source is scaled down by "
        "DIV so slower rates exercise the same pipeline within link headroom "
        "(default 115200)",
    )
    parser.addoption(
        "--p91-clock",
        type=int,
        default=18,
        help="P9.1 drain gate: SPI master clock in MHz selected through "
        "LINK_CFG (default 18, the measured clean ceiling on the current "
        "wiring; 10 is the safe characterization rate)",
    )
    parser.addoption(
        "--p91-drain-mb",
        type=int,
        default=64,
        help="P9.1 drain gate: total MiB of verified payload (0 disables; "
        "the plan gate uses 64)",
    )
