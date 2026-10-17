# Presentation Script — RTOS Logic Analyzer

This script explains every slide of the presentation in plain English. Read the text after **Say:** out loud. Text in square brackets is a pointing cue for you, not something to read aloud. Text after **If someone asks:** is extra material for questions.

The deck has 37 slides. There are two independent routes:

| Route | Slides to show | Slides you skip |
|---|---|---|
| FPGA system | 1–18, then 34–37 | 19–33 |
| Embedded system | 1–6, then 19–37 | 7–18 |

Slides 1–6 are shared. Slides 7–18 are the FPGA system. Slides 19–33 are the embedded system. Slides 34–37 are the shared results and conclusion. In PowerPoint, open **Slide Show → Custom Slide Show** and choose the route; PowerPoint skips the other subsystem automatically.

A short term sheet is at the end of this document. Look there first if any acronym is unclear.

---

## Slide 1 — A Zynq-based logic analyzer makes digital timing visible

**Route:** Shared.

**On the slide:** The cover. The right side shows a digital waveform, three connected blocks (FPGA → Firmware → PC), and the line "8 channels / 100 MS/s / USB-first".

**Say:**

> Good morning everyone. My name is Lingga Aradhana Sahadewa. Today I will present my final project: an RTOS logic analyzer built on a Zybo Z7-10 board.
>
> A logic analyzer records digital signals as they change over time, so we can see what actually happened on the wires. Software logs tell us what the program did, but they cannot tell us what the signals did.
>
> This project covers the whole path. The FPGA captures the signals. Firmware on the ARM processor manages the captured data and decodes communication protocols. The PC shows the results in a browser window.
>
> Three numbers summarize the device. It captures 8 channels at up to 100 million samples per second. It connects directly to a PC over USB. RTOS stands for real-time operating system, and FPGA stands for field-programmable gate array. Both are central to this project, and I will explain both as we go.
>
> [Point at the three blocks.] The FPGA captures timing. Firmware gives the data meaning. The PC lets us inspect it.

---

## Slide 2 — Choose the subsystem after the shared overview

**Route:** Shared.

**On the slide:** A four-row table listing the sections of the deck and their PowerPoint slide numbers.

**Say:**

> Before we start, here is the map of the presentation. We begin with a shared overview on slides 1 to 6. That part explains the problem, the device, and how the hardware and software work together.
>
> After that, the presentation splits into two independent parts. The FPGA system section runs from slide 7 to 18. It explains how the capture hardware is designed. The embedded system section runs from slide 19 to 33. It explains how the firmware works.
>
> The two sections are independent on purpose. In each section, the other subsystem is treated as a black box, which simply means we agree on its interface and do not look inside it.
>
> The deck finishes with shared results and conclusions on slides 34 to 37.
>
> [Point at the table rows.] Pick the row for the subsystem you are presenting, and follow that numbering through the deck.

---

## Slide 3 — Software logs do not show everything that happened on the wires

**Route:** Shared.

**On the slide:** A table with four rows: Signal timing, Occasional failures, Protocol errors, and Fast signals. Each row names a debugging problem, what logs can miss, and how the analyzer helps.

**Say:**

> This project starts from a simple problem: software logs do not show everything that happens on the wires.
>
> [Point at row 1.] First, signal timing. Logs record what the program did, not the order of clock and data changes on the pins. The analyzer samples all channels together, so we can compare their timing. At the maximum rate, samples are 10 nanoseconds apart.
>
> [Point at row 2.] Second, occasional failures. A bug that appears once an hour is almost impossible to reproduce by hand. The analyzer saves samples from before and after a trigger, so we can see what the signals did just before the failure.
>
> [Point at row 3.] Third, protocol errors. A log might say "message rejected", but not show a bad frame, a missing acknowledgment, or a wrong byte. The analyzer decodes UART, SPI, and I²C messages with sample timestamps, so we can see the actual bytes.
>
> [Point at row 4.] Fourth, fast signals. Software can miss a short pulse between two observations. The analyzer captures in hardware, so sampling does not depend on the software schedule.
>
> One honest limit: this device records digital high and low states. It does not measure analog voltage or signal shape like an oscilloscope.

---

## Slide 4 — The Zybo captures signals and sends results straight to the PC

**Route:** Shared.

**On the slide:** A physical wiring diagram. A target board connects to the Zybo through the JC connector. The Zybo connects to a Windows PC through J10 USB. A separate J12 box shows power, JTAG, and console.

**Say:**

> Here is the final physical system.
>
> [Point at the left box.] The target board is the device we want to debug. Its signals come into the analyzer through the JC connector. The signals must be 3.3 volts, and the boards must share a common ground. This design never connects 5-volt signals directly.
>
> [Point at the center box.] The analyzer itself is a Zybo Z7-10 board. It contains a Zynq chip with two parts: an FPGA for hardware logic and an ARM processor for software.
>
> [Point at the right box.] The Zybo connects to a Windows PC through the J10 connector using direct USB. The PC runs the analyzer server and shows the results in a browser.
>
> [Point at the J12 box.] J12 is a separate connector for board power, programming, and the console. The console port is useful during development, but it is not the analyzer data connection.
>
> The FPGA samples 8 channels at up to 100 million samples per second. You can record a triggered capture, or watch decoded messages in live mode. The PC displays waveforms and live messages, and it can save captures as files.

---

## Slide 5 — The Zynq captures data in hardware and controls it in software

**Route:** Shared.

**On the slide:** The end-to-end architecture. Digital inputs feed the FPGA, then AXI DMA, then DDR memory, then the ARM firmware, then the PC host. A dashed arrow shows register control from firmware back to the FPGA.

**Say:**

> This slide shows how data moves through the whole system. Let us follow the solid arrows.
>
> [Point at the inputs.] Signals enter from the left and are sampled by the FPGA. The FPGA can also detect a trigger and encode the samples.
>
> [Follow the arrow to AXI DMA.] The sampled stream goes to AXI DMA. DMA means direct memory access. It copies data to memory without the CPU copying every sample by hand.
>
> [Follow the arrow to DDR.] The samples land in DDR memory, which is the main working memory of the board. Cached and non-cached areas are used for different purposes, and I will explain that in the firmware section.
>
> [Follow the arrow to ARM / PS firmware.] The ARM processor runs FreeRTOS and reads the samples. It manages buffers, decodes protocols, and prepares messages for the PC.
>
> [Follow the arrow to the PC host.] Finally, the data travels over USB to the PC, where the browser shows waveforms and decoded messages.
>
> [Point at the dashed arrow.] The dashed line shows control in the opposite direction. Settings such as sample rate and trigger condition travel from the PC, through firmware, to the FPGA's registers. Data flows one way; commands flow the other way.
>
> Two short terms: PL means programmable logic, the FPGA side; PS means processing system, the ARM side. They are two parts of the same Zynq chip.

