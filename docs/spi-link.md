# P9.1 SPI Link

This document is the interface contract for the P9.1 bulk-data link between the
Zybo PL and the ESP32 gateway. The UART link (JD-1..4, docs/wiring.md) remains
the command/ACK path; the SPI link carries the bulk Zynq to ESP32 direction
(burst transfers and live events). It is a P9.1 design document; the measured
bench gate (>=1.5 MB/s over a 64 MB drain) is recorded in `docs/status.md` when
it runs.

## Physical interface

| Signal | Zybo pin | Package pin | Direction | ESP32 (provisional) |
|---|---|---|---|---|
| SPI SCLK | JD-7 | U14 | input (slave clock) | GPIO18 |
| SPI SS_N | JD-8 | U15 | input (active low) | GPIO4 |
| SPI MOSI | JD-9 | V17 | input (slave data in) | GPIO23 |
| SPI MISO | JD-10 | V18 | output (slave data out) | GPIO19 |
| SPI data-ready | JA-1 | N15 | output | GPIO27 |
| GND | JD-5/11, JA-5/11 | - | common ground | GND |

All signals are 3.3 V LVCMOS33. The JD pins are shared with the UART link
header only physically; JD-1..4 stay assigned to UART0 EMIO. The ESP32 pin
column is provisional until the module is re-checked against the DOIT DevKit V1
schematic (same rule as the UART pin map); the Zybo side is fixed by the XDC.

The Zybo Pmod outputs pass through the on-board 200 ohm series resistors. P9.1
starts at 10 MHz and the bench characterizes the SCK edges (overshoot, ringing,
setup/hold at the slave) before 20 MHz is used; the plan target is <=20 MHz.

## PL integration

`xilinx.com:ip:axi_quad_spi:3.2` configured as a **slave**:

- `CONFIG.Master_mode 0`, `CONFIG.C_SPI_MODE 0` (standard, single-bit),
  `CONFIG.C_NUM_TRANSFER_BITS 8`, `CONFIG.C_FIFO_DEPTH 256`,
  `CONFIG.C_USE_STARTUP 0`, `CONFIG.C_DUAL_QUAD_MODE 0`,
  `CONFIG.C_SPI_MEMORY 0`.
- `sck_i` <- `spi_sck`, `spisel` <- `spi_ss_n` (the core drives MISO only while
  `spisel` is low), `io0_i` <- `spi_mosi`, `io1_o` -> `spi_miso`; `ss_i`,
  `io0_o/t`, `io1_i/t`, `sck_o/t` are tied off or left unconnected.
- `ext_spi_clk` and `s_axi_aclk` run from FCLK_CLK0 (100 MHz), so the external
  clock is 5x the 20 MHz SCK ceiling (the IP requires the external clock to
  over-sample SCK).
- AXI4-Lite register space at **0x4080_0000** (64 KiB) through the existing
  `sc_lite` SmartConnect; `ip2intc_irpt` joins the PL interrupt fan-in
  (`xlconcat_irq`, port 2 -> GIC 61 family, IRQ_F2P[2]).
- Data-ready is a PS EMIO GPIO (width 1, output) driven to `spi_ready` on JA-1.
  Firmware asserts it when at least one complete frame is present in the SPI TX
  FIFO (or queued in the SPI ring) and deasserts it when the ring drains. It is
  a latency optimization only; the framing below is self-describing so the
  link is correct without it.

## Data path and framing

The slave streams **encoded `la_proto` frames** (the same wire format as the
UART link) so the ESP32 reuses the existing parser and the host sees identical
frames regardless of transport.

- The master drives a read transaction: SS_N low, then a stream of SCK bytes
  while MISO carries the slave TX FIFO content. The transaction ends when SS_N
  rises.
- The PS `SpiLinkTask` feeds the TX FIFO from a bulk ring (`spi_to_esp`) fed by
  TxTask for frames that the session routes to SPI instead of UART. It drains
  the RX FIFO into `spi_from_esp` (reserved for a future SPI command channel;
  P9.1 keeps commands on UART).
