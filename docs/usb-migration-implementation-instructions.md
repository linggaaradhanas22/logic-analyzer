# Direct Zynq USB migration implementation instructions

Prepared: 2026-09-17. Baseline: `b30adfd` on `main`.
Status at preparation: planning package only. **Current status (18 October
2026): USB implemented/adopted; this is the historical U00-U09 packet record.**
Resume from [current-handover.md](current-handover.md), not U03 bring-up. W11
firmware 0x000A0000 / UI w11 now include real live waveform preview and Reset UI;
the new image is installed and cold-boot verified on SD. Current evidence and
source/publication boundaries are in [status.md](status.md).

## 1. Outcome and authorization

Evaluate direct Zybo Z7-10 PS USB0 CDC-ACM as the primary wired analyzer
transport, then adopt it only after the required evidence and scope decision.
The user authorized this planning package. That does not authorize firmware
implementation, loading hardware, retiring the gateway, publication or release.
At the next session, use the user's then-current instruction to establish the
authorized work packets; record it in [status.md](status.md). Do not ask again
for work already authorized in that session. `[HUMAN]` denotes required physical
action/observation, not a result an agent may infer from a local build.

The candidate preserves acquisition, ordinary SG DMA, protocol framing, decoders,
trace and host exports. Wireless, mDNS and the ESP-hosted live WebSocket view are
absent from the direct connection. The static `web-artifact/` documentation portal
is a separate product and remains usable. Adding a USB network class or porting
the live browser server is outside this migration unless explicitly added later.

## 2. Read first and resolve authority

Read in this order before editing:

1. `AGENTS.md`, [status.md](status.md), [plan.md](plan.md).
2. This guide and [usb-link.md](usb-link.md), including the corrections to the
   initial `usbpsu`, DWC2, one-cable and performance assumptions.
3. [architecture.md](architecture.md), [rtos-design.md](rtos-design.md),
   [protocol.md](protocol.md), [wiring.md](wiring.md), [spi-link.md](spi-link.md).
4. [test-plan.md](test-plan.md), [limits.md](limits.md),
   [using-the-analyzer.md](using-the-analyzer.md) and the source/test map below.

`plan.md` remains normative. This guide is its proposed USB work breakdown;
`usb-link.md` is the proposed interface contract. Neither silently supersedes
an existing requirement. `status.md` records evidence and the next authorized
packet. Preserve historical PASS/FAIL entries. On contradiction, identify the
specific requirement and resolve it in the ledger before dependent work.

Use packet IDs U00-U09 alongside existing P0-P10 IDs. Do not renumber completed
phases or claim P9.1 SPI passes because direct USB works. Adoption can explicitly
supersede the SPI requirement for the wired profile, while retaining its history.

## 3. Current baseline and scope effects

The latest 2026-09-17 ledger records working S3 CDC host access on a direct PC
port and a bit-exact UART+CDC burst, but every-other-byte corruption on the
S3 SPI path. Older rows saying S3 enumeration is pending are historical. The
current generated Zynq BSP already includes `usbps`; this is evidence for a
firmware-first experiment, not proof of device-mode operation. Detailed local
paths and external primary sources are in `usb-link.md`.

| Existing area | Treatment during evaluation | Treatment at adoption |
|---|---|---|
| P3 PL / XSA | Reuse identified artifacts after verifying USB configuration and provenance | Rebuild only if a demonstrated hardware configuration change requires it |
| P4/P7/P8 acquisition, RTOS and decode | Preserve ownership and existing acceptance requirements; add USB session cleanup | Re-run affected gates on the final USB build |
| P5 ESP32 and P9.1 SPI | Retain sources and gateway profile; do not require fixing S3 SPI to evaluate USB | Record whether optional, deferred or superseded; never relabel the SPI fault as fixed |
| P9.2 compact events | Keep compact and original event formats | Verify both over USB |
| P9.3 200 MS/s | Remains separate stretch decision; 100 MS/s baseline | USB throughput does not prove faster acquisition |
| P9.4 Wi-Fi | Separate optional-profile work | Explicitly retain or defer, with rationale |
| P10.1 live web view | Still ESP-hosted; no direct-USB replacement promised | Decide its optional-profile status or acknowledged removal |
| P10.2 sigrok | Existing deferral and file export behavior remain | CDC `la_proto` is not a SUMP device and does not itself remove the deferral |
| P10.3 portal, paper and BOM | Record affected locations; keep current public claims until evidence exists | Reconcile all affected content with the chosen product scope |
| P10.4 release | Existing release hold continues | Require selected-profile packaging, final SD boot and release decision |

