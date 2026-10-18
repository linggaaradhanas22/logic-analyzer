# Using the Analyzer

This is the operator runbook for the current project state. It starts with an
unpowered Zybo Z7-10, brings up the default direct-Zynq USB profile, checks the
hardware through the unified browser UI, and then moves from internal self-test
signals to a real target.

Normal bench startup now uses the **W11 SD image, JP5 = SD**, verified on
18 October 2026. Development uses **JP5 = JTAG**, J12 for power/JTAG/console,
J10 for analyzer data and `python tools/run.py jtag-zynq`. See section 2 and
[current-handover.md](current-handover.md) for image identity and current state.
The ESP32 gateway is still supported, but it is optional and is not a supported
connection target for the unified UI. The current ESP32-S3 runs the combined
UART/SPI/I²C demonstration from `fw-esp32/demo-protocols/`; the single-UART
`fw-esp32/demo-uart/` build remains available as a simpler alternative.

All commands below run from the project root in PowerShell. Electrical details
are in `docs/wiring.md`, current limits are in `docs/limits.md`, and development
environment setup is in `docs/windows-dev.md`.

## 1. Before applying power

### Hardware checklist

| Item | Current direct-USB setting | Why |
|---|---|---|
| Zybo boot jumper | **JP5 = SD** for normal startup; **JP5 = JTAG** for development | SD runs the verified W11 image; JTAG is for `jtag-zynq` development loads. Power-cycle once after moving JP5. |
| Zybo power select | **JP6 = USB** | J12 supplies board power on the recorded bench setup. |
| USB host jumpers | **JP1 open, JP2 open** | Required for the documented J10 device-mode setup. |
| USB host connector | **J11 unused** | J10, not J11, is the analyzer device port. |
| Power/JTAG cable | PC to **J12** | Powers the board and carries JTAG plus the FTDI console. |
| Analyzer data cable | PC directly to **J10** | J10 connects the PS USB0 CDC-ACM device. Do not assume J10 powers the board. |
| Acquisition signals | Initially disconnected from **JC** | Prove the analyzer with its internal pattern generator first. |

The recorded board was a Zybo Z7-10 Rev D.0. Check the manual and schematic
for a different revision. Use data-capable USB cables and start with J10 in a
direct PC port rather than a hub.

### Electrical safety

- JC, JE and JD are **3.3 V LVCMOS only**. Never connect a 5 V signal directly.
- The FPGA inputs have no project-added over-voltage or over-current protection.
- Before probing a target, remove target power, connect a common ground first,
  then connect the signal leads. Do not connect the target VCC to the analyzer.
- Keep acquisition leads short. Confirm the target's idle voltage before use.
- Do not connect external signals for the first self-test below.

### PC checklist

- Use native Windows with the project `.venv` installed.
- Use Chrome or Edge for the UI.
- The UI package must be installed in the environment. If
  `.\.venv\Scripts\la-ui.exe` is missing, follow `docs/windows-dev.md` or run
  the editable install from that guide.
- Host-side UART/SPI/I2C capture decoding requires the native decoder DLL. Build
  and test it once with `python tools/run.py test-native`. Capture, viewing,
  export and device-side live decode do not require rebuilding it each session.

## 2. Power and load the current USB image

1. With power removed, set JP5 and JP6 as listed above.
2. Connect J12 to the PC and apply board power.
3. Connect J10 directly to a PC USB port. It may not enumerate as the analyzer
   until the USB firmware is running.
4. Load the current default USB profile:

```powershell
python tools\run.py jtag-zynq
```

`jtag-zynq` resets the PS, programs the PL, runs the FSBL, downloads the default
`la_app_usb` application and starts CPU0. A successful run writes
`build/reports/jtag-zynq.json`. The firmware is volatile: after removing board
power, run `jtag-zynq` again.

On a clean checkout, build the bitstream/XSA before the firmware. If the current
bitstream and XSA already exist and only firmware changed, skip the `bit` step:

```powershell
python tools\run.py bit          # required on a clean checkout
python tools\run.py fw-zynq      # default profile: direct USB
python tools\run.py jtag-zynq
```

The J12 FTDI console may show the boot banner and periodic status lines. Its COM
port is a debug console, **not** the analyzer data endpoint.

### SD boot (release image, JP5 = SD)

Latest USB/W11 image installed and cold-boot verified **2026-10-18**:

1. Build the image if needed: `python tools\run.py boot`.
2. Copy `build\release\BOOT.BIN` to the FAT32 root of the SD card.
3. With power removed, set **JP5 = SD** (JP6 = USB) and insert the card.
4. Power on; J10 enumerates as `1209:0001`. Continue with section 3.

