# Unified analyzer UI — implementation instructions

User-directed scope (2026-09-20): one local browser UI that runs the whole
analyzer interactively — connection, burst capture with a Saleae-style waveform
viewer, live protocol decoding, host-side decoding of captures, exports, trace
and the pattern generator — instead of the current split between `la-cli`
plus external PulseView and the three-pane live page. This document is the
execution package for that work. `docs/plan.md` remains the normative overall
plan; `docs/live-web.md` remains the frozen contract for the existing host
bridge, which this package extends rather than replaces.

**Planning only.** This document records the frozen decisions and the ordered
packets. No implementation is authorized until the user issues an execution
instruction.

## Implementation status (2026-09-20)

- **W00 complete**: the scope freeze is in `docs/plan.md` and `docs/status.md`.
- **W01-W07 complete locally**: `host/la_ui/` (store, decimation, native
  decode, jobs, exports, offline open, HTTP API), the workspace page
  (`static/{index.html,app.js,waveform.js,timeline.js,style.css}`), the
  `la-ui` / `la-cli ui` entry points and the `la-ui`/`la_ui` packaging.
  Verification: host suite 166 passed, `node host/tests/page_check.mjs` PAGE
  CHECK OK, real headless-Chrome CDP smoke with zero console errors. The
  packet-by-packet evidence is in `docs/status.md`.
- **W08 complete locally**: README/using/architecture/windows-dev/test-plan
  updates, the wheel check for `host/la_ui` and the `la-ui` entry point, and
  the portal `unified-ui` component (validate 79 components / 186 coverage
  entries, build PASS).
- **W09 complete on the bench**: `test-e2e -k ui` PASS 6/6 over
  `serial://COM13` (64k capture with exports and offline reopen, host decode,
  per-slot live with auto-stop, trace dump, 1M-sample capture in 1.58 s), the
  V04 regression 8/8 after the disconnect slot-cleanup fix, and a real
  headless-Chrome bench smoke with zero console errors. The **[HUMAN]**
  visual click-through remains open and is not claimed as passing; the USB
  BOOT.BIN SD boot was bench-verified on 2026-09-26.

The device-side boundary held: no firmware, PL, register-map or protocol file
was modified by any packet.

## 1. Outcome

- New entry point `la-ui` (and `la-cli ui`, one implementation) starts a
  stdlib-only HTTP server on `127.0.0.1:8080` that serves a single-workspace
  browser UI. The UI owns the analyzer target for its lifetime (one process,
  one serial port).
- The workspace covers, in one screen: connect/disconnect with a port list,
  ping/status counters, burst capture configuration and control, a waveform
  viewer for the captured signals, live decoded UART/SPI/I²C events on a
  time-aligned timeline with per-slot start/stop, a pattern-generator
  selector, trace dump/timeline, and exports.
- Captures can be decoded on the host (UART/SPI/I²C) and shown as annotation
  rows under the captured signals, reusing the existing native C decoders.
- The transport is direct USB only (`serial://COMx`, the V00 product default).
  The ESP32/gateway path is rejected as a UI connection; no `LINK_CFG`
  control exists in the UI.
- The existing three-pane live page remains available at `/legacy` on the
  same server; `la-web` keeps serving it exactly as today.
- `la-cli` stays unchanged as the scripted/automation and test interface.

## 2. Frozen decisions

These were settled with the user before this document; they are not open.

