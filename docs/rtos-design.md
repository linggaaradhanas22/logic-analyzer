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

### P7.7 protocol-aware stop (decoded-pattern trigger)

`DECODER_CFG` byte 2 carries `stop_on_len` and the type's reserved bytes carry
the pattern (UART 3, SPI 7, I2C 14 bytes; `docs/protocol.md`). The path is:

- `la_decoder_core` keeps a rolling match position and compares the data of
  every emitted BYTE record (in record order) against the pattern. A full
  match latches `stop_on_hit` and the matching record's `t_sample`; the event
  itself is still emitted, so the host sees the byte that completed the
  pattern. Non-BYTE events (START/STOP/ACK/...) never match. The match is
  detected when the decoder processes the segment, not at the wire edge, so a
  backlogged decoder stops the stream later than the physical byte.
- The decoder task checks the latch after each fed segment. The first decoder
  to match wins: it latches slot and sample index in `la_app_t` under a
  critical section and notifies AcqTask with `LA_NOTIFY_STOP_ON`; the match is
  reported once per configuration.
- AcqTask reuses the `LIVE_STOP` path (`acq_stop_live`): write `CTRL.ABORT`,
  stop the DMA, notify every decoder to drain and flush, abort the live ring
  (DMA-owned slots freed at once, CPU-owned slots drained by their consumers),
  then soft-reset the datapath and return the FSM to IDLE. This is the
  "freeze still-owned completed segments for burst drain" behavior of the
  plan: already-completed segments are drained and their decoded events reach
  the host instead of being discarded. It posts `LA_NOTICE_STOP_ON` with the
  matching sample index instead of a plain abort.
- CmdTask maps the notice to `EVT_TRIGGERED(match_index)` plus a LOG line, so
  the host live client can end the session and report "trigger on decoded
  value". A plain PL trigger still emits `EVT_TRIGGERED` without ending a
  session; the host opts in with `stop_on_match`.
