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
PATGEN_CFG = 0x17
TRACE_CTRL = 0x18
STATUS, EVT_TRIGGERED, EVT_DECODED, EVT_OVERRUN = 0x20, 0x21, 0x22, 0x23
DATA_BEGIN, DATA_CHUNK, DATA_END = 0x30, 0x31, 0x32
ACK, NAK, LOG = 0x40, 0x41, 0x7F

# CONFIG ctrl_flags bits (la_regs.h).
CTRL_SELFTEST_EN = 0x08
CTRL_STREAM = 0x10

# MODE.ENCODING values.
MODE_RAW = 0
MODE_TXN = 1

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
PATGEN_CFG_SIZE = 4
TRACE_CTRL_SIZE = 4

# TRACE_CTRL actions (protocol.md, additive v1 extension).
TRACE_STOP, TRACE_START, TRACE_DUMP = 1, 2, 3

# PATGEN_CTRL field masks (la_regs.h / docs/regmap.yaml).
PATGEN_EN_MASK = 0x00000001
PATGEN_MODE_MASK = 0x000000F0
PATGEN_MODE_SHIFT = 4
PATGEN_DIV_MASK = 0xFFFF0000
PATGEN_ALLOWED_MASK = 0xFFFF00F1
PATGEN_MODE_COUNTER = 0
PATGEN_MODE_WALKING_ONE = 1
PATGEN_MODE_PRBS8 = 2
PATGEN_MODE_UART_115200 = 3
PATGEN_MODE_SPI_1MHZ = 4
PATGEN_MODE_I2C_400KHZ = 5
PATGEN_MODE_UART_SINGLE = 6  # one "LA-TEST\n" frame per enable, then idle

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


def pack_patgen_cfg(patgen_ctrl: int) -> bytes:
    """PATGEN_CFG payload: raw PATGEN_CTRL value (additive v1 extension)."""
    if patgen_ctrl & ~PATGEN_ALLOWED_MASK:
        raise ValueError(f"PATGEN_CTRL reserved bits set: 0x{patgen_ctrl:08X}")
    if (patgen_ctrl & PATGEN_MODE_MASK) >> PATGEN_MODE_SHIFT > PATGEN_MODE_UART_SINGLE:
        raise ValueError(f"undefined patgen mode: 0x{patgen_ctrl:08X}")
    return struct.pack("<I", patgen_ctrl)


def unpack_patgen_cfg(payload: bytes) -> int:
    if len(payload) != PATGEN_CFG_SIZE:
        raise FrameError(f"PATGEN_CFG payload length {len(payload)}")
    return struct.unpack_from("<I", payload, 0)[0]


def pack_trace_ctrl(action: int) -> bytes:
    """TRACE_CTRL payload: action:u8 plus three reserved zero bytes (P8.3)."""
    if action not in (TRACE_STOP, TRACE_START, TRACE_DUMP):
        raise ValueError(f"undefined trace action: {action}")
    return struct.pack("<BBBB", action, 0, 0, 0)


def unpack_trace_ctrl(payload: bytes) -> int:
    if len(payload) != TRACE_CTRL_SIZE:
        raise FrameError(f"TRACE_CTRL payload length {len(payload)}")
    action = payload[0]
    if action not in (TRACE_STOP, TRACE_START, TRACE_DUMP):
        raise FrameError(f"undefined trace action: {action}")
    return action


def pack_ack(acknowledged_type: int, sequence: int) -> bytes:
    return struct.pack("<BB", acknowledged_type, sequence & 0xFF)


def pack_nak(nak_type: int, sequence: int, error: int) -> bytes:
    return struct.pack("<BBB", nak_type, sequence & 0xFF, error)


# Decoder configuration (protocol.md DECODER_CFG, 20 bytes).
DECODER_OFF, DECODER_UART, DECODER_SPI, DECODER_I2C = 0, 1, 2, 3

# EVT_DECODED event kinds (docs/decoders.md numbering).
EVENT_NAMES = {
    1: "BYTE", 2: "FRAME_ERR", 3: "PARITY_ERR", 4: "START", 5: "STOP",
    6: "DISCONTINUITY", 7: "ADDRESS", 8: "ACK", 9: "NACK",
    10: "CS_ASSERT", 11: "CS_RELEASE",
}