## 4. Source and test map

Paths below are relative to the project root; new files are explicitly marked.
Resolve root from each script's location and keep outputs beneath this project.

| Area | Files to inspect/change when its packet is authorized |
|---|---|
| Device driver | **New:** `fw-zynq/app/src/drivers/la_usb_dev.{h,c}`; inspect existing `la_irq.{h,c}`, `la_link_uart.{h,c}` and the installed `usbps_v2_9` source |
| USB service | **New:** `fw-zynq/app/src/tasks/la_usb_task.c`; static object creation and selected transport in `fw-zynq/app/src/la_app.{h,c}` and `main.c` |
| Testable USB logic | **New as needed:** portable descriptor/request/session/queue core under existing `drivers/` or `live/`; use unique filenames because the Vitis import is flat |
| Frame routing | `fw-zynq/app/src/tasks/la_tx_task.c`, `la_cmd_task.c`, `la_cmd_dispatch.{h,c}`, `la_tx_win.{h,c}`, `la_tx_pack.{h,c}`; inspect SPI task/core without copying its ISR exception |
| Memory and build | `fw-zynq/lscript.ld`, `translation_table.S`, `app/src/la_mem_geom.{h,c}`, `fw-zynq/scripts/la_platform.py`, `app.py`, `tools/run.py` |
| Native verification | `CMakeLists.txt`, `tests/native/`, `fw-zynq/tests/host/`; add portable USB tests in the existing CTest flow |
| Host compatibility | `host/la_cli/transport.py`, `client.py`, `commands.py`, `host/tests/`; changes only when a reproduced compatibility issue requires them |
| Bench tests | **New:** `tests/e2e/test_usb_transport.py`; adapt `tests/e2e/conftest.py`; reuse assertions from burst/live/trace tests without selecting SPI |
| Packaging | `tools/make_release.py`, `fw-zynq/scripts/make_bootbin.py`, `bootgen.bif`; inspect actual ELF naming rather than copying older prose |

No edits to generated register files. If a register change becomes necessary,
justify it separately, edit `docs/regmap.yaml` and run the generator. No expected
register or RTL change is part of this plan. Do not touch the neighboring DDR3 PHY.

## 5. Ordered work packets

Every packet begins with a ledger entry for scope/files/checks and ends with
evidence, blockers, next action and the repository's commit/push workflow.
Independent local checks may proceed while a physical prerequisite is pending;
dependent packets may not skip their entry gates.

### U00 — Establish the execution baseline

**Entry:** user has authorized the relevant implementation/evaluation scope.

- Follow `status.md` synchronization steps: fetch, inspect branch/worktree/log,
  fast-forward only when appropriate, and preserve unrelated changes.
- Record the current commit, board/tool locks, firmware/bitstream/XSA identity,
  open blockers and which packets are authorized. Existing untracked
  `docs/hardwarex/` and `docs/learning/` were present at planning time; do not
  accidentally stage them.
- Save known-good artifact hashes and copies below `build/usb-migration/baseline/`
  before a firmware build recreates `build/vitis/ws`. Record a rebuild route
  from the baseline commit if ignored artifacts are unavailable.
- Run relevant off-board baseline checks: `test-native` and `test-host`.
  Do not flash as part of the baseline.

