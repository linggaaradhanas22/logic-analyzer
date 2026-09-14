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
- ESP32 DOIT DevKit V1 gateway, flashed with the project firmware, on the same
  **2.4 GHz** Wi-Fi network as the PC (the classic ESP32 has no 5 GHz radio).
- PC with the project `.venv` and, for viewing captures, PulseView
  (`build/pulseview`) or any VCD viewer.
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

**ESP32 gateway:**

```powershell
.\.venv\Scripts\python.exe tools\set_wifi.py   # once per network; prompts, never committed
python tools\run.py fw-esp32
```

Flash it. This DOIT board's auto-reset does not assert GPIO0, so hold **BOOT**,
tap **EN**, release **BOOT**, then flash with reset disabled and watch the
console:

```powershell
Set-Location build\esp32
python $env:IDF_PATH\components\esptool_py\esptool\esptool.py --chip esp32 -p COM5 -b 460800 --before no_reset --after hard_reset write_flash @flash_args
Set-Location ..\..
Set-Location fw-esp32
idf.py -B ..\build\esp32 -p COM5 monitor
```

Success signs: `station started`, `got ip`, `net CONNECTED`, `fifo_ovf=0`.
Exit the monitor with `Ctrl+]`.

## 3. Verify the link

```powershell
.\.venv\Scripts\la-cli.exe ping   -t tcp://la-esp32.local:5555
.\.venv\Scripts\la-cli.exe status -t tcp://la-esp32.local:5555
```

Expected: `pong: fw ... pl 0x4C410002 uptime ...` and a STATUS snapshot with
`state idle`. If the ping times out right after a flash or reset, wait ~15 s and
retry once: the first TCP session can need a listener refresh.

`la-cli ports` lists USB-UART candidates (CP210x/CH340/FTDI) when you use a
direct `serial://COMx` link instead of the gateway.

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
generator. **Omit it for real signals**, or the JC inputs are ignored. Modes:
`counter`, `walking`, `prbs`, `uart`, `spi`, `i2c`.

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

## 7. Browser live view

The gateway serves an embedded page on port 80 while a live session runs:

1. Start a live session (step 6) or `la-cli live -t ... --decoder ...`.
2. Open `http://la-esp32.local/` (or the IP from the ESP32 boot log).
3. The page shows per-decoder tabs with decoded bytes, sample deltas and a
   stats bar; it reconnects automatically if the session restarts.

No external assets or internet access are needed.

## 8. Task/queue trace

```powershell
.\.venv\Scripts\la-cli.exe trace -t tcp://la-esp32.local:5555 --start
# ... run the workload ...
.\.venv\Scripts\la-cli.exe trace -t tcp://la-esp32.local:5555 --dump `
    --out trace.txt --png trace.png --title "live UART"
```

The PNG is a relative-time timeline of task switches, queue/stream-buffer
activity and app markers. The dump is best-effort: a few `LOG` records can drop
under relay pressure.

## 9. SPI bulk transport (P9.1)

By default burst `DATA_*` frames travel over the UART. To route them over the
SPI slave (JD-7..10 + JA-1 wired), select it once per session:

```powershell
.\.venv\Scripts\la-cli.exe link -t tcp://la-esp32.local:5555 --spi --clock 18
.\.venv\Scripts\la-cli.exe capture -t tcp://la-esp32.local:5555 --rate 10M --words 64k --out big.sr
```

Commands and ACKs stay on the UART. Framed transfers are bench-clean at
5/10/18 MHz (18 MHz is the measured ceiling; 19/20 MHz lose chunks). The
sustained host drain is currently limited to ~0.28 MB/s by the ESP32 Wi-Fi
relay, so very large captures take a long time even though the SPI wire is
faster; see `docs/limits.md`.

## 10. Command reference

| Command | Purpose |
|---|---|
| `la-cli ports` | List USB-UART candidates. |
| `la-cli ping -t URI` | Check the link and firmware identity. |
| `la-cli status -t URI` | STATUS snapshot: state, counters, watermarks, latency. |
| `la-cli abort -t URI` | Abort a running capture. |
| `la-cli capture -t URI [options]` | Arm a burst capture and export `.sr`/`.vcd`/`.png`. |
| `la-cli live -t URI [options]` | Stream decoded UART/SPI/I²C events. |
| `la-cli trace -t URI [--start\|--stop\|--dump]` | Task/queue trace ring. |
| `la-cli link -t URI --spi\|--uart [--clock MHz]` | Select the burst transport. |

Target URIs: `tcp://la-esp32.local:5555` (gateway), `tcp://<ip>:5555`, or
`serial://COMx` (direct 3.3 V USB-UART adapter on JD-1..4 + GND, `[HUMAN]`).

## 11. Troubleshooting

- **Ping times out after a flash/reset:** wait ~15 s and retry once (listener
  refresh). Then check the ESP32 console for `net CONNECTED` and the Wi-Fi
  association.
- **No PONG at all:** confirm the PC and gateway are on the same 2.4 GHz
  network, the UART ribbon is on JD-1..4 + GND, and the Zynq firmware is
  running (`jtag-zynq` or SD boot).
- **First capture after `--patgen` looks wrong:** `SELFTEST_EN` latches; omit
  `--patgen` (or re-arm without it) to capture real JC inputs.
- **ESP32 will not flash:** manual BOOT/EN sequence plus
  `esptool --before no_reset`; the auto-reset does not assert GPIO0.
- **Live session stops with `EVT_OVERRUN`:** the offered event rate exceeded
  the link. Lower the input rate, use `--compact`, or accept the reported loss.
- **PulseView shows decoder-probe Python 3.4 warnings:** a bundled PulseView
  issue; `.sr` file viewing is unaffected.
- **Everything else:** `docs/limits.md` and `docs/status.md` record the known
  limits and the exact evidence behind them.