**If someone asks:** The ARM never polls one GPIO pin per sample. That would be far too slow and jittery. The FPGA does the time-critical work, and the CPU handles memory, decoding, and communication.

---

## Slide 6 — DMA hardware and its firmware driver have different jobs

**Route:** Shared. This is the key slide for understanding the split.

**On the slide:** A five-row table comparing FPGA/hardware and ARM/firmware responsibilities: Capture signals, Represent samples, Move samples, Use buffers, and Talk to the PC.

**Say:**

> This slide separates the two worlds, and it is the most important preparation for both sections.
>
> [Point at row 1.] Capturing signals is hardware work. The FPGA logic samples inputs and detects triggers. Firmware only chooses the settings and starts or stops a capture.
>
> [Point at row 2.] Representing samples is shared. The FPGA packs raw samples or stores repeated values as runs. The firmware reads that format and decodes protocols.
>
> [Point at row 3.] Moving samples is also shared, and the wording matters. The AXI DMA block is hardware inside the FPGA design. The DMA driver and its descriptors are software. When I say "DMA hardware" I mean the block; when I say "DMA driver" I mean the firmware that controls it.
>
> [Point at row 4.] Using buffers is firmware work. The hardware writes only to the buffers it has been given. Firmware tracks who is reading each buffer and decides when it can be reused.
>
> [Point at row 5.] Talking to the PC is firmware work as well. USB is a peripheral of the ARM processor, not an FPGA capture module. The FreeRTOS tasks build USB messages, handle commands, and stream live events.
>
> One thing this slide also tells us: the UART, SPI, and I²C decoders are C firmware, not FPGA logic. The FPGA captures timing; the firmware explains it.

---

## Slide 7 — The FPGA system takes settings from a firmware black box

**Route:** FPGA system only. In the embedded route, skip.

**On the slide:** A boundary diagram. Inputs and the FPGA RTL + DMA IP are on the left. The PS + firmware is a gray "BLACK BOX" on the right. Dashed arrows show settings going in and interrupts coming out.

**Say:**

> From here, I will focus on the FPGA system. The gray block on the right is the black box for this section: the ARM, its firmware, and the PC. I will not open that box until the results.
>
> [Point at the left.] These are the parts I will explain: the capture logic on the JC inputs and the DMA hardware that moves data.
>
> [Point at the arrows.] The interface is small. Firmware writes settings into the capture registers and the DMA descriptors. The hardware sends samples to memory and raises an interrupt when something needs attention. IRQ simply means interrupt request.
>
> [Point at the bottom text box.] So the whole boundary is: settings go in; samples, status, and interrupts come out.
>
> The assumption for this section is that firmware prepares valid settings and buffer memory before arming the FPGA, and that the rest of the system stores and displays whatever we produce.

---

## Slide 8 — The FPGA samples, triggers and packages data for DMA

**Route:** FPGA system.

**On the slide:** The capture pipeline. Solid boxes: Synchronizer → Sampler → History ring → Raw/run packer → AXIS FIFO → DMA. Trigger logic and the capture controller are shown as side blocks with dashed control arrows.

**Say:**

> This is the sample path I designed.
>
> [Follow the solid arrows along the top.] Input pins first pass a synchronizer. The sampler then reads all enabled channels together at the chosen sample rate. The history ring keeps the most recent samples in order. The raw packer or run encoder converts the samples into 32-bit words. The AXIS FIFO buffers those words, and DMA takes them to memory.
>
> [Point at the Trigger logic block and its dashed arrows.] The trigger is a side observer, not a stage in the data path. It watches the sampled values and sends a match signal when the condition is met. The samples themselves never pass through the trigger.
>
> [Point at the Capture controller block.] The controller coordinates everything. It starts and stops the ring, counts samples, asks the packer to close a packet, and reports status and interrupts.
>
> Firmware only supplies settings through registers. It does not sample each pin in software.
>
> One limit to remember: the FPGA cannot pause the real-world signals when the downstream logic is busy. The FIFO gives us a short buffer, so if the system falls behind, the design reports an overflow instead of pretending the data is complete.

---

## Slide 9 — The sampler reads every enabled channel on the same clock tick

**Route:** FPGA system.

**On the slide:** A diagram of the input path: JC input → Flip-flop 1 → Flip-flop 2 → Sampler with channel mask. Below it, the sample rate formula and divider examples.

**Say:**

> Let us look at the input stage more closely.
>
> [Point at the JC input.] The signals from the target board are asynchronous. They can change at any time relative to the FPGA clock.
>
> [Point at the two flip-flops.] Two flip-flops in series synchronize each input to the 100 MHz clock. They reduce the risk of metastability, which happens when a signal changes exactly at a clock edge and the circuit reads an unstable value.
>
> [Point at the sampler block.] The sampler then applies the channel mask. Disabled channels read as zero, so they never create activity. All enabled channels are captured as one 8-bit sample on the same clock tick.
>
> [Point at the formula.] The sample rate is 100 MHz divided by the divider value plus one. A divider of zero gives the full 100 million samples per second. A divider of nine gives 10 million. A divider of ninety-nine gives 1 million.
>
> The divider does not create a second clock. It creates an enable pulse, so the whole design stays in one clock domain.
>
> Two honest limits. A 10-nanosecond sample interval cannot see a pulse shorter than that. And a synchronizer reduces risk; it is not input protection and it cannot guarantee to catch every short glitch.

---

## Slide 10 — A triggered capture includes samples from before and after the event

**Route:** FPGA system.

**On the slide:** A waveform split into pre-trigger history (gray) and post-trigger samples (pale blue), with the trigger point marked by a dashed line. Below the waveform are the trigger equations.

**Say:**

