# Logic Analyzer with Live Protocol Decoding

> **Historical Markdown manuscript (12 October 2026).** The current class report
> is the LaTeX source in `latex/` with its compiled Embedded and FPGA PDFs.
> This file is preserved as the earlier Word/Markdown manuscript and is not kept
> in sync with W11, the hardware photograph, the demonstration-video links, or
> the class-specific author variants. Use [status.md](../status.md) for current
> status.

## Project and author information

**Author and sole contributor:** Lingga Aradhana Sahadewa

**Email:** linggaaradhanas@gmail.com

**Classes:** Embedded system and FPGA

**Class-project period:** 15 September–12 October 2026

**Funding:** Korea Chamber of Commerce and Industry (KCCI)

**Documentation status:** Updated on 12 October 2026. This individual project is
used for both classes. The four weekly activity logs record five hours per week,
twenty hours in total for the shared project.

## Abstract

This project implements an eight-channel logic analyzer for 3.3 V digital
signals on a Zybo Z7-10 development board. The FPGA samples at up to 100 MS/s,
evaluates level and edge triggers, retains pre-trigger history, and packs raw
samples or transition records. Ordinary scatter-gather DMA transfers the data
into DDR memory. FreeRTOS on the Zynq Cortex-A9 processor manages acquisition,
commands, three configurable live decoder slots, transmission and USB servicing.
UART, SPI and I2C decoding is available on the device for live operation and on
the host for recorded captures. The current default connects the Zynq directly
to a Windows computer through USB CDC-ACM. A host-served browser interface
provides waveform viewing, live events, decoding, exports and task traces.
Recorded USB trials transferred 64 MiB as sixteen 4 MiB captures at 1.62 MB/s,
with byte-exact results in three trials. A 60-second UART/SSE test delivered
606,411 events without client or segment loss. SD boot was verified on
26 September 2026, and the author confirmed completion of the visual walkthrough.
The report distinguishes FPGA timing, RTOS coordination and transport capacity,
demonstrating the two classes through one integrated instrument.

## Keywords

Logic analyzer; FPGA; FreeRTOS; protocol decoding; USB; embedded systems

## Specifications

| Item | Current implementation |
|---|---|
| Hardware | Digilent Zybo Z7-10, AMD Zynq-7010, onboard 1 GiB DDR3L |
| Input channels | Eight digital channels on Pmod JC; 3.3 V LVCMOS33 only |
| Sample rate | Up to 100 MS/s; rate = 100 MHz / (SAMPLE_DIV + 1) |
| Trigger | Level/edge conditions, pre/post-trigger capture, timeout and abort |
| Pre-trigger configuration | Up to 32,768 samples; the internal ring has 65,536 entries |
| Encoding | Four eight-bit raw samples per 32-bit word, or 24-bit run plus eight-bit sample |
| Capture storage | 8 MiB burst pool; live mode reuses the first 4 MiB as 16 x 256 KiB segments |
| DMA | AXI DMA S2MM, ordinary scatter-gather descriptors; hardware cyclic mode disabled |
| RTOS | FreeRTOS on CPU0, 1 kHz tick, statically allocated application objects |
| Default connection | Direct Zynq PS USB0 CDC-ACM through J10; Windows usbser.sys |
| Protocol decoders | UART, SPI and I2C; three independently configurable live slots |
| Host applications | la-ui browser workspace and la-cli scripted interface |
| Exports | PulseView .sr, VCD, sample/event CSV, PNG and text through the applicable host tools |
| Data integrity | CRC16 per protocol frame; CRC32 and size/order checks per completed capture |
| Estimated USB-build cost | Rp 13,954,700, using retained 17 September 2026 IDR estimates |

## 1. Background and project purpose

Embedded communication faults are difficult to diagnose from software messages
alone. A UART byte can be wrong because of timing or baud configuration; an SPI
transfer can fail because of clock polarity or chip-select timing; an I2C
transaction can fail because an address was not acknowledged. A logic analyzer
records the digital waveforms and associates protocol events with their sample
positions, allowing the signal behavior to be compared with the intended
transaction.

The educational purpose is to combine FPGA design with real-time embedded
software in a working measurement instrument. Commercial instruments such as
the Saleae Logic family and Digilent Digital Discovery provide a useful point
of comparison, while sigrok/PulseView provides a compatible capture-file
ecosystem. This project emphasizes inspectable implementation: custom RTL,
register interfaces, DMA descriptors, RTOS ownership, framed transport and
shared protocol-decoder code.

The earlier project configuration used an ESP32 Wi-Fi gateway. Performance
investigation led to a direct Zynq USB path, which became the default in
September. The gateway remains an optional profile, but it is not needed for
the current instrument or its unified browser interface. The browser runs on
the PC and obtains live events from a PC-hosted bridge.

### 1.1 How the project serves both classes

