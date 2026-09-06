"""Protocol primitives for the analyzer link (docs/protocol.md).

Mirrors the normative C implementation in common/la_proto (framing, CRC16
and CRC32/IEEE, the fixed payload layouts and the message type assignments).
The host CLI uses these to talk to the Zynq/ESP32; the mock analyzer in
tests uses the same code, so wire compatibility is exercised both ways.
"""

from __future__ import annotations

import struct

SOF = 0xA5
MAX_PAYLOAD = 1024
FRAME_OVERHEAD = 7

# Message types (la_proto_message_type_t).
PING, PONG, CONFIG, ARM, ABORT, GET_STATUS = 0x01, 0x02, 0x10, 0x11, 0x12, 0x13
DECODER_CFG, LIVE_START, LIVE_STOP = 0x14, 0x15, 0x16
STATUS, EVT_TRIGGERED, EVT_DECODED, EVT_OVERRUN = 0x20, 0x21, 0x22, 0x23
DATA_BEGIN, DATA_CHUNK, DATA_END = 0x30, 0x31, 0x32
ACK, NAK, LOG = 0x40, 0x41, 0x7F

# Error codes.
ERROR_NONE = 0
ERROR_BAD_LENGTH, ERROR_BAD_FIELD, ERROR_BAD_STATE = 1, 2, 3
ERROR_UNSUPPORTED, ERROR_BUSY, ERROR_OVERFLOW, ERROR_CRC = 4, 5, 6, 7

# Wire sizes of the fixed payloads.
PONG_SIZE = 16
CONFIG_SIZE = 37
STATUS_SIZE = 52
DATA_BEGIN_SIZE = 16
DATA_CHUNK_MAX = 1016
DATA_END_SIZE = 4
ACK_SIZE = 2
NAK_SIZE = 3
DECODER_CFG_SIZE = 20

_CRC16_TABLE = None
_CRC32_TABLE = None


def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT-FALSE over the frame body (protocol.md)."""
    global _CRC16_TABLE
    if _CRC16_TABLE is None:
        table = []
        for value in range(256):
            crc = value << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
            table.append(crc)
        _CRC16_TABLE = table
    crc = 0xFFFF
    for byte in data:
        crc = ((crc << 8) & 0xFFFF) ^ _CRC16_TABLE[((crc >> 8) ^ byte) & 0xFF]
    return crc


def crc32_ieee(data: bytes) -> int:
    """CRC-32/IEEE (reflected 0xEDB88320, init/final 0xFFFFFFFF)."""
    global _CRC32_TABLE
    if _CRC32_TABLE is None:
        table = []
        for value in range(256):
            crc = value
            for _ in range(8):
                crc = (crc >> 1) ^ 0xEDB88320 if crc & 1 else crc >> 1
            table.append(crc)
        _CRC32_TABLE = table
    crc = 0xFFFFFFFF
    for byte in data:
        crc = (crc >> 8) ^ _CRC32_TABLE[(crc ^ byte) & 0xFF]
    return crc ^ 0xFFFFFFFF


def encode(message_type: int, sequence: int, payload: bytes = b"") -> bytes:
    """Encodes one frame (SOF, type, seq, LE length, payload, LE CRC16)."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too large: {len(payload)}")
    body = struct.pack("<BBH", message_type, sequence & 0xFF, len(payload)) + payload
    return bytes([SOF]) + body + struct.pack("<H", crc16_ccitt(body))


class FrameError(Exception):
    """Raised when a received frame cannot be used."""


def unpack_pong(payload: bytes) -> dict:
    if len(payload) != PONG_SIZE:
        raise FrameError(f"PONG payload length {len(payload)}")
    fw_version, pl_id, uptime = struct.unpack_from("<IIQ", payload, 0)
    return {"fw_version": fw_version, "pl_id": pl_id, "uptime_ms": uptime}


def pack_config(
    *,
    ctrl_flags: int = 0,
    sample_div: int = 0,
    mode: int = 0,
    chan_en: int = 0xFF,
    trig_mask: int = 0,
    trig_value: int = 0,
    trig_rise: int = 0,
    trig_fall: int = 0,
    pre_samples: int = 0,
    post_samples: int = 4096,
    timeout: int = 0,
    txn_max_run: int = 0x00FFFFFF,
    live_flush_cycles: int = 0,
) -> bytes:
    payload = struct.pack(
        "<IIIBBBBBIIIII",  # 3 x u32, 5 x u8, 5 x u32 = 37 bytes
        ctrl_flags, sample_div, mode, chan_en, trig_mask, trig_value, trig_rise,
        trig_fall, pre_samples, post_samples, timeout, txn_max_run, live_flush_cycles,
    )
    assert len(payload) == CONFIG_SIZE, len(payload)
    return payload


