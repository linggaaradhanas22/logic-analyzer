# Known Limits

This file collects the project's current limits and unverified gates in one
place. Each item names the evidence or the gate that would retire it; the
authoritative execution ledger is `docs/status.md`.

## Electrical and input

- **3.3 V only.** JC/JE/JD are LVCMOS33. Never connect a 5 V source without an
  appropriate level shifter. The FPGA pads provide no over-voltage or
  over-current protection; keep acquisition leads short and check idle levels
  before enabling a pattern output (`docs/wiring.md`).
- **Continuity and signal integrity are [HUMAN] gates.** Board revision, header
  pin numbering, common ground, RTS/CTS direction and no-5 V exposure are
  checked by a person before programming or HIL runs.
- **Asynchronous inputs.** `la_input_sync` reduces metastability risk with a
  two-flop synchronizer (optional 2-of-3 majority filter). It does not
  guarantee capture of pulses shorter than the sampling period or protect the
  pins electrically.

## Sampling and capture

- **The MVP is qualified at 100 MS/s.** The 200 MS/s stretch (P9.3) is not
  started: IDDR/200 MHz clocking, CDC, two-sample packing, JC bank voltage and
  IO-standard checks, timing and signal integrity must all pass first. Keep
  the released MVP at 100 MS/s unless that work is proven.
- **Pre-trigger depth is clamped to 32,768 samples** in the register map
  (`PRE_SAMPLES`), even though the internal ring holds 65,536 entries.
- **Burst capacity is bounded** by the capture pool (8 MiB) and the finite
  descriptor chain; an oversized request is rejected with a capacity NAK, not
  silently truncated.
- **Trigger semantics:** a level mask with an empty edge mask means "level
  true"; a trigger is the first qualifying sample after arming. A zero
  pre-trigger burst is emitted from the trigger sample forward.

## Link and transport rates

- **UART control/burst link: 2 Mbps** (8N1, RTS/CTS). Live event forwarding of
  the ~120 kB/s 115200 UART stream is clean in the recorded 60 s gates, but
  Wi-Fi stalls have dropped 4–19 % of relay frames in some runs; drops are now
  reported as `EVT_OVERRUN(stage=2)` instead of corrupting the stream.
- **SPI bulk link: 18 MHz measured ceiling** on the current wiring. The wire
  delivers ~1.25 MB/s at 10 MHz and ~2.25 MB/s at 18 MHz; 19/20 MHz lose
  chunks to 256-byte slave FIFO underruns. The 200 Ω series-resistor SCK/MISO
  edge characterization with a scope is still open. **With the ESP32-S3 gateway
  the link is currently open**: the S3 receives every other payload byte even
  though its SPI master passes the loopback self-test and the Zynq slave was
  bit-exact with the classic ESP32 (`docs/status.md`, 2026-09-17).
- **Host drain via the ESP32 Wi-Fi relay: ~0.28 MB/s measured.** The 2026-09-17
  ESP32-S3 port adds a USB CDC-ACM host leg (`la_usb`, native USB port,
  `serial://COMx`). The S3 USB-OTG is **full-speed only** (12 Mbps), so its
  bulk ceiling is about 1.2 MB/s: the drain is measured against that physical
  ceiling, and the plan's `>=1.5 MB/s over 64 MB` target cannot be met over
  this transport. The SPI wire is not the bottleneck.
- **S3 USB caveats (bench 2026-09-17).** The native-USB cable must go
  **directly into a PC port**: behind a USB hub every usbser.sys CDC device
  (the gateway's and the ESP32 ROM's) fails to open with `GetCommState` hangs
  or errors 22/31. Keep **both** USB cables connected: with only one cable the
  S3 browns out (`E BOD`) under Wi-Fi transmit peaks.
- **Gateway loopback echo: ~40–44 kB/s round trip**, unchanged from 921600 to
  2 Mbps. This is a relay-processing measurement, not a universal one-way
  limit or a profiled root cause.
- **Live decode rates qualified at 10 kHz SPI/I²C and 115200 UART.** 1 MHz SPI
  is a deliberate overload test: the host receives stage-1 loss reports and
  the system returns to IDLE. Decoder throughput with the cached live pool is
  ~1.29 M words/s (about 28 % above non-cached).
- **Isolated-byte latency:** the 25 ms p99 budget measured 428 us worst case
  over 20 single-shot sessions; one 92 ms outlier was observed in a full-suite
  run under weak RSSI, so the gate passes in isolation but Wi-Fi jitter
  remains a risk (P9.4).
- **Trace dumps are best-effort:** a few `LOG` records can drop under relay
  pressure; the host tolerates ≤2 % in-flight loss and the gateway stays
  healthy.

## Decoding and host

- **Decoders are transition-based.** They consume `(t_sample, sample)` tuples
  and emit events; they do not re-sample analog behavior. A segment
  discontinuity resets decoder state and emits a discontinuity marker.
- **`SAMPLE_DIV`/encoding changes and live configuration** are latched on the
  enable edge; a live session needs a fresh enable after `LIVE_STOP`.
- **Host-side reconstruction** expands transition runs into samples for `.sr`;
  PNG export caps the rendered sample count. PulseView 0.4.2's bundled Python
  decoder probe emits Python 3.4 syntax warnings; file export is unaffected.
- **sigrok/SUMP bridge (P10.2) is deferred** because no signed virtual-COM
  pair driver is available; see `docs/sigrok-bridge.md`.

## Tooling and environment

- Native Windows only. WSL, Linux containers, Make, Verilator and ESP-IDF
  Linux-host workflows are not part of this repository.
- Questa FPGA Starter Edition quirks are documented in `docs/test-plan.md`;
  the regression runner has no per-test Python vector hook.
- Vitis path-length constraints force short generated project names; crashed
  runs can orphan `vitis-server` java processes that hold file locks.
- The DOIT ESP32 board's auto-reset does not assert GPIO0: every flash needs
  the manual BOOT/EN sequence plus esptool `--before no_reset`. The ESP32-S3
  DevKitC-1 resets normally, so `idf.py -p COMx flash` works; flash over the
  CP2102 "UART" port and keep the console there (the native "USB" port carries
  the CDC host transport, and its USB-Serial-JTAG console is disabled in the
  firmware configuration).
- Wi-Fi credentials live only in the ignored `build/esp32/sdkconfig`; a fresh
  checkout must set them with `tools/set_wifi.py` before network gates.
- `build/reports` and release artifacts are ignored: on another checkout a
  missing report means not-run, not passed.
