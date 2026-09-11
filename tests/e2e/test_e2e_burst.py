"""P6.2/P6.3 end-to-end burst matrix (E2E, [HUMAN] bench gate).

Run with the P4 firmware loaded and the analyzer link reachable:

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- --target tcp://la-esp32.local:5555

P6.2 runs the internal selftest mux (SELFTEST_EN=1). P6.3 repeats the same
matrix over the physical JE->JC jumper loopback with `--external-loopback`
(SELFTEST_EN=0; the eight JE/JC jumpers must be installed). When only
channels 0-5 are jumpered, pass `--channel-mask 0x3F` to mask the two
unconnected inputs; the protocol modes use channels 0-4 only and the step
modes are then verified over the enabled channels.

Every patgen mode (counter, walking-1, PRBS8, UART, SPI, I2C) runs at the
1M/10M/100M sample rates with three consecutive captures each. Step modes
are checked sample-by-sample against their sequence model, UART bit-exactly
against its frame waveform, and SPI/I2C by decoding the reconstructed
stream with the tested native decoders (build/native/la_native.dll). The
patgen DIV is scaled per rate so each protocol waveform is sampled with at
least five samples per half period and an integer number of samples per
UART bit. Every capture must be parser/CRC/format clean and must not set
the sticky FIFO-overflow flag.
"""

from __future__ import annotations

import ctypes
import time
from pathlib import Path

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri

PROJECT_ROOT = Path(__file__).resolve().parents[2]
NATIVE_LIBRARY = PROJECT_ROOT / "build" / "native" / "la_native.dll"

FCLK_HZ = 100_000_000
RATES = ("1M", "10M", "100M")
RATE_SAMPLE_DIV = {"1M": 99, "10M": 9, "100M": 0}

STEP_DIV = 99  # 100 FCLK cycles per patgen step: 1/10/100 samples per step

# Patgen DIV per protocol mode and rate. The nominal bit periods at DIV=0
# are UART 868 FCLK cycles, SPI 50 and I2C 125; scaling by DIV+1 keeps the
# waveform well sampled at every rate.
UART_DIV = {"1M": 24, "10M": 4, "100M": 0}  # 217/434/868 samples per bit
SPI_DIV = {"1M": 9, "10M": 0, "100M": 0}    # 5/5/50 samples per half period
I2C_DIV = {"1M": 3, "10M": 1, "100M": 0}    # 5/25/125 samples per half period

UART_BIT_CYCLES = FCLK_HZ // 115200          # 868
UART_FRAME = b"LA-TEST\n"
SPI_WORDS = (0x4C, 0x41)
I2C_ADDRESS = 0x50
I2C_BYTES = (0x4C, 0x41)

MODE_CODE = {"counter": 0, "walking": 1, "prbs": 2, "uart": 3, "spi": 4, "i2c": 5}
PROTOCOL_DIV = {"uart": UART_DIV, "spi": SPI_DIV, "i2c": I2C_DIV}

# post_samples per mode and rate: at least two full pattern periods (and
# two UART frames) so each verification sees complete content.
POST_SAMPLES = {
    "counter": {"1M": 1024, "10M": 8192, "100M": 65536},
    "walking": {"1M": 1024, "10M": 8192, "100M": 65536},
    "prbs": {"1M": 1024, "10M": 8192, "100M": 65536},
    "uart": {"1M": 40000, "10M": 80000, "100M": 160000},
    "spi": {"1M": 4096, "10M": 8192, "100M": 16384},
    "i2c": {"1M": 4096, "10M": 8192, "100M": 32768},
}


@pytest.fixture(scope="module")
def client(request):
    target = request.config.getoption("--target")
    if not target:
        pytest.skip("no --target: the E2E burst tests require the bench hardware")
    analyzer = AnalyzerClient(open_uri(target))
    yield analyzer
    analyzer.close()


def _patgen_ctrl(mode: str, rate: str) -> int:
    if mode in PROTOCOL_DIV:
        div = PROTOCOL_DIV[mode][rate]
    else:
        div = STEP_DIV
    return p.PATGEN_EN_MASK | (MODE_CODE[mode] << p.PATGEN_MODE_SHIFT) | (div << 16)


def _trigger_flags(mode: str) -> dict:
    """Align the capture start: UART start bit, SPI CS assert, I2C START."""
    if mode == "uart":
        return {"trig_fall": 0x01}
    if mode == "spi":
        return {"trig_fall": 0x04}
    if mode == "i2c":
        return {"trig_mask": 0x08, "trig_value": 0x08, "trig_fall": 0x10}
    return {}


def _reconstruct(result) -> bytes:
    if result.encoding == p.MODE_RAW:
        return result.data
    samples = bytearray()
    for offset in range(0, len(result.data), 4):
        word = int.from_bytes(result.data[offset:offset + 4], "little")
        samples.extend(bytes([word & 0xFF]) * (word >> 8))
    return bytes(samples)