**Exit:** dated baseline and rollback identity recorded; local checks pass or
their exact pre-existing failures are isolated. Evidence:
`build/reports/usb-u00-baseline.md` plus canonical reports.

### U01 — Confirm board and platform feasibility

**Entry:** U00. **Files:** inspect preset/build scripts, actual XSA and generated
BSP; write evidence/ledger only unless a prerequisite repair is justified.

- Confirm `usbps` config, exported headers/library and source version from the
  actual FreeRTOS domain. Recheck USB MIO/reset configuration and boot-time PS
  initialization against the XSA used by the selected bitstream/FSBL.
- Locate installed Chapter 9/storage examples and evaluate the old AMD CDC
  techtip's bare-metal design; record usable portions and license/version limits.
- `[HUMAN]` identify Zybo Z7 revision, J10, J11, JP1/JP2, JP6, power source and
  separate debug path using the board manual. Confirm cable and direct PC port.
  No claim of single-cable power. No board modification is planned.
- Resolve VID/PID/serial identity and record a lawful development/release basis.

**Exit:** a platform evidence table and completed physical checklist, or explicit
blockers. Missing USB in the actual XSA triggers a scoped P3 prerequisite and
its normal checks; it is not permission to rebuild/change PL speculatively.
Evidence: `build/reports/usb-u01-feasibility.md`. Local U02 design can proceed
while the physical checklist is pending; loading U03 cannot.

### U02 — Freeze the integration contract

**Entry:** U01 local evidence available. **Files:** `usb-link.md`, proposed API
notes, native tests/portable helpers only if authorized.

- Resolve endpoint/descriptor bytes, identity, standard/class requests, speed
  fallback, DTR/RTS and ZLP/flush policy.
- Demonstrate a notification-only ISR/service design against `XUsbPs` interrupt
  behavior. Specify interrupt masking/acknowledgement, coalescing, task context,
  priority and race handling within the repository rule; unresolved policy
  conflict blocks device integration rather than granting an exception.
- Specify buffer counts/sizes, handles, queue full policies, cache maintenance,
  task stack/priority, bus lifecycle and bounded cancellation. Model stale
  completions and ownership recovery before writing target glue.
- Define named build profiles and exact build selection mechanism, keeping the
  gateway default. Audit all UART/SPI-specific routing, live-space checks and
  ACK-window behavior. USB rejects `LINK_CFG` without changing transport state.
- Define acceptance selectors/markers separating USB, common, SPI and ESP-only
  tests. Do not weaken a test to make the new profile pass.

**Exit:** no unresolved decision required for U03 coding; contract and native
checks cover boundaries and failure cases. Evidence:
`build/reports/usb-u02-contract.md`. Record any needed authorization decision
with its concrete conflict; do not implement a speculative ISR exception.

### U03 — Build and enumerate a minimal CDC device

**Entry:** U02 and U01 physical/identity gates; firmware/bench scope authorized.
**Files:** new USB driver/task, app creation, firmware build selection and tests.

- Implement CDC control and minimal bounded IN/OUT service in the direct-USB
  build. Use static aligned buffers; keep diagnostics on the debug console.
- Build both gateway and direct-USB profiles with the canonical runner. Confirm
  actual profile and USB driver linkage in the report; do not edit generated BSP.
- `[HUMAN]` load the candidate through the established JTAG procedure, preserving
  the selected PL artifact; confirm Windows binds `usbser.sys` and reports HS.
- Verify descriptors, supported/unsupported class requests, line coding, open/
  close, short packets and exact-packet-multiple transfers with bounded echo or
  controlled test payloads. Keep test echo isolated from analyzer protocol mode.
- Repeat 20 open/close cycles and 10 reset/re-enumeration cycles without a
  driver error, interrupt storm, stack overflow or unrecovered endpoint stall.

**Exit:** enumeration and bidirectional transfers evidenced on the actual board.
Full-speed-only, custom-driver-only or unreliable enumeration does not pass the
target gate. Evidence: `build/reports/usb-u03-enumeration.md` and device descriptor/
host/console records. No bulk throughput claim yet.

