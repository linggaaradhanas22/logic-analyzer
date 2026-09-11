"""P7.6 end-to-end live decode gates (E2E, [HUMAN] bench gate).

Run with the P7 firmware loaded, the ESP32 gateway up and the JE-1 -> JC-1
jumper installed (patgen UART on CH0):

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- --target tcp://la-esp32.local:5555 --live

`test_live_uart_decode_events` verifies the decoded "LA-TEST\\n" frame over the
live path. `test_live_uart_continuous_zero_drop` streams for
`--live-seconds` (default 10; the plan gate uses 60) and requires zero
EVT_OVERRUN frames, a clean repeating byte stream and `dropped_segments == 0`
in the final STATUS. `test_live_spi_decode_events` and
`test_live_i2c_decode_events` run the patgen SPI and I2C sources at 10 kHz
through the live decoders (one slot at a time) and check the decoded
MOSI/address/ACK/control pattern plus link headroom.
`test_live_raw_stream_decoders_off` runs the decoder-free raw stream (P7.2
decoders-off smoke) and checks a clean return to IDLE.
`test_live_link_overrun_reported_on_stall` is the P8.4 fault injection: it
stalls the host read for 3 s and requires the gateway to report the resulting
best-effort frame drops as EVT_OVERRUN(stage=2 link).
`test_live_spi_overload_reports_loss` drives the 1 MHz SPI source (far above
the live link capacity) and requires reported loss, no crash and a clean
state reset.
`test_live_garbage_flood_resync` sends garbage plus a bad-CRC frame and
requires the next command to be answered (parser resync).
`test_live_tcp_kill_reconnect` RSTs the client mid-live and requires a fresh
connection to be served (stale-client regression guard).

Remaining P7.6 gates that need a controllable source or a longer run are
tracked in docs/status.md (isolated-final-byte 25 ms p99, SPI/I2C overload,
simultaneous decoders).
"""

from __future__ import annotations

import threading
import time

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri

FCLK_HZ = 100_000_000
UART_FRAME = b"LA-TEST\n"
EVENT_BYTE = 1
SPI_WORDS = (0x4C, 0x41)
I2C_ADDRESS = 0x50
I2C_BYTES = (0x4C, 0x41)
# 2 Mbps 8N1 payload ceiling; the plan requires >=20% headroom.
LINK_PAYLOAD_BYTES_PER_S = 200_000


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


def _configure_patgen(client: AnalyzerClient, mode: int, div: int) -> None:
    """Selects a patgen mode and DIV. la_patgen latches mode and DIV on the
    EN rising edge, so a previous test that left the generator enabled in
    another mode is not re-configured by a plain enable write; disable first
    (the burst matrix does the same)."""
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(
        p.PATGEN_EN_MASK | (mode << p.PATGEN_MODE_SHIFT) | (div << 16)
    )