> A logic analyzer is most useful when it can show what happened before an event, not just after it. That is the job of this slide.
>
> [Point at the gray area.] The history ring keeps rolling while the FPGA is armed. It always holds the most recent samples. Up to 32,768 pre-trigger samples can be requested.
>
> [Point at the dashed line.] When the trigger condition is met, the FPGA marks the first matching sample. This is where the split between "before" and "after" happens.
>
> [Point at the pale blue area.] The capture then continues for the requested number of post-trigger samples and stops. The history ring replays the stored samples in the correct order, so the seam between before and after is exact, with no repeated or missing sample.
>
> [Point at the trigger line.] The trigger itself is a combination of three conditions. The level must match the programmed value. A rising-edge condition and a falling-edge condition can be added. If an edge mask is non-empty, at least one selected channel must show that edge. If the mask is empty, that edge is not required.
>
> The trigger is computed on sampled values inside the FPGA. It is a hardware trigger, not a software decision.

**If someone asks:** The trigger sample counts as the first post-trigger sample. If you request zero pre-trigger samples, the capture starts at the trigger sample itself.

---

## Slide 11 — The capture controller decides when to wait, record and close a packet

**Route:** FPGA system.

**On the slide:** The state machine: IDLE → ARMED → CAPT → DRAIN → back to IDLE. Notes below explain abort, timeout, overflow, and TKEEP/TLAST.

**Say:**

> The capture controller is the part that sequences everything. It is a hardware state machine with four real states.
>
> [Point at IDLE.] IDLE means no capture is active. The controller waits here for an ARM command.
>
> [Point at ARMED.] After ARM, the controller waits for the trigger while the history ring rolls.
>
> [Point at CAPT.] When the trigger fires, the controller counts the samples it needs to emit: the retained history plus the requested post-trigger samples.
>
> [Point at DRAIN.] After the last sample, DRAIN waits until the closing word has left the pipeline. Then the controller reports DONE and returns to IDLE.
>
> Three other behaviors to note. An abort or a trigger timeout returns directly to IDLE. An overflow during capture stops production and drains whatever was already captured. And live streaming is different: in live mode, ARM enters CAPT immediately, because there is no waiting for a trigger.
>
> [Point at the bottom line.] The stream uses two packet markers: TKEEP, which marks valid bytes in the last word, and TLAST, which marks the end of a DMA packet.
>
> DONE is an event and a status bit, not a fifth state.

---

## Slide 12 — Repeated samples can be stored as runs to save space

**Route:** FPGA system here. The embedded route gets a shorter version on slide 25.

**On the slide:** Eight sample values: five times `03`, then three times `07`. Below, the same data in raw packing and in run encoding. The bottom line shows the 32-bit run word layout.

**Say:**

> Digital signals often hold the same value for many samples in a row. This slide shows how the design can use that property.
>
> [Point at the sample values.] Here we have eight samples: five at value 03, then three at value 07. Remember that the value is the whole 8-channel vector, not one channel.
>
> [Point at raw packing.] In raw mode, each sample costs one byte. Eight samples cost eight bytes, which is two 32-bit words. Raw mode is simple and always works, but it uses a fixed amount of space.
>
> [Point at run encoding.] In run mode, we store one word for each group of equal samples. The first word says: five samples with value 03. The second says: three samples with value 07. One run word contains a 24-bit count and an 8-bit sample value.
>
> [Point at the bottom lines.] In this small example, both encodings cost eight bytes, so there is no saving yet. Run encoding starts to save space when runs are longer than four samples on average.
>
> There is a risk in the other direction too. If the signal changes on every sample, run encoding costs four bytes per sample, which is four times larger than raw. So the format is a trade-off, and the firmware always knows which format was used.

**If someone asks:** The decoder treats a run as a count of samples at one value. It advances the sample timeline by the count. Splitting a run into two words does not create a false signal edge.

---

## Slide 13 — AXI-Stream can wait, but the real input signals cannot

**Route:** FPGA system.

**On the slide:** A table of the stream signals: TVALID + TREADY, TKEEP, TLAST, the 8 KiB FIFO, and the overflow flag.

**Say:**

> This slide explains the rules of the stream that feeds DMA.
>
> [Point at row 1.] AXI-Stream uses a handshake. A word moves only when TVALID and TREADY are both high. If the receiver is busy, it lowers TREADY, and the sender must hold the word steady.
>
> [Point at row 2.] TKEEP marks which bytes are valid in the last word. A raw word normally holds four samples, but the final word can hold fewer.
>
> [Point at row 3.] TLAST marks the end of a DMA packet. The design must close word-aligned captures as well as partial tails, or DMA would never see the end of the packet.
>
> [Point at row 4.] The FIFO absorbs short delays. It holds 8 kibibytes, which is about 82 microseconds of raw data at 100 megabytes per second. After that, the real-world signals cannot be paused. At the worst-case encoded rate, the budget shrinks to about 20 microseconds.
>
> [Point at row 5.] So if the system falls behind, the overflow flag records it, and the design stops and reports the loss. It never hides a gap or pretends the samples are continuous.
>
> Backpressure can stall stored words, but it cannot stall the signals on the target board.

---

## Slide 14 — AXI-Lite registers let firmware control the RTL without changing it

**Route:** FPGA system.

**On the slide:** A table of register groups: Acquisition settings, Trigger settings, CTRL commands, STATUS / IRQ, and AXI-Lite transfers.

**Say:**

> Firmware controls the FPGA through a register block. This slide shows what that block contains.
>
> [Point at row 1.] The acquisition settings choose the sample divider, the enabled channels, the encoding, and the pre- and post-trigger counts. Firmware writes a complete, valid setup before it arms.
>
> [Point at row 2.] The trigger settings hold the level mask, the level value, and the rising and falling edge masks.
>
> [Point at row 3.] The control register takes the ARM and ABORT commands and selects modes. Command pulses clear themselves after one clock, so a single write starts exactly one capture. Mode bits stay stored.
>
> [Point at row 4.] The status register reports busy, trigger, done, and overflow conditions, plus word counters. Interrupt bits are cleared by writing a one to them. The block also has a fixed ID value, 4C410002, so software can check that it is talking to the right hardware.
>
> [Point at row 5.] Finally, the register block obeys the AXI-Lite protocol itself: it accepts address and data independently and finishes every response, even when they arrive in different cycles.
>
> This is what "control without changing the RTL" means. Firmware changes behavior by writing registers. The hardware logic itself stays the same.

---

## Slide 15 — Vivado connects the capture RTL to DMA and the PS memory port

**Route:** FPGA system.

**On the slide:** The hardware block diagram: la_top → AXI DMA → sc_dma → PS (HP0, DDR) on the data path; PS GP0 → sc_lite → la_top and DMA on the control path.

