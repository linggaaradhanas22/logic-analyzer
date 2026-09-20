# RTOS Logic Analyzer — Zybo Z7-10 + ESP32
## Agent-Executable Implementation Plan — v0.4

2026-09-05. Supersedes v0.3. Location clarification: all project-owned files live in the existing `Logic-Analyzer\Source_Code` folder (§2/§2A). Development, simulation, firmware builds, flashing, host tests and release packaging run **natively on Windows**. This revision incorporates the Windows feasibility review and the associated DMA, ownership, latency and throughput corrections. The original v0.3 remains a historical reference; execute this version.

This document specifies future deliverables; filenames and commands below do not imply that the scaffolding or implementation already exists. Work phase by phase. Steps requiring physical wiring, programming approval or bench observation are tagged **[HUMAN]**.

### Proposed direct USB migration (2026-09-17)

The documentation-only USB planning package is in
[usb-migration-implementation-instructions.md](usb-migration-implementation-instructions.md),
with its proposed interface contract in [usb-link.md](usb-link.md). Read these
with [status.md](status.md) before starting related work. This file remains the
normative overall plan; the package does not itself authorize implementation or
replace the existing gateway architecture. The user approved preparation of the
plan only. A later execution instruction and the recorded U07 adoption decision
control implementation scope and any change to the product baseline.

The candidate connects Zynq PS USB0 directly to the PC through CDC-ACM, bypassing
the ESP gateway in that profile. U00-U06 cover baseline, feasibility, contract,
enumeration, integration, correctness and measured performance. U07 decides
adoption and the wireless/browser feature scope; U08-U09 reconcile documentation
and release. P9.1's >=1.5 MB/s drain objective is retained for evaluation, with
explicit byte counts and timing; passing USB does not mark the SPI fault fixed.
P9.2 remains required, P9.3 stays separate stretch work, and P9.4/P10.1 need an
explicit optional-profile or deferral decision. The P10.4 release hold remains
until the chosen transport, final image and SD-boot gates are resolved.

**U07 decision (2026-09-19, revised): ADOPT.** U03-U05 correctness gates pass
and the U06 64 MiB drain gate passes at 1.62 MB/s in three consecutive trials
after the documented DATA_CHUNK amendment (1016 -> 4092 bytes plus the client
read/ACK pacing). Direct USB is adopted as a supported transport; the gateway
remains supported and stays the default build, and no wireless or browser
feature is removed. P9.1/P9.4/P10.1 keep their current status. Details are in
[usb-link.md](usb-link.md) section 16 and
`build/reports/usb-u06-performance.md`.

**Full-USB live pivot (2026-09-19, user-directed).** The default product
transport moves to direct USB and the live browser view moves to the PC host.
The execution package is
[usb-full-live-pivot.md](usb-full-live-pivot.md) with packets V00-V06. The
gateway remains a supported optional profile: its build, `la_web` UI and
wireless features are retained, not deleted. P9.4 (Wi-Fi tuning), P10.1
(browser live view) and the ESP32-S3 transport port become gateway-profile
features, optional/deferred for the USB-first default. Release packaging
becomes profile-aware so the SD image can carry the USB application.

**V00 scope freeze (2026-09-20, user-directed).** The pivot defaults are now
executable, not only planned:

- **USB is the default product transport/profile.** `python tools/run.py
  fw-zynq`, `jtag-zynq`, `boot` and `package` default to the USB profile
  (`la_app_usb`); `--profile gateway` keeps the ESP32-gateway build, load and
  packaging paths fully working. Docs and tool help state the default so
  nothing silently builds the wrong image.
- **The live browser view moves to the PC host.** The host bridge streams the
  decoded events that `la-cli live` already receives; the ESP32 `la_web` page
  and server stay in the repository for the gateway profile.
- **Settled transport choice: SSE** (stdlib `http.server` long-lived response)
  with the existing `{"s":<slot>,"e":[[<t>,<kind>,"<hex>"],...]}` browser
  message, so the event payload stays byte-compatible with `la_web_core`; the
  host page uses a small `EventSource` listener.
- **Settled client policy: single client first**, with the hub built so a
  multi-client fan-out can be enabled later (bounded replay ring, drop
  accounting, one serial owner).
- **P9.4, P10.1 and the ESP32-S3 transport port** are gateway-profile
  features, retained and optional/deferred for the USB-first default. P9.1's
  SPI findings and the U00-U09 evidence stay valid and are not rewritten.

**Unified UI planning (2026-09-20, user-directed).** The interactive surface
consolidates into one host-served browser workspace (`la-ui`): connection,
burst capture with a Saleae-style waveform viewer, live decoded events on a
time-aligned timeline, host-side capture decoding, exports, trace and the
pattern generator. The package is
[unified-ui-plan.md](unified-ui-plan.md) with packets W00-W09. It is a
host-only change: no firmware, PL, register-map or protocol change; the
transport is direct USB only and the ESP32/gateway path is rejected as a UI
connection; the legacy three-pane page stays at `/legacy`; `la-cli` remains
the scripted interface. Planning only until an execution instruction.

The local preset/BSP exposes USB0 and the `usbps` / `XUsbPs` driver, but device
enumeration, physical configuration and throughput are unverified. The new
contract corrects the initial driver and power assumptions and retains the
existing ISR, ownership and ordinary-SG requirements. Do not treat projected
USB speeds as evidence. Preserve the historical P0-P10 results in the ledger.

The root `logic-analyzer-implementation-plan-v0.4.md` is the earlier v0.4
snapshot; it does not contain this addendum. Execute this `docs/plan.md` and
its explicitly linked work instructions, with current evidence in `status.md`.

### Changes in v0.4

| Change | Implementation consequence |
|---|---|
| Fully native Windows workflow | No WSL, Linux VM, Docker, Bash, Make, usbipd or Linux CI dependency. |
| Reuse the DDR3 PHY development approach | SystemVerilog testbenches, Questa batch simulation and a Python regression runner. No required cocotb/Verilator flow. |
| Use installed AMD 2026.1 tools | Vivado Tcl and Vitis Python scripts; verify platform generation and an Arm FreeRTOS build in P0/P3/P4. |
| Native C tests | MSVC + CMake + Ninja; Python calls a stable C ABI DLL; Windows threading adapter replaces pthreads. |
| ESP-IDF testing split | Mocked portable logic on Windows; Unity/FreeRTOS/peripheral tests on the actual ESP32. No ESP-IDF `linux` target. |
| All project files inside the existing folder | Project root is `C:\Users\lingg\Projects\KCCI_technical\Embedded\Logic-Analyzer\Source_Code`; source, environments, dependencies, generated workspaces and outputs stay beneath it. |
| Ordinary scatter-gather DMA for burst and live | No simple-transfer calls on an SG core; cyclic hardware mode disabled; descriptor reuse follows ownership. |
| Time-bounded live packets | Flush the current run and close a partial DMA packet on a 2 ms timer as well as the size limit. |
| Safe multi-producer event transport | FreeRTOS queue of static event-buffer handles; no unprotected shared MessageBuffer. |
| Correct live throughput gates | Count timestamps and framing; 1 MHz SPI with per-byte events is an overload test on UART, not a zero-drop promise. |

Scope remains eight 3.3 V channels on JC, 100 MS/s acquisition target, Zynq PS FreeRTOS protocol decoding, ESP32 FreeRTOS gateway, and burst export plus live event display. 200 MS/s and higher-throughput links remain stretch work. The analyzer uses the Zynq PS memory controller for onboard DDR; it does not require porting the custom `ddr3_phy` core or its company primitives.

---

## 0. Goals

1. Build a working 8-channel logic analyzer.
2. **Zybo Z7-10** (Zynq-7010: dual Cortex-A9 @ 667 MHz + Artix-7 fabric): acquisition, trigger and transition-encoding in the PL; capture management and **live protocol decoding** in FreeRTOS on the PS.
3. **ESP32** (classic dual-core, ESP-IDF v5.x FreeRTOS): wireless gateway, live event relay, web view.
4. RTOS design as a first-class deliverable on both processors: task pipelines, zero-copy buffer pools, ISR→task hand-off, queues/stream/message buffers, timers, watchdogs, back-pressure, measured latency and throughput.
5. Host tool: burst capture → `.sr`/VCD for PulseView/GTKWave; live decode → terminal/CSV stream.
6. Fully scriptable from a clean checkout.

### Two operating modes

| Mode | PL output | DMA | PS work | Link payload |
|---|---|---|---|---|
| **Burst** | raw packed (4 samples/word) or transition-encoded | ordinary SG, finite descriptor chain | store, then drain | raw/RLE samples after capture |
| **Live** | transition-encoded | ordinary SG with controlled recycling; e.g. 16 × 256 KiB capacities, variable actual lengths | decode each completed packet | decoded events within measured link capacity |

### Implementation baseline (changes must be documented)

