# RTOS Ownership and Scheduling Design

This document is the P1 ownership contract for the Zynq FreeRTOS application
and the ESP-IDF bridge. It describes static storage and queue policies before
target code exists. Windows tests exercise the ownership adapter only; they do
not prove interrupt latency, cache coherency or FreeRTOS scheduling.

## Zynq Tasks

The Zynq application runs FreeRTOS 10.x on CPU0 with a 1 kHz tick and static
allocation. Priorities are relative, with a larger number meaning more urgent.

| Task | Priority | Stack | Waits on | Owned work |
|---|---|---:|---|---|
| `AcqTask` | 6 | 4 KiB | DMA/PL notifications, `ctrl_q` | Sole owner of ordinary SG descriptors, DMA state, finite burst chains and live segment publication/recycling |
| `DecodeUartTask` | 5 | 3 KiB | `seg_q[UART]` | Decode referenced segments, allocate static event blocks, publish events and release its segment reference |
| `DecodeSpiTask` | 5 | 3 KiB | `seg_q[SPI]` | Same ownership contract for SPI |
| `DecodeI2cTask` | 5 | 3 KiB | `seg_q[I2C]` | Same ownership contract for I2C |
| `CmdTask` | 5 | 4 KiB | 8 KiB RX StreamBuffer | Parse, validate and dispatch commands; never touches DMA payloads |
| `TxTask` | 5 | 4 KiB | `ev_q`, `tx_q`, control notifications | Pack event/status/data frames, flush at 2 ms, release event blocks and write the single-producer TX StreamBuffer |
| `StatusTask` | 1 | 2 KiB | `vTaskDelay(500 ms)` | Stack watermarks, runtime statistics, counters, watchdog and heartbeat LED |

P8.1 priority review (2026-09-11): TxTask was priority 4, below the decoder
tasks. Under sustained overload AcqTask keeps every `seg_q` non-empty, so the
decoders never blocked and TxTask never ran: no event or overrun frame was
sent while the decoder counters climbed. TxTask now shares priority 5 with
the decoders and CmdTask, so FreeRTOS round-robins the consumer with the
producers; the event drain is additionally bounded by the TX StreamBuffer
space so the control path is serviced every turn.

`Idle` maintains the idle counter. Stack overflow and assertion hooks record a
LOG event and halt rather than continuing with corrupted ownership. The ARM_CA9
port has no dedicated ISR stack: every handler runs on the interrupted task's
stack, so the idle (2 KiB) and timer (2 KiB) tasks are also sized for the
UART/DMA handler worst case. The P4/P5 bench raised the original 2/2/2/1 KiB
sizes after a capture-time overflow.

**Interrupt dispatch (SDT BSP).** The 2026.1 SDT BSP's
`XConnectToInterruptCntrl()` silently skips every handler connection until
`XSetupInterruptSystem()` sets its internal `ScuGicInitialized` flag. The
FreeRTOS CA9 port initialises the GIC with `XConfigInterruptCntrl()` and never
sets that flag, so `xPortInstallInterruptHandler()` returns success while every
vector stays `StubHandler`. The first connection in `la_irq_connect()` therefore
goes through `XSetupInterruptSystem()` (which sets the flag); later connections
use the port API. Without this workaround the first asserted level interrupt
(UART TXEMPTY) never clears and the CPU is starved inside `FreeRTOS_IRQ_Handler`.

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

LinkRxTask and NetServerTask are subscribed to the 5 s task watchdog, which
runs in panic mode (`CONFIG_ESP_TASK_WDT_PANIC=y`): a hung watched task resets
the gateway instead of only logging. The P8.4 fault injection verified this
with the `CONFIG_LA_LINK_WDT_TEST_HOOK` build option, which hangs LinkRxTask
at start and produced `Task watchdog got triggered - LinkRx (CPU 1)` followed
by `Rebooting... rst:0xc (SW_CPU_RESET)`; the option defaults to off and is
never set in a normal build. Ring buffers use static capacities: 64 KiB
NOSPLIT for link-to-network (P8.2), a separately budgeted live ring, and a
16 KiB UART RX buffer. TCP is single-client until a later design change adds
explicit fan-out ownership.

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

## UART burst TX window (P4.9)

The wire rules come from `docs/protocol.md` (burst transfer): the host ACKs
every 16 `DATA_CHUNK` frames or sooner when its receive window is full, the
sender may have at most 16 unacknowledged chunks, and a retransmission retains
its sequence number. The sender-side tracker policy:

