"""W09 unified UI end-to-end gates over the direct-USB device.

Run against the USB-profile Zynq (patgen on CH0 for the UART gates):

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- \\
        --target serial://COM13 -k ui

The real ``UiServer`` API is driven end to end: capture with the envelope and
detail endpoints, host decode of a patgen capture, per-slot live control with
a real SSE reader, the trace dump, exports and offline reopen, a 1M-sample
capture, and the capture/live auto-stop policy. The analyzer client verifies
the transfer byte count and CRC32 on every capture.
"""

from __future__ import annotations

import http.client
import json
import threading
import time

import pytest

from host.la_ui.server import UiServer, UiState

PATGEN_UART = "uart"


@pytest.fixture(scope="module")
def target(request) -> str:
    value = request.config.getoption("--target")
    if not value:
        pytest.skip("no --target: the UI bench gates require hardware")
    return value


@pytest.fixture(scope="module")
def ui(target):
    state = UiState(decoder_specs=("uart:rx=0,baud=115200", "", ""), rate="100M")
    server = UiServer(state, port=0)
    server.start()
    state.connect(target)
    yield state, server
    state.shutdown()
    server.stop()


def _request(server, method, path, body=None, timeout=120.0):
    connection = http.client.HTTPConnection("127.0.0.1", server.port,
                                            timeout=timeout)
    payload = None
    headers = {}
    if body is not None:
        if isinstance(body, (bytes, bytearray)):
            payload = bytes(body)
            headers["Content-Type"] = "application/octet-stream"
        else:
            payload = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
    connection.request(method, path, payload, headers)
    response = connection.getresponse()
    data = response.read()
    status = response.status
    connection.close()
    return status, data


def _json(server, method, path, body=None):
    status, data = _request(server, method, path, body)
    return status, json.loads(data.decode("utf-8"))


