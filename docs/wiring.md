# Zybo and ESP32 Wiring

This is the P1 signal contract. It is not a substitute for the Zybo schematic
or the exact ESP32 module pin table. All signals are 3.3 V logic. Physical
connection, continuity and signal-integrity checks are `[HUMAN]` gates.

## Acquisition and Pattern Headers

| Zybo header pin | Signal | Direction | Notes |
|---|---|---|---|
| JC-1 | CH0 | input | LVCMOS33, high-speed input |
| JC-2 | CH1 | input | LVCMOS33, high-speed input |
| JC-3 | CH2 | input | LVCMOS33, high-speed input |
| JC-4 | CH3 | input | LVCMOS33, high-speed input |
| JC-7 | CH4 | input | LVCMOS33, high-speed input |
| JC-8 | CH5 | input | LVCMOS33, high-speed input |
| JC-9 | CH6 | input | LVCMOS33, high-speed input |
| JC-10 | CH7 | input | LVCMOS33, high-speed input |
| JE-1 | PATGEN0 | output | maps to self-test channel 0 |
| JE-2 | PATGEN1 | output | maps to self-test channel 1 |
| JE-3 | PATGEN2 | output | maps to self-test channel 2 |
| JE-4 | PATGEN3 | output | maps to self-test channel 3 |
| JE-7 | PATGEN4 | output | maps to self-test channel 4 |
| JE-8 | PATGEN5 | output | maps to self-test channel 5 |
| JE-9 | PATGEN6 | output | maps to self-test channel 6 |
| JE-10 | PATGEN7 | output | maps to self-test channel 7 |

JE-to-JC jumpers are the internal/external self-test loopback. JC and JE use
the same channel order. JC pairs are impedance-matched differential pairs used
single-ended here; keep wires short and expect crosstalk at fast edges. Never
connect JC or JE to a 5 V source without an appropriate level shifter.

## Zynq to ESP32 UART

The MVP link is PS UART0 through EMIO on JD at 2 Mbps with RTS/CTS enabled
(bring-up started at 921600; the P7.6 live gate verified 2 Mbps on both sides
on 2026-09-11). Direction is named from the ESP32 perspective.

P9.1 gateway (2026-09-17): **ESP32-S3-DevKitC-1** (ESP32-S3-WROOM-1-N16R8). The
S3 map below matches the bench wiring; header positions are from the official
DevKitC-1 v1.1 user guide (J1 = left header, pin 1 at the 3V3 end, printed
labels are GPIO numbers):

| JD signal | Zynq UART0 EMIO | ESP32-S3 connection | Header | Meaning |
|---|---|---|---|---|
| JD-1 | TX | GPIO16 RX | J1-9 | Zynq data to ESP32 |
| JD-2 | RX | GPIO17 TX | J1-10 | ESP32 data to Zynq |
| JD-3 | RTS | GPIO14 CTS | J1-20 | Zynq may send when ESP32 permits |
| JD-4 | CTS | GPIO15 RTS | J1-8 | Zynq receive permission |
| JD-5/11 ground | signal ground | GND | J1-22 or J3-1/21/22 | common reference |

Reserved on the S3: GPIO19/20 (native USB D-/D+, the P9.1 host transport),
GPIO26-37 (module flash and octal PSRAM), GPIO0/3/45/46 (strapping) and
GPIO43/44 (UART0 console through the CP2102 bridge). The previous classic
ESP32 (DOIT DevKit V1) map (GPIO16/17/21/22) stays in `la_board.h` for that
board.

The ESP32 is powered from its own USB; do not connect the JD power pins. The
DevKitC-1 has two micro-USB ports: **"UART"** (CP2102 bridge: console and
flashing) and **"USB"** (native USB-OTG: the P9.1 CDC host transport).
`fw-esp32/components/la_board/include/la_board.h` is the single source for
this mapping and the ESP-IDF build compiles it.

The gateway also serves the P10.1 live web view on TCP port 80 (HTTP page plus
a `/ws` WebSocket) while the analyzer link runs; see `docs/windows-dev.md` for
the browser procedure. The page needs no extra wiring beyond the UART link.

### P9.1 SPI link (JD-7..10, JA-1)

The bulk Zynq-to-ESP32 data path moves to a PL `axi_quad_spi` slave while the
UART keeps commands/ACKs. `docs/spi-link.md` is the interface contract; the
physical map for the ESP32-S3 gateway is:

| Zybo pin | Signal | ESP32-S3 |
|---|---|---|
| JD-7 | SPI SCLK (slave input) | GPIO12 (J1-18, FSPICLK) |
| JD-8 | SPI SS_N (slave input, active low) | GPIO10 (J1-16, FSPICS0) |
| JD-9 | SPI MOSI (slave input) | GPIO11 (J1-17, FSPID) |
| JD-10 | SPI MISO (slave output) | GPIO13 (J1-19, FSPIQ) |
| JA-1 | SPI data-ready (slave output) | GPIO9 (J1-15, input with pull-down) |
| JD-5/11, JA-5/11 | ground | GND |