- Retaining a last-K pre-trigger history is out of scope (it needs a
  separately budgeted history pool and explicit ownership), matching the
  plan's P7.7 note.

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
- **P9.4 bounded pressure and RX rescue.** The PRESSURE wait is bounded at
  2 s; after that the control frame is dropped and counted
  (`pressure_timeouts`) instead of parking LinkRxTask forever. An unbounded
  wait was the root cause of the 2026-09-13 "gateway dead" degradation: a
  stalled host filled the ring, the blocked task stopped reading the UART,
  the full RX buffer held RTS low, and the Zynq TX path (PONG/status)
  stalled with it. LinkRxTask also drains whatever the UART driver has
  buffered after every pass (`rx_rescue_bytes`), so a lost `UART_DATA`
  event under overflow cannot deadlock the RX path either.
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
still open, so a full ring is counted in `mirror_dropped`), and drains the
driver buffer on every pass so a lost `UART_DATA` event cannot deadlock the
RX path; LinkTxTask (core 1,
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
NetServerTask (core 0, prio 12), which owns the policy, the single non-blocking
client socket and the host-stream `la_proto` parser. Since P9.4 the task polls
its non-blocking accept/recv/send paths every 2 ms instead of depending on
lwIP `select()` (which stopped waking the listening socket after a relay
flood); a zero-timeout select probe feeds the `sready`/`serr` diagnostics, a
3 s stale-listener check rebuilds the listener when established :5555
connections exist while `accept()` stays idle (`refresh`), accepted sockets
get TCP_NODELAY, and a fast reconnect gets a 250 ms grace window plus a
MSG_PEEK liveness check so the departing client's in-flight FIN is not
mistaken for a second client. LISTEN_STOP closes the listener and any client;
a lost client or Wi-Fi loss executes the ordered LINK_ABORT/LINK_LIVE_STOP
actions by encoding control frames into the link TX ring. Host frames are
re-encoded verbatim into the link ring with TCP backpressure (the socket is
not read while the ring is blocked). The Zynq->host ring is drained and
counted while no client is attached so a later session never inherits stale
frames. mDNS (`la-esp32.local`, `_la-analyzer._tcp` on :5555) comes from the
pinned `espressif/mdns` managed component (1.12.0 in
`fw-esp32/dependencies.lock`).

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
operation.

Since P8.5 (2026-09-12) the first 4 MiB of the window (the live pool) is
re-programmed Normal Write-Back cacheable (`NORM_WB_CACHE`) when
`LA_MEM_LIVE_CACHED` is 1, which is the tracked default. The measured decoder
throughput is about 28% higher than the non-cached variant (results below).
The cache contract for that region is: PL DMA writes are never coherent with
the A9 caches, so every CPU read of DMA-written data must first invalidate the
written range. Two call sites do this and must stay in sync with any new
reader:

- `la_acq_task` live publish: after `la_dma_collect_live` reports a completed
  segment and before the segment is queued to any decoder,
  `Xil_DCacheInvalidateRange(slot, round_up(length, 64))` drops the stale L1
  and L2 lines. The BSP implementation maintains both levels and takes its
  fast path for cache-line-aligned ranges.
- `la_tx_task` burst start: before packing a capture job,
  `Xil_DCacheInvalidateRange(job.buffer, round_up(total_words * 4, 64))` covers
  the whole used capture buffer (the cached prefix and the non-cached tail;
  invalidating the non-cached tail is harmless).

The CPU never writes payload data, so lines are clean and invalidation cannot
lose data. The SG descriptor pool and the rest of the window stay
Non-cacheable, so descriptor maintenance is unchanged.

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

### P8.5 results (cached vs non-cached live pool, 2026-09-12)

The plan asks whether decoder throughput improves when live segments sit in
cached memory with `Xil_DCacheInvalidateRange` instead of the Non-cacheable
window. The experiment built both variants from the same source with the
`LA_MEM_LIVE_CACHED` switch and measured the same workload on the bench.

Method: patgen SPI mode 4 at DIV=0 (1 MHz SCK, CH0/CH1/CH2), sample_div=0
(100 MS/s transition stream), live decoder slot 0 = SPI (sck 0, mosi 1, cs 2,
active low), 20 s window. The host drains events without retaining them and
does not stop on the expected stage-1/2 overrun reports
(`stop_on_overrun=False`, `collect_events=False`), so the Zynq is not
backpressured into idle. The decoder task accumulates `decoder_words[slot]`
and the XTime spent inside `la_decoder_core_feed_segment`
(`decoder_busy_us[slot]`); StatusTask prints both. The status task starves
while the decoder is saturated, so the test snapshots the counters before the
stream and after LIVE_STOP has quiesced and divides the deltas by the
host-measured window. Each variant's boot banner (`LA live pool: ...`) was
captured during the JTAG reload.

| Build | Run | Decoder words/s | Decoder busy | Dropped segments |
|---|---|---:|---:|---:|
| `LA_MEM_LIVE_CACHED=0` | 1 | 973,085 | 96.2% | 4,620 |
| `LA_MEM_LIVE_CACHED=0` | 2 | 1,006,094 | 99.5% | 4,787 |
| `LA_MEM_LIVE_CACHED=0` | 3 | 1,007,818 | 99.5% | 4,786 |
| `LA_MEM_LIVE_CACHED=1` | 1 | 1,288,915 | 99.3% | 3,337 |
| `LA_MEM_LIVE_CACHED=1` | 2 | 1,282,279 | 98.9% | 3,326 |
| `LA_MEM_LIVE_CACHED=1` | 3 | 1,284,519 | 99.0% | 3,326 |

The cached variant decodes about 28% more transition words per second than the
steady non-cached runs (the first non-cached run is 3% slower again) and drops
about 30% fewer segments at the same offered rate, so it is the tracked
default. The SPI decoder is saturated in both variants (96-99% busy), which is
what makes the comparison meaningful: the workload offers more than either
variant can decode.

Correctness was verified after the switch: the full `test-e2e` suite passes
36 + 2 skipped with the cached build, including both burst matrices (the
TxTask invalidate path) and the 60 s zero-drop UART live gate (the AcqTask
invalidate path) with bit-exact decoded data. The invalidation cost per
256 KiB segment is negligible against the milliseconds of decode work it
precedes, and the cached region is the same physical 4 MiB the live ring
already used, so no memory layout changed. The `la_mem_apply_range` helper and
the geometry constants are host-tested (`la_native_mem`).

### P8.3 results (task/queue tracing, 2026-09-12)

The plan asks for custom `traceTASK_SWITCHED_IN/OUT` and queue trace macros
into a ring dumped via `LOG`, rendered as a timeline, including one annotated
trace of a live UART decode. The implementation has three layers:

- `la_trace_core` (pure, host-tested): a 4096-record overwriting ring of
  16-byte records (`time`, handle, event, object, value) plus a 24-entry
  object table mapping raw TCB/queue handles to names.
- `la_trace` (firmware glue): the FreeRTOSConfig.h macros call
  `la_trace_task_switch`, `la_trace_queue` and `la_trace_stream`; application
  markers cover the PL/DMA interrupt entry, live segment publication, decoder
  feed and UART frame transmission. Task switches lazily register the idle
  and timer TCBs with their kernel names. Recording is off by default.
- `TRACE_CTRL` (0x18, additive v1): START clears and enables, STOP disables,
  DUMP disables and emits the ring as `LOG` lines (`LA TRACE` header, object
  table, one `REC` line per record oldest-first, `END` marker). The host
  `la-cli trace --dump` collects them, and `host/la_cli/trace.py` parses and
  renders a per-task timeline PNG (`--png`, optional millisecond zoom).

The `*_FAILED` queue/stream hooks are deliberately not wired: tasks poll
queues with zero timeouts, so failed polls would flood the ring with idle
noise.

**Critical fix.** The first implementation serialized ring writes with the
port's `ulPortSetInterruptMask()`/`vPortClearInterruptMask()` pair. That pair
manages the GIC priority mask and `vPortClearInterruptMask()` executes
`cpsie i` unconditionally when the mask changed. Called from the SWI/IRQ
context-switch path (where the CPU masks interrupts with CPSR.I but ICCPMR
is 0), it re-enabled interrupts *inside* `vTaskSwitchContext`, allowing a
nested tick IRQ to save the handler's context into the newly selected task's
TCB. The observed failure was a corrupted idle `pxTopOfStack` whose restored
PC pointed into `vPortClearInterruptMask` and whose LR/SPSR held trace-ring
addresses; the CPU then executed ring data and took a data abort. The fix
saves and restores CPSR.I itself (`mrs cpsr` + `cpsid i`, `cpsie i` only when
it was enabled), so an exception context stays masked and a task's interrupt
state and any enclosing critical section are untouched. This is documented in
`la_trace.c` next to `trace_irq_lock()`.

Bench evidence (2026-09-12, `fw 0x00080000`, 2 Mbps link): the P8.3 E2E test
(`tests/e2e/test_p83_trace.py`) passes 18 consecutive runs (12 before and 6
after the lazy idle registration), three 3 s trace sessions during the 1 MHz
SPI overload kept balanced IN/OUT records and the full DMA→segment→decode→TX
chain, and the full `test-e2e` suite passes 37 + 2 skipped. The published
annotated trace is `docs/images/p83-live-uart-trace.png`: a 5 ms zoom on the
first DMA IRQ of a 115200 UART decode showing `DM` (DMA IRQ) → `acq` segment
marker and queue send → `dec0` decoder-feed marker → `tx` stream send and
`TX` frame marker, with the idle lane labelled. The raw dump is regenerated
by the test under the ignored `build/traces/`.

### P8.6 evaluation (AMP skipped, documented 2026-09-12)

The plan marks CPU1 decoder offload as a stretch and allows a documented skip.
The measurements collected in P7/P8 show the decoder is not the system
bottleneck at any supported gate, so the skip is deliberate rather than an
omission.

Decoder demand versus capacity (all measured on this bench):

| Workload | Offered transition words/s | One decoder capacity | Utilization |
|---|---:|---:|---:|
| UART 115200 live gate (P7.6) | ~0.1 M | ~1.28 M (P8.5, cached) | < 10% |
| SPI 10 kHz full duplex gate (P7.6) | ~0.03 M | ~1.28 M | < 3% |
| I2C 10 kHz gate (P7.6) | ~0.02 M | ~1.28 M | < 2% |
| SPI 1 MHz overload (P8.4, deliberate) | ~3 M | ~1.28 M | saturated |

The binding constraint is the transport, not the CPU: the 2 Mbps UART link
carries ~200 kB/s of payload and the ESP32 relay measured ~120 kB/s, while the
v1 event stream of the 115200 UART gate is ~10,069 events/s x 12 bytes =
~121 kB/s. That stream already sits at the relay ceiling, which is why the
60 s gate is tight and why P8.2 named the P9.2 compact format as the next
lever. Moving decode work to CPU1 would not change any of those numbers; it
would only add a second execution resource that the end-to-end path cannot
feed.

Design sketch if the stretch is ever revived (kept here so the work is not
re-derived): CPU1 is released by writing its entry address to the OCM
`0xFFFFFFF0` register and executing `SEV`; it runs a bare-metal loop (or a
second FreeRTOS image) with its own MMU table that maps DDR Normal-cacheable
and the 16 MiB non-cached window exactly as CPU0 does. Segment descriptors
travel CPU0 -> CPU1 through a single-producer/single-consumer ring in OCM,
and release messages travel back CPU1 -> CPU0, so the segment pool and live
ring state stay owned by CPU0 and keep their existing single-writer rules;
CPU1 only ever holds a borrowed handle. Event blocks come from a second
static pool with SPSC rings in both directions, and either an SGI or a shared
`SEV`/`WFE` flag signals new work. Cross-core visibility uses `__DMB()`
between the payload write and the index update plus LDREX/STREX atomics; the
SCU keeps the two A9 L1s coherent for cacheable Normal memory, and the P8.5
cached live pool would need CPU1 to invalidate its own view before reading a
DMA-written segment. The gaps to accept are a second image to build and load,
no task-watchdog coverage for CPU1, and ownership bugs that only appear under
bench load.

Revisit when at least one of these becomes true: a transport measured at
1.5 MB/s or more (the P9.1 SPI link or an ESP32-S3 USB-OTG path), sustained
decode demand above roughly 1 M transition words/s at a supported gate, or a
multi-decoder configuration whose combined demand exceeds one core. Until
then P8.6 stays skipped with this rationale.

### P9.2 results (compact events, 2026-09-12)

The plan asks to quantify raw vs transition burst sizes for actual workloads,
implement versioned block/delta timestamps if higher live rates are required,
and recalculate the full-duplex SPI event bandwidth before revising the P7
gates. All three were done; the compact format is implemented and verified
but is opt-in (the v1 EVT_DECODED encoding remains the default).

**Burst raw vs transition sizes.** The step (counter/walking-1/PRBS8) and UART
rows are computed from the same bit-exact waveform models the P6.2 matrix
asserts against hardware; SPI and I2C are bench captures (4096-sample windows,
`build/reports/p92-sizes.json`). Ratio is transition bytes / raw bytes, where
raw is one byte per sample and a transition word is 4 bytes.

| Workload | Raw bytes | Transition bytes | Ratio |
|---|---:|---:|---:|
| counter/walking/PRBS8 @1M | 1,024 | 4,096 | 4.00 |
| counter/walking/PRBS8 @10M | 8,192 | 3,280 | 0.40 |
| counter/walking/PRBS8 @100M | 65,536 | 2,624 | 0.04 |
| UART 115200 @1M | 40,000 | 468 | 0.012 |
| UART 115200 @10M | 80,000 | 468 | 0.006 |
| UART 115200 @100M | 160,000 | 468 | 0.003 |
| SPI @1M / 10M / 100M | 4,096 | 3,188 / 3,188 / 320 | 0.78 / 0.78 / 0.08 |
| I2C @1M / 10M / 100M | 4,096 | 3,224 / 648 / 132 | 0.79 / 0.16 / 0.03 |

There is no fixed compression ratio: a counter that changes every sample is
4x *larger* when transition-encoded (the encoder is a live-stream format, not
a general compressor), while a UART or I2C stream shrinks 30-300x. Raw mode
is the right choice for fast-changing signals in burst captures.

**Compact EVT_BLOCK v1.** `EVT_CFG` (0x19, format 0/1) selects the live
encoding before `LIVE_START`; format 1 sends `EVT_BLOCK` (0x24) blocks with a
12-byte header (version, slot, count, t_base) and per-record
`t_delta:u16, kind:u8, len:u8, data[len]`; a delta at or above 0xFFFF uses an
absolute-u64 escape. Timestamps and event semantics are byte-exact. Bench
measurement on the 115200 UART live stream over fixed 2 s windows (same
offered rate, ~19.4k events per run):

| Encoding | Wire bytes | Events | Bytes/event |
|---|---:|---:|---:|
| v1 EVT_DECODED | 242,330 | 19,346 | 12.53 |
| compact EVT_BLOCK | 118,357 | 19,527 | 6.06 |

That is a **51.6% wire-byte reduction** (53.3% in a second run) with the
identical decoded byte stream. A single UART byte event drops from a 12-byte
v1 record to about 5.6 bytes; blocks carry one slot and use u16 deltas. The
live pipeline reaches ~120 kB/s at 115200 with v1, right at the ESP32 relay
ceiling, so the compact format roughly doubles the relay headroom and is the
lever named in the P8.2 results.

**Full-duplex SPI event bandwidth.** A full-duplex SPI byte produces two BYTE
events (MOSI and MISO) plus CS transitions; at 8-bit words the event rate is
2 x SCK/8. With the measured 12.53/6.06 bytes per event:

| SCK | Events/s | v1 wire | Compact wire | vs 200 kB/s link |
|---:|---:|---:|---:|---|
| 10 kHz | 2,500 | 31 kB/s | 15 kB/s | 16% / 8% |
| 100 kHz | 25,000 | 313 kB/s | 152 kB/s | over / 76% |
| 1 MHz | 250,000 | 3.1 MB/s | 1.5 MB/s | deliberate overload |

The P7.6 SPI/I2C 10 kHz gates keep their >=20% headroom with either encoding.
Compact events raise the zero-drop full-duplex SPI ceiling to roughly
60-80 kHz against the measured ~120 kB/s relay; 100 kHz still needs the P9.1
SPI link or the P9.4 Wi-Fi tuning. The P7 gates are therefore unchanged.

**Robustness fixes found during the measurements.** A stalled burst transfer
(the host vanished or the relay stalled) used to wedge the firmware forever:
TxTask waited for ACKs with no timeout and AcqTask then blocked forever in
`acq_wait_buffer_free`. TxTask now abandons a job after 2 s without progress
(no chunk sent and no ACK) and AcqTask bounds its buffer wait at 5 s and
reports an honest abort instead of blocking. The bench also exposed that a
live session started while the patgen is already running (after a previous
LIVE_STOP) streams almost no transitions; the P7.6 helper already toggles the
patgen EN before every session and the P9.2 tests do the same.

## Windows Adapter Boundary

The native adapter models static segment handles, synchronized reference
release, queue-full behavior and abort races with Windows threads. It is useful
for deterministic ownership tests and ctypes ABI checks. It cannot validate
FreeRTOS priority inversion, ISR wake-up latency, task watchdog behavior, DMA
coherency, UART RTS/CTS or DDR throughput; those remain P4/P5/P8 gates.
