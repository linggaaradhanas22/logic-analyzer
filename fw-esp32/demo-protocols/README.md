# ESP32-S3 combined UART + SPI + I2C demonstration

Standalone ESP-IDF stimulus for testing the Zybo analyzer with three protocols
at once. Every demo pin is on the **ESP32-S3-DevKitC-1 J1 header**, the same
header row as GPIO17. Flashing this app replaces the ESP32 gateway/UART demo.

| Signal | ESP32-S3 pin | Header | Zybo analyzer |
|---|---|---|---|
| UART TX | GPIO17 | J1-10 | **JC-1 / CH0** |
| SPI SCK | GPIO12 | J1-18 | **JC-2 / CH1** |
| SPI MOSI | GPIO11 | J1-17 | **JC-3 / CH2** |
| SPI CS | GPIO10 | J1-16 | **JC-4 / CH3** |
| I2C SCL (master) | GPIO9 | J1-15 | **JC-7 / CH4** |
| I2C SDA (master) | GPIO8 | J1-12 | **JC-8 / CH5** |
| Ground | GND | J1-22 (or J3-1/21/22) | **JC-5 or JC-11** |

Two additional jumpers make the I2C demo self-contained:

- **GPIO9 ↔ GPIO4** (master SCL to slave SCL; GPIO4 is J1-4)
- **GPIO8 ↔ GPIO5** (master SDA to slave SDA; GPIO5 is J1-5)

The ESP32 uses one I2C controller as master and the other as a slave at address
`0x50`, so no external sensor is required. The analyzer taps the master-side
SCL/SDA (`GPIO9`/`GPIO8`) and common ground. If the two loopback jumpers are
missing, I2C still produces START/address/NACK activity but no ACK or read data.

All signals are **3.3 V**. Remove any JE→JC self-test jumper on a used input.
Do not connect either board's VCC to the other; power each board from its own
USB cable. Keep the ESP32-S3 native USB cable connected for console/ROM access.

## Firmware behavior

- **UART:** GPIO17, 115200 8N1, repeating `LA-S3\n` (`4C 41 2D 53 33 0A`), 10 ms gap.
- **SPI:** mode 0, 8-bit, **10 kHz**, CS active low, repeating two-byte payload `4C 41`, one frame per 5 ms.
- **I2C:** **10 kHz**, address `0x50`; master writes `4C 41`, then reads two bytes
  `41 33` from the on-board slave. One write/read pair repeats about every 20 ms.

The four protocol tasks run independently and the UART0/native-USB console logs
startup and a heartbeat.

## Build and flash

From a normal PowerShell window at the project root, run setup once after
installing or moving the project (safe to repeat):

```powershell
python tools/run.py setup-esp32
```

Then build and flash. With `-p` omitted, the runner selects a single Espressif
native USB port (`303A:1001`) when available, otherwise uses Espressif's serial
port detection. Explicit `-p` or an existing `ESPPORT` setting takes precedence:

```powershell
python tools/run.py demo-esp32-protocols
python tools/run.py demo-esp32-protocols -- flash
```

If multiple ESP boards are connected, add `-p` and the actual COM number before
`flash`. Use the ESP32's native USB ROM port (`303A:1001`) or the
UART-labelled bridge if it enumerates. If auto-reset does not reach download
mode, hold **BOOT**, tap **RESET/EN**, release **BOOT**, then flash. Press
**RESET/EN** once after flashing to start the app. Build output and configuration
stay under `build/esp32-protocols-demo/`; reports are
`build/reports/demo-esp32-protocols.{json,log}`.
No manual ESP-IDF shell activation is needed. A stale workspace after project
relocation is archived under `build/esp32-protocols-demo-relocated-<timestamp>/`
(including its custom sdkconfig); a fresh build uses the demo defaults.
Setup reports are `build/reports/setup-esp32.{json,log}`.

## Analyzer settings

Connect LA UI to the **Zybo J10** port (currently COM13), not the ESP32 port.
Use **Patgen = off**.

For a capture that includes complete I2C write and read transactions, use
**10 MS/s, 256k words, raw, Pre 0, no edge triggers** (25.6 ms of record).
64k gives only 6.4 ms and may contain just one I2C phase.

| Slot | Decoder text |
|---|---|
| 0 | `uart:rx=0,baud=115200` |
| 1 | `spi:sck=1,mosi=2,cs=3,cpol=0,cpha=0,word=8,lsb=0` |
| 2 | `i2c:scl=4,sda=5` |

Then **Capture** and **Decode capture**. Expected results:

- UART bytes `4C 41 2D 53 33 0A` (`LA-S3\n`) on CH0.
- SPI frames with MOSI bytes `4C 41`, framed by CS assert/release on CH3.
- I2C START, address `0x50` write, ACK, `4C`, `41`, STOP; then read address
  `0x51`, ACK, `41`, `33`, NACK, STOP.

For **parallel live decoding**, click **Apply live config**, then start Slots 0,
1 and 2. All three decoder tasks run simultaneously on the one acquisition
stream. Watch that all three panes advance and `client` drops stay zero. The
10 kHz SPI/I2C rates and 115200 UART are within the qualified live rates.

## Parallelism limits

- **All eight channels are always sampled together** in one capture. This demo
  proves that CH0–CH5 can be acquired in the same record.
- **Capture and live decoding are mutually exclusive.** Starting a capture
  automatically stops a live session, and starting live requires idle; the
  datapath is either finite burst or continuous stream.
- **Live decoding can run up to three slots/protocols in parallel**, as this
  demo does. They share the Zynq acquisition stream, USB link and host replay
  ring; heavy event rates can still cause reported loss.
- **Only one capture job runs at a time.** You cannot start two independent
  capture records concurrently, but you can decode one captured record with
  up to three protocol slots afterward.
- The unified UI supports up to four live browser tabs. This is a preview path, not a
  lossless live recorder; use burst capture/export for a record to retain.