| Class | Work demonstrated by the sole contributor | Main implementation evidence |
|---|---|---|
| FPGA | Input synchronization, sample-rate division, level/edge triggering, pre-trigger ring, raw/transition packing, packet closure, pattern generation, AXI-Lite registers, simulation and routed timing/resource analysis | fpga/rtl/, fpga/tb/, fpga/scripts/, docs/regmap.yaml, Vivado reports |
| Embedded system | FreeRTOS tasks and scheduling, queues/notifications, DMA-buffer ownership, USB CDC-ACM service, UART/SPI/I2C decoders, framed integrity checks, overload/recovery, host integration and operating documentation | fw-zynq/app/src/, common/la_decode/, common/la_proto/, host/, RTOS and bench reports |
| Shared integration | Hardware-to-software register contract, ordinary SG DMA, capture verification, live-event delivery, SD startup and visual operation | docs/architecture.md, docs/rtos-design.md, docs/status.md and the operator runbook |

Both classes use the same individual project. FPGA logic is responsible for
sample-time behavior; embedded software is responsible for controlled ownership,
decoding, commands and delivery. The two sets of responsibilities are evaluated
through their interface and recorded evidence.

## 2. Hardware and system architecture

![Figure 1. USB-first system architecture](../../build/reports/project-documentation/figures/01-system.png)

*Figure 1. The current default path is JC inputs to FPGA capture, ordinary SG
DMA/DDR, Zynq FreeRTOS, J10 USB and PC tools. Configuration/status travels through
AXI4-Lite. The browser exchanges HTTP control and server-sent events (SSE) with
the host server; it does not connect directly to the FPGA.*

### 2.1 FPGA subsystem

![Figure 2. FPGA sampling and capture pipeline](../../build/reports/project-documentation/figures/02-fpga.png)

*Figure 2. The custom capture logic shares the 100 MHz fabric clock. Sample-rate
division changes the sampling enable. Packing produces 32-bit words, and the
AXI-stream packet boundary tells the DMA when the captured packet is complete.*

`la_input_sync` synchronizes the asynchronous input lines before sampling. The
self-test mux selects the internal pattern generator when requested.
`la_sampler` implements the configured divider. `la_trigger` evaluates masks
and edges, while `la_pretrig_ring` preserves samples before the trigger. The
capture controller manages the pre/post window, abort and overflow response.
The raw packer stores four sample bytes in each word; the transition encoder
stores a run count and value. The AXI-stream FIFO accepts downstream
backpressure, and the output marks the final packet beat with TLAST.

The pattern generator provides counter, walking-one, PRBS8, UART, SPI and I2C
stimulus. Internal self-test exercises the acquisition pipeline; JE-to-JC
external loopback additionally exercises the physical pads and input
synchronizers. These are distinct validation paths.

The AXI-Lite register source is `docs/regmap.yaml`. Its C, SystemVerilog and
Markdown views are generated rather than hand-maintained. The control block is
mapped at 0x40000000 and DMA control at 0x40400000 in the integrated design.

### 2.2 DMA, DDR and operating modes

In burst mode, AcqTask creates a finite descriptor chain, completes the capture
and hands its buffer to the transmitter. The host drains the completed data
after acquisition. In live mode, the FPGA sends transition packets into sixteen
256 KiB segments; completed segments are decoded and returned for reuse. The
default live flush interval is two milliseconds or the configured packet-size
limit, whichever closes the current packet first. Short actual lengths are
valid and are tracked separately from the allocated segment capacity.

Both modes use ordinary scatter-gather descriptors. Live operation reposts
released buffers under software ownership; it does not enable the DMA core's
hardware cyclic mode. DMA descriptors stay non-cacheable. The live payload
region is cacheable in the current build, with explicit invalidation before
CPU readers access memory newly written by DMA.

At 100 MS/s, raw acquisition is 100 MB/s because each sample is one byte.
That is not the sustained PC transfer rate. The current USB drain measurement
is 1.62 MB/s, so full-rate acquisition uses finite buffers. Transition encoding
reduces low-transition captures, but its worst case can expand to four bytes
per sample. Loss-free continuous operation is bounded by both decoding and
transport capacity.

### 2.3 USB and host subsystem

The Zynq PS USB0 controller uses the onboard USB PHY and J10 device connector.
CDC-ACM allows Windows to expose the analyzer as a COM endpoint through
usbser.sys. The USB-profile image does not initialize the gateway UART0 link or
the PL SPI-bulk service. J12 supplies the documented board power/JTAG/console
path; J10 carries analyzer data.

`la-ui` supplies connection controls, captures, waveform viewing, cursors and
edge stepping, host-side recorded-capture decoding, per-slot live control,
exports, offline opening, traces and pattern-generator controls. `la-cli`
provides the corresponding scripted operations. Live events come from the
device decoder tasks; decoding an existing capture uses the host native
decoder library. The browser receives events through SSE. One host connection
owner prevents competing operations from claiming the same COM port.

