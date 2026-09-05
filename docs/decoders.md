# Decoder Scaffold

Decoder input is a stream of `(t_sample: u64, sample: u8)` transitions. The
portable C library currently provides the Phase 0 UART smoke path used by the
native harness; SPI and I2C implementation is scheduled for P1.

All decoder state is caller-owned, allocation-free and emits through a callback.
Segment sequence gaps must reset state and report a discontinuity before later
events are accepted.
