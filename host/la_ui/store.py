"""Session-memory capture store for the unified UI (W01).

A capture holds the packed words exactly as the device transferred them plus
the metadata the viewer needs. Samples are decoded lazily through numpy (a
pinned project dependency): a raw capture's sample stream is its byte stream,
and a transition capture expands ``{run[23:0], sample[7:0]}`` words. Nothing
is written to disk until the user exports.
"""

from __future__ import annotations

import threading
import time
import uuid

import numpy as np

from ..la_cli import convert

WARN_SAMPLES = 1_000_000


class Capture:
    """One captured (or opened) record; ``data`` is the packed word stream."""

    def __init__(self, *, encoding: int, sample_rate_hz: int, data: bytes,
                 total_words: int | None = None,
                 trigger_index: int | None = None, name: str = "",
                 source: str = "device",
                 annotations: dict[int, list[dict]] | None = None,
                 warnings: list[str] | None = None) -> None:
        if encoding not in (0, 1):
            raise ValueError(f"unknown capture encoding {encoding}")
        self.id = uuid.uuid4().hex[:12]
        self.encoding = int(encoding)
        self.sample_rate_hz = int(sample_rate_hz)
        self.data = bytes(data)
        self.total_words = int(total_words if total_words is not None
                               else len(self.data) // 4)
        self.total_samples = (len(self.data) if self.encoding == 0
                              else convert.count_samples(self.data, self.encoding))
        self.trigger_index = trigger_index
        self.name = name or self.id
        self.source = source
        self.has_signal = len(self.data) > 0
        self.created = time.time()
        self.annotations: dict[int, list[dict]] = annotations or {}
        self.warnings = list(warnings or [])
        if self.total_samples > WARN_SAMPLES:
            self.warnings.append(
                f"capture has {self.total_samples} samples; the view is "
                f"decimated and decoding may take a while")
        self._samples: np.ndarray | None = None
        self._samples_lock = threading.Lock()

    def samples(self) -> np.ndarray:
        """Returns one uint8 sample per timestamp (cached, read-only use)."""
        with self._samples_lock:
            if self._samples is None:
                if self.encoding == 0:
                    self._samples = np.frombuffer(self.data, dtype=np.uint8)
                else:
                    words = np.frombuffer(self.data, dtype="<u4")
                    runs = (words >> 8).astype(np.int64)
                    values = (words & 0xFF).astype(np.uint8)
                    self._samples = np.repeat(values, runs)
            return self._samples

    def info(self) -> dict:
        return {
            "id": self.id,
            "name": self.name,
            "source": self.source,
            "encoding": self.encoding,
            "sample_rate_hz": self.sample_rate_hz,
            "total_words": self.total_words,
            "total_samples": int(self.total_samples),
            "trigger_index": self.trigger_index,
            "created": self.created,
            "has_signal": self.has_signal,
            "warnings": list(self.warnings),
            "annotations": {str(slot): len(events)
                            for slot, events in self.annotations.items()},
            "bytes": len(self.data),
        }

    @classmethod
    def from_capture_data(cls, result, trigger_index: int | None = None,
                          name: str = "") -> "Capture":
        """Builds a Capture from ``AnalyzerClient.capture``'s CaptureData."""
        meta = result.meta
        return cls(
            encoding=meta["encoding"],
            sample_rate_hz=meta["sample_rate_hz"],
            data=result.data,
            total_words=meta["total_words"],
            trigger_index=trigger_index,
            name=name,
        )


class CaptureStore:
    """Bounded, thread-safe, session-memory capture store."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._captures: dict[str, Capture] = {}
        self._order: list[str] = []

    def add(self, capture: Capture) -> Capture:
        with self._lock:
            self._captures[capture.id] = capture
            self._order.append(capture.id)
        return capture

    def get(self, capture_id: str) -> Capture:
        with self._lock:
            capture = self._captures.get(capture_id)
        if capture is None:
            raise KeyError(f"unknown capture {capture_id}")
        return capture

    def list(self) -> list[dict]:
        with self._lock:
            return [self._captures[capture_id].info()
                    for capture_id in reversed(self._order)
                    if capture_id in self._captures]

    def delete(self, capture_id: str) -> bool:
        with self._lock:
            if capture_id not in self._captures:
                return False
            del self._captures[capture_id]
            self._order.remove(capture_id)
            return True

    def clear(self) -> None:
        with self._lock:
            self._captures.clear()
            self._order.clear()

    def __len__(self) -> int:
        with self._lock:
            return len(self._captures)
