# RTOS Ownership and Scheduling Design

This document is the P1 ownership contract for the Zynq FreeRTOS application
and the ESP-IDF bridge. It describes static storage and queue policies before
target code exists. Windows tests exercise the ownership adapter only; they do
not prove interrupt latency, cache coherency or FreeRTOS scheduling.

## Zynq Tasks

The Zynq application runs FreeRTOS 10.x on CPU0 with a 1 kHz tick and static
allocation. Priorities are relative, with a larger number meaning more urgent.

| Task | Priority | Stack | Waits on | Owned work |
|---|---:|---:|---|---|
| `AcqTask` | 6 | 2 KiB | DMA/PL notifications, `ctrl_q` | Sole owner of ordinary SG descriptors, DMA state, finite burst chains and live segment publication/recycling |
| `DecodeUartTask` | 5 | 3 KiB | `seg_q[UART]` | Decode referenced segments, allocate static event blocks, publish events and release its segment reference |
| `DecodeSpiTask` | 5 | 3 KiB | `seg_q[SPI]` | Same ownership contract for SPI |
| `DecodeI2cTask` | 5 | 3 KiB | `seg_q[I2C]` | Same ownership contract for I2C |
| `CmdTask` | 5 | 2 KiB | 8 KiB RX StreamBuffer | Parse, validate and dispatch commands; never touches DMA payloads |
| `TxTask` | 4 | 2 KiB | `ev_q`, `tx_q`, control notifications | Pack event/status/data frames, flush at 2 ms, release event blocks and write the single-producer TX StreamBuffer |
| `StatusTask` | 1 | 1 KiB | `vTaskDelay(500 ms)` | Stack watermarks, runtime statistics, counters, watchdog and heartbeat LED |

`Idle` maintains the idle counter. Stack overflow and assertion hooks record a
LOG event and halt rather than continuing with corrupted ownership.

## ESP32 Tasks

The ESP32 uses ESP-IDF FreeRTOS SMP at a 1 kHz tick. The classic dual-core
affinity below is provisional until the exact module is identified; single-core
parts require a reviewed adaptation rather than silently reusing it.

| Task | Core | Priority | Waits on | Owned work |
|---|---:|---:|---|---|
| `LinkRxTask` | 1 | 20 | UART event queue | Parse frames and write the single-producer `link_to_net` ring; mirror decoded frames into `live_rb` |
| `LinkTxTask` | 1 | 18 | `net_to_link` ring | UART TX with RTS/CTS |
| `NetServerTask` | 0 | 12 | TCP select, 100 ms | One TCP client on port 5555; relay bytes and issue ABORT plus LIVE_STOP on disconnect |
| `LiveViewTask` | 0 | 8 | `live_rb` | P10 WebSocket broadcast and last-N history |
| `WifiMgr` | 0 | event loop | Wi-Fi/IP events | Station connection, backoff and mDNS |
| `UiTask` | 0 | 5 | 250 ms delay | LED and statistics output |

LinkRxTask and NetServerTask are subscribed to the 5 s task watchdog. Ring
buffers use static capacities: 32 KiB NOSPLIT for link-to-network, a separately
budgeted live ring, and a 16 KiB UART RX buffer. TCP is single-client until a
later design change adds explicit fan-out ownership.

## Ownership and Queue Policies

The live pool contains 16 segment handles, each with a 256 KiB capacity. The
actual DMA length is tracked separately from capacity. A segment progresses
through `FREE -> DMA_OWNED -> CPU_OWNED -> FREE`; generation and sequence
counters reject stale handles. Payload and descriptor capacities are aligned to
64 bytes and rounded to 32-byte multiples.

