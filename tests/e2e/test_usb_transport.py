"""U04 direct-USB transport coverage: commands, parser robustness, trace.

Run with the USB-profile firmware loaded and the CDC port free:

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- --target serial://COMx \
        -k test_usb_transport

Without `--target` the tests skip, so the suite stays runnable off-bench. The
raw-parser cases open the port directly because the shared `AnalyzerClient`
parser cannot be fed out-of-band bytes.
"""

from __future__ import annotations

import time

import pytest

import host.la_cli.protocol as p
from host.la_cli.client import AnalyzerClient, AnalyzerError
from host.la_cli.transport import TransportError, open_uri

PING_ATTEMPTS = 8


def _target(request) -> str:
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the U04 USB transport tests require the bench hardware")
    if not str(target).lower().startswith("serial"):
        pytest.skip("the U04 USB transport tests require the direct-USB serial target")
    return str(target)


@pytest.fixture()
def client(request):
    analyzer = AnalyzerClient(open_uri(_target(request)))
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
        pytest.fail(f"USB device did not answer PING after {PING_ATTEMPTS} attempts: {last_error}")
    yield analyzer
    analyzer.close()


def _read_frames(transport, parser, frames, timeout_s: float, stop=None) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if stop is not None and stop():
            return
        data = transport.recv(4096, 0.2)
        if data:
            parser.feed(data, lambda mt, seq, payload: frames.append((mt, seq, payload)))


def test_usb_link_cfg_rejected(client) -> None:
    """The USB profile has no UART/SPI bulk switch: every LINK_CFG must NAK."""
    for transport in (p.LINK_SPI, p.LINK_UART):
        with pytest.raises(AnalyzerError) as excinfo:
            client.configure_link(transport)
        assert excinfo.value.error_code == p.ERROR_UNSUPPORTED
    assert client.ping(timeout_s=3.0)["pl_id"] == 0x4C410002


def test_usb_trace_control(client) -> None:
    """TRACE_CTRL start plus a dump must terminate with an END marker."""
    client.trace_control(p.TRACE_START)
    lines = client.trace_dump(timeout_s=15.0)
    assert any(line.startswith("END ") for line in lines), lines[:3]


def test_usb_parser_split_frame(request) -> None:
    """A frame split across two writes must be reassembled without CRC error."""
    transport = open_uri(_target(request))
    parser = p.FrameParser()
    frames: list = []
    try:
        frame = p.encode(p.PING, 0x42)
        transport.send(frame[:3])
        time.sleep(0.05)
        transport.send(frame[3:])
        _read_frames(transport, parser, frames, 3.0, stop=lambda: bool(frames))
    finally:
        transport.close()
    assert parser.crc_errors == 0 and parser.format_errors == 0
    assert any(mt == p.PONG for mt, _seq, _payload in frames), frames


def test_usb_parser_coalesced_frames(request) -> None:
    """Two frames in one write must both be delivered."""
    transport = open_uri(_target(request))
    parser = p.FrameParser()
    frames: list = []
    try:
        transport.send(p.encode(p.PING, 0x10) + p.encode(p.PING, 0x11))
        _read_frames(transport, parser, frames, 3.0,
                     stop=lambda: len([f for f in frames if f[0] == p.PONG]) >= 2)
    finally:
        transport.close()
    pongs = [f for f in frames if f[0] == p.PONG]
    assert len(pongs) >= 2, frames
    assert parser.crc_errors == 0 and parser.format_errors == 0


def test_usb_parser_garbage_and_bad_crc_recovery(request) -> None:
    """Garbage and a corrupted frame must not break the next valid request.

    A corrupted length field can make the receiver wait for up to
    LA_PROTO_MAX_PAYLOAD payload bytes before the CRC check resynchronizes, so
    the test sends a bounded flush between the corruption and the valid PING.
    """
    transport = open_uri(_target(request))
    parser = p.FrameParser()
    frames: list = []
    try:
        transport.send(bytes([0x00, 0xFF, 0x55, 0xA5, 0x13, 0x99]))
        bad = bytearray(p.encode(p.PING, 0x20))
        bad[-1] ^= 0xFF
        transport.send(bytes(bad))
        time.sleep(0.05)
        transport.send(bytes(2048))
        time.sleep(0.05)
        transport.send(p.encode(p.PING, 0x21))
        _read_frames(transport, parser, frames, 3.0, stop=lambda: bool(frames))
    finally:
        transport.close()
    assert any(mt == p.PONG for mt, _seq, _payload in frames), frames
