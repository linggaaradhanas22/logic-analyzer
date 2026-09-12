# Logic Analyzer Link Protocol

This document is the normative P1 wire specification. The same frames are used
on Zynq UART0, the ESP32 UART relay and the host TCP connection. The C library
in `common/la_proto` is allocation-free and is the reference implementation of
framing, CRC and the fixed CONFIG/DECODER_CFG payloads.

## Frame

Every frame is encoded as:

| Bytes | Field | Meaning |
|---:|---|---|
| 1 | `SOF` | `0xA5` |
| 1 | `TYPE` | Message type below |
| 1 | `SEQ` | Sender-local sequence number, modulo 256 |
| 2 | `LEN` | Payload length, unsigned little-endian, 0 through 1024 |
| `LEN` | `PAYLOAD` | Type-specific bytes |
| 2 | `CRC16` | Unsigned little-endian CRC-16/CCITT-FALSE |

The CRC input is `TYPE`, `SEQ`, both `LEN` bytes and all payload bytes. CRC16
uses initial value `0xFFFF`, polynomial `0x1021`, no reflection and final XOR
`0x0000`. The maximum encoded frame is 1031 bytes (`LEN + 7`). A byte equal to
`SOF` in a header, payload or CRC has no special meaning.

The parser is incremental and may be fed one byte at a time. Before a frame is
accepted it checks the length, then CRC. On a bad length or CRC it returns to
the search state and treats the byte that caused the error as a possible new
`SOF`. It counts dropped bytes, format errors and CRC errors. Callback payload
storage belongs to the parser and is valid only until the next parser feed;
consumers copy or queue a handle before returning from the callback.

## Message Types

All multi-byte fields in payloads are unsigned little-endian unless explicitly
stated otherwise. Reserved bytes must be transmitted as zero and ignored only
where the receiver documents forward-compatible handling.

| Type | Name | Direction | Payload |
|---:|---|---|---|
| `0x01` | `PING` | host -> Zynq | empty |
| `0x02` | `PONG` | Zynq -> host | `fw_version:u32`, `pl_id:u32`, `uptime_ms:u64` |
| `0x10` | `CONFIG` | host -> Zynq | 37-byte acquisition configuration |
| `0x11` | `ARM` | host -> Zynq | empty |
| `0x12` | `ABORT` | host -> Zynq | empty |
| `0x13` | `GET_STATUS` | host -> Zynq | empty |
| `0x14` | `DECODER_CFG` | host -> Zynq | 20-byte decoder slot configuration |
| `0x15` | `LIVE_START` | host -> Zynq | empty |
| `0x16` | `LIVE_STOP` | host -> Zynq | empty |
| `0x17` | `PATGEN_CFG` | host -> Zynq | `patgen_ctrl:u32` (additive v1 extension) |
| `0x18` | `TRACE_CTRL` | host -> Zynq | `action:u8`, reserved[3] (additive v1 extension) |
| `0x20` | `STATUS` | Zynq -> host | status snapshot, 52 bytes in v1 |
| `0x21` | `EVT_TRIGGERED` | Zynq -> host | `trig_index:u64` |
| `0x22` | `EVT_DECODED` | Zynq -> host | zero or more event records |
| `0x23` | `EVT_OVERRUN` | Zynq -> host | `dropped:u32`, `stage:u8`, reserved[3] |
| `0x30` | `DATA_BEGIN` | Zynq -> host | burst metadata |
| `0x31` | `DATA_CHUNK` | Zynq -> host | `offset:u32` + up to 1016 bytes |
| `0x32` | `DATA_END` | Zynq -> host | reconstructed-data `crc32:u32` |
| `0x40` | `ACK` | both | acknowledged `TYPE:u8`, `SEQ:u8` |
| `0x41` | `NAK` | both | `TYPE:u8`, `SEQ:u8`, `error:u8` |
| `0x7F` | `LOG` | Zynq -> host | UTF-8 text, no trailing NUL |

The C enum `la_proto_message_type_t` contains the same assignments. Unknown
types are rejected by the typed command dispatcher but remain parseable by the
framing layer so a newer peer can be proxied without allocation.

