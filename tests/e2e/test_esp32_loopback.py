"""P5.5 ESP32 UART loopback gate (E2E, [HUMAN] bench gate).

Wire TX<->RX and RTS<->CTS jumpers on the gateway (GPIO17<->16 and
GPIO22<->21), disconnect the Zynq UART ribbon so the link loops back on
itself, then run:

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- --target tcp://HOST:5555 \
        --loopback --console COM5 --loopback-bytes 10000000

The test is skipped unless `--loopback` is given, so the default E2E run stays
safe with the Zynq connected. It streams framed traffic through the relay and
back, verifies every returned frame (type, sequence, payload and parser CRC),
reports throughput and reads the ESP32 console `fifo_ovf=` counter (which must
stay zero). Run it once at 921600 and once after rebuilding the gateway with
`CONFIG_LA_LINK_BAUD=2000000` in the ignored build/esp32/sdkconfig.
"""

from __future__ import annotations

import re
import select
import socket
import struct
import threading
import time

import pytest

from host.la_cli import protocol as p

LOOPBACK_TYPE = 0x55  # unknown to the relay: forwarded verbatim
PAYLOAD_SIZE = 1000
WINDOW_FRAMES = 256


@pytest.fixture(scope="module")
def tcp_target(request):
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the ESP32 loopback gate requires the bench hardware")
    if not request.config.getoption("--loopback"):
        pytest.skip("P5.5 loopback needs --loopback and the TX/RX + RTS/CTS jumpers")
    match = re.fullmatch(r"tcp://([^:]+):(\d+)", target)
    if not match:
        pytest.skip("the ESP32 loopback gate needs a tcp:// target")
    return match.group(1), int(match.group(2))


class _ConsoleWatch:
    """Reads the ESP32 USB console and tracks the maximum fifo_ovf counter."""

    def __init__(self, port: str) -> None:
        self.port = port
        self.max_fifo_ovf = 0
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def _run(self) -> None:
        import serial  # pyserial; imported lazily so non-bench runs skip it

        try:
            # pyserial asserts DTR/RTS on open by default, which pulses the
            # ESP32 auto-reset circuit. Create the port unopened, clear the
            # initial line states, then open without resetting the board.
            console = serial.Serial(port=None, baudrate=115200, timeout=0.2)
            console._dtr_state = False  # noqa: SLF001 - pyserial line states
            console._rts_state = False  # noqa: SLF001
            console.port = self.port
            console.open()
            try:
                time.sleep(0.5)
                console.reset_input_buffer()
                buffer = b""
                while not self._stop.is_set():
                    data = console.read(256)
                    if not data:
                        continue
                    buffer += data
                    while b"\n" in buffer:
                        line, buffer = buffer.split(b"\n", 1)
                        match = re.search(rb"fifo_ovf=(\d+)", line)
                        if match:
                            self.max_fifo_ovf = max(self.max_fifo_ovf,
                                                    int(match.group(1)))
            finally:
                console.close()
        except Exception as exc:  # pragma: no cover - bench-only path
            print(f"console watch failed on {self.port}: {exc}")

    def start(self) -> None:
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        self._thread.join(timeout=2.0)


def _build_frame(index: int) -> bytes:
    payload = struct.pack("<II", index & 0xFFFFFFFF, index) + bytes(PAYLOAD_SIZE - 8)
    return p.encode(LOOPBACK_TYPE, index & 0xFF, payload)


def test_esp32_loopback(tcp_target, request) -> None:
    host, port = tcp_target
    total_bytes = request.config.getoption("--loopback-bytes")
    deadline_s = request.config.getoption("--loopback-timeout")
    console_port = request.config.getoption("--console")
    total_frames = max(1, total_bytes // PAYLOAD_SIZE)

    watch = None
    if console_port:
        watch = _ConsoleWatch(console_port)
        watch.start()
    # Opening the console can pulse the ESP32 reset lines; retry the TCP
    # connect until the gateway finishes booting and association.
    sock = None
    connect_deadline = time.monotonic() + 30.0
    while sock is None:
        try:
            sock = socket.create_connection((host, port), timeout=5.0)
        except OSError as exc:
            if time.monotonic() > connect_deadline:
                if watch is not None:
                    watch.stop()
                pytest.fail(
                    f"cannot connect to the ESP32 gateway at {host}:{port}: {exc}"
                )
            time.sleep(0.5)

    parser = p.FrameParser()
    received_frames = 0
    received_bytes = 0
    mismatches = 0
    sent_frames = 0
    sent_bytes = 0
    pending = b""
    first_send = None
    last_receive = None

    def on_frame(message_type, sequence, payload) -> None:
        nonlocal received_frames, received_bytes, mismatches, last_receive
        received_bytes += len(payload) + 7
        last_receive = time.monotonic()
        if message_type != LOOPBACK_TYPE or len(payload) != PAYLOAD_SIZE:
            mismatches += 1
            return
        index, echo_index = struct.unpack_from("<II", payload, 0)
        if index != echo_index or (sequence & 0xFF) != (index & 0xFF):
            mismatches += 1
            return
        received_frames += 1

    deadline = time.monotonic() + deadline_s
    try:
        sock.setblocking(False)
        while received_frames < total_frames:
            if time.monotonic() > deadline:
                pytest.fail(
                    f"loopback deadline: {received_frames}/{total_frames} frames, "
                    f"{mismatches} mismatches"
                )
            outstanding = sent_frames - received_frames
            want_write = bool(pending) or (sent_frames < total_frames and
                                           outstanding < WINDOW_FRAMES)
            readable, writable, _ = select.select(
                [sock], [sock] if want_write else [], [], 0.05
            )
            if readable:
                data = sock.recv(65536)
                if not data:
                    pytest.fail("gateway closed the loopback connection")
                parser.feed(data, on_frame)
            if writable:
                if not pending:
                    pending = _build_frame(sent_frames)
                    sent_frames += 1
                    sent_bytes += len(pending)
                    if first_send is None:
                        first_send = time.monotonic()
                try:
                    sent = sock.send(pending)
                except BlockingIOError:
                    sent = 0
                pending = pending[sent:]
    finally:
        sock.close()
        if watch is not None:
            watch.stop()

    elapsed = (last_receive or time.monotonic()) - (first_send or time.monotonic())
    assert mismatches == 0, f"{mismatches} corrupted or unexpected frames"
    assert parser.crc_errors == 0, f"{parser.crc_errors} parser CRC errors"
    assert parser.format_errors == 0, f"{parser.format_errors} format errors"
    print(
        f"P5.5 loopback: {received_frames} frames, {received_bytes} bytes echoed "
        f"in {elapsed:.1f} s -> {received_bytes / max(elapsed, 1e-6) / 1000.0:.1f} "
        f"kB/s echo rate (UART carries about twice that)"
    )
    if watch is not None:
        assert watch.max_fifo_ovf == 0, (
            f"UART_FIFO_OVF counter reached {watch.max_fifo_ovf} on {watch.port}"
        )
        print(f"P5.5 loopback: max fifo_ovf={watch.max_fifo_ovf} on {watch.port}")
