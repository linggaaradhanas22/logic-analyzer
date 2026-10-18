# Current execution handover — 18 October 2026

This is the current-state entry point. [status.md](status.md) is the execution
ledger; [plan.md](plan.md) remains the requirements authority. Earlier dated
phase records are history, not instructions to resume old USB bring-up work.

**Project status: Complete — 18 October 2026.** The delivered USB-first
instrument, external demo firmware, operating documentation, and Embedded/FPGA
class submissions are complete. The source publication record is at the top of
[status.md](status.md). Optional extensions remain documented as deferred work.

## Project location and class deliverables (18 October 2026)

The active repository moved to `C:\Users\lingg\Projects\Logic-Analyzer\Source_Code`.
The previous `KCCI_technical\Embedded\Logic-Analyzer` parent contains only an
inaccessible pytest temporary directory; the project itself is in the new path.
The current report is the LaTeX HardwareX-style class report in
`docs/hardwarex/latex/`; it compiles with the pinned Tectonic 0.17.0 workflow.
Two report/presentation/workbook variants were copied to the two class delivery
folders: Embedded (Lingga Aradhana Sahadewa only) and FPGA (Lingga Aradhana
Sahadewa, Alvin Hadar, Bagas Budhi Permana). The presentation has 38 slides and
both custom shows (23 FPGA / 26 Embedded): slide 4 shows the hardware photo,
slide 34 the real UART UI, and slide 35 the decoding close-up and video link.
Both report PDFs have 19 pages, with the video also linked from their page-2
specifications tables. Both workbooks contain the same four design/implementation
weekly logs and place Project Scoring Sheet last. Class folders contain the
matching report PDF, PPTX, presentation PDF, and XLSX; delivery hashes are in the
execution ledger. The demonstration is at
<https://youtu.be/1xx84Ndi4vQ?si=ldsuYEWvYVqu2u6->.

## Ready-to-use bench state

| Item | Current, verified state |
|---|---|
| Product transport | Direct Zybo J10 USB CDC-ACM; development VID:PID `1209:0001` |
| Boot | **JP5 = SD**, **JP6 = USB**; cold boot verified on 18 October, without JTAG loading |
| Power/data | J12 supplies power; J10 carries analyzer data; J11 unused |
| Firmware / PL | **`0x000A0000` / `0x4C410002`** |
| Host UI | **w11**, `http://127.0.0.1:8080/`, Chrome/Edge, up to **4 live browser tabs** |
| Bench COM ports | Zybo analyzer COM13; ESP32-S3 native USB Serial/JTAG COM10; enumerate again if cables/PC change |
| External stimulus | ESP32-S3 `fw-esp32/demo-protocols/`: UART GPIO17 -> JC-1, SPI GPIO12/11/10 -> JC-2/3/4, I²C GPIO9/8 -> JC-7/8, with GPIO9↔GPIO4 and GPIO8↔GPIO5 loopback; shared ground -> JC-5/11 |

The ESP32-S3 currently runs the **combined UART/SPI/I²C demonstration**, not the
optional gateway. Its firmware is persistent; the Zybo now also starts the W11
image from SD after normal power loss. Do not reload JTAG for normal SD
operation. For development, power off, select JP5=JTAG, power-cycle and use
`python tools/run.py jtag-zynq`.

## Combined protocol demo (verified 18 October 2026)

`fw-esp32/demo-protocols/` exercises three protocols from the same J1 header row
as GPIO17:

| Signal | ESP32-S3 | Zybo |
|---|---|---|
| UART 115200 8N1, `LA-S3\n` | GPIO17 (J1-10) | JC-1 / CH0 |
| SPI mode 0, 8-bit, 10 kHz, `4C 41` | SCK GPIO12 (J1-18), MOSI GPIO11 (J1-17), CS GPIO10 (J1-16) | JC-2/3/4, CH1/2/3 |
| I²C 10 kHz, address 0x50 | master SCL GPIO9 (J1-15), SDA GPIO8 (J1-12) | JC-7/8, CH4/5 |
| I²C slave loopback | GPIO9↔GPIO4 (J1-15↔4), GPIO8↔GPIO5 (J1-12↔5) | — |
| Ground | GND J1-22 or J3-1/21/22 | JC-5 or JC-11 |

