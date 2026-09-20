# Using the Analyzer

This is the operating guide: how to wire the boards, bring the link up, capture
signals, decode live traffic and open the results. Development setup (toolchain,
venv, ESP-IDF, Vivado/Vitis) lives in `docs/windows-dev.md`; the electrical and
wiring contract is `docs/wiring.md`; current limits are `docs/limits.md`.

All commands run from the project root in PowerShell. The host tool is the
console script installed in the project environment:
`.venv\Scripts\la-cli.exe`.

## 1. What you need

- Zybo Z7-10 with the project bitstream and firmware (JTAG development load or
  SD release boot).
- ESP32-S3-DevKitC-1 gateway, flashed with the project firmware. The analyzer
  link uses its native USB port (CDC-ACM, `serial://COMx`); for the Wi-Fi TCP
  link and the live web view it joins the same **2.4 GHz** network as the PC.
- PC with the project `.venv` and, for viewing captures, PulseView
  (`build/pulseview`) or any VCD viewer. The unified UI (`la-ui`) renders the
  waveform in Chrome/Edge, so PulseView is optional for interactive work.
- Wiring per `docs/wiring.md`: signals to **JC** (3.3 V only), the UART ribbon
  **JD-1..4 + GND** to the ESP32, optional **JD-7..10 + JA-1** SPI bulk jumpers,
  and optional **JE→JC** loopback jumpers for self-test.
- `[HUMAN]` checks before power: common ground, RTS/CTS direction, no 5 V
  exposure, correct JP5 position.

## 2. Build and load

```powershell
python tools\run.py bit          # bitstream + XSA (Vivado)
python tools\run.py fw-zynq      # Zynq FreeRTOS application (Vitis)
python tools\run.py fw-esp32     # ESP32 gateway firmware (ESP-IDF)
```

**Zynq, development (JP5 = JTAG):**

```powershell
python tools\run.py jtag-zynq
```

This programs the PL, runs the FSBL, downloads `la_app.elf` and starts CPU0.
The FTDI COM port shows the boot banner and periodic `LA status:` lines.

**Zynq, release (JP5 = SD):** build `BOOT.BIN` and copy it to a FAT-formatted
SD card, then power-cycle with JP5 = SD.

```powershell
python tools\run.py boot
```

**ESP32-S3 gateway:**

```powershell
.\.venv\Scripts\python.exe tools\set_wifi.py   # once per network; prompts, never committed
python tools\run.py fw-esp32
```

Connect both DevKitC-1 micro-USB ports: **"UART"** (CP2102 bridge, console and
flashing) and **"USB"** (native USB-OTG, the analyzer host transport). The S3
resets normally, so flash and monitor over the UART port without a BOOT/EN
sequence:

```powershell
Set-Location fw-esp32
idf.py -B ..\build\esp32 -p COMx flash monitor
```

Success signs: `USB CDC ready on the native USB port`, `station started`,
`got ip`, `net CONNECTED`, `fifo_ovf=0`. Exit the monitor with `Ctrl+]`.

## 3. Verify the link

**USB CDC host link (P9.1 fast path).** Plug the native "USB" port into the PC;
it enumerates as a USB serial device. `la-cli ports` lists the candidates: the
CP2102 console and the CDC data port. Open the data port:

```powershell
.\.venv\Scripts\la-cli.exe ports
.\.venv\Scripts\la-cli.exe ping -t serial://COMx
```

While that port is open the gateway routes the analyzer stream to USB and
refuses TCP analyzer clients (the P10.1 web view is unaffected). Close it to
use the Wi-Fi/TCP leg again.

**Wi-Fi TCP link.**

```powershell
.\.venv\Scripts\la-cli.exe ping   -t tcp://la-esp32.local:5555
.\.venv\Scripts\la-cli.exe status -t tcp://la-esp32.local:5555
```

Expected: `pong: fw ... pl 0x4C410002 uptime ...` and a STATUS snapshot with
`state idle`. If the ping times out right after a flash or reset, wait ~15 s and
retry once: the first TCP session can need a listener refresh.