| Decision | Choice |
|---|---|
| Channels | 8 digital, 3.3 V LVCMOS, Pmod **JC** (0 Ω, high-speed) |
| Sample rate MVP / stretch | 100 MS/s / 200 MS/s (IDDR) |
| Capture memory | 1 GB DDR3L (reserve e.g. 256 MB for capture pools) + ≤128 KB BRAM pre-trigger ring |
| Decoders MVP | UART, SPI, I2C (C, table-driven, no malloc) |
| Zynq↔ESP32 | UART 921600→2 Mbps with RTS/CTS on **JD** (MVP); SPI via PL `axi_quad_spi` slave, ESP32 master ≤20 MHz on JD (P9) |
| ESP32↔Host | Wi-Fi TCP, same framing; WebSocket live view (P10) |
| Toolchain | Vivado/Vitis **2026.1**, native Questa FPGA Starter Edition smoke-tested in P0, native ESP-IDF pinned 5.x patch release, 64-bit CPython, MSVC Build Tools + CMake + Ninja; exact versions recorded in `tools/toolchain-lock.json` |
| **Development host** | **Native Windows only**, including all required CI jobs; PowerShell/Python/Tcl automation (see §2A) |

---

## 1. Architecture

```
            8 × 3.3 V inputs — Pmod JC (0 Ω, high-speed)
                              │
┌─────────────────────────────▼───────────────────────────────────────────────┐
│ ZYBO Z7-10 (Zynq-7010, 1 GB DDR3L)                                          │
│                                                                             │
│  PL @100 MHz                                  PS: FreeRTOS on CPU0          │
│  ┌────────┐ ┌───────┐ ┌───────┐ ┌─────────┐   ┌──────────┐  ┌────────────┐  │
│  │2-flop  │→│sampler│→│trigger│→│txn enc  │→  │ AXI DMA  │→ │ DDR: seg   │  │
│  │sync    │ │ /DIV  │ │+pre-  │ │{run,smp}│   │ S2MM     │  │ pool (live)│  │
│  └────────┘ └───────┘ │trg ring│ │/raw pack│   │ SG       │  │ cap buf    │  │
│                       └───────┘ └─────────┘   │ noncyclic│  │ (burst)    │  │
│  ┌────────────────────┐  ┌────────────┐       └────┬─────┘  └─────▲──────┘  │
│  │AXI-Lite regs       │  │patgen → JE │            │IRQ           │zero-copy│
│  └────────────────────┘  └────────────┘            ▼              │         │
│                                        AcqTask ─seg_q─▶ DecodeUartTask ┐    │
│                                          │           ▶ DecodeSpiTask   ├ev_q│
│                                          │           ▶ DecodeI2cTask   ┘  │  │
│                                        CmdTask ◀─────────────── TxTask ◀──┘  │
│                                        StatusTask (stats, CPU load, SWDT)   │
│                                        PS UART0 (EMIO → Pmod JD, RTS/CTS) ──┼─┐
└─────────────────────────────────────────────────────────────────────────────┘ │
                                                                               │
┌──────────────────────────────────────────────────────────────────────────────▼┐
│ ESP32 (ESP-IDF FreeRTOS SMP)                                                  │
│  Core 1: LinkRxTask, LinkTxTask         Core 0: WifiMgr, NetServerTask,       │
│          (UART ISR → parser → ringbuf)          LiveViewTask (WS), UiTask, WDT│
└──────────────────────────────────────────────────────────────────┬────────────┘
                                                        Wi-Fi TCP  │  WebSocket
                                             ┌─────────────────────▼──────────┐
                                             │ Host: la-cli  → .sr/.vcd/.png  │
                                             │       la-cli live → tty / CSV  │
                                             └────────────────────────────────┘
```

### 1.1 Throughput and latency budget

MB/s is decimal; KiB/MiB denote powers of two. These are calculations and targets, not measured board guarantees.

| Path | Calculation | Consequence |
|---|---|---|
| Raw acquisition | 100 MS/s × 1 byte = 100 MB/s | Verify end-to-end PL/DMA/DDR sustained throughput. |
| Worst-case transition stream | One 4-byte word per sample = 400 MB/s | RLE can expand data 4×. A 32-bit 100 MHz AXIS path has no stall margin at this rate; flag overload, never silently lose samples. |
| UART link, 8N1 | 921600/10 = 92,160 B/s; 2,000,000/10 = 200,000 B/s before protocol overhead | RTS/CTS pauses and framing reduce useful capacity. |
| UART 115200 decoded bytes | 11,520 byte events/s × 12 B/event = 138,240 B/s before outer framing | Target zero drops at a verified 2 Mbps link; full-rate events do not fit at 921600. |
| SPI 1 MHz, one 8-bit data direction | 125,000 byte events/s × 12 = 1.5 MB/s before outer framing and CS events | Overload test on UART. Both directions approximately double byte-event load. |
| SPI 10 MHz, one direction | 1.25 MB/s data becomes 15 MB/s with per-byte events | The proposed 1.5 MB/s SPI transport does not make this event format lossless. |
| SPI transport 20 MHz | 2.5 MB/s theoretical; ≥1.5 MB/s measured target | Requires actual peripheral servicing and signal-integrity validation. |
| Burst drain | 64 MB / 0.2 MB/s = 320 s theoretical minimum on 2 Mbps UART | Limit depth or compress low-transition captures; measure real drain time. |
| Wi-Fi | Sustained rate to be measured on the actual ESP32 variant | Do not use an assumed Wi-Fi rate as an acceptance guarantee. |

**Live packet deadline:** a 256 KiB capacity is not a requirement to fill that buffer. Emit TLAST when 64 K words are reached **or 2 ms of FCLK time has elapsed**, whichever comes first. On timer expiry, first emit the pending run (including unchanged input), then TLAST. The timer is independent of `SAMPLE_DIV`. At lower sample rates with no new sample yet, wait for the next valid sample rather than inventing one; characterize that latency separately. Normal low-rate UART tests use 100 MS/s sampling. Bound stalls or report overflow.

Without this timer, 65,536 transitions at 115200 baud require at least about 569 ms even at maximum edge activity; sparse traffic can wait far longer. TxTask's packet timer alone cannot fix acquisition latency. Use a 2 ms TxTask flush target; measure the complete 25 ms p99 UART-byte-to-Zynq-link event budget in P8, separately from Wi-Fi/UI latency.

The 4 MiB live pool is only about 41.9 ms of raw buffering at 100 MB/s (10.5 ms at 400 MB/s encoded). Partial packets consume descriptors too: sixteen 2 ms packets provide approximately 32 ms of descriptor runway even when most buffer bytes are unused. Neither pool size nor non-cached memory guarantees consumers keep up. Decoder throughput of 5 M transitions/s/task is an experimental target sharing one CPU, not three independent CPU budgets.

---

## 2. Repository Layout

The existing folder containing this plan is the project root: `C:\Users\lingg\Projects\KCCI_technical\Embedded\Logic-Analyzer\Source_Code`. Create the following entries directly inside it; do not create another enclosing project directory. Relative paths throughout this document resolve from this root.

```
Source_Code/
├── AGENTS.md  README.md  Justfile  .gitattributes  CMakeLists.txt (native tests)
├── .venv/              (ignored host Python environment)
├── .deps/esp-idf/      (ignored pinned ESP-IDF source and its submodules)
├── .tools/espressif/   (ignored IDF tools and its separate Python environment)
├── build/             (ignored questa/, native/, vivado/, vitis/, esp32/, release/, reports/, tmp/, cache/)
├── docs/
│   ├── architecture.md  regmap.yaml  regmap.md  protocol.md
│   ├── rtos-design.md   decoders.md  test-plan.md  wiring.md  research-review.md
├── fpga/
│   ├── rtl/  tb/ (SystemVerilog + vectors)  constraints/  scripts/ (build.tcl, program.tcl)  board_files/
├── fw-zynq/
│   ├── scripts/ (platform.py, app.py, debug.py, bootgen.bif)
│   ├── app/ src/ (tasks/, drivers/, decoders/)  FreeRTOSConfig.h  lscript.ld  translation_table.S
│   └── tests/host/  (native MSVC tests: protocol, decoders, segment pool)
├── fw-esp32/  main/  components/{la_link,la_net,la_live}/  sdkconfig.defaults  test/
├── common/la_proto/   (framing, CRC, packing — compiled on Zynq, ESP32, host)
├── common/la_decode/  (UART/SPI/I2C decoders — compiled on Zynq and host for tests)
├── host/  la_cli/ (transports: tcp, serial COMx)  la_sigrok_bridge/  tests/  pyproject.toml
├── tools/  check_env.py  gen_regmap.py  win/ (setup.ps1)  models/ (Python reference models: patgen, txn encoder, decoders)
└── tests/hil/
```

The decoders live in `common/la_decode/` on purpose: the same C runs on the Zynq under FreeRTOS and natively on the host under pytest via ctypes, so decoder correctness is CI-tested without hardware.

---

## 2A. Development Environment (fully native Windows)

**User requirement: every required development step runs on Windows.** Do not introduce a Linux/WSL/container prerequisite when a tool or test fails. Diagnose the Windows flow and report a concrete blocker if necessary.

### Existing setup observed on 2026-09-05

