"""V02 host live-web bridge tests: formatting parity, hub policy, SSE server.

The formatting vectors are the same ones asserted by the native gateway test
`tests/native/esp32_web_host_tests.c`, so the host bridge and `la_web_core`
stay byte-compatible.
"""

from __future__ import annotations

import http.client
import json
import time

import pytest
from click.testing import CliRunner

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.commands import cli, web
from host.la_cli.transport import open_uri
from host.la_live_web import (
    HubFull,
    LiveEventHub,
    LiveWebServer,
    MessageBuilder,
    format_frame,
)

FCLK_HZ = 100_000_000


def _event(slot: int, kind: int, t_sample: int, data: bytes = b"") -> dict:
    return {"slot": slot, "kind": kind, "t_sample": t_sample, "data": data}


# -- formatting parity with la_web_core ------------------------------------


def test_message_layout_matches_native_vector() -> None:
    builder = MessageBuilder()
    assert builder.begin(1)
    assert builder.event(1, 1234, b"OK")
    assert builder.event(8, 2000, b"")
    assert builder.end() == '{"s":1,"e":[[1234,1,"4f4b"],[2000,8,""]]}'
    assert builder.events == 2
    assert builder.slots == [0, 1, 0]


def test_frame_round_trip_matches_native_vector() -> None:
    events = [
        _event(0, 1, 0x40, b"A"),
        _event(0, 8, 0x50, b""),
    ]
    assert format_frame(events) == '{"s":0,"e":[[64,1,"41"],[80,8,""]]}'


def test_hex_encoding_and_data_cap() -> None:
    builder = MessageBuilder()
    assert builder.begin(2)
    assert builder.event(1, 7, bytes([0x00, 0x0A, 0xFF, 0x41]))
    assert builder.end() == '{"s":2,"e":[[7,1,"000aff41"]]}'

    # la_web_core caps each event at 16 data bytes; the rest is dropped.
    payload = bytes(range(20))
    capped = format_frame([_event(0, 1, 1, payload)])
    assert capped is not None
    assert payload[:16].hex() in capped
    assert payload[16:].hex() not in capped


def test_empty_message_layout() -> None:
    builder = MessageBuilder()
    assert builder.begin(0)
    assert builder.end() == '{"s":0,"e":[]}'


def test_slot_guard_and_unbegun_end() -> None:
    builder = MessageBuilder()
    assert not builder.begin(3)
    assert builder.end() is None
    assert format_frame([]) is None


def test_truncation_never_emits_a_partial_message() -> None:
    builder = MessageBuilder(capacity=40)
    for index in range(8):
        if not builder.event(1, index, bytes([1, 2, 3, 4, 5, 6, 7, 8])):
            break
    assert index < 8
    assert builder.truncated
    assert builder.end() is None

    hub = LiveEventHub(capacity=40)
    assert hub.publish_frame([_event(0, 1, 1, b"0123456789abcdef")]) is None
    assert hub.stats()["format_dropped"] == 1
    assert hub.events == 0


# -- hub policy ------------------------------------------------------------


def _publish_indexed(hub: LiveEventHub, count: int) -> None:
    for index in range(count):
        hub.publish_frame([_event(0, 1, index, bytes([index]))])


def test_ring_overflow_replay_and_drop_accounting() -> None:
    hub = LiveEventHub(ring_size=4)
    _publish_indexed(hub, 6)
    stats = hub.stats()
    assert stats["messages"] == 6
    assert stats["ring"] == 4
    assert stats["ring_dropped"] == 2
    assert stats["dropped"] == 2

    feed = hub.attach()
    try:
        replayed = [feed.queue.get_nowait() for _ in range(4)]
        assert [json.loads(message)["e"][0][0] for message in replayed] == [2, 3, 4, 5]
    finally:
        hub.detach(feed)


def test_single_client_conflict_and_release() -> None:
    hub = LiveEventHub()
    first = hub.attach()
    with pytest.raises(HubFull):
        hub.attach()
    assert hub.stats()["refused"] == 1
    hub.detach(first)
    assert hub.stats()["clients"] == 0
    second = hub.attach()
    hub.detach(second)


def test_slow_client_drops_without_blocking() -> None:
    hub = LiveEventHub(client_queue=2)
    feed = hub.attach()
    _publish_indexed(hub, 5)
    assert feed.dropped == 3
    stats = hub.stats()
    assert stats["client_dropped"] == 3
    assert stats["sent"] == 2
    assert stats["events"] == 5  # the live session is never blocked or lost
    hub.detach(feed)


def test_publish_limit_trims_the_final_batch() -> None:
    hub = LiveEventHub()
    events = [_event(0, 1, index) for index in range(10)]
    message = hub.publish_frame(events, limit=4)
    assert message is not None
    assert message.count("[") == 5  # {"s":..,"e":[ + 4 tuples
    assert hub.events == 4
    assert hub.publish_frame(events, limit=0) is None


def test_stats_message_shape() -> None:
    hub = LiveEventHub()
    hub.state = "streaming"
    hub.target = "serial://COM13"
    payload = json.loads(hub.stats_message())
    stats = payload["stats"]
    for key in ("clients", "events", "messages", "sent", "dropped", "ring",
                "ring_capacity", "ring_dropped", "client_dropped", "refused",
                "state", "target", "uptime_s"):
        assert key in stats, key
    assert stats["state"] == "streaming"
    assert stats["target"] == "serial://COM13"


# -- HTTP/SSE server -------------------------------------------------------


