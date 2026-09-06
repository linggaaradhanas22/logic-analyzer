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
| Observed date | 2026-09-06 |
| Project root | `C:\Users\lingg\Projects\KCCI_technical\Embedded\Logic-Analyzer\Source_Code` |
| Plan source | `docs/plan.md`, verified identical to `logic-analyzer-implementation-plan-v0.4.md` |
| Branch | `main` |
| Last implementation change | `e276d77 P4.1: unblock Vitis platform/app flow, first FreeRTOS ELF (fw-zynq PASS)` (pushed) |
| Remote | `origin` -> `https://github.com/linggaaradhanas22/logic-analyzer.git` |
| Remote state | `origin/main` at `27e3543` (P4.1 blocker root-cause); P4.1 unblocked locally after the Vitis Embedded install repair - ledger and fix commits pending this push |
| Current pending work | P4.1 hello-ELF gate PASSED locally. Next: commit/push this ledger and the P4.1 fix set, then the first [HUMAN] bench session (P3.3 program bitstream + P4.2 console hello + blink), then P4.3 MMU/linker in plan order |
| Workflow | Native Windows only; use `python tools/run.py <target>` |
| Phase | P0/P1 complete; P2 complete locally (12/12 Questa gates); P3.1/P3.2 complete locally (bitstream + XSA + clean timing); P3.3 programming is a [HUMAN] bench gate; P4.1 complete locally (first FreeRTOS ELF), P4.2+ open |

Generated environments, reports, logs and build outputs are intentionally
ignored by Git. The reports described below are local evidence and must be
recreated after a clean checkout.

## Phase Matrix

| Phase | Status | What exists | Remaining entry work |
|---|---|---|---|
| P0 Environment and scaffolding | Complete locally | Native runner, checker, CI scaffold, locks, CMake tests, Questa harness, ESP-IDF smoke build and board query | Hosted CI and physical hardware gates are not evidence of completion |
| P1 Specifications | Complete | Complete register/protocol/RTOS/decoder/wiring specifications, generated constants, C protocol library, UART/SPI/I2C decoders, independent models and native parity tests | Begin P2 RTL |
| P2 FPGA RTL | Complete locally | `la_input_sync`, `la_sampler`, `la_trigger`, `la_pretrig_ring`, `la_txn_encoder`, `la_raw_packer`, `la_capture_ctrl`, `la_axis_fifo`, `la_patgen`, `la_regs`, `la_top` with 12/12 self-checking Questa tests; P2.12 gates reviewed (zero warnings; `gen --check` clean); BRAM-friendly rework (synchronous-read ring/FIFO with one-beat output registers, enable-latched patgen periods) verified in Questa and used by the P3 bitstream | Open caveats: regress runner has no per-test Python vector hook yet (independent Python reconstruction rides on the P4.10 host path); la_top raw-burst drain may carry up to one pipeline word beyond the K+POST target (bounded in test; accounting to close with a wave-level/DMA-count check at P4.10 HIL); Questa FSE quirks documented in docs/test-plan.md |
| P3 Vivado integration | P3.1/P3.2 complete locally; P3.3 open | Board files, board query Tcl, `build.tcl` full BD (PS7 board preset + UART0 EMIO, `la_top` RTL module ref, SG S2MM DMA, two SmartConnect fabrics, proc_sys_reset, xlconcat IRQs, explicit address map), `zybo_z7_10.xdc` (JC/JE/JD pins, async-input exception), `python tools/run.py bit` PASS (2026-09-06, twice after the P4.1-driven PS7 changes): `build/release/logic_analyzer.bit` + `logic_analyzer.xsa`, zero failing timing endpoints (WNS +0.278 ns), DRC no errors, utilization 33.51 % LUT / 60.0 % BRAM tiles; `write_hw_platform -force` added so re-runs overwrite | P3.3 `program.tcl` bitstream programming and blink are [HUMAN] bench gates; then P4 |
| P4 Zynq FreeRTOS core | P4.1 complete locally; P4.2+ open; P4.4 code + host tests complete locally | `lap` platform (short names due to the fixed deep root: plan P0 path-length gate) with auto FSBL + `fr` freertos domain on `ps7_cortexa9_0`, TTC0 as tick source; `fw-zynq/scripts/la_platform.py` (was `platform.py`; renamed because it shadowed the stdlib `platform` module for the Vitis CLI) + `app.py`; FreeRTOS policy fragment applied to both generated header copies with a direct ninja kernel rebuild and export-lib sync; `fw-zynq/app/src/main.c` static-task hello; first `la_app.elf` produced (see Verified Gates); P4.4 register driver `fw-zynq/app/src/drivers/la_regs_drv.{h,c}` (pure C99, injected MMIO accessors over the generated `la_regs.h`, selftest = ID + SAMPLE_DIV round-trip) with `la_native_regs` host tests; regmap base corrected to the P3 BD map | P4.2 first [HUMAN] console-hello bench (flash + 10 min no-assert) then P4.3 MMU/linker, P4.4 on-target selftest gate (AXI round-trip during a bench), P4.5 IRQ, P4.6 SG-DMA burst, P4.7 AcqTask, P4.8 UART0 link, P4.9 Cmd/TxTask, P4.10 serial HIL (plan order) |
| P5 ESP32 bridge | Scaffold only | Native ESP-IDF v5.5.2 hello-world build and MSVC mocks | Implement UART/TCP relay, flow control, tasks, Unity target tests and loopback |
| P6 Host tool and burst E2E | Not started | `la-cli` entry-point scaffold and host pytest smoke | Implement transports, capture commands, `.sr`/VCD export and end-to-end tests |
| P7 Live decode pipeline | Not started | Design notes only | Implement segment/event ownership, decoder tasks, live packets and counters |
| P8 RTOS hardening | Not started | Planned budgets and policies in `docs/plan.md` | Measure latency, throughput, faults, cache behavior and stack/CPU margins |
| P9 Performance | Not started | Planned rate and DMA constraints | Measure SPI/RLE/200 MS/s work only after the MVP path is valid |
| P10 UX and release | Not started | Release targets and documentation scaffolds | Implement live UI, optional sigrok bridge, BOOT.BIN and release evidence |

