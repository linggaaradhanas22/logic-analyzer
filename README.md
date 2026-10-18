# RTOS Logic Analyzer

A logic analyzer records when digital signals are high or low so you can debug
timing and communication. This project builds one from a Zybo Z7-10
(Zynq-7010), an optional ESP32 Wi-Fi gateway and a native-Windows Python host
tool. Eight 3.3 V channels are sampled in the FPGA at up to 100 MS/s, captured
around a trigger into DDR through ordinary AXI DMA scatter-gather descriptors,
and transferred over a CRC-protected link for export or live decoding by the
host. Direct Zynq USB is the default host transport; the ESP32 Wi-Fi gateway is
a supported optional profile.

The implementation source of truth is `docs/plan.md`. The execution ledger,
measured evidence, blockers and next actions are in `docs/status.md`.

**Project status: Complete — 18 October 2026.** The USB-first logic analyzer and
both class deliverable sets are finished. Source publication and final check
results are recorded in [docs/status.md](docs/status.md).

**Current handover:** [docs/current-handover.md](docs/current-handover.md).
W11 firmware **0x000A0000**, UI **w11** and the updated SD image are verified:
JP5=SD cold boot, real live waveforms + aligned decoding, readable captured
activity and Reset UI. The ESP32-S3 combined demo supplies external UART, SPI,
and I²C stimulus. Optional gateway tuning, the 200 MS/s extension, public USB
identity, website deployment, and a tagged product release are deferred beyond
the completed class-project scope.

## What exists today

| Area | State |
|---|---|
| FPGA capture pipeline (`fpga/rtl`, 12 RTL blocks) | Implemented; 12/12 self-checking Questa tests |
| Vivado integration, Zybo bitstream/XSA | Built and on bench; timing closed, DRC clean |
| Zynq FreeRTOS core (acquisition, commands, TX, status, live decoders) | Bench-verified burst and live paths |
| ESP32 gateway (UART + SPI bulk link, TCP relay, web live view) | Optional supported profile (`--profile gateway`); bench-verified; Wi-Fi drain is its open performance limit |
| Host `la-cli` (capture, `.sr`/VCD/PNG export, live decode, trace) | Host-tested; burst and live used on bench |
| Direct Zynq USB transport (CDC-ACM, 1.62 MB/s measured) | Default product transport since the 2026-09-20 V00 pivot |
| Live UI | W11 real FPGA waveform + aligned decode, Pause/Follow and Reset; external ESP32 UART browser checks 10/10 on SD (2026-10-18); legacy event-only page retained |
| External stimulus demos | ESP32-S3 combined UART/SPI/I²C firmware (`demo-protocols`) verified on the bench; single-UART `demo-uart` retained as a simpler alternative |
| Release packaging (BOOT.BIN, optional gateway bin set, Python wheel) | W11 image installed/read-back verified and cold-booted from SD (2026-10-18); public USB identity and tagged product release remain deferred |

## Specifications

- Eight LVCMOS33 acquisition channels on Zybo JC (3.3 V only; never 5 V).
- Sample rate `100 MHz / (SAMPLE_DIV + 1)`; 100 MS/s is the design target and
  the current MVP is qualified at that clock.
- Level and edge trigger, 32,768 maximum pre-trigger samples, post-trigger
  count, timeout and abort.
- Raw packing (four samples per word) or transition encoding
  (`{run[23:0], sample[7:0]}`).
- UART, SPI and I²C live decoders (shared C library, host-tested and wired to
  the target decoder tasks).
- Framed link protocol with CRC16 frames, CRC32 whole-capture checks,
  sequence/ACK windowing and explicit overrun reporting.
- Host exports `.sr` (PulseView), VCD, PNG, text and CSV.

## Canonical commands

Run from the project root; `tools/run.py` resolves paths from its own location.

```text
python tools/run.py check-env -- --write-lock
python tools/run.py gen
python tools/run.py test-native
python tools/run.py test-esp32-mocks
python tools/run.py test-host
python tools/run.py lint          # run lint and sim sequentially
python tools/run.py sim
python tools/run.py bit           # Vivado bitstream/XSA
python tools/run.py fw-zynq       # Vitis firmware (USB profile; --profile gateway)
python tools/run.py fw-esp32      # ESP-IDF gateway (optional profile)
python tools/run.py demo-esp32-uart # standalone external S3 UART source
python tools/run.py demo-esp32-protocols # S3 UART+SPI+I2C demo (J1-header pins)
python tools/run.py boot          # BOOT.BIN packaging (USB profile; --profile gateway)
python tools/run.py package       # wheel + optional ESP32 set (--profile gateway)
python tools/run.py test-e2e -- --target serial://COM13
python tools/run.py release
```

`program`, `flash-esp` and `hil` are human-operated hardware gates. Reports are
written under ignored `build/reports`; a missing report on another checkout is
not-run, not pass.

## Documentation map

