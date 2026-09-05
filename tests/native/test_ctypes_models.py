"""Compare independent Python decoder vectors with the native C ABI."""

from __future__ import annotations

import argparse
import ctypes
from pathlib import Path
from typing import Callable

from tools.models import decoders as model


class Transition(ctypes.Structure):
    _fields_ = [("t_sample", ctypes.c_uint64), ("sample", ctypes.c_uint8)]


class Event(ctypes.Structure):
    _fields_ = [
        ("kind", ctypes.c_uint8),
        ("channel", ctypes.c_uint8),
        ("flags", ctypes.c_uint8),
        ("length", ctypes.c_uint8),
        ("t_sample", ctypes.c_uint64),
        ("data", ctypes.c_uint8 * 16),
    ]


Callback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.POINTER(Event))
DecoderStorage = ctypes.c_uint64 * 64


def _configure(native: ctypes.CDLL) -> None:
    native.la_uart_decoder_init.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint8,
        ctypes.c_uint32,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint8,
    ]
    native.la_uart_decoder_init.restype = ctypes.c_int
    native.la_uart_decoder_feed_transition.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(Transition),
        Callback,
        ctypes.c_void_p,
    ]
    native.la_uart_decoder_feed_transition.restype = ctypes.c_int
    native.la_uart_decoder_advance.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint64,
        Callback,
        ctypes.c_void_p,
    ]
    native.la_uart_decoder_advance.restype = ctypes.c_int
    native.la_uart_decoder_mark_discontinuity.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint64,
        Callback,
        ctypes.c_void_p,
    ]
    native.la_uart_decoder_mark_discontinuity.restype = ctypes.c_int

    native.la_spi_decoder_init.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int8,
        ctypes.c_int8,
        ctypes.c_int8,
        ctypes.c_int8,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint8,
    ]
    native.la_spi_decoder_init.restype = ctypes.c_int
    native.la_spi_decoder_feed_transition.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(Transition),
        Callback,
        ctypes.c_void_p,
    ]
    native.la_spi_decoder_feed_transition.restype = ctypes.c_int
    native.la_spi_decoder_mark_discontinuity.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint64,
        Callback,
        ctypes.c_void_p,
    ]
    native.la_spi_decoder_mark_discontinuity.restype = ctypes.c_int

    native.la_i2c_decoder_init.argtypes = [ctypes.c_void_p, ctypes.c_int8, ctypes.c_int8]
    native.la_i2c_decoder_init.restype = ctypes.c_int
    native.la_i2c_decoder_feed_transition.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(Transition),
        Callback,
        ctypes.c_void_p,
    ]
    native.la_i2c_decoder_feed_transition.restype = ctypes.c_int
    native.la_i2c_decoder_mark_discontinuity.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint64,
        Callback,
        ctypes.c_void_p,
    ]
    native.la_i2c_decoder_mark_discontinuity.restype = ctypes.c_int


def _events(native_feed: Callable[..., int], storage: DecoderStorage, transitions: list[model.Transition]) -> list[model.Event]:
    result: list[model.Event] = []

    def callback(_context: int, pointer: ctypes.POINTER(Event)) -> None:
        event = pointer.contents
        result.append(
            model.Event(
                int(event.kind),
                int(event.t_sample),
                int(event.channel),
                int(event.flags),
                bytes(event.data[: event.length]),
            )
        )

    callback_ref = Callback(callback)
    for item in transitions:
        transition = Transition(item.t_sample, item.sample)
        code = native_feed(ctypes.byref(storage), ctypes.byref(transition), callback_ref, None)
        if code < 0:
            raise AssertionError(f"native decoder rejected transition at {item.t_sample}")
    return result


def _assert_events(actual: list[model.Event], expected: list[model.Event], label: str) -> None:
    if actual != expected:
        raise AssertionError(f"{label} mismatch:\nactual={actual!r}\nexpected={expected!r}")


def _run(library: Path) -> None:
    native = ctypes.CDLL(str(library.resolve()))
    _configure(native)

    uart_transitions, uart_expected = model.uart_frame(
        0xA5, samples_per_bit=8, parity="E", inverted=True, start=100
    )
    uart = DecoderStorage()
    if native.la_uart_decoder_init(ctypes.byref(uart), 0, 8, 8, 1, 1, 1) != 0:
        raise AssertionError("UART model initialization failed")

    def uart_callback(_context: int, pointer: ctypes.POINTER(Event)) -> None:
        event = pointer.contents
        uart_events.append(
            model.Event(
                int(event.kind),
                int(event.t_sample),
                int(event.channel),
                int(event.flags),
                bytes(event.data[: event.length]),
            )
        )

    uart_events: list[model.Event] = []
    uart_callback_ref = Callback(uart_callback)
    for item in uart_transitions:
        transition = Transition(item.t_sample, item.sample)
        if native.la_uart_decoder_feed_transition(
            ctypes.byref(uart), ctypes.byref(transition), uart_callback_ref, None
        ) < 0:
            raise AssertionError("UART model transition failed")
    if native.la_uart_decoder_advance(
        ctypes.byref(uart), uart_transitions[-1].t_sample + 16, uart_callback_ref, None
    ) < 0:
        raise AssertionError("UART model final advance failed")
    _assert_events(uart_events, uart_expected, "UART")
    if native.la_uart_decoder_mark_discontinuity(
        ctypes.byref(uart), 1000, uart_callback_ref, None
    ) != 1 or uart_events[-1] != model.Event(model.DISCONTINUITY, 1000):
        raise AssertionError("UART discontinuity")

    spi_transitions, spi_expected = model.spi_transfer(
        [0x96, 0x3C], cpol=1, cpha=1, word_size=8, lsb_first=True
    )
    spi = DecoderStorage()
    if native.la_spi_decoder_init(ctypes.byref(spi), 0, 1, -1, 2, 1, 1, 8, 1, 0) != 0:
        raise AssertionError("SPI model initialization failed")
    spi_actual = _events(native.la_spi_decoder_feed_transition, spi, spi_transitions)
    _assert_events(spi_actual, spi_expected, "SPI")

    i2c_transitions, i2c_expected = model.i2c_transaction(
        0x2A, [0x55, 0xA5], read=False, ack=[True, True, False]
    )
    i2c = DecoderStorage()
    if native.la_i2c_decoder_init(ctypes.byref(i2c), 0, 1) != 0:
        raise AssertionError("I2C model initialization failed")
    i2c_actual = _events(native.la_i2c_decoder_feed_transition, i2c, i2c_transitions)
    _assert_events(i2c_actual, i2c_expected, "I2C")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    args = parser.parse_args()
    if not args.library.is_file():
        raise SystemExit(f"native ABI library does not exist: {args.library}")
    _run(args.library)
    print("PASS: la_native_ctypes_models")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
