import type { RunnerTargetDoc, RtlTestDoc, TestLayerDoc } from './types';

export const TEST_LAYERS: TestLayerDoc[] = [
  {
    layer: 'Reference models and unit tests',
    tool: 'Python reference models in tools/models plus the host pytest suite',
    target: 'test-host',
    boundary:
      'Shows that the host tool and the Python models agree with the recorded specifications. It exercises no RTL, no firmware and no hardware.',
  },
  {
    layer: 'Self-checking RTL simulation',
    tool: 'Questa FPGA Starter Edition procedural testbenches',
    target: 'lint then sim (12/12 testcases)',
    boundary:
      'Proves each RTL block against an in-testbench model. It is not synthesis, timing closure, DRC, DMA coherency or pad electrical behavior.',
  },
  {
    layer: 'Native firmware logic tests',
    tool: 'MSVC CMake/Ninja CTest suites and mocked ESP32 adapters',
    target: 'test-native, test-esp32-mocks',
    boundary:
      'Checks pure firmware cores and mocked adapters on Windows. It does not prove FreeRTOS preemption, interrupt latency, cache coherency or throughput.',
  },
  {
    layer: 'Target Unity and firmware builds',
    tool: 'ESP-IDF Unity app, Vitis platform/app build and native ESP-IDF build',
    target: 'test-esp32-target, fw-zynq, fw-esp32',
    boundary:
      'Exercises real-target unit behavior and confirms the firmware builds. A green build is not proof of the full end-to-end path or of timing.',
  },
  {
    layer: 'FPGA implementation',
    tool: 'Vivado synthesis, implementation, timing and DRC plus XSDB JTAG load',
    target: 'bit, jtag-zynq',
    boundary:
      'Confirms timing closure, resource use and successful programming. It does not prove protocol correctness on the wire.',
  },
  {
    layer: 'Bench end-to-end',
    tool: 'Windows pytest over the real Zynq host path (direct USB or the ESP32 gateway)',
    target: 'test-e2e, hil',
    boundary:
      'Covers the recorded wiring and rates. Unrecorded rates, weak Wi-Fi and missing jumpers remain unverified rather than passing.',
  },
  {
    layer: 'Release packaging',
    tool: 'Register generation, native precheck, profile-aware bootgen BOOT.BIN and optional bin/wheel staging',
    target: 'release, boot',
    boundary:
      'Packaging and precheck only. The W11 USB SD boot is verified (18 Oct 2026); the v1.0 tag/publication and public USB identity remain separate gates.',
  },
];

