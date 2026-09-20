"""Host-side capture decoding through the native ``la_native`` C ABI (W01).

The decoders are the same C sources the Zynq runs (``common/la_decode``),
compiled into the CMake DLL with ``WINDOWS_EXPORT_ALL_SYMBOLS``; this module
binds the exact ABI that ``tests/native/test_ctypes_models.py`` already
verifies against the independent Python models. There is no second decoder
implementation: when the DLL is missing the UI reports it and the Decode
action fails honestly (``python tools/run.py test-native`` builds it).

Decoder specs reuse the CLI grammar:

- ``uart:rx=CH,baud=BAUD``
- ``spi:sck=CH[,mosi=CH][,miso=CH][,cs=CH][,cpol=0][,cpha=0][,word=8][,lsb=0][,cs_active=0]``
- ``i2c:scl=CH,sda=CH``
"""

from __future__ import annotations

import ctypes
import os
import re
from pathlib import Path
from typing import Callable, Sequence

import numpy as np

from ..la_cli import protocol as p

PROJECT_ROOT = Path(__file__).resolve().parents[2]
DECODE_MAX_SAMPLES = 16_000_000
FEED_CHUNK = 16_384

Progress = Callable[[float, str], None]


class NativeLibraryMissing(RuntimeError):
    """The ``la_native`` DLL has not been built (or LA_NATIVE_LIB is wrong)."""


def library_path() -> Path | None:
    """Resolves the native library: LA_NATIVE_LIB wins, else build/native."""
    override = os.environ.get("LA_NATIVE_LIB")
    if override:
        path = Path(override)
        return path if path.is_file() else None
    for name in ("la_native.dll", "la_native.so", "libla_native.so"):
        path = PROJECT_ROOT / "build" / "native" / name
        if path.is_file():
            return path
    return None


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


def parse_decoder_spec(spec: str) -> dict:
    """Parses one decoder spec string into normalized parameters.

    Raises ValueError with a human-readable reason; the grammar is the CLI's
    ``--decoder`` grammar (docs/using-the-analyzer.md section 6).
    """
    name, sep, params_text = spec.strip().partition(":")
    if not sep:
        raise ValueError("decoder must be uart:..., spi:... or i2c:...")
    params: dict[str, int] = {}
    for item in params_text.split(","):
        item = item.strip()
        if not item:
            continue
        key, eq, value = item.partition("=")
        if not eq or not re.fullmatch(r"-?\d+", value.strip()):
            raise ValueError(f"invalid decoder parameter {item!r}")
        params[key.strip()] = int(value)
    if name == "uart":
        if set(params) != {"rx", "baud"}:
            raise ValueError("uart needs rx and baud")
        if params["baud"] <= 0:
            raise ValueError("baud must be positive")
        if not 0 <= params["rx"] <= 7:
            raise ValueError("UART rx channel must be 0..7")
        return {"type": "uart", "rx": params["rx"], "baud": params["baud"]}
    if name == "spi":
        known = {"sck", "mosi", "miso", "cs", "cpol", "cpha", "word", "lsb",
                 "cs_active"}
        if "sck" not in params or set(params) - known:
            raise ValueError("spi needs sck and only known keys")
        if not 0 <= params["sck"] <= 7:
            raise ValueError("SPI sck channel must be 0..7")
        mosi = params.get("mosi", -1)
        miso = params.get("miso", -1)
        cs = params.get("cs", -1)
        for label, value in (("mosi", mosi), ("miso", miso), ("cs", cs)):
            if value != -1 and not 0 <= value <= 7:
                raise ValueError(f"SPI {label} channel must be 0..7 or -1")
        if mosi == -1 and miso == -1:
            raise ValueError("SPI needs at least one data channel")
        word = params.get("word", 8)
        if word not in (8, 16):
            raise ValueError("SPI word size must be 8 or 16")
        for label in ("cpol", "cpha", "lsb", "cs_active"):
            if params.get(label, 0) not in (0, 1):
                raise ValueError(f"SPI {label} must be 0 or 1")
        return {
            "type": "spi", "sck": params["sck"], "mosi": mosi, "miso": miso,
            "cs": cs, "cpol": params.get("cpol", 0),
            "cpha": params.get("cpha", 0), "word": word,
            "lsb": params.get("lsb", 0),
            "cs_active": params.get("cs_active", 0),
        }
    if name == "i2c":
        if set(params) != {"scl", "sda"}:
            raise ValueError("i2c needs scl and sda")
        if not 0 <= params["scl"] <= 7 or not 0 <= params["sda"] <= 7:
            raise ValueError("I2C channels must be 0..7")
        if params["scl"] == params["sda"]:
            raise ValueError("I2C SCL and SDA must differ")
        return {"type": "i2c", "scl": params["scl"], "sda": params["sda"]}
    raise ValueError("decoder must be uart:..., spi:... or i2c:...")


