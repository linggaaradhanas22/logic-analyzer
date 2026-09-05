# Development Status and Handoff

This file is the execution tracker for the project. `docs/plan.md` remains the
normative implementation plan and design source of truth. Update this file at
the end of each development session so the next session can resume from the
same facts, evidence and blockers.

## Required Change Workflow

This workflow applies to every phase completion and every atomic tracked change
to source, tests, configuration, tooling or documentation. Build outputs,
reports, downloaded tools and captures remain ignored; summarize their results
here instead of committing them.

1. **Start synchronized.** Read `AGENTS.md`, `docs/plan.md` and this file. Run
   `git fetch origin`, `git status --short --branch` and
   `git log --oneline --decorate -5`. Work from `main`; if it is behind the
   remote and clean, fast-forward with `git pull --ff-only origin main`. Never
   reset or force-push to resolve divergence.
2. **Record the scope.** Update the current session log or phase row with the
   date, phase/task, intended files, acceptance checks and known blockers before
   making the change. Use `In progress` for work that spans sessions.
3. **Implement narrowly.** Keep the change atomic. Follow the project rules for
   native Windows paths, generated register files, ownership, DMA and tests.
   If work must pause, commit it as `WIP: ...`, mark it WIP here and list the
   exact continuation point rather than leaving an unexplained worktree.
4. **Verify and record evidence.** Run every relevant canonical target. Update
   this file with the exact command, PASS/FAIL/BLOCKED result, report path,
   relevant counts and remaining risks. A missing tool, XSA, board or hardware
   run is `BLOCKED` or `unverified`, never PASS.
5. **Review the commit set.** Run `git status --short --branch`,
   `git diff --check` and `git diff --stat`. Stage only intended files, then
   inspect `git diff --cached --stat` and `git diff --cached --name-only`.
   Confirm that no secret, local credential, generated environment, build
   output, log or capture is staged.
6. **Commit the atomic change.** Use a concise subject such as
   `P2: add trigger RTL` or `docs: update development status`. Include the
   status update in the same change. If the status must contain the new commit
   ID, add that ID in a follow-up status-only commit; do not leave the status
   update uncommitted.
7. **Push and verify.** Run `git push origin main`, then verify
   `git status --short --branch` is clean, `git log --oneline --decorate -2`
   shows the pushed commit and
   `git ls-remote origin refs/heads/main` matches `git rev-parse HEAD`.
   Record the pushed commit in the session log. If pushing fails, commit the
   local work, mark the push `BLOCKED` with the exact error and do not report
   the change as complete.
8. **Close the phase or change.** Set the phase/task to `Complete`, `WIP` or
   `Blocked`, list the next concrete action and leave no unexplained tracked
   files in the worktree. Re-read this document before the next task.

The definition of done is: relevant checks have evidence, blockers are honest,
the intended files and status are committed, `origin/main` contains the
commit, and the worktree is clean.

## Snapshot

| Item | Current value |
|---|---|
| Observed date | 2026-09-05 |
| Project root | `C:\Users\lingg\Projects\KCCI_technical\Embedded\Logic-Analyzer\Source_Code` |
| Plan source | `docs/plan.md`, verified identical to `logic-analyzer-implementation-plan-v0.4.md` |
| Branch | `main` |
| Last committed baseline | `6dd22b3 Add native Windows Phase 0 scaffold` |
| Remote | `origin` -> `https://github.com/linggaaradhanas22/logic-analyzer.git` |
| Remote state | `main` and `origin/main` were synchronized at the baseline commit |
| Current pending work | This workflow update in `AGENTS.md`, `README.md` and `docs/status.md` is in the working tree until committed |
| Workflow | Native Windows only; use `python tools/run.py <target>` |
| Phase | P0 local gates complete; P1/P2 implementation is next |

Generated environments, reports, logs and build outputs are intentionally
ignored by Git. The reports described below are local evidence and must be
recreated after a clean checkout.

## Phase Matrix

