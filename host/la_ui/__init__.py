"""Unified analyzer UI host package (W01).

Session-memory captures with lazily decoded samples, server-side decimation,
host-side capture decoding through the native ``la_native`` C ABI, the job
runner, the HTTP API and the browser workspace. Standard library plus the
project's pinned dependencies only; the device is never modified.
"""

from .store import WARN_SAMPLES, Capture, CaptureStore
from .render import ENVELOPE_HEADER, ENVELOPE_MAGIC, decode_window, encode_envelope
from .decode import (
    DECODE_MAX_SAMPLES,
    NativeDecoders,
    NativeLibraryMissing,
    decode_capture,
    library_path,
    parse_decoder_spec,
)

__all__ = [
    "DECODE_MAX_SAMPLES",
    "ENVELOPE_HEADER",
    "ENVELOPE_MAGIC",
    "WARN_SAMPLES",
    "Capture",
    "CaptureStore",
    "NativeDecoders",
    "NativeLibraryMissing",
    "decode_capture",
    "decode_window",
    "encode_envelope",
    "library_path",
    "parse_decoder_spec",
]
