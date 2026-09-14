import type { DecoderDoc } from './types';

export const DECODERS: DecoderDoc[] = [
  {
    slug: 'uart',
    name: 'UART decoder',
    configuration: [
      'RX channel 0 through 7.',
      'Samples per bit, derived from the configured sample rate and baud; it must be rounded once by the owner and stay constant for the decoder instance.',
      'Data bits 5 through 9.',
      'Parity none, even or odd.',
      'One or two stop bits.',
      'Optional inversion of the line level.',
    ],
    behavior: [
      'The line is idle at logical one; the decoder detects a logical falling edge as the start bit.',
      'The start bit is checked at half a bit, data is sampled at half-bit offsets least-significant bit first, then parity and each stop bit are sampled at one-bit intervals.',
      'The decoder does not interpolate between transition timestamps; it only reacts to the transitions and to advance(t) heartbeats.',
      'advance(t) supplies unchanged samples through t; it is the heartbeat for idle runs, must be called at segment boundaries, and only lets a pending frame finish.',
      'A long unchanged idle run is not a byte.',
      'BYTE data is one byte for 5 through 8 data bits and two little-endian bytes for 9 data bits.',
    ],
    events: [
      'BYTE at the frame-start timestamp with the decoded value.',
      'PARITY_ERROR at the frame-start timestamp when the parity bit does not match; the BYTE is still emitted.',
      'FRAME_ERROR for a bad start or stop level; the partial frame is discarded.',
      'DISCONTINUITY emitted by mark_discontinuity(t) when protocol state is reset.',
    ],
    errors: [
      'A parity mismatch emits PARITY_ERROR and still emits the BYTE (even parity makes the total number of ones even, odd parity makes it odd).',
      'A bad start or stop level emits FRAME_ERROR and discards the partial frame.',
      'A timestamp decrease is an error: feed_transition returns a negative value.',
      'The decoder never invents a byte from an idle run and never interpolates between transitions.',
    ],
    tests: [
      'test_uart_decoder (tests/native/native_tests.c, CTest la_native_tests): byte decode, clean frame, non-monotonic timestamp rejection.',
      'la_native_ctypes_models (tests/native/test_ctypes_models.py): UART parity and inversion vectors compared against tools/models/decoders.py.',
      'test_decoder_core_uart and test_decoder_core_stop_on (tests/native/live_core_host_tests.c, CTest la_native_live): segment feed, gap discontinuity and rolling stop-on match.',
    ],
    status: {
      status: 'bench-verified',
      summary:
        'Native/host-tested decoder library, wired into the Zynq decoder core; bench-verified at 115200 baud on the live path, with the 60 s zero-drop run and the P7.7 decoded-pattern stop both passing.',
      scope:
        'P7 live gates (test_live_uart_decode_events, test_live_uart_continuous_zero_drop, test_live_uart_isolated_byte_latency) and test_live_stop_on_decoded_pattern, 2026-09-11 to 2026-09-13. Host model parity proves functional agreement only, not A9 throughput or interrupt latency.',
      date: '13 Sep 2026',
      sources: [
        { label: 'Live UART gates', path: 'tests/e2e/test_e2e_live.py', kind: 'test' },
        { label: 'Bench evidence rows', path: 'docs/status.md', kind: 'doc' },
        { label: 'Native UART vectors', path: 'tests/native/native_tests.c', kind: 'test' },
        { label: 'Independent model', path: 'tools/models/decoders.py', kind: 'tool' },
      ],
    },
  },
  {
    slug: 'spi',
    name: 'SPI decoder',
    configuration: [
      'Required SCK channel; optional MOSI and MISO channels; optional CS channel. Signed channel numbers with -1 meaning the channel is absent.',
      'CPOL and CPHA.',
      'Word size 8 or 16 bits.',
      'Bit order: MSB-first or LSB-first.',
      'CS active level.',
      'SCK and at least one data channel are required at initialization.',
    ],
    behavior: [
      'A CS assertion starts a word stream and emits CS_ASSERT; a CS release emits CS_RELEASE.',
      'For CPHA=0, data is sampled on the leading edge (the transition away from CPOL); for CPHA=1, on the trailing edge (the transition back to CPOL).',
      'MOSI and MISO are sampled from the same post-edge sample.',
      'MSB-first words shift toward bit zero; LSB-first words place the first bit at bit zero.',
      'Each completed data lane emits a BYTE event at the final sampling edge, with one or two little-endian data bytes and the MOSI or MISO flag.',
      'When CS is absent, the decoder treats the bus as permanently selected.',
      'A clock edge coincident with CS assertion or release is a control edge, not a data edge; no duration assumption is made, only edge order matters.',
    ],
    events: [
      'CS_ASSERT when the configured active CS level appears.',
      'CS_RELEASE when CS returns to inactive.',
      'BYTE per completed lane at the final sampling edge, flagged MOSI or MISO, with one or two little-endian data bytes.',
      'FRAME_ERROR when CS releases with an incomplete word.',
      'DISCONTINUITY emitted by mark_discontinuity(t) when protocol state is reset.',
    ],
    errors: [
      'An incomplete word before CS release emits FRAME_ERROR.',
      'Initialization rejects a configuration without SCK or without at least one data channel.',
      'A timestamp decrease is an error: feed_transition returns a negative value.',
      'A control edge is never counted as a data bit, so CS assertion or release cannot fabricate a BYTE.',
    ],
    tests: [
      'test_spi_decoder (tests/native/native_tests.c, CTest la_native_tests): CS assert, 8-bit MOSI word, CS release event sequence.',
      'la_native_ctypes_models (tests/native/test_ctypes_models.py): CPOL/CPHA and bit-order vectors compared against tools/models/decoders.py.',
      'test_decoder_core_spi (tests/native/live_core_host_tests.c, CTest la_native_live): decoder-core segment feed and channel configuration.',
    ],
    status: {
      status: 'bench-verified',
      summary:
        'Native/host-tested decoder library, wired into the Zynq decoder core; bench-verified at the 10 kHz patgen SPI gate, and exercised as the 1 MHz overload fault injection that reports loss honestly.',
      scope:
        'P7.6 SPI live gate (test_live_spi_decode_events: MOSI 0x4C/0x41 with CS assert/release, dropped_segments=0) and the P8.4 overload (test_live_spi_overload_reports_loss), 2026-09-11. The patgen drives MOSI only, so MISO decoding is host-tested but not bench-decoded.',
      date: '11 Sep 2026',
      sources: [
        { label: 'Live SPI gate', path: 'tests/e2e/test_e2e_live.py', kind: 'test' },
        { label: 'P8.4 overload row', path: 'docs/status.md', kind: 'doc' },
        { label: 'Native SPI vectors', path: 'tests/native/native_tests.c', kind: 'test' },
        { label: 'Independent model', path: 'tools/models/decoders.py', kind: 'tool' },
      ],
    },
  },
  {
    slug: 'i2c',
    name: 'I2C decoder',
    configuration: [
      'Distinct SCL and SDA channels, both required.',
      'Both lines are initially high in a valid idle bus.',
      'No clock-rate parameter: clock stretching is handled implicitly because the decoder reacts only to observed SCL edges.',
    ],
    behavior: [
      'START is SDA falling while SCL remains high; STOP is SDA rising while SCL remains high.',
      'A repeated START emits START and begins a new address byte without requiring STOP.',
      'Bits are sampled on SCL rising edges, most significant bit first.',
      'After eight data bits, the ninth rising edge is the acknowledge bit: SDA low emits ACK and SDA high emits NACK.',
      'The first byte after START emits ADDRESS with the seven-bit address in data[0] and the READ flag set from its R/W bit.',
      'Later bytes emit BYTE at the eighth data edge, and ACK/NACK is timestamped at its ninth edge.',
      'An unexpected control transition does not invent a byte; a sequence gap resets the active transaction and requires a new START before ADDRESS or BYTE events are accepted.',
    ],
    events: [
      'START on SDA falling while SCL is high.',
      'STOP on SDA rising while SCL is high.',
      'ADDRESS for the first byte after START, with the READ flag when the R/W bit is one.',
      'ACK or NACK at the ninth rising edge.',
      'BYTE at the eighth data edge for later bytes.',
      'DISCONTINUITY emitted by mark_discontinuity(t) when protocol state is reset.',
    ],
    errors: [
      'A NACK is reported as an event, not as an error; the host decides what it means.',
      'An unexpected control transition does not fabricate a byte.',
      'A sequence gap resets the active transaction and requires a new START before ADDRESS or BYTE events are accepted.',
      'A timestamp decrease is an error: feed_transition returns a negative value.',
    ],
    tests: [
      'test_i2c_decoder (tests/native/native_tests.c, CTest la_native_tests): START, ADDRESS, ACK, BYTE, NACK and STOP event sequence.',
      'la_native_ctypes_models (tests/native/test_ctypes_models.py): address, ACK/NACK and STOP vectors compared against tools/models/decoders.py.',
      'test_decoder_core_i2c (tests/native/live_core_host_tests.c, CTest la_native_live): decoder-core segment feed and channel configuration.',
    ],
    status: {
      status: 'bench-verified',
      summary:
        'Native/host-tested decoder library, wired into the Zynq decoder core; bench-verified at the 10 kHz patgen I2C gate with the exact START/address/ACK/bytes/STOP pattern and no NACK.',
      scope:
        'P7.6 I2C live gate (test_live_i2c_decode_events: START, address 0x50, ACK, 0x4C, ACK, 0x41, ACK, STOP with dropped_segments=0), 2026-09-11. Host model parity proves functional agreement only, not A9 throughput or electrical signal integrity.',
      date: '11 Sep 2026',
      sources: [
        { label: 'Live I2C gate', path: 'tests/e2e/test_e2e_live.py', kind: 'test' },
        { label: 'Bench evidence row', path: 'docs/status.md', kind: 'doc' },
        { label: 'Native I2C vectors', path: 'tests/native/native_tests.c', kind: 'test' },
        { label: 'Independent model', path: 'tools/models/decoders.py', kind: 'tool' },
      ],
    },
  },
];

