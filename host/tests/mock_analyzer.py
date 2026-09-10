"""Mock analyzer for host tests: speaks protocol.md over a TCP socket.

Implements the device side of the link (framing, fixed payloads and the
burst DATA_BEGIN/DATA_CHUNK/DATA_END transfer with batch-ACK accounting) so
the CLI and client are exercised without hardware. Scenarios:

- ping: answers PING with PONG.
- status: answers GET_STATUS with a STATUS snapshot.
- capture_raw / capture_txn: ACKs CONFIG+ARM, then streams a deterministic
  capture (EVT_TRIGGERED, DATA_BEGIN, DATA_CHUNK*, DATA_END) whose sample
  content the tests reconstruct and compare.
- nak_config: rejects CONFIG with NAK bad-field.
- silent: accepts everything and never answers (timeout tests).
- chunk_gap: omits one chunk in the middle of the transfer.
- chunk_bad_crc: corrupts one chunk frame's CRC.
- noise_then_pong: sends garbage before the PONG (parser resync tests).

After a complete transfer the server waits for the host ACK drain and
records the received ACK frames for assertion (every chunk must be
acknowledged by the client).
"""

from __future__ import annotations

import socket
import struct
import threading

from host.la_cli import protocol as p

STATUS_PAYLOAD = (
    bytes([3, 0, 0, 0])                       # state + reserved
    + struct.pack("<I", 0x00000008)           # status_reg (DONE)
    + struct.pack("<Q", 4096)                 # word_count
    + struct.pack("<Q", 12345)                # trig_index
    + struct.pack("<H", 512)                  # cpu_load_permille
    + struct.pack("<H", 2048)                 # min_free_stack
    + struct.pack("<II", 10, 0)               # uart decoder stats
    + struct.pack("<II", 0, 0)                # spi
    + struct.pack("<II", 7, 1)                # i2c
)
assert len(STATUS_PAYLOAD) == 52


def counter_samples(total: int) -> bytes:
    """Deterministic raw capture: consecutive 8-bit counter values."""
    return bytes(value & 0xFF for value in range(total))


def txn_words(samples: list[tuple[int, int]]) -> bytes:
    """Encodes [(run, sample), ...] into {run[23:0], sample[7:0]} words."""
    payload = bytearray()
    for run, sample in samples:
        payload += struct.pack("<I", (run << 8) | (sample & 0xFF))
    return bytes(payload)