class NativeDecoders:
    """Thin ctypes binding over the three native decoders."""

    def __init__(self, path: Path | None = None) -> None:
        resolved = Path(path) if path is not None else library_path()
        if resolved is None or not resolved.is_file():
            raise NativeLibraryMissing(
                "the native decoder library is not built; run "
                "`python tools/run.py test-native` or set LA_NATIVE_LIB")
        self.path = resolved
        self._lib = ctypes.CDLL(str(resolved))
        self._configure()

    def _configure(self) -> None:
        lib = self._lib
        lib.la_uart_decoder_init.argtypes = [
            ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint32, ctypes.c_uint8,
            ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]
        lib.la_uart_decoder_init.restype = ctypes.c_int
        lib.la_uart_decoder_feed_transition.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(_Transition), _Callback,
            ctypes.c_void_p]
        lib.la_uart_decoder_feed_transition.restype = ctypes.c_int
        lib.la_uart_decoder_advance.argtypes = [
            ctypes.c_void_p, ctypes.c_uint64, _Callback, ctypes.c_void_p]
        lib.la_uart_decoder_advance.restype = ctypes.c_int

        lib.la_spi_decoder_init.argtypes = [
            ctypes.c_void_p, ctypes.c_int8, ctypes.c_int8, ctypes.c_int8,
            ctypes.c_int8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8,
            ctypes.c_uint8, ctypes.c_uint8]
        lib.la_spi_decoder_init.restype = ctypes.c_int
        lib.la_spi_decoder_feed_transition.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(_Transition), _Callback,
            ctypes.c_void_p]
        lib.la_spi_decoder_feed_transition.restype = ctypes.c_int

        lib.la_i2c_decoder_init.argtypes = [
            ctypes.c_void_p, ctypes.c_int8, ctypes.c_int8]
        lib.la_i2c_decoder_init.restype = ctypes.c_int
        lib.la_i2c_decoder_feed_transition.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(_Transition), _Callback,
            ctypes.c_void_p]
        lib.la_i2c_decoder_feed_transition.restype = ctypes.c_int

    @staticmethod
    def _transitions(samples: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        """Returns (times, values) for the sample changes plus t=0."""
        if samples.size == 0:
            return np.empty(0, dtype=np.int64), np.empty(0, dtype=np.uint8)
        changes = np.flatnonzero(samples[1:] != samples[:-1]) + 1
        times = np.empty(changes.size + 1, dtype=np.int64)
        times[0] = 0
        times[1:] = changes
        return times, samples[times]

    def decode_capture(self, capture, specs: Sequence[str | None],
                       progress: Progress | None = None) -> dict[int, list[dict]]:
        """Decodes one capture for each configured slot.

        ``specs`` is indexed by slot (0..2); ``None``/empty entries are
        skipped. Returns ``{slot: [event dict, ...]}``.
        """
        samples = capture.samples()
        if samples.size > DECODE_MAX_SAMPLES:
            raise ValueError(
                f"capture has {samples.size} samples; host decoding is "
                f"limited to {DECODE_MAX_SAMPLES} samples")
        times, values = self._transitions(samples)
        results: dict[int, list[dict]] = {}
        active = [(slot, spec) for slot, spec in enumerate(specs)
                  if spec and spec.strip()]
        for index, (slot, spec) in enumerate(active):
            params = parse_decoder_spec(spec)
            base = index / max(1, len(active))
            span = 1.0 / max(1, len(active))
            if progress is not None:
                progress(base, f"decoding slot {slot} ({params['type']})")
            results[slot] = self._decode_one(
                params, times, values, int(samples.size),
                int(capture.sample_rate_hz), progress, base, span)
        if progress is not None:
            progress(1.0, "decode complete")
        return results

    def _decode_one(self, params: dict, times: np.ndarray, values: np.ndarray,
                    total_samples: int, sample_rate_hz: int,
                    progress: Progress | None, base: float,
                    span: float) -> list[dict]:
        events: list[dict] = []

        def collect(_context, pointer) -> None:
            event = pointer.contents
            kind = int(event.kind)
            events.append({
                "kind": kind,
                "name": p.EVENT_NAMES.get(kind, f"kind{kind}"),
                "channel": int(event.channel),
                "flags": int(event.flags),
                "t_sample": int(event.t_sample),
                "data": bytes(event.data[:event.length]).hex(),
            })

        callback = _Callback(collect)
        storage = _Storage()
        kind = params["type"]
        if kind == "uart":
            samples_per_bit = max(1, sample_rate_hz // max(1, params["baud"]))
            if self._lib.la_uart_decoder_init(
                    ctypes.byref(storage), params["rx"], samples_per_bit,
                    8, 0, 1, 0) != 0:
                raise ValueError("UART decoder initialization failed")
            self._feed(self._lib.la_uart_decoder_feed_transition, storage,
                       times, values, callback, progress, base, span)
            if self._lib.la_uart_decoder_advance(
                    ctypes.byref(storage), total_samples, callback, None) < 0:
                raise ValueError("UART decoder advance failed")
        elif kind == "spi":
            if self._lib.la_spi_decoder_init(
                    ctypes.byref(storage), params["sck"], params["mosi"],
                    params["miso"], params["cs"], params["cpol"],
                    params["cpha"], params["word"], params["lsb"],
                    params["cs_active"]) != 0:
                raise ValueError("SPI decoder initialization failed")
            self._feed(self._lib.la_spi_decoder_feed_transition, storage,
                       times, values, callback, progress, base, span)
        else:
            if self._lib.la_i2c_decoder_init(
                    ctypes.byref(storage), params["scl"],
                    params["sda"]) != 0:
                raise ValueError("I2C decoder initialization failed")
            self._feed(self._lib.la_i2c_decoder_feed_transition, storage,
                       times, values, callback, progress, base, span)
        return events

    def _feed(self, feed, storage, times: np.ndarray, values: np.ndarray,
              callback: _Callback, progress: Progress | None,
              base: float, span: float) -> None:
        total = max(1, int(times.size))
        for index in range(total):
            transition = _Transition(int(times[index]), int(values[index]))
            code = feed(ctypes.byref(storage), ctypes.byref(transition),
                        callback, None)
            if code < 0:
                raise ValueError(
                    f"native decoder rejected the transition at "
                    f"sample {int(times[index])}")
            if progress is not None and index % FEED_CHUNK == 0:
                progress(base + span * (index / total), "feeding transitions")


def decode_capture(capture, specs: Sequence[str | None],
                   progress: Progress | None = None,
                   decoders: NativeDecoders | None = None) -> dict[int, list[dict]]:
    """Convenience wrapper: loads the library and decodes one capture."""
    engine = decoders if decoders is not None else NativeDecoders()
    return engine.decode_capture(capture, specs, progress=progress)