export const DECODER_COMMON: string[] = [
  'Decoders consume caller-owned transitions `(t_sample:u64, sample:u8)`: sample holds the eight logic channels after the acquisition sample at t_sample, and a transition is emitted only when the sample value changes.',
  'The library allocates no decoder state; the caller owns the decoder struct and must not share it between tasks without an owner or lock.',
  'Timestamps must be nondecreasing; equal timestamps are processed in call order and represent simultaneous channel changes, while a timestamp decrease is an error.',
  'The callback receives kind, channel, flags, timestamp and up to 16 data bytes; channel 0xFF means no single channel applies, and BYTE data is little-endian for 16-bit words.',
  'Callback return values are not used for ownership: the caller must copy the event or enqueue an owned event block before returning from the callback.',
  'mark_discontinuity(t) resets protocol state, records t as the last accepted timestamp and emits DISCONTINUITY.',
  'A segment sequence gap must call mark_discontinuity before feeding the next segment; the firmware then emits DISCONTINUITY, resets that decoder and increments its dropped-segment counter, so events after the gap are not interpreted as contiguous samples.',
  'advance(t) on UART supplies unchanged samples through t; it is the heartbeat for idle runs and must be called at segment boundaries, but it only lets a pending frame finish rather than inventing bytes.',
];