def unpack_status(payload: bytes) -> dict:
    if len(payload) != STATUS_SIZE:
        raise FrameError(f"STATUS payload length {len(payload)}")
    state = payload[0]
    status_reg = struct.unpack_from("<I", payload, 4)[0]
    word_count, trig_index = struct.unpack_from("<QQ", payload, 8)
    cpu_load, min_stack = struct.unpack_from("<HH", payload, 24)
    decoders = []
    for index in range(3):
        events, dropped = struct.unpack_from("<II", payload, 28 + 8 * index)
        decoders.append({"events": events, "dropped_segments": dropped})
    return {
        "state": state,
        "status_reg": status_reg,
        "word_count": word_count,
        "trig_index": trig_index,
        "cpu_load_permille": cpu_load,
        "min_free_stack": min_stack,
        "decoders": decoders,
    }


def unpack_data_begin(payload: bytes) -> dict:
    if len(payload) != DATA_BEGIN_SIZE:
        raise FrameError(f"DATA_BEGIN payload length {len(payload)}")
    total_words = struct.unpack_from("<Q", payload, 0)[0]
    encoding = payload[8]
    sample_rate = struct.unpack_from("<I", payload, 12)[0]
    return {"total_words": total_words, "encoding": encoding, "sample_rate_hz": sample_rate}


def chunk_payload(data: bytes, byte_offset: int) -> bytes:
    if len(data) > DATA_CHUNK_MAX:
        raise ValueError(f"chunk data too large: {len(data)}")
    return struct.pack("<I", byte_offset) + data


def unpack_data_end(payload: bytes) -> int:
    if len(payload) != DATA_END_SIZE:
        raise FrameError(f"DATA_END payload length {len(payload)}")
    return struct.unpack_from("<I", payload, 0)[0]


def pack_ack(acknowledged_type: int, sequence: int) -> bytes:
    return struct.pack("<BB", acknowledged_type, sequence & 0xFF)


def pack_nak(nak_type: int, sequence: int, error: int) -> bytes:
    return struct.pack("<BBB", nak_type, sequence & 0xFF, error)


class FrameParser:
    """Incremental frame parser with the same state machine as la_proto.

    Feed bytes from a transport; complete frames arrive through the callback
    with the payload owned by the parser (copied by the callback as needed).
    """

    def __init__(self) -> None:
        self._state = "sof"
        self._type = 0
        self._sequence = 0
        self._length = 0
        self._payload = bytearray()
        self._crc_running = 0
        self._received_crc = 0
        self.frames = 0
        self.crc_errors = 0
        self.format_errors = 0
        self.dropped_bytes = 0

    def feed(self, data: bytes, on_frame) -> None:
        for value in data:
            self._byte(value, on_frame)

    def _byte(self, value: int, on_frame) -> None:
        if self._state == "sof":
            if value == SOF:
                self._state = "type"
                self._crc_running = 0xFFFF
            else:
                self.dropped_bytes += 1
        elif self._state == "type":
            self._type = value
            self._crc_running = self._crc16(self._crc_running, value)
            self._state = "sequence"
        elif self._state == "sequence":
            self._sequence = value
            self._crc_running = self._crc16(self._crc_running, value)
            self._state = "length_lo"
        elif self._state == "length_lo":
            self._length = value
            self._crc_running = self._crc16(self._crc_running, value)
            self._state = "length_hi"
        elif self._state == "length_hi":
            self._length |= value << 8
            self._crc_running = self._crc16(self._crc_running, value)
            if self._length > MAX_PAYLOAD:
                self.format_errors += 1
                self._restart_after(value)
            else:
                self._payload = bytearray()
                self._state = "crc_lo" if self._length == 0 else "payload"
        elif self._state == "payload":
            self._payload.append(value)
            self._crc_running = self._crc16(self._crc_running, value)
            if len(self._payload) == self._length:
                self._state = "crc_lo"
        elif self._state == "crc_lo":
            self._received_crc = value
            self._state = "crc_hi"
        elif self._state == "crc_hi":
            self._received_crc |= value << 8
            if self._received_crc == self._crc_running:
                self.frames += 1
                if on_frame is not None:
                    on_frame(self._type, self._sequence, bytes(self._payload))
                self._restart_after(0)
            else:
                self.crc_errors += 1
                self._restart_after(value)
        else:  # pragma: no cover - defensive
            self._restart_after(value)

    @staticmethod
    def _crc16(crc: int, value: int) -> int:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        return crc

    def _restart_after(self, value: int) -> None:
        self._state = "type" if value == SOF else "sof"
        self._length = 0
        self._payload = bytearray()
        self._received_crc = 0
        self._crc_running = 0xFFFF
