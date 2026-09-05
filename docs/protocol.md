# Link Protocol Scaffold

Frames use `A5 | TYPE | SEQ | LEN(2, little-endian) | PAYLOAD | CRC16-CCITT`.
The CRC covers `TYPE` through the last payload byte. Payloads are limited to
1024 bytes. The C implementation is incremental, allocation-free and shared by
the Zynq, ESP32 and host builds.

The complete message table and live/burst semantics are specified in
`docs/plan.md` section 5. The Phase 0 native tests cover framing, CRC rejection
and resynchronization after malformed input.