## 3. Explicit RTOS implementation

### 3.1 Why FreeRTOS is used

FreeRTOS schedules the software pipeline on Zynq CPU0 while the FPGA captures
signals independently. Acquisition management must respond promptly to DMA
completions; decoder tasks must process completed data; the transmitter and USB
task must drain results; command handling must remain responsive to Stop,
Abort and configuration requests. Splitting these activities into tasks makes
their ownership and scheduling explicit rather than combining them in one
unbounded polling loop.

The operating-system tick is one millisecond, not the input sample period. The
FPGA generates sample timing at up to 100 MS/s. FreeRTOS coordinates chunks of
already sampled data, and DMA performs bulk memory writes without requiring
the CPU to copy each input sample.

`main.c` checks the reserved memory geometry, applies MMU/cache attributes,
checks the register interface, calls `la_app_init()` and starts
`vTaskStartScheduler()`. `la_app.c` constructs static task control blocks,
stacks, queues and stream buffers. The BSP enables preemption and time slicing;
the project override sets `configTICK_RATE_HZ` to 1000. Larger priority numbers
mean greater urgency.

### 3.2 Actual application tasks and scheduling

| Task | Priority | Stack | Responsibility and wait mechanism |
|---|---:|---:|---|
| AcqTask / la_acq | 6 | 4 KiB | Own acquisition state, SG descriptors and buffer publication/repost; wait for control and PL/DMA/release notifications |
| CmdTask / la_cmd | 5 | 4 KiB | Parse/validate received frames, route commands and acknowledgements, process lifecycle notices; consume RX stream buffer |
| TxTask / la_tx | 5 | 12 KiB | Pack burst/live frames, manage ACK/retransmission state, recycle event blocks and enqueue USB transmission |
| Decoder slot 0 / la_dec0 | 5 | 4 KiB | Consume its configuration/segment queues, run the configured decoder and publish event blocks |
| Decoder slot 1 / la_dec1 | 5 | 4 KiB | Same independently configurable role for slot 1 |
| Decoder slot 2 / la_dec2 | 5 | 4 KiB | Same independently configurable role for slot 2 |
| UsbTask / la_usb | 5 | 12 KiB | Process USB setup/session work, staged RX and ordered TX; wake by notification or a 1 ms service timeout |
| StatusTask / la_status | 1 | 4 KiB | Maintain the monitored stack-watermark summary on a five-second loop |

There are eight application tasks in the default USB build, plus FreeRTOS idle
and timer tasks. The three decoder slots are not hard-wired one each to UART,
SPI and I2C: any supported configuration can be assigned to a slot, or that slot
can be disabled. Stack constants in `la_app.h` count 32-bit stack elements:
1024 elements are 4 KiB, and 3072 elements are 12 KiB. Idle and timer stacks
are separately 2 KiB each.

AcqTask runs at priority 6 to service acquisition before the priority-5
software consumers. Equal-priority decoder, command, transmitter and USB tasks
share processor time. TxTask was raised from priority 4 to 5 after sustained
decoder production starved the lower-priority consumer. The code also bounds
decoder work to four segments per turn and USB live-event drain to four blocks
per turn, preserving opportunities to process stop and control work.

### 3.3 Live task-to-task data flow

![Figure 3. FreeRTOS live-data and command pipeline](../../build/reports/project-documentation/figures/03-rtos.png)

*Figure 3. AcqTask publishes segment handles to configurable decoder slots.
Decoders publish static event-block pointers to TxTask. USB reception reaches
CmdTask through the RX stream buffer. StatusTask is background monitoring; the
figure does not imply that it receives the live payload.*

The processing sequence is:

1. UsbTask feeds incoming bytes to the RX stream buffer. CmdTask parses and
   validates each command and sends the acquisition/configuration work to
   AcqTask through `ctrl_q`.
2. AcqTask starts the live SG chain. The FPGA emits transition words containing
   a 24-bit run count and an eight-bit sample value.
3. The DMA interrupt acknowledges completion and notifies AcqTask. AcqTask
   collects completed descriptors, checks their actual lengths and invalidates
   the DMA-written cache range before a decoder reads it.
4. AcqTask publishes a handle, slot, length and sequence to each enabled
   decoder's `seg_q`. The large sample payload stays in DDR; the queue does not
   copy the entire segment.
5. Each decoder consumes its borrowed segment, feeds the shared C decoding
   core and packs output into a static event block. A full block or the
   two-millisecond flush check publishes its pointer to `ev_q`.
6. TxTask encodes the event frame, places it in the USB TX pool and returns the
   event block to `block_q`. UsbTask submits ready TX slots in FIFO order;
   endpoint completion returns the slot to the pool.
7. After processing the segment, a decoder releases its reference and
   notifies AcqTask. Only after all consumer references are released may
   AcqTask repost that segment to DMA.

### 3.4 Ownership, synchronization and queue policies