export const RUNNER_TARGETS: RunnerTargetDoc[] = [
  {
    target: 'gen',
    validates:
      'Regenerates the C header, SystemVerilog package, Python constants and docs/regmap.md from docs/regmap.yaml, then runs the generator again with --check to prove the generated views are current.',
    requires: 'Project Python environment only; no hardware or licensed tool.',
    report: 'build/reports/gen.json (ignored local)',
    evidence:
      'PASS 2026-09-06; re-confirmed gen --check up to date 2026-09-12 (P8.5 gates).',
    status: 'host-tested',
  },
  {
    target: 'sim',
    validates:
      'Runs all 12 self-checking Questa testcases (11 RTL blocks plus the top-level integration) and checks each per-test PASS result marker.',
    requires:
      'Questa FPGA Starter Edition 2025.2 (licensed tool); run after lint, sequentially.',
    report: 'build/reports/sim.json (ignored local)',
    evidence:
      'PASS 12/12 testcases on 2026-09-11 after the patgen single-shot mode.',
    status: 'simulation-tested',
  },
  {
    target: 'lint',
    validates:
      'Compiles every RTL and testbench source in fpga/tb/testlist.json with Questa and reviews warnings; 12/12 compile-only testcases.',
    requires: 'Questa FPGA Starter Edition 2025.2 (licensed tool).',
    report: 'build/reports/lint.json (ignored local)',
    evidence: 'PASS 12/12 with zero remaining warnings on 2026-09-11.',
    status: 'simulation-tested',
  },
  {
    target: 'test-native',
    validates:
      'Builds and runs the MSVC CTest suites for the protocol library, decoders, ownership, trace, SPI cores and the pure firmware cores (la_native_*).',
    requires: 'MSVC x64, CMake and Ninja; no hardware.',
    report: 'build/reports/test-native.json (ignored local)',
    evidence: 'PASS 15/15 on 2026-09-13 with the P7.7 stop-on vectors.',
    status: 'host-tested',
  },
  {
    target: 'test-esp32-mocks',
    validates:
      'Builds and runs the mocked ESP32 adapter tests: relay classification and counters, network session policy and the la_web core.',
    requires: 'MSVC x64, CMake and Ninja; no hardware.',
    report: 'build/reports/test-esp32-mocks.json (ignored local)',
    evidence: 'PASS 4/4 on 2026-09-14 with la_esp32_mocks_web added by P10.1.',
    status: 'host-tested',
  },
  {
    target: 'test-esp32-target',
    validates:
      'Builds the ESP-IDF Unity test app; with a COM port it flashes, executes and captures the on-target Unity suite.',
    requires:
      'Native ESP-IDF v5.5.2 plus [HUMAN] ESP32 hardware, a selected COM port and the manual BOOT/EN download mode.',
    report:
      'build/reports/test-esp32-target.json and build/reports/test-esp32-target-capture.log (ignored local)',
    evidence:
      'PASS 5/5 on the target 2026-09-10 (relay, static queue, NOSPLIT ring, UART2 loopback, task WDT); without --port the build completes and the run reports BLOCKED.',
    status: 'target-tested',
  },
  {
    target: 'bit',
    validates:
      'Runs the Vivado batch build: block design validation, synthesis, implementation, timing, DRC, bitstream and XSA generation.',
    requires:
      'AMD Vivado 2026.1 (licensed tool); the build itself needs no board.',
    report:
      'build/reports/bit.json; signoff files under build/release (ignored local)',
    evidence:
      'PASS 2026-09-13 with zero failing endpoints (WNS +0.247 ns, WHS +0.019 ns), DRC 0 errors, LUT 36.74 %, BRAM 61.67 %; that bitstream is loaded on the bench.',
    status: 'bench-verified',
  },
  {
    target: 'fw-zynq',
    validates:
      'Runs the Vitis Python platform/app build from the XSA and emits the selected profile ELF (la_app_usb by default; la_app for --profile gateway) with the pinned linker script and FreeRTOS policy.',
    requires:
      'AMD Vitis 2026.1 (licensed tool) and an XSA produced by the bit target.',
    report: 'build/reports/fw-zynq.json (ignored local)',
    evidence:
      'PASS 2026-09-19 for both profiles (USB la_app_usb text 209072 / data 3720 / bss 294784; gateway la_app text 191144 / data 3688 / bss 243936); USB is the default profile since the 2026-09-20 V00 pivot.',
    status: 'bench-verified',
  },
  {
    target: 'jtag-zynq',
    validates:
      'Programs the PL, runs the FSBL to handoff, downloads the selected profile ELF (la_app_usb by default; la_app with --profile gateway) and starts CPU0 through XSDB over JTAG.',
    requires:
      'AMD Vitis XSDB, a Zybo Z7-10 on JTAG with its FTDI attached, and the bit/ELF artifacts; [HUMAN] bench access.',
    report: 'build/reports/jtag-zynq.json (ignored local)',
    evidence:
      'PASS 2026-09-13 for the gateway profile and repeated USB-profile loads; the default profile flipped to USB in V00. W11 USB firmware 0x000A0000 was also JTAG-loaded on 2026-10-17 before the verified SD image replaced the need for normal JTAG reloads.',
    status: 'bench-verified',
  },
  {
    target: 'fw-esp32',
    validates:
      'Builds the ESP-IDF gateway (la_board, la_proto, la_link, la_net, la_web and main) for the classic ESP32 and emits la_gateway.bin.',
    requires:
      'Native ESP-IDF v5.5.2 provisioned under .deps; the first build needs registry access for the pinned espressif/mdns component.',
    report:
      'build/reports/fw-esp32.json; build output under build/esp32 (ignored local)',
    evidence:
      'PASS 2026-09-14 with the P10.1 la_web image (la_gateway.bin 0xde2d0) and retained as the optional gateway profile. The current bench ESP32-S3 runs the standalone `demo-esp32-uart` GPIO17 stimulus instead, flashed 2026-10-17 with verified hashes; it is not the gateway image.',
    status: 'bench-verified',
  },
  {
    target: 'boot',
    validates:
      'Packages fsbl.elf, the selected profile application (la_app_usb by default; la_app with --profile gateway) and logic_analyzer.bit into BOOT.BIN with bootgen and verifies the sync header.',
    requires:
      'Python plus Bootgen from Vitis 2026.1; the ELF and bit artifacts from fw-zynq and bit.',
    report:
      'build/reports/boot.json; BOOT.BIN under build/release (ignored local)',
    evidence:
      'W11 USB BOOT.BIN PASS 2026-10-18: 2,415,152 bytes, SHA-256 6cb120cc...a847b892, containing the verified 0x000A0000 application. It was installed on the bench card with matching read-back and cold-booted with JP5=SD; USB UI E2E 6/6 and browser/external-UART checks 10/10 passed.',
    status: 'bench-verified',
  },
  {
    target: 'package',
    validates:
      'Builds the host Python wheel (including the V02 live-web bridge) and, for the selected profile, stages the optional ESP32 flash set (bootloader.bin, partition-table.bin, la_gateway.bin); verifies the wheel contents and writes release-manifest.json with sizes and SHA-256 hashes.',
    requires:
      'Python venv plus the BOOT.BIN artifact; the ESP32 build is required only for --profile gateway.',
    report:
      'build/reports/package.json; artifacts and release-manifest.json under build/release (ignored local)',
    evidence:
      'W11 USB package PASS 2026-10-18: BOOT.BIN 2,415,152 B, host wheel 139,198 B and release-manifest.json; the optional gateway bin set is staged but is not the external ESP32 UART demonstration.',
    status: 'host-tested',
  },
  {
    target: 'program',
    validates:
      'Refuses to guess: it reports BLOCKED and requires JP5=JTAG, the Zybo FTDI and a confirmed target before the documented Vivado program.tcl command is run by hand.',
    requires:
      '[HUMAN] hardware gate: USB-JTAG, JP5=JTAG and a confirmed board.',
    report: 'build/reports/program.json (blocked record; ignored local)',
    evidence:
      'Not run as a runner target in this checkout; PL programming was performed through jtag-zynq (2026-09-10, LD0 blink observed).',
    status: 'blocked',
  },
  {
    target: 'flash-esp',
    validates:
      'Refuses to guess a port: it reports BLOCKED and requires the exact ESP32 COM port and module confirmation before idf.py flash monitor is run by hand.',
    requires:
      '[HUMAN] hardware gate: ESP32 module, COM port and the manual BOOT/EN download mode.',
    report: 'build/reports/flash-esp.json (blocked record; ignored local)',
    evidence:
      'Not run as a runner target in this checkout; gateway images were flashed by hand on 2026-09-14. The current standalone ESP32-S3 UART demo was flashed through the native ROM USB port (COM10) on 2026-10-17 with esptool hash verification.',
    status: 'blocked',
  },
  {
    target: 'test-host',
    validates:
      'Runs the host pytest suite: protocol vectors and parser resync, client/mock flows, export writers, live client and CLI, the V02 host live-web bridge (formatting parity, ring/drop policy, SSE server) and the Wi-Fi helper.',
    requires:
      'Project Python environment with pyserial, numpy, pyvcd, click, pyyaml, pytest and matplotlib; no hardware.',
    report: 'build/reports/test-host.json (ignored local)',
    evidence:
      'W11 final PASS 177/177 on 2026-10-17, including real-run waveform framing/routing, bounded fanout, cancellation and Reset; earlier V02-V09 cases remain covered. Pre-existing matplotlib warnings/pytest temp-link cleanup notices are recorded in the ledger.',
    status: 'host-tested',
  },
  {
    target: 'test-e2e',
    validates:
      'Runs the bench pytest suites against a live analyzer link: burst matrices, live decode gates, fault injection, the P9.1 SPI gates, the P10.1 gateway web gate and the V04 USB live-web bridge gates.',
    requires:
      '[HUMAN] bench: powered Zybo with loaded firmware, the required jumpers and (for the gateway gates) the ESP32 gateway on the network; LA_E2E_TARGET or --target.',
    report: 'build/reports/test-e2e.json (ignored local)',
    evidence:
      'PASS 55 passed + 3 skipped on the P7.7 live suite (2026-09-13), 2/2 for the P10.1 web gate (2026-09-14), 8/8 for the V04 bridge gates over serial://COM13 (2026-09-20), and **6/6** for the W11 unified-UI gates on the cold-booted SD image (2026-10-18); without a target the runner reports BLOCKED.',
    status: 'bench-verified',
  },
  {
    target: 'hil',
    validates:
      'Runs tests/hil against a 3.3 V USB-UART adapter on the JD header for the Zynq serial bring-up path.',
    requires:
      '[HUMAN] hardware: USB-UART adapter on JD-1..4 plus GND, exclusive COM-port access, LA_HIL_PORT or --port.',
    report: 'build/reports/hil.json (ignored local)',
    evidence:
      'Not run in this checkout; the suite collects and skips without --port (preflight 2026-09-10). The ESP32 gateway now proves the on-wire UART path instead.',
    status: 'blocked',
  },
  {
    target: 'release',
    validates:
      'Release precheck: regenerates the registers, configures and builds the native tree, and runs the full CTest suite.',
    requires:
      'MSVC x64, CMake and Ninja; no hardware. The W11 SD boot is verified; the v1.0 tag/publication remains a separate step.',
    report: 'build/reports/release.json (ignored local)',
    evidence: 'PASS 18/18 on 2026-09-13 (P7.7 local gates).',
    status: 'host-tested',
  },
  {
    target: 'check-env',
    validates:
      'Checks every native prerequisite (Python, MSVC, CMake, Ninja, Questa, Vivado, Vitis, Bootgen, Arm GCC, ESP-IDF), resolves project-owned paths and can write tools/toolchain-lock.json.',
    requires: 'Project Python environment; optional --write-lock.',
    report: 'build/reports/check-env.json (ignored local)',
    evidence:
      'PASS 2026-09-06 with no missing P0 prerequisites; the lock records the observed versions (Python 3.11.15, MSVC 19.44, Vivado/Vitis 2026.1, ESP-IDF v5.5.2).',
    status: 'host-tested',
  },
  {
    target: 'query-board',
    validates:
      'Queries the installed Digilent board files and Xilinx device data through Vivado and can update tools/board-lock.json with the board part and source commit.',
    requires: 'AMD Vivado 2026.1 (licensed tool).',
    report:
      'build/reports/query-board.json and board-query.json (ignored local)',
    evidence:
      'PASS 2026-09-06; board part digilentinc.com:zybo-z7-10:part0:1.2 and the vivado-boards commit are locked.',
    status: 'host-tested',
  },
];

