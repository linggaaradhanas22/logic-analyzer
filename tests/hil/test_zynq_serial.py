"""P4.10 Zynq UART0 serial bring-up (HIL, [HUMAN] bench gate).

Run with the P4 analyzer firmware loaded over JTAG, a USB-UART adapter wired
to the Zybo JD header with RTS/CTS, and the adapter's COM port selected:

    .venv\\Scripts\\python.exe tools\\run.py hil -- --port COM7

Without --port the module is skipped so the suite stays runnable off-bench;
nothing here may be reported as passing without the physical link.

Test 1: the internal pattern generator emits its 8-bit counter; a 4096-word
raw capture must contain consecutive counter bytes (the first sample is
wherever the free-running counter was at ARM, so the model checks
sample[i] == (sample[0] + i) & 0xFF). Repeated --repeat times.

Test 2: the patgen UART mode transmits "LA-TEST\\n" at 115200 baud 8N1; a
transition-encoded capture is expanded from {run[23:0], sample[7:0]} words
and decoded in Python. Repeated --uart-repeat times.
"""

from __future__ import annotations

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri

FS_HZ = 100_000_000
UART_BIT_CYCLES = FS_HZ // 115200  # the RTL uses the integer 868
RAW_WORDS = 4096
RAW_SAMPLES = RAW_WORDS * 4
UART_SAMPLES = 120_000
UART_FRAME = b"LA-TEST\n"


@pytest.fixture(scope="module")
def client(request):
    port = request.config.getoption("--port")
    if not port:
        pytest.skip("no --port: the Zynq HIL link requires the bench hardware")
    analyzer = AnalyzerClient(open_uri(f"serial://{port}"))
    yield analyzer
    analyzer.close()


def _counter_config() -> bytes:
    return p.pack_config(
        ctrl_flags=p.CTRL_SELFTEST_EN,
        sample_div=0,
        mode=p.MODE_RAW,
        chan_en=0xFF,
        post_samples=RAW_SAMPLES,
    )


def _uart_config() -> bytes:
    return p.pack_config(
        ctrl_flags=p.CTRL_SELFTEST_EN,
        sample_div=0,
        mode=p.MODE_TXN,
        chan_en=0x01,
        post_samples=UART_SAMPLES,
    )


def _expand_transitions(data: bytes) -> bytes:
    """Expands {run[23:0], sample[7:0]} words into the sample sequence."""
    samples = bytearray()
    for offset in range(0, len(data), 4):
        word = int.from_bytes(data[offset:offset + 4], "little")
        run = word >> 8
        sample = word & 0xFF
        samples.extend(bytes([sample]) * run)
    return bytes(samples)


def _decode_uart(samples: bytes, channel: int,
                 bit_cycles: int = UART_BIT_CYCLES) -> bytes:
    """8N1 LSB-first software UART decoder over one channel bit."""
    mask = 1 << channel
    total = len(samples)
    index = 0
    decoded = bytearray()
    while index < total - 11 * bit_cycles:
        if (samples[index] & mask) == 0 and (index == 0 or samples[index - 1] & mask):
            start = index
            value = 0
            complete = True
            for bit in range(8):
                at = start + bit_cycles // 2 + (bit + 1) * bit_cycles
                if at >= total:
                    complete = False
                    break
                if samples[at] & mask:
                    value |= 1 << bit
            stop_at = start + bit_cycles // 2 + 9 * bit_cycles
            if complete and stop_at < total and (samples[stop_at] & mask):
                decoded.append(value)
                index = start + 10 * bit_cycles
                continue
        index += 1
    return bytes(decoded)


def test_ping_and_status(client: AnalyzerClient) -> None:
    pong = client.ping()
    assert pong["pl_id"] == 0x4C410002, pong
    status = client.status()
    assert status["status_reg"] & 0x1, status


def test_counter_capture_matches_model(client: AnalyzerClient, request) -> None:
    repeat = request.config.getoption("--repeat")
    client.configure_patgen(p.PATGEN_EN_MASK |
                            (p.PATGEN_MODE_COUNTER << p.PATGEN_MODE_SHIFT))
    for run in range(repeat):
        result = client.capture(_counter_config(), timeout_s=10.0)
        assert result.encoding == p.MODE_RAW
        assert result.meta["total_words"] == RAW_WORDS, (run, result.meta)
        data = result.data
        assert len(data) == RAW_SAMPLES, (run, len(data))
        first = data[0]
        for index, value in enumerate(data):
            expected = (first + index) & 0xFF
            assert value == expected, (
                f"run {run} sample {index}: got 0x{value:02X}, "
                f"expected 0x{expected:02X}"
            )


def test_uart_patgen_transition_decodes(client: AnalyzerClient, request) -> None:
    repeat = request.config.getoption("--uart-repeat")
    client.configure_patgen(p.PATGEN_EN_MASK |
                            (p.PATGEN_MODE_UART_115200 << p.PATGEN_MODE_SHIFT))
    config = _uart_config()
    for run in range(repeat):
        result = client.capture(config, timeout_s=10.0)
        assert result.encoding == p.MODE_TXN
        samples = _expand_transitions(result.data)
        assert len(samples) >= 10 * UART_BIT_CYCLES, (run, len(samples))
        decoded = _decode_uart(samples, channel=0)
        assert UART_FRAME in decoded, f"run {run}: decoded {decoded!r}"