`la-cli ports` lists USB-UART candidates (CP210x/CH340/FTDI) when you use a
direct `serial://COMx` link instead of the gateway.

The Zynq also has an **adopted direct-USB profile** (`fw-zynq -- --profile usb`,
`jtag-zynq -- --profile usb`): the Zynq enumerates as a CDC-ACM device and the
same `la-cli` commands work over `serial://COMx`. It passes the enumeration,
burst, trace, recovery and 64 MiB drain gates (1.62 MB/s); the gateway remains
supported and is still the default build (`docs/usb-link.md` section 16).

## Two independent modes

Burst capture and live decode are alternatives, not steps:

- `capture` configures, arms, drains and stops its own session; you can use it
  without ever running `live`.
- `live` configures the stream, starts it, streams decoded events and stops on
  its own (`--count`, `--timeout` or Ctrl+C); it does **not** need a preceding
  capture or ARM.
- They cannot run at the same time: both use the same acquisition engine and
  memory pool. Finish or abort one before starting the other; if a capture is
  armed, run `la-cli abort -t ...` first.
- Every command latches its own configuration at CONFIG/ARM or
  DECODER_CFG/LIVE_START, so there is no "assert once, then use" step.
- With `--patgen`, the CLI clears the pattern-generator enable before setting
  the new mode, so repeated self-test sessions relatch instead of keeping the
  previous pattern.

## 4. First capture without external signals (self-test)

The pattern generator can drive the capture input internally, so you can prove
the whole path before connecting anything:

```powershell
.\.venv\Scripts\la-cli.exe capture -t tcp://la-esp32.local:5555 `
    --rate 10M --words 4096 --mode raw --patgen counter --out selftest.sr
```

`--patgen` enables `CTRL.SELFTEST_EN` (the internal mux) and latches the pattern
generator. **Omit it for real signals**, or the JC inputs are ignored.

| Mode | Pins | Traffic |
|---|---|---|
| `counter` / `walking` / `prbs` | all 8 | raw 8-bit patterns (capture self-test) |
| `uart` | CH0 | repeated `LA-TEST\n` at 115200 8N1 |
| `uart_text` | CH0 | repeated `Hello, LA!\r\n` |
| `uart_bin` | CH0 | repeated 0x00..0x0F ramp (non-printable bytes) |
| `uart_ferr` | CH0 | `L`, `A` with a low stop bit, `0x55` — a FRAME_ERR case |
| `spi` | CH0 SCK, CH1 MOSI, CH2 CS_N | one CS frame `4C 41` |
| `spi_multi` | CH0 SCK, CH1 MOSI, CH2 CS_N | one CS frame `DE AD BE EF` |
| `spi16` | CH0 SCK, CH1 MOSI, CH2 CS_N | one CS frame, two 16-bit words `1234 ABCD` |
| `i2c` | CH3 SCL, CH4 SDA | write to 0xA0: `4C 41`, ACKs |
| `i2c_read` | CH3 SCL, CH4 SDA | read from 0xA1: data `4C`, master NACK |
| `i2c_nack` | CH3 SCL, CH4 SDA | write to 0xA0: data `4C`, slave NACK |
| `i2c_alt` | CH3 SCL, CH4 SDA | write to address 0x3C: `4F 4B` |

The generator drives **one protocol at a time**; a second decoder on other
channels needs an external signal. Decoder settings must match the mode:
`spi16` needs `word=16`, `i2c_read`/`i2c_nack` exercise the NACK event, and
`uart_ferr` exercises FRAME_ERR.

Open `selftest.sr` in PulseView. The counter mode steps on channel 0.

## 5. Capturing real signals

Connect the signal to a **JC** channel (JC-1..4, JC-7..10 = CH0..CH7; 3.3 V
only). Then:

```powershell
# 10 MS/s, 64k post-trigger samples, raw packing, rising edge on CH0
.\.venv\Scripts\la-cli.exe capture -t tcp://la-esp32.local:5555 `
    --rate 10M --words 64k --mode raw --trig-rise 0 --out capture.sr