![Figure 4. Segment ownership and safe DMA reuse](../../build/reports/project-documentation/figures/04-ownership.png)

*Figure 4. Hardware ownership and consumer ownership are exclusive. A buffer is
not returned to DMA while any enabled decoder still holds a reference.*

The segment lifecycle is `FREE -> DMA_OWNED -> CPU_OWNED -> FREE`. Each segment
has generation/sequence information to reject stale use and detect gaps.
AcqTask is the sole descriptor owner. Consumer release and repost operations
use short critical sections to maintain the live-ring state. Sequence gaps
reset a decoder and produce a discontinuity indication, preventing missing
samples from being interpreted as a continuous bus transaction.

| Shared object | Capacity | Full/unavailable policy in the code |
|---|---:|---|
| ctrl_q | 4 messages | Zero-wait forwarding; CmdTask logs a full queue. The USB-session abort send does not check its result. |
| notice_q | 8 messages | Zero-wait lifecycle publication; a failed send does not block acquisition. |
| tx_q | 16 small frames | Up to 20 ms wait, then count a TX drop. |
| ack_q | 32 messages | Zero-wait send; failure counts a TX drop. |
| job_q | 1 burst job | Buffer-busy gate limits the capture buffer to one job in flight. |
| cfg_q per slot | 2 messages | Bounded/zero-wait configuration publication; no unbounded configuration wait. |
| seg_q per slot | 8 handles | Drop only that decoder's attempted reference and count the drop; expose later sequence gaps. |
| block_q | 12 static block pointers | No free block drops/counts the new event. |
| ev_q | 8 filled block pointers | Drop/count the complete new batch and recycle its block. |
| USB RX stream buffer | 8 KiB | Retain the current staged input and retry when the stream buffer is full. |
| USB RX staging | 16 entries, up to 512 bytes each | Drop the new staging entry and count overflow when full. |
| USB TX pool | 16 x 4160-byte slots | Static counting semaphore reserves slots; default wait 500 ms, live event/overrun wait 10 ms; failures counted. |

A FreeRTOS queue safely accepts the three decoder producers. A counting
semaphore represents free USB TX slots, short critical sections reserve slots
atomically, and a ready-index FIFO preserves frame order. The shared frame
sequence counter uses atomic fetch-and-add. Zero-copy describes acquisition
payload handoff to decoders; event packing, transport framing and USB staging
still make bounded copies.

For a burst, AcqTask hands a `la_tx_job_t` with the buffer address and length to
TxTask. The buffer-busy gate remains set until the JOB_DONE notification.
TxTask manages DATA_BEGIN, DATA_CHUNK and DATA_END, the ACK window,
retransmissions and whole-capture CRC. A two-second no-progress timeout can
abandon a stalled transfer and release the job instead of retaining the
capture buffer indefinitely.

### 3.5 Interrupt work, cache handling and diagnostics

The PL/DMA handlers in `drivers/la_irq.c` acknowledge pending conditions,
record bounded instrumentation and call `xTaskNotifyFromISR()`.
`portYIELD_FROM_ISR()` requests a context switch when necessary. They do not
perform protocol decoding or CRC processing.

The USB vendor driver has an important bounded-work exception to a simplistic
"all ISRs only notify" description. Its active endpoint RX callback pops the
completed vendor buffer, copies at most 512 bytes into the sixteen-entry staging
ring, releases that buffer and notifies UsbTask. This matches the vendor
descriptor-cursor requirements. UsbTask performs class/session processing,
stream-buffer delivery, missed-completion sweeps and TX submission. USB IN
completion gives the free-slot semaphore with `xSemaphoreGiveFromISR()`.

The application objects are allocated statically with `xTaskCreateStatic()`,
`xQueueCreateStatic()` and related APIs. BSP dynamic-allocation support remains
enabled for its kernel build, but the application task pipeline uses static
storage. The idle/timer memory hooks also supply static stacks. Stack-overflow
and malloc-failure hooks print a fatal diagnostic and halt.

DMA descriptors are non-cacheable. The default live-payload prefix is
write-back cacheable, so AcqTask invalidates each written segment before
publication and TxTask invalidates a completed burst before reading it. The
CPU reads these payloads without writing them. This prevents stale cached
samples from being interpreted as new acquisition data.

StatusTask currently summarizes the watermarks of acquisition, command,
transmission and status tasks, in stack words. It does not include every
decoder/USB watermark. Kernel runtime-statistics generation is disabled; a
CPU-load field alone is not evidence of measured per-task utilization. Custom
trace hooks provide task-switch, queue/stream and DMA/decode/TX markers through
a bounded trace ring that is enabled on request.

### 3.6 RTOS code reference map

