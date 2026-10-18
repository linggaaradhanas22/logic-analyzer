# ESP32-S3 external UART demonstration

For simultaneous UART + SPI + I²C testing, use the newer
[`../demo-protocols/`](../demo-protocols/) app instead. This single-UART app is
retained as a minimal fallback.

Standalone ESP-IDF stimulus for the direct-USB Zybo analyzer. Flashing this app
replaces the ESP32 gateway application. It transmits `LA-S3\n` on **GPIO17** at
**115200 baud, 8N1**, followed by a nominal 2 ms FreeRTOS delay. GPIO16 is the
unused RX input. UART0 on the programming bridge prints a heartbeat each second,
mirrored to native USB Serial/JTAG. The native USB gateway CDC service is not
part of this app; native USB now uses the ROM/hardware Serial/JTAG identity.

## Setup, build and flash

Use a normal PowerShell window at the project root. Run setup once after
installing or moving the project (it is also safe to repeat). Tools and the
ESP-IDF Python environment stay inside this project; no ESP-IDF shell or manual
PATH activation is required.

```powershell
python tools/run.py setup-esp32
```

Build, then flash the connected ESP32-S3. These commands are copyable as written;
flashing without `-p` selects a single connected Espressif native USB port
(`303A:1001`) when available, otherwise uses Espressif's serial-port detection.
An explicit `-p` or existing `ESPPORT` setting overrides automatic selection.

```powershell
python tools/run.py demo-esp32-uart
python tools/run.py demo-esp32-uart -- flash
```

Use the **UART-labelled USB bridge port**. If several ESP boards are connected,
select this board explicitly by adding `-p` and its actual Windows COM number
before `flash`. To list ports:

```powershell
& .\.tools\espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe -m serial.tools.list_ports -v
```

Alternatively, keep native USB connected, hold BOOT, tap RESET/EN and release
BOOT, then select the newly enumerated Espressif `303A:1001` ROM port.
Generated SDK configuration, binaries and workspace are isolated beneath
`build/esp32-uart-demo/`; build/flash reports are
`build/reports/demo-esp32-uart.{json,log}`. The runner resolves paths from itself.
After relocation, the runner archives a stale CMake workspace under
`build/esp32-uart-demo-relocated-<timestamp>/` and builds a fresh workspace using
the demo defaults. The archived directory retains any previous custom sdkconfig.
Setup reports are `build/reports/setup-esp32.{json,log}`. If required tools are
missing, the runner reports the ESP-IDF export error and asks you to rerun setup.
If auto-reset cannot enter download mode, hold BOOT, tap RESET/EN, release BOOT
and retry. Press RESET/EN after flashing if it remains in the bootloader.

With power removed, connect ESP32 GND to Zybo JC-5/11 and GPIO17 to JC-1 (CH0).
Remove any JE-1 -> JC-1 loopback and former gateway wiring. Do not connect power
pins; all acquisition signals must be 3.3 V. Power the boards through USB.

In LA UI, connect to the **Zybo J10** endpoint, not the ESP32 bridge. Capture:
10 MS/s, 64k, raw, Pre 0, blank edge triggers, Patgen off, timeout 30 s.
Slot 0: `uart:rx=0,baud=115200`; other slots blank. Click Capture, then Decode
capture; expected complete messages are `4C 41 2D 53 33 0A`. The record spans
6.4 ms. Live: while idle apply Pattern generator off, Apply live config, then
Slot 0 Start; observe the separate Live pane and Stop when finished.

Build/flash success and a heartbeat confirm software startup, not electrical
waveform correctness. Verify the physical GPIO17 signal with the analyzer.