# transition encoding with 1000 pre-trigger samples, falling edge on CH2
.\.venv\Scripts\la-cli.exe capture -t tcp://la-esp32.local:5555 `
    --rate 100M --words 16k --mode txn --pre 1000 --trig-fall 2 --out capture.vcd

# PNG snapshot
.\.venv\Scripts\la-cli.exe capture -t tcp://la-esp32.local:5555 `
    --rate 1M --words 4096 --out capture.png
```

| Option | Meaning |
|---|---|
| `--rate 1M\|10M\|100M` | Requested sample rate; must map to `100 MHz / (div+1)`. The tool prints the actual rate when they differ. |
| `--words N` | Post-trigger samples (`4096`, `64k`, `1M`). |
| `--pre N` | Pre-trigger samples (0..32768). |
| `--mode raw\|txn` | Raw packs 4 samples/word; `txn` stores `{run, sample}` transitions. |
| `--trig-rise CH` / `--trig-fall CH` | Trigger edge on one channel. With no trigger, the capture starts immediately. |
| `--patgen MODE` | Internal self-test source (see step 4). |
| `--out FILE.sr\|.vcd\|.png` | Export format follows the extension. |
| `--timeout S` | Command deadline (default 30 s). |

The host verifies chunk order, the whole-capture CRC32 and the exact sample
count before writing the file. `la-cli abort -t ...` cancels an armed capture.

## 6. Live decode

Live mode streams decoded protocol events instead of raw samples. The decoder
spec is `type:key=value,...`:

- `uart:rx=CH,baud=BAUD`
- `spi:sck=CH[,mosi=CH][,miso=CH][,cs=CH][,cpol=0][,cpha=0][,word=8][,lsb=0][,cs_active=0]`
- `i2c:scl=CH,sda=CH`

```powershell
# Self-test: patgen UART on CH0, decode it, stop after 20 events
.\.venv\Scripts\la-cli.exe live -t tcp://la-esp32.local:5555 `
    --patgen uart --decoder uart:rx=0,baud=115200 --rate 100M --count 20

# Real UART traffic, compact encoding, CSV output
.\.venv\Scripts\la-cli.exe live -t tcp://la-esp32.local:5555 `
    --decoder uart:rx=0,baud=115200 --compact --csv --count 100

# SPI: SCK CH0, MOSI CH1, CS CH2
.\.venv\Scripts\la-cli.exe live -t tcp://la-esp32.local:5555 `
    --decoder spi:sck=0,mosi=1,cs=2

# Stop when the decoded bytes match "LA" (hex 4c41)
.\.venv\Scripts\la-cli.exe live -t tcp://la-esp32.local:5555 `
    --decoder uart:rx=0,baud=115200 --stop-on 4c41
```

| Option | Meaning |
|---|---|
| `--decoder SPEC` | Decoder and channel mapping (above). |
| `--rate` | Sample rate used to derive samples per bit (default 100M). |
| `--patgen MODE` | Drive the pattern generator before starting. |
| `--count N` | Stop after N events (otherwise run until `--timeout` or Ctrl+C). |
| `--compact` | Request the compact `EVT_BLOCK` encoding (less wire traffic). |
| `--stop-on HEX` | Stop on the first decoded byte-pattern match and report the sample index. |
| `--csv` | CSV rows instead of text. |
| `--timeout S` | Overall deadline. |

Qualified rates: 115200 UART (repeatable 60 s zero-drop gate) and 10 kHz SPI/I²C.
1 MHz SPI is a deliberate overload test: the host receives `EVT_OVERRUN`
reports and the system returns to IDLE. Overruns are always reported, never
silent.

## Worked example: decoding an I2C bus

Goal: watch an I2C bus live **and** save a full waveform on the PC.

### Wiring

| Signal | Target | Zybo |
|---|---|---|
| SCL | I2C clock wire | JC-1 = CH0 |
| SDA | I2C data wire | JC-2 = CH1 |
| GND | target ground | JC-5 or JC-11 (GND) |

- Probe the two signal wires plus the target ground. Do **not** connect the bus
  VCC; the analyzer only senses the lines.
- 3.3 V only: if the bus pull-ups go to 5 V, use a level shifter.
- Keep leads short and confirm common ground before powering.
- This is separate from the PC link: the **JD-1..4 + GND** ribbon to the ESP32
  (or a 3.3 V USB-UART adapter on the same pins) carries the data, and
  `la-cli ping -t tcp://la-esp32.local:5555` must answer first.