| Explanation | Source file or group |
|---|---|
| Scheduler startup, memory checks, static kernel hooks | fw-zynq/app/src/main.c |
| Task creation and static queues/buffers | fw-zynq/app/src/la_app.c |
| Priorities, stack lengths, queue depths | fw-zynq/app/src/la_app.h |
| Tick, static allocation, overflow hooks and trace hooks | fw-zynq/FreeRTOSConfig.h |
| Descriptor ownership, live publication/repost and cache invalidation | fw-zynq/app/src/tasks/la_acq_task.c |
| Segment consumption, event batching, reference release | fw-zynq/app/src/tasks/la_decoder_task.c |
| Protocol dispatch and control routing | fw-zynq/app/src/tasks/la_cmd_task.c |
| Burst ACK window, frame packing and bounded live drain | fw-zynq/app/src/tasks/la_tx_task.c |
| USB class/session service and 1 ms wait | fw-zynq/app/src/tasks/la_usb_task.c |
| IRQ notification and USB staging/semaphore operations | fw-zynq/app/src/drivers/la_irq.c; drivers/la_usb_dev.c |
| Pool/ring lifecycle and protocol state | fw-zynq/app/src/live/; common/la_decode/ |
| Five-second monitoring loop | fw-zynq/app/src/tasks/la_status_task.c |

An expanded implementation review is maintained in
`docs/hardwarex/rtos-implementation.md`. The explanation here follows active
code rather than outdated comments or initial stack/queue proposals.

## 4. Design files and reproducibility

| File group | Purpose |
|---|---|
| fpga/rtl/, fpga/tb/, fpga/constraints/, fpga/scripts/ | Editable RTL, self-checking testbenches, pin constraints and Vivado scripts |
| docs/regmap.yaml; tools/gen_regmap.py | Register-map source and generated interface views |
| fw-zynq/ | Vitis/FreeRTOS application, drivers, task adapters, memory layout and build/load scripts |
| common/la_proto/; common/la_decode/ | Shared C framing/integrity code and protocol decoders |
| host/la_cli/; host/la_ui/; host/la_live_web/ | CLI, browser workspace, file handling and live-event bridge |
| tests/; host/tests/; fw-zynq/tests/ | Portable, host, integration and bench test layers |
| docs/hardwarex/diagrams/project-report_v2.drawio | Four editable current system/FPGA/RTOS/ownership figures |
| docs/architecture.md; docs/rtos-design.md; docs/using-the-analyzer.md; docs/status.md | Architecture, ownership contract, operating sequence and dated verification ledger |
| fw-esp32/ | Retained optional gateway-profile implementation |

The code repository is https://github.com/linggaaradhanas22/logic-analyzer.
The project sources and local build reports distinguish implemented behavior
from measured evidence. Local generated reports are not automatically
available in a clean checkout and must be regenerated for independent
reproduction. The older wireless-first manuscript and its diagrams are
preserved separately as historical material.

### 4.1 Build instructions

The supported development workflow is native Windows. The main tools are
Vivado/Vitis 2026.1, Questa for procedural RTL simulation, Python 3.11 and
MSVC/CMake/Ninja for portable C checks. The optional gateway uses the pinned
native ESP-IDF installation. `python tools/run.py <target>` is the canonical
runner; it resolves the project root from its own script location.

1. Clone the repository, create its `.venv` and install the project package
   using the setup steps in `docs/windows-dev.md`.
2. Check the environment with `python tools/run.py check-env -- --write-lock`.
3. Generate the interfaces with `python tools/run.py gen`.
4. Check the software with `python tools/run.py test-native` and
   `python tools/run.py test-host`.
5. Simulate the RTL with `python tools/run.py sim` and build the bitstream/XSA
   with `python tools/run.py bit`.
6. Build the default USB firmware with `python tools/run.py fw-zynq`.
7. With the documented power/jumper setup, load the design with
   `python tools/run.py jtag-zynq`.
8. For SD startup, run `python tools/run.py boot`, copy BOOT.BIN to the FAT32
   card root, remove power, set JP5 to SD and power-cycle. `package` stages the
   profile-aware firmware/host artifacts and hash manifest.

These are reproduction instructions, not a claim that new FPGA/firmware builds
were run during this documentation refresh. The default firmware is USB;
`--profile gateway` selects the retained gateway application where supported.

## 5. Bill of materials

The prices below retain the existing Indonesian-marketplace estimates dated
17 September 2026. They are reference estimates, not newly quoted prices or
confirmed purchase invoices. The default USB build uses two USB A-to-micro-B
data cables: one for J12 power/JTAG/console and one for J10 analyzer data.
The computer is existing laboratory infrastructure and excluded from the cost.

