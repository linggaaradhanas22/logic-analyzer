# RTOS implementation — source-backed report section

Technical draft prepared on 12 October 2026 for the project report. This section
describes the current direct-USB build. Recorded measurements keep their original
dates; reviewing code does not constitute a new hardware test.

## What the RTOS does in this analyzer

The real-time operating system (RTOS) is FreeRTOS running on the Zynq processing
system's Cortex-A9 CPU0. It schedules command handling, acquisition management,
live protocol decoding, transmission, USB servicing and status monitoring. The
programmable logic (FPGA) performs the time-critical input sampling and trigger
evaluation. Therefore, a 1 ms operating-system tick does not limit the analyzer
to one sample per millisecond: the FPGA samples at up to 100 million samples per
second, and an AXI direct-memory-access (DMA) engine writes the samples to DDR
memory independently of individual CPU instructions.

This division lets acquisition continue while software interprets earlier
segments and sends results to the computer. The RTOS coordinates these
concurrent activities and controls who may reuse each buffer. It does not make
the USB link as fast as the acquisition pipeline. Full-rate raw acquisition is
buffered burst operation; the measured host drain rate is a separate quantity.

The default build uses Zynq USB CDC-ACM directly. The ESP32 gateway remains an
optional build profile, but its network tasks are not part of the default
eight-task Zynq application. The PC's Python server and browser are host
software, not additional FreeRTOS tasks on the analyzer.

**Implementation evidence:** `fw-zynq/app/src/main.c` calls `la_app_init()` and
then `vTaskStartScheduler()`; `fw-zynq/app/src/la_app.c` creates the application
tasks, queues and stream buffers. The profile selected by `tools/run.py` governs
which application is built; the fallback macro in an old header comment does
not define the runner's current default.

## Scheduling and static allocation

The project configuration sets `configTICK_RATE_HZ` to 1000, giving a 1 ms tick.
The local generated board-support-package configuration enables preemption and
time slicing. A larger task-priority number means higher urgency. Acquisition
management runs at priority 6, command/decoder/transmit/USB work at priority 5,
and background status work at priority 1. A newly ready acquisition task can
preempt lower-priority work. Equal-priority runnable tasks share CPU time; tasks
waiting for notifications, queues or delays allow other tasks to execute.

The task control blocks, stacks, queues, event blocks and stream-buffer storage
are statically allocated and instantiated with APIs such as
`xTaskCreateStatic()` and `xQueueCreateStatic()`. Dynamic-allocation support
remains enabled in the BSP for compatibility with its kernel build; this is not
the same as allocating the analyzer's task pipeline dynamically. Idle and timer
task memory is also supplied through static memory hooks in `main.c`.

| Application task | Priority | Allocated stack | Responsibility |
|---|---:|---:|---|
| `la_acq` / AcqTask | 6 | 4 KiB | Own capture lifecycle, ordinary SG DMA descriptors, live publication and reposting; respond to PL/DMA notifications and control messages. |
| `la_cmd` / CmdTask | 5 | 4 KiB | Parse incoming protocol frames, validate requests and route commands, responses and incoming acknowledgements. |
| `la_tx` / TxTask | 5 | 12 KiB | Pack completed capture data and decoded events; manage acknowledgement/retransmission state and enqueue USB frames. |
| `la_dec0` | 5 | 4 KiB | Decode live segments according to slot 0's configuration; publish event batches and release borrowed segments. |
| `la_dec1` | 5 | 4 KiB | Same function for independently configured slot 1. |
| `la_dec2` | 5 | 4 KiB | Same function for independently configured slot 2. |
| `la_usb` / UsbTask | 5 | 12 KiB | Handle USB setup/session work, staged received data, endpoint servicing and ordered transmission. |
| `la_status` / StatusTask | 1 | 4 KiB | Update the monitored stack-watermark summary on a five-second loop; optional console diagnostics. |

The three decoder tasks are configurable slots. They are not permanently
assigned one each to UART, SPI and I2C; a slot can use a supported protocol or
be disabled. FreeRTOS stack constants count `StackType_t` elements, not bytes.
On this 32-bit Cortex-A9 port, 1024 elements are 4096 bytes and 3072 elements are
12,288 bytes. Idle and timer stacks are separately configured as 512 elements
(2 KiB each); they are not counted among the eight application tasks.

TxTask shares priority 5 with the decoders because the earlier priority-4
consumer starved under sustained decoder overload. Giving the consumer equal
priority allows it to drain events and service the command path while producers
remain runnable. A tick or flush interval is a scheduling parameter, not a
guarantee that all workloads will meet that latency.

**Implementation evidence:** `fw-zynq/FreeRTOSConfig.h`, the local generated
`build/vitis/ws/lap/ps7_cortexa9_0/fr/bsp/include/FreeRTOSConfig.h`,
`fw-zynq/app/src/la_app.h`, `la_app.c` and `tasks/la_status_task.c`. The generated
BSP is local build evidence; it must be regenerated on another checkout.