### Live decode

```powershell
.\.venv\Scripts\la-cli.exe live -t tcp://la-esp32.local:5555 `
    --decoder i2c:scl=0,sda=1 --rate 10M
```

Events print as START / ADDRESS / ACK / DATA / STOP. Stop with Ctrl+C or
`--count N`; add `--csv` to save rows. A dry run without a real bus uses the
internal pattern generator (no jumpers):

```powershell
.\.venv\Scripts\la-cli.exe live -t tcp://la-esp32.local:5555 `
    --patgen i2c --decoder i2c:scl=3,sda=4 --count 10
```

### Full waveform capture

```powershell
.\.venv\Scripts\la-cli.exe capture -t tcp://la-esp32.local:5555 `
    --rate 10M --words 64k --mode raw --pre 1000 --out i2c.sr
```

- `64k` samples at 10 MS/s is a ~6.5 ms window; raise `--words` or use
  `--mode txn` for longer records.
- Trigger: `--trig-fall 1` catches an SDA falling edge (usually a START).
  Without a trigger the capture starts immediately. The CLI exposes one edge
  channel, so a true "SCL high and SDA falling" START condition is not directly
  selectable.
- Open `i2c.sr` in PulseView (`.vcd` in GTKWave, `.png` for a snapshot).

## 7. Unified UI (la-ui)

`la-ui` (or `la-cli ui`) serves the unified workspace at
`http://127.0.0.1:8080/`: one page for connection, burst capture with a
waveform viewer, host-side capture decoding, live decoding, exports, the trace
timeline and the pattern generator. It is a host-only feature (no firmware or
PL change) and connects to the direct-USB analyzer only (`serial://COMx`);
the ESP32/gateway path stays with `la-web`. The page needs Chrome or Edge.

```powershell
# Start idle; connect from the page (port dropdown + Connect)
.\.venv\Scripts\la-ui.exe

# Or connect immediately and pre-configure the live decoder slots
.\.venv\Scripts\la-ui.exe -t serial://COM13 `
    --decoder uart:rx=0,baud=115200 `
    --decoder spi:sck=1,mosi=2,cs=3
```

1. **Connect.** Pick a COM port (or type `serial://COMx`) and click
   **Connect**; the header shows `linked` plus the firmware/PL identity.
   **Ping** refreshes it.
2. **Capture.** Set rate, words, pre-trigger, raw/txn mode, trigger edges and
   an optional patgen mode, then **Capture**. The waveform appears when the
   transfer finishes; the status line shows the sample count. **Abort**
   cancels a running capture. Captures above 1,000,000 samples are allowed
   with a warning (the view is decimated).
3. **Inspect.** Wheel zooms at the cursor, drag pans, double-click fits.
   Click the ruler to place cursor A, Shift+click for cursor B, drag a line
   to move it; Δt and 1/Δt show in the Channels panel. **Zoom A/B** fits the
   cursor range, **◀ edge / edge ▶** step to the selected channel's edges,
   and the search box finds a 0/1/x pattern on that channel. Channel names,
   colors, visibility, order and heights persist in the browser. **PNG**
   saves the visible canvas.
4. **Decode.** Fill the slot spec fields (same grammar as `la-cli live`) and
   press **Decode capture**; decoded UART/SPI/I²C events are drawn as
   annotation rows under the signals. Decoding uses the same C decoders as
   the device through the native DLL, so build it first with
   `python tools/run.py test-native`.
5. **Live.** **Apply live config** validates the three slot specs, then each
   pane's **Start**/**Stop** controls that decoder independently; decoded
   events stream onto the pane's time axis (up to 100,000 events kept per
   slot, drops counted). Starting a capture stops a running live session
   automatically, and vice versa.