- `TxTask` keeps one window slot per in-flight chunk; a chunk is pushed only
  when the window is not full. Sequence numbers never duplicate inside the
  window: a NAK does not add a slot, it flags the existing slot as
  retransmit-pending and the resend reuses the same sequence, so an ACK for
  that sequence can clear at most one slot.
- An `ACK(type, seq)` clears the outstanding chunk slot with that sequence.
  ACKs for sequences that are not outstanding are stale and ignored; the
  sender treats duplicate ACKs the same way.
- A `NAK(type, seq, error)` on a chunk keeps the slot outstanding, marks it
  for retransmission and leaves the window count unchanged. NAKs on commands
  or on a completed burst stop that burst per protocol.md, which is
  CmdTask/AcqTask policy, not the window tracker's.
- The tracker is pure logic (no FreeRTOS or UART calls); `TxTask` alone owns
  it, one writer, and holds the retransmit buffer that backs a resend.

## CmdTask command dispatch core (P4.9)

`la_cmd_dispatch.{h,c}` is the pure receive-side router for the burst-path
command set (PING, CONFIG, ARM, ABORT, GET_STATUS and the inbound ACK/NAK
responses). CmdTask owns it as the single writer; the framing parser runs
inside CmdTask, and every frame CmdTask accepts passes through one
`la_cmd_dispatch_step()` call. Responses are never sent by CmdTask itself:
ACK/NAK/PONG/STATUS frames are transmitted by TxTask (the single TX
StreamBuffer writer), and ARM/ABORT are forwarded to AcqTask on `ctrl_q`.

- **Acquisition view mirror.** The core keeps the host-visible lifecycle
  (IDLE/ARMED/CAPT). ARM is accepted only when IDLE with a valid CONFIG
  latch and transitions the view to ARMED optimistically; ABORT is accepted
  from ARMED or CAPT and returns to IDLE optimistically. These optimistic
  transitions are exact because the dispatch guards are the same ones
  AcqTask's `la_acq_fsm` enforces on the same configuration, so an accepted
  command cannot fail downstream. Every AcqTask lifecycle event (ARM
  accepted, trigger, completion, abort, timeout) is echoed back through
  `la_cmd_dispatch_acq_notice()` so the view is reconciled even if a
  divergence ever occurs.
- **CONFIG latch.** A CONFIG frame is unpacked and validated with
  `la_proto_unpack_config`; the validated copy is the single source for the
  next ARM. CONFIG is refused (NAK busy) while the machine is not IDLE and
  never reaches the register driver during a run. The latch survives the
  whole capture lifecycle and is replaced only by the next valid CONFIG.
- **Error mapping.** Bad length -> NAK bad-length; invalid field values ->
  NAK bad-field; ARM without a CONFIG latch -> NAK bad-state; double
  ARM/CONFIG while busy -> NAK busy; unknown types, Zynq->host messages
  received as requests, DECODER_CFG/LIVE_START/LIVE_STOP and STREAM-flagged
  ARMs -> NAK unsupported (P7.x replaces the last two). ABORT in IDLE is an
  idempotent ACK without forwarding (an abort of an idle machine is
  trivially satisfied; the ESP32 relay may send ABORT unconditionally on
  disconnect).
- **Inbound ACK/NAK routing.** ACK of a DATA_CHUNK and NAK of a DATA_CHUNK
  are routed to TxTask (window clear / retransmit pending per the window
  rules above). A NAK of any other frame type (commands, DATA_BEGIN,
  DATA_END) is routed as a burst failure: TxTask stops the burst job per
  protocol.md. Malformed or stale ACK/NAK frames are ignored by the core;
  the window tracker applies its own stale/duplicate rules.

## TxTask burst transfer packer (P4.9)

`la_tx_pack.{h,c}` is the pure generator for one completed burst job: an
AcqTask JOB (total 32-bit words plus encoding and sample rate recorded at
arm) becomes the ordered protocol.md frame payloads DATA_BEGIN,
DATA_CHUNK*(up to 1016 data bytes each), DATA_END. Chunk bytes are copied
through a caller read callback, so the packer owns no capture memory and a
host test can back it with an ordinary buffer. DATA_END carries the
CRC32/IEEE of the concatenated chunk bytes (streamed on first emission),
which equals the CRC of the whole capture byte range.

