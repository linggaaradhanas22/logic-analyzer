"""Synchronous analyzer client: ping, status and burst capture flows.

Implements the host side of protocol.md: per-command request/response with
ACK/NAK correlation, the burst DATA_BEGIN/DATA_CHUNK/DATA_END transfer with
batch ACKs every 16 chunks (plus a drain at the end) and end-to-end CRC
verification. All transfers run inside one connection and stop on a
deadline; parser CRC/format counters are reported with every result.
"""

from __future__ import annotations

import queue
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


def account_test_records(result: dict) -> dict:
    """U03 loss accounting for AnalyzerClient.test_records().

    Counts missing, duplicated, reordered and NAKed records separately and
    reports the parser CRC/format errors. One host write is one transport
    send, not one USB packet; when an ACK is missing the device-side JTAG
    trace is the authority on whether the record reached the controller."""
    sent_sequences = [sequence for _record_id, sequence in result["sent"]]
    acked = [entry for entry in result["acked"] if isinstance(entry, int)]
    naks = [entry for entry in result["acked"] if not isinstance(entry, int)]
    write_errors = result.get("write_errors", [])
    seen: set[int] = set()
    duplicates: list[int] = []
    for sequence in acked:
        if sequence in seen:
            duplicates.append(sequence)
        seen.add(sequence)
    sent_set = set(sent_sequences)
    missing = [sequence for sequence in sent_sequences if sequence not in seen]
    expected = [sequence for sequence in sent_sequences if sequence in seen]
    observed = [sequence for sequence in acked if sequence in sent_set]
    return {
        "sent": len(sent_sequences),
        "acked": len(acked),
        "missing": missing,
        "duplicates": duplicates,
        "reordered": observed != expected,
        "naks": naks,
        "write_errors": write_errors,
        "crc_errors": result["parser"]["crc_errors"],
        "format_errors": result["parser"]["format_errors"],
        "clean": (not missing and not duplicates and observed == expected and
                  not naks and not write_errors and
                  result["parser"]["crc_errors"] == 0 and
                  result["parser"]["format_errors"] == 0),
    }