export const RTL_TESTS: RtlTestDoc[] = [
  {
    block: 'la_input_sync',
    testbench: 'tb_la_input_sync',
    covers:
      'Two-cycle synchronizer latency (three with the majority filter), reset, per-channel independence, single-cycle glitch rejection and a two-cycle pulse pass.',
  },
  {
    block: 'la_sampler',
    testbench: 'tb_la_sampler',
    covers:
      'DIV in {0, 1, 9, 99} period checks, chan_en masking, aligned sample_valid and reset.',
  },
  {
    block: 'la_trigger',
    testbench: 'tb_la_trigger',
    covers:
      'Level semantics with an empty edge mask, no spurious first-sample edge, divider-gap tolerance and per-cycle mirror parity.',
  },
  {
    block: 'la_pretrig_ring',
    testbench: 'tb_la_pretrig_ring',
    covers:
      'Stream pass-through, exact pre/post seam with no duplicate or lost sample, history wrap, K clamp, stall overflow with an exact drop count, double trigger, abort and clean rerun.',
  },
  {
    block: 'la_txn_encoder',
    testbench: 'tb_la_txn_encoder',
    covers:
      'Predicted-word event model parity, run totals equal accepted samples, max_run splits, mid-run and no-op flushes, a flush behind a stalled word and reset.',
  },
  {
    block: 'la_raw_packer',
    testbench: 'tb_la_raw_packer',
    covers:
      'Byte-stream reconstruction with tkeep, packet length and tlast structure, flush tails, burst packet counts, live-to-burst tlast isolation and stalls.',
  },
  {
    block: 'la_capture_ctrl',
    testbench: 'tb_la_capture_ctrl',
    covers:
      'Full, window-only and empty burst emission targets and word counts, timeout, abort, overflow drain, stream timer and 65536-word flushes.',
  },
  {
    block: 'la_axis_fifo',
    testbench: 'tb_la_axis_fifo',
    covers:
      'Order and transport wrap, fill and stall, non-compliant drop with sticky overflow and clear, count accuracy and reset.',
  },
  {
    block: 'la_patgen',
    testbench: 'tb_la_patgen',
    covers:
      'Counter, walking-1 and PRBS8 models with DIV scaling plus recorded-history UART 115200, SPI and I2C sequences.',
  },
  {
    block: 'la_regs',
    testbench: 'tb_la_regs',
    covers:
      'AXI-Lite BFM: independent AW/W order, BREADY/RREADY stalls, byte strobes, self-clearing ARM/ABORT/SOFT_RESET, masked fields, PRE/TXN clamps, IRQ W1C and out-of-range access.',
  },
  {
    block: 'la_top',
    testbench: 'tb_la_top',
    covers:
      'AXI BFM plus AXIS sink and word-to-sample reconstruction: the txn UART selftest decodes LA-TEST and the raw counter burst emits exactly K+POST samples with one tlast.',
  },
  {
    block: 'lint matrix',
    testbench: 'fpga/tb/testlist.json',
    covers:
      'The compile-only pass over all 12 testcases with explicit warning review. It proves the sources compile cleanly, not that any block behaves correctly.',
  },
];
