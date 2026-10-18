"""Interactive live-session controller for the host bridge (V07/V09).

The bridge owns the analyzer transport for its lifetime; the browser starts
and stops live sessions through `/control/start` and `/control/stop`. V07
added a whole-session controller; V09 makes each decoder slot independent:

- `start_slot(n)` enables slot n. If no session is running it starts one with
  just that slot; if a session is running the slot's DECODER_CFG is queued to
  the live pump, which sends it mid-session (the firmware forwards it to that
  slot's decoder task).
- `stop_slot(n)` queues an OFF config for that slot, or ends the session with
  LIVE_STOP when it was the last enabled slot.

Other slots keep streaming untouched. Each session runs on a worker thread and
publishes decoded frames through the hub; the transport stays open between
sessions (one owner).
"""

from __future__ import annotations

import queue
import threading
from typing import Callable

from ..la_cli import protocol as p
from .core import LiveEventHub


class LiveSessionController:
    """Per-slot start/stop over one live session at a time."""

    def __init__(self, client, hub: LiveEventHub,
                 slot_configs: dict[int, bytes], stream_config: bytes, *,
                 count: int | None = None, timeout_s: float | None = None,
                 event_format: int = 0, stop_bytes: bytes | None = None,
                  prepare: Callable[[], None] | None = None,
                  stop_on_overrun: bool = False,
                  on_waveform=None) -> None:
        self.client = client
        self.hub = hub
        self.slot_configs = {int(slot): bytes(config)
                             for slot, config in slot_configs.items()}
        self.stream_config = stream_config
        self.count = count
        self.timeout_s = timeout_s
        self.event_format = event_format
        self.stop_bytes = stop_bytes
        self.prepare = prepare
        self.stop_on_overrun = stop_on_overrun
        self.on_waveform = on_waveform
        self.sessions = 0
        self.error: str | None = None
        self.enabled: set[int] = set()
        self._lock = threading.Lock()
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()
        self._streaming = False
        self._commands: queue.Queue[bytes] = queue.Queue()
        self._publish_slots()

    # -- state ---------------------------------------------------------------

    @property
    def state(self) -> str:
        with self._lock:
            if self._thread is not None and self._thread.is_alive():
                return "stopping" if self._stop.is_set() else "streaming"
            return "idle"

    def slot_states(self) -> list[str]:
        with self._lock:
            return self._slot_states_locked()

    def _slot_states_locked(self) -> list[str]:
        alive = self._thread is not None and self._thread.is_alive()
        states: list[str] = []
        for slot in range(3):
            if slot not in self.slot_configs:
                states.append("unconfigured")
            elif alive and slot in self.enabled:
                states.append("streaming")
            else:
                states.append("idle")
        return states

    def _publish_slots(self) -> None:
        with self._lock:
            self.hub.slots = self._slot_states_locked()

    # -- control -------------------------------------------------------------

    def _begin_session_locked(self, slots: set[int]) -> threading.Thread:
        self.enabled = set(slots)
        self._stop = threading.Event()
        self.error = None
        self._streaming = False
        self.sessions += 1
        thread = threading.Thread(target=self._run,
                                  name="la-live-session", daemon=True)
        self._thread = thread
        self.hub.state = "streaming"
        self.hub.error = None
        return thread

    def start_slot(self, slot: int) -> tuple[bool, str]:
        """Enables one slot; returns (ok, message)."""
        with self._lock:
            if slot not in self.slot_configs:
                return False, f"no decoder configured for slot {slot}"
            if self._thread is not None and self._thread.is_alive():
                if slot in self.enabled:
                    return True, "already streaming"
                self.enabled.add(slot)
                if self._streaming:
                    self._commands.put(self.slot_configs[slot])
                self.hub.slots = self._slot_states_locked()
                return True, "streaming"
            thread = self._begin_session_locked({slot})
        self._publish_slots()
        thread.start()
        return True, "streaming"

    def stop_slot(self, slot: int) -> bool:
        """Disables one slot; ends the session when it was the last one."""
        end_session = False
        with self._lock:
            if self._thread is None or not self._thread.is_alive():
                self.enabled.discard(slot)
                self.hub.slots = self._slot_states_locked()
                return True
            if slot not in self.enabled:
                return True
            self.enabled.discard(slot)
            if not self.enabled:
                self._stop.set()
                self.hub.state = "stopping"
                end_session = True
            else:
                self._commands.put(p.pack_decoder_off(slot))
            self.hub.slots = self._slot_states_locked()
        if end_session:
            self.stop(timeout=5.0)
        return True

    def stop_all(self) -> bool:
        with self._lock:
            self.enabled.clear()
            self._stop.set()
            if self._thread is not None and self._thread.is_alive():
                self.hub.state = "stopping"
            self.hub.slots = self._slot_states_locked()
        return self.stop(timeout=5.0)

    def start_all(self) -> bool:
        """Autostart: enables every configured slot in one session."""
        with self._lock:
            if self._thread is not None and self._thread.is_alive():
                return False
            slots = set(self.slot_configs)
            if not slots:
                return False
            thread = self._begin_session_locked(slots)
        self._publish_slots()
        thread.start()
        return True

    def stop(self, timeout: float = 5.0) -> bool:
        """Requests a stop and waits; True when the session ended in time."""
        self._stop.set()
        with self._lock:
            thread = self._thread
        if thread is not None:
            thread.join(timeout=timeout)
        return not (thread is not None and thread.is_alive())

    def wait(self, timeout: float | None = None) -> None:
        with self._lock:
            thread = self._thread
        if thread is not None:
            thread.join(timeout=timeout)

    # -- session thread ------------------------------------------------------

    def _publish(self, frame_events) -> None:
        limit = None if self.count is None else max(0, self.count - self.hub.events)
        self.hub.publish_frame(frame_events, limit=limit)

    def _run(self) -> None:
        try:
            with self._lock:
                self._streaming = True
                enabled = set(self.enabled)
            # The device latches decoder configs across sessions, so a new
            # session must state its slot set explicitly: enabled slots get
            # their config and every other slot gets an OFF.
            configs = [
                self.slot_configs[slot] if slot in enabled
                else p.pack_decoder_off(slot)
                for slot in range(3)
            ]
            self._publish_slots()
            if self.prepare is not None:
                self.prepare()
            extra = {"on_waveform": self.on_waveform} if self.on_waveform is not None else {}
            self.client.live(
                configs,
                config=self.stream_config,
                count=self.count,
                timeout_s=self.timeout_s,
                event_format=self.event_format,
                stop_on_match=self.stop_bytes is not None,
                collect_events=False,
                stop_on_overrun=self.stop_on_overrun,
                on_event=self._publish,
                stop_event=self._stop,
                command_queue=self._commands,
                **extra,
            )
        except Exception as exc:  # noqa: BLE001 - reported to the page
            self.error = str(exc)
        finally:
            with self._lock:
                self.enabled.clear()
                self._streaming = False
                self._thread = None
                self.hub.slots = self._slot_states_locked()
            self.hub.state = "idle"
            self.hub.error = self.error
