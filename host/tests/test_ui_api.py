"""W02: unified UI HTTP API, jobs, live control and exports.

The tests run the real ``UiServer`` against the existing TCP mock analyzer,
so the whole host stack is exercised without hardware.
"""

from __future__ import annotations

import http.client
import json
import struct
import threading
import time

import pytest

from host.la_ui.jobs import JobBusy, JobRunner
from host.la_ui.server import UiServer, UiState
from host.tests.mock_analyzer import MockAnalyzer

PAGE = b"<!doctype html><html><body><div id=\"app\">ui</div></body></html>"


@pytest.fixture
def ui():
    state = UiState(decoder_specs=("uart:rx=0,baud=115200",), rate="100M")
    server = UiServer(state, port=0, page=PAGE)
    server.start()
    yield state, server
    server.stop()
    state.shutdown()


def _request(server, method, path, body=None, timeout=15.0):
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


def _wait_job(server, job_id, timeout=30.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        status, payload = _json(server, "GET", f"/api/jobs/{job_id}")
        assert status == 200
        if payload["state"] in ("done", "error", "cancelled"):
            return payload
        time.sleep(0.05)
    raise AssertionError(f"job {job_id} did not finish in {timeout}s")


def _capture(server, **options):
    body = {"rate": "10M", "words": "4096", "mode": "raw"}
    body.update(options)
    status, payload = _json(server, "POST", "/api/capture", body)
    assert status == 200, payload
    job = _wait_job(server, payload["job"]["id"])
    assert job["state"] == "done", job
    return job["result"]["capture"]


class _SseReader(threading.Thread):
    def __init__(self, port: int) -> None:
        super().__init__(name="ui-sse", daemon=True)
        self.port = port
        self.messages: list[dict] = []
        self.error: Exception | None = None
        self._stop = threading.Event()

    def run(self) -> None:
        try:
            connection = http.client.HTTPConnection("127.0.0.1", self.port,
                                                    timeout=5)
            connection.request("GET", "/events")
            response = connection.getresponse()
            assert response.status == 200, response.status
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


# -- shell and legacy contract ---------------------------------------------


def test_page_and_legacy_route(ui):
    _state, server = ui
    status, body = _request(server, "GET", "/")
    assert status == 200 and body == PAGE
    status, body = _request(server, "GET", "/legacy")
    assert status == 200
    assert b"<html" in body.lower()
    status, _body = _request(server, "GET", "/nope")
    assert status == 404


def test_status_keeps_legacy_fields(ui):
    _state, server = ui
    status, payload = _json(server, "GET", "/status")
    assert status == 200
    for key in ("clients", "events", "ring_capacity", "ring_dropped",
                "client_dropped", "state", "slots", "slot_labels", "bridge"):
        assert key in payload
    assert payload["session"] == "idle"
    assert payload["ui"]["version"]


# -- connection --------------------------------------------------------------


def test_connect_ping_and_disconnect(ui):
    _state, server = ui
    with MockAnalyzer("ping") as mock:
        status, payload = _json(server, "POST", "/api/connect",
                                {"target": mock.uri})
        assert status == 200 and payload["connected"] is True
        assert payload["pong"]["pl_id"] == 0x4C410002
        status, payload = _json(server, "POST", "/api/connect",
                                {"target": mock.uri})
        assert status == 409
        status, payload = _json(server, "GET", "/api/ping")
        assert status == 200 and payload["pong"]["fw_version"] == 1
        status, payload = _json(server, "GET", "/api/status")
        assert status == 200 and payload["status"]["word_count"] == 4096
        status, payload = _json(server, "POST", "/api/disconnect")
        assert status == 200 and payload["connected"] is False
        status, _payload = _json(server, "GET", "/api/state")
        assert status == 200


def test_capture_requires_connection(ui):
    _state, server = ui
    status, payload = _json(server, "POST", "/api/capture", {"words": "4096"})
    assert status == 409 and "not connected" in payload["error"]


def test_disconnect_clears_latched_decoder_slots(ui):
    _state, server = ui
    from host.la_cli import protocol as p

    with MockAnalyzer("ping") as mock:
        assert _json(server, "POST", "/api/connect",
                     {"target": mock.uri})[0] == 200
        assert _json(server, "POST", "/api/disconnect")[0] == 200
        # The V09 device latches decoder configs across sessions; disconnect
        # states OFF for all three slots so a later la-cli session starts clean.
        assert len(mock.decoder_history) == 3
        assert all(config[1] == p.DECODER_OFF
                   for config in mock.decoder_history)


def test_ports_route(ui):
    _state, server = ui
    status, payload = _json(server, "GET", "/api/ports")
    assert status == 200 and isinstance(payload["ports"], list)


# -- capture, decimation and exports ----------------------------------------


def test_capture_job_and_viewer_endpoints(ui):
    _state, server = ui
    with MockAnalyzer("capture_raw") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        capture = _capture(server, words="4096", mode="raw")
        assert capture["total_samples"] == 3056
        assert capture["has_signal"] is True
        capture_id = capture["id"]

        status, payload = _json(server, "GET", f"/api/captures/{capture_id}/meta")
        assert status == 200 and payload["id"] == capture_id

        status, blob = _request(server, "GET",
                                f"/api/captures/{capture_id}/envelope?buckets=64")
        assert status == 200
        assert blob[:4] == b"LAEN"
        assert blob[5] == 8  # channels
        assert int.from_bytes(blob[6:8], "little") == 64

        status, window = _request(server, "GET",
                                  f"/api/captures/{capture_id}/samples?start=0&count=8")
        assert status == 200 and window == bytes(range(8))

        status, payload = _json(server, "GET", "/api/captures")
        assert status == 200 and len(payload["captures"]) == 1

        for fmt, marker in (("sr", b"PK"), ("vcd", b"$date"),
                            ("csv-events", b"# la-ui events"),
                            ("csv-samples", b"# la-ui samples")):
            status, body = _request(
                server, "GET",
                f"/api/captures/{capture_id}/export?format={fmt}")
            assert status == 200, fmt
            assert marker in body, fmt

        status, payload = _json(server, "DELETE", f"/api/captures/{capture_id}")
        assert status == 200 and payload["deleted"] is True
        assert _json(server, "GET", f"/api/captures/{capture_id}/meta")[0] == 404


def test_capture_config_validation(ui):
    _state, server = ui
    with MockAnalyzer("capture_raw") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        for bad in ({"rate": "0"}, {"words": "0"}, {"mode": "bad"},
                    {"pre": 999999}, {"trig_rise": 9}, {"patgen": "nope"}):
            body = {"rate": "10M", "words": "4096", "mode": "raw"}
            body.update(bad)
            status, payload = _json(server, "POST", "/api/capture", body)
            assert status == 200  # job submitted; the job reports the error
            job = _wait_job(server, payload["job"]["id"])
            assert job["state"] == "error", (bad, job)
            assert "error" in job


def test_decode_job_and_annotations(ui):
    _state, server = ui
    with MockAnalyzer("capture_raw") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        capture = _capture(server, words="4096")
        status, payload = _json(
            server, "POST", f"/api/captures/{capture['id']}/decode",
            {"specs": ["uart:rx=0,baud=1000000", "", ""]})
        assert status == 200
        job = _wait_job(server, payload["job"]["id"])
        assert job["state"] == "done", job
        assert job["result"]["counts"]["0"] >= 1
        status, payload = _json(
            server, "GET", f"/api/captures/{capture['id']}/annotations")
        assert status == 200
        events = payload["annotations"]["0"]
        assert events and events[0]["name"] in ("BYTE", "FRAME_ERR",
                                                "PARITY_ERR")
        status, payload = _json(
            server, "POST", f"/api/captures/{capture['id']}/decode", {"specs": []})
        assert status == 400


def test_open_exported_sr_round_trip(ui):
    _state, server = ui
    with MockAnalyzer("capture_txn") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        capture = _capture(server, words="4096", mode="txn")
        status, body = _request(
            server, "GET", f"/api/captures/{capture['id']}/export?format=sr")
        assert status == 200 and body[:2] == b"PK"
        status, payload = _json(server, "POST", "/api/open?name=round.sr", body)
        assert status == 200, payload
        opened = payload["capture"]
        assert opened["source"] == "open:round.sr"
        assert opened["total_samples"] == capture["total_samples"]
        status, payload = _json(server, "GET", "/api/captures")
        assert len(payload["captures"]) == 2


# -- live control ------------------------------------------------------------


def test_live_control_and_sse(ui):
    _state, server = ui
    with MockAnalyzer("live") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        status, payload = _json(
            server, "POST", "/api/live/config",
            {"specs": ["uart:rx=0,baud=115200", "i2c:scl=3,sda=4", ""]})
        assert status == 200
        assert payload["slot_labels"][0].startswith("uart:")
        reader = _SseReader(server.port)
        reader.start()
        deadline = time.monotonic() + 5.0
        while not reader.messages and time.monotonic() < deadline:
            time.sleep(0.02)
        status, payload = _json(server, "POST", "/control/start?slot=0")
        assert status == 200 and payload["state"] == "streaming"
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            events = [message for message in reader.messages
                      if message.get("e")]
            if events:
                break
            time.sleep(0.05)
        assert events, "no live events reached the SSE feed"
        status, payload = _json(server, "POST", "/control/stop?slot=0")
        assert status == 200 and payload["state"] == "idle"
        assert reader.error is None
        reader.stop()


def test_live_config_rejects_bad_spec(ui):
    _state, server = ui
    with MockAnalyzer("ping") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        status, payload = _json(server, "POST", "/api/live/config",
                                {"specs": ["can:rx=0"]})
        assert status == 400 and "bad decoder spec" in payload["error"]


def test_control_without_connection(ui):
    _state, server = ui
    status, payload = _json(server, "POST", "/control/start?slot=0")
    assert status == 409


def test_patgen_route(ui):
    _state, server = ui
    with MockAnalyzer("ping") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        status, payload = _json(server, "POST", "/api/patgen", {"mode": "uart"})
        assert status == 200 and payload["patgen"] == "uart"
        assert len(mock.patgen_history) == 2
        # Protocol modes keep their nominal DIV; step modes are slowed so the
        # viewer shows square waves (100 MS/s -> DIV 15 = 16 samples/step).
        assert struct.unpack("<I", mock.patgen_history[0])[0] == 0
        assert struct.unpack("<I", mock.patgen_history[1])[0] == 0x31
        status, payload = _json(server, "POST", "/api/patgen", {"mode": "counter"})
        assert status == 200 and payload["patgen"] == "counter"
        assert len(mock.patgen_history) == 4
        assert struct.unpack("<I", mock.patgen_history[3])[0] == 0x000F0001
        status, payload = _json(server, "POST", "/api/patgen", {"mode": "bogus"})
        assert status == 400


def test_capture_configures_step_patgen(ui):
    _state, server = ui
    with MockAnalyzer("capture_raw") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        capture = _capture(server, rate="10M", words="4096", patgen="counter")
        assert capture["total_samples"] == 3056
        assert len(mock.patgen_history) == 2
        assert struct.unpack("<I", mock.patgen_history[0])[0] == 0
        # 10 MS/s -> sample_div 9 -> DIV 159 = 16 samples/step.
        assert struct.unpack("<I", mock.patgen_history[1])[0] == 0x009F0001


# -- trace -------------------------------------------------------------------


def test_trace_job_and_model(ui):
    _state, server = ui
    with MockAnalyzer("ping") as mock:
        assert _json(server, "POST", "/api/connect", {"target": mock.uri})[0] == 200
        status, payload = _json(server, "POST", "/api/trace/dump")
        assert status == 200
        job = _wait_job(server, payload["job"]["id"])
        assert job["state"] == "done", job
        status, payload = _json(server, "GET", "/api/trace")
        assert status == 200
        model = payload["model"]
        assert model["lanes"] and model["spans"] and model["markers"]
        assert model["summary"]["records"] == 4


# -- job runner --------------------------------------------------------------


def test_job_runner_single_slot_and_cancel():
    runner = JobRunner()
    release = threading.Event()
    started = threading.Event()

    def slow(cancel, progress):
        started.set()
        while not release.is_set():
            if cancel.is_set():
                raise RuntimeError("cancelled")
            time.sleep(0.01)
        return {"ok": True}

    first = runner.submit("test", slow)
    assert started.wait(2.0)
    with pytest.raises(JobBusy):
        runner.submit("test", slow)
    assert runner.cancel(first.id) is True
    assert runner.wait_idle(timeout=2.0)
    assert first.state == "cancelled"
    release.set()