## Fixed Payloads

### CONFIG, type `0x10`

The payload is exactly 37 bytes. `ctrl_flags` contains only the `SELFTEST_EN`
and `STREAM` bits; ARM, ABORT and SOFT_RESET are commands or local actions and
are not accepted in CONFIG.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | `ctrl_flags:u32` |
| 4 | 4 | `sample_div:u32` |
| 8 | 4 | `mode:u32`, only bits 0 and 1 are defined |
| 12 | 1 | `chan_en:u8` |
| 13 | 1 | `trig_mask:u8` |
| 14 | 1 | `trig_value:u8` |
| 15 | 1 | `trig_rise:u8` |
| 16 | 1 | `trig_fall:u8` |
| 17 | 4 | `pre_samples:u32`, 0 through 32768 |
| 21 | 4 | `post_samples:u32`, ignored when STREAM is set |
| 25 | 4 | `timeout:u32` sample clocks, zero disables timeout |
| 29 | 4 | `txn_max_run:u32`, 1 through `0x00FFFFFF` |
| 33 | 4 | `live_flush_cycles:u32`, nonzero when STREAM is set |

The receiver validates every field before accepting ARM or LIVE_START. The
register-map source and generated constants define the masks and reset values;
the C packer refuses invalid values instead of silently truncating them.

### PATGEN_CFG, type `0x17`

An additive v1 extension used by the serial HIL and E2E self-test flows; the
burst CONFIG payload has no pattern-generator field. The payload is exactly 4
bytes: the raw `PATGEN_CTRL` register value (`docs/regmap.yaml`), little-endian.
Reserved bits must be zero and `MODE` must be a defined value (0 counter, 1
walking-1, 2 PRBS8, 3 UART 115200, 4 SPI 1 MHz, 5 I2C 400 kHz, 6 UART single
frame then idle high); invalid
values are rejected with NAK bad-field. The receiver acknowledges the frame
and latches the value immediately; the pattern generator latches mode and DIV
at its enable edge. Peers that do not implement the type reject it with NAK
unsupported, which is the P4 burst firmware's behaviour only if the command
is unknown - this firmware implements it.

### TRACE_CTRL, type `0x18`

An additive v1 extension for the P8.3 task/queue trace. The payload is
exactly 4 bytes: `action:u8` followed by 3 reserved bytes that must be zero.
Action 1 (`STOP`) disables trace recording; action 2 (`START`) clears the
trace ring and enables recording; action 3 (`DUMP`) disables recording and
dumps the ring. Invalid actions are rejected with NAK bad-field. The receiver
acknowledges the frame, and for `DUMP` follows the ACK with a sequence of
`LOG` frames carrying the dump; the final line begins with `END `, so a host
can detect the end without a second command. The dump lines are ASCII:

| Line | Meaning |
|---|---|
| `LA TRACE 1 <counts_per_second>` | header; converts raw times to seconds |
| `TASK <number> <id-hex> <name>` | task object (id is the TCB pointer) |
| `QUEUE <id-hex> <name>` | queue or stream-buffer object |
| `REC <time> <token> <object> <value> <id-hex>` | one record, oldest first |
| `END <records> <total> <dropped>` | record count, accepted count, wrap loss |

Record tokens: `IN`/`OUT` (task switched in/out), `QS`/`QR`/`QF`/`RX` (queue
send, receive, send-failed, receive-failed), `SS`/`SR`/`SF`/`SE` (stream
buffer send, receive, send-failed, receive-failed), `PL`/`DM` (PL and DMA
interrupt entry), `SEG` (live segment published), `DEC` (decoder segment
fed), `TX` (UART frame queued) and `OVR` (loss report queued).

### DECODER_CFG, type `0x14`

The payload is exactly 20 bytes. Byte 0 is slot 0 through 2; byte 1 is type:
0 off, 1 UART, 2 SPI, 3 I2C. Bytes 2 and 3 are reserved. The remaining 16
bytes are a type-specific union.

