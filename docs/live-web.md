# Host live web bridge — frozen contract (V01, revised V07)

This document is the V01 contract for the PC-hosted live view introduced by the
full-USB pivot (`docs/usb-full-live-pivot.md`), revised by V07 (2026-09-20) to
add interactive Start/Stop control from the page. The bridge replaces the ESP32
`la_web` server for the USB-first default while keeping the gateway profile and
its web UI intact. `docs/plan.md` remains normative; `docs/protocol.md` defines
the analyzer wire protocol and is **not** changed by this bridge (no new wire
messages).

## 1. Scope and ownership

- The bridge runs on the PC, next to `la-cli`, and owns the analyzer target for
  its lifetime: **one process, one serial port / TCP connection**. Starting
  `la-cli live --serve` (or `la-web`) takes ownership; a second bridge or CLI
  session against the same port fails to open and must report the transport
  error, not retry silently.
- The bridge consumes the same `AnalyzerClient.live(...)` events as the CLI.
  The Zynq keeps decoding; the bridge only transports and formats.
- Teardown: on Ctrl+C the bridge stops any running session (LIVE_STOP), stops
  the HTTP server, closes the analyzer transport and releases the port before
  exiting. A page Stop only ends the session; the transport stays open for the
  next Start.
- No new PyPI runtime dependency: Python standard library only
  (`http.server`, `json`, `queue`, `threading`).

## 2. Transport choice: SSE (settled in V00)

- **Chosen: Server-Sent Events.** The bridge is push-only, so a long-lived
  `text/event-stream` HTTP response is enough and needs no WebSocket framing
  code. A WebSocket variant was rejected for V02 because it would require a
  hand-written RFC 6455 handshake/framing implementation for no feature gain.
- The browser message payload is **byte-compatible with `la_web_core`**:
  `{"s":<slot>,"e":[[<t_sample>,<kind>,"<hex>"],...]}` with lowercase hex and
  `la_web_core`'s 16-byte per-event data cap and slot range 0..2. The gateway
  page at `fw-esp32/components/la_web/web/index.html` therefore keeps its data
  handling; the host page only swaps its `WebSocket` listener for an
  `EventSource` listener (the small edit that chose SSE over WebSocket).
- SSE frames carry exactly one JSON message per `data:` line, terminated by a
  blank line, e.g. `data: {"s":0,"e":[[64,1,"4c"]]}\n\n`.

## 3. Endpoints and defaults

| Method | Path | Response |
|---|---|---|
| GET | `/` (and `/index.html`) | `text/html` live page (host copy of the gateway page) |
| GET | `/events` | `text/event-stream` feed (see section 4) |
| GET | `/status` | `application/json` bridge snapshot (see section 5) |
| POST | `/control/start` | starts a live session; `409` when one is running |
| POST | `/control/stop` | stops the running session; idempotent, always `200` |
| other | | `404` |

- Default bind: **`127.0.0.1`**, default port: **`8080`** (`--bind`/`--port`
  override). Binding to a non-loopback address is allowed only when explicitly
  requested; the bridge has no authentication and must not be exposed by
  default.
- The page is served from `host/la_live_web/static/index.html`, shipped inside
  the wheel as package data. No CDN or external asset is referenced.
- `HEAD` is not required; responses are UTF-8.

## 3a. Interactive session control (V07)

- The bridge starts **idle**: opening the page does not start decoding. The
  page's **Start** button posts `/control/start`; **Stop** posts
  `/control/stop`. The CLI prints the URL and waits for the browser
  (`--autostart` keeps the old scripted behavior: one session starts
  immediately and the command exits when it ends).
- One session at a time. `start` while streaming returns `409` with
  `{"error": "a live session is already running"}`; `stop` while idle is a
  no-op `200`. A session that ends by `--count`, `--timeout` or an error
  returns to idle and Start is enabled again.
- The analyzer transport stays open between sessions (one owner), so Start →
  Stop → Start works without re-opening the port. Each start re-arms the
  pattern generator when `--patgen` was selected (the mode/DIV latch rule).
- State machine: `idle -> streaming -> stopping -> idle`. `/status` reports
  `session` (`idle`/`streaming`/`stopping`), `sessions` (started count) and the
  last session `error` when one occurred.