| ID | Component and current role | Quantity | Unit estimate (IDR) | Total (IDR) |
|---|---|---:|---:|---:|
| A1 | Zybo Z7-10 development board, required | 1 | 13,800,000 | 13,800,000 |
| W1 | USB A-to-micro-B data cable, J12 power/JTAG/console | 1 | 25,750 | 25,750 |
| W2 | USB A-to-micro-B data cable, J10 analyzer data | 1 | 25,750 | 25,750 |
| W3 | Female-female jumper-wire pack, target leads and optional external loopback | 1 | 20,700 | 20,700 |
| M1 | 16 GB class-10 microSD, standalone SD startup | 1 | 82,500 | 82,500 |
| PC1 | Existing Windows host computer, excluded infrastructure | 1 | 0 | 0 |
| PS1 | Optional 5 V supply; documented bench uses J12 USB power | 1 | 37,050 | 37,050 |
| A2 | Optional classic ESP32-WROOM gateway board | 1 | 45,000 | 45,000 |
| AP1 | Optional Wi-Fi access point for gateway profile | 1 | 218,000 | 218,000 |
| A3 | Optional ESP32-S3 board; historical gateway transport work | 1 | 115,000 | 115,000 |
| A4 | Optional 3.3 V USB-UART diagnostic adapter | 1 | 25,000 | 25,000 |
| BB1 | Optional solderless breakout breadboard | 1 | 13,800 | 13,800 |

**Required USB-build subtotal: Rp 13,954,700.** This includes A1, W1, W2, W3
and M1. Optional items total Rp 453,850; all listed items total Rp 14,408,550.
The optional power supply is not included in the required subtotal. KCCI funded
the project; these figures describe the retained estimate basis rather than
asserting how much funding was paid.

### 5.1 Retained material-source references

The original supplier references are retained for traceability:

- A1: https://www.tokopedia.com/invoklab/digilent-zybo-z7-zynq-7000-arm-fpga-soc-development-board-zybo-z7-10-0f6d6
- W1/W2: https://www.tokopedia.com/robot-indonesia/robot-rdm100xs-kabel-data-micro-usb-kabel-charger-handphone-2a-panjang-1-meter-garansi-resmi-1-tahun-bisa-cod-1736307162016286082-1736307173264033154
- W3: https://www.tokopedia.com/reinid/dupont-jumper-wire-female-to-female-kabel-40pcs-20cm-rainbow-cable-1729850275615311272
- M1: https://www.tokopedia.com/v-genindonesia/memory-hp-maestro-microsd-16gb-maestro-class-10-chip-only

Optional-item supplier references remain in the preserved September manuscript.
No custom PCB or enclosure is needed for the documented build. The Zybo Pmod
series resistors are part of the board, not separately purchased components.

## 6. Operation instructions

### 6.1 Connections and startup

| Connection or setting | Current direct-USB configuration |
|---|---|
| J12 | PC USB connection for board power/JTAG/FTDI console |
| J10 | PC USB data connection to the analyzer CDC-ACM endpoint |
| J11 | Unused in device mode |
| JP6 | USB power selection for the documented bench arrangement |
| JP1 / JP2 | Open for the documented J10 device-mode arrangement |
| JP5 | JTAG for development load, or SD for the verified standalone image |
| JC | Eight acquisition inputs, CH0-CH7, plus common ground to the target |
| JE | Pattern-generator outputs for optional external loopback |

1. Remove power before changing jumpers or wiring. Check the board revision,
   target logic voltage and shared ground. Do not connect target VCC.
2. Connect J12 for power and J10 for analyzer data. Load the USB image over
   JTAG, or boot the prepared SD card with JP5 set to SD.
3. Identify J10's USB Serial Device COM port. The recorded bench used COM13;
   another PC can assign a different number. Do not select J12's FTDI console.
4. Start `la-ui` from the project environment, or use `tools/win/la-ui-start.vbs`.
   Open http://127.0.0.1:8080/ in the browser.
5. Rescan/select the analyzer port and Connect. Ping should report firmware
   0x00090000 and PL ID 0x4C410002 for the documented image.
6. Begin with the internal counter self-test before connecting target signals.
   Use the UI runbook's 100 MS/s, raw, zero-pre-trigger configuration. The UI
   scales step-pattern timing so the Fit view shows readable square waves.

### 6.2 Capture, decode and inspect

Use zoom/pan/Fit, channel selection, cursors A/B, edge stepping and pattern
search to inspect recorded waveforms. Capture decoding uses the host native
DLL, built by `test-native`. At 10 MS/s, the pattern-generator recipes are:

| Protocol | Generator mode | Decoder configuration | Expected stimulus |
|---|---|---|---|
| UART | uart | uart:rx=0,baud=115200 | Repeating LA-TEST followed by newline |
| SPI | spi | spi:sck=0,mosi=1,cs=2 | Frames containing 4C 41; the first can be partial |
| I2C | i2c | i2c:scl=3,sda=4 | START, address/write, ACK, 4C 41, STOP |

For a real target, replace generator input with appropriately wired 3.3 V
signals, keep common ground and choose a rate that resolves the signal edges.
Configure trigger/pre/post settings for the region of interest. In live mode,
configure the desired decoder slots and use Start/Stop; each enabled slot has
independent configuration. Decoded-pattern stop acts after the decoder has
processed the relevant segment, not exactly at the physical edge.