### U04 — Integrate the complete analyzer stream

**Entry:** U03 passes. **Files:** TxTask, CmdTask, app transport selection,
USB service/core, native and host tests as needed.

- Feed OUT into the existing parser with one RX producer; send every framed
  response/data/event/trace class through the selected transport.
- Preserve ACK/retry and capture release semantics; apply complete-frame
  admission, bounded pressure and session cancellation from `usb-link.md`.
- Verify PING/STATUS, CONFIG/ARM/ABORT, decoder/event configuration, live stop,
  decoded-pattern stop and trace commands. Check USB `LINK_CFG` rejection.
- Exercise parser split/coalesced frames, garbage/bad CRC recovery, short OUT
  handling, no stale replies after reset, and simultaneous command/data pressure.
- Audit serial writes/reads and timeouts. `SerialTransport.send()` currently
  calls `write()` without checking its returned count, and construction does
  not set `write_timeout`; measure/test required behavior before any change.
  Preserve generic TCP/serial semantics and add host regression coverage for
  each justified fix. No new host transport is required by design.

**Exit:** control and data share the direct USB link correctly; native/host
checks and both profile builds pass. Evidence: `build/reports/usb-u04-protocol.md`.

### U05 — Verify burst, live, trace and failure recovery

**Entry:** U04. **Files:** `tests/e2e/test_usb_transport.py`, fixtures/selectors,
affected portable logic and targeted bug fixes.

- Run the common burst matrices (raw/transition, all patgen modes, short tails,
  multiple ordinary SG descriptors, repeated captures, abort and capacity errors).
  `[HUMAN]` separately verify all eight external channels with the established
  JE-to-JC loopback; internal selftest is not external-input proof.
- Run UART 115200 for 60 s with zero unexplained drops, SPI/I2C 10 kHz baselines,
  compact and original events, decoded-pattern stop, `.sr`/VCD exports and trace.
  Preserve the 25 ms p99 isolated-byte requirement; do not substitute USB packet
  timing or a Zynq-only latency counter for end-to-end evidence.
- Add stall, process termination, COM close, unplug/replug, bus reset, suspend/
  resume, endpoint halt and reset-during-capture cases. Test late completions and
  every buffer returning exactly once. `[HUMAN]` events require observed actions.
- Require IDLE/resource recovery within 3 s after detected host loss/no-progress
  start, consistent with the 2 s timeout plus cleanup. Then reopen, PING and
  capture bit-exactly. Record detection delay separately for physical unplug.
- Keep 1 MHz/10 MHz SPI decode overload cases as loss-accounting/recovery tests;
  do not require artificial drops if USB removes the old bottleneck. Introduce
  controlled overload if needed to verify explicit loss reporting.
- Complete the existing 500-capture soak and review stack watermarks, USB errors,
  queue occupancy, parser/CRC counters and acquisition overflow.

**Exit:** required common/USB gates pass with named selections and counts;
ESP/TCP/SPI-only exclusions are justified, not counted as passed. Evidence:
`build/reports/usb-u05-regression.md`, canonical `test-e2e.json`, captures/traces.

### U06 — Measure the performance gate

**Entry:** U05 correctness and recovery pass. **Files:** dedicated USB drain
test/report; narrowly scoped optimizations only when measurement identifies need.

- Add a USB drain gate that never sends `LINK_CFG(SPI)`. Reuse the known counter
  and CRC assertions from `test_p91_spi.py`; its current drain test selects SPI
  and cannot establish direct-Zynq USB performance unchanged.
- Transfer 64 MiB (67,108,864 bytes) as sixteen 4 MiB raw captures, verifying
  every byte, frame CRC, final DATA_END CRC, sample count and ordering.
- Match the current P9 harness's conservative wall timing: start before the
  repeated capture loop, stop after final host verification. Include command,
  acquisition, transport and host verification time. Divide verified bytes by
  elapsed seconds and 1,000,000 for MB/s. Also report isolated drain timing if
  available, clearly as a different metric.