GPIO9-14 are the S3's SPI2/FSPI IOMUX pins, so the bulk link uses the
controller's native pin path (the S3 has no SPI3; `LA_BOARD_SPI_HOST` is
`SPI2_HOST`). The classic ESP32 used GPIO18/23/19/4 for SPI and GPIO27 for
data-ready; those pins are invalid on the S3 (GPIO19/20 are USB, GPIO22/23/27
do not exist).

JD-7..10 and JA-1 were wired and bench-characterized on 2026-09-13: framed
captures are bit-exact at 5, 10 and 18 MHz (20/20 runs at 18 MHz), 19/20 MHz
lose chunks to slave FIFO underruns, and the sustained host-verified drain is
0.28 MB/s because the ESP32 Wi-Fi relay was the bottleneck. The 2026-09-17 S3
port replaces that host leg with USB CDC (`la_usb`); the drain is measured over
the native USB port. The 200 ohm series resistor edge characterization and the
64 MB drain gate are recorded in `docs/spi-link.md` and `docs/status.md`.
Reassembly note: the SPI jumpers are separate from the UART ribbon and must be
reconnected after any rewiring; the `test_p91_*` gates fail honestly (no
frames) when they are missing.

The Zybo FTDI console UART1 is for logs and is not the analyzer data endpoint.
Use a separate Windows USB-UART adapter for direct UART0 bring-up, with its
RTS/CTS signals wired as shown. Select the COM port explicitly when more than
one USB serial device is present.

### P5.5 loopback (no Zynq)

To run the gateway loopback gate, remove the JD ribbon (the Zynq TX and the
ESP32 TX would otherwise contend) and jumper the ESP32 alone:

- ESP32-S3: GPIO17 (UART2 TX) <-> GPIO16 (UART2 RX) and GPIO15 (UART2 RTS)
  <-> GPIO14 (UART2 CTS).
- Classic ESP32: GPIO17 <-> GPIO16 and GPIO22 (RTS) <-> GPIO21 (CTS).

Then run `python tools/run.py test-e2e -- --target tcp://HOST:5555 --loopback
--console COMx --loopback-bytes 10000000` (the console is the ESP32 USB port;
without `--loopback` the gate is skipped so the Zynq E2E run stays safe). The
P5.5 gate ran it at 921600 and then at 2 Mbps by setting
`CONFIG_LA_LINK_BAUD` in the ignored `build/esp32/sdkconfig` and reflashing;
the tracked default is now 2000000 (matching the Zynq `LA_LINK_BAUD`). The
`fifo_ovf=` counter on the console must stay 0.

### P5.2 target Unity tests

`python tools/run.py test-esp32-target` builds the standalone Unity app; with
`-- --port COMx` it flashes, resets into run mode and captures the summary
(`build/reports/test-esp32-target-capture.log`). It uses the UART driver's
internal loopback, so no jumpers are needed; restore the gateway firmware with
`python tools/run.py fw-esp32` and the usual flash flow afterwards.

### P6.3 external loopback (JE -> JC)

With the Zynq link active, jumper the patgen outputs to the acquisition
inputs: JE-1..4 -> JC-1..4 (channels 0-3) and JE-7..10 -> JC-7..10
(channels 4-7). Run the burst matrix with the external mux:

```powershell
python tools\run.py test-e2e -- --target tcp://la-esp32.local:5555 --external-loopback -k test_matrix
```

The protocol modes (UART, SPI, I2C) use channels 0-4 only. With all eight
JE-1..4 -> JC-1..4 and JE-7..10 -> JC-7..10 jumpers installed the matrix runs
at the default `--channel-mask 0xFF` (verified 18/18 on 2026-09-11). When
fewer than eight jumpers are available, mask the unwired channels, for
example `--channel-mask 0x3F` for channels 0-5; the step modes are then
verified over the enabled channels. Keep the JD ribbon connected: these
jumpers are separate from the P5.5 ESP32 loopback, which requires the JD
ribbon removed.

## Power and Operating Rules

- Keep the ESP32 on its own USB or approved 3.3 V supply and connect signal ground.
- Do not power an ESP32 GPIO from a 5 V rail.
- Keep acquisition leads short; inspect idle levels before enabling a patgen output.
- JP5 is JTAG for development and SD for release. Power-cycle after changing it.
- `[HUMAN]` Confirm the board revision, header pin numbering and ground pin from the Zybo schematic.
- `[HUMAN]` Verify continuity, common ground, RTS/CTS direction, 3.3 V levels and no 5 V exposure before `program`, `flash-esp` or HIL.