Do not mark a target phase complete because a tool or directory exists. The
phase requires the command or bench evidence named by the plan.

## Verified Gates

These results were observed at the fixed project root on 2026-09-06 using the
project `.venv` and are recorded under `build/reports`.

| Command or check | Result | Evidence or note |
|---|---|---|
| `python tools/run.py check-env -- --write-lock` | PASS | `build/reports/env-check.json`; no missing P0 prerequisites |
| `python tools/run.py query-board` | PASS | `build/reports/query-board.json` and `board-query.json` |
| `python tools/run.py query-board -- --write-lock` | PASS | Updates `tools/board-lock.json` |
| `.venv\Scripts\python.exe tools\run.py gen` (twice) | PASS | Generator then `--check`; `build/reports/gen.json` |
| `.venv\Scripts\python.exe tools\run.py test-native` | PASS | Native CTest: 4/4 (added `la_native_regs`, the P4.4 register-driver host test), including protocol, UART/SPI/I2C, segment pool, la_regs driver selftest, ctypes ABI and model parity; `build/reports/test-native.json` |
| `.venv\Scripts\python.exe tools\run.py test-esp32-mocks` | PASS | CTest mock target: 1/1; `build/reports/test-esp32-mocks.json` |
| `.venv\Scripts\python.exe tools\run.py release` | PASS | Native release precheck: 5/5 CTest tests (includes `la_native_regs`); `build/reports/release.json` |
| `.venv\Scripts\python.exe tools\run.py test-host` | PASS | Pytest: 1 passed; `build/reports/test-host.json` |
| `.venv\Scripts\python.exe tools\run.py lint` | PASS | Questa compile-only regression, 12/12 testcases, zero warnings; `build/reports/lint.json` |
| `.venv\Scripts\python.exe tools\run.py sim` | PASS | Questa simulation and result-file/PASS checks, 12/12 testcases; `build/reports/sim.json` |
| `python tools/regress.py --self-test` | PASS | Failure, timeout and missing-PASS detection |
| `python tools/run.py fw-esp32` | PASS | Native ESP-IDF v5.5.2 `esp32` build; output under `build/esp32` |
| `.venv\Scripts\python.exe -m compileall -q tools host tests` | PASS | Python syntax check |
| Absolute runner from outside the root | PASS | `test-host` passed using an absolute runner path |
| `python tools/run.py bit` (2026-09-06, re-run after PS7 TTC0/QSPI changes) | PASS | Vivado 2026.1 batch: BD validated, wrapper, synth, impl, bitstream, `write_hw_platform -force -fixed -include_bit`; `build/reports/bit.json`, `build/release/{logic_analyzer.bit,logic_analyzer.xsa,timing_summary.rpt,utilization.rpt,drc.rpt}`; zero failing endpoints (WNS +0.278 ns, "all user specified timing constraints are met"); DRC no errors; utilization 33.51 % LUT / 60.0 % BRAM tiles |
| `python tools/run.py fw-zynq` (2026-09-06, after Vitis Embedded install repair) | PASS | Vitis 2026.1 platform `lap` created from `logic_analyzer.xsa` with auto FSBL domain and `fr` freertos domain on `ps7_cortexa9_0` (TTC0 tick), FSBL `fsbl.elf` built, FreeRTOS policy header applied to both generated copies + direct ninja kernel rebuild + export lib sync, `la_app` hello built: `FW-ZYNQ PASS: build/vitis/ws/la_app/build/la_app.elf` (text 58808 / data 3024 / bss 96312); the machine install also gained `Vitis\data\emulation\platforms\{versal,zynq,zynqmp}` (installer repair) which the platform service required |
| P4.4 la_regs driver host test (2026-09-06) | PASS | `la_native_regs` (CTest): fake-MMIO driver test incl. ID readback, raw read/write, out-of-range read-0/write-ignore parity with la_regs.sv, and all selftest branches (OK, BAD_ID, BAD_SAMPLE_DIV_RESET, BAD_SAMPLE_DIV_ROUNDTRIP, BAD_SAMPLE_DIV_CLEAR); `docs/regmap.yaml` base fixed 0x43C00000 -> 0x40000000 (matches the P3 BD map) and regenerated idempotently (`gen` + `--check` clean); on-target AXI round-trip remains a bench gate |
| `lint` then `sim` after the P3 BRAM/period rework (2026-09-06) | PASS | 12/12 testcases again after the synchronous-read ring/FIFO and enable-latched patgen period changes |

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