6. **Export / open.** `.sr`, `.vcd`, events CSV and samples CSV download
   from the Export panel; **Open** loads a saved `.sr`/`.vcd`/`.csv` without
   a device, so the viewer and decode work offline. `.sr` is the exact-count
   format; a VCD cannot recover a trailing constant run.
7. **Trace / patgen.** **Dump** renders the P8.3 task/queue timeline;
   **Arm** latches a pattern-generator mode for the next session.

The legacy three-pane live page remains at `http://127.0.0.1:8080/legacy`
(the same contract as `la-web`), and `la-cli` remains the scripted interface.
`la-ui` owns the serial port for its lifetime: stop it before running another
`la-cli`/`la-web` session on the same port. Ctrl+C stops any live session and
releases the port.

## 8. Browser live view (legacy la-web)

Since the 2026-09-20 pivot the live view is hosted on the PC; since V07 the
page controls the session, and since V09 each decoder runs independently. The
bridge consumes the same decoded events as `la-cli live`, keeps a bounded
replay ring and serves the page over SSE; see `docs/live-web.md` for the
frozen contract.

```powershell
# One decoder (UART patgen on CH0): http://127.0.0.1:8080
.\.venv\Scripts\la-web.exe -t serial://COM13 --patgen uart --decoder uart:rx=0,baud=115200

# Three decoders in slots 0/1/2 (one --decoder per slot, in order)
.\.venv\Scripts\la-web.exe -t serial://COM13 --patgen uart `
    --decoder uart:rx=0,baud=115200 `
    --decoder spi:sck=1,mosi=2,cs=3 `
    --decoder i2c:scl=4,sda=5
```

1. Start the command; it prints `live web view: http://127.0.0.1:8080/` and
   waits.
