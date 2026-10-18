# P9.1 SPI Link

This document is the interface contract for the P9.1 bulk-data link between the
Zybo PL and the ESP32 gateway. The UART link (JD-1..4, docs/wiring.md) remains
the command/ACK path; the SPI link carries the bulk Zynq to ESP32 direction
(burst transfers and live events). It is a P9.1 design document. Current status:
the framed-burst gates and a 4 MiB byte-verified capture passed; the 64 MB drain
target is met on the default direct-USB path at 1.62 MB/s, while the optional
ESP32 Wi-Fi relay remains at 0.28 MB/s and is deferred. See `docs/status.md`.

## Physical interface

| Signal | Zybo pin | Package pin | Direction | ESP32-S3 (J1) |
|---|---|---|---|---|
| SPI SCLK | JD-7 | U14 | input (slave clock) | GPIO12 (J1-18, FSPICLK) |
| SPI SS_N | JD-8 | U15 | input (active low) | GPIO10 (J1-16, FSPICS0) |
| SPI MOSI | JD-9 | V17 | input (slave data in) | GPIO11 (J1-17, FSPID) |
| SPI MISO | JD-10 | V18 | output (slave data out) | GPIO13 (J1-19, FSPIQ) |
| SPI data-ready | JA-1 | N15 | output | GPIO9 (J1-15) |
| GND | JD-5/11, JA-5/11 | - | common ground | GND |

All signals are 3.3 V LVCMOS33. The JD pins are shared with the UART link
header only physically; JD-1..4 stay assigned to UART0 EMIO. The S3 column is
verified against the official DevKitC-1 v1.1 header guide and matches
`fw-esp32/components/la_board/include/la_board.h`; the Zybo side is fixed by
the XDC. GPIO9-14 are the S3's SPI2/FSPI IOMUX pins (the S3 has no SPI3).

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
  them over the SPI slave. Its `spi_clock_mhz` byte selects the ESP32 master
  clock (0 = firmware default, 1-50 MHz) so the bench can sweep 10/20 MHz
  without a reflash; the Zynq slave ignores it. Commands, ACKs and every other
  frame stay on the UART, and the ESP32 merges the SPI and UART streams into
  the single host relay. TxTask retries a full 8 KiB SPI ring briefly before
  reporting a drop.
- **Host leg (P9.1 S3 port, 2026-09-17):** the ESP32-S3-DevKitC-1 gateway also
  exposes the relay over its native USB port as a CDC-ACM device (`la_usb`).
  While the host has that port open, it owns the analyzer stream: NetServerTask
  stops draining the relay ring and refuses TCP analyzer clients, so frames are
  never split between legs. The host uses the existing `serial://COMx`
  transport; the Wi-Fi TCP leg and the P10.1 web view keep working when the USB
  port is closed.

## Measured results (2026-09-13 bench)

The service path was rebuilt during the 10/20 MHz characterization:

- **ESP32 master**: continuous fixed-size 1 KiB DMA chunks fed to the
  incremental parser (the old 5-byte header/body reader lost framing after a
  single offset and collapsed to ~5 bytes per transaction), with an idle-gap
  parser resync at session boundaries.
- **Zynq**: the burst job start flushes the ring/FIFOs; data-ready rises only
  with a 4 KiB head start (small captures release after a 5 ms quiet gap); the
  ISR refills the TX FIFO directly (the task-only refill could not bridge the
  half-empty window above 10 MHz); the ring push/feed pair is mutually
  exclusive (the SPI task could preempt `la_spi_core_push` mid-compaction and
  copy from a stale head).
- **ESP32 relay**: control frames retry a fragmented NOSPLIT ring instead of
  silently dropping after a positive capacity answer.
- **Zynq producer**: the SPI wait for ring space is bounded by the stalled-host
  timeout (2 s), not the old ~20 ms cap that aborted every long capture.

Measured with the P9.1 bench tests (`tests/e2e/test_p91_spi.py`):

| Check | Result |
|---|---|
| 4/16/64 KB framed captures at 10 MHz | bit-exact, zero parser errors |
| 64/128/256 KB framed captures at 10 MHz | bit-exact, zero parser errors |
| 16 KB bursts at 5/10/18 MHz | bit-exact, zero parser errors (20/20 at 18 MHz) |
| 19/20 MHz | chunks lost to slave FIFO underruns; not usable |
| Fault injection (40 MHz) then 10 MHz recovery | failure reported, next capture clean |
| 4 MiB capture at 10 MHz | completes, every byte verified |
| Sustained host-verified drain | **0.28 MB/s** |

The sustained rate is limited by the **ESP32 -> host transport**, not the SPI
wire: the same capture path reaches the Zynq at 1.25 MB/s at 10 MHz and
2.25 MB/s at 18 MHz, while the gateway-to-host TCP path topped out near
0.28 MB/s in this setup. The 2026-09-17 S3 port replaces that leg with USB
CDC-ACM; the ESP32-S3 USB-OTG is **full-speed only** (12 Mbps, bulk ceiling
about 1.2 MB/s), so the drain is measured and recorded against that physical
ceiling. 20 MHz additionally exceeds what the 256-byte slave FIFO can sustain
even with the ISR bridge (19 MHz already loses chunks), so 18 MHz is the
measured burst ceiling on the current wiring.

## Test plan

| Gate | Command | Pass condition |
|---|---|---|
| IP catalog + BD | `python tools/run.py bit` | BD validates, timing closed, XSA written |
| Zynq register core | `python tools/run.py test-native` | mocked-MMIO driver test passes |
| ESP32 portable core | `python tools/run.py test-esp32-mocks` | SPI framing/parse core tests pass |
| Bench 10 MHz | `test-e2e -k test_p91_spi_capture_sizes` | framed transfer with zero CRC errors |
| Bench 64 MB drain | `test-e2e -k test_p91_spi_drain_rate` | >=1.5 MB/s, every byte verified (blocked: relay-limited, see above) |
| Underrun injection | `test-e2e -k test_p91_spi_fault_recovery` | corrupted frame detected, resync, counted |

Bench prerequisites: JD-7..10 and JA-1 jumpered to the ESP32 pins above, the
UART ribbon unchanged, and a scope or LA on SCK for the edge characterization.