| Item | Evidence and follow-up |
|---|---|
| Host | Windows 11 Home Single Language, build 26200, approximately 32 GB RAM. AMD lists supported Windows editions separately; Home is not in that matrix. Successful local builds remain a P0 gate. |
| Vivado | `C:\AMDDesignTools\2026.1\Vivado`; `vivado -version` ran successfully. |
| Vitis | `C:\AMDDesignTools\2026.1\Vitis`; FreeRTOS BSP files found in the shared installation. Arm compiler availability and platform/application build are not yet proven. |
| Questa | `C:\altera_lite\25.1std\questa_fse\win64`; reuse Python → vlib/vlog/vsim orchestration from the DDR3 project as a reference only. Smoke-test the actual license/features before committing to the harness. |
| ESP32 tooling | PlatformIO Arduino-ESP32 packages found; this does not prove native ESP-IDF is installed. |
| Python/build tools | Create dedicated project environments and verify compiler/tool availability. Do not depend on another application's Python venv. |
| Boards | Only Bluetooth COM ports observed; exact ESP32 module, usable GPIOs, USB drivers and physical wiring remain unverified. |

### Tool responsibilities

| Work | Native Windows tool |
|---|---|
| RTL unit/integration simulation | Questa, self-checking SystemVerilog testbenches, Python regression runner |
| Compile/lint gate | Questa compile warnings plus naming/static checks; Vivado synthesis/DRC/timing for implementation |
| FPGA build/programming | Vivado batch Tcl, hw_server, Bootgen |
| Zynq application | Vitis 2026.1 Python API via `vitis -s`, Arm FreeRTOS BSP |
| ESP32 application | Official native ESP-IDF installation, `idf.py build/flash/monitor` |
| Portable C and mocked application tests | MSVC x64, CMake, Ninja, CTest, pytest/ctypes |
| Actual ESP32 RTOS/peripheral tests | ESP-IDF Unity test application on the board, driven through Windows COM |
| CLI, export, HIL | Dedicated 64-bit Windows CPython, pyserial/TCP, pytest, PulseView |
| Recipes | Python is the canonical runner; PowerShell and optional `just` wrappers call it |

**Project location is fixed by the user:** use `C:\Users\lingg\Projects\KCCI_technical\Embedded\Logic-Analyzer\Source_Code` as the single repository and development root. This is the folder already containing this plan. Initialize Git here if needed. Keep source, documentation, firmware, board files, captures, local dependencies, Python environments, generated workspaces, logs, test results and release packages inside this folder. Do not relocate the project or use a separate checkout/build tree elsewhere.

**Path resolution:** derive `PROJECT_ROOT` from the runner's own location (for example, `Path(__file__).resolve().parents[1]` in `tools/run.py`), not the shell's current directory. Pass that root to Tcl/Vitis scripts explicitly and construct child paths with `pathlib` or `file join`. Use ignored `build/{questa,native,vivado,vitis,esp32,release,reports,tmp,cache}` directories. Run simulators with a working directory beneath `build/questa` so transcript/WLF/work libraries remain local. Configure CMake, Vitis, Vivado and ESP-IDF output/workspace directories explicitly; host captures default to `build/captures`.

**Project-owned dependencies and environments:** host Python lives in `.venv`; pinned ESP-IDF source lives in `.deps/esp-idf`, with its tools and separate managed Python environment under `.tools/espressif`. Set `IDF_PATH` and `IDF_TOOLS_PATH` to these absolute project-local paths before IDF setup/build. Route project tool caches/downloads and child-process `TEMP`/`TMP` into `build/cache` and `build/tmp` where supported. Record pinned versions in the lock file, ignore downloaded/generated contents, and document commands to recreate them. Existing machine-wide application installations (AMD tools, Questa, MSVC, base CPython, drivers and PulseView) stay installed at their current locations; this rule concerns project-owned files, not relocating installed applications or OS-managed data.

**Path-length gate:** this root is longer than the previously suggested checkout. Keep generated project/component names and directory nesting short; run real native Questa, CMake, Vivado, Vitis and ESP-IDF smoke builds at this exact root during P0/P3/P4. Do not claim path compatibility from the Windows long-path setting alone. If a tool hits a limit, first shorten internal generated names while preserving this physical root; report any unresolved blocker instead of silently moving sources or outputs elsewhere.

**Environment handling:** use native AMD, ESP-IDF and Visual Studio developer environments in separate child processes so their Python/compiler PATH entries do not contaminate one another. A `.bat` executed as a child of PowerShell cannot export environment changes back into the parent: execute the tool in the same `cmd /c` invocation as its setup batch file, or explicitly import its environment in the Python wrapper. Pass arguments safely and propagate every nonzero exit code. Keep project host Python separate from the ESP-IDF-managed Python.

**Questa harness:** use procedural SV testbenches, deterministic seeded stimulus, explicit scoreboards and portable checks; avoid mandatory UVM, constrained randomization or functional-coverage features until supported by the installed edition. Python generates vector files and compares result files with reference models. Invoke `vlib`, `vlog -sv`, and `vsim -c` directly; no Make dependency. Detect fatal errors, timeout, missing PASS marker and process failure; handle UTF-8/UTF-16 simulator logs. This follows the DDR3 regression approach without modifying that project.

**C tests:** build DLLs with an exported C ABI and explicit-width integers using MSVC x64. Python loads them through ctypes. Isolate firmware HAL and synchronization behind adapters; use Windows synchronization for host race tests and the actual FreeRTOS mechanisms for board tests. Host timing cannot validate RTOS deadlines or interrupt behavior.

**USB:** keep Zybo FTDI JTAG/console and ESP32 USB attached directly to Windows. Install the driver appropriate to the observed board. Console UART1 is for logs; acquisition commands use UART0 on JD through a separate flow-control-capable USB-UART adapter, or the ESP32 TCP gateway. Do not mistake the Zybo console COM port for the analyzer data endpoint. Select ports explicitly when enumeration is ambiguous.

**Windows hygiene:** preserve the fixed project root, keep internal paths short, use repository-local Git settings, LF for SV/C/Python/Tcl and explicit CRLF for batch files. Long-path registry changes and Defender exclusions are not prerequisites and must not be applied automatically. Do not disable Secure Boot for optional serial bridges.

**CI:** native Windows only. Hosted Windows runners can run portable C/Python jobs; dedicated Windows runners with installed, licensed tools run Questa/Vivado/Vitis. A native ESP-IDF runner builds firmware. Hardware tests use a bench runner with exclusive device access and a person present. CI is optional initially; all release gates must also be runnable locally with logged evidence. No Linux job or container is required.

## 3. Rules for Agents (`AGENTS.md`)

1. Never edit generated files (`la_regs.h`, `la_regs_pkg.sv`, `regs.py`); edit `docs/regmap.yaml`, run `python tools/run.py gen`.
2. Every RTL module has a self-checking SystemVerilog test passing in native Questa before block-design integration. Python reference models provide independent expectations. `python tools/run.py sim` delegates to the canonical Python runner.
3. Every protocol or decoder change updates `common/` and its native tests (`python tools/run.py test-native`).
4. Script build/test steps through Python, Vivado Tcl, Vitis Python and native ESP-IDF. Initial installers and physical bench actions may require a person.
4a. **Native Windows only (§2A).** No required WSL, Bash, Make, Docker, Verilator, cocotb or ESP-IDF Linux host target. A failed Windows gate is a blocker to diagnose, not permission to switch development OS.
4b. Keep every project-owned file and generated workspace beneath the fixed §2 root; do not substitute a different checkout or output folder. Automation uses Python, PowerShell, Tcl and optional Justfile wrappers. Follow `.gitattributes`; use `pathlib` and Tcl path functions, configurable roots and enumerated COM ports. Commands must work in the documented native shells.
5. **[HUMAN]** steps: stop, print exact instructions, wait.
6. RTOS rules: no busy-waiting; ISRs only notify/give/send-from-ISR; acquisition buffers passed by handle; bounded copies for event packing and transport framing are permitted and measured; every shared object has an owner documented in `rtos-design.md`; every task reports stack high-water mark; every queue has a documented full-policy (block / drop-oldest / drop-newest).
7. DMA rule: align buffer allocations and descriptors to 64 bytes; reserve allocation capacities in multiples of 32 bytes. Actual received lengths may be shorter, including raw tails. Non-cached mappings or correct cache maintenance cover descriptors as well as payloads. Never recycle memory owned by hardware or a consumer; only AcqTask changes descriptor ownership.
8. One task ID per commit, message prefixed with the ID. Done = deliverable + verification passes + docs updated.

---

## 4. Phase Overview