@pytest.fixture
def web_server():
    hub = LiveEventHub()
    server = LiveWebServer(hub, port=0)
    server.start()
    yield server, hub
    server.stop()


def _get(port: int, path: str) -> tuple[int, dict, bytes]:
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    connection.request("GET", path)
    response = connection.getresponse()
    headers = dict(response.getheaders())
    body = response.read()
    connection.close()
    return response.status, headers, body


def test_server_serves_page_and_status(web_server) -> None:
    server, hub = web_server
    status, headers, body = _get(server.port, "/")
    assert status == 200
    assert "text/html" in headers["Content-Type"]
    assert b"host live view" in body
    assert b"/events" in body

    status, headers, body = _get(server.port, "/status")
    assert status == 200
    assert "application/json" in headers["Content-Type"]
    snapshot = json.loads(body)
    assert snapshot["connected"] is False
    assert snapshot["ring_capacity"] == 4096

    status, _headers, _body = _get(server.port, "/missing")
    assert status == 404


def test_server_sse_streams_events_and_stats(web_server) -> None:
    server, hub = web_server
    connection = http.client.HTTPConnection("127.0.0.1", server.port, timeout=5)
    connection.request("GET", "/events")
    response = connection.getresponse()
    try:
        assert response.status == 200
        assert "text/event-stream" in response.getheader("Content-Type")
        hub.publish_frame([_event(0, 1, 64, b"L"), _event(0, 1, 65, b"A")])
        lines: list[str] = []
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline and not any(
                line.startswith("data:") and '"e"' in line for line in lines):
            line = response.readline().decode("utf-8").rstrip("\r\n")
            if line:
                lines.append(line)
        data_lines = [line[6:] for line in lines if line.startswith("data:")]
        assert any('"e"' in line for line in data_lines), lines
        message = json.loads(next(line for line in data_lines if '"e"' in line))
        assert message == {"s": 0, "e": [[64, 1, "4c"], [65, 1, "41"]]}

        # The status endpoint reports the attached client and the event count.
        status, _headers, body = _get(server.port, "/status")
        assert status == 200
        snapshot = json.loads(body)
        assert snapshot["connected"] is True
        assert snapshot["events"] == 2
    finally:
        connection.close()


def test_server_second_feed_gets_409(web_server) -> None:
    server, _hub = web_server
    first = http.client.HTTPConnection("127.0.0.1", server.port, timeout=5)
    first.request("GET", "/events")
    response = first.getresponse()
    try:
        assert response.status == 200
        second = http.client.HTTPConnection("127.0.0.1", server.port, timeout=5)
        second.request("GET", "/events")
        conflict = second.getresponse()
        assert conflict.status == 409
        assert b"single-client" in conflict.read()
        second.close()
    finally:
        first.close()


def test_page_is_served_from_package_data() -> None:
    from host.la_live_web.server import PAGE_PATH
    assert PAGE_PATH.is_file()
    page = PAGE_PATH.read_text(encoding="utf-8")
    assert "EventSource" in page
    assert "WebSocket" not in page


# -- integration with the mock analyzer ------------------------------------


def test_bridge_publishes_mock_live_frames(analyzer) -> None:
    mock = analyzer("live")
    hub = LiveEventHub()
    client = AnalyzerClient(open_uri(mock.uri))
    try:
        client.live(
            p.pack_decoder_cfg_uart(0, 0, 115200, FCLK_HZ),
            count=None, timeout_s=5.0, collect_events=False,
            on_event=hub.publish_frame,
        )
    finally:
        client.close()
    stats = hub.stats()
    assert stats["events"] == 24  # three frames of the eight "LA-TEST\n" bytes
    assert stats["messages"] == 3
    assert stats["format_dropped"] == 0
    feed = hub.attach()
    try:
        replayed = [feed.queue.get_nowait() for _ in range(3)]
        text = "".join(
            bytes.fromhex(record[2]).decode()
            for message in replayed
            for record in json.loads(message)["e"]
            if record[1] == 1
        )
        assert text == "LA-TEST\n" * 3
    finally:
        hub.detach(feed)


# -- CLI integration (V03) --------------------------------------------------


def test_live_serve_command_with_mock(analyzer) -> None:
    mock = analyzer("live")
    result = CliRunner().invoke(cli, ["live", "-t", mock.uri, "--serve",
                                      "--port", "0"])
    assert result.exit_code == 0, result.output
    assert "live web view: http://127.0.0.1:" in result.output
    assert "24 events in 3 messages" in result.output


def test_web_command_count_limits_the_feed(analyzer) -> None:
    mock = analyzer("live")
    result = CliRunner().invoke(web, ["-t", mock.uri, "--port", "0",
                                      "--count", "5"])
    assert result.exit_code == 0, result.output
    assert "5 events in 1 message" in result.output


def test_live_serve_rejects_csv(analyzer) -> None:
    mock = analyzer("live")
    result = CliRunner().invoke(cli, ["live", "-t", mock.uri, "--serve", "--csv"])
    assert result.exit_code != 0
    assert "cannot be combined" in result.output


def test_web_bind_failure_is_reported(analyzer) -> None:
    mock = analyzer("live")
    blocker = LiveWebServer(LiveEventHub(), port=0)
    blocker.start()
    try:
        result = CliRunner().invoke(web, ["-t", mock.uri, "--port",
                                          str(blocker.port)])
        assert result.exit_code != 0
        assert "cannot bind the live web view" in result.output
    finally:
        blocker.stop()