## Live acquisition-to-decoding flow

1. UsbTask feeds received command bytes into the 8 KiB RX stream buffer.
   CmdTask parses and validates them, then sends control/configuration messages
   to AcqTask. Per-slot decoder configuration changes also pass through AcqTask
   so the count of segment consumers remains consistent.
2. AcqTask starts an ordinary scatter-gather DMA descriptor chain. The live pool
   has sixteen segments, each with 256 KiB capacity (4 MiB total). Neither
   simple DMA transfers nor hardware cyclic mode is used.
3. The FPGA emits transition/run words, flushing the packet at its configured
   time/size boundary. Each word stores an eight-bit sample value and a
   24-bit run count. Completed segments may be much shorter than 256 KiB;
   capacity and actual received length are tracked separately.
4. The DMA interrupt acknowledges the hardware condition and notifies AcqTask.
   AcqTask collects completed descriptors and invalidates the written cache
   range before a decoder reads a cached DMA payload.
5. AcqTask publishes the same segment by handle to each enabled decoder's
   segment queue. The queue carries a handle, slot, length and sequence, not a
   256 KiB payload copy. A reference count records the remaining consumers.
6. Each decoder feeds the transition words to the shared C decoder core,
   obtains a static event block and adds decoded records. It processes at most
   four queued segments per turn so stop/configuration notifications can be
   serviced under load. Event batches flush when full or on the task's
   two-millisecond flush check.
7. Filled event-block pointers enter the multi-producer event queue. TxTask
   encodes the records, enqueues complete frames to the USB pool and returns
   the event block to the free-block queue. UsbTask submits ready USB frames
   in FIFO order; completion returns a TX slot to the pool.
8. The decoder releases its borrowed segment reference and notifies AcqTask.
   A segment becomes reusable only after all consumer references are released.
   AcqTask alone reposts its buffer to DMA. A sequence gap resets the affected
   decoder and produces a discontinuity indication instead of treating missing
   data as continuous samples.

The segment ownership lifecycle is `FREE -> DMA_OWNED -> CPU_OWNED -> FREE`.
Zero-copy applies to the large acquisition payload passed to decoders. Event
packing, transport framing and USB RX staging use bounded copies; the whole
system must not be described as copy-free.

**Implementation evidence:** `tasks/la_acq_task.c`, `tasks/la_decoder_task.c`,
`tasks/la_tx_task.c`, `tasks/la_usb_task.c`, `live/la_live_ring.c`,
`live/la_segment_pool.c` and `live/la_decoder_core.c` beneath
`fw-zynq/app/src/`.

## Burst acquisition and buffer lifetime

Burst mode completes a finite capture before the host drains the result.
AcqTask publishes a `la_tx_job_t` containing the capture-buffer address, word
count, encoding and sample rate to the one-entry job queue. It holds a
buffer-busy gate until TxTask sends the `LA_NOTIFY_JOB_DONE` release
notification. A subsequent capture cannot overwrite memory still used by the
transmitter. TxTask packs DATA_BEGIN, DATA_CHUNK and DATA_END frames, tracks
outstanding chunks, retransmits requested chunks under their original sequence
numbers and computes the whole-capture CRC32. Frames also carry CRC16.

The burst pool is 8 MiB; the live pool reuses its first 4 MiB, because the two
modes are mutually exclusive. The 64 MiB USB throughput trial was sixteen
separate 4 MiB captures per trial, not a single 64 MiB capture buffer.

## Synchronization, queue capacity and overload policy

| Object | Capacity | Producer / consumer | Behavior when unavailable or full |
|---|---:|---|---|
| `ctrl_q` | 4 messages | CmdTask or USB session handling / AcqTask | Command forwarding is zero-wait; CmdTask logs `ctrl queue full` on failure. The session-abort send does not check its result. Do not describe this as an unconditional full-queue NAK guarantee. |
| `notice_q` | 8 messages | AcqTask / CmdTask | Zero-wait publication; failed sends do not block acquisition. |
| `tx_q` | 16 small frames | CmdTask / TxTask | Wait up to 20 ms; failure increments the TX-drop count. |
| `ack_q` | 32 messages | CmdTask / TxTask | Zero-wait send; failure increments the TX-drop count. |
| `job_q` | 1 job | AcqTask / TxTask | One burst job is in flight; the buffer-busy gate prevents reuse. |
| Per-slot `cfg_q` | 2 messages | AcqTask / decoder | Configuration is queued without an unbounded wait; successful publication determines configuration application. |
| Per-slot `seg_q` | 8 handles | AcqTask / decoder | Drop that consumer's attempted reference and increment its queue-drop count. Later sequence gaps become discontinuities. |
| `block_q` | 12 pointers | Recyclers / decoder tasks | No free block drops/counts the new decoded event. |
| `ev_q` | 8 pointers | Decoder tasks / TxTask | Drop/count the whole new batch and recycle its block. |
| RX stream buffer | 8 KiB | UsbTask / CmdTask | Keep the current staged data and retry when no space is available. |
| USB RX staging ring | 16 entries, up to 512 bytes each | Endpoint service / UsbTask | Drop a new entry and increment `rx_overflows` if the staging ring is full. |
| USB TX frame pool | 16 slots, 4160 bytes each | Frame enqueue / USB submission and completion | A static counting semaphore bounds slot ownership. Default/control wait is 500 ms; live event/overrun frames use a 10 ms bound and count failure. |