| Phase | Name | Output | HW? |
|---|---|---|---|
| P0 | Environment & scaffolding | tools verified, skeleton, CI | No |
| P1 | Specifications | regmap, protocol, RTOS design, decoder spec, models | No |
| P2 | FPGA RTL | sync, sampler, trigger, ring, txn encoder, patgen, regs — simulated | No |
| P3 | Vivado integration | BD, bitstream, `.xsa`, timing clean | Build no / program yes |
| P4 | Zynq FreeRTOS core | burst capture (ordinary SG DMA) + link + drain | Yes |
| P5 | ESP32 bridge | UART↔TCP relay with flow control | Yes |
| P6 | Host tool & E2E burst | `.sr`/VCD of self-test pattern in PulseView | Yes |
| **P7** | **Live decode pipeline** | ordinary SG DMA, segment pool, decoder tasks, event stream | Yes |
| P8 | RTOS hardening & measurement | latency/throughput numbers, traces, fault injection | Yes |
| P9 | Performance | SPI link, RLE-in-burst, 200 MS/s stretch | Yes |
| P10 | UX & release | WebSocket live view, sigrok bridge, BOOT.BIN, docs | Yes |

Order: P0→P1→{P2 ∥ native tests of P4/P5/P7 decoders ∥ P6 host package}→P3→P4→P5→P6→P7→P8→P9→P10.

### Implementation status (2026-09-14)

This section keeps the plan honest about what is built; `docs/status.md` is the
authoritative evidence ledger and phase status.

- P0–P6 are complete, including the P6.4 PulseView evidence and the
  `v0.1-burst` tag.
- P7 is complete on bench: UART/SPI/I²C live decoders, the segment pool and
  live SG DMA, compact-adjacent event batching, the 60 s zero-drop gate, the
  isolated-byte latency gate (428 us worst case against the 25 ms p99 budget)
  and the P7.7 decoded-pattern stop. Tag `v0.2-live`.
- P8 is complete (P8.6 documented skip), including fault injection, the 500
  capture soak, the cached live pool (+28 % decoder throughput) and the trace
  pipeline with an annotated live-UART trace.
- P9.2 is complete (compact `EVT_BLOCK` events, measured 51.6–53.3 % wire
  saving on the 115200 live stream). P9.1 is bench-clean for framed bursts at
  5/10/18 MHz and a 4 MiB byte-verified capture; the >=1.5 MB/s 64 MB drain
  gate is blocked on the ESP32 Wi-Fi host transport (0.28 MB/s measured), and
  the plan already names the ESP32-S3 USB-OTG path as the replacement. P9.3
  remains a stretch decision; the MVP stays at 100 MS/s. P9.4 Wi-Fi tuning
  remains open.
- P10.1 is bench-verified (embedded HTTP page plus `/ws` decoded-event
  stream). P10.2 is deferred by this plan's own condition: no currently
  compatible signed virtual-COM pair driver is installed, so the SUMP bridge
  cannot be verified without changing driver/security state; the assessment is
  recorded in `docs/sigrok-bridge.md`. P10.3 documentation is consolidated in
  the tracked `docs/` set and the `web-artifact/` portal. P10.4 tooling exists
  (`python tools/run.py boot` plus the release packaging step) but the final
  BOOT.BIN, the [HUMAN] SD boot and the `v1.0` tag wait for the ESP32-S3
  transport port, per the recorded bench decision.

---

## 5. Phase Details

### P0 — Environment & Scaffolding

- **P0.0 Windows prerequisites [HUMAN, once]** — verify existing AMD 2026.1 and Questa first. Install missing native ESP-IDF, Visual Studio Build Tools (Desktop development with C++, MSVC x64 and Windows SDK), CMake, Ninja, 64-bit CPython, and PulseView. `just` is optional convenience. Confirm exact ESP32 module before selecting IDF target/GPIO mapping. Record versions without changing global tools used by DDR3. Provision project-owned IDF source/tools under `.deps/esp-idf` and `.tools/espressif` as specified in §2A.
- **P0.1 Environment checker** — implement `tools/check_env.py`: native Windows check; configurable AMD/Questa/IDF paths; report resolved executables and versions for Python, vlib/vlog/vsim, Vivado, Vitis, hw_server, Bootgen, cl, CMake, Ninja and idf.py. Detect missing Arm compiler/device packages. Verify that all configured project environments, workspaces, temporary/cache paths and output directories resolve beneath PROJECT_ROOT; test actual builds at this root and report path-limit failures. `python tools/check_env.py` must pass required checks or name the exact missing prerequisite. Write `tools/toolchain-lock.json` after successful smoke tests; pin exact ESP-IDF patch, Python packages, board-file commit and AMD version.
- **P0.2 Scaffold** — create §2 layout, `.gitignore`, `.gitattributes`, and `tools/run.py` with targets `gen sim lint test-native test-esp32-mocks test-esp32-target bit fw-zynq fw-esp32 program flash-esp test-host hil release`. Canonical invocation: `python tools/run.py <target>`. Optional `just <target>` delegates to this runner. Use Windows PowerShell syntax without `&&` assumptions. Force LF for source, CRLF for `.bat/.cmd`; mark binary artifacts as binary. Initialize Git in this existing Source_Code folder if needed; do not create a second checkout or renormalize unrelated repositories. Ignore `.venv/`, `.deps/`, `.tools/` and `build/`; create local temp/cache/output directories and configure child processes to use them.
- **P0.3 Python environment** — create project `.venv` using the selected CPython; pin `pyserial numpy pyvcd click pyyaml pytest matplotlib` and install the host package editable after scaffolding. Keep ESP-IDF's managed environment separate. Gate: host import and pytest smoke pass using the explicitly selected interpreter.
- **P0.3a C harness** — CMake/Ninja build from the MSVC developer environment; export a C ABI DLL for ctypes. Create deterministic protocol/decoder tests and a Windows thread-based segment-ownership stress test. Example commands: `cmake -S . -B build/native -G Ninja`, `cmake --build build/native`, `ctest --test-dir build/native --output-on-failure`. Run them through `test-native`; validate both x64 ABI and nonzero-error propagation.
- **P0.3b Questa harness** — implement `tools/regress.py` and `fpga/tb/testlist.json`. Smoke-test compile, simulation, PASS detection, intentional failure and timeout. Use procedural SV and Python vector/result exchange. No licensed advanced verification feature is assumed. If the installed edition cannot run this harness, document the exact failure and resolve simulator availability before P2; do not silently skip simulation.
- **P0.3c Firmware tools smoke** — native `idf.py --version` plus ESP32 hello-world build; verify Vitis Python CLI and Arm compiler availability. Once P3 supplies the XSA, require a minimal Zynq FreeRTOS application build before P4 driver work. Existence of an installation directory is not a successful firmware-build gate.
- **P0.4 CI** — Windows portable-test job plus dedicated Windows Questa/AMD/ESP-IDF jobs as described in §2A. Keep toolchain versions aligned with local locks. Hardware jobs are explicit bench jobs; unavailable CI never masquerades as a passing test.
- **P0.5 Board files** — pin `Digilent/vivado-boards` by commit; query the installed board-part name/version rather than assuming `part0:1.2`; target `xc7z010clg400-1`. Record preset and installed device support.
- **P0.6 Documentation** — write `docs/research-review.md` with the v0.4 corrections and `docs/windows-dev.md` with exact native setup/build/test/flash commands and troubleshooting. Copy this plan to `docs/plan.md` as the implementation source of truth. Gate: all planned commands are mapped to a runner target, missing prerequisites are explicit, and no required Linux dependency remains.

---

### P1 — Specifications

**P1.1 Register map** (`docs/regmap.yaml` → generated headers). AXI-Lite, 32-bit, base `0x43C0_0000`.

| Off | Name | Acc | Meaning |
|---|---|---|---|
| 0x00 | `ID` | RO | `0x4C41_0002` |
| 0x04 | `CTRL` | RW | [0] ARM (self-clr) [1] ABORT [2] SOFT_RESET [3] SELFTEST_EN [4] **STREAM** (continuous, no POST count) |
| 0x08 | `STATUS` | RO | [0] IDLE [1] ARMED [2] TRIGGERED [3] DONE [4] FIFO_OVF [5] ABORTED [6] STREAMING |
| 0x0C | `SAMPLE_DIV` | RW | f_s = 100 MHz / (DIV+1) |
| 0x10 | `MODE` | RW | [0] ENCODING: 0 = raw packed (4 samples/word), 1 = **transition** `{run[23:0], sample[7:0]}`; [1] IDDR_EN (P9) |
| 0x14 | `CHAN_EN` | RW | [7:0] |
| 0x18 | `TRIG_MASK` | RW | [7:0] level-trigger participation |
| 0x1C | `TRIG_VALUE` | RW | [7:0] |
| 0x20 | `TRIG_RISE` | RW | [7:0] |
| 0x24 | `TRIG_FALL` | RW | [7:0] |
| 0x28 | `PRE_SAMPLES` | RW | ≤ implemented pre-trigger depth (P2.4); validate together with POST size and FIFO stall budget |
| 0x2C | `POST_SAMPLES` | RW | ignored when STREAM=1 |
| 0x30 | `TRIG_INDEX` | RO | sample index of trigger |
| 0x34 | `WORD_COUNT` | RO | 32-bit words emitted (low), 0x38 high |
| 0x3C | `IRQ_EN` / 0x40 `IRQ_STAT` (W1C) | | [0] DONE [1] TRIGGERED [2] OVERFLOW |
| 0x44 | `PATGEN_CTRL` | RW | [0] EN, [7:4] MODE: 0 counter, 1 walking-1, 2 PRBS8, 3 **UART 115200 "LA-TEST\n"**, 4 **SPI 1 MHz frame**, 5 **I2C 400 kHz write** ; [31:16] DIV |
| 0x48 | `TIMEOUT` | RW | trigger timeout, sample clocks |
| 0x4C | `TXN_MAX_RUN` | RW | Maximum run length, 1..2^24−1; timer flush may close a shorter run |
| 0x50 | `LIVE_FLUSH_CYCLES` | RW | FCLK cycles between live packet flushes; default 200000 = 2 ms at 100 MHz; validate nonzero in STREAM |