Decoder texts:

```text
uart:rx=0,baud=115200
spi:sck=1,mosi=2,cs=3,cpol=0,cpha=0,word=8,lsb=0
i2c:scl=4,sda=5
```

A 10 MS/s, 256k raw capture contains UART `LA-S3\n`, SPI `4C 41` frames and I²C
write `4C 41` plus read `41 33`. Verified 18 October: capture/decode all three
slots; then **three live decoder slots started together** produced 2,322 UART +
3,100 SPI + 3,121 I²C events in four seconds with **zero ring/client/format
drops**, 1,937 real waveform packets and no preview omissions. Evidence:
`build/reports/demo-protocols/verification.json`.

Parallelism boundaries:

- A capture samples all eight channels together, and one capture can be decoded
  with up to three protocol slots. Our verification did that in one record.
- Up to three live decoder tasks run concurrently on the one Zynq acquisition
  stream. The demo proves UART+SPI+I²C simultaneously.
- **Capture and live decode cannot run at the same time.** Starting a capture
  stops live; starting live requires idle. They use different acquisition modes.
- Only one capture job runs at a time; there is no second independent capture
  engine. Host capture decode is not a real-time parallel capture.
- Event throughput remains shared. The demo uses conservative rates; raising
  protocol rates can produce reported client/ring loss under overload.

### Live view troubleshooting

The unified UI now supports up to four browser tabs on `/events`. Earlier, a
stale or second tab held the only SSE feed; the visible tab could show
`events`/`waveform packets` counters (updated through `/api/state` polling)
while every Live pane stayed blank. A fresh live start also clears stale replay
history and queued frames. If a pane still shows no waveform: reload the page,
close excess tabs, and start the live session again; the server accepts four
tabs. Evidence: two independent SSE readers each received 296 event messages and
925 waveform messages from the same live session
(`build/reports/demo-protocols/verify_two_feeds.py`).


## SD image and package identity

- Image: `build/release/BOOT.BIN`, USB profile, **2,415,152 bytes**.
- BOOT.BIN SHA-256:
  `6cb120ccc53c573edf9188a82d2fe1622def58836ffa46fdc40224eca847b892`.
- Packaged/loaded W11 ELF SHA-256:
  `f46389813150988c2104b2899a2f6dd1565a6f49572023d6f29ff4d515b55699`.
- The card was identified as the FAT32 removable D: volume on a 16 GB USB
  reader. Its previous BOOT.BIN matched the known September image. Only BOOT.BIN
  was replaced, with flushed temporary copy, atomic replacement and matching
  read-back hash. D: is no longer mounted after the card returned to the Zybo.
- Previous card image: `build/reports/sd-w11/card-previous-BOOT.BIN`; previous
  release set: `build/reports/sd-w11/baseline/`.
- Rebuild/package: `python tools/run.py boot`, then `python tools/run.py package`.
  `build/release/release-manifest.json` records current hashes, the host wheel
  and the **optional gateway** ESP32 images. Those gateway images are not the
  UART demonstration and must not be flashed for this demo.

## W11 behavior

1. Captures open at a readable scale near first visible activity. **Focus
   activity** restores it; **Fit** shows the entire record with connected digital
   rails or a min/max activity band. Dense byte labels are suppressed, not
   overlapped. No signal timing is stretched or invented.
2. Live panes draw **actual FPGA transition words** and Zynq-decoded annotations
   on the same sample clock. `WAVE_CFG` (`0x1C`) opts into `EVT_WAVEFORM`
   (`0x25`). Slots enabled mid-stream preserve the acquisition epoch. The
   protocol details are in [protocol.md](protocol.md).
3. **Pause view** freezes a bounded snapshot while acquisition continues;
   **Follow** resumes recent activity; Fit history/zoom/drag allow inspection.
   History is bounded to two seconds or 16,384 runs and 10,000 annotations per
   slot. Gaps/omissions are visible; this preview is not lossless recording.