- **Boundaries.** The packer does not assign sequence numbers, does not
  track the ACK window (`la_tx_win` is separate), and does not frame-encode
  (`la_proto_encode`). TxTask calls `la_tx_pack_next()` only when the window
  can accept the produced frame and transmits every produced frame exactly
  once, in order; a produced-but-undelivered frame would desynchronize the
  CRC. Chunk boundaries start at multiples of 1016 bytes; 1016 is divisible
  by 4, so a 32-bit word is never split across chunks.
- **Retransmission.** A chunk NAK marks the window slot retransmit-pending;
  TxTask rebuilds that chunk with `la_tx_pack_chunk_at()`, which reproduces
  the first-emission payload deterministically from the byte offset without
  touching the CRC or offset state. TxTask maps window slot -> chunk index.
- **Job and buffer lifetime.** The capture buffer stays owned by the job
  from JOB receipt until the END frame is on the wire and every outstanding
  chunk slot is resolved (window drained, or the burst abandoned on a
  non-chunk NAK / disconnect / overflow). On abandonment nothing is
  retransmitted further. Empty jobs (zero words) emit DATA_BEGIN followed
  by DATA_END with the empty-buffer CRC; completion is sticky (`next()`
  stays -1).

## P7 live decode pipeline cores (P7.1-P7.4)

The P7 live path is built from pure, host-tested cores (`fw-zynq/app/src/live/`)
plus task adapters:

- **la_segment_pool** (promoted from the P7.1 host tests): 16 segments with
  FREE/DMA_OWNED/CPU_OWNED states, generation and sequence counters,
  publish-with-consumer-count and abort rules. The lock backend is selected at
  compile time - Win32 critical section for the native tests, pthread for a
  POSIX native build, FreeRTOS critical section for the app - and every pool
  operation runs in a task, never an ISR, so a critical section is sufficient
  and the same source compiles for both builds.
- **la_live_ring**: one slot per 256 KiB live segment (16 slots = 4 MiB at the
  base of the non-cached capture pool). AcqTask is the single writer: start
  acquires every slot (DMA-owned), complete publishes the actual length with
  the enabled-decoder consumer count, release drops one consumer reference,
  repost re-acquires the lowest FREE slot and abort frees DMA-owned slots
  while leaving CPU-owned payloads to their consumers. A zero-consumer packet
  recycles immediately; a queue-full for one decoder releases only that
  reference; repost failure is the pool-exhaustion signal. The BD ID carries
  the segment index, so the completed descriptor maps to its slot without a
  side table.
- **la_decoder_core**: one wrapper around `common/la_decode` per decoder slot,
  fed with transition words `{run[23:0], sample[7:0]}`. A value change calls
  feed_transition; UART additionally advances to the last sample of every run
  so bit samples and end-of-frame events inside a long run are not missed. A
  segment sequence that is not the expected successor emits DISCONTINUITY,
  resets the decoder state and increments dropped_segments, so events after a
  gap are never interpreted as contiguous samples. Event counts feed the
  STATUS decoder records.
- **la_event_batch**: builds EVT_DECODED payloads (11-byte record header plus
  data) up to the 1024-byte payload cap. A record that does not fit is dropped
  whole (drop-newest), never truncated.
- **la_proto** gained the EVT_OVERRUN payload helper (`dropped:u32` LE,
  `stage:u8`, reserved[3]).
- **Dispatch/FSM**: DECODER_CFG latches a validated slot in IDLE (a slot is
  enabled when its type is not OFF); LIVE_START requires a latched STREAM
  CONFIG with a nonzero flush interval and transition encoding whenever a
  decoder is enabled, ACKs and moves the view to CAPT; LIVE_STOP is accepted
  from CAPT and is an idempotent ACK in IDLE. `la_acq_fsm` stream
  configurations arm straight to CAPT (the PL streams from the arm edge);
  burst behavior is unchanged, and ARM+STREAM stays NAK unsupported because
  live streaming uses LIVE_START.

## P4 target wiring (P4.5-P4.9)

The P4 firmware instantiates the design above in `fw-zynq/app/src`:
`la_app.{h,c}` owns every static object, `drivers/la_irq` and
`drivers/la_dma` and `drivers/la_link_uart` wrap the Xilinx drivers, and
`tasks/la_acq_task`, `tasks/la_cmd_task`, `tasks/la_tx_task` and
`tasks/la_status_task` run the task logic. The pure cores
(`la_acq_fsm`, `la_burst_plan`, `la_cmd_dispatch`, `la_tx_win`,
`la_tx_pack`, `la_irq_stats`, `la_regs_drv`) carry the decisions and are
host-tested; the task files only move bytes and hardware state.

