# RTOS Logic Analyzer

A logic analyzer records when digital signals are high or low so you can debug
timing and communication. This project builds one from a Zybo Z7-10
(Zynq-7010), an ESP32 Wi-Fi gateway and a native-Windows Python host tool. Eight
3.3 V channels are sampled in the FPGA at up to 100 MS/s, captured around a
trigger into DDR through ordinary AXI DMA scatter-gather descriptors, relayed
over a CRC-protected link, and exported or live-decoded by the host.

The implementation source of truth is `docs/plan.md`. The execution ledger,
measured evidence, blockers and next actions are in `docs/status.md`.

## What exists today

| Area | State |
|---|---|
| FPGA capture pipeline (`fpga/rtl`, 12 RTL blocks) | Implemented; 12/12 self-checking Questa tests |
| Vivado integration, Zybo bitstream/XSA | Built and on bench; timing closed, DRC clean |
| Zynq FreeRTOS core (acquisition, commands, TX, status, live decoders) | Bench-verified burst and live paths |
| ESP32 gateway (UART + SPI bulk link, TCP relay, web live view) | Bench-verified; Wi-Fi drain is the open performance limit |
| Host `la-cli` (capture, `.sr`/VCD/PNG export, live decode, trace) | Host-tested; burst and live used on bench |
| Live web view on the ESP32 (HTTP + WebSocket) | Bench-verified (P10.1) |
| Release packaging (BOOT.BIN tooling, ESP32 bin set, Python wheel) | Tooling present; final release waits on the P9.1 transport port |

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
python tools/run.py fw-zynq       # Vitis firmware
python tools/run.py fw-esp32      # ESP-IDF gateway
python tools/run.py boot          # BOOT.BIN packaging
python tools/run.py test-e2e -- --target tcp://la-esp32.local:5555
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
| `docs/using-the-analyzer.md` | Operating guide: load, verify, capture, live decode, web view, trace |
| `docs/architecture.md` | Zynq/PL integration, constraints, resource use |
| `docs/rtos-design.md` | Tasks, queues, ownership, cache contract, measurements |
| `docs/protocol.md` | Frame format and every message payload |
| `docs/regmap.yaml` / `docs/regmap.md` | Normative register map and generated view |
| `docs/decoders.md` | UART/SPI/I²C transition-decoder specification |
| `docs/wiring.md` | JC/JE/JD pin maps, loopback wiring, power rules |
| `docs/spi-link.md` | P9.1 SPI bulk-link contract and measured results |
| [`docs/usb-migration-implementation-instructions.md`](docs/usb-migration-implementation-instructions.md) | Proposed direct-Zynq USB migration: agent work packets, decision gates and handoff; execution not started |
| [`docs/usb-link.md`](docs/usb-link.md) | Proposed USB CDC-ACM contract, verified prerequisites and remaining bench questions |
| `docs/test-plan.md` | Acceptance matrices per phase |
| `docs/limits.md` | Known limits and unverified gates |
| `docs/sigrok-bridge.md` | P10.2 SUMP bridge assessment and deferral |
| `docs/windows-dev.md` | Native Windows setup and troubleshooting |
| `docs/web-artifact-implementation-instructions.md` | Documentation-portal contract (`web-artifact/`) |

For bench operation start with `docs/using-the-analyzer.md`; the same guide is on
the portal at `/using`. Typical first commands:

```text
.venv\Scripts\la-cli.exe ping -t tcp://la-esp32.local:5555
.venv\Scripts\la-cli.exe capture -t tcp://la-esp32.local:5555 --rate 10M --words 64k --out capture.sr
.venv\Scripts\la-cli.exe live -t tcp://la-esp32.local:5555 --decoder uart:rx=0,baud=115200
```

## Known limits

See `docs/limits.md`. The headline items: 3.3 V inputs only; the released MVP
is 100 MS/s with the 200 MS/s stretch unproven; the ESP32 Wi-Fi relay caps the
host drain at ~0.28 MB/s today, so the P9.1 64 MB/1.5 MB/s gate is blocked on
the ESP32-S3 USB transport port; and Wi-Fi jitter can delay the isolated-byte
latency gate.

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