**Say:**

> This is how the blocks are connected inside the FPGA design.
>
> [Follow the top path.] The capture logic, called la_top, produces a 32-bit stream. The AXI DMA block converts that stream into memory writes and sends them through the sc_dma interconnect to the processor's high-performance port, called HP0. From there the samples go into DDR memory.
>
> This DMA uses ordinary scatter-gather descriptors. Scatter-gather means DMA reads a list of buffer addresses instead of one fixed target, so a capture can span many buffers. This design uses finite descriptor lists. It does not use cyclic DMA and it does not use simple one-shot transfers.
>
> [Follow the bottom path.] The PS also needs to reach the registers. The general-purpose port, GP0, goes through a second interconnect called sc_lite. From there it reaches both the capture registers and the DMA registers. That is why there are two separate interconnects: control traffic and memory traffic have different requirements.
>
> Everything in the fabric runs on one 100 MHz clock, and the reset is released synchronously, so every block starts in a known state.

**If someone asks:** S2MM means "stream to memory-mapped". The stream side is 32 bits; the memory side is 64 bits. The DMA's status stream is disabled because the design does not provide one, and leaving it enabled caused a stall during bring-up.

---

## Slide 16 — Self-checking tests check sample order as well as error handling

**Route:** FPGA system.

**On the slide:** A table of test areas: Sampling and trigger, History and encoding, Stalls and overflow, Full acquisition path, and Wired loopback.

**Say:**

> I did not only build the logic; I verified it. Each block has a self-checking simulation test, and this slide summarizes the layers.
>
> [Point at row 1.] The sampling and trigger tests check the divider settings, the masks, and every edge and level combination.
>
> [Point at row 2.] The history and encoding tests check the ring wrap, the exact pre/post seam, the run counts, and the raw byte tails. The testbenches compare every output against an expected result, so a wrong sample order fails the test.
>
> [Point at row 3.] The stall and overflow tests hold the stream receiver busy, fill the buffers, and check that the design reports the problem instead of losing data silently.
>
> [Point at row 4.] The full acquisition test sets up the registers and feeds the design with built-in test patterns, then reconstructs the samples and checks them.
>
> [Point at row 5.] Finally, the wired loopback test sends patterns out of the board's JE connector and reads them back through the JC inputs. Eighteen out of eighteen cases passed at 1, 10, and 100 million samples per second.
>
> In total, twelve of twelve self-checking simulation cases pass, or 12/12, and every block has its own test.
>
> The pattern generator used for these tests is test hardware. The real protocol decoders are firmware, and they come later in the presentation.

---

## Slide 17 — The FPGA design fits the Zybo and meets timing at 100 MHz

**Route:** FPGA system.

**On the slide:** Two horizontal bars: lookup tables at 37.93 percent (6,675 of 17,600) and block RAM at 61.67 percent (37 of 60 tiles). The right side lists the timing numbers.

**Say:**

> These are the results from the actual routed design. Routing is the final step where the tools place the logic on the chip and connect it.
>
> [Point at the first bar.] The design uses about 38 percent of the available lookup tables. Lookup tables are the small logic elements that implement digital functions.
>
> [Point at the second bar.] It uses about 62 percent of the block RAM tiles. Block RAM is the dedicated on-chip memory, and our history ring and FIFO use it. That is the highest utilization in the design, but it still fits.
>
> [Point at the right column.] The design meets timing at a 10-nanosecond clock period, which is 100 megahertz. The worst setup slack is plus 0.576 nanoseconds and the worst hold slack is plus 0.022 nanoseconds. Positive slack means the signals arrive with time to spare. There are zero failing paths, and the design-rule checks reported no errors.
>
> To be clear about scope: these results support the 100-megahertz design. The 200-megahertz version mentioned in the project plan has not been built or tested, so I do not claim it.

---

## Slide 18 — The FPGA delivers ordered sample packets, not decoded protocol messages

**Route:** FPGA system. The route continues at slide 34.

**On the slide:** A table of the hardware contribution: Synchronous capture, Triggered history, Stream packaging, and Implementation proof. A link at the bottom points to slide 34.

**Say:**

> This closes the FPGA part of the talk. Let me summarize the boundary one more time.
>
> The FPGA contributes two things. First, synchronous capture: eight channels sampled together at the selected rate. Second, triggered history: an ordered window of samples from before and after the trigger.
>
> It packages those samples into defined packets with valid-byte flags and packet endings, and it reports overflow when it cannot keep up.
>
> What the FPGA does not do is decode protocols. It does not know what a UART byte or an I²C address is. It captures and packages timing. Decoding is firmware work.
>
> The evidence for this section is twelve out of twelve simulation tests, the wired loopback tests, and the timing-closed bitstream.
>
> In the FPGA route, we now jump to the shared results on slide 34, where we will see the final user interface, the operating limits, and the conclusion.

---

## Slide 19 — The embedded system treats capture hardware as a black box

**Route:** Embedded system only. In the FPGA route, skip.

**On the slide:** A boundary diagram. Capture hardware is a gray black box on the left. The ARM/FreeRTOS block is in focus. The PC client is a black box on the right. Dashed arrows show command and setup paths.

**Say:**

> From here, I will focus on the embedded system: the firmware that runs on the ARM processor.
>
> [Point at the left.] For this section, the capture hardware is a black box. We agree on its contract: firmware writes settings into registers, prepares memory buffers, and starts a capture. The hardware then writes sample bytes into memory, reports how many bytes it wrote, and raises interrupts when something finishes.
>
> [Point at the center.] This is what I will explain: how firmware owns those buffers, decodes the sample stream into protocol messages, and sends the results over USB.
>
> [Point at the right.] The PC is also a black box. It sends commands and acknowledgments, and it receives and displays the results.
>
> The inputs to firmware are completed lengths, sample bytes, status, and interrupts. The outputs are settings, descriptors, USB messages, and error reports.
>
> Everything from here on runs under FreeRTOS on CPU0. FreeRTOS is a real-time operating system: it schedules multiple tasks and makes sure the most important work runs first.

---

## Slide 20 — FreeRTOS gives acquisition priority and keeps commands responsive

**Route:** Embedded system.

**On the slide:** A table of tasks and priorities: AcqTask level 6; Decoder, Cmd, Tx, and Usb tasks level 5; StatusTask level 1. The footer note says: higher number equals higher priority, and all tasks run on CPU0.

**Say:**