def _assert_clean(result, client: AnalyzerClient) -> None:
    assert result.parser_stats["crc_errors"] == 0
    assert result.parser_stats["format_errors"] == 0
    assert not (client.status()["status_reg"] & 0x10), "FIFO overflow set"


def _next_prbs(value: int) -> int:
    feedback = ((value >> 7) ^ (value >> 5) ^ (value >> 4) ^ (value >> 3)) & 1
    return ((value << 1) & 0xFF) | feedback


def _step_sequence(mode: str) -> list[int]:
    if mode == "counter":
        return list(range(256))
    if mode == "walking":
        return [1 << index for index in range(8)]
    values = [0x01]
    while len(values) < 255:
        values.append(_next_prbs(values[-1]))
    return values


def _assert_step_stream(samples: bytes, mode: str, mask: int, step: int) -> None:
    """Every patgen step is sampled exactly `step` times; the reconstructed
    stream must be a contiguous slice of the periodic step waveform, with
    disabled channels masked to zero (bit-exact, sampler-phase independent)."""
    values = [value & mask for value in _step_sequence(mode)]
    model = bytes(value for value in values for _ in range(step))
    full = model * (len(samples) // len(model) + 2)
    assert samples in full, (
        mode, mask, step, len(samples), samples[:64].hex(), full[:64].hex(),
    )


class _Transition(ctypes.Structure):
    _fields_ = [("t_sample", ctypes.c_uint64), ("sample", ctypes.c_uint8)]


class _Event(ctypes.Structure):
    _fields_ = [
        ("kind", ctypes.c_uint8),
        ("channel", ctypes.c_uint8),
        ("flags", ctypes.c_uint8),
        ("length", ctypes.c_uint8),
        ("t_sample", ctypes.c_uint64),
        ("data", ctypes.c_uint8 * 16),
    ]


_Callback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.POINTER(_Event))
_Storage = ctypes.c_uint64 * 64


def _load_native() -> ctypes.CDLL:
    if not NATIVE_LIBRARY.is_file():
        pytest.fail(
            f"native decoder library missing: {NATIVE_LIBRARY}; "
            "run `python tools/run.py test-native` first"
        )
    native = ctypes.CDLL(str(NATIVE_LIBRARY))
    native.la_uart_decoder_init.argtypes = [
        ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint32, ctypes.c_uint8,
        ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8,
    ]
    native.la_uart_decoder_init.restype = ctypes.c_int
    native.la_uart_decoder_feed_transition.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(_Transition), _Callback, ctypes.c_void_p,
    ]
    native.la_uart_decoder_feed_transition.restype = ctypes.c_int
    native.la_uart_decoder_advance.argtypes = [
        ctypes.c_void_p, ctypes.c_uint64, _Callback, ctypes.c_void_p,
    ]
    native.la_uart_decoder_advance.restype = ctypes.c_int
    native.la_spi_decoder_init.argtypes = [
        ctypes.c_void_p, ctypes.c_int8, ctypes.c_int8, ctypes.c_int8,
        ctypes.c_int8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8,
        ctypes.c_uint8, ctypes.c_uint8,
    ]
    native.la_spi_decoder_init.restype = ctypes.c_int
    native.la_spi_decoder_feed_transition.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(_Transition), _Callback, ctypes.c_void_p,
    ]
    native.la_spi_decoder_feed_transition.restype = ctypes.c_int
    native.la_i2c_decoder_init.argtypes = [ctypes.c_void_p, ctypes.c_int8, ctypes.c_int8]
    native.la_i2c_decoder_init.restype = ctypes.c_int
    native.la_i2c_decoder_feed_transition.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(_Transition), _Callback, ctypes.c_void_p,
    ]
    native.la_i2c_decoder_feed_transition.restype = ctypes.c_int
    return native


def _decode(native: ctypes.CDLL, decoder: str, samples: bytes, idle_sample: int,
            init_args: tuple) -> list[tuple]:
    """Feed the reconstructed stream to a native decoder. One idle sample is
    prepended because the trigger starts the capture exactly at the first
    edge; the line is known idle immediately before that edge."""
    stream = bytes([idle_sample]) + samples
    transitions = [(0, stream[0])]
    previous = stream[0]
    for t_sample in range(1, len(stream)):
        if stream[t_sample] != previous:
            transitions.append((t_sample, stream[t_sample]))
            previous = stream[t_sample]

    storage = _Storage()
    init = getattr(native, f"la_{decoder}_decoder_init")
    feed = getattr(native, f"la_{decoder}_decoder_feed_transition")
    assert init(ctypes.byref(storage), *init_args) == 0, decoder
    events: list[tuple] = []

    def callback(_context, pointer) -> None:
        event = pointer.contents
        events.append((
            int(event.kind),
            int(event.channel),
            int(event.flags),
            bytes(event.data[: event.length]),
        ))

    callback_ref = _Callback(callback)
    for t_sample, sample in transitions:
        transition = _Transition(t_sample, sample)
        assert feed(ctypes.byref(storage), ctypes.byref(transition), callback_ref, None) >= 0
    if decoder == "uart":
        advance = native.la_uart_decoder_advance
        assert advance(ctypes.byref(storage), len(stream) - 1, callback_ref, None) >= 0
    return events


