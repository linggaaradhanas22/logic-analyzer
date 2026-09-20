"""HTTP API and workspace server for the unified UI (W02).

Standard library only. The server extends the V01-V09 host bridge contract
(`docs/live-web.md`): the SSE feed, `/status` and `/control/start|stop` keep
their behavior and the legacy three-pane page stays available at `/legacy`.
New `/api/*` routes expose connection, capture jobs, decimation, host decode,
exports, trace, pattern generator and offline open.

One process owns the analyzer target. Long device operations run through the
single-slot `JobRunner`; capture and live are mutually exclusive and the
server stops the other mode automatically (auto-stop policy).
"""

from __future__ import annotations

import json
import re
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from queue import Empty
from urllib.parse import parse_qs, unquote, urlsplit

from ..la_cli import protocol as p
from ..la_cli import trace as trace_mod
from ..la_cli.client import AnalyzerClient, AnalyzerError
from ..la_cli.commands import FS_HZ, PATGEN_MODES
from ..la_cli.transport import TransportError, open_uri
from ..la_live_web import LiveEventHub, LiveSessionController
from ..la_live_web.server import PAGE_PATH as LEGACY_PAGE_PATH
from .decode import NativeDecoders, parse_decoder_spec
from .export import EXPORTS
from .jobs import JobBusy, JobCancelled, JobRunner
from .open import OpenError, open_capture
from .render import DETAIL_MAX_SAMPLES, decode_window, encode_envelope
from .store import Capture, CaptureStore

UI_VERSION = "w10"
PAGE_DIR = Path(__file__).resolve().parent / "static"
STATIC_FILES = {
    "index.html": "text/html; charset=utf-8",
    "app.js": "text/javascript; charset=utf-8",
    "waveform.js": "text/javascript; charset=utf-8",
    "timeline.js": "text/javascript; charset=utf-8",
    "style.css": "text/css; charset=utf-8",
}
MAX_BODY = 64 * 1024 * 1024
DEFAULT_CAPTURE_TIMEOUT = 30.0


class ApiError(RuntimeError):
    """An API-level failure with an HTTP status."""

    def __init__(self, status: int, message: str) -> None:
        super().__init__(message)
        self.status = int(status)
        self.message = str(message)


def parse_size(text: str) -> int:
    """Parses sizes like 4096, 64k or 1M (SI decimal suffixes)."""
    match = re.fullmatch(r"(\d+)([kKmMgG]?)", str(text).strip())
    if not match:
        raise ValueError(f"invalid size {text!r}; use e.g. 4096, 64k or 1M")
    value = int(match.group(1))
    suffix = match.group(2).lower()
    if suffix == "k":
        value *= 1000
    elif suffix == "m":
        value *= 1_000_000
    elif suffix == "g":
        value *= 1_000_000_000
    return value


def sample_div(rate_hz: int) -> int:
    if rate_hz <= 0 or rate_hz > FS_HZ:
        raise ValueError(f"rate must be within 1 Hz .. {FS_HZ} Hz")
    return max(0, round(FS_HZ / rate_hz) - 1)


def pack_slot_spec(spec: str, slot: int, sample_rate_hz: int) -> bytes:
    """Packs one decoder spec into a DECODER_CFG payload (CLI grammar)."""
    params = parse_decoder_spec(spec)
    if params["type"] == "uart":
        return p.pack_decoder_cfg_uart(slot, params["rx"], params["baud"],
                                       sample_rate_hz)
    if params["type"] == "spi":
        return p.pack_decoder_cfg_spi(
            slot, params["sck"], params["mosi"], params["miso"], params["cs"],
            params["cpol"], params["cpha"], params["word"], params["lsb"],
            params["cs_active"])
    return p.pack_decoder_cfg_i2c(slot, params["scl"], params["sda"])


def _optional_channel(value, label: str) -> int | None:
    if value is None or value == "":
        return None
    try:
        channel = int(value)
    except (TypeError, ValueError) as exc:
        raise ApiError(400, f"{label} must be a channel 0..7") from exc
    if not 0 <= channel <= 7:
        raise ApiError(400, f"{label} must be a channel 0..7")
    return channel