> This is the task design. FreeRTOS runs several tasks, and each one has a clear job and a priority.
>
> [Point at the note.] Two rules first. A higher number means a higher priority, and all of these tasks run on CPU0 in the ARM processor.
>
> [Point at row 1.] AcqTask has priority 6, the highest. It owns the DMA, the capture registers, and the buffer handoffs. It wakes on hardware and DMA notifications and never waits on a decoder.
>
> [Point at row 2.] The decoder tasks have priority 5. They decode UART, SPI, and I²C from captured samples. They pass buffer handles along and count any dropped events instead of blocking.
>
> [Point at rows 3 and 4.] CmdTask checks incoming commands and routes them to the right task. TxTask sends capture data, events, and command responses. It limits how much event work it does per turn, so commands still get time to run.
>
> [Point at row 5.] UsbTask handles USB requests, receives data, and submits transfers. It sends frames in order and uses fixed-size queues.
>
> [Point at row 6.] StatusTask runs at low priority. It checks the watchdog, collects counters and stack usage, and reports every 500 milliseconds.
>
> One design story worth mentioning: TxTask originally ran below the decoders. Under heavy load it never got time to run, so events were produced but never sent. The fix was to raise it to the same level as the decoders and to bound its work per turn. That is a real lesson in real-time scheduling.

---

## Slide 21 — Firmware initializes the drivers before FreeRTOS starts the tasks

**Route:** Embedded system.

**On the slide:** A four-stage flow: Boot and platform setup → Create the application → Connect interrupt handlers, then start the scheduler → Do the longer work in tasks.

**Say:**

> This slide shows how the firmware starts up.
>
> [Point at stage 1.] The first-stage boot loader loads the FPGA bitstream and starts the ARM application. That is a platform service we rely on.
>
> [Point at stage 2.] The application then sets up its memory mapping, creates its tasks and queues with static storage, probes the FPGA registers to confirm it is talking to the right hardware, and initializes the drivers.
>
> [Point at stage 3.] Next, the interrupt handlers are connected and the scheduler is started. When the FPGA or DMA raises an interrupt, the handler acknowledges the source so it stops firing, and then notifies AcqTask. The handler does not decode anything or move payloads by itself.
>
> [Point at stage 4.] The longer work happens in the tasks: AcqTask collects DMA completions, CmdTask parses commands, and UsbTask services USB requests and transfer queues.
>
> This split is deliberate. Interrupt handlers must be short. If an interrupt handler decodes protocols or copies large buffers, it blocks everything else. So the handlers notify, and the tasks do the real work.

**If someone asks:** All tasks use static allocation. Stack checks and a periodic status report help catch memory problems before they become failures.

---

## Slide 22 — Commands are checked before they reach the acquisition hardware

**Route:** Embedded system.

**On the slide:** A diagram: UsbTask (RX bytes → rx_sb) → CmdTask (parse/validate) → ctrl_q (capture request) → AcqTask (apply settings/DMA, ARM or stop). TxTask handles ACK/NAK/STATUS and chunk acknowledgments.

**Say:**

> This slide follows one command from the USB port to the hardware.
>
> [Point at UsbTask.] Received bytes arrive from USB and go into a receive stream buffer. The label rx_sb is short for receive-byte stream buffer.
>
> [Point at CmdTask.] CmdTask reads that byte stream and parses it into complete messages. Every message is checked: its length, its fields, and the current state of the device.
>
> [Point at the three text lines.] There are three main commands. CONFIG carries a complete acquisition setup and is validated first. ARM starts the capture using the validated setup. ABORT stops the capture and returns the device to idle.
>
> [Point at ctrl_q and AcqTask.] Accepted commands go through a small queue to AcqTask, which owns the DMA and the capture registers. Other tasks never edit descriptors directly. They send requests.
>
> [Point at TxTask.] On the response side, TxTask sends acknowledgments, errors, and status messages. During a burst capture, it also tracks the acknowledgments for each data chunk, because the PC confirms chunks as they arrive.
>
> Two rules keep this safe. A bad request is rejected with a NAK, not acted on. And a capture request larger than the 8-mebibyte capture buffer is rejected before the hardware is armed.

---

## Slide 23 — DMA writes samples to memory; firmware controls when buffers are reused

**Route:** Embedded system.

**On the slide:** A diagram of the hardware APIs: la_top registers at 0x40000000, DMA registers at 0x40400000, DDR buffers, and AcqTask as the descriptor owner. The right side lists descriptor and cache rules.

**Say:**

> This slide shows how the firmware actually drives the DMA hardware.
>
> [Point at the three boxes on top.] The capture block and the DMA block are controlled through registers at two fixed addresses. AcqTask writes the settings, then writes the DMA descriptors. A descriptor is a small record that tells the DMA where in memory to write and how much space is available.
>
> When DMA finishes, it reports how many bytes it actually wrote. This matters, because a packet can close earlier than expected, for example when the timer closes it. Firmware always uses the reported length, not the buffer capacity.
>
> [Point at the alignment rules.] Buffers and descriptors are aligned to 64-byte boundaries. Descriptors are kept outside the CPU cache, because hardware and software both read them.
>
> [Point at the cache rule.] The sample buffers use the cache for speed, but that creates a classic problem: the CPU might read an old cached copy instead of the fresh data DMA wrote. So before reading a buffer that DMA wrote, firmware invalidates the old cache entries. Then the CPU sees the real data.
>
> [Point at the IRQ line.] One more boundary: the hardware interrupt does not do the work. The interrupt handler acknowledges the hardware, and AcqTask receives a notification and collects the completed buffers.

---

## Slide 24 — Burst mode records a capture, then sends it to the PC

**Route:** Embedded system.

**On the slide:** The burst lifecycle: Check the settings → Prepare DMA, then arm the FPGA → Trigger and capture → Transfer and verify → release the buffer. The right column explains the rates.

**Say:**

> Burst mode is the simpler of the two operating modes. The idea is: capture first, transfer later.
>
> [Walk down the steps.] First, CmdTask checks the settings. A capture that does not fit in the 8-mebibyte buffer is rejected here. Next, AcqTask prepares the DMA descriptors and arms the FPGA. Then the FPGA waits for the trigger and captures. Finally, TxTask transfers the capture to the PC as data messages, and the buffer is released only after the transfer completes.
>
> Why capture first? Because of a rate mismatch. At full speed, the FPGA produces raw samples at 100 megabytes per second. The USB link delivers the measured 1.62 megabytes per second. If we tried to stream raw samples live at full rate, the link could never keep up. Instead, DDR memory holds the capture, and the PC collects it afterward.
>
> [Point at the right column.] Two protection rules. The next capture is blocked until the current buffer is free, so a new capture cannot overwrite data that is still being sent. And checksums detect corrupted data during the transfer; if something is wrong, the host can tell.
>
> This mode is called "burst" because the acquisition is a short burst at full speed, decoupled from the slower transfer.

