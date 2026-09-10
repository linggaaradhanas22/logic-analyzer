"""Wire-level unit tests against fixed vectors (docs/protocol.md)."""

from __future__ import annotations

import struct

import pytest

from host.la_cli import protocol as p


def test_crc_reference_vectors() -> None:
    assert p.crc16_ccitt(b"123456789") == 0x29B1
    assert p.crc32_ieee(b"123456789") == 0xCBF43926
    assert p.crc32_ieee(b"") == 0


def test_encode_layout() -> None:
    frame = p.encode(0x31, 0x17, b"\x00\xA5\x7F\xFF")
    assert len(frame) == 4 + 7
    assert frame[0] == p.SOF
    assert frame[1] == 0x31
    assert frame[2] == 0x17
    assert struct.unpack_from("<H", frame, 3)[0] == 4
    assert frame[5:9] == b"\x00\xA5\x7F\xFF"
    assert struct.unpack_from("<H", frame, 9)[0] == p.crc16_ccitt(frame[1:9])


def test_parser_round_trip() -> None:
    payload = b"0123456789"
    frame = p.encode(p.DATA_CHUNK, 0x42, payload)
    seen = []

    parser = p.FrameParser()
    parser.feed(frame[:5], lambda t, s, pl: seen.append((t, s, pl)))
    parser.feed(frame[5:], lambda t, s, pl: seen.append((t, s, pl)))
    assert seen == [(p.DATA_CHUNK, 0x42, payload)]
    assert parser.frames == 1


def test_parser_resync_after_garbage_and_bad_crc() -> None:
    frame = p.encode(p.PING, 1)
    seen = []
    parser = p.FrameParser()
    parser.feed(b"junk\x00\x01\x02" + frame, lambda t, s, pl: seen.append((t, s)))
    assert seen == [(p.PING, 1)]
    assert parser.dropped_bytes == len(b"junk\x00\x01\x02")

    corrupted = bytearray(p.encode(p.PING, 2))
    corrupted[-1] ^= 0x10
    parser = p.FrameParser()
    parser.feed(bytes(corrupted) + frame, lambda t, s, pl: seen.append((t, s)))
    assert parser.crc_errors == 1
    assert parser.frames == 1  # the second PING still parses


def test_parser_rejects_oversized_length() -> None:
    parser = p.FrameParser()
    parser.feed(bytes([p.SOF, p.CONFIG, 1, 0xFF, 0x7F]), None)
    assert parser.format_errors == 1
    assert parser.frames == 0


def test_config_payload_layout() -> None:
    payload = p.pack_config(sample_div=9, mode=1, chan_en=0xFF, pre_samples=1024,
                            post_samples=8192, txn_max_run=0x00FFFFFF)
    assert len(payload) == p.CONFIG_SIZE
    assert struct.unpack_from("<I", payload, 0)[0] == 0
    assert struct.unpack_from("<I", payload, 4)[0] == 9
    assert struct.unpack_from("<I", payload, 8)[0] == 1
    assert payload[12] == 0xFF
    assert struct.unpack_from("<I", payload, 17)[0] == 1024
    assert struct.unpack_from("<I", payload, 21)[0] == 8192
    assert struct.unpack_from("<I", payload, 33)[0] == 0


def test_status_layout() -> None:
    raw = (
        bytes([2, 0, 0, 0])
        + struct.pack("<I", 0x18)
        + struct.pack("<Q", 123)
        + struct.pack("<Q", 7)
        + struct.pack("<H", 300)
        + struct.pack("<H", 900)
        + struct.pack("<II", 1, 2)
        + struct.pack("<II", 3, 4)
        + struct.pack("<II", 5, 6)
    )
    result = p.unpack_status(raw)
    assert result["state"] == 2
    assert result["status_reg"] == 0x18
    assert result["word_count"] == 123
    assert result["trig_index"] == 7
    assert result["cpu_load_permille"] == 300
    assert result["min_free_stack"] == 900
    assert [d["events"] for d in result["decoders"]] == [1, 3, 5]
    assert [d["dropped_segments"] for d in result["decoders"]] == [2, 4, 6]


def test_patgen_cfg_layout() -> None:
    ctrl = (
        p.PATGEN_EN_MASK
        | (p.PATGEN_MODE_UART_115200 << p.PATGEN_MODE_SHIFT)
        | (7 << 16)
    )
    payload = p.pack_patgen_cfg(ctrl)
    assert payload == bytes([0x31, 0x00, 0x07, 0x00])
    assert p.unpack_patgen_cfg(payload) == ctrl
    with pytest.raises(ValueError):
        p.pack_patgen_cfg(ctrl | 0x02)  # reserved bit
    with pytest.raises(ValueError):
        p.pack_patgen_cfg(p.PATGEN_EN_MASK | (6 << p.PATGEN_MODE_SHIFT))
    with pytest.raises(p.FrameError):
        p.unpack_patgen_cfg(b"\x00\x00\x00")


def test_data_begin_and_end_layout() -> None:
    begin = struct.pack("<Q", 0x1122334455667788) + bytes([1, 0, 0, 0]) + struct.pack(
        "<I", 100_000_000
    )
    meta = p.unpack_data_begin(begin)
    assert meta["total_words"] == 0x1122334455667788
    assert meta["encoding"] == 1
    assert meta["sample_rate_hz"] == 100_000_000
    assert p.unpack_data_end(struct.pack("<I", 0xDEADBEEF)) == 0xDEADBEEF
