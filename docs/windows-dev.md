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
python tools/run.py fw-esp32
python tools/run.py program
python tools/run.py flash-esp -- --port COM12
python tools/run.py hil
```

`bit`, `fw-zynq`, `fw-esp32`, `program`, `flash-esp` and `hil` stop with an
explicit prerequisite message when the required installation, XSA, board or
bench is unavailable. A Zybo console COM port is not the UART0 analyzer data
endpoint. Select COM ports explicitly when more than one candidate exists.

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