Short critical sections protect shared segment-release/repost operations and
USB TX slot reservation. The counting semaphore measures free USB TX slots;
the ready-index FIFO preserves frame order. The shared frame sequence counter
uses an atomic fetch-and-add. Queues safely accept multiple decoder producers;
an unsynchronized multi-writer message buffer is not used.

TxTask drains at most four event blocks per turn and checks USB pool space.
This bounded work prevents an endless live-event backlog from completely
monopolizing command handling. Overrun counters and events expose loss; they
do not guarantee loss-free operation for arbitrary input rates. A temporarily
empty posted-descriptor set is not by itself declared an error in the current
code: freed segments can be reposted on notification, while a real PL FIFO
overflow follows the stop/report path.

## Interrupt handling and USB-specific bounded work

The PL and acquisition-DMA interrupt handlers clear/acknowledge pending
conditions, update bounded instrumentation and notify AcqTask with
`xTaskNotifyFromISR()`. They do not run protocol decoders or CRC processing.
`portYIELD_FROM_ISR()` requests a reschedule when a more urgent task is woken.

The current USB implementation also has a bounded driver-specific RX staging
operation in endpoint callbacks: pop a completed vendor buffer, copy at most
512 bytes into the sixteen-entry staging ring, release the vendor buffer and
notify UsbTask. This is required by the vendor dispatcher's descriptor-cursor
behavior. The active initialization enables the USB endpoint-dispatch interrupt;
older comments describing all endpoint completions as polled are stale.
UsbTask additionally sweeps for missed completions, processes control requests,
feeds the RX stream buffer and submits TX frames on notification or a 1 ms
service timeout. USB IN completion uses `xSemaphoreGiveFromISR()` to release a
TX slot. Thus, the actual USB path must not be described as literally doing
only notification in every callback or as doing all payload copies in tasks.

**Implementation evidence:** `drivers/la_irq.c`,
`drivers/la_usb_dev.c` (`la_usb_stage_push`, `la_usb_rx_service`,
`la_usb_tx_handler`, `la_usb_dev_init`) and `tasks/la_usb_task.c`.

## Diagnostics and measured evidence

The application enables stack-overflow checking and a malloc-failure hook.
Failure hooks print a diagnostic and halt; they do not silently recover.
StatusTask currently samples the acquisition, command, transmission and status
task watermarks and reports their minimum in stack words. It does not currently
include all decoder and USB task watermarks. Kernel runtime-statistics
generation is disabled, so the existence of a CPU-load field is not evidence
of measured per-task CPU utilization.

Custom trace hooks record task switches and queue/stream activity in a bounded
ring, together with application markers for DMA, segment publication, decoding
and transmission. Trace recording is disabled until requested. This makes the
DMA-to-decoder-to-transmitter scheduling chain observable without claiming that
the trace itself guarantees a deadline.

Recorded results relevant to this design are:

- **19 September 2026, USB drain:** three byte-exact 67,108,864-byte aggregate
  trials in 41.4–41.5 s, each 1.62 MB/s. Source:
  `build/reports/usb-u06-performance.md`.
- **20 September 2026, USB live UART/SSE gate:** 606,411 device events and
  606,411 feed events over 60 s, 606,405 decoded bytes, zero client/format drops,
  zero overrun events and zero dropped segments. Replay-ring evictions were
  history rollover, not active-client loss. Source: `docs/status.md`, V04 entry,
  and `build/reports/usb-v04-live-web.md`.
- **12 September 2026, cached decoder comparison:** about 1.28–1.29 million
  transition words/s for the cached SPI-decoder workload, approximately 28%
  above the non-cached comparison. This was a deliberate overloaded SPI test,
  with dropped segments in both configurations, not a loss-free 1 MHz claim.
  Source: `docs/rtos-design.md`, P8.5 results.
- **Historical isolated-byte gate:** 428 microseconds worst observed over
  twenty single-shot sessions against the 25 ms budget. This earlier
  Zynq/link measurement is not a new USB browser end-to-end latency result.
  Source: `docs/rtos-design.md`, P7.6/P8.2 evidence.

The current code and these dated reports support an explicit RTOS explanation.
They do not establish fresh timing measurements or every task's runtime CPU
load. The human browser/real-board visual walkthrough was author-confirmed on
12 October 2026 and accepted on the current W11 view on 18 October 2026.