class AnalyzerClient:
    def __init__(self, transport: Transport) -> None:
        self._transport = transport
        self._sequence = 0
        self._parser = p.FrameParser()
        self._log_lines: list[str] = []
        # ACK window pacing for burst captures; the U06 drain gate lowers it
        # to keep the device window flowing (see the U06 report).
        self.ack_batch = ACK_BATCH

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

    def raw_send(self, data: bytes) -> None:
        """P8.4 fault injection: sends raw bytes outside the protocol framing
        (garbage-flood parser-resync tests)."""
        self._transport.send(data)

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

    def _pump(self, deadline: float, handler, deadline_ok: bool = False,
              recv_cap: float = 0.02, stop_check=None, poll=None) -> None:
        """Feeds the parser until handler() returns True, the deadline passes
        or the transport fails. handler receives (type, sequence, payload).
        With deadline_ok the deadline is a normal stop (live streaming);
        otherwise it raises TransportError. recv_cap bounds each read; the
        drain gate passes 0 to poll without the timeout tail. stop_check is an
        optional predicate polled every iteration so another thread can end a
        live session promptly; poll is an optional callback run every
        iteration (the live command queue drain)."""
        stopped = False

        def on_frame(message_type, sequence, payload) -> None:
            nonlocal stopped
            if handler(message_type, sequence, payload):
                stopped = True

        while not stopped:
            if stop_check is not None and stop_check():
                return
            if poll is not None:
                poll()
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                if deadline_ok:
                    return
                raise TransportError(f"timeout waiting for the analyzer on {self.description}")
            # Keep the recv window short and large enough for a maximum frame
            # (4103 bytes): a 0.5 s cap made every partial read wait out the
            # timeout and throttled the 4 MiB USB drain (U06).
            data = self._transport.recv(65536, min(remaining, recv_cap))
            if not data:
                if self._transport.eof:
                    raise TransportError(
                        f"connection closed by the analyzer on {self.description}"
                    )
                continue
            self._parser.feed(data, on_frame)

    def _drain(self, handler) -> None:
        """Reads whatever is already buffered without waiting for a deadline.

        The 1 ms recv keeps TCP non-blocking-safe while still returning
        immediately on an idle serial link."""
        while True:
            data = self._transport.recv(4096, 0.001)
            if not data:
                return
            self._parser.feed(data, handler)

    def _drain_commands(self, command_queue):
        """Builds the live pump callback that sends queued DECODER_CFG frames.

        The pump owns the transport during a live session, so the V09 per-slot
        start/stop requests are queued by the bridge and sent here; the
        firmware ACKs each one and the pump ignores non-LIVE_START ACKs."""
        def poll() -> None:
            while True:
                try:
                    payload = command_queue.get_nowait()
                except queue.Empty:
                    return
                self._send(p.DECODER_CFG, payload)
        return poll

    def ping(self, timeout_s: float = 5.0) -> dict:
        deadline = time.monotonic() + timeout_s
        result: dict = {}

        def on_pong(message_type, sequence, payload):
            if message_type == p.PONG:
                result["pong"] = p.unpack_pong(payload)
                return True
            if message_type == p.NAK:
                nak_type, _seq, error = payload[0], payload[1], payload[2]
                if nak_type != p.PING:
                    return False  # stale NAK for an older request; ignore
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
                nak_type, _seq, error = payload[0], payload[1], payload[2]
                if nak_type != p.GET_STATUS:
                    return False  # stale NAK for an older request; ignore
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
                if nak_type != requested_type:
                    return False  # stale NAK for an older request; ignore
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

    def configure_waveform(self, enabled: bool, timeout_s: float = 5.0) -> None:
        deadline = time.monotonic() + timeout_s
        self._send(p.WAVE_CFG, bytes((1, int(bool(enabled)), 0, 0)))
        self._expect_ack(p.WAVE_CFG, "WAVE_CFG", deadline)

    def configure_decoder(self, config_payload: bytes,
                          timeout_s: float = 5.0) -> None:
        """Sends one DECODER_CFG payload and waits for its ACK.

        Works while idle and mid-session (the firmware forwards it to the
        slot's decoder task). The UI uses it on disconnect to clear latched
        slots (V09: the device latches decoder configs across sessions)."""
        deadline = time.monotonic() + timeout_s
        self._send(p.DECODER_CFG, bytes(config_payload))
        self._expect_ack(p.DECODER_CFG, "DECODER_CFG", deadline)

    def configure_link(self, transport: int, spi_clock_mhz: int = 0,
                       timeout_s: float = 5.0) -> None:
        """Selects the burst DATA_* transport (additive v1 LINK_CFG, P9.1).

        transport is p.LINK_UART (default) or p.LINK_SPI. Commands and ACKs
        always stay on the UART; only burst frames move. spi_clock_mhz sets
        the ESP32 SPI master clock (0 keeps the firmware default)."""
        deadline = time.monotonic() + timeout_s
        self._send(p.LINK_CFG, p.pack_link_cfg(transport, spi_clock_mhz))
        self._expect_ack(p.LINK_CFG, "LINK_CFG", deadline)

    def trace_control(self, action: int, timeout_s: float = 5.0,
                      start: int = 0, count: int = 0) -> None:
        """Sends TRACE_CTRL STOP/START/DUMP (P8.3 additive v1 extension).

        For DUMP, start/count select a record chunk; 0 means the firmware
        maximum. STOP/START ignore both fields."""
        deadline = time.monotonic() + timeout_s
        self._send(p.TRACE_CTRL, p.pack_trace_ctrl(action, start, count))
        self._expect_ack(p.TRACE_CTRL, "TRACE_CTRL", deadline)

    def test_records(self, record_ids, gap_s: float = 0.0,
                     settle_s: float = 2.0) -> dict:
        """U03 bulk-OUT loss detector: sends numbered TEST_RECORD frames back
        to back, then collects the per-record ACKs for a bounded settle window.

        Returns {"sent": [(record_id, sequence), ...], "acked": [...],
        "write_errors": [(record_id, message), ...], "parser": {...}}.
        `acked` holds the acknowledged request sequences in arrival order plus
        ("nak", sequence, error) tuples. One host write is one transport send;
        it is not assumed to be one USB packet or one device completion. A
        write that times out (device NAKs) is recorded, not retried, so the
        burst keeps its timing. ACKs are drained after every write so a long
        stream does not overflow the device TX pool before the settle window
        (2026-09-19: the 200-record stream must stay clean). The device-side
        JTAG trace records the same ids."""
        sent: list[tuple[int, int]] = []
        acked: list = []
        write_errors: list[tuple[int, str]] = []
        deadline = time.monotonic() + settle_s

        def on_frame(message_type, sequence, payload):
            if message_type == p.ACK and len(payload) >= 2 and \
                    payload[0] == p.TEST_RECORD:
                acked.append(payload[1])
            elif message_type == p.NAK and len(payload) >= 3 and \
                    payload[0] == p.TEST_RECORD:
                acked.append(("nak", payload[1], payload[2]))
            return False

        for record_id in record_ids:
            sequence = self._next_sequence()
            try:
                self._transport.send(
                    p.encode(p.TEST_RECORD, sequence,
                             p.pack_test_record(record_id))
                )
            except TransportError as exc:
                write_errors.append((record_id, str(exc)))
            sent.append((record_id, sequence))
            if gap_s > 0.0:
                time.sleep(gap_s)
            self._drain(on_frame)

        self._pump(deadline, on_frame, deadline_ok=True)
        return {"sent": sent, "acked": acked, "write_errors": write_errors,
                "parser": self._parser_stats()}

    def trace_dump(self, timeout_s: float = 30.0,
                   chunk: int = 64) -> list[str]:
        """P8.3: stops tracing and collects the LOG dump chunk by chunk.

        The firmware emits at most its chunk maximum records per request
        (bounded responses keep slow transports responsive); each request gets
        its own timeout_s window. Returns the raw dump lines (header, object
        table, records, END)."""
        lines: list[str] = []
        start = 0
        final_end = ""
        while True:
            deadline = time.monotonic() + timeout_s
            received: list[str] = []
            done = False

            def on_dump(message_type, sequence, payload):
                nonlocal done
                if message_type == p.ACK:
                    # The dump follows the ACK; keep collecting until END.
                    return False
                if message_type == p.LOG:
                    line = payload.decode("utf-8", errors="replace")
                    received.append(line)
                    if line.startswith("END "):
                        done = True
                        return True
                    return False
                if message_type == p.NAK:
                    _type, _seq, error = payload[0], payload[1], payload[2]
                    raise AnalyzerError(f"TRACE_CTRL rejected: {nak_text(error)}", error)
                return False

            self._send(p.TRACE_CTRL,
                       p.pack_trace_ctrl(p.TRACE_DUMP, start, chunk))
            self._pump(deadline, on_dump)
            if not done:
                raise TransportError("trace dump ended without an END marker")
            for line in received:
                if line.startswith("END "):
                    final_end = line
                else:
                    lines.append(line)
            parts = received[-1].split()
            next_index = int(parts[1]) if len(parts) > 1 else start
            total = int(parts[2]) if len(parts) > 2 else next_index
            if next_index <= start or next_index >= total:
                break
            start = next_index
        if final_end:
            lines.append(final_end)
        return lines

    def capture(self, config: bytes, timeout_s: float = 30.0,
                recv_cap: float = 0.02, cancel_event=None) -> CaptureData:
        """Arms a burst capture with a wire CONFIG payload and collects the
        DATA_BEGIN/CHUNK*/END transfer, ACKing every self.ack_batch chunks.

        recv_cap bounds each read during the transfer; the U06 drain gate
        passes 0 to poll without the timeout tail."""
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
                if len(pending_acks) >= self.ack_batch:
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

        self._pump(deadline, on_data, recv_cap=recv_cap,
                   stop_check=(cancel_event.is_set if cancel_event is not None else None))
        if cancel_event is not None and cancel_event.is_set():
            # The capture worker remains the sole reader. An HTTP reset/Abort
            # must never race a second ACK pump against this transfer.
            self.abort(timeout_s=2.0)
            raise AnalyzerError("capture cancelled")
        return CaptureData(meta, bytes(collected), chunks, self._parser_stats())

    def live(self, decoder_config: bytes, config: bytes | None = None,
             count: int | None = None, timeout_s: float | None = None,
             read_stall_s: float = 0.0,
             on_started=None, stop_on_overrun: bool = True,
             collect_events: bool = True,
             event_format: int = p.EVT_FORMAT_RECORDS,
             stop_on_match: bool = False,
              on_event=None, stop_event=None, command_queue=None,
              on_waveform=None) -> list[dict]:
        """Configures the live stream and collects decoded events (P7.5).

        `config` is an optional 37-byte STREAM CONFIG payload sent before the
        decoder slot; the P7 firmware requires it (LIVE_START without a
        latched STREAM CONFIG is NAKed). Stops after `count` events, after an
        EVT_OVERRUN, or on the deadline; a caller interrupt leaves LIVE_STOP
        to the command. `read_stall_s` is the P8.4 fault-injection hook: it
        stops reading for that many seconds right after the LIVE_START ACK so
        the gateway relay buffers fill and the link-overrun report is
        exercised. `on_started` is called once when the LIVE_START ACK is
        seen (P7.6 isolated-byte trigger). Every decoded event carries an
        `arrival` monotonic timestamp for P8.2 latency measurements.

        `stop_on_overrun=False` keeps draining after an EVT_OVERRUN so a
        sustained-overload session runs for its full deadline (P8.5
        throughput measurement). `collect_events=False` counts decoded events
        without retaining them, so a long high-rate drain stays bounded; the
        returned list then holds only the overrun records. `event_format`
        selects the encoding (P9.2): the default 0 keeps the v1 EVT_DECODED
        records, 1 requests the compact EVT_BLOCK encoding with an EVT_CFG
        frame before DECODER_CFG.         `stop_on_match=True` (P7.7) stops the pump
        when the firmware reports an EVT_TRIGGERED frame for a decoded stop
        pattern; the match sample index is kept in `self.live_match_index`.
        `self.live_stats` reports the decoded event count, received event
        frames and their wire bytes for the comparison of the two
        encodings.

        `on_event(frame_events)` (V02) is an optional sink called with the
        decoded event dicts of each event frame as they arrive, including the
        final batches drained while waiting for the LIVE_STOP ACK. It must not
        raise or block; the host web bridge uses it to publish the browser
        feed. It is called before the `count` trimming, so a sink that needs
        an exact count applies its own limit.

        `stop_event` (V07) is an optional `threading.Event` polled by the
        pump; when set, the session stops promptly and still sends LIVE_STOP,
        so a browser Stop button can end a live session from another thread.

        `decoder_config` may be one 20-byte DECODER_CFG payload or a list of
        them (V09); each is ACKed in order before LIVE_START. `command_queue`
        is an optional `queue.Queue` of DECODER_CFG payloads sent from the
        pump loop while the session runs, so a per-slot start/stop can enable
        or disable a decoder without restarting the session; their ACKs are
        ignored by the live pump (the firmware only NAKs a bad config)."""
        if timeout_s is None:
            deadline = float("inf")
        else:
            deadline = time.monotonic() + timeout_s
        self.live_stats = {"events": 0, "frames": 0, "wire_bytes": 0, "overruns": 0}
        self.live_match_index: int | None = None
        if isinstance(decoder_config, (bytes, bytearray)):
            decoder_configs = [bytes(decoder_config)]
        else:
            decoder_configs = [bytes(config) for config in decoder_config]
        if config is not None:
            self._send(p.CONFIG, config)
            self._expect_ack(p.CONFIG, "CONFIG", deadline)
        # The firmware latch persists across sessions, so always set the
        # requested format explicitly. A pre-P9.2 peer NAKs EVT_CFG with
        # unsupported; for the default records format that is harmless.
        try:
            self._send(p.EVT_CFG, p.pack_evt_cfg(event_format))
            self._expect_ack(p.EVT_CFG, "EVT_CFG", deadline)
        except AnalyzerError as exc:
            if (event_format != p.EVT_FORMAT_RECORDS or
                    exc.error_code != p.ERROR_UNSUPPORTED):
                raise
        for config_payload in decoder_configs:
            self._send(p.DECODER_CFG, config_payload)
            self._expect_ack(p.DECODER_CFG, "DECODER_CFG", deadline)
        self._send(p.LIVE_START)
        collected: list[dict] = []
        stop_requested = False
        start_acknowledged = False
        decoded_seen = 0

        def events_of(message_type: int, payload: bytes):
            if message_type == p.EVT_BLOCK:
                return p.iter_block_events(payload)
            return p.iter_decoded_events(payload)

        def account_frame(message_type: int, payload: bytes) -> None:
            if message_type in (p.EVT_DECODED, p.EVT_BLOCK):
                self.live_stats["frames"] += 1
                self.live_stats["wire_bytes"] += len(payload) + p.FRAME_OVERHEAD

        def on_live(message_type, sequence, payload):
            nonlocal stop_requested, start_acknowledged, decoded_seen
            if message_type == p.EVT_WAVEFORM:
                if on_waveform is not None:
                    on_waveform(p.unpack_waveform(payload))
                return False
            if message_type == p.ACK:
                acked_type, _acked_sequence = payload[0], payload[1]
                if acked_type == p.LIVE_START:
                    start_acknowledged = True
                    if read_stall_s > 0.0:
                        time.sleep(read_stall_s)
                    if on_started is not None:
                        on_started()
                return False
            account_frame(message_type, payload)
            if message_type in (p.EVT_DECODED, p.EVT_BLOCK):
                arrival = time.monotonic()
                frame_events = [
                    {"slot": slot, "kind": kind, "t_sample": t_sample,
                     "data": data, "arrival": arrival}
                    for slot, kind, t_sample, data in events_of(message_type, payload)
                ]
                if on_event is not None:
                    on_event(frame_events)
                if not collect_events:
                    for _event in frame_events:
                        decoded_seen += 1
                        if count is not None and decoded_seen >= count:
                            stop_requested = True
                            break
                    return stop_requested
                for event in frame_events:
                    collected.append(event)
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
            if message_type == p.EVT_TRIGGERED:
                # P7.7: a decoded stop pattern matched (or a PL trigger in a
                # live session); the caller decides whether that ends the
                # session.
                self.live_match_index = int.from_bytes(payload[:8], "little")
                if stop_on_match:
                    stop_requested = True
                    return True
                return False
            if message_type == p.EVT_OVERRUN:
                dropped = int.from_bytes(payload[:4], "little")
                stage = payload[4]
                self.live_stats["overruns"] += 1
                collected.append({"overrun": True, "dropped": dropped, "stage": stage})
                return stop_on_overrun
            if message_type == p.NAK:
                nak_type, _seq, error = payload[0], payload[1], payload[2]
                raise AnalyzerError(
                    f"live stream failed: NAK on 0x{nak_type:02X} ({nak_text(error)})", error
                )
            if message_type == p.LOG:
                self._log_lines.append(payload.decode("utf-8", errors="replace"))
            return False

        self._pump(deadline, on_live, deadline_ok=True,
                   stop_check=(stop_event.is_set if stop_event is not None else None),
                   poll=(self._drain_commands(command_queue)
                         if command_queue is not None else None))
        self._send(p.LIVE_STOP)
        stop_deadline = time.monotonic() + 5.0

        def on_stop(message_type, sequence, payload):
            """Keeps collecting while waiting for the LIVE_STOP ACK: the final
            batches and any EVT_OVERRUN report may still be in flight."""
            if message_type == p.EVT_WAVEFORM:
                if on_waveform is not None:
                    on_waveform(p.unpack_waveform(payload))
                return False
            if message_type == p.ACK:
                return payload[0] == p.LIVE_STOP
            account_frame(message_type, payload)
            if message_type in (p.EVT_DECODED, p.EVT_BLOCK):
                arrival = time.monotonic()
                frame_events = [
                    {"slot": slot, "kind": kind, "t_sample": t_sample,
                     "data": data, "arrival": arrival}
                    for slot, kind, t_sample, data in events_of(message_type, payload)
                ]
                if on_event is not None:
                    on_event(frame_events)
                if collect_events:
                    collected.extend(frame_events)
                return False
            if message_type == p.EVT_OVERRUN:
                dropped = int.from_bytes(payload[:4], "little")
                stage = payload[4]
                self.live_stats["overruns"] += 1
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
        self.live_stats["events"] = (
            len(collected) if collect_events else decoded_seen
        )
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
