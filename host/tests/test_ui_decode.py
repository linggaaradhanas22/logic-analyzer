"""W01: host-side capture decoding parity with the native model vectors.

The host decode path feeds the same transitions through the same C decoders
the Zynq runs; these tests compare the results with the independent Python
models in ``tools/models/decoders.py`` (the same vectors
``tests/native/test_ctypes_models.py`` uses). The native library must be
built first: ``python tools/run.py test-native``.
"""

from __future__ import annotations

import numpy as np
import pytest

from host.la_ui.decode import (
    NativeDecoders,
    NativeLibraryMissing,
    decode_capture,
    library_path,
    parse_decoder_spec,
)
from host.la_ui.store import Capture
from tools.models import decoders as model


@pytest.fixture(scope="module")
def decoders() -> NativeDecoders:
    if library_path() is None:
        pytest.skip("native library not built; run python tools/run.py test-native")
    return NativeDecoders()


def _capture(transitions, total: int, initial: int, sample_rate_hz: int,
             encoding: int = 0) -> Capture:
    samples = np.full(total, initial, dtype=np.uint8)
    for item in transitions:
        if item.t_sample >= total:
            break
        samples[item.t_sample:] = item.sample
    return Capture(encoding=encoding, sample_rate_hz=sample_rate_hz,
                   data=samples.tobytes(), total_words=(total + 3) // 4)


def _actual(events) -> list[tuple]:
    return [(event["kind"], event["t_sample"], event["channel"],
             event["flags"], bytes.fromhex(event["data"])) for event in events]


def _expected(events) -> list[tuple]:
    return [(event.kind, event.t_sample, event.channel, event.flags,
             event.data) for event in events]


def test_uart_capture_decodes_like_model(decoders):
    transitions, expected = model.uart_frame(0xA5, samples_per_bit=8)
    total = transitions[-1].t_sample + 64
    capture = _capture(transitions, total, initial=1,
                       sample_rate_hz=8_000_000)
    result = decoders.decode_capture(capture, ["uart:rx=0,baud=1000000"])
    assert _actual(result[0]) == _expected(expected)


def test_spi_capture_decodes_like_model(decoders):
    transitions, expected = model.spi_transfer(
        [0x96, 0x3C], cpol=1, cpha=1, word_size=8, lsb_first=True)
    total = transitions[-1].t_sample + 64
    capture = _capture(transitions, total, initial=transitions[0].sample,
                       sample_rate_hz=10_000_000)
    result = decoders.decode_capture(
        capture, ["spi:sck=0,mosi=1,cs=2,cpol=1,cpha=1,word=8,lsb=1"])
    assert _actual(result[0]) == _expected(expected)


def test_i2c_capture_decodes_like_model(decoders):
    transitions, expected = model.i2c_transaction(
        0x2A, [0x55, 0xA5], read=False, ack=[True, True, False])
    total = transitions[-1].t_sample + 64
    capture = _capture(transitions, total, initial=transitions[0].sample,
                       sample_rate_hz=10_000_000)
    result = decoders.decode_capture(capture, ["i2c:scl=0,sda=1"])
    assert _actual(result[0]) == _expected(expected)


def test_decode_multiple_slots(decoders):
    uart_transitions, uart_expected = model.uart_frame(0x4C, samples_per_bit=8)
    total = uart_transitions[-1].t_sample + 64
    capture = _capture(uart_transitions, total, initial=1,
                       sample_rate_hz=8_000_000)
    result = decoders.decode_capture(
        capture, ["uart:rx=0,baud=1000000", None, "i2c:scl=0,sda=1"])
    assert set(result) == {0, 2}
    assert _actual(result[0]) == _expected(uart_expected)
    assert result[2] == []


def test_decode_capture_wrapper(decoders):
    transitions, _expected_events = model.uart_frame(0x55, samples_per_bit=8)
    total = transitions[-1].t_sample + 64
    capture = _capture(transitions, total, initial=1,
                       sample_rate_hz=8_000_000)
    result = decode_capture(capture, ["uart:rx=0,baud=1000000"],
                            decoders=decoders)
    assert len(result[0]) == 1


def test_parse_decoder_spec_errors():
    assert parse_decoder_spec("uart:rx=1,baud=9600")["rx"] == 1
    assert parse_decoder_spec("spi:sck=0,mosi=1")["mosi"] == 1
    assert parse_decoder_spec("i2c:scl=3,sda=4")["sda"] == 4
    for bad in ("", "uart", "uart:rx=0", "uart:rx=0,baud=0",
                "spi:sck=0", "spi:sck=0,mosi=-1", "i2c:scl=1,sda=1",
                "can:rx=0", "uart:rx=x,baud=9600"):
        with pytest.raises(ValueError):
            parse_decoder_spec(bad)


def test_missing_library_reports_clearly(monkeypatch, tmp_path):
    monkeypatch.setenv("LA_NATIVE_LIB", str(tmp_path / "absent.dll"))
    assert library_path() is None
    with pytest.raises(NativeLibraryMissing):
        NativeDecoders()