| Topic | Decision |
|---|---|
| UI form | Local browser page served by the Python host; no desktop shell |
| Frontend stack | Hand-written vanilla JS + Canvas; no npm, no CDN, no build step |
| Waveform features | Zoom/pan + time ruler + cursor readout; channel labels/colors/reorder/height; dual cursors A/B with measurements; protocol overlays on captures; trigger marker + pre/post boundary; edge/glitch annotations and search; save/load view state and screenshots |
| Live content | Decoded events only (no raw waveform while streaming); no firmware/protocol change |
| Live display | Time-aligned event timeline (not a raw signal waveform) |
| Capture decoding | Yes, host-side, in the first version, via the existing native C decoders |
| Capture size target | Up to 1M samples smoothly; larger allowed with a warning and decimation |
| Device support | Direct-USB Zynq profile only (`serial://COMx`) |
| UI functions | Connect/disconnect + ports; ping/status/counters; capture config + arm + abort + waveform; live decoder config + per-slot start/stop; patgen; export; trace; link selection (dropped, see below) |
| Layout | Single unified workspace |
| CLI relationship | Keep `la-cli`; add `la-ui` as the new interactive front end |
| Trigger scope | Exactly today's PL trigger (level mask/value, any-rise, any-fall, timeout, pre/post); no RTL/firmware change |
| Export path | Browser download of host-generated files |
| Offline viewing | Yes: open saved `.sr`, `.vcd`, `.csv` without a device |
| Concurrent captures | One capture at a time; a new capture replaces the view |
| Legacy page | Kept as a fallback, served at `/legacy` on the same server |
| Verification | Host tests + `[HUMAN]` browser smoke + bench E2E |
| Entry point | New `la-ui` console script and `la-cli ui` |
| Link selection | Removed: direct USB has no selectable link and the device NAKs `LINK_CFG` (U04 gate `test_usb_link_cfg_rejected`) |
| Trace display | Time-aligned timeline canvas like `la-cli trace` |
| Browser | Chrome/Edge only |
| Theme | Dark default |
| Live history | Bounded by event count (default 100,000 events per decoder, drop counted) |
| Capture store | Host session memory only; files written only on export |
| Open formats | `.sr` (sigrok), `.vcd`, `.csv` |
| Waveform PNG | Client canvas snapshot; the host matplotlib PNG stays for the CLI |
| Mode switching | Auto-stop the other mode (live ⇄ capture) before starting |
| CSV export | Two kinds: decoded-event CSV and raw-sample CSV |
| Capture decode trigger | Explicit **Decode** button; specs editable for the capture |
| Captures > 1M samples | Allowed with a warning; still rendered with decimation |

## 3. Boundaries and non-goals

- **Host-only change.** No firmware, PL/RTL, register-map or wire-protocol
  change. `docs/protocol.md` and `docs/regmap.yaml` are not edited. If any
  packet discovers that a device change is required, stop, record the blocker
  and ask; do not modify the device.
- **No ESP32/gateway UI.** The gateway profile, its `la_web` server and the
  ESP32 build stay in the repository untouched, but the UI does not connect to
  them and no link selector is shown.
- **No live raw waveform.** Live streaming carries decoded events only. A
  rolling transition/waveform stream is a future device-side project and is
  explicitly not in this package.
- **No advanced triggers.** Only the existing PL trigger fields are exposed.
- **No new runtime dependencies.** Python standard library only, as in
  `docs/live-web.md` section 1. `numpy`, `pyvcd` and the other pinned packages
  are already project dependencies and may be used.
- **Single client, loopback only.** One SSE feed client and one owner of the
  analyzer port; `--bind` to a non-loopback address stays an explicit opt-in.
- **No native project format.** Only `.sr`, `.vcd` and `.csv` are opened.
- Chrome/Edge only; no Firefox-specific work.

## 4. Baseline (do not regress)

- `python tools/run.py test-host`: 115 passed at the planning baseline.
- `python tools/run.py test-native`: 18/18; `release`: 22/22.
- The V04 USB live-web suite: `test-e2e -- --target serial://COM13
  --live-seconds 60 -k usb_live_web` 8/8 against the V06 image.
- The legacy page contract (`docs/live-web.md`): `/events` SSE payload,
  `/status` fields, `/control/start|stop` semantics and the single-client
  `409` policy are unchanged; the existing `host/tests/test_live_web.py`
  must keep passing.
- Export parity: `.sr`/`.vcd` writers in `host/la_cli/convert.py` are reused
  unchanged; exported `.sr` files still open in PulseView 0.4.2.