For UART, bytes 4 through 8 are `rx_channel`, `data_bits`, `parity` (0 none,
1 even, 2 odd), `stop_bits` and `inverted`; bytes 9 through 11 are reserved;
bytes 12 through 15 are `baud_hz`; bytes 16 through 19 are `sample_rate_hz`.

For SPI, bytes 4 through 7 are signed channel numbers `sck`, `mosi`, `miso`
and `cs`; `-1` means that an optional channel is absent. Bytes 8 through 12
are `cpol`, `cpha`, `word_size` (8 or 16), `lsb_first` and `cs_active`.
Bytes 13 through 19 are reserved. SCK and at least one data channel are
required.

For I2C, bytes 4 and 5 are signed `scl` and `sda` channel numbers. Bytes 6
through 19 are reserved. Both channels are required and distinct.

### LIVE_START / LIVE_STOP, types `0x15` / `0x16`

Both payloads are empty. `LIVE_START` is accepted only in IDLE with a latched
STREAM `CONFIG` whose `live_flush_cycles` is nonzero and, when at least one
decoder slot is enabled, with transition encoding (`MODE.ENCODING = 1`);
otherwise it is NAKed (bad-state or bad-field). The reply is an ACK, the PL
streams from the arm edge and the host-visible state becomes CAPT.
`LIVE_STOP` is accepted from CAPT and is an idempotent ACK in IDLE. An ARM
whose CONFIG has STREAM set is still NAK unsupported: live streaming uses
`LIVE_START`.

## Status and Events

The v1 `STATUS` payload is 52 bytes:
`state:u8`, reserved[3], `status_reg:u32`, `word_count:u64`,
`trig_index:u64`, `cpu_load_permille:u16`, `min_free_stack:u16`, followed by
three `{events:u32, dropped_segments:u32}` records in UART, SPI, I2C order.

An `EVT_DECODED` payload is a sequence of records:
`slot:u8`, `kind:u8`, `t_sample:u64`, `len:u8`, `data[len]`. The event kinds
are BYTE, FRAME_ERROR, PARITY_ERROR, START, STOP, DISCONTINUITY, ADDRESS,
ACK, NACK, CS_ASSERT and CS_RELEASE. A batch must contain complete records;
there is no padding between records.

`EVT_OVERRUN.stage` is 0 for DMA, 1 for decoder and 2 for link. A decoder
segment sequence gap emits a DISCONTINUITY event, resets that decoder's state
machine and increments its dropped-segment counter. Events after the gap must
not be interpreted as contiguous samples.

## Burst Transfer

`DATA_BEGIN` is 16 bytes: `total_words:u64`, `encoding:u8`, reserved[3] and
`sample_rate_hz:u32`. `encoding` is 0 for raw packed words and 1 for
transition words. `DATA_CHUNK` contains a byte offset followed by at most 1016
data bytes, so its payload is at most 1020 bytes. `DATA_END` contains the
CRC32/IEEE of the concatenated data bytes, using initial `0xFFFFFFFF`, reflected
polynomial `0xEDB88320` and final XOR `0xFFFFFFFF`.

The host ACKs every 16 DATA_CHUNK frames and may ACK sooner when the receive
window is full. The sender may have at most 16 unacknowledged chunks. A NAK
identifies the original type and sequence and carries one error code. The
receiver discards an incomplete burst after timeout, disconnect or NAK.

## Sequence, Flow Control and Errors

Each endpoint owns one modulo-256 sequence space. A retransmission retains the
same sequence number. ACK and NAK frames are not themselves acknowledged. A
receiver rejects a command with `NAK` when its state, length or field values
are invalid. Error codes are: 0 none, 1 bad length, 2 bad field, 3 bad state,
4 unsupported, 5 busy, 6 overflow and 7 CRC.

UART uses RTS/CTS in addition to the frame window. The Zynq transmitter
reserves capacity for STATUS, ACK and NAK so best-effort decoded events cannot
starve control traffic. The ESP32 relays valid frames verbatim and sends
ABORT followed by LIVE_STOP when its TCP client disconnects. TCP uses the same
frame bytes; it does not add a second length or CRC layer.