---

## Slide 25 — Live mode decodes data as it arrives

**Route:** Embedded system.

**On the slide:** A vertical flow: Hardware data contract → AcqTask publishes completed buffer handles → Decoder reconstructs sample time → TxTask sends timestamped events over USB.

**Say:**

> Live mode is the other operating mode. Instead of waiting for a capture to finish, firmware decodes data as it arrives.
>
> [Point at the top box.] First, the hardware contract. There are two formats. In raw format, one byte is one sample of all eight channels. In run format, one 32-bit word contains a count and a sample value: the count is 24 bits, and the value is 8 bits.
>
> [Point at the second box.] When the hardware finishes a packet, AcqTask publishes it as a buffer handle. A handle is a small reference, not a copy of the data.
>
> [Point at the third box.] The decoder tasks then reconstruct sample time. This is the key idea: a run word says "this value lasted N samples". The decoder adds N to the sample timeline and feeds the value at the right time to the protocol decoder. So we recover the timing without storing every sample.
>
> [Point at the bottom box.] Finally, TxTask sends timestamped events over USB: bytes, frame markers, acknowledgments, and errors.
>
> Three decoder slots can run at the same time, each with its own settings, and each can be started or stopped from the PC. The live browser shows decoded messages, not a continuous raw waveform. That is the trade-off: smaller, meaningful messages instead of huge raw data.

**If someone asks:** A pattern match can also stop the live session. That is a firmware feature: when a decoded value matches the pattern, the session stops. It is not the same as the hardware trigger.

---

## Slide 26 — A buffer is reused only after every decoder has finished with it

**Route:** Embedded system.

**On the slide:** The ownership cycle: FREE → DMA_OWNED → CPU_OWNED → back to FREE. The formula line shows that a handle is a slot plus a generation, and a segment carries a sequence number and actual length.

**Say:**

> This slide explains how the firmware keeps track of memory safely. It is one of the most important ideas in the embedded design.
>
> [Follow the cycle.] A buffer starts FREE. When AcqTask hands it to the DMA, it becomes DMA_OWNED, which means hardware is writing into it and nobody else may touch it. When DMA finishes, the buffer becomes CPU_OWNED. Decoders read it at that point.
>
> [Point at the arrow back to FREE.] The buffer returns to FREE only when the last reader releases it. AcqTask then reposts it for the next packet. If even one decoder is still reading, the buffer is not reused.
>
> [Point at the formula line.] Two safeguards. A handle contains a slot number and a generation counter. The generation changes when the slot is reused, so an old handle is rejected instead of reading the wrong data. And each segment carries a sequence number. If a decoder sees a gap in the sequence, it resets its state and marks the discontinuity, because decoding across missing samples would produce false messages.
>
> [Point at the bottom text.] When the pool runs out of free buffers, acquisition stops and reports an overflow. When an abort happens, the firmware stops the DMA and waits for the remaining readers to release their buffers. A partially read buffer is never overwritten.

---

## Slide 27 — Fixed-size queues make overload behavior visible and predictable

**Route:** Embedded system.

**On the slide:** A table of objects and their overflow behavior: Live sample pool, Decoder segment queue, Event blocks / ev_q, and USB transmit pool.

**Say:**

> Real systems do not have infinite memory. This slide shows the four main resources and what happens when each one fills up.
>
> [Point at row 1.] The live sample pool holds sixteen buffers of 256 kibibytes each. If no buffer can be posted safely, acquisition stops. We never overwrite live memory.
>
> [Point at row 2.] Each decoder has a queue of eight buffer handles. If one queue is full, only that decoder skips the buffer. Its reference is released, the loss is counted, and the other decoders continue.
>
> [Point at row 3.] Events are packed into twelve static blocks and queued for transmission. If the queue is full, whole events are dropped and counted. The counter is how the host learns that the system was overloaded.
>
> [Point at row 4.] USB has sixteen transmit frame slots. If they fill, the task waits with a time limit. If live traffic is too heavy, events are dropped and counted, but commands still get through, because command handling shares the schedule.
>
> The principle everywhere is the same: a full queue must always produce a visible, counted result. Nothing is dropped silently.

---

## Slide 28 — The device and PC use the same UART, SPI and I²C decoders

**Route:** Embedded system.

**On the slide:** A table of the three decoders and how each one works, plus a footer showing the shared C code library.

**Say:**

> This slide shows what the firmware does with the sample stream. It decodes three common protocols.
>
> [Point at row 1.] For UART, the decoder uses the baud rate to find the center of each bit and read the received level. It checks parity and stop bits, and it reports bytes, parity errors, and framing errors.
>
> [Point at row 2.] For SPI, the decoder reads data at the clock edge selected by the polarity and phase settings. It tracks the chip-select line and reports data bytes or words plus chip-select changes.
>
> [Point at row 3.] For I²C, the decoder follows the clock and data lines, recognizes START and STOP conditions, and handles clock stretching. It reports addresses, read/write direction, bytes, ACK, and NACK.
>
> [Point at the footer.] The important part: this is one shared C library, used in two places. The device uses it for live decoding. The PC uses the same code to decode saved captures. One implementation means one set of behavior, tested once and reused.
>
> Realistic scopes: the tested live workloads are 115,200-baud UART and 10-kilohertz SPI and I²C. Higher traffic is allowed, but it may drop data, and the system reports that loss.

---

## Slide 29 — USB carries commands, capture data and live events

**Route:** Embedded system.

**On the slide:** The USB connection chain and the application frame format: SOF, TYPE, SEQ, LEN, PAYLOAD, CRC16. Below, the burst integrity rules.

**Say:**

