import type { GlossaryEntry } from './types';

export const GLOSSARY: GlossaryEntry[] = [
  {
    term: 'ACK',
    definition:
      'A short control frame (0x40) that confirms a specific message type and sequence number. The host acknowledges every 16 DATA_CHUNK frames, and commands such as CONFIG wait for an ACK before the next request.',
  },
  {
    term: 'AXI Stream',
    definition:
      'The one-way ready/valid bus between the FPGA capture pipeline and the AXI DMA S2MM port. Each 32-bit beat carries four raw samples or one transition word; TKEEP marks valid bytes and TLAST closes a DMA packet.',
  },
  {
    term: 'AXI4-Lite',
    definition:
      'The simple memory-mapped bus the PS uses to read and write the 21 analyzer registers at base 0x4000_0000. It carries configuration and status, not sample data.',
  },
  {
    term: 'backpressure',
    definition:
      'A receiver telling a producer to pause, expressed as TREADY low on the AXI stream or a full downstream ring. Sampling cannot be paused, so the capture path flags an overflow instead of pretending the data is continuous.',
  },
  {
    term: 'BRAM',
    definition:
      'Block RAM inside the FPGA. Here it holds the pre-trigger history ring, the 8 KiB AXIS FIFO and other small buffers, all built with synchronous reads and one-beat output registers.',
  },
  {
    term: 'CRC',
    definition:
      'Cyclic redundancy check. CRC16-CCITT protects every protocol frame (TYPE through PAYLOAD); CRC32 covers a whole capture and is checked in DATA_END after all DATA_CHUNK frames.',
  },
  {
    term: 'DDR',
    definition:
      'The Zybo external DDR3 memory reached through the PS memory controller. Capture payloads and scatter-gather descriptors live in reserved regions of it, separate from the firmware and from the neighboring DDR3 PHY project.',
  },
  {
    term: 'descriptor',
    definition:
      'A small memory record that tells the AXI DMA where to write a packet and how long it may be. This project always uses ordinary scatter-gather descriptors; cyclic mode and simple-transfer calls are not permitted.',
  },
  {
    term: 'digital channel',
    definition:
      'One logic input line, sampled as a single bit per sample time. The analyzer has eight 3.3 V channels on the JC header.',
  },
  {
    term: 'discontinuity',
    definition:
      'A gap in the sample stream where time or data is unknown. Decoders reset their state and emit a discontinuity marker rather than decoding across the gap as if it were contiguous.',
  },
  {
    term: 'DMA',
    definition:
      'Direct memory access. The AXI DMA S2MM engine writes FPGA capture packets into DDR without the CPU copying them; the PS prepares descriptors and receives completion interrupts.',
  },
  {
    term: 'fabric clock',
    definition:
      'The 100 MHz FCLK_CLK0 that clocks the whole PL design. The sample rate is derived from it as 100 MHz / (SAMPLE_DIV + 1).',
  },
  {
    term: 'FIFO',
    definition:
      'First-in, first-out buffer. The 8 KiB AXIS FIFO decouples the capture pipeline from DMA stalls, and the AXI Quad SPI slave has a 256-byte FIFO on the bulk link.',
  },
  {
    term: 'FPGA',
    definition:
      'Field-programmable gate array: the programmable logic that performs sampling, triggering, encoding and packing on the Zynq chip.',
  },
  {
    term: 'frame',
    definition:
      'One protocol unit: SOF 0xA5, TYPE, SEQ, LEN, PAYLOAD and CRC16. The framing is identical on the Zynq UART link and the TCP socket so the ESP32 can relay frames verbatim.',
  },
  {
    term: 'HIL',
    definition:
      'Hardware-in-the-loop. The hil runner target and tests/hil exercise the real board over a selected COM port. It is a [HUMAN] gate and was not run in this checkout.',
  },
  {
    term: 'IRQ',
    definition:
      'Interrupt request. The PL raises level interrupts for DONE, TRIGGERED and OVERFLOW, and the DMA raises its own completion interrupt. Interrupt handlers only notify tasks.',
  },
  {
    term: 'ISR',
    definition:
      'Interrupt service routine. Here it only notifies a task or performs an ISR-safe send; no parsing, decoding or buffer processing happens in interrupt context.',
  },
  {
    term: 'logic analyzer',
    definition:
      'An instrument that records digital signals over time so timing and protocol behavior can be inspected. This project builds one from a Zybo Z7-10, an ESP32 Wi-Fi gateway and a native Windows host tool.',
  },
  {
    term: 'mDNS',
    definition:
      'Multicast DNS. The ESP32 gateway advertises itself as la-esp32.local so the host can find it without a fixed address.',
  },
  {
    term: 'metastability',
    definition:
      'The uncertain output of a flip-flop whose input changes too close to the clock edge. The input synchronizer reduces the chance that this uncertainty propagates into the fabric.',
  },
  {
    term: 'MMIO',
    definition:
      'Memory-mapped I/O: register access through ordinary load and store instructions. The Zynq firmware drives the analyzer and DMA registers this way.',
  },
  {
    term: 'NAK',
    definition:
      'A negative acknowledgement (0x41) carrying an error code. It tells the host that a command was rejected or malformed instead of silently ignoring it.',
  },
  {
    term: 'ownership',
    definition:
      'The rule that exactly one party owns a buffer at a time. Segment states move FREE to DMA_OWNED to CPU_OWNED to FREE, and buffers are transferred by handle so nobody frees memory that is still in use.',
  },
  {
    term: 'payload',
    definition:
      'The bytes between the frame header and the CRC. Its layout depends on the message type and is bounded to 1024 bytes.',
  },
  {
    term: 'PL',
    definition:
      'Programmable logic: the FPGA fabric half of the Zynq. All capture RTL (la_top and its blocks) lives here.',
  },
  {
    term: 'post-trigger',
    definition:
      'Samples captured after the trigger. POST_SAMPLES sets how many are kept, and the field is ignored in streaming mode.',
  },
  {
    term: 'pre-trigger',
    definition:
      'Samples kept from before the trigger in the history ring. The register map clamps PRE_SAMPLES to 32,768 even though the internal ring holds 65,536 entries.',
  },
  {
    term: 'PS',
    definition:
      'Processing system: the hard ARM cores, DDR controller and peripherals half of the Zynq. It runs the FreeRTOS firmware.',
  },
  {
    term: 'PulseView',
    definition:
      'The sigrok waveform viewer used to open exported .sr files. Version 0.4.2 is pinned project-local under build/pulseview.',
  },
  {
    term: 'queue',
    definition:
      'A FreeRTOS queue passes fixed-size items between tasks. Every queue in this project documents its producer, consumer, depth and full policy (drop, block or return).',
  },
  {
    term: 'raw packing',
    definition:
      'The default burst encoding: four 8-bit samples packed into one 32-bit word, with TKEEP marking the valid bytes of a partial final word.',
  },
  {
    term: 'ready/valid',
    definition:
      'The AXI-stream handshake: the producer asserts VALID with data and the consumer asserts READY when it can accept a beat. A transfer happens only on a cycle where both are high.',
  },
  {
    term: 'reference count',
    definition:
      'A counter that tracks how many consumers still own a shared segment. The pool recycles a buffer only when the last successful consumer releases its reference.',
  },
  {
    term: 'ring buffer',
    definition:
      'A fixed-size circular buffer addressed by index rather than by moving memory. The pre-trigger history, the ESP32 live mirror ring and the link rings are ring buffers with documented full policies.',
  },
  {
    term: 'RLE',
    definition:
      'Run-length encoding. Transition mode stores {run[23:0], sample[7:0]} (a run length and the value it held), and the host reconstructs samples from those runs.',
  },
  {
    term: 'RTOS',
    definition:
      'Real-time operating system. The Zynq runs FreeRTOS 10.x on CPU0 and the ESP32 runs ESP-IDF FreeRTOS; task priorities, static allocation and queue policies are part of the design.',
  },
  {
    term: 'RTS/CTS',
    definition:
      'Request-to-send and clear-to-send hardware flow control on the UART link. It lets a full receiver pause the sender instead of dropping bytes.',
  },
  {
    term: 'sample',
    definition:
      'One reading of all enabled digital channels at a single instant, packed into an 8-bit vector with one bit per channel.',
  },
  {
    term: 'sample rate',
    definition:
      'How many samples are taken per second: 100 MHz / (SAMPLE_DIV + 1). The MVP is qualified at 100 MS/s; the 200 MS/s stretch is not proven.',
  },
  {
    term: 'scatter-gather',
    definition:
      'A DMA mode that follows a chain of descriptors instead of one flat transfer. The finite ordinary chain lets a capture span many DDR buffers without cyclic mode.',
  },
  {
    term: 'sequence number',
    definition:
      'A byte in every frame used to order requests and to match ACK/NAK replies. It also lets the parser detect a missing or reordered frame.',
  },
  {
    term: 'sigrok',
    definition:
      'The open-source signal-analysis project behind PulseView and the .sr file format. The optional P10.2 SUMP bridge that would connect it live is deferred.',
  },
  {
    term: 'stream buffer',
    definition:
      'A FreeRTOS single-writer, single-reader byte stream. It carries UART RX/TX and the ESP32 SPI hand-off, where the two ends never need fixed-size messages.',
  },
  {
    term: 'synchronizer',
    definition:
      'The two-flop (optionally 2-of-3 majority) stage that brings asynchronous pads safely into the fabric clock domain. It does not capture pulses shorter than the sampling period or protect the pins electrically.',
  },
  {
    term: 'task',
    definition:
      'An RTOS thread of execution. The Zynq runs acquisition, command, TX, status and decoder tasks; the ESP32 runs link, network, web, UI and Wi-Fi tasks, each with a documented priority and stack.',
  },
  {
    term: 'TCP',
    definition:
      'The stream transport between the host and the ESP32 gateway on port 5555. The gateway relays analyzer frames verbatim and serves a single client at a time.',
  },
  {
    term: 'TKEEP',
    definition:
      'The AXI-stream byte-keep signal. It marks which bytes of a beat are valid so that a partial final word is not mistaken for real samples.',
  },
  {
    term: 'TLAST',
    definition:
      'The AXI-stream marker used here to close a DMA packet: the raw packer tags the final word of a burst packet so the DMA knows the packet boundary. An early or missing TLAST corrupts packet framing.',
  },
  {
    term: 'transition encoding',
    definition:
      'The compact burst encoding {run[23:0], sample[7:0]}. Instead of every sample it stores how many samples in a row held one value, which is far smaller for slow signals.',
  },
  {
    term: 'trigger',
    definition:
      'The qualifying event that positions a capture: a level mask combined with optional rise/fall edge masks, where an empty edge mask means level-true. The trigger is the first qualifying sample after arming.',
  },
  {
    term: 'UART',
    definition:
      'The asynchronous serial link. The Zynq uses UART0 on EMIO to the ESP32 at 2 Mbps with RTS/CTS; the ESP32 relays it to TCP and keeps commands and ACKs on this path.',
  },
  {
    term: 'VCD',
    definition:
      'Value Change Dump, a text waveform format the host can export alongside .sr captures. It is written with pyvcd.',
  },
  {
    term: 'W1C',
    definition:
      'Write-one-to-clear. Reading an IRQ_STAT bit shows that an event occurred; writing a 1 clears it, and writing a 0 leaves it unchanged.',
  },
  {
    term: 'Zynq',
    definition:
      'The AMD/Xilinx Zynq-7010 on the Zybo Z7-10, which combines a dual-core ARM PS with FPGA PL. It is the board that samples and captures the signals.',
  },
];
