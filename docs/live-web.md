# Host live web bridge — frozen contract (V01, revised V07)

This document is the V01 contract for the PC-hosted live view introduced by the
full-USB pivot (`docs/usb-full-live-pivot.md`), revised by V07 (2026-09-20) to
add interactive Start/Stop control from the page. The bridge replaces the ESP32
`la_web` server for the USB-first default while keeping the gateway profile and
its web UI intact. `docs/plan.md` remains normative; `docs/protocol.md` defines
the analyzer wire protocol and is **not** changed by this bridge (no new wire
messages).

**Unified UI note (W00-W09, 2026-09-20).** The unified workspace
(`docs/unified-ui-plan.md`) reuses this bridge's hub, SSE feed, `/status` and
`/control/*` contract unchanged and serves the workspace at `/` with this
legacy page at `/legacy`. `la-web` keeps serving the page below exactly as
documented here; the acceptance checks in section 8 still apply to it.

**W11 continuation (18 October 2026).** The unified `la-ui` now opts into real
waveform preview on USB firmware 0x000A0000, verified after SD cold boot. The
legacy event message/standalone `la-web` contract below is retained. Unified
SSE additionally carries `{"waveform":...}` messages from exact FPGA runs,
not reconstructed decoded bytes; stats add waveform packet/omission counts.
Preview replay is bounded separately to 16 packets. `/api/reset` stops work,
disconnects and clears session/browser state. See [protocol.md](protocol.md)
and [current-handover.md](current-handover.md) for current limits and evidence.

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
| POST | `/control/start?slot=N` | starts/enables decoder slot N; `409` when it cannot |
| POST | `/control/start` | enables every configured slot (scripted/autostart) |
| POST | `/control/stop?slot=N` | disables slot N; ends the session when it was the last one |
| POST | `/control/stop` | stops every slot and the session; idempotent |
| other | | `404` |

- Default bind: **`127.0.0.1`**, default port: **`8080`** (`--bind`/`--port`
  override). Binding to a non-loopback address is allowed only when explicitly
  requested; the bridge has no authentication and must not be exposed by
  default.
- The page is served from `host/la_live_web/static/index.html`, shipped inside
  the wheel as package data. No CDN or external asset is referenced.
- `HEAD` is not required; responses are UTF-8.

## 3a. Interactive session control (V07, per-slot in V09)

