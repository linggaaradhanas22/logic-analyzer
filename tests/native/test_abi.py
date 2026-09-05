"""Small ctypes ABI smoke for the native shared library."""

from __future__ import annotations

import argparse
import ctypes
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    args = parser.parse_args()
    library = args.library.resolve()
    if not library.is_file():
        raise SystemExit(f"native ABI library does not exist: {library}")
    native = ctypes.CDLL(str(library))
    native.la_crc16_ccitt.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    native.la_crc16_ccitt.restype = ctypes.c_uint16
    value = b"123456789"
    buffer = ctypes.create_string_buffer(value)
    actual = native.la_crc16_ccitt(buffer, len(value))
    if actual != 0x29B1:
        raise SystemExit(f"unexpected CRC from native ABI: 0x{actual:04X}")
    print("PASS: la_native_ctypes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