The installed BOOT.BIN is **2,415,152 bytes**, SHA-256
`6cb120ccc53c573edf9188a82d2fe1622def58836ffa46fdc40224eca847b892`.
Expected firmware is **0x000A0000**, PL **0x4C410002**, UI **w11**. Card
read-back, cold identity, SD UI E2E 6/6 and external UART browser 10/10 passed;
reports/backups are in `build/reports/sd-w11/`. JTAG loading is not needed
after normal power cycling. The earlier September SD image is historical.

JTAG (above) remains the development path; the SD boot runs the same USB
profile and data endpoint, with no JTAG cable or `jtag-zynq` step. The gate
evidence is recorded in `docs/status.md` and `docs/current-handover.md`.

## 3. Find the analyzer COM port

After `jtag-zynq` completes, Windows should enumerate J10 as a **USB Serial
Device (COMx)**. The current development identity is VID:PID `1209:0001`.

Confirm it under **Device Manager > Ports (COM & LPT)**, or use the UI's Ports
list and **Rescan** in the next step. Choose the COM port with `1209:0001`. Do
not choose the J12 FTDI console (`0403:6010`). The COM number is assigned by
Windows and may not be COM13 on another PC or after moving cables. The legacy
`la-cli ports` candidate filter lists USB-UART adapter families and does not
currently include the project's `0x1209` direct-USB identity.

If the J10 device is absent:

1. Confirm `jtag-zynq` passed and the board is still powered from J12.
2. Confirm JP6 is set to USB and J10 uses a data-capable cable.
3. Unplug and reconnect J10, then rescan Device Manager or the UI Ports list.
4. Move J10 to a direct PC port before changing firmware.

## 4. Open and connect the UI

Start the server without claiming the COM port yet:

```powershell
.\.venv\Scripts\la-ui.exe
```

Open `http://127.0.0.1:8080/` in Chrome or Edge. The executable prints the same
URL but does not open the browser automatically.

No terminal needed: double-click the **LA UI** desktop shortcut (or
`tools\win\la-ui-start.vbs`). It starts the server hidden and opens the page;
a second double-click just reopens the page. Double-click **Stop LA UI** (or
`tools\win\la-ui-stop.vbs`) when finished. The launchers use the same default
port 8080 and the same `.venv`.

In the UI:

1. In **Ports**, select the `USB Serial Device` with `1209:0001`. Click
   **Rescan** if it appeared after the page loaded. Selecting it fills Target as
   `serial://COMx`.
2. Click **Connect**. The UI owns that COM port until **Disconnect** or Ctrl+C;
   do not run `la-cli` or `la-web` against the same port concurrently.
3. Confirm the link pill changes from `offline` to **`linked`**.
4. Confirm the header displays firmware and PL identities. The current checkout
    reports firmware `0x000A0000` and PL `0x4C410002` with the W11 live-waveform
    build. Older SD images may still report `0x00090000` (event-only live view).
5. Click **Ping**. A success toast includes the same IDs and a nonzero uptime.
6. Check the bottom bar before testing: `session idle`, `events 0`, `drops 0`,
   with no warning text.

**Stop here if Connect or Ping fails.** A capture made before the identity check
does not prove that the UI is talking to the intended device image.

**Reset UI** (top right) stops acquisition, cancels pending work, disconnects
the analyzer and clears in-memory captures, decoded/live history, trace view,
cursors, channel layout and this UI's saved browser settings. Forms return to
10 MS/s, 64k raw, no trigger, Pre 0, Patgen off and the default UART decoder
text; all eight channels are shown. Export captures you want to retain first.
Reset does not erase exported files or reflash/reset either board. Select the
analyzer port and Connect again afterward.

## 5. Run the first self-test

The internal pattern generator bypasses the JC pins, so this test needs no
external signal wiring.

In the **Capture** panel set:

| Control | Value |
|---|---|
| Rate | `100 MS/s` |
| Words | `4096` |
| Mode | `raw (4 samples/word)` |
| Pre | `0` |
| Trig rise / Trig fall | blank |
| Patgen | `counter` |
| Timeout | `30` |

Click **Capture**. A passing first capture has all of these signs:

- Capture status becomes `captured 4096 samples (raw)`.
- The viewer title shows 4096 samples at 100,000,000 Hz.
- Eight channel rows are present.
- CH0 toggles fastest, CH1 at half that rate, then CH2 and the higher counter
  bits progressively more slowly. The UI slows the step modes to 16 samples
  per step, so the default Fit view is a legible square-wave counter: CH0 has
  the most cycles and CH7 runs slowest (one cycle across a 4096-word capture).