- The bridge starts **idle**: opening the page does not start decoding. Each
  decoder pane has its own **Start**/**Stop** buttons; the header has **Stop
  all**. `--autostart` enables every configured slot immediately and exits when
  the session ends (scripted path).
- **One live session, independent slots.** The first Start starts the session
  with only that slot enabled; another pane's Start sends that slot's
  `DECODER_CFG` mid-session (the firmware forwards it through AcqTask to the
  slot's decoder task). A pane's Stop sends an OFF config for that slot only;
  the other panes keep streaming. Stopping the last enabled slot sends
  LIVE_STOP and the session returns to idle.
- **The session states its slot set explicitly:** because the device latches
  decoder configs across sessions, every session start sends the enabled
  slots' configs plus an OFF for each of the other two slots. This prevents a
  stale slot from a previous session or bridge process from streaming.
- Slots are configured on the command line, one spec per `--decoder` (up to
  three, in slot order: `uart:...`, `spi:...` or `i2c:...`). A pane whose slot
  has no spec shows `unconfigured` and its Start is refused with `409`.
- The analyzer transport stays open between sessions (one owner), so Start →
  Stop → Start works without re-opening the port. Each session start re-arms
  the pattern generator when `--patgen` was selected (the mode/DIV latch rule).
- State machine: `idle -> streaming -> stopping -> idle`. `/status` reports
  `session`, `sessions` (started count), `slots` (per-slot
  `idle`/`streaming`/`unconfigured`), `slot_labels` and the last session
  `error`.
- Interactive sessions keep draining after an `EVT_OVERRUN` and count it in
  the stats; the scripted `--autostart` path ends on it like the classic
  `la-cli live` command.
- Firmware note: `DECODER_CFG` is accepted in view CAPT (streaming) as well as
  idle; AcqTask is the only writer of the segment consumer count, so the
  mid-session enable/disable goes through `LA_CTRL_DECODER_CFG`.

## 3b. Page rendering budget (V08)

- The live page is a high-rate view (measured 115200 UART stream: ~9,950
  events/s in ~491 SSE messages/s). It must stay interactive while streaming:
  - incoming messages are coalesced and drawn once per animation frame
    (`requestAnimationFrame`), never one DOM insertion per record;
  - each pane keeps at most 2000 lines and trims by removing child nodes
    (chunked at 500); the pane is never rebuilt with `innerHTML`;
  - a frame carrying more than 2000 records renders the newest and counts the
    rest as `display-skipped` in the footer; the stats bar's `events` remains
    the authoritative received count;
  - auto-scroll follows only while the pane is at the bottom, so scrolling up
    during a stream keeps the user's position.

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
  client (also serving as an SSE keepalive), plus one immediately when the
  feed attaches so the page shows the real slot state without the first-second
  delay. A client that stops reading is detected by the socket write failing;
  it is detached and its slot freed.
- The page header shows a `page vXX | server vXX` marker from the stats
  message; a missing or older marker means the browser is running a stale
  page and needs a hard refresh (Ctrl+Shift+R).

## 5. Stats message and `/status` snapshot

The stats message reuses the gateway's `stats` envelope but reports host
counters (no relay/heap):

```json
{"stats":{"clients":1,"events":1234,"messages":42,"sent":42,"dropped":0,
          "ring":17,"ring_capacity":4096,"ring_dropped":0,"client_dropped":0,
          "refused":0,"state":"streaming","target":"serial://COM13",
          "decoder":"slot 0: uart:rx=0,baud=115200",
          "slots":["streaming","idle","unconfigured"],
          "slot_labels":["uart:rx=0,baud=115200","",""],
          "error":null,"uptime_s":12.3}}
```

`/status` returns the same fields plus `"connected": true|false` and, when a
session controller is present, `"session"` and `"sessions"`, so the E2E gates
can assert counts without parsing SSE.

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

## 7. CLI surface (V03/V09)

- `la-cli live ... --serve [--bind HOST] [--port PORT] [--autostart]` serves
  the host live view; the text/CSV modes are unchanged when `--serve` is
  absent. `--decoder` may be repeated up to three times; spec N configures
  slot N.
- `la-web` is the standalone equivalent (`--target`, repeated `--decoder`,
  `--rate`, `--patgen`, `--count`, `--compact`, `--stop-on`, `--timeout`,
  `--bind`, `--port`, `--autostart`). Both entry points call the same
  implementation and print the bound URL once.
- Exit codes: 0 on a clean session, non-zero with a Click error when the
  target cannot be opened or the server cannot bind.

## 8. Acceptance checks

- V02: `test-host` green including formatting parity against the native
  vectors, ring overflow/drop accounting, single-client `409`, slow-client
  drop without blocking, malformed frame rejection (no partial message) and
  clean detach.
- V03/V07/V09: `[HUMAN]` browser smoke — open `http://127.0.0.1:8080`, click a
  pane's **Start**, run the patgen source, confirm events render line by line,
  start a second pane and confirm both stream, stop one pane and confirm the
  other keeps running, stop the last and confirm idle, then reload the page to
  confirm the feed reconnects. Host tests cover the same control endpoints
  against the mock analyzer.
- V04: over `serial://COM13` the feed receives exactly the requested event
  count (`--count`), `/status` matches the SSE message total, the device
  returns to IDLE, and the 1 MHz SPI overload recovers within 3 s while the
  bridge serves.
- V09: over `serial://COM13`, per-slot Start/Stop is independent (two UART
  decoders on CH0, an I2C decoder in slot 2), a new session states its slot
  set explicitly, and the device returns to IDLE.
