"""Synchronous analyzer client: ping, status and burst capture flows.

Implements the host side of protocol.md: per-command request/response with
ACK/NAK correlation, the burst DATA_BEGIN/DATA_CHUNK/DATA_END transfer with
batch ACKs every 16 chunks (plus a drain at the end) and end-to-end CRC
verification. All transfers run inside one connection and stop on a
deadline; parser CRC/format counters are reported with every result.
"""

from __future__ import annotations

import time

from . import protocol as p
from .transport import Transport, TransportError

ACK_BATCH = 16


class AnalyzerError(RuntimeError):
    """Protocol-level failure (NAK or an inconsistent transfer)."""

    def __init__(self, message: str, error_code: int | None = None) -> None:
        super().__init__(message)
        self.error_code = error_code


class CaptureData:
    def __init__(self, meta: dict, data: bytes, chunks: int, parser_stats: dict) -> None:
        self.meta = meta
        self.data = data
        self.chunks = chunks
        self.parser_stats = parser_stats

    @property
    def encoding(self) -> int:
        return self.meta["encoding"]

    @property
    def sample_rate_hz(self) -> int:
        return self.meta["sample_rate_hz"]


_NAK_TEXT = {
    p.ERROR_NONE: "none",
    p.ERROR_BAD_LENGTH: "bad length",
    p.ERROR_BAD_FIELD: "bad field",
    p.ERROR_BAD_STATE: "bad state",
    p.ERROR_UNSUPPORTED: "unsupported",
    p.ERROR_BUSY: "busy",
    p.ERROR_OVERFLOW: "overflow",
    p.ERROR_CRC: "CRC",
}


def nak_text(error_code: int) -> str:
    return _NAK_TEXT.get(error_code, f"code {error_code}")


