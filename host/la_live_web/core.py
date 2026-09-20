"""Portable live-event hub for the host web bridge (V02).

Formatting is a byte-for-byte port of the gateway's ``la_web_core``
(``fw-esp32/components/la_web``): one browser message per decoded event
frame, ``{"s":<slot>,"e":[[<t_sample>,<kind>,"<hex>"],...]}``, lowercase hex,
16-byte data cap and no partial message on truncation. The hub owns the
bounded replay ring, the single-client policy (fan-out ready for more) and the
drop accounting; it is free of socket and CLI code so it is host-tested.
"""

from __future__ import annotations

import json
import queue
import threading
import time
from collections import deque
from typing import Iterable, Mapping

SLOTS = 3
MAX_DATA = 16
DEFAULT_CAPACITY = 4096
DEFAULT_RING_SIZE = 4096
DEFAULT_CLIENT_QUEUE = 256


class HubFull(RuntimeError):
    """``LiveEventHub.attach()`` refused: the client limit is reached."""


class MessageBuilder:
    """Mirrors ``la_web_core_t`` begin/event/end semantics.

    ``capacity`` counts the trailing NUL like the C buffer, so a message of
    ``capacity - 1`` characters fits and anything longer sets ``truncated``.
    """

    def __init__(self, capacity: int = DEFAULT_CAPACITY) -> None:
        self.capacity = int(capacity)
        self.parts: list[str] = []
        self.length = 0
        self.events = 0
        self.slots = [0] * SLOTS
        self.truncated = False
        self.begun = False

    def _append(self, text: str) -> bool:
        if self.length + len(text) >= self.capacity:
            self.truncated = True
            return False
        self.parts.append(text)
        self.length += len(text)
        return True

    def begin(self, slot: int) -> bool:
        slot = int(slot)
        if not 0 <= slot < SLOTS:
            return False
        if not self._append('{"s":%d,"e":[' % slot):
            return False
        self.slots[slot] += 1
        self.begun = True
        return True

    def event(self, kind: int, t_sample: int, data: bytes | bytearray = b"") -> bool:
        payload = bytes(data or b"")
        if len(payload) > MAX_DATA:
            payload = payload[:MAX_DATA]
        if self.events > 0 and not self._append(","):
            return False
        if not self._append("[%d,%d,\"" % (int(t_sample), int(kind))):
            return False
        for value in payload:
            if not self._append("%02x" % value):
                return False
        if not self._append("\"]"):
            return False
        self.events += 1
        return True

    def end(self) -> str | None:
        if not self.begun:
            return None
        if not self._append("]}"):
            return None
        if self.truncated:
            return None
        return "".join(self.parts)


def format_frame(events: Iterable[Mapping], capacity: int = DEFAULT_CAPACITY) -> str | None:
    """Formats one decoded event frame exactly like the gateway.

    The message slot is the first record's slot and every record in the frame
    is appended to that message (``la_web_server.c`` behaviour). Returns
    ``None`` for an empty frame, an out-of-range first slot or truncation.
    """
    records = list(events)
    if not records:
        return None
    builder = MessageBuilder(capacity)
    if not builder.begin(records[0]["slot"]):
        return None
    for record in records:
        if not builder.event(record["kind"], record["t_sample"],
                             record.get("data", b"")):
            return None
    return builder.end()


class ClientFeed:
    """One attached browser: a bounded queue plus its own drop counter."""

    def __init__(self, queue_size: int = DEFAULT_CLIENT_QUEUE) -> None:
        self.queue: queue.Queue[str] = queue.Queue(maxsize=int(queue_size))
        self.dropped = 0

    def offer(self, message: str) -> bool:
        try:
            self.queue.put_nowait(message)
            return True
        except queue.Full:
            self.dropped += 1
            return False


class LiveEventHub:
    """Bounded replay ring, single-client policy and drop accounting."""

    def __init__(self, ring_size: int = DEFAULT_RING_SIZE, max_clients: int = 1,
                 client_queue: int = DEFAULT_CLIENT_QUEUE,
                 capacity: int = DEFAULT_CAPACITY) -> None:
        self.ring_size = int(ring_size)
        self.max_clients = int(max_clients)
        self.client_queue = int(client_queue)
        self.capacity = int(capacity)
        self.state = "idle"
        self.target = ""
        self.decoder = ""
        self.error: str | None = None
        self._lock = threading.Lock()
        self._ring: deque[str] = deque()
        self._feeds: list[ClientFeed] = []
        self._events = 0
        self._messages = 0
        self._sent = 0
        self._ring_dropped = 0
        self._client_dropped = 0
        self._format_dropped = 0
        self._refused = 0
        self._started = time.monotonic()

    @property
    def clients(self) -> int:
        with self._lock:
            return len(self._feeds)

    @property
    def events(self) -> int:
        with self._lock:
            return self._events

    def publish_frame(self, events: Iterable[Mapping],
                      limit: int | None = None) -> str | None:
        """Formats and publishes one decoded event frame.

        ``limit`` caps the records published from this frame (the final batch
        of a ``--count`` session may overshoot the requested count). Returns
        the message that was published, or ``None`` when the frame was empty
        or malformed/truncated (counted as ``format_dropped``). A full client
        queue drops the message for that client only.
        """
        records = list(events)
        if limit is not None:
            if limit <= 0:
                return None
            records = records[:limit]
        if not records:
            return None
        message = format_frame(records, self.capacity)
        if message is None:
            with self._lock:
                self._format_dropped += 1
            return None
        with self._lock:
            self._events += len(records)
            self._messages += 1
            self._ring.append(message)
            if len(self._ring) > self.ring_size:
                self._ring.popleft()
                self._ring_dropped += 1
            for feed in self._feeds:
                if feed.offer(message):
                    self._sent += 1
                else:
                    self._client_dropped += 1
        return message

    def attach(self) -> ClientFeed:
        """Attaches a client and replays the ring; raises ``HubFull``."""
        with self._lock:
            if len(self._feeds) >= self.max_clients:
                self._refused += 1
                raise HubFull(
                    f"live view already has {len(self._feeds)} client(s); "
                    "the host bridge is single-client"
                )
            feed = ClientFeed(self.client_queue)
            for message in self._ring:
                if not feed.offer(message):
                    self._client_dropped += 1
            self._feeds.append(feed)
            return feed

    def detach(self, feed: ClientFeed) -> None:
        with self._lock:
            if feed in self._feeds:
                self._feeds.remove(feed)

    def stats(self) -> dict:
        with self._lock:
            client_dropped = self._client_dropped
            return {
                "clients": len(self._feeds),
                "events": self._events,
                "messages": self._messages,
                "sent": self._sent,
                "dropped": self._ring_dropped + client_dropped + self._format_dropped,
                "ring": len(self._ring),
                "ring_capacity": self.ring_size,
                "ring_dropped": self._ring_dropped,
                "client_dropped": client_dropped,
                "format_dropped": self._format_dropped,
                "refused": self._refused,
                "state": self.state,
                "target": self.target,
                "decoder": self.decoder,
                "error": self.error,
                "uptime_s": round(time.monotonic() - self._started, 3),
            }

    def stats_message(self) -> str:
        return json.dumps({"stats": self.stats()}, separators=(",", ":"))
