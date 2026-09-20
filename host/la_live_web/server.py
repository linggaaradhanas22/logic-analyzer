"""HTTP + SSE server for the host live-web bridge (V02).

Standard library only. Routes (see ``docs/live-web.md``):

- ``GET /`` and ``/index.html`` -> the packaged live page;
- ``GET /events`` -> ``text/event-stream`` feed (single client, 409 when busy);
- ``GET /status`` -> JSON bridge snapshot;
- ``POST /control/start`` -> starts a live session (409 when one is running);
- ``POST /control/stop`` -> stops the running session (idempotent);
- anything else -> 404.
"""

from __future__ import annotations

import json
import threading
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from queue import Empty
from urllib.parse import parse_qs, urlsplit

from .core import HubFull, LiveEventHub

PAGE_PATH = Path(__file__).resolve().parent / "static" / "index.html"


class _LiveWebHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "la-live-web/1.0"

    def log_message(self, format: str, *args) -> None:  # noqa: A002
        pass  # keep the console clean; the CLI owns user-facing output

    def _send_bytes(self, status, content_type: str, body: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _send_json(self, status, payload: dict) -> None:
        body = json.dumps(payload, indent=2).encode("utf-8") + b"\n"
        self._send_bytes(status, "application/json", body)

    def do_GET(self) -> None:  # noqa: N802 (BaseHTTPRequestHandler API)
        path = urlsplit(self.path).path
        if path in ("/", "/index.html"):
            self._send_bytes(HTTPStatus.OK, "text/html; charset=utf-8",
                             self.server.page)
        elif path == "/status":
            snapshot = dict(self.server.hub.stats())
            snapshot["connected"] = snapshot["clients"] > 0
            controller = self.server.controller
            if controller is not None:
                snapshot["session"] = controller.state
                snapshot["sessions"] = controller.sessions
                if controller.error:
                    snapshot["error"] = controller.error
            self._send_json(HTTPStatus.OK, snapshot)
        elif path == "/events":
            self._serve_events()
        else:
            self._send_bytes(HTTPStatus.NOT_FOUND, "text/plain; charset=utf-8",
                             b"not found\n")

    def do_HEAD(self) -> None:  # noqa: N802
        self.do_GET()

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlsplit(self.path)
        path = parsed.path
        controller = self.server.controller
        if path not in ("/control/start", "/control/stop"):
            self._send_bytes(HTTPStatus.NOT_FOUND, "text/plain; charset=utf-8",
                             b"not found\n")
            return
        if controller is None:
            self._send_json(HTTPStatus.CONFLICT,
                            {"error": "this bridge has no session controller"})
            return
        query = parse_qs(parsed.query)
        slot_text = (query.get("slot") or [None])[0]
        slot: int | None = None
        if slot_text is not None:
            try:
                slot = int(slot_text)
            except ValueError:
                self._send_json(HTTPStatus.BAD_REQUEST,
                                {"error": f"invalid slot {slot_text!r}"})
                return
            if not 0 <= slot <= 2:
                self._send_json(HTTPStatus.BAD_REQUEST,
                                {"error": "slot must be 0..2"})
                return
        if path == "/control/start":
            if slot is None:
                controller.start_all()
                ok, message = True, "streaming"
            else:
                ok, message = controller.start_slot(slot)
            if not ok:
                self._send_json(HTTPStatus.CONFLICT,
                                {"error": message, "slot": slot,
                                 "slots": controller.slot_states()})
                return
            self._send_json(HTTPStatus.OK, {
                "slot": slot,
                "state": controller.state,
                "message": message,
                "slots": controller.slot_states(),
            })
            return
        if slot is None:
            stopped = controller.stop_all()
        else:
            stopped = controller.stop_slot(slot)
        self._send_json(HTTPStatus.OK, {
            "slot": slot,
            "state": controller.state,
            "stopped": stopped,
            "slots": controller.slot_states(),
        })

    def _serve_events(self) -> None:
        hub: LiveEventHub = self.server.hub
        try:
            feed = hub.attach()
        except HubFull as exc:
            self._send_bytes(HTTPStatus.CONFLICT, "text/plain; charset=utf-8",
                             (str(exc) + "\n").encode("utf-8"))
            return
        try:
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "text/event-stream; charset=utf-8")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "close")
            self.send_header("X-Accel-Buffering", "no")
            self.end_headers()
            self.close_connection = True
            # One immediate stats message so the page shows the real slot
            # state as soon as the feed connects (not up to a second later).
            try:
                self.wfile.write(
                    f"data: {hub.stats_message()}\n\n".encode("utf-8"))
                self.wfile.flush()
            except OSError:
                return
            while not self.server.stop_event.is_set():
                try:
                    message = feed.queue.get(timeout=self.server.stats_interval)
                except Empty:
                    message = hub.stats_message()
                try:
                    self.wfile.write(f"data: {message}\n\n".encode("utf-8"))
                    self.wfile.flush()
                except OSError:
                    break
        finally:
            hub.detach(feed)


class _LiveWebServer(ThreadingHTTPServer):
    daemon_threads = True
    # Windows SO_REUSEADDR lets a second process steal a bound port; the
    # single-owner contract needs a real bind failure instead.
    allow_reuse_address = False
    block_on_close = False

    def __init__(self, address, handler, hub: LiveEventHub, page: bytes,
                 stats_interval: float, controller=None) -> None:
        self.hub = hub
        self.page = page
        self.stats_interval = stats_interval
        self.controller = controller
        self.stop_event = threading.Event()
        super().__init__(address, handler)


class LiveWebServer:
    """Owns the HTTP server thread; ``start()`` returns the page URL."""

    def __init__(self, hub: LiveEventHub, host: str = "127.0.0.1",
                 port: int = 8080, stats_interval: float = 1.0,
                 page: bytes | None = None, controller=None) -> None:
        self.hub = hub
        self.host = host
        self.port = int(port)
        self.stats_interval = float(stats_interval)
        self.page = page if page is not None else PAGE_PATH.read_bytes()
        self.controller = controller
        self._server: _LiveWebServer | None = None
        self._thread: threading.Thread | None = None

    @property
    def url(self) -> str:
        host = self.host
        if ":" in host and not host.startswith("["):
            host = f"[{host}]"
        return f"http://{host}:{self.port}/"

    def start(self) -> str:
        if self._server is not None:
            raise RuntimeError("live web server already started")
        self._server = _LiveWebServer((self.host, self.port), _LiveWebHandler,
                                      self.hub, self.page, self.stats_interval,
                                      controller=self.controller)
        self.port = self._server.server_address[1]
        self._thread = threading.Thread(target=self._server.serve_forever,
                                        name="la-live-web", daemon=True)
        self._thread.start()
        return self.url

    def stop(self) -> None:
        if self._server is None:
            return
        self._server.stop_event.set()
        self._server.shutdown()
        self._server.server_close()
        if self._thread is not None:
            self._thread.join(timeout=5.0)
        self._server = None
        self._thread = None