**P1.2 Generator** `tools/gen_regmap.py` → C header, SV package, Python constants. Idempotence gate: run `python tools/run.py gen` twice, compare outputs and run `git diff --exit-code` as a separate command.

**P1.3 Protocol** (`docs/protocol.md`). Same framing on UART and TCP so the ESP32 relays verbatim.
Frame: `SOF 0xA5 | TYPE | SEQ | LEN(2 LE) | PAYLOAD | CRC16-CCITT` (CRC over TYPE..PAYLOAD), max payload 1024 B.

| TYPE | Name | Dir | Payload |
|---|---|---|---|
| 0x01/0x02 | `PING`/`PONG` | H→Z / Z→H | — / fw version, PL ID, uptime |
| 0x10 | `CONFIG` | H→Z | mirrors acquisition RW regs including `TXN_MAX_RUN` and `LIVE_FLUSH_CYCLES`; validate values before ARM |
| 0x11/0x12/0x13 | `ARM` / `ABORT` / `GET_STATUS` | H→Z | — |
| 0x14 | `DECODER_CFG` | H→Z | slot(1), type(1: 0 off,1 UART,2 SPI,3 I2C), params (union, see P1.5) |
| 0x15/0x16 | `LIVE_START` / `LIVE_STOP` | H→Z | — |
| 0x20 | `STATUS` | Z→H | state, STATUS reg, word_count, trig_index, **per-decoder counters (events, dropped segs), CPU load, min free stack** |
| 0x21 | `EVT_TRIGGERED` | Z→H | trig_index |
| 0x22 | `EVT_DECODED` | Z→H | batch: n × {slot(1), kind(1), t_sample(8), len(1), data[len]} — kinds: BYTE, FRAME_ERR, PARITY_ERR, START, STOP, ACK, NACK, ADDR, CS_ASSERT, CS_RELEASE |
| 0x23 | `EVT_OVERRUN` | Z→H | dropped segment count, stage (dma / decoder / link) |
| 0x30/0x31/0x32 | `DATA_BEGIN` / `DATA_CHUNK` / `DATA_END` | Z→H | total words, encoding, rate / offset + ≤1016 B / crc32 |
| 0x40/0x41 | `ACK` / `NAK` | both | type+seq / +error |
| 0x7F | `LOG` | Z→H | text |

Flow control: host ACKs every 16 DATA_CHUNK frames; EVT_DECODED is best-effort with drop counters and reserved control/status capacity. UART also uses RTS/CTS. The baseline BYTE record costs 12 bytes; batching outer frames does not remove per-event timestamps. Define any future compact block format as a versioned protocol extension before using its improved bandwidth in acceptance tests. Decoder segment loss must reset protocol state and produce a discontinuity indication; never decode across unknown samples as though contiguous.

**P1.4 Protocol library** `common/la_proto/`: C99, no malloc, incremental parser, CRC16/32, pack/unpack. Native tests + fuzz (random bytes never crash; resync on SOF). `python tools/run.py test-native`.

**P1.5 Decoder spec** (`docs/decoders.md`) and library `common/la_decode/`. Input abstraction: a stream of transitions `(t_sample: u64, sample: u8)`; decoders keep small state machines, emit events through a callback. Params:
- UART: rx channel, baud (derived to samples/bit from f_s), data bits 5–9, parity N/E/O, stop bits, inverted flag. Mid-bit sampling; framing/parity error events; idle detection uses `TXN_MAX_RUN` heartbeat words.
- SPI: SCK, MOSI, MISO, CS channels (any may be −1), CPOL, CPHA, word size 8/16, MSB/LSB first, CS active level.
- I2C: SCL, SDA; emits START/ADDR(+R/W)/BYTE/ACK/NACK/STOP; handles clock stretching implicitly (edge-driven).
Python reference models in `tools/models/` generate stimulus + expected events; C decoders are tested against them natively via ctypes. Record native decoder throughput as a regression metric; it does not establish A9 throughput, which must be measured on target.

**P1.6 RTOS design** (`docs/rtos-design.md`) — the central RTOS artifact.

**Zynq PS — FreeRTOS 10.x Xilinx port, CPU0, tick 1 kHz, static allocation.**

| Task | Prio | Stack | Waits on | Does |
|---|---|---|---|---|
| `AcqTask` | 6 | 2 KB | DMA/PL notifications and control queue | Owns all ordinary SG descriptor operations. Burst: finite capture chain. Live: collects actual lengths, assigns sequence/time metadata, establishes references before publishing handles to each decoder queue, and releases failed enqueue references. Recycles only after all successful consumers release. No blocking on decoders; no free buffer leads to explicit acquisition overflow/stop. |
| `DecodeUartTask` / `DecodeSpiTask` / `DecodeI2cTask` | 5 | 3 KB | `seg_q[i]` | Decode valid words, allocate static event blocks, enqueue handles to multi-producer FreeRTOS `ev_q`; drop/count on exhaustion. Release segment reference and notify AcqTask on last release. Detect sequence gaps, reset decoder state and report discontinuities. |
| `CmdTask` | 5 | 2 KB | StreamBuffer from UART RX ISR (8 KB) | parse → validate → dispatch (`ctrl_q` to AcqTask; decoder config via `xTaskNotify` + mutex-protected config struct; ACK accounting semaphore for TxTask) |
| `TxTask` | 4 | 2 KB | `ev_q`, `tx_q`, control notifications, bounded ACK wait | Packs events into ≤1 KiB frames; flush target 2 ms. Returns static event blocks after consumption. Service commands/status during burst ACK waits; drive the single-producer UART TX StreamBuffer. |
| `StatusTask` | 1 | 1 KB | `vTaskDelay(500)` | stack watermarks, run-time stats → CPU load, per-stage counters, pets SWDT, heartbeat LED |
| Idle hook | — | — | — | idle counter; `vApplicationStackOverflowHook` + `configASSERT` → LOG + halt |

Design decisions to document: per-decoder queues and CPU budgets; static handle ownership; event pool size (initial 16 KiB budget) and multi-producer queue capacity; zero-wait drop policy with reserved error/status reporting; SG descriptor runway using both byte and packet rates (§1.1). Use a portable ownership/synchronization adapter for Windows tests and a FreeRTOS implementation on target. StreamBuffers remain single-writer/single-reader; no unsynchronized multiple writers to a MessageBuffer. Avoid unsupported assumptions about memcpy speed; measure CPU/memory costs.

ISRs: UART RX → `xStreamBufferSendFromISR`; PL IRQ → `xTaskNotifyFromISR(AcqTask, bits)`; DMA S2MM → `xTaskNotifyFromISR(AcqTask)`. No processing in ISRs. GIC via `XScuGic`, FreeRTOS handles the tick on the SCU private timer.

Cache policy: payload pool, capture buffer and SG descriptors start in a reserved Normal Non-cacheable DDR region. Specify linker bounds, MMU section alignment and barriers; ensure no overlap with firmware, heap, stack or FSBL use. Generated BSP files are not edited manually: apply a scripted supported configuration/override. If enabling caches later, maintain descriptor and payload coherency separately, with aligned allocations and actual-length-aware cache ranges.

**ESP32 — ESP-IDF v5.x FreeRTOS SMP, tick 1 kHz.**

| Task | Core | Prio | Waits on | Does |
|---|---|---|---|---|
| `LinkRxTask` | 1 | 20 | UART event queue; `rx_full_threshold` lowered (e.g. 64) | parse frames → `esp_ringbuf link_to_net` (32 KB NOSPLIT); mirrors `EVT_DECODED` into `live_rb` for LiveViewTask |
| `LinkTxTask` | 1 | 18 | `net_to_link` ringbuf | UART TX with RTS/CTS |
| `NetServerTask` | 0 | 12 | `select()` 100 ms | TCP :5555, single client, relay both ways; on disconnect send `ABORT` + `LIVE_STOP` |
| `LiveViewTask` (P10) | 0 | 8 | `live_rb` | WebSocket broadcast of decoded events, keeps last-N ring for late joiners |
| `WifiMgr` | 0 | event loop | Wi-Fi/IP events | STA, backoff reconnect, mDNS `la-esp32.local` |
| `UiTask` | 0 | 5 | delay 250 ms | LED states, stats print |
| Task WDT | — | — | — | LinkRx + NetServer, 5 s |

