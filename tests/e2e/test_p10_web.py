"""P10.1 live web view bench gates (E2E, [HUMAN] bench gate).

Run with the web-view firmware flashed and the live source wired
(JE-1 -> JC-1 jumper, UART patgen on CH0):

    .venv\\Scripts\\python.exe tools\\run.py test-e2e -- \\
        --target tcp://HOST:5555 --live -k test_p10

`test_p10_web_page_served` fetches the embedded page over HTTP and checks the
content type and the UI markers. `test_p10_ws_streams_events` opens the /ws
WebSocket, runs a live UART decode session in the background and requires the
page stream to carry the decoded "LA-TEST\\n" bytes plus a stats message.

Without --target the tests skip so the suite stays runnable off-bench.
"""

from __future__ import annotations

import base64
import json
import os
import socket
import struct
import threading
import time
import urllib.parse
import urllib.request

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri

FCLK_HZ = 100_000_000
UART_FRAME = b"LA-TEST\n"
EVENT_BYTE = 1


@pytest.fixture(scope="module")
def target(request) -> str:
    value = request.config.getoption("--target")
    if not value:
        pytest.skip("no --target: the P10.1 web gates require the bench hardware")
    return value


@pytest.fixture(scope="module")
def client(target):
    analyzer = AnalyzerClient(open_uri(target))
    yield analyzer
    analyzer.close()


def _web_host(target: str) -> str:
    parsed = urllib.parse.urlparse(target)
    return parsed.hostname or "192.168.100.37"


class _WsClient:
    """Minimal RFC 6455 text client (stdlib only; server frames are unmasked)."""

    def __init__(self, host: str, port: int = 80, path: str = "/ws",
                 timeout: float = 5.0) -> None:
        self._sock = socket.create_connection((host, port), timeout=timeout)
        key = base64.b64encode(os.urandom(16)).decode()
        request = (
            f"GET {path} HTTP/1.1\r\nHost: {host}:{port}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        )
        self._sock.sendall(request.encode())
        header = b""
        while b"\r\n\r\n" not in header:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise AssertionError("websocket handshake closed early")
            header += chunk
        status = header.split(b"\r\n", 1)[0]
        assert b"101" in status, status

    def recv_text(self) -> str | None:
        try:
            first = self._recv_exact(2)
        except (socket.timeout, TimeoutError):
            return None
        opcode = first[0] & 0x0F
        length = first[1] & 0x7F
        if length == 126:
            length = struct.unpack(">H", self._recv_exact(2))[0]
        elif length == 127:
            length = struct.unpack(">Q", self._recv_exact(8))[0]
        payload = self._recv_exact(length) if length else b""
        if opcode == 0x8:
            return None
        if opcode != 0x1:
            return ""
        return payload.decode("utf-8", errors="replace")

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass

    def _recv_exact(self, count: int) -> bytes:
        data = b""
        while len(data) < count:
            chunk = self._sock.recv(count - len(data))
            if not chunk:
                raise AssertionError("websocket closed mid-frame")
            data += chunk
        return data


def test_p10_web_page_served(target: str) -> None:
    host = _web_host(target)
    with urllib.request.urlopen(f"http://{host}/", timeout=8) as response:
        assert response.status == 200
        assert "text/html" in response.headers.get("Content-Type", "")
        body = response.read()
    assert b"live view" in body, body[:200]
    assert b"/ws" in body, "page does not reference the websocket endpoint"


def test_p10_ws_streams_events(target: str, client: AnalyzerClient) -> None:
    host = _web_host(target)
    baud = 115200
    client.configure_patgen(0)
    time.sleep(0.01)
    client.configure_patgen(p.PATGEN_EN_MASK | (p.PATGEN_MODE_UART_115200 << p.PATGEN_MODE_SHIFT))
    ws = _WsClient(host)
    try:
        result: dict = {}

        def run_live() -> None:
            try:
                result["events"] = client.live(
                    p.pack_decoder_cfg_uart(0, 0, baud, FCLK_HZ),
                    config=p.pack_config(
                        ctrl_flags=p.CTRL_STREAM, sample_div=0, mode=1,
                        chan_en=0xFF, post_samples=0, txn_max_run=0x00FFFFFF,
                        live_flush_cycles=200_000,
                    ),
                    count=32, timeout_s=10.0,
                )
            except Exception as exc:  # noqa: BLE001
                result["error"] = exc

        worker = threading.Thread(target=run_live, daemon=True)
        worker.start()

        decoded = b""
        saw_stats = False
        deadline = time.monotonic() + 12.0
        while time.monotonic() < deadline:
            if saw_stats and len(decoded) >= len(UART_FRAME):
                break
            text = ws.recv_text()
            if not text:
                continue
            message = json.loads(text)
            if "stats" in message:
                saw_stats = True
                continue
            for record in message.get("e", []):
                if record[1] == EVENT_BYTE and record[2]:
                    decoded += bytes.fromhex(record[2])
        worker.join(timeout=12.0)
        assert "error" not in result, result.get("error")
        assert saw_stats, "no stats message on the websocket"
        assert UART_FRAME in decoded, decoded[:64]
    finally:
        ws.close()