def _wait_job(server, job_id, timeout=120.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        status, payload = _json(server, "GET", f"/api/jobs/{job_id}")
        assert status == 200
        if payload["state"] in ("done", "error", "cancelled"):
            return payload
        time.sleep(0.1)
    raise AssertionError(f"job {job_id} did not finish in {timeout}s")


def _capture(server, **options):
    body = {"rate": "10M", "words": "64k", "mode": "raw", "timeout": 60}
    body.update(options)
    status, payload = _json(server, "POST", "/api/capture", body)
    assert status == 200, payload
    job = _wait_job(server, payload["job"]["id"])
    assert job["state"] == "done", job
    return job["result"]["capture"]


class _SseReader(threading.Thread):
    def __init__(self, port: int) -> None:
        super().__init__(name="ui-e2e-sse", daemon=True)
        self.port = port
        self.messages: list[dict] = []
        self.error: Exception | None = None
        self._stop = threading.Event()

    def run(self) -> None:
        try:
            connection = http.client.HTTPConnection("127.0.0.1", self.port,
                                                    timeout=10)
            connection.request("GET", "/events")
            response = connection.getresponse()
            while not self._stop.is_set():
                line = response.readline()
                if not line:
                    break
                text = line.decode("utf-8").strip()
                if text.startswith("data: "):
                    self.messages.append(json.loads(text[6:]))
        except Exception as exc:  # noqa: BLE001 - surfaced by the test
            self.error = exc

    def stop(self) -> None:
        self._stop.set()


def test_ping_and_state(ui):
    _state, server = ui
    status, payload = _json(server, "GET", "/api/state")
    assert status == 200
    assert payload["connected"] is True
    assert payload["pong"]["pl_id"] == 0x4C410002
    status, payload = _json(server, "GET", "/api/ping")
    assert status == 200 and payload["pong"]["fw_version"] == 0x00090000


def test_capture_viewer_and_exports(ui):
    _state, server = ui
    capture = _capture(server, patgen="counter")
    assert capture["total_samples"] == 64_000
    assert capture["encoding"] == 0
    assert capture["has_signal"] is True
    capture_id = capture["id"]

    status, blob = _request(
        server, "GET", f"/api/captures/{capture_id}/envelope?buckets=1024")
    assert status == 200 and blob[:4] == b"LAEN"
    assert int.from_bytes(blob[6:8], "little") == 1024

    status, window = _request(
        server, "GET", f"/api/captures/{capture_id}/samples?start=0&count=16")
    assert status == 200 and len(window) == 16

    for fmt, marker in (("sr", b"PK"), ("vcd", b"$timescale"),
                        ("csv-events", b"# la-ui events"),
                        ("csv-samples", b"# la-ui samples")):
        status, body = _request(
            server, "GET", f"/api/captures/{capture_id}/export?format={fmt}")
        assert status == 200, fmt
        assert marker in body, fmt
        if fmt == "sr":
            status, payload = _json(server, "POST", "/api/open?name=e2e.sr",
                                    body)
            assert status == 200, payload
            opened = payload["capture"]
            assert opened["total_samples"] == capture["total_samples"]


def test_decode_uart_capture(ui):
    _state, server = ui
    capture = _capture(server, patgen=PATGEN_UART)
    status, payload = _json(
        server, "POST", f"/api/captures/{capture['id']}/decode",
        {"specs": ["uart:rx=0,baud=115200", "", ""]})
    assert status == 200, payload
    job = _wait_job(server, payload["job"]["id"])
    assert job["state"] == "done", job
    assert job["result"]["counts"]["0"] > 0
    status, payload = _json(
        server, "GET", f"/api/captures/{capture['id']}/annotations")
    events = payload["annotations"]["0"]
    bytes_seen = [event["data"] for event in events if event["name"] == "BYTE"]
    assert "4c" in bytes_seen and "41" in bytes_seen


def test_live_per_slot_and_auto_stop(ui):
    state, server = ui
    assert _json(server, "POST", "/api/patgen", {"mode": PATGEN_UART})[0] == 200
    status, payload = _json(
        server, "POST", "/api/live/config",
        {"specs": ["uart:rx=0,baud=115200", "spi:sck=1,mosi=2,cs=3",
                   "i2c:scl=3,sda=4"]})
    assert status == 200, payload
    reader = _SseReader(server.port)
    reader.start()
    deadline = time.monotonic() + 5.0
    while not reader.messages and time.monotonic() < deadline:
        time.sleep(0.05)

    status, payload = _json(server, "POST", "/control/start?slot=0")
    assert status == 200 and payload["state"] == "streaming"
    deadline = time.monotonic() + 15.0
    while time.monotonic() < deadline:
        if any(message.get("e") for message in reader.messages):
            break
        time.sleep(0.05)
    assert any(message.get("e") for message in reader.messages), \
        "no live UART events reached the SSE feed"

    status, payload = _json(server, "POST", "/control/start?slot=1")
    assert status == 200 and payload["slots"][1] == "streaming"
    status, payload = _json(server, "POST", "/control/stop?slot=0")
    assert status == 200 and payload["state"] == "streaming"
    assert payload["slots"][0] == "idle" and payload["slots"][1] == "streaming"

    # A capture auto-stops the live session (W09 mode-switch policy).
    capture = _capture(server, patgen="counter", words="16k")
    assert capture["total_samples"] == 16_000
    status, payload = _json(server, "GET", "/api/state")
    assert payload["session"] == "idle"
    assert reader.error is None
    reader.stop()


def test_trace_dump(ui):
    _state, server = ui
    status, payload = _json(server, "POST", "/api/trace/start")
    assert status == 200, payload
    time.sleep(1.0)
    status, payload = _json(server, "POST", "/api/trace/dump")
    assert status == 200, payload
    job = _wait_job(server, payload["job"]["id"])
    assert job["state"] == "done", job
    status, payload = _json(server, "GET", "/api/trace")
    assert status == 200
    model = payload["model"]
    assert model["lanes"] and model["summary"]["records"] > 0


def test_one_million_sample_capture(ui):
    _state, server = ui
    started = time.monotonic()
    capture = _capture(server, patgen="counter", words="1M", timeout=120)
    elapsed = time.monotonic() - started
    assert capture["total_samples"] == 1_000_000
    status, blob = _request(
        server, "GET", f"/api/captures/{capture['id']}/envelope?buckets=2048")
    assert status == 200 and blob[:4] == b"LAEN"
    print(f"1M-sample capture: {elapsed:.2f}s, "
          f"{len(blob)} envelope bytes, warnings={capture['warnings']}")
