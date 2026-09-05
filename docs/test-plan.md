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
  exact stream properties. The drain-boundary accounting should be closed
  with a focused wave-level check before P3 bitstream bring-up.
- Questa Altera FSE quirks worked around: `default_nettype none` with ANSI
  ports fails to compile; task/function default arguments are unsupported;
  local declarations inside `always_comb` can break parsing. Follow the
  patterns used in the passing sources.
- Simulation is not timing closure: P3 synthesis, DRC and timing still gate
  implementation.

Native tests are functional agreement checks. They do not establish target
FreeRTOS task timing, A9 decoder throughput, DMA descriptor coherency, UART
flow-control behavior, FPGA timing, electrical levels or physical wiring.
