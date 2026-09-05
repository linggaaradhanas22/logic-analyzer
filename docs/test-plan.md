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