### 6.3 Export and shutdown

Export .sr for exact sample-count reopening in PulseView or the UI. VCD stores
transitions and may not preserve a trailing constant run's exact sample count;
the host reports this limitation. Sample CSV contains waveforms, while event
CSV contains decoded annotations. PNG captures the visible waveform region.
Trace controls retrieve the RTOS timeline. Stop live operation, finish/cancel
jobs, disconnect the UI, stop its host server and remove power before rewiring.

All acquisition inputs are 3.3 V only and have no project-added over-voltage
protection. Use an appropriate external level shifter for 5 V logic. Short
leads and common ground are important; synchronization does not guarantee
capture of pulses shorter than the configured sample period.

## 7. Validation and characterization

### 7.1 Recorded verification evidence

The table summarizes existing reports. Counts and dates belong to their
recorded runs; they are not fresh hardware measurements from writing this
report. Historical baseline tests are identified separately from the current
USB results.

| Check | Recorded outcome | Evidence/date |
|---|---|---|
| Procedural RTL regression | 12/12 self-checking Questa cases passed in the recorded regressions | fpga/tb/testlist.json; docs/status.md and simulation reports |
| Routed FPGA resources | 6,675/17,600 LUTs (37.93%); 9,198/35,200 registers (26.13%); 37/60 BRAM tiles (61.67%) | build/release/utilization.rpt, 20 September 2026 |
| Routed timing | Setup WNS +0.576 ns; hold WHS +0.022 ns; no failing constrained endpoints | Routed timing summary, 20 September 2026 |
| Capture matrix | Six generator modes at 1, 10 and 100 MS/s, three captures each; internal and full eight-channel external matrices recorded as 18/18 | Historical P6/P7 matrix entries in docs/status.md |
| USB aggregate drain | Three trials, each 67,108,864 bytes as 16 x 4 MiB captures; 41.4–41.5 s; 1.62 MB/s; byte-exact/CRC-clean | build/reports/usb-u06-performance.md, 19 September 2026 |
| USB UART + SSE live gate | 606,411 device events and feed events in 60 s; 606,405 decoded bytes; 124.7 kB/s wire; zero client/format drops, overrun events or dropped segments | build/reports/usb-v04-live-web.md and V04 ledger entry, 20 September 2026 |
| USB live protocols/recovery | UART, 10 kHz SPI/I2C, stop-on-pattern, compact events, overload recovery and close/reopen gates; 8/8 suite | V04/V06 recorded USB live suites, 20 September 2026 |
| UI real-device integration | 6/6 API/E2E cases; million-sample capture, decoding, per-slot live/SSE, trace/export/offline open | W09 entry, 20 September; SD-boot re-run, 26 September 2026 |
| Cached decoder comparison | Approximately 1.28–1.29 M transition words/s, about 28% faster than non-cached under deliberate SPI overload | docs/rtos-design.md P8.5, 12 September 2026 |
| Historical isolated-byte latency | 428 microseconds worst over 20 single-shot sessions against a 25 ms budget | P7.6/P8.2 evidence; earlier Zynq/link scope |
| Historical capture soak | 500 back-to-back 4096-sample captures, exact results and no FIFO overflow, 232 s | P8.4 recorded soak evidence |
| Software regression | Release precheck 22/22; latest recorded host regression 167 passed | Release/W09 evidence and 1 October 2026 viewer-fix entry |
| USB-profile SD boot | BOOT.BIN cold boot, USB enumeration, ping/status and 6/6 UI E2E cases | build/reports/sd-boot-verification.md, 26 September 2026 |
| Human visual walkthrough | Completed, confirmed directly by the author during this documentation update | Author confirmation received 12 October 2026; execution date and additional measurements not specified |

The routed FPGA report meets its user-specified internal constraints. It still
lists methodology/external-I/O delay warnings, so timing closure is not a
general guarantee of external signal integrity or every possible probe setup.
The sample rate, decoder processing rate and host transfer rate have different
units and measure different stages.

The 64 MiB figure is aggregate transferred data, not capture-buffer capacity.
The cached SPI comparison intentionally overloads the decoder and drops
segments in both variants; it is not a loss-free 1 MHz live-decoding claim.
The historical 428-microsecond result is not a newly measured USB-to-browser
end-to-end latency. Replay-ring eviction removes old history; the zero-client-
drop result refers to the active SSE consumer.

### 7.2 Visual walkthrough status

The author has confirmed that the visual walkthrough is complete. This closes
the previously outstanding human observation item for the project report.
The confirmation is recorded separately from automated browser/API testing;
no extra pass counts, timings, screenshots or individual interaction results
are inferred from it. It was received on 12 October 2026, rather than presented
as an assertion that the walkthrough itself happened on that date.

### 7.3 Capabilities and remaining limitations