def pack_decoder_cfg_uart(slot: int, rx_channel: int, baud_hz: int,
                          sample_rate_hz: int, data_bits: int = 8,
                          parity: int = 0, stop_bits: int = 1,
                          inverted: int = 0) -> bytes:
    if not 0 <= slot <= 2:
        raise ValueError("decoder slot must be 0..2")
    if not 0 <= rx_channel <= 7:
        raise ValueError("UART rx channel must be 0..7")
    if not 5 <= data_bits <= 9 or parity not in (0, 1, 2) or stop_bits not in (1, 2):
        raise ValueError("invalid UART decoder parameters")
    payload = bytearray(DECODER_CFG_SIZE)
    payload[0] = slot
    payload[1] = DECODER_UART
    payload[4] = rx_channel
    payload[5] = data_bits
    payload[6] = parity
    payload[7] = stop_bits
    payload[8] = inverted
    struct.pack_into("<I", payload, 12, baud_hz)
    struct.pack_into("<I", payload, 16, sample_rate_hz)
    return bytes(payload)


def _decoder_channel(value: int, *, optional: bool = False) -> int:
    if optional and value == -1:
        return 0xFF
    if not 0 <= value <= 7:
        raise ValueError("decoder channel must be 0..7" + (" or -1" if optional else ""))
    return value


def pack_decoder_cfg_spi(slot: int, sck_channel: int, mosi_channel: int = -1,
                         miso_channel: int = -1, cs_channel: int = -1,
                         cpol: int = 0, cpha: int = 0, word_size: int = 8,
                         lsb_first: int = 0, cs_active: int = 0) -> bytes:
    """SPI DECODER_CFG (protocol.md 0x14): signed channel bytes, -1 = absent."""
    if not 0 <= slot <= 2:
        raise ValueError("decoder slot must be 0..2")
    channels = (
        _decoder_channel(sck_channel),
        _decoder_channel(mosi_channel, optional=True),
        _decoder_channel(miso_channel, optional=True),
        _decoder_channel(cs_channel, optional=True),
    )
    if mosi_channel == -1 and miso_channel == -1:
        raise ValueError("SPI needs at least one data channel")
    if cpol not in (0, 1) or cpha not in (0, 1) or lsb_first not in (0, 1) or cs_active not in (0, 1):
        raise ValueError("invalid SPI decoder flags")
    if word_size not in (8, 16):
        raise ValueError("SPI word size must be 8 or 16")
    payload = bytearray(DECODER_CFG_SIZE)
    payload[0] = slot
    payload[1] = DECODER_SPI
    payload[4:8] = bytes(channels)
    payload[8] = cpol
    payload[9] = cpha
    payload[10] = word_size
    payload[11] = lsb_first
    payload[12] = cs_active
    return bytes(payload)


def pack_decoder_cfg_i2c(slot: int, scl_channel: int, sda_channel: int) -> bytes:
    """I2C DECODER_CFG (protocol.md 0x14)."""
    if not 0 <= slot <= 2:
        raise ValueError("decoder slot must be 0..2")
    if not 0 <= scl_channel <= 7 or not 0 <= sda_channel <= 7:
        raise ValueError("I2C channels must be 0..7")
    if scl_channel == sda_channel:
        raise ValueError("I2C SCL and SDA must differ")
    payload = bytearray(DECODER_CFG_SIZE)
    payload[0] = slot
    payload[1] = DECODER_I2C
    payload[4] = scl_channel
    payload[5] = sda_channel
    return bytes(payload)


def iter_decoded_events(payload: bytes):
    """Parses an EVT_DECODED batch into (slot, kind, t_sample, data)."""
    offset = 0
    while offset < len(payload):
        if offset + 11 > len(payload):
            raise FrameError("truncated EVT_DECODED record header")
        slot = payload[offset]
        kind = payload[offset + 1]
        t_sample = struct.unpack_from("<Q", payload, offset + 2)[0]
        length = payload[offset + 10]
        offset += 11
        if offset + length > len(payload):
            raise FrameError("truncated EVT_DECODED record data")
        yield slot, kind, t_sample, payload[offset:offset + length]
        offset += length


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
