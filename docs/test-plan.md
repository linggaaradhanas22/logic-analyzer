# Verification Plan

The canonical entry point is `python tools/run.py <target>`. Reports and logs
are written below ignored `build/reports`; their absence on another checkout
means not-run, not pass.

## P1 Acceptance Matrix

| Requirement | Check | Evidence |
|---|---|---|
| Register source is normative | `python tools/run.py gen`, repeat, then `python tools/gen_regmap.py --check` | No second-run diff; generated C/SV/Python/Markdown views current |
| Protocol framing and CRC | `python tools/run.py test-native` | CTest protocol boundaries, fragmentation, CRC rejection, resynchronization and typed payload validation |
| UART decoder | `python tools/run.py test-native` | Native 8N1, parity/error and timestamp checks; ctypes model parity |
| SPI decoder | `python tools/run.py test-native` | CPOL/CPHA, MSB/LSB, CS and word-size vectors; ctypes model parity |
| I2C decoder | `python tools/run.py test-native` | START, ADDRESS, ACK/NACK, BYTE, STOP and repeated-start vectors; ctypes model parity |
| Segment gaps | `python tools/run.py test-native` | DISCONTINUITY resets decoder state and rejects stale timestamps |
| Protocol/decoder Python ABI | `python tools/run.py test-native` | `la_native_ctypes` and `la_native_ctypes_models` CTest entries |
| Host package remains healthy | `python tools/run.py test-host` | Host pytest smoke |
| Existing native mocks remain healthy | `python tools/run.py test-esp32-mocks` | Mock CTest target |
| Questa sources remain syntactically valid | `python tools/run.py lint` then `python tools/run.py sim` sequentially | Compile and simulation reports; no P1 RTL is claimed complete |

## P1 Native Test Details

The C harness checks maximum and empty frames, payload bytes containing `0xA5`,
fragmented feeds, malformed lengths, CRC errors, parser counters and typed
CONFIG/DECODER_CFG round trips. Decoder tests cover UART data widths, parity,
inversion and framing errors; SPI CPOL/CPHA, optional lanes and bit order; and
I2C address direction, repeated START, ACK/NACK and STOP. The independent
Python model test loads the built DLL with ctypes, uses an explicit event
layout and compares every event field.

Native tests are functional agreement checks. They do not establish target
FreeRTOS task timing, A9 decoder throughput, DMA descriptor coherency, UART
flow-control behavior, FPGA timing, electrical levels or physical wiring.

## Later Gates

P2 adds one self-checking procedural Questa test per RTL block. P3 adds Vivado
synthesis, DRC, timing and XSA evidence. P4/P5/P6 add target firmware,
serial/ESP32 loopback and burst HIL gates. P7/P8 add live ownership, fault
injection and measured latency/throughput. Human-operated programming and
bench checks must remain explicitly marked `[HUMAN]` until observed.

## P2 Acceptance Matrix

| Requirement | Check | Evidence |
|---|---|---|
| One self-checking test per RTL block | `python tools/run.py sim` (run `lint` first, sequentially) | 12/12 testcases PASS with per-test `PASS: tb_<name>` result markers; `build/reports/sim.json` and `build/reports/lint.json` |
| All RTL and testbench sources compile clean | `python tools/run.py lint` | 12/12 compile-only cases PASS; explicit warning review: zero remaining Questa vlog/vsim/vopt warnings (a redundant-digit literal in `la_patgen` was fixed) |
| P2.1 synchronizer | `tb_la_input_sync` | 2-cycle (3 with filter) latency, reset, per-channel independence, single-cycle glitch rejection and two-cycle pulse pass for the 2-of-3 majority path |
| P2.2 sampler | `tb_la_sampler` | DIV in {0, 1, 9, 99} period checks, chan_en masking, aligned sample_valid, reset |
| P2.3 trigger | `tb_la_trigger` | level + "empty edge mask = true" rise/fall semantics, no spurious first-sample edge, divider-gap tolerance, mirror parity per cycle |
| P2.4 pre-trigger ring | `tb_la_pretrig_ring` | stream pass-through, exact pre/post seam (no dup/loss/invention), history wrap, K clamp at DEPTH-1 with a permanently full ring, stall overflow with exact drop count, double trigger during replay, abort and clean rerun; value-FIFO scoreboard |
| P2.5 txn encoder | `tb_la_txn_encoder` | predicted-word event model parity, run totals == accepted samples, max_run splits, mid-run/no-op flushes, flush behind a stalled word, max_run=1, reset |
| P2.6 raw packer | `tb_la_raw_packer` | byte-stream reconstruction equals accepted samples (tkeep-aware), packet length/tlast structure, flush tails, word-aligned deferred flush, stalls, activity assertion |
| P2.7 capture control | `tb_la_capture_ctrl` | full/window-only/empty burst emission targets and word counts, timeout, abort, overflow drain, stream timer and 65536-word flushes |
| P2.8 AXIS FIFO | `tb_la_axis_fifo` | order/transport wrap, fill and stall, non-compliant drop with sticky overflow and clear, count accuracy, reset |
| P2.9 patgen | `tb_la_patgen` | counter/walking-1/PRBS8 models with DIV scaling; recorded-history decoders for UART 115200 "LA-TEST\n", SPI 0x4C41 frames and the I2C write sequence |
| P2.10 registers | `tb_la_regs` | AXI-Lite BFM: independent AW/W order, BREADY/RREADY stalls, byte strobes, self-clearing ARM/ABORT/SOFT_RESET, masked fields, PRE/TXN clamps, IRQ W1C + events, out-of-range |
| P2.11 top integration | `tb_la_top` | AXI BFM + AXIS sink + word-to-sample reconstruction: txn UART selftest decodes "LA-TEST\n"; raw counter burst verified stream, counts, tlast, STATUS/IRQ behavior |
| P2.12 static rules | reviewed in the status ledger | module/port naming consistent (`la_*`, AXI-style signals), register offsets only from the generated package, no hand-edited generated files (`gen --check` passes) |