class UiState:
    """Connection, store, jobs and live session for one UI server."""

    def __init__(self, decoder_specs=(), rate: str = "100M",
                 patgen: str = "off") -> None:
        self.lock = threading.RLock()
        self.device_lock = threading.RLock()
        self.client: AnalyzerClient | None = None
        self.target: str | None = None
        self.pong: dict | None = None
        self.store = CaptureStore()
        self.jobs = JobRunner()
        self.hub = LiveEventHub()
        self.controller: LiveSessionController | None = None
        self.rate_text = str(rate)
        self.rate_hz = FS_HZ // (sample_div(parse_size(rate)) + 1)
        self.patgen_mode = str(patgen)
        if self.patgen_mode != "off" and self.patgen_mode not in PATGEN_MODES:
            raise ValueError(f"unknown patgen mode {patgen!r}")
        specs = [str(spec or "").strip() for spec in decoder_specs][:3]
        while len(specs) < 3:
            specs.append("")
        for spec in specs:
            if spec:
                pack_slot_spec(spec, 0, self.rate_hz)  # validate
        self.decoder_specs = specs
        self.trace_model: dict | None = None
        self.trace_active = False
        self._build_controller_locked()

    # -- connection ---------------------------------------------------------

    def connect(self, target: str) -> dict:
        target = str(target or "").strip()
        if not target:
            raise ApiError(400, "target is required (serial://COMx)")
        with self.lock:
            if self.client is not None:
                raise ApiError(409, f"already connected to {self.target}")
        try:
            transport = open_uri(target)
        except TransportError as exc:
            raise ApiError(400, str(exc)) from exc
        client = AnalyzerClient(transport)
        try:
            pong = client.ping(timeout_s=5.0)
        except (AnalyzerError, TransportError) as exc:
            client.close()
            raise ApiError(502, str(exc)) from exc
        with self.lock:
            self.client = client
            self.target = target
            self.pong = pong
            self.hub.target = target
            self._build_controller_locked()
        return self.connection_info()

    def disconnect(self) -> dict:
        with self.lock:
            controller = self.controller
        if controller is not None and controller.state != "idle":
            controller.stop_all()
        with self.lock:
            client = self.client
            self.client = None
            self.target = None
            self.pong = None
            self.controller = None
            self.hub.target = ""
            self.hub.state = "idle"
        if client is not None:
            client.close()
        return {"connected": False}

    def shutdown(self) -> None:
        try:
            self.disconnect()
        except Exception:  # noqa: BLE001 - shutdown must not raise
            pass

    def require_client(self) -> AnalyzerClient:
        with self.lock:
            if self.client is None:
                raise ApiError(409, "not connected; open a port first")
            return self.client

    def require_controller(self) -> LiveSessionController:
        with self.lock:
            if self.controller is None:
                raise ApiError(409, "not connected; open a port first")
            return self.controller

    def _require_idle(self, what: str) -> None:
        controller = self.controller
        if controller is not None and controller.state != "idle":
            raise ApiError(409, f"stop the live session before {what}")

    def _build_controller_locked(self) -> None:
        slot_configs: dict[int, bytes] = {}
        for slot, spec in enumerate(self.decoder_specs):
            if spec:
                slot_configs[slot] = pack_slot_spec(spec, slot, self.rate_hz)
        stream_config = p.pack_config(
            ctrl_flags=p.CTRL_STREAM,
            sample_div=sample_div(parse_size(self.rate_text)),
            mode=1,
            chan_en=0xFF,
            post_samples=0,
            txn_max_run=0x00FFFFFF,
            live_flush_cycles=200_000,
        )
        self.slot_configs = slot_configs
        self.stream_config = stream_config
        self.hub.slot_labels = list(self.decoder_specs)
        self.hub.decoder = " | ".join(
            f"slot {slot}: {spec}" for slot, spec in enumerate(self.decoder_specs)
            if spec)
        if self.client is not None:
            self.controller = LiveSessionController(
                self.client, self.hub, slot_configs, stream_config,
                prepare=self._live_prepare, stop_on_overrun=False)

    def connection_info(self) -> dict:
        with self.lock:
            controller = self.controller
            return {
                "connected": self.client is not None,
                "target": self.target,
                "pong": self.pong,
                "rate": self.rate_text,
                "rate_hz": self.rate_hz,
                "patgen": self.patgen_mode,
                "decoder_specs": list(self.decoder_specs),
                "slot_labels": list(self.hub.slot_labels),
                "session": controller.state if controller is not None else "idle",
                "sessions": controller.sessions if controller is not None else 0,
                "slots": (controller.slot_states() if controller is not None
                          else ["unconfigured"] * 3),
                "version": UI_VERSION,
            }

    def state_snapshot(self) -> dict:
        snapshot = self.connection_info()
        job = None
        running = self.jobs.running_id
        if running is not None:
            job = self.jobs.get(running).info()
        snapshot["captures"] = self.store.list()
        snapshot["job"] = job
        snapshot["trace_ready"] = self.trace_model is not None
        snapshot["trace_active"] = self.trace_active
        snapshot["stats"] = self.hub.stats()
        return snapshot

    # -- live ---------------------------------------------------------------

    def configure_live(self, specs) -> dict:
        specs = [str(spec or "").strip() for spec in (specs or [])][:3]
        while len(specs) < 3:
            specs.append("")
        for spec in specs:
            if spec:
                try:
                    pack_slot_spec(spec, 0, self.rate_hz)
                except ValueError as exc:
                    raise ApiError(400, f"bad decoder spec {spec!r}: {exc}") from exc
        with self.lock:
            if self.client is None:
                raise ApiError(409, "not connected; open a port first")
            if self.controller is not None and self.controller.state != "idle":
                raise ApiError(409, "stop the live session before changing decoders")
            self.decoder_specs = specs
            self._build_controller_locked()
            return {"decoder_specs": list(self.decoder_specs),
                    "slot_labels": list(self.hub.slot_labels)}

    def start_slot(self, slot: int) -> dict:
        if self.jobs.busy:
            self.jobs.cancel_all()
            if not self.jobs.wait_idle(timeout=6.0):
                raise ApiError(409, "a capture is still running; cancel it first")
        with self.device_lock:
            controller = self.require_controller()
            ok, message = controller.start_slot(slot)
            if not ok:
                raise ApiError(409, message)
            return {"slot": slot, "state": controller.state,
                    "message": message, "slots": controller.slot_states()}

    def stop_slot(self, slot: int) -> dict:
        with self.device_lock:
            controller = self.require_controller()
            stopped = controller.stop_slot(slot)
            return {"slot": slot, "state": controller.state,
                    "stopped": stopped, "slots": controller.slot_states()}

    def stop_all_slots(self) -> dict:
        with self.device_lock:
            controller = self.require_controller()
            stopped = controller.stop_all()
            return {"slot": None, "state": controller.state, "stopped": stopped,
                    "slots": controller.slot_states()}

    def start_all_slots(self) -> dict:
        if self.jobs.busy:
            self.jobs.cancel_all()
            if not self.jobs.wait_idle(timeout=6.0):
                raise ApiError(409, "a capture is still running; cancel it first")
        with self.device_lock:
            controller = self.require_controller()
            if not controller.start_all():
                raise ApiError(409, "no decoder is configured")
            return {"slot": None, "state": controller.state,
                    "message": "streaming", "slots": controller.slot_states()}

    def _live_prepare(self) -> None:
        if self.patgen_mode != "off":
            self._configure_patgen(self.patgen_mode)

    # -- pattern generator ---------------------------------------------------

    def _configure_patgen(self, mode: str) -> None:
        if mode == "off":
            return
        client = self.require_client()
        # la_patgen latches mode/DIV on the EN rising edge; clear EN first so
        # a repeated session relatches instead of keeping the previous mode.
        client.configure_patgen(0)
        client.configure_patgen(p.PATGEN_EN_MASK |
                                (PATGEN_MODES[mode] << 4))

    def set_patgen(self, mode: str) -> dict:
        mode = str(mode or "off").strip().lower()
        if mode != "off" and mode not in PATGEN_MODES:
            raise ApiError(400, f"unknown patgen mode {mode!r}")
        with self.device_lock:
            self.require_client()
            self._require_idle("changing the pattern generator")
            if mode != "off":
                self._configure_patgen(mode)
            self.patgen_mode = mode
        return {"patgen": self.patgen_mode}

    # -- capture -------------------------------------------------------------

    def build_capture_config(self, options: dict) -> tuple[bytes, dict]:
        try:
            rate_hz = parse_size(options.get("rate", "10M"))
            div = sample_div(rate_hz)
        except ValueError as exc:
            raise ApiError(400, str(exc)) from exc
        try:
            words = parse_size(options.get("words", "64k"))
        except ValueError as exc:
            raise ApiError(400, str(exc)) from exc
        if words <= 0:
            raise ApiError(400, "words must be positive")
        mode = str(options.get("mode", "raw")).strip().lower()
        if mode not in ("raw", "txn"):
            raise ApiError(400, "mode must be raw or txn")
        try:
            pre = int(options.get("pre") or 0)
        except (TypeError, ValueError) as exc:
            raise ApiError(400, "pre must be a number") from exc
        if not 0 <= pre <= 32768:
            raise ApiError(400, "pre must be within 0..32768")
        trig_rise = _optional_channel(options.get("trig_rise"), "trig_rise")
        trig_fall = _optional_channel(options.get("trig_fall"), "trig_fall")
        trig_mask = 0
        if trig_rise is not None:
            trig_mask |= 1 << trig_rise
        if trig_fall is not None:
            trig_mask |= 1 << trig_fall
        patgen = str(options.get("patgen", "off")).strip().lower()
        if patgen != "off" and patgen not in PATGEN_MODES:
            raise ApiError(400, f"unknown patgen mode {patgen!r}")
        config = p.pack_config(
            ctrl_flags=p.CTRL_SELFTEST_EN if patgen != "off" else 0,
            sample_div=div,
            mode={"raw": 0, "txn": 1}[mode],
            chan_en=0xFF,
            trig_mask=trig_mask,
            trig_rise=1 << trig_rise if trig_rise is not None else 0,
            trig_fall=1 << trig_fall if trig_fall is not None else 0,
            pre_samples=pre,
            post_samples=words,
            timeout=0,
        )
        meta = {
            "rate_hz": FS_HZ // (div + 1),
            "sample_div": div,
            "mode": mode,
            "words": words,
            "pre": pre,
            "trig_rise": trig_rise,
            "trig_fall": trig_fall,
            "patgen": patgen,
        }
        return config, meta

    def capture(self, options: dict, cancel: threading.Event, progress) -> dict:
        options = dict(options or {})
        config, meta = self.build_capture_config(options)
        try:
            timeout_s = float(options.get("timeout") or DEFAULT_CAPTURE_TIMEOUT)
        except (TypeError, ValueError) as exc:
            raise ApiError(400, "timeout must be a number") from exc
        if not 1.0 <= timeout_s <= 600.0:
            raise ApiError(400, "timeout must be within 1..600 seconds")
        name = str(options.get("name") or "").strip()
        with self.device_lock:
            client = self.require_client()
            self._stop_live_locked()
            if meta["patgen"] != "off":
                self._configure_patgen(meta["patgen"])
            progress(0.05, "arming")
            try:
                result = client.capture(config, timeout_s=timeout_s)
            except (AnalyzerError, TransportError) as exc:
                raise ApiError(502, f"capture failed: {exc}") from exc
            if cancel.is_set():
                raise JobCancelled()
            progress(0.9, "reading trigger index")
            trigger = None
            try:
                trigger = int(client.status(timeout_s=5.0)["trig_index"])
            except Exception:  # noqa: BLE001 - trigger marker is optional
                trigger = None
            capture = Capture.from_capture_data(
                result, trigger_index=trigger,
                name=name or f"capture {int(time.time())}")
            self.store.add(capture)
            progress(1.0, "captured")
            return {"capture": capture.info(), "meta": meta,
                    "parser": result.parser_stats}

    def _stop_live_locked(self) -> None:
        controller = self.controller
        if controller is not None and controller.state != "idle":
            if not controller.stop_all():
                raise ApiError(409, "the live session did not stop in time")

    def cancel_job(self, job_id: str) -> bool:
        job = self.jobs.get(job_id)
        self.jobs.cancel(job_id)
        if job.kind == "capture" and job.state == "running":
            # Cooperative: ABORT the device so a stuck transfer ends early.
            try:
                client = self.client
                if client is not None:
                    client.abort(timeout_s=2.0)
            except Exception:  # noqa: BLE001 - best effort
                pass
        return True

    # -- host decode ---------------------------------------------------------

    def decode(self, capture_id: str, specs, cancel: threading.Event,
               progress) -> dict:
        capture = self.store.get(capture_id)
        specs = [str(spec or "").strip() for spec in (specs or [])][:3]
        while len(specs) < 3:
            specs.append("")
        if not any(specs):
            raise ApiError(400, "at least one decoder spec is required")
        for spec in specs:
            if spec:
                try:
                    parse_decoder_spec(spec)
                except ValueError as exc:
                    raise ApiError(400, f"bad decoder spec {spec!r}: {exc}") from exc

        def job_progress(value: float, message: str = "") -> None:
            if cancel.is_set():
                raise JobCancelled()
            progress(value, message)

        engine = NativeDecoders()
        annotations = engine.decode_capture(capture, specs, progress=job_progress)
        capture.annotations = annotations
        counts = {str(slot): len(events) for slot, events in annotations.items()}
        return {"capture": capture.info(), "counts": counts}

    # -- trace ---------------------------------------------------------------

    def trace_start(self) -> dict:
        with self.device_lock:
            client = self.require_client()
            self._require_idle("using the trace ring")
            try:
                client.trace_control(p.TRACE_START, timeout_s=10.0)
            except (AnalyzerError, TransportError) as exc:
                raise ApiError(502, str(exc)) from exc
            self.trace_active = True
        return {"trace_active": True}

    def trace_stop(self) -> dict:
        with self.device_lock:
            client = self.require_client()
            self._require_idle("using the trace ring")
            try:
                client.trace_control(p.TRACE_STOP, timeout_s=10.0)
            except (AnalyzerError, TransportError) as exc:
                raise ApiError(502, str(exc)) from exc
            self.trace_active = False
        return {"trace_active": False}

    def trace_dump(self, cancel: threading.Event, progress) -> dict:
        with self.device_lock:
            client = self.require_client()
            self._require_idle("using the trace ring")
            progress(0.1, "dumping the trace ring")
            try:
                lines = client.trace_dump(timeout_s=30.0)
            except (AnalyzerError, TransportError) as exc:
                raise ApiError(502, str(exc)) from exc
            if cancel.is_set():
                raise JobCancelled()
            self.trace_active = False
            progress(0.8, "parsing the trace")
            try:
                dump = trace_mod.parse_trace(lines)
            except trace_mod.TraceParseError as exc:
                raise ApiError(502, f"trace parse failed: {exc}") from exc
            self.trace_model = trace_mod.timeline_model(dump)
            progress(1.0, "trace ready")
            return {"model": self.trace_model, "lines": len(lines)}

    # -- offline open --------------------------------------------------------

    def open_file(self, name: str, data: bytes) -> dict:
        try:
            capture = open_capture(name, data)
        except OpenError as exc:
            raise ApiError(400, str(exc)) from exc
        self.store.add(capture)
        return {"capture": capture.info()}

    # -- live status ---------------------------------------------------------

    def ping(self) -> dict:
        with self.device_lock:
            client = self.require_client()
            self._require_idle("pinging")
            try:
                pong = client.ping(timeout_s=5.0)
            except (AnalyzerError, TransportError) as exc:
                raise ApiError(502, str(exc)) from exc
            self.pong = pong
        return {"pong": self.pong}

    def device_status(self) -> dict:
        with self.device_lock:
            client = self.require_client()
            self._require_idle("reading status")
            try:
                result = client.status(timeout_s=5.0)
                logs = client.log_lines[-20:]
            except (AnalyzerError, TransportError) as exc:
                raise ApiError(502, str(exc)) from exc
        return {"status": result, "logs": logs}


