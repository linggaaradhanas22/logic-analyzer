import type { RoadmapPhase } from './types';

export const ROADMAP: RoadmapPhase[] = [
  {
    id: 'P0',
    name: 'Environment and scaffolding',
    outcome:
      'Native Windows toolchain verified, project skeleton, canonical runner, environment/board locks, CI scaffold, native C tests, Questa harness and an ESP-IDF smoke build.',
    status: 'host-tested',
    statusNote:
      'Complete locally: the runner, checker, locks and smoke builds pass on this machine. Hosted CI hardware jobs and physical gates are not evidence of completion.',
    evidence:
      'check-env PASS with missing_prerequisites [] and tools/toolchain-lock.json written (2026-09-06); query-board PASS with the board part and board-file commit locked; native CTest and Questa smoke tests pass.',
    remaining:
      'No open P0 item. Hardware and licensed-tool steps stay explicit bench jobs rather than CI substitutes.',
    subsystems: ['host-tools', 'fpga-capture', 'zynq-rtos', 'esp32-gateway'],
  },
  {
    id: 'P1',
    name: 'Specifications',
    outcome:
      'Complete register, protocol, RTOS, decoder and wiring specifications, generated constants, the shared C protocol library, UART/SPI/I2C decoders and independent Python models.',
    status: 'host-tested',
    statusNote:
      'Complete and exercised by native and host tests. The specifications are normative for later phases; agreement tests are not target timing evidence.',
    evidence:
      'gen idempotent with --check clean; native protocol/decoder/parity tests pass; docs/regmap.yaml base corrected to 0x40000000 to match the P3 block design (2026-09-06).',
    remaining:
      'None open. Later phases consume these specifications; decoder target behavior is proven separately in P7.',
    subsystems: ['protocol-link', 'fpga-capture', 'host-tools', 'zynq-rtos'],
  },
  {
    id: 'P2',
    name: 'FPGA RTL',
    outcome:
      'Eleven la_* RTL blocks plus the generated register package. Each block has a self-checking Questa test (input sync, sampler, trigger, pre-trigger ring, txn encoder, raw packer, capture control, AXIS FIFO, patgen, registers and top integration), and the package is exercised through tb_la_regs and tb_la_top.',
    status: 'simulation-tested',
    statusNote:
      'Complete locally with 12/12 compile-only and procedural testcases and zero warnings. Simulation is not synthesis or timing closure; P3 supplies that evidence.',
    evidence:
      'lint and sim 12/12 on 2026-09-11 after the patgen single-shot mode; the 2026-09-10 burst-close rework (pkt_words_o, packer tlast, ring stream_en) and the BRAM-friendly synchronous-read rework are verified in Questa.',
    remaining:
      'The regression runner has no per-test Python vector hook, so independent reconstruction rides on the P4.10 host path; the one pipeline-boundary word drain caveat is closed there.',
    subsystems: ['fpga-capture', 'physical-io'],
  },
  {
    id: 'P3',
    name: 'Vivado integration',
    outcome:
      'Block design on the pinned Zybo Z7-10 board part with PS7 preset, SmartConnect fabrics, SG DMA, constraints and a timing-clean bitstream/XSA.',
    status: 'bench-verified',
    statusNote:
      'Complete on bench: timing and DRC are clean and the programmed design ran (LD0 blink). JP5 stays on JTAG for development; SD boot belongs to P10.',
    evidence:
      'bit PASS with zero failing endpoints (WNS +0.255 ns, WHS +0.024 ns), DRC 0 errors and LD0 blinking at 1 Hz (2026-09-10); the P9.1 JA-1 rebuild closed timing again at WNS +0.247 ns (2026-09-13).',
    remaining:
      'No open P3 item. DDR/EMIO behavior is exercised by P4 and later bench gates, not by this build.',
    subsystems: ['fpga-capture', 'physical-io', 'dma-memory'],
  },
  {
    id: 'P4',
    name: 'Zynq FreeRTOS core (burst mode)',
    outcome:
      'FreeRTOS application with acquisition, command, TX and status tasks; ordinary SG DMA burst capture into DDR, the UART0 EMIO link and register/IRQ/DMA drivers.',
    status: 'bench-verified',
    statusNote:
      'Complete on bench: the burst path is proven end to end through the gateway. The optional USB-UART HIL adapter route was not needed.',
    evidence:
      'P4.6 burst suite 7/7 with exact counts and zero CRC/format errors (2026-09-10); a 1M-sample 16-BD capture strictly consecutive; on-target register selftest OK (ID 0x4C410002); 10-minute soak without assert.',
    remaining:
      'P4.10 HIL through a USB-UART adapter is optional now that the ESP32 gateway proves the on-wire path; the DMA coherency readback is covered by the counter captures.',
    subsystems: ['zynq-rtos', 'dma-memory', 'protocol-link'],
  },
  {
    id: 'P5',
    name: 'ESP32 bridge',
    outcome:
      'ESP-IDF gateway with UART2 RTS/CTS link tasks, static rings, single-client TCP relay on port 5555, mDNS, UiTask and task watchdog.',
    status: 'bench-verified',
    statusNote:
      'Complete on bench: association, relay and loopback are recorded. Relay throughput and drop behavior moved to P8/P9 measurement.',
    evidence:
      'Associated at RSSI about -60; five burst captures over TCP with zero relay drops and fifo_ovf=0; target Unity suite 5/5; P5.5 loopback 10 MB at 921600 and 2 Mbps with zero CRC errors (2026-09-10).',
    remaining:
      'Relay capacity is profiled under P8/P9. The stale-client states were resolved by the P8.2 heap fix and the P9.4 link-deadlock fix.',
    subsystems: ['esp32-gateway', 'protocol-link'],
  },
  {
    id: 'P6',
    name: 'Host tool and burst end-to-end',
    outcome:
      'la-cli with TCP/serial transports, ping/status/capture, streaming reconstruction and .sr/VCD/PNG export, plus bit-exact end-to-end burst matrices.',
    status: 'bench-verified',
    statusNote:
      'Complete on bench: both matrices pass and the exported capture opens in PulseView. Host tests cover the tool separately from the bench path.',
    evidence:
      'P6.2 internal matrix 18/18 and P6.3 external JE-to-JC 18/18 at 1M/10M/100M with three consecutive captures each (2026-09-11); PulseView 0.4.2 rendered the exported .sr; annotated tag v0.1-burst.',
    remaining:
      'None. The full eight-channel external matrix is recorded complete with the default channel mask.',
    subsystems: ['host-tools', 'fpga-capture', 'physical-io', 'protocol-link'],
  },
  {
    id: 'P7',
    name: 'Live decode pipeline',
    outcome:
      'Segment pool with ownership and reference counts, live ordinary SG DMA, UART/SPI/I2C decoder tasks and batched event delivery to the host, including protocol-aware stop.',
    status: 'bench-verified',
    statusNote:
      'Complete on bench at a 2 Mbps link. The recorded gates pass; occasional Wi-Fi jitter is tracked under P9.4, not as a P7 defect.',
    evidence:
      '60 s 115200 zero-drop UART gate repeatable; SPI/I2C 10 kHz gates with headroom; isolated-byte p99 428 us against the 25 ms budget; P7.7 decoded-pattern stop bench-verified (2026-09-13); full live suite 55 passed; annotated tag v0.2-live.',
    remaining:
      'None in P7. One isolated-byte latency outlier under weak RSSI and best-effort trace LOG loss are P9.4 items.',
    subsystems: ['zynq-rtos', 'dma-memory', 'protocol-link', 'host-tools'],
  },
  {
    id: 'P8',
    name: 'RTOS hardening and measurement',
    outcome:
      'Fault injection, a 500-capture soak, the cached live pool, the task/queue trace pipeline and recorded latency/throughput measurements.',
    status: 'bench-verified',
    statusNote:
      'Complete with P8.6 as a documented skip. Hardening results are recorded in docs/status.md and docs/rtos-design.md.',
    evidence:
      'TCP kill 4/4, garbage-flood resync, WDT reset observed, 3 s read-stall link-overrun report, 500 back-to-back captures exact; cached pool 1.29 M words/s vs 0.98 M non-cached (~28 % more, ~30 % fewer drops); annotated live-UART trace (2026-09-11..12).',
    remaining:
      'P8.6 AMP offload stays a documented skip with revisit conditions in docs/rtos-design.md; trace dumps remain best-effort with up to 2 % LOG loss under relay pressure.',
    subsystems: ['zynq-rtos', 'dma-memory', 'esp32-gateway'],
  },
  {
    id: 'P9',
    name: 'Performance',
    outcome:
      'SPI bulk link, compact live events, the 200 MS/s stretch decision and Wi-Fi tuning.',
    status: 'blocked',
    statusNote:
      'Phase in progress: P9.2 is complete, P9.1 framed bursts are bench-clean and the 64 MB / 1.5 MB/s drain gate passes on the default direct-USB transport (1.62 MB/s, U06 2026-09-19); the gateway drain and P9.3/P9.4 remain open as gateway-profile/optional work.',
    evidence:
      'P9.1 framed bursts bit-exact 4 KB to 256 KB at 10 MHz, 20/20 at 18 MHz, a 4 MiB byte-verified capture and honest 40 MHz fault recovery; P9.2 compact events measured 51.6-53.3 % wire-byte saving (2026-09-12/13); direct USB drain 1.62 MB/s over three 64 MiB trials with byte-exact data.',
    remaining:
      'The gateway Wi-Fi drain is 0.28 MB/s (a supported optional profile); 19/20 MHz lose chunks (18 MHz ceiling); the 200 ohm SCK/MISO edge characterization is open; P9.3 200 MS/s stretch decision and P9.4 Wi-Fi tuning remain gateway-profile items.',
    subsystems: ['fpga-capture', 'protocol-link', 'esp32-gateway', 'host-tools'],
  },
  {
    id: 'P10',
    name: 'UX and release',
    outcome:
      'Host live web view (with the ESP32 page retained for the gateway profile), the sigrok bridge decision, consolidated documentation and profile-aware release packaging (BOOT.BIN, optional ESP32 bin set, Python wheel).',
    status: 'blocked',
    statusNote:
      'Phase in progress: P10.1 is bench-verified on the gateway and the V00-V04 pivot moved the default live view to the PC host bridge (SSE); P10.2 is deferred by the plan condition and the release gate still waits on the [HUMAN] SD boot and tag authorization.',
    evidence:
      'P10.1 page and /ws WebSocket carried the decoded LA-TEST bytes plus stats over the phone hotspot (2026-09-14) with test-esp32-mocks 4/4; the host bridge passed the V04 USB gates 8/8 on 2026-09-20 (60 s zero-drop 606,411 events, SPI/I2C, stop-on, compact, overload recovery 0.05 s).',
    remaining:
      'P10.2 sigrok/SUMP bridge deferred (no signed virtual-COM pair driver; docs/sigrok-bridge.md); P10.4 profile-aware boot/package pass and the [HUMAN] SD boot + v1.0 tag authorization remain.',
    subsystems: ['esp32-gateway', 'host-tools', 'zynq-rtos'],
  },
];