- The bottom bar returns to `session idle`.
- No red capture-failure toast appears. A completed capture has passed the
  host's exact-size and whole-capture CRC checks; parser counters are not shown
  on the page.

If this test fails, leave the target disconnected and use the troubleshooting
section. Do not proceed to real-signal debugging with an unproven base path.

## 6. Learn the waveform controls

Use the counter capture to check the viewer before connecting a target:

New captures open at a readable time scale around the first visible signal
activity. **Focus activity** restores that view. **Fit** or double-click shows
the full record with continuous high/low rails; dense activity is represented
by a min/max band when exact samples cannot fit. Labels are suppressed rather
than overlapped at wide zoom. Neither control changes the recorded samples.

1. Scroll over the waveform to zoom around the pointer.
2. Drag the waveform to pan; double-click it or click **Fit** to show all data.
3. Select a channel, then use **edge left** / **edge right** to step through its
   transitions. Cursor A moves to the next or previous exact edge and the view
   scrolls when the edge is outside the current window; pressing again steps on
   from the cursor.
4. Click the ruler for cursor A and Shift+click for cursor B, or use the cursor
   buttons. The Channels panel reports delta time and reciprocal frequency.
5. Enter a `0/1/x` pattern and use **Find next** / **Find prev**. At a newly
   zoomed level, the first click may fetch detail and ask you to press again.
6. Rename, recolor, hide, reorder or resize channels as needed. These settings
   persist in browser storage.
7. Click **PNG** for a screenshot of the visible waveform.

## 7. Try protocol capture and decode

Protocol self-tests also use internal signals. Run one row at a time using
`10 MS/s`, `64k` words, raw mode, zero pre-trigger and no trigger.

| Try | Capture Patgen | Decoder slot text | Expected decoded result |
|---|---|---|---|
| UART | `uart` | `uart:rx=0,baud=115200` | Repeating `LA-TEST\n` BYTE events on CH0 |
| SPI | `spi` | `spi:sck=0,mosi=1,cs=2` | Repeating frames containing `4C 41`; the first can be partial |
| I2C | `i2c` | `i2c:scl=3,sda=4` | START, address/write, ACK, bytes `4C 41`, STOP |

For each test:

1. Set the Capture **Patgen** mode and click **Capture**.
2. Enter the matching decoder text in Slot 0.
3. Click **Decode capture**.
4. Confirm the decode status reports events and an annotation row appears under
   the waveform.
5. Zoom into the annotation row and compare the event bytes with the table.

Capture decoding uses the host native DLL. If it reports that the library is
missing, run `python tools/run.py test-native`, restart `la-ui`, reconnect and
decode again.

Additional pattern modes are available for focused tests:

| Mode | Signal and purpose |
|---|---|
| `counter`, `walking`, `prbs` | Eight-channel raw-pattern capture tests |
| `uart_text`, `uart_bin` | Printable text or a binary byte ramp on CH0 |
| `uart_ferr` | UART frame-error case on CH0 |
| `spi_multi`, `spi16` | Multi-byte or 16-bit SPI cases on CH0..CH2 |
| `i2c_read`, `i2c_nack`, `i2c_alt` | I2C read, NACK and alternate-address cases on CH3..CH4 |

The pattern generator emits one protocol at a time. A second decoder on other
channels needs an external signal source.

## 8. Try live decode

Live decode and burst capture are alternatives; they cannot run simultaneously.
Starting a capture stops a live session automatically. Before starting live
decode, finish or abort any capture and confirm `session idle`.

For a UART live self-test:

1. Enter `uart:rx=0,baud=115200` in Slot 0.
2. Click **Apply live config**. Slot 0 should become configured.
3. In **Pattern generator**, select `uart` and click **Arm**.
4. In the Slot 0 live pane, click **Start**.
5. Confirm real high/low traces and decoded annotations update together, `events`
   increases and the
   `client` count inside `drops ... (ring ..., client ...)` remains zero. The
   `ring` count may increase after the bounded replay history fills; that alone
   does not mean the active browser lost events.
6. Click Slot 0 **Stop**. Confirm the session returns to `idle`.

Up to three decoder slots can be configured. Each configured pane shows the
protocol's actual input channels, with decoded annotations on the same time
axis. Unconfigured panes are hidden. The W11 view requires USB firmware
`0x000A0000` or newer; if an older SD image is running, the UI reports this
instead of presenting decoded events as a waveform. The self-test still needs
the real JE-to-JC loopback for live input; remove that jumper before connecting
an external source to the same JC input.