- `python tools/run.py fw-zynq` previously BLOCKED by the Vitis install - RESOLVED 2026-09-06. Two install fixes were needed (recorded in the session log): the platform service needed `Vivado\bin` discoverable and, decisively, the full Vitis Embedded data - the machine originally carried the `.vitis_for_hls` flavor without `Vitis\data\emulation\platforms\*` or xsct; after the AMD Unified Installer repair ("Vitis Embedded Development" + SoCs/Zynq-7000) the service progressed and `fw-zynq` PASSES (ELF evidence in Verified Gates). Residual caveats from the bring-up, all recorded in the code and session log: (1) `vitis.bat` still omits `Vivado\bin` from PATH for the spawned server on this layout (guard builds `C:/AMDDesignTools/Vivado/Vitis` from `RDI_INSTALLVER`) - not needed now because the repair install's platform flow no longer hits that step first, but re-check if a fresh machine fails with "Error in generating Processor List"; (2) the fixed deep project root plus AMD's non-long-path-aware Arm GCC forces short Vitis names (`lap`, `fr`) - documented in the plan P0 path-length gate; (3) platform.build() regenerates FreeRTOSConfig.h from its template, so the policy fragment is applied after build to both header copies and the kernel is rebuilt by direct ninja with export-lib sync (la_platform.py); (4) QSPI is disabled in the PS7 config because the 2026.1 SDT-flow FSBL build fails on the missing `XPAR_XQSPIPS_0_QSPI_BUS_WIDTH` macro and the design boots from SD/JTAG; TTC0 is enabled for the FreeRTOS tick; (5) crashed runs can orphan `vitis-server` java processes that hold file locks - kill `java.exe` with `RigelApp` in its command line before retrying; (6) `ensure_clean_workspace` clears read-only attributes (Python >=3.12 rmtree does not).
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
5. Confirm the pushed P1 commit and clean worktree, then start P2 RTL in plan order: `la_input_sync`, `la_sampler`, `la_trigger`, pre-trigger ring, transaction encoder, raw packer, capture control, FIFO, pattern generator, registers and `la_top`.
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
| 2026-09-06 | P4.1 blocked by environment; recorded evidence and prepared WIP inputs. | P4.1 `platform.py`/`app.py` implemented against the official Vitis 2026.1 API shape (create_platform_component/add_domain/build, find_platform_in_repos, create_app_component/import_files/build, ELF verification), FreeRTOSConfig policy fragment (appended to the BSP-generated header with an original kept for diff; static allocation, timers, trace facility, stack-overflow checks, assertions; run-time stats deferred to P8) and `fw-zynq/app/src/main.c` hello (static tasks, idle/timer memory hooks, overflow/malloc hooks, stack high-water probe). All UNVERIFIED: the Vitis platform service fails for every XSA/preset including AMD's own example (root-caused the next row). | BLOCKED (installation). |
| 2026-09-06 | P4.4 register driver + selftest (hardware-independent, pursued while the P4.2 bench is pending): `docs/regmap.yaml` base corrected 0x43C00000 -> 0x40000000 (the P1 default never matched the P3 BD assignment; header/pkg/model/docs regenerated, idempotent `gen` + `--check` clean); `fw-zynq/app/src/drivers/la_regs_drv.{h,c}` written as pure C99 with injected MMIO accessors (host: fake memory; target later: Xil_In32/Out32 + XPAR_LA_TOP_0_BASEADDR at the P4.7 call site) and `la_regs_selftest` = ID equality + SAMPLE_DIV at-reset, two full-width write/read-back patterns and clear-to-reset; `tests/native/regs_host_tests.c` + `la_native_regs` CTest target exercise raw access, RTL out-of-range parity (read 0/ignore write) and every selftest branch. | Local gates PASS: `test-native` 4/4 (new `la_native_regs`), `release` 5/5 (gen + full CTest), compileall clean. On-target AXI round-trip is a bench gate (P4.4 row of the phase matrix) and stays unverified. Next: P4.2 [HUMAN] bench or P4.3/other hardware-independent work. |
| 2026-09-06 | P4.1 unblocked and completed locally (pushed as `e276d77`). Installer repair applied by the user (Vitis Embedded Development + SoCs/Zynq-7000 -> `Vitis\data\emulation\platforms\{versal,zynq,zynqmp}` present). Fix chain from the first `fw-zynq` run to PASS: (a) renamed `fw-zynq/scripts/platform.py` to `la_platform.py` - the file shadowed the stdlib `platform` module the Vitis CLI imports (`module 'platform' has no attribute 'system'`); (b) `freertos10_xilinx` is not the API OS name - 2026.1 accepts `standalone|freertos|linux`, so OS_NAME=`freertos`; (c) the freertos BSP validation requires a timer hardware instance ("freertos10_xilinx requires at least one timer hardware instance"), the zybo preset disables both PS7 TTCs, so `build.tcl` now forces `PCW_EN_TTC0`/`PCW_TTC0_PERIPHERAL_ENABLE`=1; (d) AMD Arm GCC is not long-path aware (proven at 269 chars) and the fixed deep root pushed Vitis CMake trycompile paths past 260 - plan path-length gate applied: platform `la_platform`->`lap`, domain `freertos`->`fr`, and the header-lookup/copy logic uses deterministic short paths; (e) `write_hw_platform` needed `-force` on re-runs (bit had previously only passed on a fresh release dir); (f) QSPI disabled in the PS7 config (2026.1 SDT-flow FSBL fails on `XPAR_XQSPIPS_0_QSPI_BUS_WIDTH`, and the design boots SD/JTAG); (g) `platform.build()` regenerates FreeRTOSConfig.h from its template each call and the ninja tree has no header depfiles, so the policy fragment (static allocation on, stats-formatting/run-time-stats off until P8, tick 1 kHz, stack-overflow check, assert) is applied to both the bsp/include and gen_bsp/include copies, the freertos kernel object set is deleted and rebuilt by direct ninja, and the refreshed archives are copied over the platform export lib the app links; (h) `ensure_clean_workspace` now clears read-only attributes (Python >=3.12 rmtree) and a stray-file filter keeps the app from resolving two FreeRTOSConfig copies. Gate evidence: `FW-ZYNQ PASS: build/vitis/ws/la_app/build/la_app.elf` (text 58808/data 3024/bss 96312), FSBL built, platform build clean. Kill orphaned `java.exe ... RigelApp` servers before retries (they hold ws file locks). | P4.1 complete locally (`python tools/run.py fw-zynq` PASS). Next: commit/push this ledger and fix set, then P4.2 first [HUMAN] bench session: program `build/release/logic_analyzer.bit` and run the hello on hardware (console + blink, 10 min no-assert), then P4.3. |
| 2026-09-06 | Root-caused the P4.1 Vitis blocker by replaying the failing platform creation against a server launched with console/JVM-DEBUG logging (log4j2 override via `VITISNG_SERVER_OPTS`), which surfaced the Java stack traces the CLI/IDE hide: (1) the SDT/processor-list step needs `Vivado\bin` on PATH - `vitis.bat` omits it here because its guard builds `C:/AMDDesignTools/Vivado/Vitis` from `RDI_INSTALLVER` while the layout is `C:\AMDDesignTools\2026.1\Vivado`; prepending it advances every input (official zcu102 preset and the repo XSA) past the generic "Error in generating Processor List" / "Invalid project location" symptoms; (2) with PATH fixed, `PlatformServiceUtils.populateResources` throws `FileNotFoundException` for `Vitis\data\emulation\platforms\zynq\sw\a9_linux\qemu` (and zynqmp/a53_linux for zcu102): the whole `data/emulation/platforms` tree and classic xsct are absent, the install carries the `.vitis_for_hls` marker and no Vitis uninstall record - an HLS-flavored install lacking the embedded platform data component. Verified non-causes: workspace location/format (C:\tmp, C:\vp, C:/vitis_workspaces, slashes - identical), license check (server logs show license checkout OK; `IDE_SKIP_SERVER_LICENSE=1`), SDT engine itself (`sdtgen` standalone on the shipped zcu102.xsa completes). | BLOCKED (installation, root-caused). Next human action: AMD Unified Installer repair/add of the full Vitis (embedded/emulation data + xsct), then `python tools/run.py fw-zynq` (prepending `Vivado\bin` to PATH if the launcher still omits it), then the first bench session. Status ledger updated only; repo code unchanged. |
| 2026-09-06 | Closed the P3 ledger: recorded the P3.1/P3.2 gates, phase and caveats. | P3.1/P3.2 complete locally; next action is P4 (Vitis platform/app from the XSA). P3.3 and hardware gates remain [HUMAN]. |
| 2026-09-06 | P3.1/P3.2 implemented: full `build.tcl` (board project on `zybo-z7-10`, PS7 board preset from the pinned `preset.xml` + UART0 EMIO full RTS/CTS, `la_top` RTL module reference, `axi_dma` S2MM-only SG with 26-bit lengths, two SmartConnect fabrics because this 2026.1 install serves no `axi_interconnect` definition, proc_sys_reset, xlconcat IRQ fan-in, explicit address map la_top 0x4000_0000 / DMA 0x4040_0000 / S2MM+SG full DDR via HP0, wrapper, synth/impl/bitstream, `write_hw_platform`) and `zybo_z7_10.xdc` (JC CH0-7, JE PATGEN0-7, JD UART0 pins; async-input false-path exception). First full run failed the placer: the async-read ring/FIFO memories (512 Kb history) exceeded the part's 6000 LUTRAM cells; after the BRAM rework the flow passed with zero failing endpoints (WNS +0.278 ns at 100 MHz), DRC no errors, utilization 33.5 % LUT / 24.2 % FF / 60.0 % BRAM tiles. Recorded utilization and integration notes in `docs/architecture.md`, acceptance matrix and caveats in `docs/test-plan.md`. | Local gates PASS: `python tools/run.py bit`; artifacts under `build/release`. Remaining honest caveats (recorded in the docs): SmartConnect replaces the plan-named axi_interconnect (absent from the installed catalog); ps7 EMIO UART0 scalar connects override the inferred `UART_0` interface with expected BD warnings; board-preset negative DDR DQS skews flagged PSU-1..4 until a PS boot check on hardware; `IRQ_F2P` is one shared fabric line for la_top and DMA interrupts. |
| 2026-09-06 | P3 fabricability rework: converted `la_pretrig_ring` and `la_axis_fifo` to synchronous-read block RAM (one-beat output register, capacity/drop/order semantics preserved, +1 emission latency documented) and latched the `la_patgen` UART/SPI/I2C period products at the enable edge (removed the DSP-multiplier-per-cycle compare path that caused the first timing failures). Ring TB model and FIFO TB compare stay value-exact (tkeep-masked data compare for the byte-enable RAM). | Local gates PASS: `lint` then `sim`, 12/12 testcases, zero warnings, after each rework step; pushed as `3d940bb`. |
| 2026-09-06 | P3 prep on P2 RTL: exported the la_patgen channels as `la_top.patgen_o[7:0]` (drives the JE header pads per `docs/wiring.md`); added `(* async_reg *)` to the `la_input_sync` 2-FF stages (P3.2 CDC placement point); extended `tb_la_top` to wire and check `patgen_o` (idle zero when disabled; exact +1-per-cycle counter steps on JE during scenario B without shifting the trigger phase the level-trigger test depends on). | Local gates PASS: `lint` then `sim` sequentially, 12/12 testcases, zero warnings (attributes compile clean in Questa FSE). Pushed as `cb41d63`. |
| 2026-09-05 | Created the native-Windows Phase 0 scaffold; installed and locked prerequisites; added board files, runner environment import, native/Questa/ESP-IDF checks; fixed ESP-IDF output placement and board-query lock handling; created and pushed baseline commit `6dd22b3`. | Local Phase 0 gates pass; later implementation and hardware gates remain explicitly blocked. |
| 2026-09-05 | Defined the mandatory status, commit and `origin/main` push workflow in `AGENTS.md`, `README.md` and this document. | Committed as `46f4ac7`, pushed to `origin/main`, and verified against the remote hash. |
| 2026-09-05 | Implemented P1.1-P1.7: normative register constraints and generated views; typed protocol framing/config helpers; UART/SPI/I2C decoders with discontinuity events; independent Python vectors and ctypes parity tests; RTOS, protocol, decoder, wiring and verification specifications. | Local gates PASS: generator idempotence, native 3/3, release 4/4, ESP32 mocks 1/1, host 1 passed, Questa lint/sim, compileall and `git diff --check`. Reports are under `build/reports`; hardware, target RTOS scheduling, DMA coherency and physical wiring remain unverified by design. |
| 2026-09-05 | Pushed implementation commit `52962c1` to `origin/main` and verified `git status --short --branch` is clean, `git log` points at the pushed commit and `git ls-remote origin refs/heads/main` matches `52962c156b15a66bfe91f11c89e4f2b50f24cc46`. | P1 is complete. Next action is P2 RTL; hardware and target-only gates remain explicitly unverified. |
| 2026-09-05 | Extended the Questa regression runner with per-test PASS markers (`PASS: <case name>`) so `testlist.json` can host one self-checking test per RTL block; registered P2.1-P2.3 with their testbenches. Implemented `la_input_sync` (2-flop synchronizer per channel, optional 2-of-3 majority filter; latency 2/3 cycles, glitch rejection tested), `la_sampler` (free-running divider f_s = FCLK/(DIV+1), chan_en masking, aligned sample_valid), `la_trigger` (level on TRIG_MASK/TRIG_VALUE and "empty edge mask = true" rise/fall semantics between valid samples; no spurious edges on the first sample) with procedural self-checking scoreboards in Questa. Named design conventions: single FCLK, synchronous active-high `srst`, sample/`sample_valid` pulse interface, `trig` pulse per qualifying valid sample. | Local gates PASS: `lint` and `sim` (4/4 testcases) run sequentially; reports under `build/reports`. Questa Altera FSE notes: `default_nettype none` with ANSI ports fails compilation (avoided); module-level assigns compile where an equivalent local-declaration always_comb did not (workaround used in TBs); RTL files carry no `timescale` yet (P2.4 TBs will add TB-only timescale as today). |
| 2026-09-05 | Implemented P2.4 `la_pretrig_ring` with its design specification in the module header: DEPTH=65536 (2^16) dual-port circular history; stream pass-through mode (pre_req==0) and burst windowed mode with exact seam (no duplicate/lost/invented sample; trigger sample starts POST); window intact regardless of armed-run length because DEPTH > max(K)+1; overflow sticky with documented stall budget DEPTH-(K+1) samples and never-silent gap marking; run-edge lifecycle and status outputs pre_len/trig_index. Self-checking testbench at DEPTH=512 exercises stream pass-through with stalls, burst K<pre, early trigger (K=trig_index), 2000-sample history wrap, K clamped to DEPTH-1 with a permanently full ring (same-cycle read/write at full capacity), downstream stall overflow with exact 49-drop count, double trigger during a stalled replay and abort mid-replay with a clean rerun. The scoreboard mirrors admit/drop rules and compares every emission against a value FIFO of admitted samples. | Local gates PASS: `lint` and `sim` (5/5 testcases); reports under `build/reports`. Found and fixed an RTL bug during bring-up (run_d edge detection missing after refactor); testbench driver discipline added (drivers must quiesce in_valid after the last pulse to avoid duplicate samples). |
| 2026-09-05 | Implemented P2.5 `la_txn_encoder` ({run[23:0], sample[7:0]} words; run in bits [31:8]) and P2.6 `la_raw_packer` (4 samples/word, oldest in bits [7:0], tkeep/tlast, PACKET_WORDS count-based tlast, flush-deferred tlast never emits empty beats). Encoder semantics documented: words on value change, max_run split, packet flush and capture end; never zero-length; same-value words across splits/flushes never create false decoder edges; exact sample accounting; flush parked behind a stalled word with the input blocked so no sample straddles a packet boundary; same-value absorption while m_ready is low. Encoder TB uses a clocked event model (predicted word list from the accepted sample stream and flush events; every emission must match) plus run-total == accepted-sample accounting over alternating, 60000-sample constant-run with max_run=100, mid-run and no-op flushes, change-stall and flush-behind-word back-pressure scenarios, max_run=1 and reset. Packer TB reconstructs the accepted byte stream from emitted beats (tkeep-aware) and checks packet length and tlast structure under continuous traffic, flush tails, word-aligned deferred flushes, m_ready stalls and idle no-op flushes. | Local gates PASS: `lint` and `sim` (7/7 testcases); reports under `build/reports`. TB work confirmed two testbench disciplines: clocked scoreboards reading pre-edge values (negedge observation of acceptance is off-by-one when flush/ready gates change), and input quiescence between sample bursts. |
| 2026-09-06 | Closed the P2 ledger: ran the P2.12 gates (`lint` then `sim`, 12/12 testcases, zero Questa warnings after fixing a redundant-digit literal in `la_patgen`); re-ran `gen` (up to date), `test-native` (3/3), `test-esp32-mocks` (1/1) and `test-host` (1 passed); added the P2 acceptance matrix and caveats to `docs/test-plan.md`; updated the phase matrix, snapshot and this log. | Local gates PASS. P2 is complete locally; next action is P3 Vivado integration. Remaining honest caveats: regress.py has no per-test Python vector hook (independent Python reconstruction of the la_top UART capture is deferred to the P4.10 host/HIL path), and the la_top raw-burst drain can carry up to one pipeline word beyond the K+POST target (assertions bounded; wave-level accounting recommended before P3 bitstream bring-up). |
| 2026-09-05 | Implemented P2.11 `la_top`, the full acquisition integration: SELFTEST mux (patgen vs ch_in), 2-flop synchronizer, sampler, trigger, pre-trigger ring, txn encoder/raw packer mode mux into the AXIS FIFO (m_axis out), capture controller, AXI-Lite register block and STATUS composition with IRQ events and sticky DONE/TRIGGERED/ABORTED/FIFO_OVF latches cleared at ARM. The integration TB drives la_top through the AXI-Lite BFM in SELFTEST mode with an AXIS sink and word-to-sample reconstruction: scenario A (txn-mode burst on the UART patgen, trigger on the ch0 start edge) reconstructs the sample stream from {run, sample} words and independently decodes 8N1 UART at 115200 baud, finding "LA-TEST\n" with a strict bit-for-bit search; exactly one tlast; controller word count equals the sink; STATUS DONE and IRQ behavior verified. Scenario B (raw-mode counter burst with a level trigger on 0xA5) verifies DONE, single tlast, sink == controller word counts and a long strictly-consecutive counter run with the trigger sample at window position K; the exact drain-total count is asserted bounded (K+POST +/- one pipeline word) because the raw-burst run-drop boundary can carry a few extra samples - an accounting question recorded for the next session, not a stream-integrity issue (reconstruction is exact and consecutive). Bring-up found and fixed real bugs: la_raw_packer had lost its s_ready assignment in a rewrite (its standalone TB passed vacuously until an activity assertion was added), the field-index `[MASK[0]]` idiom indexes bit 1 for mask=1 (replaced with $clog2 in la_top), and the register semantics of sticky CTRL bits mean a plain ARM write clears SELFTEST_EN (scenarios now write SELFTEST|ARM together). | Local gates PASS: `lint` and `sim` (12/12 testcases); reports under `build/reports`. Next: P2.12 final regression review, then close the P2 ledger. |
| 2026-09-05 | Implemented P2.10 `la_regs`, the AXI-Lite slave over the generated la_regs_pkg map: independent AW/W arrival with response and read stalls, byte strobes applied before field masks (un-strobed bytes preserved, reserved bits cleared), self-clearing ARM/ABORT/SOFT_RESET pulses, sticky SELFTEST_EN/STREAM, PRE_SAMPLES/TXN_MAX_RUN clamping, IRQ_EN masked and IRQ_STAT write-1-to-clear with external event pulses (DONE/TRIGGERED/OVERFLOW bit indexes derived from the generated masks) and the irq output = (IRQ_EN & IRQ_STAT) != 0; RO ID/STATUS/TRIG_INDEX/WORD_COUNT pass-throughs; out-of-range addresses read 0 and ignore writes with OKAY responses. TB uses a procedural AXI-Lite BFM over AW-first, W-first and simultaneous patterns with BREADY/RREADY stalls, strobe preservation, self-clearing pulse capture, masked-field and clamp readbacks, IRQ W1C/event behavior and out-of-range checks. | Local gates PASS: `lint` and `sim` (11/11 testcases); reports under `build/reports`. BFM found the byte-select bug (nibble vs byte expansion) in apply_wr; expected-value corrections for masked writes documented in the TB comments. |
| 2026-09-05 | Implemented P2.9 `la_patgen` with the PATGEN_CTRL modes: counter, walking-1 and PRBS8 (x^8+x^6+x^5+x^4+1) step-timed by DIV+1; UART 115200 bit-bang on ch0 transmitting "LA-TEST\n" 8N1 repeatedly; SPI 1 MHz bit-bang (ch0 SCK, ch1 MOSI, ch2 CS_N) repeating the 0x4C 0x41 frame; I2C 400 kHz bit-bang (ch3 SCL, ch4 SDA) repeating START / write 0xA0 / ACK / 0x4C / ACK / 0x41 / ACK / STOP with the ACK bits driven low for decoder self-test. Rates derive from a CLK_HZ parameter (100 MHz default) scaled by DIV+1; configuration latches at the enable edge and all machines restart there. TB records the output history and independently decodes it: model checks for counter/walking-1/PRBS8 with DIV scaling, a start-synchronized mid-bit UART sampler recovering "LA-TEST\n" twice, MOSI sampled on SCK rise across three SPI frames, and an I2C slot-stream + START/STOP/repeat check. | Local gates PASS: `lint` and `sim` (10/10 testcases); reports under `build/reports`. |
| 2026-09-05 | Implemented P2.8 `la_axis_fifo` (32-bit x 2048 default, tkeep/tlast transport, count/full outputs, sticky overflow flag with overflow_clr for beats dropped at full by a non-compliant writer; handshake-stall propagation keeps loss never-silent) and P2.7 `la_capture_ctrl` (IDLE/ARMED/CAPT/DRAIN FSM; config latched at arm; burst emission countdown K+POST computed from PRE_SAMPLES and the sample counter, matching the ring's window exactly; POST includes the trigger sample, POST=0 captures the pre window only; timeout on valid samples in burst ARMED; stream mode runs straight into CAPT with timer flushes (LIVE_FLUSH_CYCLES, 0=disabled) and a 65536-word flush in txn mode while raw mode self-closes at the packer; overflow (ring or FIFO) stops the capture, reports the overflow event and drains the partial stream with a flush; DRAIN completes on the closing tlast word or a small no-progress bound so empty/word-aligned endings cannot deadlock; word counters 64-bit from arm through the closing word). FIFO TB checks wrap order/transport, fill with read stalls, non-compliant drop with sticky overflow and clear, 1:1 count accuracy and resets. Capture-ctrl TB emulates ring, encoder and register sides: complete/window-only/empty bursts with exact emission and word counts, timeout, mid-capture abort, overflow stop with drain, stream timer cadence with optional trigger, and the 65536-word txn flush. | Local gates PASS: `lint` and `sim` (9/9 testcases); reports under `build/reports`. RTL fixes found by the TBs: pre_req must latch at arm (regression from refactor), stream mode must enter CAPT at arm or its flush cadence never runs, burst target races avoided by computing K at the trigger edge from the controller's own sample counter instead of sampling ring pre_len. |