- Measured transport facts: USB drain 1.62 MB/s; the capture transfer
  verifies chunk order, byte count and CRC32 in `AnalyzerClient.capture`.
- The V09 per-slot live behavior and the "session states its slot set
  explicitly" firmware interaction are preserved.

## 5. Architecture

### 5.1 New host package `host/la_ui/`

| File | Responsibility |
|---|---|
| `store.py` | `CaptureStore`: session-memory captures keyed by id; metadata (encoding, sample rate, sample count, trigger index, word count), packed sample bytes, lazily built decimation envelopes, decoder annotations |
| `render.py` | Server-side decimation: per-channel min/max envelopes over power-of-two sample buckets; windowed raw-detail slices; no full expansion in memory (streams `iter_samples`) |
| `decode.py` | `ctypes` wrapper over `build/native/la_native.dll` (same ABI as `tests/native/test_ctypes_models.py`): UART/SPI/I²C init/feed/advance/discontinuity; transition generation from raw and transition-encoded captures; bounded event lists |
| `jobs.py` | Single background job runner (`capture`, `decode`, `trace`, `export`) with a state machine, progress, cancellation and one-job-at-a-time policy; owns device access under a lock |
| `export.py` | Reuses `host/la_cli/convert.py` for `.sr`/`.vcd`; CSV writers for decoded events and raw samples; serves files for browser download |
| `open.py` | Offline openers: `.sr` (zip metadata + `logic-1-1`), `.vcd` (pyvcd reader), `.csv` (event or sample detection) |
| `server.py` | `UiServer` built on `host/la_live_web/server.py`: reuses the event hub, SSE feed and `/control/*`; adds the `/api/*` routes, the `/` page and the `/legacy` page |
| `static/index.html` | Workspace shell |
| `static/app.js` | Connection/session state, API client, panels, mode switching |
| `static/waveform.js` | Canvas waveform renderer |
| `static/timeline.js` | Live event timeline and trace timeline renderer |
| `static/style.css` | Dark theme |

The entry point is a new `la-ui` console script plus `la-cli ui`, both calling
the same implementation in `host/la_ui/server.py`. `la-web` is unchanged.

### 5.2 HTTP routes

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | Unified workspace page |
| GET | `/legacy` | Existing three-pane page (from `host/la_live_web/static/index.html`) |
| GET | `/events` | SSE live feed (unchanged contract) |
| GET | `/status` | Bridge snapshot (unchanged fields, extended with UI state) |
| POST | `/control/start` / `/control/stop` | Per-slot live start/stop (unchanged, `?slot=N`) |
| GET | `/api/ports` | Serial port list |
| POST | `/api/connect` / `/api/disconnect` | Open/close the single-owner target |
| GET | `/api/ping` / `/api/status` | Device identity and STATUS snapshot |
| POST | `/api/patgen` | Latch a `PATGEN_CFG` mode |
| POST | `/api/capture` | Start a capture job (CONFIG + ARM + collect) |
| GET | `/api/jobs/<id>` | Job state, progress, error |
| POST | `/api/jobs/<id>/cancel` | Abort a capture / cancel a job |
| GET | `/api/captures/<id>/meta` | Capture metadata for the viewer |
| GET | `/api/captures/<id>/envelope?buckets=N` | Decimated per-channel min/max envelope |
| GET | `/api/captures/<id>/samples?start=&count=` | Exact sample window (zoomed detail) |
| POST | `/api/captures/<id>/decode` | Start a host decode job with decoder specs |
| GET | `/api/captures/<id>/export?format=sr\|vcd\|csv-events\|csv-samples` | Download |
| DELETE | `/api/captures/<id>` | Drop a capture from the session store |
| POST | `/api/open` | Upload an `.sr`/`.vcd`/`.csv` file for offline viewing |
| POST | `/api/trace/start` / `/api/trace/stop` | Trace ring control |
| GET | `/api/trace` | Trace dump rendered as timeline JSON |
| other | | `404` |