class AnalyzerClient:
    def __init__(self, transport: Transport) -> None:
        self._transport = transport
        self._sequence = 0
        self._parser = p.FrameParser()
        self._log_lines: list[str] = []

    @property
    def description(self) -> str:
        return self._transport.description

    @property
    def log_lines(self) -> list[str]:
        return list(self._log_lines)

    @property
    def parser(self) -> p.FrameParser:
        return self._parser

    def close(self) -> None:
        self._transport.close()

    def kill(self) -> None:
        """P8.4 fault injection: aborts the transport (TCP RST) so the peer
        sees an abrupt disconnect instead of a clean FIN."""
        self._transport.kill()

    def _next_sequence(self) -> int:
        sequence = self._sequence
        self._sequence = (self._sequence + 1) & 0xFF
        return sequence

    def _send(self, message_type: int, payload: bytes = b"") -> int:
        sequence = self._next_sequence()
        self._transport.send(p.encode(message_type, sequence, payload))
        return sequence

    def _parser_stats(self) -> dict:
        return {
            "frames": self._parser.frames,
            "crc_errors": self._parser.crc_errors,
            "format_errors": self._parser.format_errors,
            "dropped_bytes": self._parser.dropped_bytes,
        }

    def _pump(self, deadline: float, handler, deadline_ok: bool = False) -> None:
        """Feeds the parser until handler() returns True, the deadline passes
        or the transport fails. handler receives (type, sequence, payload).
        With deadline_ok the deadline is a normal stop (live streaming);
        otherwise it raises TransportError."""
        stopped = False

        def on_frame(message_type, sequence, payload) -> None:
            nonlocal stopped
            if handler(message_type, sequence, payload):
                stopped = True

        while not stopped:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                if deadline_ok:
                    return
                raise TransportError(f"timeout waiting for the analyzer on {self.description}")
            data = self._transport.recv(4096, min(remaining, 0.5))
            if not data:
                if self._transport.eof:
                    raise TransportError(
                        f"connection closed by the analyzer on {self.description}"
                    )
                continue
            self._parser.feed(data, on_frame)

    def ping(self, timeout_s: float = 5.0) -> dict:
        deadline = time.monotonic() + timeout_s
        result: dict = {}

        def on_pong(message_type, sequence, payload):
            if message_type == p.PONG:
                result["pong"] = p.unpack_pong(payload)
                return True
            if message_type == p.NAK:
                _nak_type, _seq, error = payload[0], payload[1], payload[2]
                raise AnalyzerError(f"PING rejected: {nak_text(error)}", error)
            if message_type == p.LOG:
                self._log_lines.append(payload.decode("utf-8", errors="replace"))
            return False

        self._send(p.PING)
        self._pump(deadline, on_pong)
        return result["pong"]

    def status(self, timeout_s: float = 5.0) -> dict:
        deadline = time.monotonic() + timeout_s
        result: dict = {}

        def on_status(message_type, sequence, payload):
            if message_type == p.STATUS:
                result["status"] = p.unpack_status(payload)
                return True
            if message_type == p.NAK:
                _nak_type, _seq, error = payload[0], payload[1], payload[2]
                raise AnalyzerError(f"GET_STATUS rejected: {nak_text(error)}", error)
            if message_type == p.LOG:
                self._log_lines.append(payload.decode("utf-8", errors="replace"))
            return False

        self._send(p.GET_STATUS)
        self._pump(deadline, on_status)
        return result["status"]

    def _expect_ack(self, requested_type: int, what: str, deadline: float) -> None:
        def on_ack(message_type, sequence, payload):
            if message_type == p.ACK:
                acked_type, acked_sequence = payload[0], payload[1]
                if acked_type == requested_type:
                    return True
                return False  # stale ACK for an older request; ignore
            if message_type == p.NAK:
                nak_type, _seq, error = payload[0], payload[1], payload[2]
                raise AnalyzerError(
                    f"{what} rejected ({nak_text(error)}) for type 0x{requested_type:02X}",
                    error,
                )
            if message_type == p.LOG:
                self._log_lines.append(payload.decode("utf-8", errors="replace"))
            return False

        self._pump(deadline, on_ack)

    def abort(self, timeout_s: float = 5.0) -> None:
        """Sends ABORT; an idempotent ACK is expected from an idle machine."""
        deadline = time.monotonic() + timeout_s
        self._send(p.ABORT)
        self._expect_ack(p.ABORT, "ABORT", deadline)

    def arm(self, config: bytes, timeout_s: float = 5.0) -> None:
        """Sends CONFIG then ARM and waits for both ACKs without collecting a
        burst (used by abort/timeout bench tests)."""
        deadline = time.monotonic() + timeout_s
        if len(config) != p.CONFIG_SIZE:
            raise ValueError(f"CONFIG payload must be {p.CONFIG_SIZE} bytes")
        self._send(p.CONFIG, config)
        self._expect_ack(p.CONFIG, "CONFIG", deadline)
        self._send(p.ARM)
        self._expect_ack(p.ARM, "ARM", deadline)

    def configure_patgen(self, patgen_ctrl: int, timeout_s: float = 5.0) -> None:
        """Latches a PATGEN_CTRL value (additive v1 PATGEN_CFG extension)."""
        deadline = time.monotonic() + timeout_s
        self._send(p.PATGEN_CFG, p.pack_patgen_cfg(patgen_ctrl))
        self._expect_ack(p.PATGEN_CFG, "PATGEN_CFG", deadline)

    def capture(self, config: bytes, timeout_s: float = 30.0) -> CaptureData:
        """Arms a burst capture with a wire CONFIG payload and collects the
        DATA_BEGIN/CHUNK*/END transfer, ACKing in batches of 16."""
        deadline = time.monotonic() + timeout_s
        if len(config) != p.CONFIG_SIZE:
            raise ValueError(f"CONFIG payload must be {p.CONFIG_SIZE} bytes")
        self._send(p.CONFIG, config)
        self._expect_ack(p.CONFIG, "CONFIG", deadline)
        self._send(p.ARM)

        meta: dict = {}
        chunks = 0
        expected_offset = 0
        pending_acks: list[int] = []
        collected = bytearray()
        arm_acknowledged = False

        def send_ack_batch() -> None:
            nonlocal pending_acks
            for seq in pending_acks:
                self._transport.send(p.encode(p.ACK, self._next_sequence(), p.pack_ack(p.DATA_CHUNK, seq)))
            pending_acks = []

        def on_data(message_type, sequence, payload):
            nonlocal chunks, expected_offset, meta, arm_acknowledged
            if message_type == p.ACK:
                acked_type, _acked_sequence = payload[0], payload[1]
                if acked_type == p.ARM:
                    arm_acknowledged = True
                return False
            if message_type == p.DATA_BEGIN:
                meta = p.unpack_data_begin(payload)
                return False
            if message_type == p.DATA_CHUNK:
                offset = int.from_bytes(payload[:4], "little")
                data = payload[4:]
                if offset != expected_offset:
                    raise AnalyzerError(
                        f"chunk gap: expected byte offset {expected_offset}, got {offset} "
                        f"(parser: {self._parser_stats()})"
                    )
                collected.extend(data)
                chunks += 1
                expected_offset += len(data)
                pending_acks.append(sequence)
                if len(pending_acks) >= ACK_BATCH:
                    send_ack_batch()
                return False
            if message_type == p.DATA_END:
                if not arm_acknowledged:
                    raise AnalyzerError("DATA_END without an ARM acknowledgement")
                end_crc = p.unpack_data_end(payload)
                send_ack_batch()
                self._finish_capture(meta, collected, chunks, end_crc)
                return True
            if message_type == p.EVT_TRIGGERED:
                return False
            if message_type == p.NAK:
                nak_type, _seq, error = payload[0], payload[1], payload[2]
                raise AnalyzerError(
                    f"burst failed: NAK on 0x{nak_type:02X} ({nak_text(error)})", error
                )
            if message_type == p.LOG:
                self._log_lines.append(payload.decode("utf-8", errors="replace"))
            return False

        self._pump(deadline, on_data)
        return CaptureData(meta, bytes(collected), chunks, self._parser_stats())

    def live(self, decoder_config: bytes, config: bytes | None = None,
             count: int | None = None, timeout_s: float | None = None,
             read_stall_s: float = 0.0) -> list[dict]:
        """Configures the live stream and collects decoded events (P7.5).

        `config` is an optional 37-byte STREAM CONFIG payload sent before the
        decoder slot; the P7 firmware requires it (LIVE_START without a
        latched STREAM CONFIG is NAKed). Stops after `count` events, after an
        EVT_OVERRUN, or on the deadline; a caller interrupt leaves LIVE_STOP
        to the command. `read_stall_s` is the P8.4 fault-injection hook: it
        stops reading for that many seconds right after the LIVE_START ACK so
        the gateway relay buffers fill and the link-overrun report is
        exercised."""
        if timeout_s is None:
            deadline = float("inf")
        else:
            deadline = time.monotonic() + timeout_s
        if config is not None:
            self._send(p.CONFIG, config)
            self._expect_ack(p.CONFIG, "CONFIG", deadline)
        self._send(p.DECODER_CFG, decoder_config)
        self._expect_ack(p.DECODER_CFG, "DECODER_CFG", deadline)
        self._send(p.LIVE_START)
        collected: list[dict] = []
        stop_requested = False
        start_acknowledged = False

        def on_live(message_type, sequence, payload):
            nonlocal stop_requested, start_acknowledged
            if message_type == p.ACK:
                acked_type, _acked_sequence = payload[0], payload[1]
                if acked_type == p.LIVE_START:
                    start_acknowledged = True
                    if read_stall_s > 0.0:
                        time.sleep(read_stall_s)
                return False
            if message_type == p.EVT_DECODED:
                for slot, kind, t_sample, data in p.iter_decoded_events(payload):
                    collected.append(
                        {"slot": slot, "kind": kind, "t_sample": t_sample, "data": data}
                    )
                    if count is not None and len(collected) >= count:
                        stop_requested = True
                        break
                if stop_requested and count is not None and len(collected) > count:
                    # A batch may overshoot the count: keep the first `count`
                    # decoded events and every overrun report (an overrun can
                    # share the read with the event that reached the count).
                    keep: list[dict] = []
                    decoded = 0
                    for event in collected:
                        if event.get("overrun") or decoded < count:
                            keep.append(event)
                            if not event.get("overrun"):
                                decoded += 1
                    collected[:] = keep
                return stop_requested
            if message_type == p.EVT_OVERRUN:
                dropped = int.from_bytes(payload[:4], "little")
                stage = payload[4]
                collected.append({"overrun": True, "dropped": dropped, "stage": stage})
                return True
            if message_type == p.NAK:
                nak_type, _seq, error = payload[0], payload[1], payload[2]
                raise AnalyzerError(
                    f"live stream failed: NAK on 0x{nak_type:02X} ({nak_text(error)})", error
                )
            if message_type == p.LOG:
                self._log_lines.append(payload.decode("utf-8", errors="replace"))
            return False

        self._pump(deadline, on_live, deadline_ok=True)
        self._send(p.LIVE_STOP)
        stop_deadline = time.monotonic() + 5.0

        def on_stop(message_type, sequence, payload):
            """Keeps collecting while waiting for the LIVE_STOP ACK: the final
            batches and any EVT_OVERRUN report may still be in flight."""
            if message_type == p.ACK:
                return payload[0] == p.LIVE_STOP
            if message_type == p.EVT_DECODED:
                for slot, kind, t_sample, data in p.iter_decoded_events(payload):
                    collected.append(
                        {"slot": slot, "kind": kind, "t_sample": t_sample, "data": data}
                    )
                return False
            if message_type == p.EVT_OVERRUN:
                dropped = int.from_bytes(payload[:4], "little")
                stage = payload[4]
                collected.append({"overrun": True, "dropped": dropped, "stage": stage})
                return False
            return False

        try:
            self._pump(stop_deadline, on_stop)
        except TransportError:
            pass  # the device may already have torn the stream down
        if count is not None:
            trimmed: list[dict] = []
            decoded_count = 0
            for event in collected:
                if event.get("overrun") or decoded_count < count:
                    trimmed.append(event)
                    if not event.get("overrun"):
                        decoded_count += 1
            collected = trimmed
        return collected

    def stop_live(self) -> None:
        """Fire-and-forget LIVE_STOP for interrupt paths."""
        try:
            self._send(p.LIVE_STOP)
        except TransportError:
            pass

    @staticmethod
    def _finish_capture(meta: dict, collected: bytearray, chunks: int, end_crc: int) -> None:
        if not meta:
            raise AnalyzerError("DATA_END without DATA_BEGIN")
        expected_bytes = meta["total_words"] * 4
        if len(collected) != expected_bytes:
            raise AnalyzerError(
                f"capture size mismatch: {len(collected)} bytes for "
                f"{meta['total_words']} words ({chunks} chunks)"
            )
        actual_crc = p.crc32_ieee(bytes(collected))
        if actual_crc != end_crc:
            raise AnalyzerError(
                f"DATA_END CRC mismatch: frame 0x{end_crc:08X}, computed 0x{actual_crc:08X}"
            )
