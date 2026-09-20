# Native Windows Development

The fixed project root is the directory containing this file. All project-owned
dependencies and generated outputs remain below it.

## First setup

1. Use a 64-bit CPython and create the project environment:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -e .
```

2. Install or expose CMake, Ninja, MSVC x64 Build Tools, the Windows SDK,
   Vivado/Vitis 2026.1, native Questa and PulseView. Keep the ESP-IDF source in
   `.deps\esp-idf` and its tools in `.tools\espressif`.

3. Provision the pinned native ESP-IDF release once. Use a base 64-bit CPython
   for `install-python-env`, not the project `.venv`:

```powershell
New-Item -ItemType Directory -Force .deps, .tools | Out-Null
git clone --branch v5.5.2 --recursive https://github.com/espressif/esp-idf.git .deps\esp-idf
$env:IDF_PATH = (Resolve-Path .deps\esp-idf).Path
$env:IDF_TOOLS_PATH = (Resolve-Path .tools).Path + "\espressif"
& .\.venv\Scripts\python.exe $env:IDF_PATH\tools\idf_tools.py --non-interactive install --targets=esp32
$BasePython = "C:\path\to\base\python.exe"
& $BasePython $env:IDF_PATH\tools\idf_tools.py --non-interactive install-python-env --features=core
```

Wi-Fi credentials for the gateway live only in the generated, git-ignored
`build\esp32\sdkconfig`. Set or change them with the tracked helper, then
rebuild and reflash:

```powershell
.\.venv\Scripts\python.exe tools\set_wifi.py
python tools\run.py fw-esp32
```

The helper prompts for the SSID, hides the password input and writes only the
two `CONFIG_LA_WIFI_*` lines; credentials never enter tracked files.

For the optional project-local PulseView install, use the official 64-bit
0.4.2 installer:

```powershell
$ProjectRoot = (Resolve-Path .).Path
$PulseInstaller = "build\cache\PulseView-0.4.2-64bit-static-release-installer.exe"
Invoke-WebRequest -Uri "https://sigrok.org/download/binary/pulseview/pulseview-0.4.2-64bit-static-release-installer.exe" -OutFile $PulseInstaller
Unblock-File -LiteralPath $PulseInstaller
& $PulseInstaller /S "/D=$ProjectRoot\build\pulseview"
```

Set `PULSEVIEW_EXE` if it is placed elsewhere.

4. Run the checker from the selected native developer shell:

```powershell
python tools/check_env.py --write-lock
python tools/run.py query-board -- --write-lock
```

The checker accepts `VIVADO_HOME`, `VITIS_HOME`, `QUESTA_ROOT`, `IDF_PATH`,
`IDF_TOOLS_PATH`, `CMAKE_EXE`, `NINJA_EXE`, `PULSEVIEW_EXE` and
`ARM_TOOLCHAIN_ROOT`. Paths for project-owned environments, caches, temporary
files and outputs must resolve under this root. Machine-installed AMD and
Questa binaries may remain outside the root. The runner imports the x64 MSVC
developer environment and the project-local ESP-IDF tool environment for their
respective targets. The local ESP-IDF smoke currently uses the default `esp32`
target only; confirm the exact module before selecting GPIOs or flashing
hardware.

## Build and test

```powershell
python tools/run.py gen
python tools/run.py test-native
python tools/run.py test-esp32-mocks
python tools/run.py fw-esp32
python tools/run.py test-host
python tools/run.py lint
python tools/run.py sim
```

The runner sets `TEMP`, `TMP`, `PIP_CACHE_DIR` and tool-specific output paths to
ignored directories under `build`. Target commands do not depend on the
caller's working directory or an activated tool environment. Logs and JSON
evidence are stored in `build/reports`.

## Firmware and hardware targets

```powershell
python tools/run.py bit
python tools/run.py fw-zynq
python tools/run.py jtag-zynq
python tools/run.py fw-esp32
python tools/run.py program
python tools/run.py flash-esp -- --port COM12
python tools/run.py hil -- --port COM12
```

For the normal Zybo development loop, set JP5 to JTAG and power-cycle once
after moving the jumper. `jtag-zynq` uses the board's PROG/UART USB cable to
reset the PS through JTAG SRST, program `logic_analyzer.bit`, run the generated
FSBL through its JTAG handoff, download the application ELF and start
Cortex-A9 #0. Since the V00 pivot (2026-09-20) the default profile is the
direct-USB application `la_app_usb`; pass `--profile gateway` to build/load
`la_app` for the ESP32 gateway. UART1 console output remains available on the
FTDI COM port; the microSD card is only required for the final release boot
gate.

`bit`, `fw-zynq`, `jtag-zynq`, `fw-esp32`, `program`, `flash-esp` and `hil` stop with an
explicit prerequisite message when the required installation, XSA, board or
bench is unavailable. A Zybo console COM port is not the UART0 analyzer data
endpoint. Select COM ports explicitly when more than one candidate exists.

The `hil` target runs `tests/hil/test_zynq_serial.py` against the analyzer
link: PS UART0 through EMIO on the JD header, 2 Mbps 8N1 with RTS/CTS
(`LA_LINK_BAUD`; 921600 during bring-up). It
needs a separate 3.3 V USB-UART adapter wired as TX->JD-2, RX->JD-1,
RTS->JD-4, CTS->JD-3 and GND->JD-5 or JD-11; set `LA_HIL_PORT=COMx` or pass
`--port COMx`. Without a port the HIL tests are skipped, and the unconnected
JD RX input floats and storms the UART0 interrupt, so do not judge the
firmware from a link-less run.

## Changing Wi-Fi networks

Only the ESP32 credentials and the host target change when the bench moves to a
different Wi-Fi network; the Zynq link is unaffected.

### PC

1. Join the same network as the gateway. The classic ESP32 is 2.4 GHz only, so
   a hotspot must offer 2.4 GHz.
2. Verify with `netsh wlan show interfaces` (connected, correct SSID, 2.4 GHz).
3. No address configuration: reach the gateway through its mDNS name
   `la-esp32.local`. The DHCP address changes per network; never hardcode it.

### ESP32 gateway

1. Set the credentials (the helper prompts and hides the password):
   `.venv\Scripts\python.exe tools\set_wifi.py`
2. Rebuild: `python tools\run.py fw-esp32`
3. Export the project-local ESP-IDF environment once per shell:
   `$env:IDF_PATH = (Resolve-Path .deps\esp-idf).Path`,
   `$env:IDF_TOOLS_PATH = (Resolve-Path .tools\espressif).Path`,
   `. .\.deps\esp-idf\export.ps1`
4. Flash and watch the console (run from the project directory because the
   build directory is configured for it):
   `Set-Location fw-esp32` then
   `idf.py -B ..\build\esp32 -p COM5 flash monitor`
   If the flash stalls or reports "Failed to connect / wrong boot mode", this
   DOIT board's auto-reset did not assert GPIO0: hold BOOT, tap EN, release
   BOOT, then flash with reset disabled and attach the console:

```powershell
Set-Location build\esp32
python $env:IDF_PATH\components\esptool_py\esptool\esptool.py --chip esp32 -p COM5 -b 460800 --before no_reset --after hard_reset write_flash @flash_args
Set-Location ..\..
Set-Location fw-esp32
idf.py -B ..\build\esp32 -p COM5 monitor
```

5. Success signs: `station started (ssid "...")`, a `got ip` line,
   `net CONNECTED` and `fifo_ovf=0`. Exit the monitor with `Ctrl+]` and
   `Set-Location ..`.

### Zynq

Not affected by the network. Power the board with JP5=JTAG and run
`python tools\run.py jtag-zynq`; COM6 shows the P4 boot line and status ticks.

### Readiness check

```powershell
.venv\Scripts\la-cli.exe ping -t tcp://la-esp32.local:5555
```

Expected `pong: fw ... pl ... uptime ...`. On a timeout, tap EN on the ESP32
once and retry (the known stale-client state); if it still fails, check the
monitor log and `ping la-esp32.local`. The burst E2E gate then runs as
`python tools\run.py test-e2e -- --target tcp://la-esp32.local:5555`.