| Document | Contents |
|---|---|
| `docs/plan.md` | Normative implementation plan and design requirements |
| `docs/status.md` | Execution ledger: phase state, verified gates, blockers |
| `docs/current-handover.md` | Current SD image, bench wiring, W11 features, evidence and publication state |
| `docs/using-the-analyzer.md` | Operating guide: load, verify, capture, live decode, web view, trace |
| `docs/architecture.md` | Zynq/PL integration, constraints, resource use |
| `docs/rtos-design.md` | Tasks, queues, ownership, cache contract, measurements |
| `docs/protocol.md` | Frame format and every message payload |
| `docs/regmap.yaml` / `docs/regmap.md` | Normative register map and generated view |
| `docs/decoders.md` | UART/SPI/I²C transition-decoder specification |
| `docs/wiring.md` | JC/JE/JD pin maps, loopback wiring, power rules |
| `docs/spi-link.md` | P9.1 SPI bulk-link contract and measured results |
| `docs/live-web.md` | Host live-web bridge contract (SSE, bounded replay ring); legacy `la-web` is single-client, unified `la-ui` supports four tabs |
| [`docs/unified-ui-plan.md`](docs/unified-ui-plan.md) | W00-W11 unified UI package: one browser workspace for capture, waveform viewing, live decode, exports and trace |
| [`docs/usb-migration-implementation-instructions.md`](docs/usb-migration-implementation-instructions.md) | Historical U00-U09 execution package; USB is implemented/adopted, with current continuation linked at the top |
| [`docs/usb-link.md`](docs/usb-link.md) | Implemented USB CDC-ACM contract and dated integration evidence |
| `docs/test-plan.md` | Acceptance matrices per phase |
| `docs/limits.md` | Known limits and unverified gates |
| `docs/sigrok-bridge.md` | P10.2 SUMP bridge assessment and deferral |
| `docs/windows-dev.md` | Native Windows setup and troubleshooting |
| `docs/web-artifact-implementation-instructions.md` | Documentation-portal contract (`web-artifact/`) |

For bench operation start with `docs/using-the-analyzer.md`; the same guide is on
the portal at `/using`. Typical first commands:

```text
.venv\Scripts\la-cli.exe ping -t serial://COM13
.venv\Scripts\la-cli.exe capture -t serial://COM13 --rate 10M --words 64k --out capture.sr
.venv\Scripts\la-cli.exe live -t serial://COM13 --decoder uart:rx=0,baud=115200
.venv\Scripts\la-web.exe -t serial://COM13 --decoder uart:rx=0,baud=115200
.venv\Scripts\la-ui.exe -t serial://COM13
```

`la-web` (or `la-cli live --serve`) serves the legacy host live view on
`http://127.0.0.1:8080/`: each decoder pane has its own **Start**/**Stop**
(up to three decoders via repeated `--decoder`, one per slot), the header has
**Stop all**, and the analyzer stays connected between sessions. The gateway
profile uses `tcp://la-esp32.local:5555` with the same commands.

`la-ui` serves the unified workspace on the same default address: connect,
burst capture with the waveform viewer (zoom/pan, cursors, measurements,
channel layout, trigger and pre/post markers, edge and pattern search),
host-side capture decoding with annotation rows, real live digital waveforms
with aligned decoded events and per-slot/Pause/Follow controls, Reset UI,
exports (`.sr`/`.vcd`/CSV/PNG),
offline open of saved captures, the trace timeline and the pattern generator.
The legacy page stays available at `/legacy` on the same server; see
`docs/using-the-analyzer.md` section 7 and `docs/unified-ui-plan.md`.
On Windows, the **LA UI** desktop shortcut (or `tools\win\la-ui-start.vbs`)
starts it hidden and opens the browser; **Stop LA UI** ends it.

## Known limits

See `docs/limits.md`. The headline items: 3.3 V inputs only; the released MVP
is 100 MS/s with the 200 MS/s stretch unproven; the default direct-USB transport
drains 1.62 MB/s measured, while the optional ESP32 Wi-Fi relay caps its host
drain at ~0.28 MB/s and keeps the P9.1 64 MB/1.5 MB/s gate open; and Wi-Fi
jitter can delay the isolated-byte latency gate on the gateway profile.

## Workflow rules

- Native Windows only; no WSL, Linux containers, Make or Verilator fallback.
- Edit `docs/regmap.yaml`, then `python tools/run.py gen`; never hand-edit
  generated register files.
- Every RTL block has a self-checking Questa test; protocol and decoder
  changes require `test-native`.
- ISRs only notify or use ISR-safe sends; buffers move by handle with
  documented ownership and queue full policies.
- AXI DMA uses ordinary scatter-gather descriptors only; cyclic mode and
  simple-transfer calls are not permitted.
- Hardware and licensed-tool steps that cannot run locally are reported as
  blocked or unverified, never as passing.
- Update `docs/status.md` with every tracked change; commit and push to
  `origin/main` as described there.