- Run three consecutive trials; each must reach >=1.5 MB/s with zero corruption,
  capture overflow or unexplained loss. Report all rates, not just the fastest.
  Record HS evidence, build hashes, power/cable/host, queue/CPU/stack counters.
- Profile batching, ACK windows, host read size/timeouts and copying if the gate
  fails. No guessed 20-40 MB/s target, lowered gate or changed protocol window
  without a documented design amendment and affected regression checks.

**Exit:** `build/reports/usb-u06-performance.json` and a readable summary with
raw byte counts/time bounds. Failure leaves adoption blocked by performance;
it does not erase the valid enumeration/correctness results.

### U07 — Record the adoption decision

**Entry:** U03-U06 evidence reviewed and user scope decision available.
**Files:** `plan.md`, `status.md`, `usb-link.md`, `limits.md`.

Present a concrete result: gate table, measured limits, recovery behavior,
power/cable arrangement, code footprint and feature changes. Record whether
direct USB becomes primary, whether the gateway remains supported/experimental,
and the disposition of P9.1, P9.4 and P10.1. Preserve the wired 100 MS/s scope.

**Exit:** dated adopt/defer/reject decision with exact evidence. Adoption updates
the normative baseline; defer/reject returns to the gateway profile and records
USB WIP without deleting its findings. Do not equate an implementation request
with permission to silently remove wireless or browser features.

### U08 — Reconcile technical and public documentation

**Entry:** U07 adoption and final behavior established. **Files:** matrix below.

| Deliverable | Existing home and required update |
|---|---|
| Requirements and roadmap | `docs/plan.md`: selected architecture, baseline transport, RTOS goals, P9/P10 disposition; label root `logic-analyzer-implementation-plan-v0.4.md` as a historical snapshot if it is no longer synchronized |
| Integration and scheduling | `docs/architecture.md`, `docs/rtos-design.md`: USB path, ownership, task/IRQ/cache policy, measured resources |
| Interfaces and wiring | `docs/usb-link.md`, `docs/protocol.md`, `docs/wiring.md`, `docs/spi-link.md`: adopted contract, profile-specific LINK_CFG behavior, data/debug/power ports, retained legacy status |
| Verification and limits | `docs/test-plan.md`, `docs/limits.md`: USB acceptance matrix with evidence; retained unresolved gates |
| Use and build | `README.md`, `docs/using-the-analyzer.md`, `docs/windows-dev.md`: select/build/load profile, identify COM port, capture/live/trace, power and reconnect instructions |
| Documentation portal | `docs/web-artifact-implementation-instructions.md`, `docs/web-artifact-content-review.md`, `web-artifact/lib/docs/` and affected diagrams/routes: architecture, seven-subsystem assumption, component coverage, glossary, roadmap and measured claims |
| HardwareX and BOM | `docs/hardwarex/manuscript.md` and `docs/hardwarex/diagrams/`: title/abstract, wireless claims, RTOS contribution, diagram paths, BOM/power/cables, operation, validation and design-file manifest |

Recheck the existing untracked manuscript/learning content with the user-owned
worktree before editing or staging it. Keep current and historical diagrams
clearly labeled. Verify the portal with its own documented validation/build
commands; publishing follows the then-current user authorization separately.

**Exit:** no present-tense USB claims beyond measured evidence; all architecture,
BOM and feature descriptions agree. Evidence: `build/reports/usb-u08-doc-review.md`.

### U09 — Package and close the release

**Entry:** U08, release scope authorized and all retained release gates resolved
or explicitly dispositioned. **Files:** existing packaging tools/docs as needed.

- Make package requirements profile-aware: direct USB must not secretly require
  ESP images; retained gateway artifacts must be labeled as that profile.
- Run `boot`, `package` and `release` through `tools/run.py`. Verify selected
  ELF, matching FSBL/bitstream, hashes, wheel and manifest from actual outputs.
