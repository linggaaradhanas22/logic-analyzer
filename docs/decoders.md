# Transition Decoder Specification

The decoders consume caller-owned, allocation-free transitions:
`(t_sample:u64, sample:u8)`. `sample` contains the eight logic channels after
the acquisition sample at `t_sample`; a transition is emitted only when the
sample value changes. Timestamps are nondecreasing. Equal timestamps are
processed in call order and are useful when a source records simultaneous
channel changes as separate transitions. A timestamp decrease is an error.

The callback receives an event with a numeric kind, channel, flags, timestamp,
and up to 16 data bytes. `channel=0xFF` means no single channel applies. BYTE
data is little-endian for 16-bit words. Decoder state is never allocated by the
library and must not be shared between tasks without an owner or lock.

## Common Rules

- The first transition establishes the initial line levels and does not infer a protocol edge.
- `mark_discontinuity(t)` resets protocol state, records `t` as the last accepted timestamp and emits DISCONTINUITY.
- A segment sequence gap must call `mark_discontinuity` before feeding the next segment.
- Callback return values are not used for ownership; the caller must copy the event or enqueue an owned event block before returning.
- `advance(t)` on UART supplies unchanged samples through `t`; it is the heartbeat mechanism for idle runs and must be called at segment boundaries.
- Decoder event timestamps describe the start of a UART frame, the active SPI sampling edge, the eighth I2C data edge, or the relevant I2C control edge as specified below.

Event kinds are BYTE, FRAME_ERROR, PARITY_ERROR, START, STOP, DISCONTINUITY,
ADDRESS, ACK, NACK, CS_ASSERT and CS_RELEASE. SPI BYTE flags are MOSI or MISO.
I2C ADDRESS has the READ flag when the address byte R/W bit is one.

## UART

Configuration is RX channel 0 through 7, samples per bit, data bits 5 through
9, parity none/even/odd, one or two stop bits, and optional inversion. The
line is idle at logical one. The decoder detects a logical falling edge, checks
the start bit at half a bit, samples data at half-bit offsets, then samples
parity and each stop bit at one-bit intervals. Data is least-significant bit
first. BYTE data is one byte for 5 through 8 data bits and two little-endian
bytes for 9 data bits.

For even parity, the parity bit makes the total number of ones even. For odd
parity it makes the total odd. A parity mismatch emits PARITY_ERROR at the
frame-start timestamp and still emits the BYTE. A bad start or stop level
emits FRAME_ERROR and discards the partial frame. A long unchanged idle run is
not a byte; the caller's `advance` heartbeat only lets a pending frame finish.

The input configuration normally derives samples per bit from the configured
sample rate and baud. It must be rounded once by the owner and remain constant
for a decoder instance. The decoder does not interpolate between transition
timestamps.

## SPI

Configuration contains required SCK, optional MOSI/MISO and optional CS
channels, CPOL, CPHA, 8- or 16-bit words, bit order and CS active level. A CS
assertion starts a word stream and emits CS_ASSERT. A CS release emits
CS_RELEASE; an incomplete word before release also emits FRAME_ERROR.

For CPHA=0, data is sampled on the leading edge (the transition away from
CPOL). For CPHA=1, data is sampled on the trailing edge (the transition back
to CPOL). MOSI and MISO are sampled from the same post-edge sample. MSB-first
words shift toward bit zero; LSB-first words place the first bit at bit zero.
Each completed data lane emits a BYTE event at the final sampling edge, with
one or two little-endian data bytes and the corresponding MOSI or MISO flag.

When CS is absent, the decoder treats the bus as permanently selected. A
clock edge coincident with CS assertion or release is a control edge, not a
data edge. Clock stretching is not a SPI concept; no duration assumption is
made, only edge order matters.

## I2C

Configuration requires distinct SCL and SDA channels. Both lines are initially
high in a valid idle bus. START is SDA falling while SCL remains high; STOP is
SDA rising while SCL remains high. A repeated START emits START and begins a
new address byte without requiring STOP.

Bits are sampled on SCL rising edges, most significant bit first. After eight
data bits, the ninth rising edge is the acknowledge bit: SDA low emits ACK and
SDA high emits NACK. The first byte after START emits ADDRESS with the seven-bit
address in data[0] and READ set from its R/W bit. Later bytes emit BYTE at the
eighth data edge. ACK/NACK is timestamped at its ninth edge. Clock stretching
is handled implicitly because the decoder reacts only to observed SCL edges.

An unexpected control transition does not invent a byte. A sequence gap resets
the active transaction and requires a new START before ADDRESS or BYTE events
are accepted.

## Native Acceptance

`tools/models/decoders.py` is an independent stimulus/reference model. Native
CTest drives the same vectors through the C ABI and compares event kind,
timestamp, channel, flags and data. The model tests include UART parity and
inversion, SPI CPOL/CPHA and bit order, I2C address/ACK/NACK/STOP, and explicit
discontinuity/reset behavior. These tests establish functional agreement but
do not establish FreeRTOS scheduling, A9 throughput, interrupt latency or
electrical signal integrity.
