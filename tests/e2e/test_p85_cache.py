"""P8.5 cached-vs-non-cached decoder throughput measurement.

Drives the 1 MHz SPI overload (the heaviest repeatable decoder workload) for
`--p85-seconds` while a Zynq console reader samples the
`LA status: dec words ... busy_us ...` counters. The counters are cumulative
and only advance while the stream is live; the status task itself starves
during the overload, so the test takes one snapshot before the stream and one
after LIVE_STOP has quiesced and divides the deltas by the host-measured
window. It prints

    P8.5 decoder throughput: ... words/s, ... permille busy

and asserts a conservative floor. The experiment compares firmware builds:
`LA_MEM_LIVE_CACHED=0` (the whole DDR window Non-cacheable) versus `=1` (the
4 MiB live pool Write-Back cacheable plus Xil_DCacheInvalidateRange before
every decoder read). The boot banner (`LA live pool: ...`) identifies the
loaded build; record it with the reload console capture.

The host drains events without retaining them (`collect_events=False`) and
does not stop on the expected stage=1/2 overrun reports
(`stop_on_overrun=False`), so the measurement window is the full deadline and
the gateway relay never backpressures the Zynq into idle.
"""

from __future__ import annotations

import re
import threading
import time

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri

SPI_PATGEN_MODE = 4
SPI_OVERLOAD_DIV = 0  # 1 MHz SCK
WORDS_PER_SECOND_FLOOR = 500_000
DEC_WORDS_RE = re.compile(
    r"LA status: dec words (\d+) (\d+) (\d+) busy_us (\d+) (\d+) (\d+)"
)


class _ZynqConsole:
    """Reads the Zynq FTDI console without pulsing DTR/RTS (the Zybo FTDI
    lines are not the ESP32 auto-reset circuit, but keeping the line states
    neutral avoids any surprise) and records every decoder counter sample."""

    def __init__(self, port: str) -> None:
        self.port = port
        self.samples: list[tuple[float, list[int], list[int]]] = []
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def start(self) -> None:
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        self._thread.join(timeout=3.0)

    def _run(self) -> None:
        import serial  # pyserial; bench-only path

        try:
            console = serial.Serial(port=None, baudrate=115200, timeout=0.2)
            console._dtr_state = False  # noqa: SLF001 - pyserial line states
            console._rts_state = False  # noqa: SLF001
            console.port = self.port
            console.open()
            try:
                time.sleep(0.2)
                console.reset_input_buffer()
                buffer = b""
                while not self._stop.is_set():
                    data = console.read(256)
                    if not data:
                        continue
                    buffer += data
                    while b"\n" in buffer:
                        raw, buffer = buffer.split(b"\n", 1)
                        line = raw.decode("utf-8", errors="replace")
                        match = DEC_WORDS_RE.search(line)
                        if match:
                            values = [int(value) for value in match.groups()]
                            self.samples.append(
                                (time.monotonic(), values[:3], values[3:])
                            )
            finally:
                console.close()
        except Exception as exc:  # pragma: no cover - bench-only path
            print(f"zynq console watch failed on {self.port}: {exc}")

    def latest(self):
        return self.samples[-1] if self.samples else None


@pytest.fixture(scope="module")
def client(request):
    if not request.config.getoption("--live"):
        pytest.skip("P8.5 requires --live (P7 firmware and bench wiring)")
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


def _configure_patgen(client: AnalyzerClient, mode: int, div: int) -> None:
    """Mode and DIV latch on the patgen EN rising edge, so disable first."""
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(
        p.PATGEN_EN_MASK | (mode << p.PATGEN_MODE_SHIFT) | (div << 16)
    )


def _spi_decoder() -> bytes:
    return p.pack_decoder_cfg_spi(0, 0, mosi_channel=1, cs_channel=2,
                                  cs_active=0)


def test_p85_decoder_throughput(client: AnalyzerClient, request) -> None:
    console_port = request.config.getoption("--zynq-console")
    if not console_port:
        pytest.skip("P8.5 needs --zynq-console COMx for the decoder counters")
    seconds = request.config.getoption("--p85-seconds")
    if seconds < 5.0:
        pytest.skip("P8.5 needs --p85-seconds >= 5 for a meaningful window")

    console = _ZynqConsole(console_port)
    console.start()
    try:
        wait_deadline = time.monotonic() + 8.0
        while console.latest() is None and time.monotonic() < wait_deadline:
            time.sleep(0.2)
        before = console.latest()
        assert before is not None, "no LA status line on the Zynq console"

        _configure_patgen(client, SPI_PATGEN_MODE, SPI_OVERLOAD_DIV)
        start = time.monotonic()
        events = client.live(_spi_decoder(), config=_stream_config(), count=None,
                             timeout_s=seconds, stop_on_overrun=False,
                             collect_events=False)
        stop = time.monotonic()
        client.configure_patgen(0)

        after = None
        after_deadline = time.monotonic() + 10.0
        while time.monotonic() < after_deadline:
            latest = console.latest()
            if latest is not None and latest[0] > stop:
                after = latest
                break
            time.sleep(0.2)
        assert after is not None, (
            "no LA status line after the stream; the status task may still be "
            "starved or the Zynq console is not readable"
        )

        overruns = [event for event in events if event.get("overrun")]
        dropped = sum(event["dropped"] for event in overruns)

        elapsed = stop - start
        words_delta = [after[1][index] - before[1][index] for index in range(3)]
        busy_delta = [after[2][index] - before[2][index] for index in range(3)]
        words_per_s = words_delta[0] / elapsed
        busy_permille = busy_delta[0] / (elapsed * 1000.0)

        status = client.status()
        print(
            "P8.5 decoder throughput: "
            f"{words_per_s:.0f} words/s, {busy_permille:.0f}/1000 busy "
            f"({busy_delta[0]} us over {elapsed:.2f} s), "
            f"words={words_delta[0]}, overruns={len(overruns)}, "
            f"dropped={dropped}, "
            f"dropped_segments={status['decoders'][0]['dropped_segments']}"
        )
        assert words_delta[0] > 0, "decoder fed no words during the window"
        assert words_per_s > WORDS_PER_SECOND_FLOOR, (
            f"decoder throughput {words_per_s:.0f} words/s below the floor")
        assert busy_permille < 1000.0, "decoder busy time exceeded the window"
    finally:
        console.stop()