Each pane has independent Start/Stop controls; **Stop all** ends acquisition.
**Pause view** freezes the displayed snapshot while acquisition keeps running;
**Follow** resumes following recent activity, including holding the latest
burst readable during idle gaps. Mouse-wheel zoom and drag pause the view for
inspection; **Fit history** shows the retained interval. The browser retains
at most two seconds or 16,384 actual runs and 10,000 annotations per slot.
Missing preview ranges are blank and reported as gaps/omissions, not joined
by invented edges. Preview is bounded and can be omitted under pressure while
the decoder continues; use burst capture/export for a saved waveform record.

The Capture Rate dropdown controls burst acquisition. Live sampling uses the
UI server startup rate (100 MS/s by default; `la-ui --rate 10M` selects 10 MS/s).
The live ruler uses the actual packet rate, not the Capture dropdown.

The unified UI accepts up to four browser tabs on the same live feed; each tab
receives the same events and waveform packets. A new live session clears stale
server replay and queued frames. If the counters rise but a pane stays blank,
reload the page, close excess tabs, and start the session again.

Qualified inputs are 115200-baud UART and 10 kHz SPI/I2C. A 1 MHz SPI event
stream is an intentional overload case and should report an overrun rather than
silently corrupting data.

## 9. Capture a real target

### Wire the target

1. Stop live mode, wait for `session idle`, and click **Disconnect**.
2. Remove target power before changing probe wiring.
3. Connect target ground to JC-5 or JC-11 first.
4. Connect the 3.3 V signal lines to the required channels:

| Zybo pin | UI channel | Zybo pin | UI channel |
|---|---:|---|---:|
| JC-1 | CH0 | JC-7 | CH4 |
| JC-2 | CH1 | JC-8 | CH5 |
| JC-3 | CH2 | JC-9 | CH6 |
| JC-4 | CH3 | JC-10 | CH7 |

5. Confirm no line exceeds 3.3 V, reconnect in the UI, and power the target.

### Choose capture settings

- Set **Patgen = off**. A non-off Capture Patgen selects the internal source and
  ignores real JC inputs.
- Start with `10 MS/s`, raw mode and a modest `64k` record. Increase to
  `100 MS/s` when edge timing requires it.
- Leave triggers blank for an immediate capture.
- Set **Trig rise** or **Trig fall** to a channel number `0..7` to wait for one
  edge. Set **Pre** to keep up to 32,768 samples before that edge.
- Use raw mode for exact uniformly sampled logic. Use txn mode for long,
  transition-sparse records.
- Captures above 1,000,000 samples are accepted with a viewer warning and are
  decimated for the overview.

Click **Capture**, then check the exact sample count and warnings before drawing
conclusions from the waveform. **Abort** cancels an armed or running capture.

### Decode the captured bus

Enter the real channel mapping, then click **Decode capture**:

- UART: `uart:rx=0,baud=115200`
- SPI: `spi:sck=0,mosi=1,miso=2,cs=3,cpol=0,cpha=0,word=8,lsb=0`
- I2C: `i2c:scl=0,sda=1`

Adjust channels and protocol options to match the target. Wrong baud, clock
polarity/phase, word size or channel mapping can produce plausible but incorrect
events, so verify settings against the target configuration.

## 10. Export and reopen work

The Export panel provides:

- `.sr`: exact-count sigrok/PulseView capture; preferred archival format.
- `.vcd`: standard transition waveform; it cannot recover a trailing constant
  run that was not represented by an edge.
- `.png`: current canvas view.
- events CSV: decoded annotations.
- samples CSV: expanded samples.

Use **Open .sr / .vcd / .csv** to inspect a saved file without a device. An
opened `.sr`, `.vcd` or samples CSV becomes a normal signal capture, so waveform
viewing and host-side decode remain available while disconnected. An events CSV
contains decoded annotations only; it has no signal waveform to view or
re-decode. Stop `la-ui` before opening the same analyzer port from another
program; offline files do not require the port.

## 11. Optional task/queue trace

The Trace panel is for firmware behavior rather than signal acquisition:

1. While idle, click **Start**.
2. Run the workload to observe.
3. Return to idle and click **Dump**.
4. Inspect the task lanes, queue/stream activity and application markers in the
   Trace timeline.

The trace is best-effort; a small number of LOG records can drop under pressure.
It is not a replacement for the acquisition waveform.

## 12. End the session safely

