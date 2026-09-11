"""P7.6 end-to-end live decode gates (E2E, [HUMAN] bench gate).

Run with the P7 firmware loaded, the ESP32 gateway up and the JE-1 -> JC-1
jumper installed (patgen UART on CH0):

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- --target tcp://la-esp32.local:5555 --live

`test_live_uart_decode_events` verifies the decoded "LA-TEST\\n" frame over the
live path. `test_live_uart_continuous_zero_drop` streams for
`--live-seconds` (default 10; the plan gate uses 60) and requires zero
EVT_OVERRUN frames, a clean repeating byte stream and `dropped_segments == 0`
in the final STATUS. `test_live_raw_stream_decoders_off` runs the decoder-free
raw stream (P7.2 decoders-off smoke) and checks a clean return to IDLE.

Remaining P7.6 gates that need a controllable source or a longer run are
tracked in docs/status.md (isolated-final-byte 25 ms p99, SPI/I2C zero-drop
and overload, simultaneous decoders).
"""

from __future__ import annotations

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri

FCLK_HZ = 100_000_000
UART_FRAME = b"LA-TEST\n"
EVENT_BYTE = 1


@pytest.fixture(scope="module")
def client(request):
    if not request.config.getoption("--live"):
        pytest.skip("live gates require --live (P7 firmware and bench wiring)")
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the live gates require the bench hardware")
    analyzer = AnalyzerClient(open_uri(target))
    yield analyzer
    analyzer.close()


def _stream_config(mode: int) -> bytes:
    return p.pack_config(
        ctrl_flags=p.CTRL_STREAM,
        sample_div=0,
        mode=mode,
        chan_en=0xFF,
        post_samples=0,
        txn_max_run=0x00FFFFFF,
        live_flush_cycles=200_000,
    )


def _configure_patgen_uart(client: AnalyzerClient, baud: int) -> None:
    div = max(0, 115200 // baud - 1) if baud <= 115200 else 0
    client.configure_patgen(
        p.PATGEN_EN_MASK
        | (p.PATGEN_MODE_UART_115200 << p.PATGEN_MODE_SHIFT)
        | (div << 16)
    )


def _uart_decoder(baud: int) -> bytes:
    return p.pack_decoder_cfg_uart(0, 0, baud, FCLK_HZ)


def _aligned_frame_bytes(events: list[dict]) -> bytes:
    """Joins BYTE records and drops the initial partial frame (the live stream
    starts at an arbitrary point, so the first bytes may be garbage)."""
    data = b"".join(
        event["data"]
        for event in events
        if not event.get("overrun") and event["kind"] == EVENT_BYTE
        and event["slot"] == 0
    )
    start = data.find(b"L")
    assert start >= 0, f"no frame start in {data[:32]!r}"
    return data[start:]


def test_live_uart_decode_events(client: AnalyzerClient, request) -> None:
    baud = request.config.getoption("--live-baud")
    _configure_patgen_uart(client, baud)
    events = client.live(_uart_decoder(baud), config=_stream_config(1), count=32,
                         timeout_s=10.0)
    overruns = [event for event in events if event.get("overrun")]
    assert not overruns, overruns
    data = _aligned_frame_bytes(events)
    assert data[: len(UART_FRAME)] == UART_FRAME


def test_live_uart_continuous_zero_drop(client: AnalyzerClient, request) -> None:
    seconds = request.config.getoption("--live-seconds")
    baud = request.config.getoption("--live-baud")
    _configure_patgen_uart(client, baud)
    events = client.live(_uart_decoder(baud), config=_stream_config(1), count=None,
                         timeout_s=seconds)
    overruns = [event for event in events if event.get("overrun")]
    decoded = [event for event in events if not event.get("overrun")]
    assert not overruns, f"{len(overruns)} EVT_OVERRUN frames during the run"
    data = _aligned_frame_bytes(events)
    assert len(data) >= 8 * max(1, int(seconds)), "live stream decoded too few bytes"
    expected = (UART_FRAME * (len(data) // len(UART_FRAME) + 1))[: len(data)]
    assert data == expected, "decoded stream is not the repeating UART frame"
    status = client.status()
    assert status["state"] == 0, "firmware did not return to IDLE after LIVE_STOP"
    assert status["decoders"][0]["dropped_segments"] == 0
    assert status["decoders"][0]["events"] >= len(decoded)


def test_live_raw_stream_decoders_off(client: AnalyzerClient) -> None:
    decoder_off = bytes([0, 0]) + b"\x00" * 18
    events = client.live(decoder_off, config=_stream_config(0), count=None,
                         timeout_s=2.0)
    assert all(event.get("overrun") for event in events) or not events
    status = client.status()
    assert status["state"] == 0, "decoder-free live stream did not stop cleanly"