Timing budgets (measured in P8): ISR→task ≤20 µs Zynq and ≤100 µs ESP32; UART byte→complete event on Zynq UART link ≤25 ms p99 at input 115200, link 2 Mbps and a non-overloaded workload. Live packet flush 2 ms and transmit batch flush 2 ms are initial targets. Decoder throughput ≥5 M transitions/s/task is measured with shared CPU loading. Descriptor-service turnaround target ≤200 µs is not permission for a 200 µs gap in the data path: the 8 KiB FIFO covers only about 82 µs at 100 MB/s. Measure Wi-Fi/UI latency separately.

**P1.7 Wiring** (`docs/wiring.md`):
- **JC** pins 1–4, 7–10 → CH0–7 (3.3 V only; note JC pairs are impedance-matched differential pairs used single-ended — expect crosstalk on adjacent pins at fast edges; keep leads short).
- **JD** pin 1 → ESP32 RX, pin 2 → ESP32 TX, pin 3 → ESP32 CTS (Zynq RTS), pin 4 → ESP32 RTS (Zynq CTS), GND↔GND. GPIO16/17/14/15 are provisional classic-ESP32 examples only: confirm exact module, PSRAM/strapping restrictions and schematic before wiring. Reserve JD 7–10 for SPI and allocate an additional verified data-ready pin in P9.
- **JE** 1–4, 7–10 → patgen outputs; jumper JE→JC for external self-test.
- ESP32 on its own USB; common ground; **do not** connect JC to 5 V targets without a level shifter.
- JP5: JTAG for dev, SD for release; power-cycle after changing.

---

### P2 — FPGA RTL (simulation only)

SystemVerilog-2012, single 100 MHz FCLK clock and synchronous active-high reset for MVP. Each RTL module has `fpga/tb/tb_<name>.sv` with a scoreboard; Python reference models generate vectors and compare outputs. Run through native Questa using `python tools/run.py sim`.

- **P2.1 `la_input_sync`** — 2-flop synchronizer per channel, optional majority glitch filter. Test: latency, filter.
- **P2.2 `la_sampler`** — divider; 8-bit `sample`, `sample_valid`. Test DIV ∈ {0,1,9,99}.
- **P2.3 `la_trigger`** — level ∧ (any RISE) ∧ (any FALL), empty edge masks = true; one-cycle `trig`. Table-driven vectors.
- **P2.4 `la_pretrig_ring`** — 32 KiB-class BRAM circular history. Before RTL, specify simultaneous write/read behavior, retained pre-trigger history, arriving post-trigger sample storage and the worst-case output stall bound. A simple frozen-ring replay followed by pass-through loses samples. Use a proven dual-port buffered schedule with enough retention for in-flight samples; reject unsupported PRE/POST combinations or flag overflow. Tests: zero/max pre-history, wrap, trigger during stalls, continuous input during replay and exact pre/post seam with no duplicate/lost sample.
- **P2.5 `la_txn_encoder`** — output `{run[23:0], sample[7:0]}` represents a positive count of samples at that value. Emit on change, count limit, packet flush and capture end; never emit zero-length runs. Maintain state under AXIS back-pressure; timer flush splits an unchanged run without creating a false edge. Test against Python for all boundaries and stall/overflow cases.
- **P2.6 `la_raw_packer`** — 4 samples → 1 word, `tkeep` on tail. Test.
- **P2.7 `la_capture_ctrl`** — IDLE→ARMED→TRIGGERED→CAPTURING→DONE; finite POST or STREAM, timeout/abort and raw/encoded selection. Live TLAST at 64 K words or `LIVE_FLUSH_CYCLES`, after flushing pending run/raw tail. Respect tkeep and actual lengths. A stop/error must quiesce the producer and DMA before buffers return to FREE. Tests include isolated UART byte followed by long idle, timer/edge coincidence, partial packets, abort and bounded/no-progress stalls.
- **P2.8 `la_axis_fifo`** — 32-bit × 2048 (8 KiB BRAM), sticky overflow. It covers approximately 82 µs at raw 100 MB/s, or 20 µs at worst-case encoded 400 MB/s. Sampling cannot be paused by TREADY: explicitly stop/mark a gap on exhaustion. Test that overload is detected and never silently corrupts time/sample continuity.
- **P2.9 `la_patgen`** — modes per `PATGEN_CTRL`, including UART/SPI/I2C stimulus generators (bit-banged from ROM tables) so protocol decoders can be self-tested end-to-end without external hardware. Test vs `tools/models/patgen.py`.
- **P2.10 `la_regs`** — AXI-Lite slave from generated package; W1C and self-clearing ARM. Test with a procedural SV AXI-Lite BFM: independent AW/W arrival, response stalls, byte strobes, readback and invalid register/config cases.
- **P2.11 `la_top`** — integration, irq, SELFTEST mux, blink. SV AXI-Lite BFM configures/arms; SV AXIS sink checks handshakes/tkeep/TLAST and writes captured words to a file. Python reconstructs samples and independently decodes UART to `LA-TEST` followed by newline. Add controlled sink stalls, overflow and live idle-flush cases. Same vectors drive native C decoder tests.
- **P2.12** `python tools/run.py lint` compiles all RTL/testbench sources with Questa, reports warnings and checks project naming/static rules; `python tools/run.py sim` passes all cases. Review warnings explicitly; compile success is not timing closure. Vivado DRC/synthesis/timing gates follow in P3.

---

### P3 — Vivado Integration

- **P3.1 `fpga/scripts/build.tcl`** (batch): project on `zybo-z7-10` board part; `la_top` as RTL module reference (`create_bd_cell -type module -reference la_top`); `processing_system7` with board preset (DDR, UART1 on MIO for console), **UART0 on EMIO** (TX/RX/RTS/CTS → JD), FCLK_CLK0 100 MHz, IRQ_F2P, S_AXI_HP0 64-bit; `axi_dma` S2MM only, **32-bit stream**, **SG enabled for both burst and live; cyclic mode disabled**, buffer length width 26; `axi_interconnect`; connect both M_AXI_S2MM payload writes and M_AXI_SG descriptor traffic to PS DDR, and map AXI-Lite controls from the PS. Include proc_sys_reset with synchronized resets and validate all clocks/address segments; `xlconcat` IRQs. `write_hw_platform -fixed -include_bit`. **Runs on native Windows** from a `settings64.bat` shell; project created under `PROJECT_ROOT/build/vivado` (forward slashes in TCL, `file normalize` everywhere); outputs collected under `PROJECT_ROOT/build/release`. `python tools/run.py bit` is the native build entry point.
- **P3.2 Constraints** `zybo_z7_10.xdc`: JC inputs LVCMOS33 (optional PULLDOWN), JE outputs, JD UART0 EMIO pins, documented asynchronous-input timing exceptions to the first synchronizer stage; ASYNC_REG placement and CDC review. Do not leave placeholder delays as release constraints. Timing must have zero failing endpoints; save utilization to `docs/architecture.md` (expect a few K LUTs on the 17.6 K available).
- **P3.3 `program.tcl`** via `hw_server` — native Windows only (FTDI JTAG stays attached to Windows). **[HUMAN]** USB-JTAG, JP5=JTAG. Verify blink.

---

### P4 — Zynq FreeRTOS Core (burst mode)

- **P4.1 Vitis Python platform/app** — `fw-zynq/scripts/platform.py` and `app.py`, run through native Vitis 2026.1 `vitis -s`; derive exact API calls from installed version documentation/examples. Create the Vitis workspace under `PROJECT_ROOT/build/vitis` and collect release artifacts under `PROJECT_ROOT/build/release`. Create platform from XSA, processor `ps7_cortexa9_0`, available `freertos10_xilinx` domain, stdout UART1; build hello FreeRTOS first. Script BSP configuration for static allocation, timers, runtime/trace stats, stack overflow checks and assertions using supported options. Do not assume a `configUSE_STREAM_BUFFERS` switch exists; check actual kernel sources/BSP. Preserve reproducible config inputs, linker and MMU overrides. Gate: `python tools/run.py fw-zynq` emits ELF and build log with pinned versions; no XSCT compatibility branch required.
- **P4.2 Hello RTOS** — all tasks as stubs; StatusTask prints task list/watermarks. **[HUMAN]** flash; 10 min no assert.
- **P4.3 MMU/linker setup** — script a supported non-cached mapping for capture payload and SG descriptors; 64-byte-aligned allocations, cache-line-rounded capacities. Boot assertions verify address bounds/alignment and non-overlap. Test DMA writes and CPU readback; a CPU-only write/read test does not prove DMA coherency.
- **P4.4 Register driver + selftest** (`ID`, `SAMPLE_DIV` round-trip).
- **P4.5 IRQ plumbing** (`XScuGic`, notifications only); latency measured with `XTime_GetTime` and reported in `STATUS`.
- **P4.6 DMA burst driver** — `XAxiDma` ordinary SG with a finite preposted chain. No `XAxiDma_SimpleTransfer` on this SG-enabled core. Respect configured descriptor length field (26 bits) and allocation alignment; read actual completed lengths including short final packets. Provision sufficient raw or worst-case encoded capacity before arming; stop explicitly on capacity exhaustion. Test multi-BD capture, short tail, errors/reset and abort without consumer reuse races.
- **P4.7 AcqTask (burst path)** — CONFIG/ARM/ABORT/timeout state machine, `EVT_TRIGGERED`, hands `TxJob` to TxTask.
- **P4.8 Link UART0 driver** — `XUartPs` on EMIO, RTS/CTS enabled, RX trigger-level IRQ → StreamBuffer; TX via StreamBuffer + TX-empty IRQ. 921600 first.
- **P4.9 CmdTask + TxTask** — dispatch, windowed ACK, streaming CRC32.
- **P4.10 Serial bring-up (no ESP32)** — **[HUMAN]** USB-UART with RTS/CTS on JD (appears as `COMx` on Windows; HIL tests run from the Windows venv). `tests/hil/test_zynq_serial.py`: 4096-word counter capture (raw packed) matches model; transition-encoded UART patgen capture decodes to `"LA-TEST\n"` in Python. Repeat 100× to shake out cache/alignment issues.