4. **Reset UI** stops/cancels work, disconnects, clears in-memory captures,
   live/trace/job history, cursors and saved browser preferences, and restores
   default forms and all eight channels. It does not erase exports or reflash
   either board. Select the port and Connect afterward.
5. Capture Rate controls burst sampling; live sampling uses the server startup
   rate (100 MS/s by default, `la-ui --rate 10M` for 10 MS/s).

Demo capture: 10 MS/s, 64k, raw, Pre 0, blank edge triggers, Patgen off, Slot 0
`uart:rx=0,baud=115200`. Expected bytes: `4C 41 2D 53 33 0A`.

## Evidence and boundaries

| Check | Latest evidence |
|---|---|
| Native protocol/decoder/ownership tests | 18/18 PASS, final source closeout, 18 October |
| Host tests | 180/180 PASS, final source closeout, 18 October; existing matplotlib deprecation warnings |
| JavaScript/page model | Syntax and PAGE CHECK OK, including exact runs, gaps, frozen pause and idle history bounds |
| Firmware build / JTAG development load | PASS, 17 October; not used for the 18 October cold boot |
| SD image creation, package, card read-back | PASS, 18 October |
| SD firmware identity | `0x000A0000` / `0x4C410002`, fresh uptime after user-confirmed cold power-cycle |
| SD USB UI E2E | **6/6 PASS**, 11.35 s, 18 October |
| SD browser + external ESP32 UART | **10/10 PASS**, zero page errors; activity/Fit, real waveform + bytes, status, Pause/Follow, late-slot alignment, Reset during streaming and armed capture, reconnect/Ping |
| UART+SPI+I²C capture and parallel live | PASS 18 Oct 2026: 256k capture decoded on all three slots; I²C ACK/address/data; 4 s live with 3 slots = 8,543 events, zero ring/client/format drops, 1,937 waveform packets. Report: `build/reports/demo-protocols/verification.json` |
| Multi-tab live feed fix | PASS 18 Oct 2026: fresh live start clears stale replay/queues; up to 4 UI tabs; two SSE readers each received 296 event + 925 waveform messages from the same session. `verify_two_feeds.py`; host suite 180/180 |
| Documentation/portal sync | Markdown links OK; portal validate PASS (7 subsystems, 79 components, 26 messages, 186 coverage); production build PASS; targeted lint/format PASS. Source not deployed |

Current SD reports/screenshots are under `build/reports/sd-w11/`, especially
`sd-boot-verification.json`, `card-installation.json` and `browser-checks.json`.
Earlier JTAG W11 evidence is under `build/reports/ui-waveform/`. Ignored reports
are machine-local evidence; missing reports on another checkout mean not-run.

No new external SPI/I2C-preview or overload qualification is claimed. The
historical 1.62 MB/s drain, 60-second live and other P0-P10 measurements keep
their original dates; they were not all rerun on W11. The original visual
walkthrough was author-confirmed on 12 October; the user accepted the new W11
view on 18 October, separately from the automated checks above.

## Repository/publication handoff

- The user authorized committing all pending source and pushing `origin/main`.
  The top of the execution ledger records the final commit and remote checks;
  older WIP statements are historical.
- The portal source is included in the closeout. Website deployment is separate
  and has not been performed by this source publication.
- Compiled class deliverables remain in their delivery folders. Generated
  environments, captures, logs, build reports and release binaries stay ignored;
  the editable sources and hardware photograph are included in Git.
- Public USB identity and a tagged product release are deferred. Optional
  gateway S3 SPI corruption, SUMP integration and 200 MS/s remain documented
  limitations or extensions, not completed measurements.
- No implementation task remains for the delivered class project. For future
  maintenance, read this file, AGENTS.md, plan/status and `git status`; verify
  SD firmware identity before bench work, then follow the user's new request.
  Do not resume U03 enumeration, assume the ESP32 is a gateway, or overwrite the
  SD card with the previous `0x00090000` image.