| Object | Producer/owner | Consumer | Full or failure policy |
|---|---|---|---|
| SG descriptor ring | `AcqTask` only | DMA hardware | Stop acquisition, quiesce hardware and report DMA overflow; never recycle hardware-owned descriptors |
| `seg_q[decoder]` | `AcqTask` | One decoder task | Queue stores handles, not payload copies. If full, release the attempted reference and increment dropped-segment count |
| Segment reference count | `AcqTask` establishes references | Decoder last release | Each successful queue publish adds one reference. The last release notifies AcqTask; no consumer returns a descriptor directly |
| Static event-block pool | Decoder tasks | `TxTask` | Zero-wait drop-newest; increment per-decoder event-drop count and retain reserved status capacity |
| `ev_q` handle queue | UART/SPI/I2C decoder tasks | `TxTask` | Multi-producer FreeRTOS queue. Zero-wait drop-newest of the event block, then release it |
| `ctrl_q` | `CmdTask` | `AcqTask` | Bounded command queue; reject with NAK when full |
| UART RX StreamBuffer | UART RX ISR | `CmdTask` | ISR send-from-ISR only; parser-visible overflow counter and NAK/LOG path |
| UART TX StreamBuffer | `TxTask` | UART TX ISR | Single writer/single reader. TxTask blocks only within its bounded command budget; control/status frames have reserved capacity |
| ESP32 link rings | One named producer | One named consumer | Drop-newest decoded best-effort data, preserve control frames and count drops |

The initial event-block budget is 16 KiB. The exact block size and queue depth
are constants in the target implementation and must be reported in STATUS;
the budget may not be exceeded by hidden heap allocation. All shared objects
have one documented writer. A FreeRTOS queue of handles is used for multiple
decoder producers; an unsynchronized shared MessageBuffer is prohibited.

## ISR and DMA Rules

ISRs perform notification or send-from-ISR operations only:

- UART RX ISR calls `xStreamBufferSendFromISR`.
- PL IRQ calls `xTaskNotifyFromISR(AcqTask, bits)`.
- DMA S2MM IRQ calls `xTaskNotifyFromISR(AcqTask)`.
- UART TX ISR drains the single-producer TX StreamBuffer.

No decoder, CRC, allocation, queue walking or payload copy runs in an ISR.
`AcqTask` is the only code that changes SG ownership. Both burst and live use
ordinary scatter-gather descriptors; cyclic mode and simple-transfer APIs are
not permitted. A descriptor is republished only after DMA has stopped using it
and every successful consumer reference has been released.

## Cache, MMU and Buffer Contract

The initial payload pool, burst capture buffer and SG descriptors occupy a
reserved Normal Non-cacheable DDR region. Linker bounds, MMU section alignment
and non-overlap with firmware, heap, stack and FSBL must be asserted at boot.
If caching is enabled later, descriptor and payload maintenance are separate,
64-byte aligned and based on actual received lengths, including short tails.
Barriers are required before publishing a descriptor and after CPU ownership.

An acquisition overflow stops or marks a gap; it never silently drops samples.
If no free live segment exists, AcqTask reports `EVT_OVERRUN(stage=DMA)` and
stops the stream after quiescing the producer. Decoder sequence gaps reset the
affected state machine and emit DISCONTINUITY.

## Timing and Measurement

These are initial targets, not local test claims:

| Budget | Target |
|---|---:|
| PL live packet flush | 2 ms or 64 Ki words, whichever occurs first |
| TxTask event-frame flush | 2 ms |
| Zynq ISR to task | <=20 us |
| ESP32 ISR to task | <=100 us |
| UART input byte to complete Zynq event | <=25 ms p99 at 115200 input and 2 Mbps link, non-overloaded |
| Decoder throughput | 5 M transitions/s/task experimental target on shared CPU |
| SG service turnaround | <=200 us target; not a permitted data-path gap |

At 100 MS/s raw acquisition is 100 MB/s. A 4 MiB live pool is about 41.9 ms
of raw runway and less for worst-case transition expansion. Sixteen 2 ms
partial packets provide approximately 32 ms of descriptor runway. P8 must
measure queue high-water marks, stack high-water marks, CPU load, dropped
segments, event drops, DMA errors, cache maintenance and latency distributions
on target hardware.

## Windows Adapter Boundary

The native adapter models static segment handles, synchronized reference
release, queue-full behavior and abort races with Windows threads. It is useful
for deterministic ownership tests and ctypes ABI checks. It cannot validate
FreeRTOS priority inversion, ISR wake-up latency, task watchdog behavior, DMA
coherency, UART RTS/CTS or DDR throughput; those remain P4/P5/P8 gates.