---

### P5 — ESP32 Bridge

- **P5.1 Skeleton** — native ESP-IDF with exact patch and target pinned. Translate intended settings into supported Kconfig names: 1 kHz FreeRTOS tick, task WDT, runtime/trace stats, UART RX buffer 16 KiB and measured lwIP parameters. Wi-Fi credentials via local Kconfig/NVS inputs excluded from Git. From `fw-esp32`, use the wrapper to invoke `idf.py -B <PROJECT_ROOT/build/esp32> build` then **[HUMAN]** `idf.py -B <PROJECT_ROOT/build/esp32> -p <COM-port> flash monitor`; placeholders are resolved to absolute paths by the runner. Confirm module before assigning GPIOs/task affinity; S3 has a different pin map, C3 requires single-core adaptation.
- **P5.2 `la_link`** — UART RTS/CTS, RX FIFO threshold around 64 bytes, event queue, static ownership and link buffers; pin tasks only when valid for the selected chip. Test parser/routing/failure logic with mocked UART/network/RTOS adapters compiled by MSVC (`test-esp32-mocks`). Add native-built ESP-IDF Unity test firmware for actual queues, notifications, ring buffers, UART and watchdog behavior (`test-esp32-target`, **[HUMAN]**). No ESP-IDF `linux` target; host mocks do not validate RTOS scheduling.
- **P5.3 `la_net`** — Wi-Fi STA with backoff, `NetServerTask` single-client TCP :5555, relay verbatim, `TCP_NODELAY`, on disconnect send `ABORT`+`LIVE_STOP`; mDNS.
- **P5.4 UiTask + task WDT.**
- **P5.5 Loopback (no Zynq)** — TX↔RX + RTS↔CTS jumpers; `test_esp32_loopback.py`: 10 MB at 921600 then 2 Mbps, zero CRC errors; record throughput and RX-overrun counter (`UART_FIFO_OVF` events must be 0).

---

### P6 — Host Tool & End-to-End (burst)

- **P6.1 `la_cli`** — transports `tcp://host:port` and `serial://<port>` where `<port>` is `COM5`, `\\.\COM12` (auto-prefixed for COM ≥10); `la-cli ports` lists candidates via `serial.tools.list_ports` with VID/PID matching (CP210x/CH340/FTDI); identify ports by VID/PID and serial number; require explicit selection when ambiguous, never hard-code a default; `ping`, `status`, `capture --rate 10M --words 64k --mode txn|raw --trig-rise 0 --pre 1000 --out cap.sr|cap.vcd [--png]`. `.sr` writer (zip: `version`, `metadata`, `logic-1-1`, unitsize 1), VCD via `pyvcd`. Reconstruct transition runs into samples for `.sr` export with bounded-memory streaming; test raw and encoded round trips. Python decoders (from `tools/models`) can annotate offline. `pytest host/tests` with a mock server.
- **P6.2 E2E self-test** — **[HUMAN]** full wiring. `test_e2e_burst.py`: patgen modes × rates {1M,10M,100M}, internal mux, zero mismatches, zero overflow, 3 consecutive runs.
- **P6.3 External loopback** JE→JC (real I/O + synchronizers) — same test with `SELFTEST_EN=0`.
- **P6.4** Open `.sr` in PulseView, screenshot into docs. Tag `v0.1-burst`.

---

### P7 — Live Decode Pipeline (RTOS centrepiece)

- **P7.1 Segment pool** — 16 × 256 KiB non-cached capacity, actual packet length tracked separately; states FREE→DMA_OWNED→CPU_OWNED→FREE, generation/sequence counters and synchronized refcounts. Windows thread-based race tests force queue-full, slow consumers, last-release and abort races; repeat essential ownership tests under FreeRTOS on target. No descriptor republished while any consumer owns its payload.
- **P7.2 Ordinary SG live DMA** — prepost free buffers using the supported XAxiDma BD-ring APIs with cyclic mode disabled. Completion IRQ notifies AcqTask; it collects all completed descriptors, publishes handles and requeues only fully released buffers in driver-supported order. A zero-consumer packet may recycle immediately. Queue-full for one decoder releases only that reference, not the entire buffer. On pool exhaustion, detect overflow and stop acquisition, record loss, quiesce/reset safely; never overwrite CPU-owned memory. HIL: raw counter 100 MS/s for 10 s with decoders off, checking sequence/data and overflow (not merely absence of DMA errors), then encoded traffic with decoders. Test idle partial packets and deliberately delayed consumers.
- **P7.3 Decoder tasks** — wrappers around `common/la_decode`, per-task `seg_q` depth 8; safe config publication, static event block allocation and multi-producer FreeRTOS queue `ev_q`. Reset decoder state on segment discontinuity. Unit tests verify ordering, multiple writers, pool exhaustion and event/segment release on every error path.
- **P7.4 CmdTask/TxTask extensions** — `DECODER_CFG`, `LIVE_START/STOP`, `EVT_DECODED` batching timer, `EVT_OVERRUN`, `STATUS` counters.
- **P7.5 Host `la-cli live --decoder uart:rx=0,baud=115200 [--decoder spi:...]`** — prints timestamped events; `--csv`; `--stats` shows per-stage drops and CPU load every second.
- **P7.6 E2E live gates** — at a verified 2 Mbps UART link, one continuous UART 115200 stream for 60 s with zero drops; isolated final byte followed by idle must also meet 25 ms p99. Start SPI zero-drop tests at 10 kHz and I2C at 10 kHz, one decoder at a time; count actual encoded wire bytes and require at least 20% headroom against measured link capacity. For simultaneous UART/SPI/I2C use independent signals on available channels and reduce offered rates to preserve total headroom. Record both SPI directions and all control events. SPI 1 MHz/10 MHz are deliberate overload tests with accurate loss counters, state reset and no crash, not zero-drop UART gates. Increase documented supported rates only after measurements; compact event format or faster transport is separate P9 work.
- **P7.7 Protocol-aware stop (stretch within P7)** — `DECODER_CFG.stop_on` byte pattern: a decoder match makes AcqTask `ABORT` and freeze only still-owned completed segments for burst drain after quiescing DMA; retaining a last-K history requires a separately budgeted history pool and explicit ownership → "trigger on decoded value", a feature hardware LAs charge for. Tag `v0.2-live`.

---

### P8 — RTOS Hardening & Measurement

- **P8.1 Priority/ownership review** — table of every queue/buffer: producer, consumer, depth, full-policy, measured max occupancy. Mutexes only where needed (priority inheritance). No priority inversion paths (document).
- **P8.2 Measurements** (`la-cli bench`): ISR→task latency p50/p99/max; segment turnaround; decoder transitions/s per task (feed synthetic segments from a test hook); UART-byte→event-on-wire latency; CPU load per task from run-time stats; ESP32 byte→frame and frame→socket latencies. Fill `rtos-design.md#results`.
- **P8.3 Tracing** — custom `traceTASK_SWITCHED_IN/OUT` + queue trace macros into a ring dumped via `LOG`; render a timeline with a Python script (or SystemView/Tracealyzer if available). Include one annotated trace of a live UART decode showing DMA IRQ → AcqTask → DecodeUartTask → TxTask → UART TX.
- **P8.4 Fault injection** — Wi-Fi drop mid-live (Zynq must return to IDLE within 1 s); TCP kill; garbage bytes flood (parser resync, no task death); ACK starvation (TxTask blocks, AcqTask unaffected, `EVT_OVERRUN(link)` reported); hang `LinkRxTask` via test hook → ESP32 task WDT resets; overload decoders with 10 MHz SPI stimulus → controlled drop-newest, no crash, counters accurate; 500 back-to-back burst captures.
- **P8.5 Cached vs non-cached experiment** — decoder throughput with segments in cached memory + `Xil_DCacheInvalidateRange` vs non-cached; pick and document.
- **P8.6 (Stretch) AMP** — move decoder tasks to CPU1 (second FreeRTOS or bare-metal loop) with an OCM mailbox; segment handles cross cores; measure gain. Document even if skipped.

Exit: budgets from P1.6 met or revised with justification.

---

### P9 — Performance