def _configure_patgen_uart(client: AnalyzerClient, baud: int) -> None:
    """UART patgen with the DIV that scales the nominal 115200 down to `baud`."""
    div = max(0, 115200 // baud - 1) if baud <= 115200 else 0
    _configure_patgen(client, p.PATGEN_MODE_UART_115200, div)


def _assert_headroom(events: list[dict], elapsed_s: float) -> None:
    """Counts a conservative encoded-wire estimate (record header plus one
    frame header/CRC per event; real batching is smaller) and requires at
    least 20% headroom against the 2 Mbps 8N1 payload ceiling."""
    decoded = [event for event in events if not event.get("overrun")]
    wire_bytes = sum(11 + len(event["data"]) + 7 for event in decoded)
    rate = wire_bytes / max(elapsed_s, 1e-6)
    assert rate <= 0.8 * LINK_PAYLOAD_BYTES_PER_S, (rate, len(decoded), elapsed_s)


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


def test_live_spi_decode_events(client: AnalyzerClient) -> None:
    """P7.6 SPI live gate at 10 kHz: the patgen SPI source (nominal 1 MHz /
    (99+1)) drives SCK=CH0, MOSI=CH1 and CS_N=CH2 over the JE->JC loopback.
    The patgen drives MOSI only; MISO needs a second data source (P9 link)."""
    _configure_patgen(client, 4, 99)
    started = time.monotonic()
    events = client.live(
        p.pack_decoder_cfg_spi(0, 0, mosi_channel=1, cs_channel=2, cs_active=0),
        config=_stream_config(1), count=None, timeout_s=2.0)
    elapsed = time.monotonic() - started
    overruns = [event for event in events if event.get("overrun")]
    assert not overruns, overruns
    kinds = [event["kind"] for event in events]
    assert 10 in kinds and 11 in kinds, f"missing CS events in {kinds}"
    # The stream starts mid-frame: skip the first partial frame (its
    # CS_ASSERT..CS_RELEASE group) and align on the next CS_ASSERT.
    start = kinds.index(11) + 1
    assert kinds[start] == 10, f"no CS assert after the partial frame: {kinds[start:start + 4]}"
    aligned = events[start:]
    assert 2 not in [event["kind"] for event in aligned], "SPI frame error after alignment"
    words = [event["data"][0] for event in aligned if event["kind"] == 1]
    assert len(words) >= 4, words
    expected = (list(SPI_WORDS) * (len(words) // 2 + 1))[: len(words)]
    assert words == expected, (words, expected)
    _assert_headroom(events, elapsed)
    status = client.status()
    assert status["state"] == 0, "firmware did not return to IDLE after LIVE_STOP"
    assert status["decoders"][1]["dropped_segments"] == 0


def test_live_i2c_decode_events(client: AnalyzerClient) -> None:
    """P7.6 I2C live gate at 10 kHz: the patgen I2C source (nominal 400 kHz /
    (39+1)) drives SCL=CH3 and SDA=CH4 over the JE->JC loopback; the patgen
    drives the ACK bits low, so the decoded pattern is START, address 0x50,
    ACK, 0x4C, ACK, 0x41, ACK, STOP repeating."""
    _configure_patgen(client, 5, 39)
    started = time.monotonic()
    events = client.live(p.pack_decoder_cfg_i2c(0, 3, 4),
                         config=_stream_config(1), count=None, timeout_s=2.0)
    elapsed = time.monotonic() - started
    overruns = [event for event in events if event.get("overrun")]
    assert not overruns, overruns
    kinds = [event["kind"] for event in events]
    assert 9 not in kinds, "I2C NACK"
    assert 4 in kinds, f"no START in {kinds}"
    start = kinds.index(4)  # first START aligns the stream
    aligned = kinds[start:]
    pattern = (4, 7, 8, 1, 8, 1, 8, 5)  # START, ADDR, ACK, BYTE, ACK, BYTE, ACK, STOP
    complete = len(aligned) // len(pattern)
    assert complete >= 2, (complete, aligned)
    assert tuple(aligned[: complete * len(pattern)]) == pattern * complete, aligned
    for event in events[start:start + complete * len(pattern)]:
        if event["kind"] == 7:
            assert event["data"] == bytes([I2C_ADDRESS]), event
        elif event["kind"] == 1:
            assert event["data"][0] in I2C_BYTES, event
    _assert_headroom(events, elapsed)
    status = client.status()
    assert status["state"] == 0, "firmware did not return to IDLE after LIVE_STOP"
    assert status["decoders"][2]["dropped_segments"] == 0


def test_live_link_overrun_reported_on_stall(client: AnalyzerClient) -> None:
    """P8.4 fault injection: stop reading the TCP socket for 3 s while the
    live UART stream runs. The ESP32 relay's to_net ring fills and drops
    best-effort EVT_DECODED frames; the link report must surface them as
    EVT_OVERRUN(stage=2) once the host resumes reading."""
    _configure_patgen_uart(client, 115200)
    events = client.live(_uart_decoder(115200), config=_stream_config(1),
                         count=None, timeout_s=15.0, read_stall_s=3.0)
    overruns = [event for event in events if event.get("overrun")]
    assert overruns, "no EVT_OVERRUN after the host read stall"
    assert overruns[0]["stage"] == 2, overruns
    assert overruns[0]["dropped"] >= 1, overruns
    status = client.status()
    assert status["state"] == 0, "firmware did not return to IDLE after LIVE_STOP"


def test_live_raw_stream_decoders_off(client: AnalyzerClient) -> None:
    decoder_off = bytes([0, 0]) + b"\x00" * 18
    events = client.live(decoder_off, config=_stream_config(0), count=None,
                         timeout_s=2.0)
    assert all(event.get("overrun") for event in events) or not events
    status = client.status()
    assert status["state"] == 0, "decoder-free live stream did not stop cleanly"


def test_live_spi_overload_reports_loss(client: AnalyzerClient) -> None:
    """P8.4/P7.6 overload: the patgen SPI at 1 MHz (DIV=0) offers ~230 k
    events/s, far beyond the live link capacity. The loss must be reported
    with a nonzero counter, no crash, a clean return to IDLE and a working
    command path afterwards (state reset)."""
    _configure_patgen(client, 4, 0)
    events = client.live(
        p.pack_decoder_cfg_spi(0, 0, mosi_channel=1, cs_channel=2, cs_active=0),
        config=_stream_config(1), count=None, timeout_s=5.0)
    overruns = [event for event in events if event.get("overrun")]
    assert overruns, "1 MHz SPI overload produced no loss report"
    assert overruns[0]["stage"] in (1, 2), overruns
    assert overruns[0]["dropped"] >= 1, overruns
    status = client.status()
    assert status["state"] == 0, "overload did not return to IDLE"
    # Recovery: the normal 10 kHz SPI source decodes cleanly again.
    _configure_patgen(client, 4, 99)
    events = client.live(
        p.pack_decoder_cfg_spi(0, 0, mosi_channel=1, cs_channel=2, cs_active=0),
        config=_stream_config(1), count=32, timeout_s=5.0)
    assert not [event for event in events if event.get("overrun")]


def test_live_garbage_flood_resync(client: AnalyzerClient) -> None:
    """P8.4: flood the gateway with garbage (including SOF bytes), complete
    any pending partial frame, force a CRC resync, then require the next
    valid command to be answered. The host-stream parser must resync and no
    task may die."""
    garbage = bytes(range(256)) * 8
    flush = b"\x00" * (p.MAX_PAYLOAD + 64)
    bad_crc = bytes([p.SOF, p.PING, 0, 0, 0, 0xDE, 0xAD])
    client.raw_send(garbage + flush + bad_crc)
    pong = client.ping(timeout_s=10.0)
    assert pong["fw_version"] != 0, pong
    status = client.status()
    assert status["state"] == 0


def test_live_tcp_kill_reconnect(client: AnalyzerClient, request) -> None:
    """P8.4 TCP kill: abort the client connection (RST) during a live flood
    and require the gateway to accept and serve a fresh connection without a
    reset. This is the stale-client failure mode observed after heap
    exhaustion; it must not recur with a healthy heap. Runs last because it
    destroys the module-scoped client connection."""
    target = request.config.getoption("--target")
    _configure_patgen_uart(client, 115200)
    errors: list[Exception] = []

    def stream() -> None:
        try:
            client.live(_uart_decoder(115200), config=_stream_config(1),
                        timeout_s=10.0)
        except Exception as exc:  # the RST is expected to break the pump
            errors.append(exc)

    thread = threading.Thread(target=stream, daemon=True)
    thread.start()
    time.sleep(2.0)
    client.kill()
    thread.join(timeout=5.0)
    assert not thread.is_alive(), "live pump did not stop after the RST"
    fresh = AnalyzerClient(open_uri(target))
    try:
        pong = fresh.ping(timeout_s=10.0)
        assert pong["fw_version"] != 0, pong
    finally:
        fresh.close()