### Interrupt plumbing (P4.5)

`la_irq_init` installs handlers through the FreeRTOS port
(`xPortInstallInterruptHandler` + `vPortEnableInterrupt`, the SDT-flow
entry points) and then raises both fabric lines to
`XINTERRUPT_DEFAULT_PRIORITY` (0xA0): the port itself does not set a GIC
priority, and the ISR-safe API asserts unless the priority is numerically
below the FreeRTOS mask (`configMAX_API_CALL_INTERRUPT_PRIORITY` = 18 ->
0x90 on the 5-bit GIC). The DMA interrupt ID comes from
`XAxiDma_Config.IntrId[0]`; the PL interrupt has no generated ID because
`la_top_0` is a module reference, so it is derived from the P3 xlconcat
wiring: `la_top_0/irq_o` -> `xlconcat/In0` -> `IRQ_F2P[0]` -> GIC 61,
encoded as `61 - 32 = 29` in the SDT numbering (the DMA on `In1` is
`XPAR_FABRIC_AXI_DMA_0_INTR = 30`, which pins the mapping).

Both ISRs are notification-only: the PL ISR reads `IRQ_STAT`, write-1-clears
the pending bits and latches them for AcqTask; the DMA ISR acknowledges the
channel through `la_dma_isr()`. Neither touches a queue. `la_irq_stats`
records every ISR duration and, from AcqTask, the ISR-exit-to-task wake
latency in XTime counts; StatusTask prints min/avg/max in microseconds. The
v1 STATUS payload has no latency field, so latency is reported through the
console (and LOG for host-visible cases) rather than STATUS.

### DMA burst driver (P4.6)

`la_burst_plan` computes the worst-case word count before an ARM is
accepted (raw: `ceil(samples/4)`; transition: one word per sample) and
splits it into a finite chain of at most 256 BDs of at most 64 KiB. An ARM
whose plan exceeds the 8 MiB capture pool is rejected with NAK overflow by
`la_cmd_dispatch` before any register is touched. `la_dma_prepare` then
resets the channel, recreates the BD ring inside the non-cached SG pool,
programs the exact chain with actual lengths and buffer addresses, hands it
to hardware and starts the channel. There is no `XAxiDma_SimpleTransfer`
call and no cyclic mode anywhere. On PL DONE, AcqTask polls
`la_dma_collect` until the closing RXEOF beat lands (bounded by 50 ms),
sums the actual completed lengths, and resets the channel so a partially
filled chain can never leak into the next capture; a channel/BD error or a
timeout aborts instead of handing a job. The PL `WORD_COUNT` is the
authoritative word count for the TxJob.

### AcqTask (P4.7)

AcqTask owns `la_acq_fsm`, all `la_regs_drv` writes, `la_dma` and the
capture buffer. On an ARM from `ctrl_q` it applies the latched CONFIG to the
registers, programs IRQ_EN, prepares the DMA chain, writes the CTRL.ARM
pulse and posts an ARMED notice. It wakes on the PL/DMA notifications with
`xTaskNotifyWait` and a software timeout that mirrors the PL trigger
timeout (a backup; the PL aborts autonomously and has no ABORT IRQ bit). On
completion it hands exactly one `la_tx_job_t` to TxTask through `job_q` and
sets a buffer-busy gate that only the JOB_DONE notification from TxTask
clears; the next ARM waits for that release, so the capture buffer is never
reused while a burst is still on the wire. Empty captures are still handed
over (DATA_BEGIN + DATA_END with the empty CRC) so the host never times
out. ABORT or timeout writes CTRL.ABORT, stops the DMA and posts an
ABORTED notice.

### UART0 link (P4.8)

`la_link_uart` drives PS UART0 through EMIO at `LA_LINK_BAUD` (2 Mbps since
the P7.6 gate; 921600 during bring-up) 8N1 with automatic RTS/CTS (`MODEMCR.FCM`). The XUartPs interrupt handler reports RX
data/timeout/error events; the event handler copies the received bytes into
the RX StreamBuffer with `xStreamBufferSendFromISR` and re-arms the
receive, counting overflow and error events. TX is a single in-flight
`XUartPs_Send` from a staging buffer: TxTask writes frames to the TX
StreamBuffer and calls `la_link_uart_tx_kick`; the TX-empty interrupt
refills the staging buffer from the StreamBuffer from ISR context. The
staging buffer is never overwritten while the UART still reads it. A
recorded bench caveat: with nothing driving the JD header, the floating
UART0 RX pad generates framing/overrun interrupts continuously and can
starve the CPU; the board wiring expects the peer (USB-UART adapter or
ESP32) to drive RX/CTS, and a pad pull-up on `uart0_rx`/`uart0_ctsn` is the
follow-up if the design must tolerate an unconnected header.