- Default bind `127.0.0.1`, default port `8080`; the bind is exclusive.
- Envelopes and sample windows are served as compact binary (`application/
  octet-stream`) consumed by `Float32Array`/`Uint8Array` views; JSON is used
  for metadata, jobs and annotations.
- A second job while one runs returns `409`; the page disables the conflicting
  controls.

### 5.3 Workspace layout

```
┌───────────────────────────────────────────────────────────────────────┐
│ [COM13 ▾] [Connect]  ● linked  fw 0x00090000  pl 0x4C410002   [Ping] │
├───────────────┬───────────────────────────────────────────────────────┤
│ Connection    │  Waveform / Timeline canvas                           │
│  ports, ping  │   time ruler                                          │
│               │   CH0..CH7 rows                                       │
│ Capture       │   decoder annotation rows (capture decode)            │
│  rate, words, │   cursors A/B                                         │
│  pre, mode,   ├───────────────────────────────────────────────────────┤
│  trigger,     │  Live panes: slot 0 | slot 1 | slot 2 (timeline +     │
│  patgen,      │  per-pane Start/Stop, state pill, bounded history)    │
│  [Arm][Abort] ├───────────────────────────────────────────────────────┤
│               │  Trace timeline (collapsible)                         │
│ Export        │                                                       │
│  .sr .vcd     │                                                       │
│  .png .csv    │                                                       │
│  [Decode]     │                                                       │
├───────────────┴───────────────────────────────────────────────────────┤
│ status bar: session state, events, drops, warnings, page/server ver   │
└───────────────────────────────────────────────────────────────────────┘
```

- Dark theme; the page carries a `page vXX | server vXX` marker (the V09
  lesson) so a stale cached page is visible.
- Settings (target, capture config, channel names/colors/order/heights, last
  decoder specs, cursor positions, zoom) persist in `localStorage`.
- Mode switching: starting a capture while live runs sends `LIVE_STOP`, waits
  for IDLE, then arms; starting live during an armed capture sends `ABORT`
  first. Both transitions are reported in the status bar.

### 5.4 Waveform renderer

- Canvas 2D, one row per channel plus one row per decoded slot.
- Zoom: mouse wheel anchored at the cursor; pan: drag; `Fit` and `Zoom
  selection`; keyboard step/edge navigation.
- Time ruler with adaptive units (ns/µs/ms/s) derived from the sample rate.
- Dual cursors A/B: drag or click placement; readout of tA, tB, Δt, 1/Δt and
  pulse width/period between the cursors on the selected channel.
- Channel rows: name, color, vertical order and height (persisted); 1/0 levels
  and per-sample transitions; a min/max envelope when a pixel covers many
  samples.
- Trigger marker (from `TRIG_INDEX`) and the pre/post-trigger boundary.
- Edge search: next/previous edge on a channel; glitch annotation when a pulse
  is shorter than the current pixel; search for a level pattern.
- Protocol annotations: decoded events drawn as boxes/labels on the slot row,
  aligned to their `t_sample`, with hover details.
- Screenshot: the visible canvas is exported as a PNG download.

### 5.5 Data budget and decimation

- Target: 1M samples. Raw capture is 1 MB; a worst-case transition-encoded
  capture is 4 MB of words; both are held in host memory once and served on
  demand.
- The page requests an envelope of roughly one bucket per horizontal pixel
  (e.g. 2,048 buckets for a full-width view) and exact sample windows when
  zoomed in (bounded, e.g. ≤ 65,536 samples per request).
- Server decimation streams `iter_samples` (never expands an RLE capture in
  memory) and caches the envelope per bucket count.
- Captures above 1M samples are accepted with a visible warning; the viewer
  still renders from the envelope and the page reports when a detail request
  is clamped.

### 5.6 Host decoding of captures

- Decoder specs reuse the CLI grammar: `uart:rx=0,baud=115200`,
  `spi:sck=0,mosi=1,cs=2,...`, `i2c:scl=0,sda=1`.