### Live web view

Since the V00 pivot the default live view is hosted by the PC bridge: start
`la-cli live -t serial://COMx --decoder uart:rx=0,baud=115200 --serve` (or
`la-web`) and open `http://127.0.0.1:8080/`. The bridge consumes the same live
events, keeps a bounded replay ring and serves SSE; see `docs/live-web.md` and
`docs/using-the-analyzer.md` section 7. The [HUMAN] browser smoke is opening
the page during a patgen session and reloading it.

The gateway profile also keeps its ESP32-hosted page (P10.1). Open
`http://la-esp32.local/` (or the IP from the boot log) while a live session
runs (`la-cli live -t tcp://la-esp32.local:5555 --decoder uart:rx=0,baud=115200`);
decoded bytes appear in per-decoder tabs with a stats bar. The page is embedded
in the firmware, so no external assets or internet access are needed. Bench
gate: `python tools\run.py test-e2e -- --target tcp://la-esp32.local:5555 --live -k test_p10`.

## Troubleshooting

- `cmake` missing: open a Visual Studio x64 Native Tools prompt or set
  `CMAKE_EXE` to the installed `cmake.exe`; do not use a Linux replacement.
- `cl` missing: install Desktop development with C++, MSVC x64 and a Windows
  SDK, then rerun from the matching developer prompt.
- Questa binaries missing: set `QUESTA_ROOT` to the directory containing
  `vlib.exe`, `vlog.exe` and `vsim.exe`. The actual compile/simulation smoke is
  required before RTL work.
- Vivado/Vitis missing: set `VIVADO_HOME` and `VITIS_HOME` to the installed
  2026.1 directories. The checker also looks for the Arm compiler and device
  data.
- ESP-IDF missing: provision the pinned source under `.deps\esp-idf`, set
  `IDF_PATH` and `IDF_TOOLS_PATH`, and use the separate managed Python
  environment created by `idf_tools.py`; Arduino packages are not an ESP-IDF
  build.
- A path-limit failure: keep the fixed root, shorten generated project names or
  nested build names, and rerun the native gate. Do not move the checkout.
