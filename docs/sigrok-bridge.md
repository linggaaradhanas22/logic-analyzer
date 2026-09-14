# P10.2 sigrok / SUMP bridge assessment

Status: **deferred by the plan's own condition (2026-09-14).** This document
records the assessment required before the optional bridge can be implemented,
so the decision is explicit rather than silently absent.

## What the bridge would do

The plan's P10.2 option is a native Windows bridge that makes PulseView treat
the analyzer as a serial OLS-compatible device:

1. PulseView opens a virtual serial port and speaks the SUMP/OLS protocol
   (reset, ID, metadata, run, trigger configuration, then sample reads).
2. The bridge translates those requests into analyzer TCP transactions
   (`CONFIG`/`ARM`/capture drain) against the ESP32 gateway.
3. The bridge returns SUMP sample blocks to PulseView for waveform display.

It would cover **SUMP capture only**. It cannot stream the project's custom
decoded events (`EVT_DECODED`) into PulseView, and `.sr`/VCD file export
remains the required integration either way.

## The gate the plan sets

> "If a currently compatible signed virtual-COM driver is available, use a COM
> pair plus TCP bridge; verify on the actual Windows/Secure Boot setup without
> changing security settings. Otherwise defer this optional feature. No Unix
> PTY or WSL fallback."

## Assessment (this machine, 2026-09-14)

- No virtual COM-pair driver is installed. A `Win32_PnPSignedDriver` query
  finds only Microsoft Bluetooth serial ports, the ESP32 CP210x USB-UART and
  other unrelated devices; there is no com0com, Eltima, HHD or equivalent
  pair driver in `Program Files` or `System32\drivers`.
- The usual open-source option, com0com, does not ship a WHQL-signed driver in
  its default distribution. Installing it (or enabling test signing) would
  change the machine's driver/security state, which the plan explicitly
  forbids for this optional feature.
- Secure Boot state could not be read from the unelevated session
  (`Confirm-SecureBootUEFI` returned access denied), so even the unsigned
  fallback could not be evaluated without an elevated change.
- `host/la_sigrok_bridge/` contains only a placeholder package; no partial
  bridge code exists.

Conclusion: the plan's precondition is not met, so P10.2 is **deferred**. No
security setting was changed and no driver was installed.

## Revisit conditions

Implement the bridge only when all of these hold:

1. A signed, currently compatible COM-pair driver is installed or supplied by
   the user, or the user explicitly authorizes a driver/security change.
2. PulseView 0.4.2 (already pinned under `build/pulseview`) can open the pair
   and issue SUMP requests.
3. The ESP32 gateway and Zynq firmware are on the bench, because the bridge's
   only value is against real hardware.

The implementation would live in `host/la_sigrok_bridge/`, reuse
`host/la_cli/protocol.py` and the analyzer client for TCP transactions, and add
host tests with a mock SUMP client. It must not add a Unix PTY or WSL path.