### CmdTask and TxTask wiring (P4.9)

CmdTask owns the `la_proto` parser and `la_cmd_dispatch`. It drains the RX
StreamBuffer in 5 ms slices, feeds every accepted frame through the
dispatch core and maps outcomes onto the queues: ACK/NAK/PONG/STATUS/LOG
and EVT_TRIGGERED go to `tx_q`; ARM/ABORT go to `ctrl_q`; inbound
ACK/NAK routing goes to `ack_q`; PATGEN_CFG is written straight to
PATGEN_CTRL after the ACK. Notices from AcqTask reconcile the dispatch
view and produce EVT_TRIGGERED frames. TxTask owns `la_tx_win`,
`la_tx_pack`, the window-slot -> chunk-index map and the single TX
StreamBuffer writer. It starts a job from `job_q`, sends DATA_BEGIN, up to
1016-byte DATA_CHUNKs while the 16-deep window has room, and DATA_END once
every chunk is produced and acknowledged; a NAK rebuilds the exact chunk
payload with `la_tx_pack_chunk_at` under the original sequence. The Zynq
sequence space is a single atomic counter shared by CmdTask and TxTask.

| Queue | Producer | Consumer | Full policy |
|---|---|---|---|
| `ctrl_q` (4) | CmdTask | AcqTask | Reject is impossible from the dispatch guards; a full queue logs and drops the forward |
| `notice_q` (8) | AcqTask | CmdTask | Lifecycle notices; a full queue drops the notice and the next one reconciles the view |
| `tx_q` (16) | CmdTask | TxTask | Blocking send with a 20 ms budget, then drop + counter |
| `ack_q` (32) | CmdTask | TxTask | Zero-wait; a full queue drops the routing message and counts a drop |
| `job_q` (1) | AcqTask | TxTask | One job in flight by construction (buffer-busy gate) |
| `rx_sb` 8 KiB | UART RX ISR | CmdTask | ISR counts the unsent remainder and the parser sees the loss |
| `tx_sb` 4 KiB | TxTask | UART TX ISR | Bounded 100 ms send; a short send abandons the burst and counts a drop |

### P7 live path wiring (P7.2-P7.4)

`drivers/la_dma` gained the live ring calls: `la_dma_prepare_live` builds an
ordinary 16-BD SG ring (one 256 KiB segment per BD, no cyclic mode) with the
segment index in the BD ID, `la_dma_collect_live` drains and frees completed
BDs with their actual lengths, `la_dma_post_live` allocates one free BD and
re-posts it for a recycled segment, and `la_dma_free_bd_count` bounds the
repost loop. AcqTask owns the whole live lifecycle:

- **LIVE_START** (from CmdTask `ctrl_q`) waits for the burst buffer-busy gate,
  applies the STREAM CONFIG, starts the pool ring and the live BD ring,
  publishes the enabled decoder configurations on each decoder task's `cfg_q`
  and arms the PL. The PL streams from the arm edge (stream configuration
  arms straight to CAPT) and a `LA_NOTICE_STREAMING` reconciles the dispatch
  view.
- **Completion IRQs** collect completed BDs, publish each segment with the
  enabled-decoder count and push a `la_live_segment_msg_t` to every enabled
  decoder's `seg_q` (depth 8). A full `seg_q` releases only that decoder's
  reference and counts `decoder_queue_drops`; the decoder observes the
  sequence gap as DISCONTINUITY. Free segments are re-acquired and re-posted
  in the same IRQ, and a decoder release notification (`LA_NOTIFY_SEGMENT_FREE`)
  triggers the same repost loop.
- **Pool exhaustion**: when no descriptor is posted and no segment is free
  the consumers are behind, so AcqTask stops before the PL FIFO overflows,
  reports `EVT_OVERRUN(stage=0)` and quiesces. A PL FIFO overflow also ends
  the stream after the controller's drain and DONE.
- **LIVE_STOP/ABORT**: write CTRL.ABORT, reset the S2MM channel, notify every
  decoder task (`LA_NOTIFY_LIVE_STOP`) to drain and release, then abort the
  ring and wait (bounded, 200 x 1 ms) until the last CPU-owned reference is
  released before returning to IDLE. A partially read segment is never
  overwritten because a slot is only reposted while FREE.

