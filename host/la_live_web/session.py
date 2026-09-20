"""Interactive live-session controller for the host bridge (V07).

The bridge owns the analyzer transport for its lifetime; the browser starts
and stops live sessions through `/control/start` and `/control/stop`. Each
session runs on a worker thread and publishes decoded frames through the hub,
so the page can be opened first and stream on demand. The transport stays open
between sessions (one owner), and a stop ends the session cleanly with
LIVE_STOP before the state returns to idle.
"""

from __future__ import annotations

import threading
import time
from typing import Callable

from .core import LiveEventHub


class LiveSessionController:
    """Start/stop one live session at a time; restartable after stop."""

    def __init__(self, client, hub: LiveEventHub, decoder_config: bytes,
                 stream_config: bytes, *, count: int | None = None,
                 timeout_s: float | None = None,
                 event_format: int = 0, stop_bytes: bytes | None = None,
                 prepare: Callable[[], None] | None = None,
                 stop_on_overrun: bool = False) -> None:
        self.client = client
        self.hub = hub
        self.decoder_config = decoder_config
        self.stream_config = stream_config
        self.count = count
        self.timeout_s = timeout_s
        self.event_format = event_format
        self.stop_bytes = stop_bytes
        self.prepare = prepare
        self.stop_on_overrun = stop_on_overrun
        self.sessions = 0
        self.error: str | None = None
        self._lock = threading.Lock()
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()

    @property
    def state(self) -> str:
        with self._lock:
            if self._thread is not None and self._thread.is_alive():
                return "stopping" if self._stop.is_set() else "streaming"
            return "idle"

    def start(self) -> bool:
        """Starts a session; returns False when one is already running."""
        with self._lock:
            if self._thread is not None and self._thread.is_alive():
                return False
            self._stop = threading.Event()
            self.error = None
            self.sessions += 1
            thread = threading.Thread(target=self._run,
                                      name="la-live-session", daemon=True)
            self._thread = thread
        self.hub.state = "streaming"
        self.hub.error = None
        thread.start()
        return True

    def stop(self, timeout: float = 5.0) -> bool:
        """Requests a stop and waits; True when the session ended in time."""
        self._stop.set()
        with self._lock:
            self.hub.state = "stopping" if (
                self._thread is not None and self._thread.is_alive()) else "idle"
            thread = self._thread
        if thread is not None:
            thread.join(timeout=timeout)
        return not (thread is not None and thread.is_alive())

    def wait(self, timeout: float | None = None) -> None:
        with self._lock:
            thread = self._thread
        if thread is not None:
            thread.join(timeout=timeout)

    def _publish(self, frame_events) -> None:
        limit = None if self.count is None else max(0, self.count - self.hub.events)
        self.hub.publish_frame(frame_events, limit=limit)

    def _run(self) -> None:
        try:
            if self.prepare is not None:
                self.prepare()
            self.client.live(
                self.decoder_config,
                config=self.stream_config,
                count=self.count,
                timeout_s=self.timeout_s,
                event_format=self.event_format,
                stop_on_match=self.stop_bytes is not None,
                collect_events=False,
                stop_on_overrun=self.stop_on_overrun,
                on_event=self._publish,
                stop_event=self._stop,
            )
        except Exception as exc:  # noqa: BLE001 - reported to the page
            self.error = str(exc)
        finally:
            with self._lock:
                self._thread = None
            self.hub.state = "idle"
            self.hub.error = self.error
