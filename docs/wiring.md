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

The MVP link is PS UART0 through EMIO on JD at 921600 baud initially, with
RTS/CTS enabled. Direction is named from the ESP32 perspective. The confirmed
gateway module is a classic ESP32 (DOIT ESP32 DevKit V1, ESP32-WROOM-32,
CP2102 USB-UART, 4 MB flash, no PSRAM); the approved pin map keeps the JD
ribbon and breadboard wiring on the right-hand header row:

| JD signal | Zynq UART0 EMIO | ESP32 connection | Meaning |
|---|---|---|---|
| JD-1 | TX | GPIO16 RX | Zynq data to ESP32 |
| JD-2 | RX | GPIO17 TX | ESP32 data to Zynq |
| JD-3 | RTS | GPIO21 CTS | Zynq may send when ESP32 permits |
| JD-4 | CTS | GPIO22 RTS | Zynq receive permission |
| JD-5/11 ground | signal ground | ESP32 GND | common reference |

GPIO16/17 are the UART2 default pins; GPIO21/22 replace the UART2 default
RTS/CTS (8/7) to stay on one side. The earlier GPIO14/15 example is not used:
GPIO15 is a boot-strapping pin. `fw-esp32/components/la_board/include/la_board.h`
is the single source for this mapping and the ESP-IDF build compiles it.

The ESP32 is powered from its own USB; do not connect the JD power pins. The
ESP32 USB console (UART0 through the CP2102, `COMx`) is for logs only. JD-7
through JD-10 remain reserved for the P9 SPI link and a separate verified
data-ready signal is required for P9.

The Zybo FTDI console UART1 is for logs and is not the analyzer data endpoint.
Use a separate Windows USB-UART adapter for direct UART0 bring-up, with its
RTS/CTS signals wired as shown. Select the COM port explicitly when more than
one USB serial device is present.

### P5.5 loopback (no Zynq)

To run the gateway loopback gate, remove the JD ribbon (the Zynq TX and the
ESP32 TX would otherwise contend) and jumper the ESP32 alone:

- GPIO17 (UART2 TX) <-> GPIO16 (UART2 RX)
- GPIO22 (UART2 RTS) <-> GPIO21 (UART2 CTS)

Then run `python tools/run.py test-e2e -- --target tcp://HOST:5555 --loopback
--console COMx --loopback-bytes 10000000` (the console is the ESP32 USB port;
without `--loopback` the gate is skipped so the Zynq E2E run stays safe). Run
it once at the default 921600 and once after rebuilding the gateway with
`CONFIG_LA_LINK_BAUD=2000000` in the ignored `build/esp32/sdkconfig` and
reflashing. The `fifo_ovf=` counter on the console must stay 0.

### P5.2 target Unity tests

`python tools/run.py test-esp32-target` builds the standalone Unity app; with
`-- --port COMx` it flashes, resets into run mode and captures the summary
(`build/reports/test-esp32-target-capture.log`). It uses the UART driver's
internal loopback, so no jumpers are needed; restore the gateway firmware with
`python tools/run.py fw-esp32` and the usual flash flow afterwards.

## Power and Operating Rules

- Keep the ESP32 on its own USB or approved 3.3 V supply and connect signal ground.
- Do not power an ESP32 GPIO from a 5 V rail.
- Keep acquisition leads short; inspect idle levels before enabling a patgen output.
- JP5 is JTAG for development and SD for release. Power-cycle after changing it.
- `[HUMAN]` Confirm the board revision, header pin numbering and ground pin from the Zybo schematic.
- `[HUMAN]` Verify continuity, common ground, RTS/CTS direction, 3.3 V levels and no 5 V exposure before `program`, `flash-esp` or HIL.