`tasks/la_decoder_task` (one per slot, prio 5, 1 KiB stack) drains `cfg_q`
then `seg_q`, feeds `la_decoder_core` and fills a static event block from the
free-block queue. A full block or the 2 ms flush timer sends the block
pointer to the multi-producer `ev_q`; a missing free block or a full `ev_q`
drops whole events and increments the per-slot dropped counter. After every
segment it copies the core counters into the app arrays that STATUS reads.
Consumer releases go through `la_live_ring_release` (not the pool directly),
under a short critical section shared with AcqTask's repost loop, so the ring
slot state, `published` count and `dma_posted` count stay exact.
`TxTask` reports a decoder drop increase as `EVT_OVERRUN(stage=1)` **before**
draining the event backlog: under overload `ev_q` is never empty, so a report
placed after the drain would only reach the host at teardown.

| Queue | Producer | Consumer | Full policy |
|---|---|---|---|
| `cfg_q` (2) per decoder | AcqTask (LIVE_START) | decoder task | One config per run; a full queue leaves the slot disabled |
| `seg_q` (8) per decoder | AcqTask | decoder task | Drop that decoder's reference only; the sequence gap becomes DISCONTINUITY |
| `block_q` (12) | TxTask + decoder tasks | decoder tasks | No free block drops the event and counts it |
| `ev_q` (8) | decoder tasks (multi-producer) | TxTask | Full queue drops the whole batch and counts its records |
| `notice_q` | AcqTask | CmdTask | EVT_OVERRUN notices are additive to the lifecycle notices |

## ESP32 link relay core (P5.2)

`la_relay.{h,c}` (in the `la_link` IDF component, include/link API; pure C,
compiled by MSVC for `test-esp32-mocks` and by the ESP-IDF toolchain for
`fw-esp32`) is the routing policy for one directed relay leg. The tasks own
the UART/TCP drivers and the byte rings (`link_to_net` 64 KiB NOSPLIT since
the P8.2 tuning, `net_to_link`, the P10 live ring); the core owns
classification, decisions and counters. P8.2 measurements are surfaced by
UiTask: `to_net_max_used` is the peak `link_to_net` occupancy in bytes and
`overrun_reports` counts the emitted `EVT_OVERRUN(stage=2)` frames.

- **Type tables are caller-supplied.** The Zynq -> host leg (LinkRxTask)
  installs `EVT_DECODED` as its only best-effort and mirror type; the
  host -> Zynq leg (LinkTxTask) installs empty tables, making every frame
  control traffic. Unknown parseable types are not in any best-effort
  table, so they relay verbatim and can never be dropped by policy (a
  newer peer is proxied).
- **Congestion policy.** Best-effort frames are drop-newest when the
  destination ring cannot take the frame (counted in
  `best_effort_dropped`, never silent). Control frames are preserved: no
  room yields a PRESSURE decision and one `control_pressure_events`
  increment; the caller blocks on its ring put or retries - control and
  status traffic cannot be starved by the decoded event stream. A
  FORWARD decision is only counted after the caller reports its real
  outcome (`frames_forwarded` / `frames_forward_failed`), because the
  NOSPLIT ring can still refuse a put when free space is fragmented even
  though the capacity callback said yes.
- **P8.4 link-overrun report.** Each best-effort drop also increments
  `pending_drops`; LinkRxTask turns the accumulated count into one
  `EVT_OVERRUN(stage=2 link)` frame for the host and clears the count only
  after the ring accepts the report (`la_relay_pending_drops()` /
  `la_relay_clear_pending_drops()`). The report is best-effort too: while
  the ring is still full it stays pending and is retried after the next
  forwarded frame, so the host learns about every silent-loss window
  instead of seeing a corrupted event stream. The deterministic host
  read-stall test (`test_live_link_overrun_reported_on_stall`) and the
  natural 60 s runs both exercise this path.
- **Live-view mirror.** After a successful forward of a mirror type,
  LinkRxTask also mirrors the frame into the P10 live ring and reports the
  outcome (`frames_mirrored` / `mirror_dropped`). A net-leg drop skips the
  mirror entirely, so the live view never shows events the net link lost.
- **Counters** are frame-based and are surfaced by UiTask (P5.4) and,
  later, in the web view; `la_relay_reset_counters()` clears them without
  touching the configuration. None of this validates UART RTS/CTS, ring
  scheduling or Wi-Fi timing - those remain P5.5/P8 target gates.