def list_ports() -> list[dict]:
    """Lists serial ports, flagging likely analyzer/gateway identities."""
    import serial.tools.list_ports

    ports = []
    for info in serial.tools.list_ports.comports():
        vid_pid = (f"{info.vid:04X}:{info.pid:04X}"
                   if info.vid is not None and info.pid is not None else "")
        likely = info.vid in (0x1209, 0x303A, 0x10C4, 0x1A86, 0x0403)
        ports.append({
            "device": info.device,
            "description": info.description or "",
            "vid_pid": vid_pid,
            "serial": info.serial_number or "",
            "analyzer": bool(likely),
        })
    return ports


class _UiHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = f"la-ui/{UI_VERSION}"

    def log_message(self, format: str, *args) -> None:  # noqa: A002
        pass  # the CLI owns user-facing output

    # -- response helpers ----------------------------------------------------

    def _send_bytes(self, status, content_type: str, body: bytes,
                    disposition: str | None = None) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        if disposition:
            self.send_header("Content-Disposition", disposition)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _send_json(self, status, payload) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8") + b"\n"
        self._send_bytes(status, "application/json; charset=utf-8", body)

    def _read_body(self) -> bytes:
        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0:
            return b""
        if length > MAX_BODY:
            raise ApiError(413, f"request body too large ({length} bytes)")
        return self.rfile.read(length)

    def _read_json(self) -> dict:
        body = self._read_body()
        if not body:
            return {}
        try:
            payload = json.loads(body.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ApiError(400, "invalid JSON body") from exc
        if not isinstance(payload, dict):
            raise ApiError(400, "JSON body must be an object")
        return payload

    @property
    def state(self) -> UiState:
        return self.server.state

    # -- routing -------------------------------------------------------------

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlsplit(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)
        try:
            self._route_get(path, query)
        except ApiError as exc:
            self._send_json(exc.status, {"error": exc.message})
        except KeyError as exc:
            self._send_json(HTTPStatus.NOT_FOUND, {"error": str(exc)})
        except (AnalyzerError, TransportError) as exc:
            self._send_json(HTTPStatus.BAD_GATEWAY, {"error": str(exc)})
        except Exception as exc:  # noqa: BLE001 - reported, never silent
            self._send_json(HTTPStatus.INTERNAL_SERVER_ERROR,
                            {"error": f"{type(exc).__name__}: {exc}"})

    def do_HEAD(self) -> None:  # noqa: N802
        self.do_GET()

    def do_DELETE(self) -> None:  # noqa: N802
        parsed = urlsplit(self.path)
        try:
            match = re.fullmatch(r"/api/captures/([0-9a-f]+)", parsed.path)
            if not match:
                raise ApiError(HTTPStatus.NOT_FOUND,
                               f"not found: {parsed.path}")
            deleted = self.state.store.delete(match.group(1))
            if not deleted:
                raise ApiError(HTTPStatus.NOT_FOUND,
                               f"unknown capture {match.group(1)}")
            self._send_json(HTTPStatus.OK, {"deleted": True})
        except ApiError as exc:
            self._send_json(exc.status, {"error": exc.message})

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlsplit(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)
        try:
            self._route_post(path, query)
        except ApiError as exc:
            self._send_json(exc.status, {"error": exc.message})
        except JobBusy as exc:
            self._send_json(HTTPStatus.CONFLICT, {"error": str(exc)})
        except KeyError as exc:
            self._send_json(HTTPStatus.NOT_FOUND, {"error": str(exc)})
        except (AnalyzerError, TransportError) as exc:
            self._send_json(HTTPStatus.BAD_GATEWAY, {"error": str(exc)})
        except Exception as exc:  # noqa: BLE001
            self._send_json(HTTPStatus.INTERNAL_SERVER_ERROR,
                            {"error": f"{type(exc).__name__}: {exc}"})

    def _route_get(self, path: str, query: dict) -> None:
        state = self.state
        if path in ("/", "/index.html"):
            self._send_bytes(HTTPStatus.OK, "text/html; charset=utf-8",
                             self.server.page)
            return
        if path == "/legacy":
            self._send_bytes(HTTPStatus.OK, "text/html; charset=utf-8",
                             self.server.legacy_page)
            return
        if path.startswith("/static/"):
            name = unquote(path[len("/static/"):])
            content_type = STATIC_FILES.get(name)
            if content_type is None:
                raise ApiError(HTTPStatus.NOT_FOUND, f"unknown asset {name!r}")
            asset = PAGE_DIR / name
            if not asset.is_file():
                raise ApiError(HTTPStatus.NOT_FOUND, f"missing asset {name!r}")
            self._send_bytes(HTTPStatus.OK, content_type, asset.read_bytes())
            return
        if path == "/events":
            self._serve_events()
            return
        if path == "/status":
            snapshot = dict(state.hub.stats())
            snapshot["connected"] = snapshot["clients"] > 0
            controller = state.controller
            snapshot["session"] = controller.state if controller is not None else "idle"
            snapshot["sessions"] = controller.sessions if controller is not None else 0
            if controller is not None and controller.error:
                snapshot["error"] = controller.error
            snapshot["ui"] = {"target": state.target,
                              "captures": len(state.store),
                              "version": UI_VERSION}
            self._send_json(HTTPStatus.OK, snapshot)
            return
        if path == "/api/state":
            self._send_json(HTTPStatus.OK, state.state_snapshot())
            return
        if path == "/api/ports":
            self._send_json(HTTPStatus.OK, {"ports": list_ports()})
            return
        if path == "/api/ping":
            self._send_json(HTTPStatus.OK, state.ping())
            return
        if path == "/api/status":
            self._send_json(HTTPStatus.OK, state.device_status())
            return
        if path == "/api/captures":
            self._send_json(HTTPStatus.OK, {"captures": state.store.list()})
            return
        if path == "/api/trace":
            if state.trace_model is None:
                raise ApiError(HTTPStatus.NOT_FOUND, "no trace dump yet")
            self._send_json(HTTPStatus.OK, {"model": state.trace_model})
            return
        match = re.fullmatch(r"/api/jobs/([0-9a-f]+)", path)
        if match:
            self._send_json(HTTPStatus.OK, state.jobs.get(match.group(1)).info())
            return
        match = re.fullmatch(r"/api/captures/([0-9a-f]+)/(\w[\w-]*)", path)
        if match:
            self._route_capture_get(match.group(1), match.group(2), query)
            return
        raise ApiError(HTTPStatus.NOT_FOUND, f"not found: {path}")

    def _route_capture_get(self, capture_id: str, action: str,
                           query: dict) -> None:
        state = self.state
        capture = state.store.get(capture_id)
        if action == "meta":
            self._send_json(HTTPStatus.OK, capture.info())
            return
        if action == "annotations":
            self._send_json(HTTPStatus.OK,
                            {"annotations": capture.annotations})
            return
        if action == "envelope":
            try:
                buckets = int((query.get("buckets") or ["1024"])[0])
            except ValueError as exc:
                raise ApiError(400, "buckets must be an integer") from exc
            if not 1 <= buckets <= 1_000_000:
                raise ApiError(400, "buckets must be within 1..1000000")
            _actual, blob = encode_envelope(capture, buckets)
            self._send_bytes(HTTPStatus.OK, "application/octet-stream", blob)
            return
        if action == "samples":
            try:
                start = int((query.get("start") or ["0"])[0])
                count = int((query.get("count") or [str(DETAIL_MAX_SAMPLES)])[0])
            except ValueError as exc:
                raise ApiError(400, "start/count must be integers") from exc
            if not 0 <= count <= DETAIL_MAX_SAMPLES:
                raise ApiError(400,
                               f"count must be within 0..{DETAIL_MAX_SAMPLES}")
            self._send_bytes(HTTPStatus.OK, "application/octet-stream",
                             decode_window(capture, start, count))
            return
        if action == "export":
            fmt = (query.get("format") or [""])[0]
            entry = EXPORTS.get(fmt)
            if entry is None:
                raise ApiError(400, f"unknown format {fmt!r}; use "
                                    f"{', '.join(sorted(EXPORTS))}")
            filename, content_type, writer = entry
            body = writer(capture)
            stem = re.sub(r"[^A-Za-z0-9._-]+", "-", capture.name) or "capture"
            disposition = f'attachment; filename="{stem}-{filename}"'
            self._send_bytes(HTTPStatus.OK, content_type, body,
                             disposition=disposition)
            return
        raise ApiError(HTTPStatus.NOT_FOUND, f"unknown capture action {action!r}")

    def _route_post(self, path: str, query: dict) -> None:
        state = self.state
        if path in ("/control/start", "/control/stop"):
            self._route_control(path, query)
            return
        if path == "/api/connect":
            payload = self._read_json()
            self._send_json(HTTPStatus.OK,
                            state.connect(payload.get("target", "")))
            return
        if path == "/api/disconnect":
            self._read_body()
            self._send_json(HTTPStatus.OK, state.disconnect())
            return
        if path == "/api/live/config":
            payload = self._read_json()
            self._send_json(HTTPStatus.OK,
                            state.configure_live(payload.get("specs") or []))
            return
        if path == "/api/patgen":
            payload = self._read_json()
            self._send_json(HTTPStatus.OK,
                            state.set_patgen(payload.get("mode", "off")))
            return
        if path == "/api/capture":
            payload = self._read_json()
            state.require_client()
            job = state.jobs.submit(
                "capture", lambda cancel, progress:
                state.capture(payload, cancel, progress))
            self._send_json(HTTPStatus.OK, {"job": job.info()})
            return
        if path == "/api/open":
            name = (query.get("name") or [""])[0] or "upload.bin"
            data = self._read_body()
            if not data:
                raise ApiError(400, "upload body is empty")
            self._send_json(HTTPStatus.OK, state.open_file(name, data))
            return
        if path == "/api/trace/start":
            self._read_body()
            self._send_json(HTTPStatus.OK, state.trace_start())
            return
        if path == "/api/trace/stop":
            self._read_body()
            self._send_json(HTTPStatus.OK, state.trace_stop())
            return
        if path == "/api/trace/dump":
            self._read_body()
            job = state.jobs.submit(
                "trace", lambda cancel, progress:
                state.trace_dump(cancel, progress))
            self._send_json(HTTPStatus.OK, {"job": job.info()})
            return
        match = re.fullmatch(r"/api/jobs/([0-9a-f]+)/cancel", path)
        if match:
            self._read_body()
            state.cancel_job(match.group(1))
            self._send_json(HTTPStatus.OK,
                            {"job": state.jobs.get(match.group(1)).info()})
            return
        match = re.fullmatch(r"/api/captures/([0-9a-f]+)/decode", path)
        if match:
            payload = self._read_json()
            capture_id = match.group(1)
            state.store.get(capture_id)
            specs = payload.get("specs") or []
            if not any(str(spec or "").strip() for spec in specs):
                raise ApiError(400, "at least one decoder spec is required")
            job = state.jobs.submit(
                "decode", lambda cancel, progress:
                state.decode(capture_id, specs, cancel, progress))
            self._send_json(HTTPStatus.OK, {"job": job.info()})
            return
        raise ApiError(HTTPStatus.NOT_FOUND, f"not found: {path}")

    def _route_control(self, path: str, query: dict) -> None:
        state = self.state
        if state.controller is None:
            self._send_json(HTTPStatus.CONFLICT,
                            {"error": "connect to the analyzer first"})
            return
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
        try:
            if path == "/control/start":
                result = (state.start_all_slots() if slot is None
                          else state.start_slot(slot))
            else:
                result = (state.stop_all_slots() if slot is None
                          else state.stop_slot(slot))
        except ApiError as exc:
            self._send_json(exc.status, {"error": exc.message, "slot": slot})
            return
        self._send_json(HTTPStatus.OK, result)

    # -- SSE feed (same contract as the V02 bridge) ---------------------------

    def _serve_events(self) -> None:
        hub: LiveEventHub = self.server.state.hub
        try:
            feed = hub.attach()
        except Exception as exc:  # noqa: BLE001 - HubFull
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


class _UiHttpServer(ThreadingHTTPServer):
    daemon_threads = True
    # Windows SO_REUSEADDR lets a second process steal a bound port; the
    # single-owner contract needs a real bind failure instead.
    allow_reuse_address = False
    block_on_close = False

    def __init__(self, address, handler, state: UiState, page: bytes,
                 legacy_page: bytes, stats_interval: float) -> None:
        self.state = state
        self.page = page
        self.legacy_page = legacy_page
        self.stats_interval = stats_interval
        self.stop_event = threading.Event()
        super().__init__(address, handler)


class UiServer:
    """Owns the HTTP server thread; ``start()`` returns the page URL."""

    def __init__(self, state: UiState, host: str = "127.0.0.1",
                 port: int = 8080, stats_interval: float = 1.0,
                 page: bytes | None = None,
                 legacy_page: bytes | None = None) -> None:
        self.state = state
        self.host = host
        self.port = int(port)
        self.stats_interval = float(stats_interval)
        self.page = page if page is not None else (PAGE_DIR / "index.html").read_bytes()
        self.legacy_page = (legacy_page if legacy_page is not None
                            else LEGACY_PAGE_PATH.read_bytes())
        self._server: _UiHttpServer | None = None
        self._thread: threading.Thread | None = None

    @property
    def url(self) -> str:
        host = self.host
        if ":" in host and not host.startswith("["):
            host = f"[{host}]"
        return f"http://{host}:{self.port}/"

    def start(self) -> str:
        if self._server is not None:
            raise RuntimeError("UI server already started")
        self._server = _UiHttpServer((self.host, self.port), _UiHandler,
                                     self.state, self.page, self.legacy_page,
                                     self.stats_interval)
        self.port = self._server.server_address[1]
        self._thread = threading.Thread(target=self._server.serve_forever,
                                        name="la-ui", daemon=True)
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