> This slide explains how the device talks to the PC.
>
> [Point at the top chain.] The Zynq processor has a USB controller and an external PHY. The PHY is the physical interface chip. Direct USB means the PC sees the device as a serial port using its standard driver, without any extra gateway hardware.
>
> [Point at the frame.] All communication uses the same frame format. SOF marks the start of a frame. TYPE says what the message is. SEQ is a sequence number that helps detect missing frames. LEN gives the payload length. The payload carries the actual data. CRC16 is a checksum that detects corrupted frames. The receiver checks the length and the checksum before accepting a frame.
>
> [Point at the burst integrity box.] For burst transfers, the capture is sent as chunks. Each chunk carries an offset and up to 4,092 data bytes. At the end, a CRC32 checksum covers the whole capture, so the host can verify that every byte arrived correctly.
>
> During the transfer, the PC acknowledges chunks so the device can keep a window of them in flight. Live event traffic is different: it is best-effort. If it is too heavy, events are dropped and reported, but the command path keeps working.

---

## Slide 30 — UsbTask owns endpoint service and handles session changes

**Route:** Embedded system.

**On the slide:** A table of USB operations: Enumeration/control, Host-to-device data, Device-to-host data, and Reset/session change.

**Say:**

> This slide is about the task that owns USB: UsbTask.
>
> [Point at row 1.] When the device connects, the host enumerates it. UsbTask serves the descriptors and the class requests so Windows recognizes it as a serial port and binds the standard driver.
>
> [Point at row 2.] For data coming from the PC, UsbTask stages completed bytes and passes them to CmdTask through the receive buffer. This matters because USB read boundaries are arbitrary. A command frame might arrive in three reads, or one read might contain two frames. The parser handles that.
>
> [Point at row 3.] For data going to the PC, UsbTask takes ready frames and submits them in the order they were queued. It never overwrites a frame that is still in flight.
>
> [Point at row 4.] Finally, sessions. If the USB bus resets, or the cable is unplugged and reconnected, UsbTask clears stale state: it invalidates old work, resets the transfer state, and asks acquisition to stop. Otherwise old completions could be mistaken for new data. This is a classic embedded problem, and the design handles it explicitly.

---

## Slide 31 — Firmware tests check parsing, buffer reuse, overload and recovery

**Route:** Embedded system.

**On the slide:** A table of test areas and results: Portable C code 18/18 native tests; Release and PC tools 22/22 prechecks and 167 host tests; Repeated captures 500 captures; USB recovery.

**Say:**

> Like the FPGA side, the firmware has its own verification.
>
> [Point at row 1.] Eighteen native tests run on Windows. Native tests exercise the portable C code: commands, decoders, buffer rules, and session logic. They are fast and easy to repeat, but they cannot prove interrupt timing on real hardware.
>
> [Point at row 2.] Twenty-two release prechecks and 167 host tests cover the generated definitions and the PC-side tools.
>
> [Point at row 3.] For real behavior, the bench tests do the work. A run of 500 captures passed with exact lengths and clean checksums.
>
> [Point at row 4.] The USB recovery tests push bad input on purpose: split frames, merged frames, garbage bytes, and corrupted checksums. The parser resynchronizes within a bounded amount of data, and closing the port during a capture returns the device to idle.
>
> Two honest notes. A corrupted length field can blind the receiver for a short time, so recovery is deterministic but not instant. And physical unplug and sleep tests remain as separate human checks, because they depend on manual actions.

---

## Slide 32 — USB transferred 64 MiB at 1.62 MB/s in each test

**Route:** Embedded system.

**On the slide:** A bar chart with three trials at 1.62 megabytes per second and a dashed target line at 1.5 megabytes per second.

**Say:**

> This is the main performance result for the direct USB link.
>
> [Point at the bars.] Each of the three trials transferred 67,108,864 bytes in 41.4 to 41.5 seconds, and measured 1.62 megabytes per second. The dashed line is the project target of 1.5 megabytes per second, so the result passes the requirement with margin.
>
> Two definitions are worth being exact about. First, MB per second uses decimal units, while MiB uses binary units. The difference matters when we compare numbers. Second, each trial was not one giant capture. Each trial was sixteen separate 4-mebibyte captures, done back to back.
>
> [Point at the right column.] The timing includes everything: sending commands, capturing on the board, transferring over USB, and verifying on the PC. Every capture matched the expected counter values, the frame checksums, and the whole-capture checksum.
>
> The turning point was the design amendment: larger 4,092-byte chunks, larger host reads, and an acknowledgment every four chunks. Before that, the link only reached between 0.03 and 0.85 megabytes per second, because every small chunk needed a full handshake. With the larger frames, the link reached the target.
>
> One scope note: this result supports buffered burst transfers. It does not mean that full-rate raw sampling can stream over USB continuously.

---

## Slide 33 — The 60-second UART test showed no reported event loss

**Route:** Embedded system.

**On the slide:** Big numbers: device decoded 606,411 events and host feed received 606,411 events. A small table shows zero client drops, zero overruns or lost segments, and 124.7 kilobytes per second of USB framed traffic.

**Say:**

> This is the main correctness result for live mode.
>
> [Point at the two numbers.] Over a 60-second run, the device decoded 606,411 events, and the host received 606,411 events. The two numbers match, and the drop counters are zero: zero client drops, zero overruns, and zero dropped segments.
>
> [Point at the small table.] The USB traffic was about 125 kilobytes per second. That number includes timestamps, event data, and all the framing overhead.
>
> The workload was a 115,200-baud UART pattern generated on the board and looped back into the analyzer. The total includes 606,405 byte events and 6 other events, which are protocol markers like frame errors or start conditions. Separate tests also passed for SPI and I²C at 10 kilohertz.
>
> One test on this slide is deliberately negative. The 1-megahertz SPI test overloads the system on purpose. The expected behavior there is that the device reports lost data, stays healthy, and returns to idle. That is what it does.
>
> Finally, a wording detail: the table talks about replay history. Old messages are removed from the browser's replay buffer when it fills up. That is different from losing messages on the way to the connected viewer, and the two counters are kept separate.

---

## Slide 34 — The browser lets you inspect waveforms and decoded messages

**Route:** Shared close.

**On the slide:** A screenshot of the analyzer application showing a UART waveform and decoded bytes on one timeline, with cursor A marking a sample.

**Say:**

> Now we come back to the shared results, where both subsystems work together.
>
> [Point at the screenshot.] This is the actual user interface. The waveform shows one channel, and the decoded bytes sit directly below it on the same timeline. The cursor marks a sample position so you can measure timing. You can zoom, scroll, search for edges, decode a capture, view live messages, and export the results as industry-standard files.
>
> The screenshot is an offline illustration: I loaded a synthetic UART capture into the viewer, so it is not a new hardware measurement. The important evidence behind this slide is different: on the 26th of September, the final USB image booted from an SD card with no JTAG connection, and all six user-interface end-to-end tests passed on that image. Those tests covered capture, host decoding, live control, and reopening saved captures offline.
>
> One item is still open. The automated checks pass, but a human visual check on the real board is still on the to-do list. I want to be clear about that boundary.