- Eight-channel, 3.3 V acquisition is qualified at up to 100 MS/s. The 200 MS/s
  stretch is unverified and is not part of the current class-project claim.
- Burst capacity is bounded by the 8 MiB pool and finite descriptor chain;
  oversized captures are rejected rather than silently shortened.
- UART 115200 and 10 kHz SPI/I2C have recorded live gates. High-rate live
  workloads can overload decoder/event/transport stages and generate explicit
  discontinuity/overrun information.
- Full-rate raw capture is buffered acquisition, not a 100 MB/s continuous
  USB stream. The default host drain measurement is 1.62 MB/s.
- The optional gateway retains historical Wi-Fi/SPI limitations; its remaining
  ESP32-S3 SPI issue and optional tuning are not declared fixed by USB adoption.
- The signed-driver-dependent SUMP bridge is deferred. The final v1.0 tag and
  public release authorization remain separate from demonstrated operation.
- The development USB identity is retained for the current in-house setup;
  no public product identity or certification is asserted.

## 8. Weekly project activity summary

The official class-project period is 15 September–12 October 2026. Some
underlying technical validation predates this period; it is cited as existing
evidence rather than reassigned to a later week. Weekly entries below summarize
dated activity within the period. Five hours per week is the author's instructed
log allocation, not a duration calculated from test-running times.

| Week | Period | Activity and outcome summary | Hours |
|---|---|---|---:|
| 1 | 15–21 September 2026 | Investigated gateway/S3 transport limitations; planned and implemented direct Zynq USB; corrected endpoint toggle/ownership/order handling; measured three 64 MiB USB drain trials; adopted USB default; integrated live/SSE, independent decoder slots and the unified UI with recorded bench gates. | 5 |
| 2 | 22–28 September 2026 | Updated the UI-first operator runbook and wiring/startup instructions; added hidden Windows start/stop launchers; verified USB-profile cold SD boot and the 6/6 UI E2E suite. | 5 |
| 3 | 29 September–5 October 2026 | Corrected counter Fit legibility, orthogonal waveform rendering and exact cursor edge stepping; ran the recorded 167-test host regression and offline browser checks. | 5 |
| 4 | 6–12 October 2026 | Prepared class-specific presentation routes and a complete speaking script; reconciled current project/RTOS report content and weekly KCCI forms; recorded the author's confirmation of the completed visual walkthrough. | 5 |
| Total | Shared individual project for both classes | Four weekly logs; one twenty-hour allocation | 20 |

Detailed weekly activity logs are provided on four separate sheets in the
accompanying KCCI workbook. Unrequested administrative fields are left blank,
and the original Project Scoring Sheet is preserved without modification.

## 9. Conclusion

The project demonstrates an integrated FPGA and embedded-system instrument:
the FPGA supplies deterministic sampling/trigger/packing, while FreeRTOS
provides explicit scheduling, ownership, live decoding and transport service.
The default USB path replaces the older headline Wi-Fi limitation with a
recorded 1.62 MB/s aggregate drain result. Device live events, host waveform
analysis, independent decoder-slot control, exports, SD startup and the
author-confirmed visual walkthrough complete the documented operating path.

The principal lesson is that sample rate alone does not define an analyzer's
end-to-end capacity. Buffers, descriptor lifetime, decoder throughput,
transport pacing and honest overload reporting must also be designed and
verified. The report shows both classes' contributions through this shared
hardware/software contract while retaining the limits of the measured evidence.

## Author contribution

Lingga Aradhana Sahadewa is the sole author and contributor for both classes:
project design, FPGA and embedded implementation, integration, validation,
visualization and documentation. No additional author or team member is listed.

## Funding and acknowledgment

This project was funded by the Korea Chamber of Commerce and Industry (KCCI).
The author acknowledges KCCI's support for the project used in the Embedded
system and FPGA classes.

## References

1. Project code, specifications and dated execution ledger:
   https://github.com/linggaaradhanas22/logic-analyzer; docs/status.md,
   docs/architecture.md, docs/rtos-design.md and docs/using-the-analyzer.md.
2. Digilent, Zybo Z7 reference documentation:
   https://digilent.com/reference/programmable-logic/zybo-z7/start.
3. AMD, Vivado/Vitis and AXI DMA documentation: https://docs.amd.com/;
   the actual routed utilization/timing reports dated 20 September 2026.
4. FreeRTOS kernel and scheduling documentation: https://www.freertos.org/;
   the project's configuration and source files listed in section 3.6.
5. sigrok/PulseView signal-analysis ecosystem: https://sigrok.org/.
6. Saleae logic analyzers: https://www.saleae.com/; Digilent Digital Discovery:
   https://digilent.com/shop/digital-discovery-portable-logic-analyzer-and-pattern-generator/.

External links identify reference material. Technical implementation and
performance statements in this report are grounded in the local source and
dated evidence listed above.