- **Underrun protection:** the FIFO must never underrun inside a frame. The
  task only queues whole frames and stops feeding at a frame boundary when the
  ring runs dry; the master reads whole frames using the length field. If the
  slave does underrun anyway (SCK keeps clocking), the IP shifts out `0x00` and
  sets the `DTR_UNDERRUN` status bit. The ESP32 detects the corrupted frame
  through the `la_proto` length/CRC check, discards it and resyncs on the next
  frame header (`0xA5`). Underruns are counted in the Zynq status counters and
  in the ESP32 link counters; they are never silent.
- **Transaction sizing:** the ESP32 reads in bounded DMA bursts (default 4 KiB,
  tuned on the bench). A frame may span bursts; the parser is incremental. A
  burst boundary never splits a header from its payload in a way the parser
  cannot resync from, because the parser only commits a frame after the CRC.
- **Flow control:** the SPI master polls `spi_ready` before starting a
  transaction and stops when it is low; the UART remains the lossless control
  path. The SPI path is best-effort bulk with CRC, matching the live-event
  semantics.

## Bandwidth budget

At 20 MHz with 8-bit transfers the raw rate is 2.5 MB/s. The budget for the
1.5 MB/s target:

| Item | Value |
|---|---|
| SCK | 20 MHz |
| Raw 8-bit rate | 2.5 MB/s |
| Frame overhead (`la_proto` header + CRC) | ~4 bytes per 1024-byte payload |
| Transaction gaps (SS/command/poll) | ~5 % |
| Expected net | ~2.2 MB/s |

At 10 MHz the expected net is ~1.1 MB/s, so the bench first proves the link at
10 MHz, then characterizes the 200 ohm edges before moving to 20 MHz. The
64 MB drain measurement uses a deterministic source (patgen counter or trace
dump) and verifies every byte through the host parser.

## Firmware split

- **Zynq (`la_spi_link`):** AXI-Lite register access (SPICR/SPISR/SPIDTR/
  SPIDRR/SPISSR), interrupt-driven or polled FIFO service in `SpiLinkTask`,
  bulk ring ownership, underrun/overrun counters, data-ready EMIO control.
  Register-level access is host-tested against a mocked MMIO map (same pattern
  as `la_regs`).
- **ESP32 (`la_spi_link`):** SPI master with DMA (`spi_bus_initialize` +
  `spi_device_polling_transmit`/`spi_master_transmit_receive` in 4 KiB bursts),
  data-ready polling, incremental `la_proto` parser, frames pushed into the
  existing `to_net` relay ring with the same EVT_OVERRUN reporting.
- **Routing:** `LINK_CFG` (0x1A, additive v1, `docs/protocol.md`) selects the
  burst `DATA_*` transport at run time: 0 keeps them on the UART, 1 routes
  them over the SPI slave. Commands, ACKs and every other frame stay on the
  UART, and the ESP32 merges the SPI and UART streams into the single host
  relay. TxTask retries a full 8 KiB SPI ring briefly before reporting a drop.

## Test plan

| Gate | Command | Pass condition |
|---|---|---|
| IP catalog + BD | `python tools/run.py bit` | BD validates, timing closed, XSA written |
| Zynq register core | `python tools/run.py test-native` | mocked-MMIO driver test passes |
| ESP32 portable core | `python tools/run.py test-esp32-mocks` | SPI framing/parse core tests pass |
| Bench 10 MHz | bench script (P9.1) | framed transfer with zero CRC errors |
| Bench 64 MB drain | bench script (P9.1) | >=1.5 MB/s, every byte verified |
| Underrun injection | bench script (P9.1) | corrupted frame detected, resync, counted |

Bench prerequisites: JD-7..10 and JA-1 jumpered to the ESP32 pins above, the
UART ribbon unchanged, and a scope or LA on SCK for the edge characterization.