| Phase | Status | What exists | Remaining entry work |
|---|---|---|---|
| P0 Environment and scaffolding | Complete locally | Native runner, checker, CI scaffold, locks, CMake tests, Questa harness, ESP-IDF smoke build and board query | Hosted CI and physical hardware gates are not evidence of completion |
| P1 Specifications | Partial scaffold | Register map, protocol/RTOS/wiring documents, generated constants, C protocol library and UART smoke decoder | Complete protocol semantics, SPI/I2C decoders, reference models and specification coverage |
| P2 FPGA RTL | Not started | Generated `la_regs_pkg.sv` and the generic Questa smoke harness | Implement each RTL block and its self-checking test, then integrate `la_top` |
| P3 Vivado integration | Not started | Board files, board query Tcl, build/program Tcl scaffolds | Add `la_top.sv`, constraints, block design, synthesis, timing, bitstream and XSA |
| P4 Zynq FreeRTOS core | Not started | Linker/configuration scaffolds, host segment-pool test, Vitis CLI smoke | Generate the XSA first, then implement the Vitis platform/application and target tests |
| P5 ESP32 bridge | Scaffold only | Native ESP-IDF v5.5.2 hello-world build and MSVC mocks | Implement UART/TCP relay, flow control, tasks, Unity target tests and loopback |
| P6 Host tool and burst E2E | Not started | `la-cli` entry-point scaffold and host pytest smoke | Implement transports, capture commands, `.sr`/VCD export and end-to-end tests |
| P7 Live decode pipeline | Not started | Design notes only | Implement segment/event ownership, decoder tasks, live packets and counters |
| P8 RTOS hardening | Not started | Planned budgets and policies in `docs/plan.md` | Measure latency, throughput, faults, cache behavior and stack/CPU margins |
| P9 Performance | Not started | Planned rate and DMA constraints | Measure SPI/RLE/200 MS/s work only after the MVP path is valid |
| P10 UX and release | Not started | Release targets and documentation scaffolds | Implement live UI, optional sigrok bridge, BOOT.BIN and release evidence |

Do not mark a target phase complete because a tool or directory exists. The
phase requires the command or bench evidence named by the plan.

## Verified Gates

These results were observed at the fixed project root on 2026-09-05 using the
project `.venv` and are recorded under `build/reports`.

| Command or check | Result | Evidence or note |
|---|---|---|
| `python tools/run.py check-env -- --write-lock` | PASS | `build/reports/env-check.json`; no missing P0 prerequisites |
| `python tools/run.py query-board` | PASS | `build/reports/query-board.json` and `board-query.json` |
| `python tools/run.py query-board -- --write-lock` | PASS | Updates `tools/board-lock.json` |
| `python tools/run.py gen` | PASS | Register outputs are already current |
| `python tools/run.py test-native` | PASS | Native CTest subset: 2/2, including ctypes ABI |
| `python tools/run.py test-esp32-mocks` | PASS | CTest mock target: 1/1 |
| `python tools/run.py release` | PASS | Native release precheck: 3/3 CTest tests |
| `python tools/run.py test-host` | PASS | Pytest: 1 passed |
| `python tools/run.py lint` | PASS | Questa compile-only regression |
| `python tools/run.py sim` | PASS | Questa simulation and result-file/PASS checks |
| `python tools/regress.py --self-test` | PASS | Failure, timeout and missing-PASS detection |
| `python tools/run.py fw-esp32` | PASS | Native ESP-IDF v5.5.2 `esp32` build; output under `build/esp32` |
| `python -m compileall -q tools host tests` | PASS | Python syntax check |
| Absolute runner from outside the root | PASS | `test-host` passed using an absolute runner path |

Run `lint` and `sim` sequentially. They share the ignored
`build/questa` workspace, and concurrent cleanup can produce Windows file-lock
errors that are not code failures.

## Environment Lock

The authoritative observed versions and paths are in
`tools/toolchain-lock.json`. Important pinned values are:

| Tool or dependency | Observed value |
|---|---|
| Python | 3.11.15 in `.venv` |
| MSVC | 19.44.35228 x64 |
| CMake | 4.4.3 |
| Ninja | 1.13.0 Kitware build |
| Questa | 2025.2 FPGA Starter Edition |
| Vivado/Vitis/Bootgen | 2026.1 |
| ESP-IDF | v5.5.2, commit `30aaf64524299d3bde422ca9a2848090d1bc5d0f` |
| Arm GNU | 12.2.1 |
| PulseView | 0.4.2, project-local under `build/pulseview` |
| Board repository | Digilent `vivado-boards`, commit `36f34ab687b7fa9c778b779d027f3bce63b3ace9` |
| Board part | `digilentinc.com:zybo-z7-10:part0:1.2` |
| Target part | `xc7z010clg400-1` |

