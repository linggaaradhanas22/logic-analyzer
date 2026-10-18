# Full-USB live pivot — implementation instructions

**Current continuation (18 October 2026):** V00-V09 are implemented and the USB
profile is the product default. W11 firmware 0x000A0000 / UI w11 add real live
waveforms + decode, activity focus and Reset UI; the updated SD image passed
cold boot, UI E2E 6/6 and external UART browser checks 10/10. Use
[current-handover.md](current-handover.md) for current boot/wiring, hashes,
evidence and source/publication state. The ordered pivot packets below are
historical, not instructions to restart implementation.

User-directed scope change (2026-09-19): the analyzer becomes **USB-first**.
Direct Zynq PS USB0 is the primary host transport, and the live browser view is
**hosted on the PC** instead of the ESP32. This document is the execution
package for that pivot; `docs/plan.md` remains the normative overall plan and
the U00-U09 migration package remains the evidence base for the USB stack.

## 1. Outcome

- The default product build/load path is the USB profile (`la_app_usb`); the
  gateway remains a supported optional profile, not removed.
- Live decoding (UART/SPI/I2C) stays on the Zynq. Only the transport and the
  live-view host change.
- The PC serves the live view: a host web bridge streams the decoded events
  that `la-cli live` already receives to a browser page, with a bounded
  replay ring and a reconnect story.
- Release packaging becomes profile-aware: the SD/BOOT.BIN path can ship the
  USB image, and the ESP32 images are optional/labeled as the gateway profile.

## 2. Boundaries and non-goals

- Do not silently remove wireless/browser features: the gateway profile, its
  `la_web` server and the ESP32 build stay in the repository and remain
  buildable/testable. The pivot changes defaults and the live-view host.
- Native Windows only; no WSL/Linux/container dependency. Prefer the Python
  standard library; no new PyPI runtime dependency without a recorded
  decision.
- The wired 100 MS/s acquisition scope, the protocol wire format and the
  existing test suites stay authoritative. Live wire changes require
  `test-native` plus the affected regression checks.
- The USB live path must remain fault tolerant: the bounded event drain fixed
  on 2026-09-19 (`la_usb_enqueue_wait`, `la_usb_tx_space`, 4-block cap) is
  part of the contract.

## 3. Baseline (do not regress)

- U00-U09 evidence: `build/reports/usb-u03-enumeration.md`,
  `usb-u04-protocol.md`, `usb-u05-regression.md`,
  `usb-u06-performance.{md,json}`, `usb-u08-doc-review.md`,
  `usb-u09-release.md`.
- USB profile: 1.62 MB/s drain, 60 s live UART zero-drop, p99 <= 25 ms,
  overload recovery <= 3 s, trace dump, 9/9 transport suite.
- Gateway profile: unchanged build; `la_web` single-page UI at
  `fw-esp32/components/la_web/web/index.html`, formatting core
  `la_web_core.{h,c}`, server glue `la_web_server.c`; the browser message is
  `{"s":<slot>,"e":[[<t_sample>,<kind>,"<hex>"], ...]}` (see
  `la_web_core.h`).

## 4. Ordered work packets

### V00 — Freeze the pivot scope

**Entry:** this document reviewed. **Files:** `docs/plan.md`, `docs/status.md`,
`docs/limits.md`, `README.md`.

- Record the dated decision: USB is the default product transport; the
  gateway is a supported optional profile; the live view moves to the host.
- Disposition P9.4 (Wi-Fi tuning), P10.1 (browser live view) and the
  ESP32-S3 transport port: **retained as the gateway profile's features**,
  not deleted; mark them optional/deferred for the USB-first default.
- State the open choices for the user: WebSocket vs SSE, single vs multi
  client, whether `fw-zynq` defaults to USB or keeps gateway and adds
  `--profile usb` as default via a new flag.

**Exit:** dated decision and dispositions in `plan.md`/`status.md`; no code
change. Evidence: the ledger entry.

### V01 — Host live bridge contract

**Entry:** V00. **Files:** new `docs/live-web.md`, `docs/protocol.md` if the
wire changes (it should not).

- Define the host bridge: it owns the analyzer target (one serial port at a
  time), consumes `AnalyzerClient.live(...)` events, keeps a bounded replay
  ring, and serves the page plus the event feed.
- Freeze the browser contract: reuse the existing `{"s":..,"e":[[..]]}`
  message so the ESP32 page can be served unchanged, or document an SSE
  variant. Decide and record WebSocket (stdlib handshake/framing, no new
  dependency) vs SSE (simpler, page edit required).