- **P9.1 SPI link** — PL `axi_quad_spi` slave on JD 7–10; start 10 MHz and characterize 200 Ω series-resistor effects before 20 MHz. Assign verified data-ready GPIO. Design PS FIFO service, transaction sizing, underrun protection and DMA support based on the actual IP; do not assume memory-to-SPI DMA exists. ESP32 master DMA reads framed bulk data; UART retains commands/ACKs. Target ≥1.5 MB/s over a 64 MB drain, measured. ESP32-S3 USB-OTG may replace the ESP32→host transport, but cannot bypass a slow Zynq→ESP32 UART link.
- **P9.2 Compression/compact events** — quantify raw vs transition burst sizes for actual workloads; no fixed compression ratio promised. Implement versioned block/delta timestamps if higher live rates are required, retaining timing/error semantics and round-trip tests. Recalculate full-duplex SPI event bandwidth before revising P7 gates. RLE reconstruction in `.sr` export is required already in P6.
- **P9.3 200 MS/s stretch** — define IDDR or 200 MHz sampler clocking, CDC and two-sample packing before RTL. Validate the actual JC bank voltage, IO standard, receiver requirements and board wiring before any differential/LVDS proposal; do not assume LVDS_25 works on the existing 3.3 V bank. Timing/CDC/DRC and physical signal integrity must pass. Keep the released MVP at 100 MS/s if this is unproven.
- **P9.4 Wi-Fi tuning** — lwIP buffers, TCP window, `TCPIP_TASK_AFFINITY`; record MB/s.

---

### P10 — UX & Release

- **P10.1 Live web view** — `esp_http_server` + WebSocket `LiveViewTask`: decoded bytes scrolling, per-decoder tabs, stats; single-page HTML embedded in flash.
- **P10.2 sigrok integration (optional)** — native Windows SUMP bridge translates PulseView OLS serial requests into analyzer TCP transactions. If a currently compatible signed virtual-COM driver is available, use a COM pair plus TCP bridge; verify on the actual Windows/Secure Boot setup without changing security settings. Otherwise defer this optional feature. No Unix PTY or WSL fallback. `.sr`/VCD file export remains the required integration; SUMP capture support does not imply continuous display of custom decoded events.
- **P10.3 Docs** — architecture, wiring photos, RTOS write-up with measured numbers and traces, known limits (3.3 V only, 100 MS/s, link rates).
- **P10.4 Release** — `bootgen -image fw-zynq/scripts/bootgen.bif -arch zynq -o BOOT.BIN` (FSBL + bit + `la_fw.elf`) **[HUMAN]** SD + JP5=SD; ESP32 `.bin` set; Python wheel. Tag `v1.0`.

---

## 6. Test Strategy

All commands below use `python tools/run.py <target>`; optional `just` recipes are aliases. Run relevant gates after changes and all required gates before release. Store tool versions, seed, elapsed time, exit status and PASS/FAIL evidence in `build/reports`.

| Layer | Tool | Target / gate |
|---|---|---|
| RTL units and top contract | Native Questa + SV BFMs/scoreboards + Python vectors | `lint`, `sim`; every RTL change |
| Protocol/decoders/ownership | MSVC CMake/Ninja, CTest, pytest/ctypes, Windows thread stress | `test-native`; every common/pool change |
| Zynq dispatch | Native mocked HAL + board verification | `test-native`; dispatch changes and HIL before release |
| ESP32 portable logic | MSVC + mocked adapters | `test-esp32-mocks`; component changes |
| ESP32 real RTOS/peripherals | ESP-IDF Unity firmware executed on ESP32 through native Windows | `test-esp32-target`; relevant target changes and before tags, **[HUMAN]** |
| Host tool/export | Windows pytest + mock TCP/serial, `.sr` round-trip | `test-host`; host/protocol changes |
| Bitstream/Zynq firmware | Native Vivado Tcl + Vitis Python | `bit`, `fw-zynq`; RTL/platform/firmware changes |
| ESP32 firmware | Native ESP-IDF | `fw-esp32`; firmware/config changes |
| Burst/live/fault/latency | Windows pytest and actual bench | `hil`; before release, **[HUMAN]** |

Native mock tests are logic checks, not evidence of FreeRTOS preemption, DMA/cache coherency, interrupt latency, UART flow control or Wi-Fi behavior. Those require actual target tests. Windows CI uses the same targets without Linux containers. Missing hardware/license/tool prerequisites must be recorded as unverified, never marked passing.

---

## 7. Risks and Mitigations

| Risk | Mitigation / acceptance gate |
|---|---|
| SG mode mismatch or premature buffer reuse | Ordinary SG only; driver APIs and explicit ownership; delay-consumer and abort-race tests. |
| DMA cache incoherency | Non-cached payloads and descriptors first; mapped bounds, barriers and DMA readback tests. |
| Live data stalls in partially filled packets | PL timed run flush + TLAST and bounded Tx batching; isolated-byte/idle latency test. |
| Event rate exceeds UART/CPU | Budget complete wire records; measured headroom, explicit loss/reset, optional compact format and faster transport. |
| RLE expansion or pre-trigger replay loss | Worst-case 400 MB/s analysis; simultaneous capture/replay scoreboard; visible overflow/stop. |
| Concurrent event writers | Multi-producer queue of statically owned event blocks; exhaustion/race tests. |
| Questa Starter Edition limitations | P0 compile/run/negative-test smoke; use supported procedural SV features; resolve unavailable capabilities before RTL work. |
| Windows/Vitis setup mismatch | Existing AMD 2026.1 paths, pinned Python API and native firmware-build smoke. Windows Home is not a vendor support guarantee. |
| Missing native ESP-IDF or compiler dependencies | Install exact pinned tool versions; isolated environments; Arduino packages are not a substitute. |
| Host mocks mistaken for target evidence | Separate required ESP32 Unity, DMA and HIL gates. |
| USB/pin assumptions | Enumerate observed devices, verify exact module and flow-control wiring, separate console/data ports. |
| BRAM/timing/signal integrity | Review actual utilization, CDC/DRC and asynchronous input constraints; scope-check external signals; 200 MS/s stretch only. |
| Optional PulseView virtual-COM compatibility | File export required; bridge deferred if no compatible signed driver exists. |
| Paths and shell failures | Fixed user-selected root, short internal names, actual path-length smoke builds; explicit tool child-process environments; propagate failure and parse simulator logs correctly. |

---

## 8. Kick-off Prompt for a Coding Agent

> Implement this v0.4 plan using fully native Windows entirely inside `C:\Users\lingg\Projects\KCCI_technical\Embedded\Logic-Analyzer\Source_Code`. This existing folder is PROJECT_ROOT; keep all project sources, dependencies, environments, generated workspaces, tests, captures and releases beneath it. Do not create an external checkout or build tree. Derive paths from the runner and validate tool path limits at this root. First inspect the actual repository and applicable AGENTS.md. If the repository is only this plan, start P0 scaffolding and make docs/plan.md the implementation source of truth; do not assume generated documentation or runners already exist. Use installed Vivado/Vitis 2026.1, native Questa SystemVerilog tests driven by Python, MSVC/CMake/Ninja native tests, and native ESP-IDF. No WSL, Linux containers, Make or ESP-IDF linux host target is required or authorized as a replacement workflow. Verify exact tool versions and the Questa feature/license smoke in P0. Use ordinary SG DMA for both modes with cyclic mode disabled, explicit buffer ownership, timed live packet flushing and a multi-producer event queue. Apply the corrected full-record throughput gates. Execute phases in order; run relevant verification, record evidence and update status per task. Perform human bench steps only when hardware/access is available and authorized. Do not mark unperformed target tests as passing. Keep the DDR3 PHY project unchanged. If a baseline change becomes necessary, explain the concrete reason before dependent implementation.

## 9. Reference evidence for the revision

- [AMD AXI DMA driver](https://raw.githubusercontent.com/Xilinx/embeddedsw/master/XilinxProcessorIPLib/drivers/axidma/src/xaxidma.c): `XAxiDma_SimpleTransfer` rejects SG-enabled cores.
- [AMD cyclic DMA behavior](https://docs.amd.com/r/en-US/pg021_axi_dma/Cyclic-DMA-Mode): hardware revisits descriptors continuously until stopped/reset; software refcounts do not gate it.
- [FreeRTOS MessageBuffer writer requirements](https://freertos.org/Documentation/02-Kernel/04-API-references/09-Message-buffers/03-xMessageBufferSend): multiple writers require serialization; this plan instead uses a queue.
- [Espressif host application testing](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/host-apps.html): Linux/POSIX host simulation is not a native Windows substitute for target testing. Check matching pinned-IDF documentation during implementation.
- [AMD OS support matrix](https://www.amd.com/en/support/adaptive-socs-and-fpgas/installer-info-general.html): distinguish native Windows feasibility from officially listed editions.
- [Vitis embedded development documentation](https://docs.amd.com/r/en-US/ug1400-vitis-embedded/Main-Page): use the installed 2026.1 Python API and matching examples.

Local environment observations are dated in §2A and must be rechecked at development start. Arithmetic in §1.1 follows this plan's packet format, not a measured hardware claim.
