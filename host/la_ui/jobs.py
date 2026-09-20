"""Single-job background runner for the unified UI (W02).

The device can only do one long operation at a time, so the server runs at
most one job: a burst capture, a host decode, a trace dump or an export.
Each job reports state, progress, an error and a JSON result; cancellation is
cooperative through a ``threading.Event`` that the job function polls.
"""

from __future__ import annotations

import threading
import time
import uuid
from dataclasses import dataclass, field


class JobBusy(RuntimeError):
    """A job is already running; the caller must wait or cancel it."""


class JobCancelled(RuntimeError):
    """The job observed its cancellation event and stopped."""


@dataclass
class Job:
    id: str
    kind: str
    state: str = "queued"
    progress: float = 0.0
    message: str = ""
    error: str | None = None
    result: dict | None = None
    created: float = field(default_factory=time.time)
    started: float | None = None
    finished: float | None = None

    def info(self) -> dict:
        return {
            "id": self.id,
            "kind": self.kind,
            "state": self.state,
            "progress": round(self.progress, 4),
            "message": self.message,
            "error": self.error,
            "result": self.result,
            "created": self.created,
            "started": self.started,
            "finished": self.finished,
        }


class JobRunner:
    """One running job at a time; keeps the last ``history`` jobs."""

    def __init__(self, history: int = 50) -> None:
        self.history = int(history)
        self._lock = threading.Lock()
        self._jobs: dict[str, Job] = {}
        self._order: list[str] = []
        self._running: str | None = None
        self._cancel: dict[str, threading.Event] = {}
        self._thread: threading.Thread | None = None

    @property
    def busy(self) -> bool:
        with self._lock:
            return self._running is not None

    @property
    def running_id(self) -> str | None:
        with self._lock:
            return self._running

    def submit(self, kind: str, fn) -> Job:
        """Starts ``fn(cancel, progress)`` on a worker thread."""
        with self._lock:
            if self._running is not None:
                raise JobBusy(f"job {self._running} is already running")
            job = Job(id=uuid.uuid4().hex[:12], kind=kind)
            cancel = threading.Event()
            self._jobs[job.id] = job
            self._order.append(job.id)
            self._cancel[job.id] = cancel
            self._running = job.id
            while len(self._order) > self.history:
                stale = self._order.pop(0)
                self._jobs.pop(stale, None)
                self._cancel.pop(stale, None)
            thread = threading.Thread(target=self._run, args=(job, fn, cancel),
                                      name=f"la-ui-{kind}", daemon=True)
            self._thread = thread
        thread.start()
        return job

    def _run(self, job: Job, fn, cancel: threading.Event) -> None:
        job.state = "running"
        job.started = time.time()

        def progress(value: float, message: str = "") -> None:
            job.progress = max(0.0, min(1.0, float(value)))
            if message:
                job.message = message

        try:
            result = fn(cancel, progress)
            if cancel.is_set():
                job.state = "cancelled"
                job.error = "cancelled"
            else:
                job.state = "done"
                job.result = result if isinstance(result, dict) else {"value": result}
        except JobCancelled:
            job.state = "cancelled"
            job.error = "cancelled"
        except Exception as exc:  # noqa: BLE001 - reported through the API
            if cancel.is_set():
                job.state = "cancelled"
                job.error = str(exc)
            else:
                job.state = "error"
                job.error = str(exc)
        finally:
            job.finished = time.time()
            with self._lock:
                self._running = None

    def get(self, job_id: str) -> Job:
        with self._lock:
            job = self._jobs.get(job_id)
        if job is None:
            raise KeyError(f"unknown job {job_id}")
        return job

    def cancel(self, job_id: str) -> bool:
        with self._lock:
            cancel = self._cancel.get(job_id)
        if cancel is None:
            return False
        cancel.set()
        return True

    def cancel_all(self) -> None:
        with self._lock:
            events = list(self._cancel.values())
        for event in events:
            event.set()

    def wait_idle(self, timeout: float | None = None) -> bool:
        with self._lock:
            thread = self._thread
        if thread is not None:
            thread.join(timeout)
        return not self.busy