1. Stop each live decoder or click **Stop all**.
2. Wait for `session idle`; abort any unfinished capture.
3. Export anything that must survive the session-memory store.
4. Click **Disconnect**. This also clears all three latched decoder slots.
5. Stop the UI server with Ctrl+C so it releases port 8080 and the COM port.
6. Remove target power before removing signal leads; remove signal leads before
   removing the common ground.
7. Remove Zybo power when finished. The JTAG-loaded firmware will need to be
   loaded again after the next power-up.

## 13. Troubleshooting

| Symptom | Check and recovery |
|---|---|
| `la-ui.exe` is missing | Install the project into `.venv` as described in `docs/windows-dev.md`. |
| Browser cannot open port 8080 | Stop another `la-ui`/`la-web` process, or start with `--port N` and open that port. |
| No `1209:0001` COM port | Confirm J12 power, JP6=USB, successful `jtag-zynq`, direct J10 data cable; reconnect J10. |
| Only FTDI COM ports appear | Those are J12 debug/JTAG interfaces, not the J10 analyzer endpoint. |
| Connect says the port is busy | Close Device Manager's port view if applicable and stop every CLI/UI/terminal process using that COM port. |
| Link is `linked` but IDs differ | Reload the current default image with `python tools/run.py jtag-zynq` before testing. |
| First counter capture is flat | Confirm Capture Patgen is `counter`, triggers are blank and the image IDs are current. |
| Real target capture shows self-test data | Set Capture Patgen to `off` and capture again. |
| Capture remains armed | The selected edge did not occur; click Abort, verify the trigger channel and try immediate capture. |
| Decode reports a missing native library | Run `python tools/run.py test-native`, restart the server and reconnect. |
| Decode events are wrong | Check sample rate, channel mapping and protocol settings; verify the electrical idle levels. |
| Live reports `client` drops or device overrun | Lower the input event rate, use fewer active slots, or treat the run as an honest overload result. A rising `ring` count alone is normal replay-history rollover. |
| Live counters rise but pane is blank | Reload the page and close excess browser tabs (up to four are supported); press **Stop all**, then start live again. A stale tab can no longer hold the only feed after the W11 multi-tab fix. |
| UI says `server error` | Confirm that `la-ui` is still running in PowerShell. If it stopped, restart it; then reload the browser and reconnect the hardware. |

## 14. Scripted CLI equivalents

The same direct-USB path is available for automation. Replace COM13 with the
port identified on the current PC:

```powershell
.\.venv\Scripts\la-cli.exe ping -t serial://COM13
.\.venv\Scripts\la-cli.exe status -t serial://COM13
.\.venv\Scripts\la-cli.exe capture -t serial://COM13 `
    --rate 10M --words 4096 --mode raw --patgen counter --out selftest.sr
.\.venv\Scripts\la-cli.exe live -t serial://COM13 `
    --patgen uart --decoder uart:rx=0,baud=115200 --count 20
```

The UI and CLI cannot own the port together. Disconnect and stop one before
starting the other.

## 15. Optional ESP32 gateway profile

The ESP32-S3 gateway, Wi-Fi TCP transport and gateway web page remain supported
as an optional profile. They require different firmware, wiring and power:

- Build/load the Zynq with `--profile gateway`.
- Flash and power the ESP32-S3; keep both of its USB cables attached on the
  recorded bench setup.
- Wire JD-1..4 + GND for UART control and optionally JD-7..10 + JA-1 for SPI.
- Use `tcp://la-esp32.local:5555` with `la-cli` or use `la-web`.

The supported `la-ui` operating path is the direct-USB `serial://COMx` product
path; use `la-web` for the gateway. Gateway details and current SPI/Wi-Fi
limitations are in `docs/wiring.md`, `docs/spi-link.md` and `docs/limits.md`.

## 16. Current boundaries

- Direct Zynq USB is the default product transport and measured 1.62 MB/s over
  three byte-exact 64 MiB trials.
- The MVP is qualified at 100 MS/s; 200 MS/s is not implemented or proven.
- Pre-trigger depth is limited to 32,768 samples.
- The current USB development identity `1209:0001` is for in-house testing; the
  release USB identity remains an open gate.
- The current W11 USB-profile SD boot was verified on 2026-10-18; public USB
  identity and final release tag/publication remain open.
- The UI is supported in Chrome/Edge. The original walkthrough was
  author-confirmed on 12 October; the user accepted W11's new view on
  18 October. Automated SD/browser checks are separate 6/6 and 10/10 evidence,
  not an invented scored human walkthrough.