- The decode job walks the capture's transitions once per configured slot and
  records events (kind, channel, flags, t_sample, data) with the same kinds as
  `docs/protocol.md` (`BYTE`, `FRAME_ERR`, `PARITY_ERR`, `START`, `STOP`,
  `ADDRESS`, `ACK`, `NACK`, `CS_ASSERT`, `CS_RELEASE`, `DISCONTINUITY`).
- The decoder library is resolved from `LA_NATIVE_LIB`, else
  `build/native/la_native.dll`. If it is missing, the Decode button reports a
  clear error (run `python tools/run.py test-native` to build it); no silent
  fallback to a second implementation.
- Decode runs only on an explicit **Decode** click and is bounded by a job
  state with progress.

### 5.7 Offline open

- `/api/open` accepts an uploaded file (file picker or drag-and-drop) and
  parses it server-side:
  - `.sr`: zip with `metadata` + `logic-1-1` (the project's own export);
  - `.vcd`: parsed with the already-pinned `pyvcd` reader;
  - `.csv`: event CSV or sample CSV, detected from the header.
- An opened file becomes a normal capture in the store, so the waveform,
  decode, measurement and export paths are identical to a device capture.
- No server-side path typing.

### 5.8 Live timeline

- Each slot pane renders its decoded events on a shared time axis derived
  from `t_sample` and the configured sample rate.
- Bounded history: 100,000 events per slot (configurable constant); when the
  bound is reached the oldest are dropped and the pane's drop count increments.
- Per-slot Start/Stop, state pills and `Stop all` keep the V09 semantics; the
  session still states its slot set explicitly on start.
- Overrun reports are counted and shown in the status bar, never silently
  dropped.

### 5.9 Trace

- `Trace start/stop/dump` uses the existing `TRACE_CTRL` chunked dump through
  `AnalyzerClient.trace_dump`.
- The dump is parsed into the same timeline model as `host/la_cli/trace.py`
  and rendered on a canvas with zoom/pan; the CLI keeps its matplotlib PNG.

## 6. Ordered work packets

### W00 — Freeze scope and record decisions

**Entry:** this document reviewed. **Files:** `docs/unified-ui-plan.md`,
`docs/status.md`, `docs/plan.md`.

- Record the dated decision and the table in section 2; state that this is a
  host-only change and that the gateway path is rejected for the UI.
- Add the pointer from `docs/plan.md`.

**Exit:** the three documents updated; no code change. Evidence: the ledger
entry and the planning commit.

### W01 — Capture store, decimation and decode service

**Entry:** W00. **Files:** new `host/la_ui/{__init__,store,render,decode}.py`,
new `host/tests/test_ui_store.py`, `host/tests/test_ui_decode.py`.

- Implement the store, the streaming envelope/detail server and the ctypes
  decoder wrapper with the same ABI as `tests/native/test_ctypes_models.py`.
- Tests: raw and txn captures; envelope min/max correctness against a
  Python reference; detail windows; exact sample counts; UART/SPI/I²C decode
  of model-generated captures matching the Python models; missing-DLL error
  path; 1M-sample envelope build within a recorded budget.

**Exit:** `python tools/run.py test-native` (builds the DLL) then
`python tools/run.py test-host` green; no new dependency.

### W02 — Job runner, HTTP API and exports

**Entry:** W01. **Files:** new `host/la_ui/{jobs,export,server}.py`, new
`host/tests/test_ui_api.py`, `host/la_cli/commands.py` (`ui` command),
`pyproject.toml` (`la-ui` script, package data).

- Implement the one-job runner, the `/api/*` routes, the exclusive bind, the
  legacy `/legacy` route and the download endpoints.
- Tests with the existing mock analyzer: connect/ping/status, capture job
  lifecycle, cancel, `409` on a second job, envelope/detail/meta responses,
  export bytes for `.sr`/`.vcd`/CSV (including a `.sr` zip round-trip), the
  `/legacy` page, and unchanged `/events`, `/status`, `/control/*`.
- Verify the existing `host/tests/test_live_web.py` still passes unchanged.

**Exit:** `test-host` green including the legacy suite; `python -m compileall
-q tools host tests` clean.

### W03 — Workspace shell

**Entry:** W02. **Files:** new `host/la_ui/static/{index.html,app.js,style.css}`,
`host/tests/test_ui_page.py`.

- Implement the layout of section 5.3: connection bar, side panels, canvas
  region, live panes, status bar, dark theme, page/server version marker,
  `localStorage` settings, mode switching.
- Page-logic tests use the established Node approach (`node --check` plus a
  DOM-stub run, as in the V08/V09 evidence scripts) and the host API against
  the mock analyzer.

**Exit:** `node --check` on every script; `test-host` green; a served-page
DOM-stub run reports the workspace controls bound.

### W04 — Waveform renderer

**Entry:** W03. **Files:** `host/la_ui/static/waveform.js`, `app.js`,
`style.css`, `host/tests/test_ui_page.py`.

- Implement zoom/pan, ruler, cursors A/B with measurements, channel
  labels/colors/reorder/height, trigger marker and pre/post boundary, edge
  search/glitch annotation, annotation rows and the PNG screenshot.
- Rendering targets (to be measured and recorded, not assumed): envelope
  redraw within 33 ms at 1M samples; wheel/drag interaction stays responsive
  with the live feed running.
- Tests: DOM-stub checks for control wiring and state persistence; the
  renderer math (time↔pixel mapping, bucket selection, cursor measurement)
  is unit-tested in Node with fixed fixtures.

**Exit:** Node tests PASS; `test-host` green; a [HUMAN] visual check is
scheduled with W09.

### W05 — Capture decode overlays and CSV exports

**Entry:** W04. **Files:** `host/la_ui/static/waveform.js`, `app.js`,
`host/la_ui/server.py`, `host/tests/test_ui_api.py`,
`host/tests/test_ui_page.py`.

- Wire the Decode button to the decode job and draw annotation rows aligned
  to `t_sample`; hover details; error surface when the DLL is missing.
- Wire both CSV exports (decoded events, raw samples) and the `.sr`/`.vcd`
  downloads.
- Tests: decode job over the mock store; annotation payload shape; CSV row
  contents for both kinds; a capture decoded end-to-end from a model vector.

**Exit:** `test-host` green; the exported `.sr` of a decoded capture opens in
PulseView (re-checked at W09 on the bench).

### W06 — Trace timeline, patgen and status

**Entry:** W05. **Files:** `host/la_ui/static/timeline.js`, `app.js`,
`server.py`, `host/tests/test_ui_api.py`.

- Implement trace start/stop/dump and the canvas timeline (task switches,
  queue/stream activity, app markers) with zoom/pan.
- Wire the patgen selector (all V10 modes) and the status/counter panel
  (state, dropped segments, latency, watermarks) with a poll while connected.
- Tests: trace JSON from a recorded dump fixture; patgen request payloads;
  status rendering with the mock analyzer.

**Exit:** `test-host` green; a bench trace dump is recorded at W09.

### W07 — Offline open

**Entry:** W06. **Files:** `host/la_ui/{open,server}.py`,
`host/tests/test_ui_open.py`.

- Implement the `.sr`/`.vcd`/`.csv` openers and the `/api/open` upload route;
  an opened file becomes a store capture with full viewer/decode/export
  behavior.
- Tests: round-trip a host-written `.sr` and `.vcd`; parse a CSV event log and
  a CSV sample log; malformed files return a clear error; no device is needed.

**Exit:** `test-host` green; opening a capture exported in an earlier step
renders without a device.

### W08 — Documentation, packaging and regression

**Entry:** W07. **Files:** `README.md`, `docs/using-the-analyzer.md`,
`docs/windows-dev.md`, `docs/architecture.md`, `docs/test-plan.md`,
`docs/live-web.md` (legacy note only), `pyproject.toml`,
`tools/make_release.py`, `web-artifact/` where the host tooling is described.

- Document `la-ui`: quick start, the workspace controls, the USB-only scope,
  the legacy page, the decode/DLL requirement and the browser requirement.
- Packaging: ship `host.la_ui` and its static files in the wheel; verify both
  `la-ui` and `la-cli` entry points in the release check.
- Regression: `test-host`, `test-native`, `release`, and the V04 USB live-web
  suite 8/8 against the current USB image.

**Exit:** `build/reports/ui-w08-docs.md`; packaging check PASS; no present-
tense claim beyond measured evidence.

### W09 — Bench E2E and browser smoke

**Entry:** W08. **Files:** new `tests/e2e/test_ui.py`,
`build/reports/ui-w09-*.md`.

- Over `serial://COM13` with the current USB image, run the UI server
  headlessly and drive the API: connect/ping/status; a 1M-sample patgen
  counter capture (exact count/CRC); envelope/detail fetches; a UART patgen
  capture decoded to the expected bytes; three live slots started/stopped
  independently; trace dump; both CSV downloads; offline reopen of the
  exported `.sr`.
- `[HUMAN]` browser smoke: open `http://127.0.0.1:8080`, connect, capture,
  zoom/pan/cursors, Decode, start two live panes and stop one, trace view,
  export, reopen the exported file with the device disconnected, hard-refresh
  for the version marker.
- Record every step as PASS/BLOCKED with evidence paths; a missing board,
  port or browser step is BLOCKED, never PASS.

**Exit:** `build/reports/ui-w09-bench.md` and the [HUMAN] record; `release`
and `package` re-run after the final change.

## 7. Verification commands

```
python tools/run.py test-native
python tools/run.py test-host
python tools/run.py test-e2e -- --target serial://COM13 -k usb_live_web
python tools/run.py test-e2e -- --target serial://COM13 -k ui
python tools/run.py release
python tools/run.py package
python -m compileall -q tools host tests
node --check host/la_ui/static/app.js
node --check host/la_ui/static/waveform.js
node --check host/la_ui/static/timeline.js
git diff --check
```

The legacy regression suite and the V04 USB live-web suite must stay green.
The bench target is `serial://COM13` with the current V06/V10 USB image; the
[PHYSICAL] browser smoke is separate from the automated API run.

## 8. Risks, stop conditions and review items

- **Stop and report** if any packet needs a firmware, PL, register or protocol
  change, or if the direct-USB link cannot carry a required stream.
- **Page complexity:** the renderer is the largest single artifact; the
  packet order keeps it behind a tested API and a shell.
- **Decode cost:** a 1M-sample decode runs in a background job; the page must
  stay usable and report progress. If host decode is too slow to be useful,
  record the measurement rather than hiding it.
- **Port ownership:** `la-ui` and `la-cli`/`la-web` cannot share the port; the
  UI must report the open failure honestly, as `docs/live-web.md` requires.
- **Memory:** session-only captures; the page warns above 1M samples; DELETE
  drops a capture.
- **Review items decided in this document** (confirm at review, all reversible
  without redesign): offline open uses a browser file upload rather than a
  server-side path; route names under `/api/`; binary envelope/detail
  transport; DLL resolution order with an explicit error; side-panel sections
  instead of mode tabs; default port 8080 with no auto-open; 100,000-event
  live bound; 1M-sample warning threshold.

## 9. Rollback and handoff

- Rollback is trivial: the package is host-only. Removing the `la-ui` entry
  point and the `host/la_ui/` package returns the repository to the current
  state; `la-cli`, `la-web` and the legacy page are untouched throughout.
- The gateway profile, its web UI and all device evidence remain valid and
  are not modified by any packet.
- Update `docs/status.md` at the end of every packet with the date, files,
  exact command, PASS/FAIL/BLOCKED, evidence path and next action.