- Interactive sessions keep draining after an `EVT_OVERRUN` and count it in
  the stats; the scripted `--autostart` path ends on it like the classic
  `la-cli live` command.

## 4. Event feed, replay and backpressure

- On `GET /events` the bridge attaches the client, then replays the current
  bounded ring (oldest first) and streams new messages. The replay exists for
  the page reload/reconnect story.
- **Bounded replay ring:** 4096 formatted messages by default. When full the
  oldest message is evicted and `ring_dropped` is counted; the live session is
  never blocked by the ring.
- **Single client (settled in V00):** `max_clients` defaults to 1. A second
  concurrent `GET /events` is refused with `409 Conflict` and a short text
  body; the hub counts `refused`. The hub fan-out is a per-client bounded
  queue so `max_clients` can be raised later without a redesign.
- **Slow client:** each client has a 256-message queue. When it is full the
  message is dropped for that client and `client_dropped` is counted; the live
  pump and the analyzer link are never blocked. Reconnect replays the ring.
- The bridge emits one `{"stats":{...}}` message per second to each attached
  client (also serving as an SSE keepalive). A client that stops reading is
  detected by the socket write failing; it is detached and its slot freed.

## 5. Stats message and `/status` snapshot

The stats message reuses the gateway's `stats` envelope but reports host
counters (no relay/heap):

```json
{"stats":{"clients":1,"events":1234,"messages":42,"sent":42,"dropped":0,
          "ring":17,"ring_capacity":4096,"ring_dropped":0,"client_dropped":0,
          "refused":0,"state":"streaming","target":"serial://COM13",
          "decoder":"uart:rx=0,baud=115200","error":null,"uptime_s":12.3}}
```

`/status` returns the same fields plus `"connected": true|false` and, when a
session controller is present, `"session"`, `"sessions"` and the last session
`"error"`, so the E2E gates can assert counts without parsing SSE.

## 6. Formatting parity

- `host/la_live_web/core.py` mirrors `la_web_core.{h,c}`:
  - one message per decoded event frame; the message slot is the first
    record's slot;
  - `[t_sample, kind, "hex"]` tuples, lowercase hex, 16-byte data cap,
    `begin`/`event`/`end` truncation with no partial message emitted;
  - exact strings match the native vectors in
    `tests/native/esp32_web_host_tests.c` (`{"s":0,"e":[[64,1,"41"],[80,8,""]]}`,
    `{"s":1,"e":[[1234,1,"4f4b"],[2000,8,""]]}`, `{"s":0,"e":[]}`, hex
    `"000aff41"`).
- Overrun reports are **not** decoded events and are not part of the browser
  message; they are counted in `/status` (`events` excludes them) and appear in
  the CLI summary. The page shows the live byte stream; `EVT_OVERRUN` is a
  host-side condition.

## 7. CLI surface (V03)

- `la-cli live ... --serve [--bind HOST] [--port PORT] [--autostart]` serves
  the host live view; the text/CSV modes are unchanged when `--serve` is
  absent. Without `--autostart` the session is controlled from the page.
- `la-web` is the standalone equivalent (`--target`, `--decoder`, `--rate`,
  `--patgen`, `--count`, `--compact`, `--stop-on`, `--timeout`, `--bind`,
  `--port`, `--autostart`). Both entry points call the same implementation and
  print the bound URL once.
- Exit codes: 0 on a clean session, non-zero with a Click error when the
  target cannot be opened or the server cannot bind.

## 8. Acceptance checks

- V02: `test-host` green including formatting parity against the native
  vectors, ring overflow/drop accounting, single-client `409`, slow-client
  drop without blocking, malformed frame rejection (no partial message) and
  clean detach.
- V03/V07: `[HUMAN]` browser smoke — open `http://127.0.0.1:8080`, click
  **Start**, run the patgen UART source, confirm events render line by line,
  click **Stop**, confirm the state returns to idle, Start again, and reload
  the page to confirm the feed reconnects. Host tests cover the same control
  endpoints against the mock analyzer.
- V04: over `serial://COM13` the feed receives exactly the requested event
  count (`--count`), `/status` matches the SSE message total, the device
  returns to IDLE, and the 1 MHz SPI overload recovers within 3 s while the
  bridge serves.