The lock has `missing_prerequisites: []`. Its status is
`environment-observed`; smoke statuses for later gates remain blocked where
the implementation is not present.

## Blockers and Caveats

- `python tools/run.py bit` is blocked because `fpga/rtl/la_top.sv` does not exist yet. This is the planned P2/P3 implementation boundary, not a missing Vivado installation.
- `python tools/run.py fw-zynq` is blocked because `build/release/logic_analyzer.xsa` does not exist. The Vitis CLI smoke passes, but `fw-zynq/scripts/platform.py` and `app.py` are still P4.1 placeholders.
- `test-esp32-target` requires a confirmed ESP32 module, a selected COM port and physical hardware. The current build uses the default `esp32` target only.
- `program`, `flash-esp` and `hil` are human-operated hardware gates. Do not report them as passing without a bench run.
- PulseView 0.4.2 is detected, but its version probe emits bundled Python 3.4 decoder syntax warnings. Resolve or explicitly accept this before relying on interactive decoder integration in P6.
- `build/reports` is ignored. If a report is absent on another machine, the corresponding check is `not-run`, not passed.
- Do not modify the neighboring DDR3 PHY project.

## Next Session Checklist

1. Read `AGENTS.md`, `docs/plan.md` and this file before editing.
2. Confirm the checkout root and run `python tools/run.py check-env -- --write-lock`.
3. Run `python tools/run.py query-board -- --write-lock` and confirm the board lock still matches the installed board files.
4. Run `python tools/run.py gen`, `test-native`, `test-esp32-mocks`, `test-host`, `lint` and `sim`; run `lint` and `sim` sequentially.
5. Start P1 completion where needed: expand protocol semantics, implement SPI/I2C decoder paths and add independent Python reference models.
6. Start P2 RTL in plan order: `la_input_sync`, `la_sampler`, `la_trigger`, pre-trigger ring, transaction encoder, raw packer, capture control, FIFO, pattern generator, registers and `la_top`.
7. Add one self-checking procedural Questa test for every RTL block and keep all tests in `fpga/tb/testlist.json`.
8. Do not attempt `bit` until `fpga/rtl/la_top.sv` and real constraints exist. Do not attempt Vitis application integration until an XSA is generated.
9. Keep ESP-IDF generated configuration at `build/esp32/sdkconfig`; do not commit a source-level `sdkconfig`.
10. After source or protocol changes, rerun the relevant native and simulation gates and update this document with the command, result and report path.

## Continuation Anchors

| Need | Start here |
|---|---|
| Normative requirements | `docs/plan.md` |
| Session state and blockers | `docs/status.md` |
| Canonical commands and environment import | `tools/run.py` |
| Tool versions and smoke state | `tools/toolchain-lock.json` |
| Board part and source commit | `tools/board-lock.json` |
| Native protocol/decoder/ownership tests | `tests/native/`, `fw-zynq/tests/host/`, `CMakeLists.txt` |
| Questa flow | `tools/regress.py`, `fpga/tb/testlist.json`, `fpga/tb/tb_smoke.sv` |
| ESP-IDF build | `fw-esp32/`, `docs/windows-dev.md` |
| Vivado/XSA entry point | `fpga/scripts/build.tcl` |

## Session Log

| Date | Work completed | Result |
|---|---|---|
| 2026-09-05 | Created the native-Windows Phase 0 scaffold; installed and locked prerequisites; added board files, runner environment import, native/Questa/ESP-IDF checks; fixed ESP-IDF output placement and board-query lock handling; created and pushed baseline commit `6dd22b3`. | Local Phase 0 gates pass; later implementation and hardware gates remain explicitly blocked. |
| 2026-09-05 | Defined the mandatory status, commit and `origin/main` push workflow in `AGENTS.md`, `README.md` and this document. | Workflow documentation is pending its commit and push. |