def _uart_waveform(samples_per_bit: int) -> bytes:
    """One periodic "LA-TEST\\n" frame: per byte start(0), 8 LSB-first data
    bits and stop(1), then the inter-frame gap. The RTL gap loop runs 10
    iterations plus the reset boundary, so 11 idle bit periods follow the
    last stop bit (91 bits per frame)."""
    bits: list[int] = []
    for byte in UART_FRAME:
        bits.append(0)
        bits.extend((byte >> bit) & 1 for bit in range(8))
        bits.append(1)
    bits.extend([1] * 11)
    return bytes(bit for bit in bits for _ in range(samples_per_bit))


def _assert_uart(samples: bytes, rate: str) -> None:
    """The reconstructed stream must be a contiguous slice of the periodic
    UART waveform: bit-exact, independent of the sampler phase."""
    samples_per_bit = (UART_BIT_CYCLES * (UART_DIV[rate] + 1)) // (RATE_SAMPLE_DIV[rate] + 1)
    period = _uart_waveform(samples_per_bit)
    model = period * (len(samples) // len(period) + 2)
    assert samples in model, (
        rate, samples_per_bit, len(samples), len(period),
        samples[:64].hex(), model[:64].hex(),
    )


def _assert_spi(native: ctypes.CDLL, samples: bytes, rate: str) -> None:
    events = _decode(native, "spi", samples, 0x04, (0, 1, -1, 2, 0, 0, 8, 0, 0))
    kinds = [kind for kind, _c, _f, _d in events]
    assert 2 not in kinds, (rate, "frame error", events)
    assert 10 in kinds, (rate, "no CS assert", kinds)
    words = [data[0] for kind, _c, _f, data in events if kind == 1]
    assert len(words) >= 4, (rate, words)
    expected = (list(SPI_WORDS) * (len(words) // 2 + 1))[: len(words)]
    assert words == expected, (rate, words)


def _assert_i2c(native: ctypes.CDLL, samples: bytes, rate: str) -> None:
    events = _decode(native, "i2c", samples, 0x18, (3, 4))
    kinds = [kind for kind, _c, _f, _d in events]
    assert 9 not in kinds, (rate, "NACK", events)
    pattern = (4, 7, 8, 1, 8, 1, 8, 5)  # START, ADDR, ACK, BYTE, ACK, BYTE, ACK, STOP
    complete = len(events) // len(pattern)
    assert complete >= 2, (rate, "frames", kinds)
    assert tuple(kinds[: complete * len(pattern)]) == pattern * complete, (rate, kinds)
    for kind, _channel, flags, data in events[: complete * len(pattern)]:
        if kind == 7:
            assert data == bytes([I2C_ADDRESS]) and flags == 0, (rate, data, flags)
        elif kind == 1:
            assert data[0] in I2C_BYTES, (rate, data)


@pytest.mark.parametrize("mode", tuple(MODE_CODE))
@pytest.mark.parametrize("rate", RATES)
def test_matrix(client: AnalyzerClient, request, mode: str, rate: str) -> None:
    external = request.config.getoption("--external-loopback")
    repeat = request.config.getoption("--burst-repeat")
    channel_mask = request.config.getoption("--channel-mask")
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(_patgen_ctrl(mode, rate))
    time.sleep(0.01)
    config = p.pack_config(
        ctrl_flags=0 if external else p.CTRL_SELFTEST_EN,
        sample_div=RATE_SAMPLE_DIV[rate],
        mode=p.MODE_TXN,
        chan_en=channel_mask,
        post_samples=POST_SAMPLES[mode][rate],
        timeout=0,
        **_trigger_flags(mode),
    )
    step = (STEP_DIV + 1) // (RATE_SAMPLE_DIV[rate] + 1)
    native = _load_native() if mode in ("spi", "i2c") else None
    label = f"{'external' if external else 'selftest'} {mode} @{rate}"
    for run in range(repeat):
        result = client.capture(config, timeout_s=60)
        samples = _reconstruct(result)
        assert len(samples) == POST_SAMPLES[mode][rate], (
            label, run, len(samples), POST_SAMPLES[mode][rate],
        )
        _assert_clean(result, client)
        if mode in ("counter", "walking", "prbs"):
            _assert_step_stream(samples, mode, channel_mask, step)
        elif mode == "uart":
            _assert_uart(samples, rate)
        elif mode == "spi":
            _assert_spi(native, samples, rate)
        else:
            _assert_i2c(native, samples, rate)