2. Open that URL: each decoder tab has its own **Start**/**Stop** and a state
   pill (`idle`/`streaming`/`unconfigured`); the header has **Stop all**.
3. Click **Start** on Decoder 0: the pill turns **streaming** and that pane
   fills with decoded bytes line by line (`+delta` timestamps). Click **Start**
   on Decoder 1: it joins the same live session (no restart, no gap for
   Decoder 0).
4. Click **Stop** on Decoder 0: only that pane stops; Decoder 1 keeps
   streaming. Stopping the last running decoder ends the session and the
   device returns to IDLE.
5. Reloading the page reconnects the feed (the pill shows reconnecting, then
   connected) and replays the bounded ring; a second feed gets HTTP `409`
   (single-client policy).

A pane with no `--decoder` spec shows `unconfigured` and cannot be started.
`--patgen` drives **one protocol's pins at a time**: UART `CH0`, SPI
`sck=CH0, mosi=CH1, cs=CH2`, I²C `scl=CH3, sda=CH4` (`counter`/`walking`/`prbs`
drive the raw pattern outputs). A decoder on other channels sees undriven pins
— SPI can sit on a stuck `CS_ASSERT` and I²C stays empty — so simultaneous
decoders need external signals on their own channels; the CLI prints a note
when several decoders share one patgen source.
`--autostart` starts every configured decoder immediately and exits when the
session ends (scripted smoke); `--count N` caps a session at N events and
`--timeout S` bounds it. Ctrl+C stops all decoders, closes the server and
releases the analyzer port, so stop it before another `la-cli` session on the
same port. A slow browser drops messages (counted in the stats bar) instead of
blocking the link. The host bridge works over any target (`serial://` or
`tcp://`); the gateway profile also keeps its ESP32-hosted page on port 80
(P10.1) while a gateway live session runs.

No external assets or internet access are needed.

## 9. Task/queue trace

```powershell
.\.venv\Scripts\la-cli.exe trace -t tcp://la-esp32.local:5555 --start
# ... run the workload ...
.\.venv\Scripts\la-cli.exe trace -t tcp://la-esp32.local:5555 --dump `
    --out trace.txt --png trace.png --title "live UART"
```

The PNG is a relative-time timeline of task switches, queue/stream-buffer
activity and app markers. The dump is best-effort: a few `LOG` records can drop
under relay pressure.

## 10. SPI bulk transport (P9.1)

By default burst `DATA_*` frames travel over the UART. To route them over the
SPI slave (JD-7..10 + JA-1 wired), select it once per session:

```powershell
.\.venv\Scripts\la-cli.exe link -t serial://COMx --spi --clock 18
.\.venv\Scripts\la-cli.exe capture -t serial://COMx --rate 10M --words 64k --out big.sr
```

Commands and ACKs stay on the UART. Framed transfers are bench-clean at
5/10/18 MHz (18 MHz is the measured ceiling; 19/20 MHz lose chunks). For large
captures use the native USB CDC port (`serial://COMx`): the ESP32-S3 USB-OTG is
full-speed, so the host drain is bounded near 1.2 MB/s, far above the ~0.28 MB/s
of the Wi-Fi TCP leg; see `docs/limits.md`.

## 11. Command reference

| Command | Purpose |
|---|---|
| `la-cli ports` | List USB-UART candidates. |
| `la-cli ping -t URI` | Check the link and firmware identity. |
| `la-cli status -t URI` | STATUS snapshot: state, counters, watermarks, latency. |
| `la-cli abort -t URI` | Abort a running capture. |
| `la-cli capture -t URI [options]` | Arm a burst capture and export `.sr`/`.vcd`/`.png`. |
| `la-cli live -t URI [options]` | Stream decoded UART/SPI/I²C events. |
| `la-cli live -t URI ... --serve [--bind HOST] [--port N] [--autostart]` | Serve the host browser page; each pane's Start/Stop controls its decoder (V09). Repeat `--decoder` for slots 0-2. |
| `la-cli web -t URI [options]` / `la-web` | Standalone host live web view (same implementation as `live --serve`). |
| `la-cli ui [options]` / `la-ui` | Unified workspace: capture + waveform viewer, live decode, host decode, exports, trace, patgen. `--target` optional; `--decoder` repeatable; `--bind`/`--port`; legacy page at `/legacy`. |
| `la-cli trace -t URI [--start\|--stop\|--dump]` | Task/queue trace ring. |
| `la-cli link -t URI --spi\|--uart [--clock MHz]` | Select the burst transport. |

Target URIs: `serial://COMx` (gateway native USB CDC port, or a direct 3.3 V
USB-UART adapter on JD-1..4 + GND, `[HUMAN]`), `tcp://la-esp32.local:5555`
(Wi-Fi gateway) or `tcp://<ip>:5555`.

## 12. Troubleshooting

- **Ping times out after a flash/reset:** wait ~15 s and retry once (listener
  refresh). Then check the ESP32 console for `net CONNECTED` and the Wi-Fi
  association.
- **No PONG at all:** confirm the PC and gateway are on the same 2.4 GHz
  network, the UART ribbon is on JD-1..4 + GND, and the Zynq firmware is
  running (`jtag-zynq` or SD boot).
- **First capture after `--patgen` looks wrong:** `SELFTEST_EN` latches; omit
  `--patgen` (or re-arm without it) to capture real JC inputs.
- **ESP32-S3 will not flash:** flash over the CP2102 "UART" port with
  `idf.py -p COMx flash`; the S3 resets normally. The older DOIT classic board
  needs the manual BOOT/EN sequence plus `esptool --before no_reset`.
- **USB CDC port missing or TCP refused:** the native "USB" port enumerates
  only after the firmware boots (TinyUSB); until then the ROM USB-Serial-JTAG
  device shows there. Use the CP2102 port for the console. While the CDC port
  is open the gateway refuses TCP analyzer clients by design.
- **Live session stops with `EVT_OVERRUN`:** the offered event rate exceeded
  the link. Lower the input rate, use `--compact`, or accept the reported loss.
- **PulseView shows decoder-probe Python 3.4 warnings:** a bundled PulseView
  issue; `.sr` file viewing is unaffected.
- **Everything else:** `docs/limits.md` and `docs/status.md` record the known
  limits and the exact evidence behind them.