The P5.2 adapter layer (`la_link_uart.c`, `la_link_tasks.c`, `la_link.h`)
implements this on the confirmed classic-ESP32 board: UART2 at
`CONFIG_LA_LINK_BAUD` (2 Mbps since the P7.6 gate) 8N1 with
hardware RTS/CTS, a 16 KiB RX driver buffer, ~64-byte RX FIFO threshold and a
20-entry event queue; LinkRxTask (core 1, prio 20) parses with `la_proto`,
re-encodes accepted frames verbatim into a static 64 KiB NOSPLIT `link_to_net`
ring and reports accumulated drops as `EVT_OVERRUN(stage=2)`, mirroring
`EVT_DECODED` into an 8 KiB live ring (the P10 consumer is
still open, so a full ring is counted in `mirror_dropped`); LinkTxTask (core 1,
prio 18) drains a static 16 KiB NOSPLIT `net_to_link` ring to the UART with a
bounded, watchdog-safe wait for TX room. NetServerTask is the only producer of
`net_to_link`, so the single-producer rule holds. The adapters are compiled by
the IDF build only; the policy core stays host-tested.

## ESP32 network session policy core (P5.3)

`la_net_policy.{h,c}` (in the `la_net` IDF component; pure C, compiled by
MSVC for `test-esp32-mocks` and by the ESP-IDF toolchain for `fw-esp32`) is
the session lifecycle policy the `la_net` tasks implement: Wi-Fi station
association with exponential backoff, single-client TCP on :5555, and the
protocol rule that a lost client answers the Zynq link with ABORT then
LIVE_STOP (protocol.md). One policy instance is owned by the session task
(NetServerTask); WifiMgr forwards Wi-Fi events into it. No ESP-IDF, socket,
timer or FreeRTOS calls happen in the core - adapters feed events plus a
monotonic millisecond clock and execute the returned actions in order.

- **States** IDLE -> CONNECTING -> (BACKOFF) -> CONNECTED ->
  CLIENT_ACTIVE. START from IDLE emits WIFI_CONNECT immediately and is the
  only legal start; double starts are ignored and STOP is required first.
  STOP from any running state emits the matching stop actions (LISTEN_STOP
  only when listening or active) and returns to IDLE.
- **Backoff** starts after the first failure: the wait doubles per
  consecutive failure up to the cap and resets on a successful
  association. BACKOFF_TICK events with a clock before the deadline are
  ignored; at or after it a new WIFI_CONNECT is emitted. Wi-Fi loss while a
  client is active also emits LINK_ABORT + LINK_LIVE_STOP (the Zynq session
  would otherwise run unseen) plus LISTEN_STOP.
- **Single-client rule**: CLIENT_CONNECTED is accepted only in CONNECTED
  (silent state change); otherwise CLIENT_REJECT is emitted and counted, so
  a second connection never displaces the relayed client.
- **Actions are executed in order**: a client disconnect produces exactly
  `[LINK_ABORT, LINK_LIVE_STOP]` so the Zynq sees the abort before the live
  stop. Counters (attempts/successes, accepted/rejected clients,
  disconnects) feed UiTask; nothing here validates real Wi-Fi association
  timing, lwIP behavior or mDNS - P5.5/P8 target gates.

The P5.3 adapter layer (`net_wifi.c`, `net_server.c`, `net_mdns.c`) drives the
policy from the Wi-Fi event task (WIFI_UP on `IP_EVENT_STA_GOT_IP`, WIFI_LOST
on `WIFI_EVENT_STA_DISCONNECTED`) through a static event queue into
NetServerTask (core 0, prio 12), which owns the policy, a 100 ms `select()`
loop, the single non-blocking client socket and the host-stream `la_proto`
parser. LISTEN_STOP closes the listener and any client; a lost client or Wi-Fi
loss executes the ordered LINK_ABORT/LINK_LIVE_STOP actions by encoding control
frames into the link TX ring. Host frames are re-encoded verbatim into the link
ring with TCP backpressure (the socket is not read while the ring is blocked).
The Zynq->host ring is drained and counted while no client is attached so a
later session never inherits stale frames. mDNS (`la-esp32.local`,
`_la-analyzer._tcp` on :5555) comes from the pinned `espressif/mdns` managed
component (1.12.0 in `fw-esp32/dependencies.lock`).

## Cache, MMU and Buffer Contract