- `[HUMAN]` boot the final SD image using the documented board procedure;
  repeat enumeration, PING and representative burst/live recovery gates on that
  image. A JTAG-only result does not establish SD boot.
- Tag/publish only under the release authorization. Update the ledger, push
  intended changes and verify remote hashes/worktree as required.

**Exit:** selected-profile artifacts and boot evidence in `build/release/` and
`build/reports/usb-u09-release.md`; remaining unverified hardware stays explicit.

## 6. Verification commands and evidence rules

Use the project Python environment as described in `docs/windows-dev.md`.
If `python` is not on PATH, invoke `.venv/Scripts/python.exe` with the same
`tools/run.py` target; do not install or move an environment outside the project.

The following canonical targets already exist; run those relevant to the packet:

```powershell
python tools/run.py test-native
python tools/run.py test-host
python tools/run.py fw-zynq
python tools/run.py test-e2e -- --target serial://COMx -k test_usb
python tools/run.py boot
python tools/run.py package
python tools/run.py release
```

`COMx` is a placeholder to replace with the observed Zybo device port.
`test_usb` is a **planned selector**: add the U03-U06 tests before running it,
check that required tests are collected and record nonzero pass counts. A
zero-selection or skipped run cannot pass a gate. Profile-selection arguments
do not exist yet; U02 must specify them and U03 must implement/document them
through the runner. Default `fw-zynq` currently builds the existing gateway path.

Record exact common-test selections after the U02 fixture audit. Existing E2E
includes ESP console, Wi-Fi, web and SPI assumptions, so a blanket run is not a
substitute for an explicit applicability matrix. Physical-load commands such
as `python tools/run.py jtag-zynq` are only for the authorized bench packet.
If RTL/PS configuration changes, add `gen`, `lint`, `sim` and `bit` as applicable;
run `lint` and `sim` sequentially. Protocol/decoder changes require `test-native`.

Canonical JSON reports under `build/reports/` may be overwritten. Preserve each
gate's report/log copy under `build/reports/usb-migration/Uxx/` with date, commit,
artifact hashes, command, test selection, counts, PASS/FAIL/BLOCKED and observation.
Keep captures in `build/captures/usb-migration/` and traces in
`build/traces/usb-migration/`. None is staged. Summarize results and exact paths
in `status.md`; another checkout must recreate absent local evidence.

## 7. Stop, rollback and handoff

Stop dependent work on an unresolved driver/platform mismatch, unsafe or unknown
board configuration, missing identity, ISR-rule conflict, descriptor/cache
ownership ambiguity, corruption, missed recovery bound or failed acceptance
gate. Record the narrow blocker and continue only independent authorized work.
Do not lower thresholds, introduce a Linux workflow or claim an unrun test passes.

Rollback means select/rebuild the preserved gateway profile, restore identified
artifacts through the authorized bench procedure, and re-run a baseline ping/
capture. It does not mean reset the worktree, erase USB evidence, force-push or
pretend the known S3 SPI fault is resolved. Keep PL untouched unless the ledger
records a necessary configuration change and its reverse procedure.

Every handoff must state: current packet/substep, authorization scope, exact
commit/profile/artifact identities, changed files, commands and evidence paths,
pending `[HUMAN]` work, failed/unrun checks, rollback identity and one next action.
Follow `status.md` for intended staging, commit, push and remote verification.

Suggested next-agent instruction once execution is authorized:

> Read AGENTS.md, docs/status.md, docs/plan.md,
> docs/usb-migration-implementation-instructions.md and docs/usb-link.md.
> Confirm the current authorized scope and execute the first incomplete eligible
> U packet. Preserve the gateway fallback and existing evidence. Resolve driver,
> interrupt, power and ownership prerequisites before dependent implementation.
> Record exact checks and blockers after each atomic change; commit and push
> only intended files using the required workflow. Do not infer hardware PASS,
> product adoption or publication from completion of a local build.