### P2 Caveats (recorded, not silent)

- The plan's Python-model vector exchange for the txn encoder and patgen is
  covered by clocked in-testbench models and recorded-history decoders; the
  regress runner has no per-test vector-script hook yet, so an independent
  Python reconstruction of the `la_top` capture stream (plan P2.11 wording)
  is a follow-up that rides on the P4.10 host/HIL path instead.
- The `la_top` raw-burst drain can carry up to one pipeline-boundary word
  beyond the K+POST target; the integration test asserts bounded counts plus
  exact stream properties. The drain-boundary accounting remains open and is
  to be closed with a wave-level check at the P4.10 host/HIL bring-up, where
  the DMA byte count can be compared against the reconstruction.
- Questa Altera FSE quirks worked around: `default_nettype none` with ANSI
  ports fails to compile; task/function default arguments are unsupported;
  local declarations inside `always_comb` can break parsing. Follow the
  patterns used in the passing sources.
- Simulation is not timing closure: P3 synthesis, DRC and timing still gate
  implementation.

## P3 Acceptance Matrix

| Requirement | Check | Evidence |
|---|---|---|
| Block design builds on the pinned board part | `python tools/run.py bit` | `build/reports/bit.json` PASS; `validate_bd_design` clean; PS7 board preset applied from `fpga/board_files/zybo-z7-10/A.0/preset.xml` (DDR, console UART1 on MIO) + UART0 EMIO full RTS/CTS on JD, FCLK_CLK0 100 MHz, IRQ_F2P, 64-bit S_AXI_HP0; `la_top` RTL module reference; `axi_dma` S2MM-only with SG and 26-bit buffer length |
| Address map and fabric wiring | validated BD + address prints in the bit log | la_top regs 0x4000_0000/64K, DMA regs 0x4040_0000/64K (GP0); DMA S2MM and SG both map the full DDR low 1G range through S_AXI_HP0 |
| Bitstream and XSA artifacts | `python tools/run.py bit` | `build/release/logic_analyzer.bit` and `build/release/logic_analyzer.xsa` (`write_hw_platform -fixed -include_bit`) |
| P3.2 constraints | XDC review | JC CH0-7, JE PATGEN0-7 and JD UART0 EMIO pads pinned per `part0_pins.xml` (LVCMOS33); async `ch_in` exception to the `la_input_sync` stage documented with `set_false_path` + `async_reg` review |
| Timing closure | `build/release/timing_summary.rpt` | Zero failing endpoints; "All user specified timing constraints are met"; WNS +0.278 ns at 100 MHz |
| Utilization recorded | `build/release/utilization.rpt` | 33.5 % LUT, 24.2 % FF, 60.0 % BRAM tiles; table in `docs/architecture.md` |
| Implementation DRC | `build/release/drc.rpt` | No errors; stock REQP-181 advisory inside the Xilinx AXI DMA IP only |
| P2 regression after the BRAM fabricability rework | `lint` then `sim` sequentially | 12/12 testcases PASS with the synchronous-read ring/FIFO and the enable-latched patgen periods |

### P3 Caveats (recorded, not silent)

- This Vivado 2026.1 install serves no `axi_interconnect` IP definition
  (catalog probe; the on-disk `axi_interconnect_v2_1` files are excluded
  from the served catalog). The plan's interconnect requirement is met with
  two SmartConnect fabrics; endpoint data widths are explicit (GP0 32-bit
  AXI4-Lite fan-out, S2MM 64-bit, SG 32-bit, HP0 64-bit). The remaining
  SmartConnect auto-configuration warnings (BD 41-3281) were reviewed and
  are expected in this topology.
- The ps7 EMIO UART0 pins are grouped by the PS7 core into an inferred
  `UART_0` interface; `build.tcl` connects the scalar pads and the BD
  override warnings (BD 41-1306) are expected: `build.tcl` is the source of
  truth and the BD is never regenerated from `writ_bd_tcl`.
- The board preset carries negative DDR DQS-to-CLK skew values, which Vivado
  2026.1 flags as CRITICAL WARNING PSU-1..4. The values come from the pinned
  Digilent preset unchanged; DDR behavior still requires a PS boot check on
  hardware (P4/P10), so the warnings are recorded, not silenced.
- `la_top.srst` is the active-high inversion of the synchronized
  `proc_sys_reset` peripheral reset (release synchronized, assert coincides
  with the PS reset).
- `IRQ_F2P` is a single fabric input fed by both `la_top.irq_o` and
  `axi_dma.s2mm_introut` (xlconcat); P4 software must distinguish the
  sources by status reads, and the DMA interrupt alone cannot be masked in
  the GIC.
- Simulation is not silicon: DDR training, EMIO UART behavior, DMA coherency
  and pad electricals still gate on hardware runs ([HUMAN] P3.3/P4/P10).

Native tests are functional agreement checks. They do not establish target
FreeRTOS task timing, A9 decoder throughput, DMA descriptor coherency, UART
flow-control behavior, FPGA timing, electrical levels or physical wiring.