- Freeze endpoints, defaults (bind `127.0.0.1`, port 8080), single-client
  policy, reconnect/backpressure behavior, and the bounded-ring size.
- Record the port-conflict rule with `la-cli live` (one owner; `--serve`
  implies ownership).

**Exit:** frozen contract with acceptance checks; no hardware needed.

### V02 — Bridge core (portable, host-tested)

**Entry:** V01. **Files:** new `host/la_live_web/` (`core.py`, `server.py`,
`static/index.html`), `host/tests/test_live_web.py`, `pyproject.toml`
(package + `la-web` script if chosen).

- Implement the event hub (bounded ring, fan-out to the connected client,
  drop accounting), the JSON formatting equivalent to `la_web_core`, the
  HTTP static serving and the chosen feed transport, all stdlib-only.
- Unit-test with the existing mock analyzer: formatting parity with
  `esp32_web_host_tests.c`, ring overflow, client connect/disconnect, slow
  client behavior, malformed frames.

**Exit:** `test-host` green with the new tests; no new dependencies.

### V03 — CLI integration and browser smoke

**Entry:** V02. **Files:** `host/la_cli/commands.py` (a `live --serve` mode or
the `la-web` entry point), `docs/using-the-analyzer.md`.

- Wire the bridge to a real `serial://COMx` USB target: start the live
  session, serve the page, stream events, stop cleanly (LIVE_STOP) on
  Ctrl+C/close, and release the port.
- `[HUMAN]` browser smoke: open `http://127.0.0.1:8080`, run the patgen UART
  source and confirm events render and the page reconnects after a reload.

**Exit:** host tests + the [HUMAN] browser record; the CLI text/CSV live
modes stay unchanged.

### V04 — USB live end-to-end gates

**Entry:** V03. **Files:** `tests/e2e/test_usb_live_web.py`, fixtures.

- Run the live matrices with the bridge in the loop over `serial://COM13`:
  60 s UART zero-drop, p99 isolated byte, SPI/I2C 10 kHz, decoded-pattern
  stop, compact events; assert the browser feed receives the same event
  count as `la-cli live` and that the device returns to IDLE.
- Re-run the USB fault tolerance: 1 MHz SPI overload recovers <= 3 s while
  the bridge serves; close/reopen the bridge does not wedge the port.

**Exit:** `build/reports/usb-v04-live-web.md` with counts and captures.

### V05 — Documentation and packaging

**Entry:** V04. **Files:** `README.md`, `docs/using-the-analyzer.md`,
`docs/windows-dev.md`, `docs/architecture.md`, `docs/limits.md`,
`docs/test-plan.md`, `web-artifact/` where the architecture is described,
`tools/make_release.py`, `pyproject.toml`.

- USB-first quick start (build/load the USB profile, run the host live view),
  gateway as the optional wireless profile.
- Packaging: include the host bridge in the wheel; keep the ESP32 images
  optional/labeled; make `boot` profile-aware so the SD image can carry the
  USB application (`la_app_usb`) with the same FSBL/bitstream.
- Update the portal/manuscript only where the transport/default claim
  changes; keep the measured evidence labels.

**Exit:** `build/reports/usb-v05-docs.md`; no present-tense claim beyond
measured evidence.

### V06 — Release

**Entry:** V05. **Files:** packaging tools/docs as needed.

- `boot` with the USB profile, `package`, `release`; verify the selected ELF,
  FSBL/bitstream match, hashes and manifest from actual outputs.
- `[HUMAN]` SD boot of the USB-profile image (JP5=SD): enumeration, PING,
  representative burst and live gates, and the host live view against that
  image.
- Tag/publish only under explicit release authorization.

**Exit:** `build/reports/usb-v06-release.md`; SD boot recorded as
PASS/BLOCKED, never assumed.

## 5. Verification commands

```
python tools/run.py test-native
python tools/run.py test-host
python tools/run.py release
python tools/run.py fw-zynq -- --profile usb
python tools/run.py fw-zynq
python tools/run.py jtag-zynq -- --profile usb
python tools/run.py test-e2e -- --target serial://COMx -k test_usb
python tools/run.py test-e2e -- --target serial://COMx -k "live"
python tools/run.py boot
python tools/run.py package
```

## 6. Stop, rollback and handoff

- The rollback is the current adopted state: gateway default build, USB
  profile available, all U00-U09 evidence intact. No step may delete the
  gateway profile or its web UI.
- If the host bridge blocks (dependency, browser policy, port conflict),
  record the exact blocker and keep `la-cli live` as the supported path.
- Update `docs/status.md` at the end of every packet with the date, files,
  command, PASS/FAIL/BLOCKED, evidence path and next action.