The initial payload pool, burst capture buffer and SG descriptors occupy a
reserved Normal Non-cacheable DDR region. Linker bounds, MMU section alignment
and non-overlap with firmware, heap, stack and FSBL must be asserted at boot.
If caching is enabled later, descriptor and payload maintenance are separate,
64-byte aligned and based on actual received lengths, including short tails.
Barriers are required before publishing a descriptor and after CPU ownership.

Concrete layout (P4.3, single source: `fw-zynq/app/src/la_mem_geom.h` and
`fw-zynq/lscript.ld`): the Zybo Z7-10 has 1 GiB DDR from `0x0010_0000` to
`0x4000_0000`; the top 16 MiB (`0x3F00_0000`, sixteen 1 MiB MMU sections) is
carved out as the Normal Non-cacheable window. The burst capture pool is the
first 8 MiB of the window and the SG descriptor pool the next 1 MiB; both are
64-byte aligned with cache-line-rounded capacities. The P7 live pool reuses
the first 4 MiB of the capture pool as sixteen 256 KiB segments
(`la_live_ring`), which is legal because burst and live are mutually
exclusive modes. The linker script exports
`__la_nc_base/size/end` and `__la_fw_end`, asserts at link time that firmware
never reaches the window, and `main()` re-checks the geometry at boot before
programming each 1 MiB section Non-cacheable with `Xil_SetTlbAttributes(..,
NORM_NONCACHE)`. Anything that reads or writes the window from the CPU side
must still respect the 64-byte allocation alignment for future cached
operation; today no cache maintenance is needed on the non-cached path.

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

### P8.2 results (ESP32 relay, 2026-09-11)

Measured on the bench with the P7 firmware, the 2 Mbps link and the 115200
UART live stream (~10 k events/s, ~120 kB/s of `EVT_DECODED` frames), final
configuration (64 KiB `link_to_net` ring, 16 KiB lwIP windows):

| Metric | Observed |
|---|---|
| One-way live forwarding | ~1000 frames/s, ~120 kB/s, zero drops in the clean 60 s runs |
| Peak `link_to_net` occupancy, isolated 60 s gate | 40,144 B of 65,536 B (61%) |
| Peak `link_to_net` occupancy, full `test-e2e` suite | 65,536 B, reached only by the deliberate 3 s host read-stall fault injection |
| Drops, full suite | 721 frames / 11 `EVT_OVERRUN(stage=2)` reports, all from the stall fault injection; the 60 s gate had zero |
| Minimum free heap during the full suite | 65,508 B |
| RSSI during the runs | -61 to -69 dBm |

The first tuning attempt also raised the lwIP TCP windows to 32 KiB. That
exhausted the heap during live streaming (3.5 KB free): lwIP could not
allocate send pbufs, TCP stalled, the relay dropped 29 k frames without being
able to enqueue the stage=2 reports, and the session ended in the stale-client
state. Reverting the windows to 16 KiB while keeping the 64 KiB static ring
(no heap cost) restored a 65 KB heap floor and made the full suite green.
The old 32 KiB ring dropped at the isolated-gate workload; 64 KiB absorbs it.
Remaining levers for more margin: a larger UART driver RX buffer, host
read-path efficiency, and the P9.2 compact event format.

**ESP32 response latency (P8.2).** The `link_to_net` ring is not a `select()`
descriptor, so NetServerTask only drained it after its select timeout: a
sparse host command (PING, PATGEN_CFG) waited up to 100 ms for its reply.
Reducing `LA_NET_SELECT_TIMEOUT_US` to 5 ms cut the measured command round
trip from 93-125 ms to ~15 ms with no measurable cost.

**Isolated final byte (P7.6).** The patgen mode 6 single shot (one
`LA-TEST\n` frame per enable, then idle high) plus the Zynq-side
`live_max_us` counter measure the true UART-byte-to-send latency: over 20
shots the worst value was 428 us against the 25 ms p99 budget (the host
round trip is 46-235 ms but includes the command path and is not the gate
metric). The host-side test asserts the frame decodes and the final byte's
event is delivered with the in-stream baseline. A wake-latency outlier of
625 ms was recorded in the same ISR stats run; it is a P8.2 follow-up
(probably a boot/first-use artifact, not reproduced in the live path).

## Windows Adapter Boundary

The native adapter models static segment handles, synchronized reference
release, queue-full behavior and abort races with Windows threads. It is useful
for deterministic ownership tests and ctypes ABI checks. It cannot validate
FreeRTOS priority inversion, ISR wake-up latency, task watchdog behavior, DMA
coherency, UART RTS/CTS or DDR throughput; those remain P4/P5/P8 gates.