class MockAnalyzer:
    def __init__(self, scenario: str) -> None:
        self.scenario = scenario
        self.port: int
        self.received_acks: list[tuple[int, int]] = []
        self.parser = p.FrameParser()
        self._listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._listener.bind(("127.0.0.1", 0))
        self.port = self._listener.getsockname()[1]
        self._listener.listen(1)
        self._thread: threading.Thread | None = None

    def __enter__(self) -> "MockAnalyzer":
        self._thread = threading.Thread(target=self._accept_loop, daemon=True)
        self._thread.start()
        return self

    def __exit__(self, *exc) -> None:
        try:
            self._listener.close()
        except OSError:
            pass
        if self._thread is not None:
            self._thread.join(timeout=5)

    @property
    def uri(self) -> str:
        return f"tcp://127.0.0.1:{self.port}"

    def _accept_loop(self) -> None:
        try:
            self._listener.settimeout(0.5)
            connection, _ = self._listener.accept()
        except OSError:
            return
        try:
            with connection:
                self._serve(connection)
        except OSError:
            pass

    # -- device side --------------------------------------------------------

    def _serve(self, connection: socket.socket) -> None:
        self._sequence = 0
        connection.settimeout(0.2)
        capture_state = {"chunk_count": 0, "ack_wanted": None}

        def on_frame(message_type, sequence, payload) -> None:
            if message_type == p.PING:
                if self.scenario == "silent":
                    return
                if self.scenario == "close_on_ping":
                    try:
                        connection.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass
                    return
                if self.scenario == "noise_then_pong":
                    self._send_raw(connection, b"\x00garbage\xff\xaa" * 4)
                self._send(connection, p.PONG, struct.pack("<IIQ", 1, 0x4C410002, 42))
            elif message_type == p.GET_STATUS:
                self._send(connection, p.STATUS, STATUS_PAYLOAD)
            elif message_type == p.ABORT:
                self._send(connection, p.ACK, p.pack_ack(p.ABORT, sequence))
            elif message_type == p.ACK:
                acked_type, acked_sequence = payload[0], payload[1]
                self.received_acks.append((acked_type, acked_sequence))
                if (capture_state["ack_wanted"] is not None and
                        len(self.received_acks) >= capture_state["ack_wanted"]):
                    capture_state["ack_wanted"] = 0  # drain satisfied
            elif message_type == p.CONFIG:
                if self.scenario == "nak_config":
                    self._send(connection, p.NAK,
                               p.pack_nak(p.CONFIG, sequence, p.ERROR_BAD_FIELD))
                else:
                    self._send(connection, p.ACK, p.pack_ack(p.CONFIG, sequence))
            elif message_type == p.ARM:
                self._send(connection, p.ACK, p.pack_ack(p.ARM, sequence))
                if self.scenario in ("capture_raw", "capture_txn"):
                    capture_state["chunk_count"] = self._stream_capture(connection)
                    capture_state["ack_wanted"] = capture_state["chunk_count"]
                elif self.scenario == "chunk_gap":
                    capture_state["chunk_count"] = self._stream_capture(
                        connection, omit_chunk=2)
                    capture_state["ack_wanted"] = 0  # client aborts; no drain
                elif self.scenario == "chunk_bad_crc":
                    self._stream_capture(connection, corrupt_chunk=2)
                    capture_state["ack_wanted"] = 0
            elif message_type == p.PATGEN_CFG:
                self._send(connection, p.ACK, p.pack_ack(p.PATGEN_CFG, sequence))
            elif message_type == p.DECODER_CFG:
                if self.scenario == "live":
                    self._send(connection, p.ACK, p.pack_ack(p.DECODER_CFG, sequence))
            elif message_type == p.LIVE_START:
                if self.scenario == "live":
                    self._send(connection, p.ACK, p.pack_ack(p.LIVE_START, sequence))
                    self._stream_live(connection)
            elif message_type == p.LIVE_STOP:
                if self.scenario == "live":
                    self._send(connection, p.ACK, p.pack_ack(p.LIVE_STOP, sequence))
            # Other frame types (NAK, unknown): ignored by the mock.

        def poll_once() -> bool:
            try:
                data = connection.recv(4096)
            except socket.timeout:
                return False
            except OSError:
                return True
            if not data:
                return True
            # The parser is stateful: partial frames survive across feeds and
            # complete-frame payloads are copied inside the callback.
            self.parser.feed(data, on_frame)
            return False

        while True:
            if poll_once():
                return
            if capture_state["ack_wanted"] == 0:
                return  # transfer finished and ACK drain satisfied

    # -- transfer -----------------------------------------------------------

    def _send_raw(self, connection: socket.socket, data: bytes) -> None:
        connection.sendall(data)

    def _send(self, connection: socket.socket, message_type: int, payload: bytes) -> None:
        sequence = self._sequence
        self._sequence = (self._sequence + 1) & 0xFF
        self._send_raw(connection, p.encode(message_type, sequence, payload))

    def _stream_capture(self, connection: socket.socket,
                        omit_chunk: int | None = None,
                        corrupt_chunk: int | None = None) -> int:
        if self.scenario in ("capture_raw", "chunk_gap", "chunk_bad_crc"):
            self.capture_payload = counter_samples(3056)
            encoding = 0
        else:
            runs = [(900, 0x00), (7, 0x55), (300, 0xFF), (43, 0xAA), (60, 0x01)]
            self.capture_payload = txn_words(runs)
            encoding = 1
        begin = struct.pack("<Q", len(self.capture_payload) // 4) + bytes(
            [encoding, 0, 0, 0]
        ) + struct.pack("<I", 10_000_000)
        # EVT_TRIGGERED precedes the transfer on the real device.
        self._send(connection, p.EVT_TRIGGERED, struct.pack("<Q", 12345))
        self._send(connection, p.DATA_BEGIN, begin)
        chunk_index = 0
        offset = 0
        while offset < len(self.capture_payload):
            data = self.capture_payload[offset:offset + p.DATA_CHUNK_MAX]
            if chunk_index == omit_chunk:
                offset += len(data)
                chunk_index += 1
                continue
            payload = p.chunk_payload(data, offset)
            if chunk_index == corrupt_chunk:
                frame = bytearray(p.encode(p.DATA_CHUNK, self._next_seq(), payload))
                frame[-1] ^= 0x01  # flip one CRC byte
                self._send_raw(connection, bytes(frame))
            else:
                self._send(connection, p.DATA_CHUNK, payload)
            offset += len(data)
            chunk_index += 1
        self._send(connection, p.DATA_END, struct.pack("<I", p.crc32_ieee(self.capture_payload)))
        return chunk_index

    def _next_seq(self) -> int:
        sequence = self._sequence
        self._sequence = (self._sequence + 1) & 0xFF
        return sequence

    def _stream_live(self, connection: socket.socket) -> None:
        """Streams three EVT_DECODED batches of decoded 'LA-TEST\\n' bytes."""
        text = b"LA-TEST\n"
        base = 10_000
        for batch in range(3):
            payload = bytearray()
            for index, byte in enumerate(text):
                t_sample = base + batch * 10_000 + index * 100
                payload += bytes([0])  # slot 0
                payload += bytes([1])  # kind BYTE
                payload += struct.pack("<Q", t_sample)
                payload += bytes([1])  # data length
                payload += bytes([byte])
            self._send(connection, p.EVT_DECODED, bytes(payload))
        self._send(connection, p.EVT_OVERRUN,
                   struct.pack("<I", 3) + bytes([1]) + b"\x00\x00\x00")