---

## Slide 35 — The device has tested limits and a few release checks left

**Route:** Shared close.

**On the slide:** A table of limits: Input voltage, Capture size and rate, Live decoding, Visual UI check, and Public release.

**Say:**

> Good engineering means stating the limits clearly, so this slide lists them.
>
> [Point at row 1.] The inputs are eight 3.3-volt CMOS channels with no built-in protection. Use a common ground and short leads, and add a level shifter for 5-volt signals.
>
> [Point at row 2.] The working device is the 100-megahertz design with an 8-mebibyte capture buffer and up to 32,768 pre-trigger samples. The 200-megahertz version is a project-plan stretch item; it has not been built or tested, so the released design stays at 100.
>
> [Point at row 3.] Live decoding is qualified at 115,200-baud UART and 10-kilohertz SPI and I²C. Higher traffic can lose data, and the device counts and reports it.
>
> [Point at row 4.] The automated browser tests pass and the recent viewer fixes are tested offline, but the visual check on the real board is still open.
>
> [Point at row 5.] SD boot passed, but the USB identity used during development is for in-house use, and the version 1.0 release needs approval.
>
> These are the exact boundaries I am standing behind today.

---

## Slide 36 — The FPGA handles timing; firmware and the PC make the data useful

**Route:** Shared close.

**On the slide:** A summary table with four goals: Capture signal timing, Keep data safe, Read protocol messages, and Inspect and save results.

**Say:**

> Let me close with the main argument of the project.
>
> [Point at row 1.] Capturing signal timing is the FPGA's job. It samples eight channels at up to 100 million samples per second, detects triggers, and keeps history. The wired loopback tests and the timing-closed build support this.
>
> [Point at row 2.] Keeping data safe is the firmware's job. FreeRTOS tracks buffer ownership and decides when memory can be reused. Missing data and overloads are always reported.
>
> [Point at row 3.] Reading protocol messages is also firmware's job. The shared C decoders turn samples into UART, SPI, and I²C events. The same code runs on the device and on the PC.
>
> [Point at row 4.] Making the data usable is the PC's job. Direct USB connects the device to a browser workspace, and the buffered transfer measured 1.62 megabytes per second.
>
> [Point at the bottom line.] Remember the rate difference: fast sampling is buffered in DDR memory. Full-rate raw data is not streamed continuously over USB.
>
> The engineering lesson is simple. In a mixed hardware and software system, correctness depends on the boundaries: who owns memory, who may reuse it, and when. Getting those boundaries right matters as much as the maximum clock speed.

---

## Slide 37 — RTOS Logic Analyzer

**Route:** Shared close.

**On the slide:** The back cover with the project name and platform line.

**Say:**

> Thank you for your attention. I am happy to answer questions, especially about the FPGA and firmware boundary, buffer ownership, the difference between the acquisition rate and the USB rate, and which results are verified on real hardware.

**If someone asks:** Suggested answers to the most likely questions are in the list below.

- **Why not stream raw data live?** The FPGA produces raw data faster than USB can carry it. Burst mode buffers in DDR first. Live mode sends decoded events instead of raw samples.
- **Why use run encoding?** It saves space when signals repeat, which is common in protocols. The firmware knows the format and reconstructs time from the run counts.
- **How do you prevent data loss?** Buffers are owned by one party at a time, reuse waits for the last reader, queues have documented drop-and-count behavior, and the hardware reports overflow.
- **Why is USB 1.62 megabytes per second, not the nominal USB speed?** That number is the measured application throughput for the whole acquire-transfer-verify workflow, not the raw bus signaling rate. Larger chunks and host pacing made the link efficient.
- **What is not finished?** The human visual check on the real board, the public USB identity, and the version 1.0 release approval. The 200-megahertz stretch design has not been tested.

---

## Term sheet

| Term | Plain meaning |
|---|---|
| RTOS | Real-time operating system. It schedules tasks so the most important work runs first. |
| FreeRTOS | The RTOS used on the ARM processor. |
| FPGA | Field-programmable gate array. Hardware logic that can be configured after manufacturing. |
| PL | Programmable logic. The FPGA part of the Zynq chip. |
| PS | Processing system. The ARM part of the Zynq chip. |
| RTL | Register-transfer level. The style of hardware description used for the FPGA logic. |
| AXI | Advanced eXtensible Interface. The on-chip connection standard. |
| AXI-Stream | The AXI variant used for streaming sample words. |
| AXI-Lite | The AXI variant used for simple register reads and writes. |
| DMA | Direct memory access. Hardware that copies data without the CPU copying each item. |
| S2MM | Stream to memory-mapped. The DMA direction from a stream into memory. |
| Scatter-gather | A DMA mode where the DMA follows a list of buffer descriptors. |
| DDR | Double data rate memory. The board's main working memory. |
| FIFO | First in, first out buffer. It smooths short timing differences. |
| BRAM | Block RAM. Dedicated memory inside the FPGA. |
| IRQ | Interrupt request. A hardware signal that asks the CPU for attention. |
| ISR | Interrupt service routine. The short handler that runs when an interrupt fires. |
| CDC-ACM | Communications Device Class, Abstract Control Model. The USB class that looks like a serial port. |
| CRC | Cyclic redundancy check. A checksum that detects corrupted data. |
| SSE | Server-Sent Events. The browser technology used to stream live messages. |
| UART | Universal asynchronous receiver/transmitter. A common two-wire serial protocol. |
| SPI | Serial peripheral interface. A clocked four-wire protocol. |
| I²C | Inter-integrated circuit. A two-wire addressed protocol. |
| LUT | Lookup table. The small logic element inside an FPGA. |
| WNS / WHS | Worst negative slack for setup timing / worst hold slack. Positive values mean timing passes. |
| MVP | Minimum viable product. The smallest complete, working version. |
| FSBL | First-stage boot loader. The small program that loads the FPGA and starts the application. |
| GP0 / HP0 | The processor's general-purpose register port / high-performance memory port. |
| DONE | The FPGA status bit that reports a capture has finished. |

---

*End of script.*
