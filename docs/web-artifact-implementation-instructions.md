# Three-level web artifact implementation instructions

Prepared: 2026-09-11, Asia/Jakarta. Revised: 2026-09-14 (P7–P10.1 landed; the
statuses, component lists, evidence snapshot and roadmap emphasis below were
reconciled with `docs/status.md` and the current source before implementation).

Repository baseline: `748d966` (`origin/main` at revision time; the current
ledger in `docs/status.md` supersedes older statements).

These instructions are the implementation contract for expanding the existing
`web-artifact/` explainer into a complete, navigable explanation of the RTOS
logic-analyzer project. They are intentionally explicit enough for a lower-cost
coding model to implement without inventing the information architecture,
content boundaries, evidence rules, or page behavior.

The work described here is a website change only. It must not change RTL,
firmware, register definitions, protocols, tests, generated files, hardware
configuration, or bench evidence. It must not publish or redeploy the existing
Site unless the user separately asks for publication.

## 1. Required outcome

Turn the current single-page overview into an educational technical portal with
progressive disclosure at exactly three architectural levels:

1. **Level 1 — system context:** one complete diagram of how signals, commands,
   captured data, and status move across the whole product.
2. **Level 2 — subsystem architecture:** one route for every Level 1 block,
   containing a subsystem-specific diagram and a plain-language explanation of
   its responsibility and boundaries.
3. **Level 3 — component detail:** one data-driven detail route for every
   meaningful implementation component, with behavior, interfaces, ownership,
   configuration, failure handling, status, evidence, source files, and tests.

A new reader must be able to start at the whole-system picture, select any
block, drill into its components, and return through breadcrumbs without losing
context. An expert must be able to find the corresponding specifications,
implementation files, and verification boundaries without reading the entire
repository.

“Explain every single thing” means every project-owned architectural area,
runtime component, public interface, operating mode, build stage, and test layer
must have an intentional home in the portal. It does **not** mean copying every
source line, private build log, or historical ledger entry onto the website.

## 2. Audience and writing contract

Write for two audiences at once:

- A learner who knows basic digital electronics but may not know Zynq, AXI,
  DMA, RTOS scheduling, or framed protocols.
- An engineer who wants exact names, data formats, ownership rules, limits,
  implementation status, source locations, and verification evidence.

Every page must begin with a short plain-language answer, then add technical
depth. Define a term before using its acronym prominently. Prefer concrete
sentences such as “The DMA writes completed FPGA packets into reserved DDR
buffers” over abstract phrases such as “high-performance data movement.”

Do not use marketing claims. Do not claim that a planned path works. Do not
describe simulation, native tests, or an internal pattern source as physical
external-input proof. Preserve all claim corrections in
`docs/web-artifact-content-review.md`.

## 3. Non-goals and hard boundaries

- Preserve the current Vinext/React/Tailwind stack, lockfile, component library,
  visual identity, colors, typography, and evidence-first tone.
- Do not add packages, a database, authentication, uploads, telemetry, remote
  content loading, an embedded terminal, or hardware controls.
- Do not add search in this implementation. Navigation, breadcrumbs, page
  indexes, related links, and the glossary are sufficient for the requested
  scope.
- Do not use Mermaid, canvas, generated raster art, or decorative imagery.
  Build diagrams from semantic HTML/CSS and small functional SVG connectors.
- Do not hand-edit `common/la_proto/include/la_regs.h`,
  `fpga/rtl/la_regs_pkg.sv`, `tools/models/regs.py`, or `docs/regmap.md`.
- Do not edit `docs/regmap.yaml`; the website must explain the current map.
- Do not run hardware, firmware, Vivado, Vitis, Questa, or bench tests for this
  content-only implementation.
- Do not register a new Site. Preserve the existing project ID in
  `web-artifact/.openai/hosting.json`.
- Do not publish. A production build and local source review are required;
  deployment is a separate user decision.
- Do not modify the neighboring DDR3 PHY project.

## 4. Source-of-truth and evidence policy

Use sources in this priority order:

1. **Current implemented behavior:** current source plus the newest specific,
   dated evidence in `docs/status.md`.
2. **Normative interfaces:** `docs/regmap.yaml`, `docs/protocol.md`,
   `docs/decoders.md`, and `docs/wiring.md`.
3. **Architecture and ownership:** `docs/architecture.md` and
   `docs/rtos-design.md`.
4. **Intended or future behavior:** `docs/plan.md`.
5. **Verification boundaries:** `docs/test-plan.md` and the tests themselves.

When sources disagree, never silently pick the most optimistic statement.
Prefer the newest narrow evidence for what works and the normative
specification for what is intended. Label the discrepancy as historical or
open if it matters to the explanation.

### 4.1 Status vocabulary

Use only the following status keys. Render both an icon and text; color alone is
not sufficient.

| Key | Visible label | Meaning |
|---|---|---|
| `bench-verified` | Bench verified | Exercised through physical target hardware in a recorded run. |
| `target-tested` | Target tested | Exercised on the real target, but not necessarily in the full end-to-end path. |
| `simulation-tested` | Simulation tested | Proven by a self-checking RTL simulation, not physical I/O. |
| `host-tested` | Host tested | Exercised by native Windows or Python tests without proving target timing. |
| `implemented-unverified` | Implemented; target proof open | Source exists, but the relevant target or hardware gate is still open. |
| `planned` | Planned | Required by the plan but not implemented in the running target path. |
| `known-issue` | Known issue | An observed behavior still needs correction or profiling. |
| `blocked` | Blocked | Work cannot currently be validated because a named prerequisite is absent. |

Every status badge must have a one-sentence scope immediately nearby. For
example, “Bench verified with the internal counter source at 1 and 10 MS/s;
external JE→JC and the complete 100 MS/s matrix remain open.”

### 4.2 Evidence snapshot

Create one exported constant for the evidence snapshot. Use:

- content review date: `14 Sep 2026`;
- latest bench evidence date represented by the site: `14 Sep 2026` (P10.1
  live web view; the newest P9.1 SPI bench run is 13 Sep 2026);
- implementation baseline: `748d966` (replace with the portal commit hash in a
  follow-up data-only update so every source link resolves);
- repository: `https://github.com/linggaaradhanas22/logic-analyzer`.

Do not calculate these values from the viewer's clock. Generate source links
from this constant so dates and revisions cannot drift across pages. Before
implementation, compare `HEAD` and the latest ledger entries against this
snapshot. If later work changes a fact, update only the affected content and
record the new reviewed revision.

Never expose local Wi-Fi credentials, host addresses, COM-port assignments that
identify a workstation, absolute machine paths, source credentials, or ignored
raw reports. Link to tracked repository files at the pinned revision.

## 5. Global information architecture

Implement these routes. The subsystem and component pages must be generated
from central content data, not copied into many hand-written page files.

| Route | Purpose |
|---|---|
| `/` | Level 1 system context, operating modes, strongest evidence, current limits, and entry points. |
| `/using` | Step-by-step operating guide: build/load, link verification, self-test and real captures, live decode, browser view, trace, SPI transport, command reference, troubleshooting. |
| `/subsystems/[slug]` | Level 2 diagram and overview for one of the seven system blocks. |
| `/components/[slug]` | Level 3 detail page generated from the component catalog. |
| `/reference/registers` | All registers and fields from the normative map, grouped by purpose. |
| `/reference/protocol` | Frame anatomy, messages, payloads, transfer sequence, errors, and flow control. |
| `/reference/decoders` | Shared transition model plus UART, SPI, and I²C behavior and status. |
| `/reference/wiring` | JC/JE/JD mappings, ESP32 mapping, loopback setups, voltage and power rules. |
| `/verification` | Test pyramid, canonical targets, current recorded evidence, and unverified gates. |
| `/roadmap` | P0–P10 progress, current next actions, known issues, and future work. |
| `/tooling` | Native-Windows workflow, generator, build stages, artifact outputs, and release packaging. |
| `/glossary` | Short definitions for project-specific and general technical terms. |

Use a persistent desktop header with the project name and links to Overview,
Subsystems, Reference, Verification, Roadmap, and Glossary. On mobile, use the
existing `Sheet` primitive for the navigation menu. Do not create a second
parallel navigation scheme.

All non-home pages require:

- breadcrumb: `System / <Subsystem> / <Component>` as applicable;
- page-level status and scope sentence;
- “Where this sits” miniature context strip;
- previous/next links in architectural order;
- related component/reference links;
- pinned source/evidence footer.

## 6. Level 1 — whole-system architecture

The first meaningful viewport on `/` must show the Level 1 architecture, not a
large marketing hero. Retain the current definition of a logic analyzer and a
compact project summary above or beside the diagram.

### 6.1 Required Level 1 nodes

Use these seven nodes in this order:

| Order | Slug | Visible name | One-line responsibility | Primary status |
|---:|---|---|---|---|
| 1 | `physical-io` | Signals and physical I/O | Accept eight 3.3 V digital inputs and provide self-test and board-to-board wiring. | `bench-verified` for JE→JC loopback; external-source matrix `implemented-unverified` |
| 2 | `fpga-capture` | FPGA capture pipeline | Synchronize, sample, trigger, retain history, encode, and packetize signals at fabric speed. | `bench-verified` (internal and JE→JC sources); external-input rate matrix open |
| 3 | `dma-memory` | AXI DMA and DDR | Move complete AXI-stream packets into bounded DDR buffers through ordinary scatter-gather descriptors. | `bench-verified` for burst and live |
| 4 | `zynq-rtos` | Zynq FreeRTOS control | Coordinate acquisition, commands, interrupts, transmission, status, and ownership on the ARM processor. | `bench-verified` for burst and live decode |
| 5 | `protocol-link` | Framed UART + SPI link | Carry commands, status, events, and capture chunks over a CRC-protected RTS/CTS UART protocol, with a best-effort SPI bulk path. | `bench-verified` for burst/live UART; SPI framed transfers bench-verified, drain rate `blocked` |
| 6 | `esp32-gateway` | ESP32 wireless gateway | Relay framed traffic between UART/SPI and one Wi-Fi TCP client while exposing health counters and the live web view. | `bench-verified` with a Wi-Fi throughput `known-issue` |
| 7 | `host-tools` | Host tools and capture files | Configure captures, validate transfers, reconstruct samples, and export `.sr`, VCD, PNG, text, or CSV; stream live decoded events. | `host-tested`; burst and live used on bench |

Every node is a native link to `/subsystems/<slug>`. The entire card must be
keyboard focusable, show a visible focus state, and include its order, name,
one-line role, and status label. Do not require hover to discover the link.

### 6.2 Required Level 1 connections

Show three named flows without animating continuously:

- **Capture data:** physical I/O → FPGA → DMA/DDR → Zynq RTOS → framed UART →
  ESP32 → host.
- **Commands and acknowledgements:** host → ESP32 → UART/protocol → Zynq RTOS
  → FPGA registers/DMA control.
- **Status and errors:** FPGA/DMA/RTOS → protocol → ESP32 → host.

Add a simple selector with `Capture data`, `Commands`, and `Status/errors`.
Selecting a flow highlights its edges and displays a short numbered explanation
below the diagram. This state is explanatory only and must not pretend to be
live telemetry. On narrow screens, render the nodes vertically and replace
crossing arrows with per-node “receives from” and “sends to” labels.

### 6.3 Required overview content below the diagram

Keep the corrected current-page content, reorganized into these sections:

1. Burst capture and planned live decode tabs.
2. Calculated capture-versus-transport capacity and separately labeled measured
   gateway loopback results.
3. Evidence cards: the current burst/live E2E counts, 5/5 ESP32 target Unity,
   12/12 FPGA simulation, the 500-capture soak and the current host-test count,
   each rechecked against the newest ledger row rather than copied from this
   document.
4. Open work: the external-input rate/stimulus matrix, the P9.1 drain transport
   port (ESP32-S3 USB-OTG), the P9.3 200 MS/s decision, P9.4 Wi-Fi tuning, the
   200 Ω SPI edge characterization, P10.2 sigrok deferral, and P10.4 release
   (final BOOT.BIN, [HUMAN] SD boot, `v1.0` tag). The stale-client issue is
   resolved (2026-09-13); the remaining gateway limit is Wi-Fi drain rate and
   jitter, which must be described as measured, not as a root cause.

## 7. Level 2 — subsystem pages

Each subsystem route must contain:

1. A two-sentence beginner explanation.
2. A boundary statement: what belongs inside and outside this subsystem.
3. A subsystem diagram using the exact component order below.
4. A numbered “what happens” sequence.
5. Inputs and outputs table.
6. Design decisions and constraints.
7. Status/evidence summary that may contain more than one status.
8. Component index linking every node to its Level 3 page.
9. Related reference pages and source files.

### 7.1 `physical-io`

Diagram order:

`External source → JC CH0–CH7 → input constraints → synchronizer → capture`

Secondary branches:

`Pattern generator → JE PATGEN0–PATGEN7 → optional JE-to-JC jumpers`

`Zynq UART0 EMIO on JD ↔ ESP32 UART2 GPIOs`

Required topics:

- Eight acquisition channels, LVCMOS33, exact JC pin mapping.
- Eight pattern outputs, exact JE mapping, internal self-test versus external
  physical loopback distinction.
- Two-flop synchronization, optional 2-of-3 filter, asynchronous-input CDC
  boundary, and what synchronization does not guarantee.
- JD UART TX/RX/RTS/CTS mapping to GPIO16/17/21/22 and common ground.
- Separate power, no JD power connection, no 5 V exposure, short leads, and
  human continuity/level checks.
- ESP32-only UART loopback wiring and why the JD ribbon must be removed.
- JP5 JTAG versus SD boot purpose.

Component slugs:

`jc-inputs`, `input-cdc`, `je-pattern-output`, `jd-uart-wiring`,
`jd-spi-wiring`, `esp32-loopback-wiring`, `power-and-level-safety`.

Primary sources: `docs/wiring.md`, `docs/architecture.md`,
`fpga/constraints/zybo_z7_10.xdc`, `fw-esp32/components/la_board/include/la_board.h`.

### 7.2 `fpga-capture`

Diagram order:

`input mux → la_input_sync → la_sampler → la_trigger + la_pretrig_ring →
raw/transition encoding → la_axis_fifo → AXI stream`

Show `la_capture_ctrl` supervising trigger, run, flush, drain, and completion;
show `la_regs` on the control side; show `la_patgen` feeding the self-test mux.

Component slugs in signal/control order:

| Slug | Visible name | Required explanation |
|---|---|---|
| `la-top` | `la_top` integration | Top-level muxing, status composition, AXI-Lite control, AXI-stream output, heartbeat, and how all RTL blocks connect. |
| `la-input-sync` | Input synchronizer | Two flip-flops, optional majority filter, latency, channel independence, metastability-risk reduction, and pulse/filter limits. |
| `la-sampler` | Sample-rate divider | `100 MHz / (DIV + 1)`, `sample_valid`, enabled-channel mask, and why sampling rate is distinct from fabric clock. |
| `la-trigger` | Trigger detector | Level mask/value, rising/falling masks, first-sample behavior, and how combined conditions qualify a trigger. |
| `la-pretrig-ring` | Pre-trigger ring | 65,536-entry internal ring, configurable maximum 32,768 pre-trigger samples, trigger seam, replay, stream pass-through, stalls, overflow, and no-silent-loss rule. |
| `la-raw-packer` | Raw sample packer | Four oldest-first 8-bit samples per 32-bit word, `tkeep`, counted burst `tlast`, partial tails, stalls, and live/burst isolation. |
| `la-txn-encoder` | Transition encoder | `{run[23:0], sample[7:0]}`, value changes, maximum-run split, flush semantics, reconstruction, and quiet-versus-busy bandwidth behavior. |
| `la-capture-ctrl` | Capture controller | IDLE/ARMED/CAPT/DRAIN lifecycle, latched configuration, K+POST accounting, timeout, abort, overflow drain, live flush, `pkt_words`, and word count. |
| `la-axis-fifo` | AXI-stream FIFO | 32-bit data plus `tkeep/tlast`, BRAM implementation, ready/valid backpressure, count/full, sticky overflow, and loss visibility. |
| `la-regs` | AXI-Lite registers | Independent AW/W handling, byte strobes, self-clearing controls, sticky status, W1C IRQ status, clamps, and generated-map authority. |
| `la-patgen` | Pattern generator | Counter, walking one, PRBS8, UART, SPI, and I²C modes; divider latching; JE output; self-test purpose and evidence boundary. |

Every component page links to its same-name RTL and testbench. `la-regs` also
links to the register reference. `la-top` links to the Vivado integration in
`fpga/scripts/build.tcl` without presenting the script itself as an RTL block.

### 7.3 `dma-memory`

Diagram order:

`la_axis_fifo → AXI DMA S2MM → ordinary SG descriptor chain → HP0 → reserved
non-cached DDR capture region → TxJob reader`

Control/interrupt branch:

`AcqTask → DMA driver`; `DMA IRQ + PL IRQ → GIC → AcqTask notification`.

Component slugs:

| Slug | Visible name | Required explanation |
|---|---|---|
| `vivado-block-design` | Zynq block design | PS7, GP0, HP0, SmartConnect, analyzer registers, S2MM-only DMA, two interrupt inputs, addresses, clock, and resets. |
| `axi-dma-sg` | AXI DMA scatter-gather | S2MM-only configuration, ordinary descriptors, status-stream disable, why cyclic/simple transfer are prohibited, and packet closure through TLAST. |
| `la-burst-plan` | Burst planner | Raw versus worst-case transition sizing, descriptor count, 1016-byte chunk alignment implications, capacity rejection, and timeout conversion. |
| `la-dma-driver` | DMA driver | Initialization, finite chain preparation, descriptor submission, actual-length/EOF collection, ISR acknowledgement, stop/reset/rebuild, and sole ownership. |
| `ddr-memory-map` | DDR and linker geometry | Firmware region, reserved non-cached window, capture/descriptor separation, alignment, linker assertions, and MMU programming. |
| `pl-dma-interrupts` | PL and DMA interrupts | `xlconcat` inputs, GIC identities, SDT numbering caveat, ISR acknowledgement, task notifications, and latency counters. |
| `burst-buffer-lifetime` | Burst buffer lifetime | DMA-owned to job-owned transition, busy gate, transmission completion, abort/reset behavior, and when memory becomes reusable. |

Primary sources: `docs/architecture.md`, `docs/rtos-design.md`,
`fpga/scripts/build.tcl`, `fw-zynq/lscript.ld`,
`fw-zynq/translation_table.S`, `fw-zynq/app/src/la_mem_geom.*`,
`fw-zynq/app/src/drivers/la_dma.*`, `fw-zynq/app/src/drivers/la_irq.*`, and
`fw-zynq/app/src/tasks/la_burst_plan.*`.

### 7.4 `zynq-rtos`

Diagram arrangement:

- Put `CmdTask`, `AcqTask`, `TxTask`, and `StatusTask` as the core tasks, and
  the implemented `DecodeUartTask`, `DecodeSpiTask` and `DecodeI2cTask` as the
  live path. Both paths are wired and bench-verified; show the live path in a
  visually distinct region because it runs only while a live session is armed.
- Draw only documented queues/notifications. Do not invent task-to-task calls.

Component slugs:

| Slug | Visible name | Required explanation |
|---|---|---|
| `zynq-app-shell` | Static application shell | Startup, static task/queue/stream-buffer allocation, shared app state, sequence allocation, and failure hooks. |
| `acq-task` | `AcqTask` | `ctrl_q`, CONFIG/ARM/ABORT handling, register programming, DMA ownership, trigger/completion notices, buffer-busy gate, timeout, and soft-reset cleanup. |
| `acq-fsm` | Acquisition state machine | IDLE/ARMED/CAPT transitions, configure/arm/trigger/timeout/abort/complete guards, and host-tested pure-core role. |
| `cmd-task` | `CmdTask` | UART RX parsing, one dispatch step per frame, queueing results, and why it does not transmit directly or touch DMA payloads. |
| `cmd-dispatch` | Command dispatcher | Configuration latch, optimistic acquisition view, error mapping, inbound ACK/NAK routing, PATGEN, live DECODER_CFG/LIVE_START/LIVE_STOP, EVT_CFG, LINK_CFG and TRACE_CTRL support. |
| `tx-task` | `TxTask` | Sole UART TX writer, control/data priorities, burst-job processing, SPI bulk routing, sequence assignment, ACK handling, retransmission, stalled-job abandon, and job release. |
| `tx-packer` | Burst frame packer | DATA_BEGIN/CHUNK/END generation, 1016-byte chunks, streaming CRC32, deterministic chunk rebuild, and empty job behavior. |
| `tx-window` | Transmit window | Maximum 16 outstanding chunks, sequence uniqueness, ACK clear, NAK retransmit marking, stale responses, and no count inflation. |
| `status-task` | `StatusTask` | Periodic watermarks, counters, CPU/stack/IRQ reporting, heartbeat, live latency counters, and what STATUS v1 does not carry. |
| `rtos-queues` | Queues and stream buffers | Producer, consumer, capacity, handle-versus-payload semantics, and full/failure policy for every implemented object. |
| `isr-policy` | ISR policy | PL/DMA/UART/SPI notifications, ISR-safe byte movement, prohibited ISR work, interrupt priority, and interrupted-task stack implications. |
| `irq-statistics` | Interrupt timing statistics | ISR duration and ISR-exit-to-task-wake measurements, min/avg/max calculation, and console visibility. |
| `segment-pool` | Live segment pool | 16 × 256 KiB handles, FREE→DMA_OWNED→CPU_OWNED→FREE, generation/sequence checks, refcounts, queue-full release, and bench status. |
| `decoder-tasks` | Decoder tasks | One task per UART/SPI/I²C decoder, segment handle consumption, static event blocks, multi-producer event queue, discontinuity reset, and the measured rates. |
| `live-event-batch` | Live event batching | `EVT_DECODED` v1 versus compact `EVT_BLOCK` (`EVT_CFG`), delta timestamps with a u64 escape, and the measured wire-byte saving. |

Primary sources: `docs/rtos-design.md`, `fw-zynq/FreeRTOSConfig.h`, and all
files under `fw-zynq/app/src/`. Use the current source to distinguish the four
instantiated application tasks from the three planned decoder tasks.

### 7.5 `protocol-link`

Diagram sequence:

`command object → typed payload → frame encoder → UART byte stream → incremental
parser → type validation/dispatch`

Overlay the burst exchange:

`CONFIG/ACK → ARM/ACK → EVT_TRIGGERED → DATA_BEGIN → DATA_CHUNK × N →
batched ACKs → DATA_END`.

Add the P9.1 SPI bulk path as a second transport under the same frame format:
`TxTask → spi_to_esp ring → axi_quad_spi slave (JD-7..10) → ESP32 SPI master
→ incremental parser → same relay ring`, with `LINK_CFG` selecting the
transport and clock at run time and the UART retaining commands/ACKs.

Component slugs:

| Slug | Visible name | Required explanation |
|---|---|---|
| `frame-format` | Frame format | SOF, type, sequence, little-endian length, 0–1024-byte payload, CRC16, 1031-byte maximum, and embedded SOF handling. |
| `incremental-parser` | Incremental parser | Byte-at-a-time operation, ownership of callback payload, length/CRC rejection, SOF resynchronization, and counters. |
| `message-catalog` | Message catalog | All message types 0x01–0x7F, direction, payload purpose, and current support boundary. |
| `config-payload` | Acquisition configuration | Exact 37-byte layout, validation, control flags, channels, trigger fields, pre/post samples, timeout, encoding, and flush settings. |
| `decoder-config-payload` | Decoder configuration | Exact 20-byte union, three slots, UART/SPI/I²C layouts, channel rules, `stop_on` pattern bytes, and live-session validation. |
| `status-and-events` | Status and event payloads | STATUS v1 layout, EVT_TRIGGERED, EVT_DECODED record batching, compact EVT_BLOCK, EVT_OVERRUN stages, LOG, and missing latency field. |
| `burst-transfer` | Burst transfer | DATA_BEGIN metadata, chunk offsets and size, DATA_END CRC32, ordered reconstruction, timeout/disconnect discard, and exact-size checks. |
| `ack-window` | ACK/NAK flow control | Endpoint sequence spaces, 16-chunk window, batch ACK behavior, retransmission sequence reuse, stale responses, and reserved control capacity. |
| `protocol-errors` | Error model | Bad length/field/state, unsupported, busy, overflow, CRC, and how each layer exposes failures. |
| `zynq-uart-driver` | Zynq UART0 driver | EMIO UART0, 2 Mbps 8N1 since the P7.6 gate, RTS/CTS, RX/TX stream buffers, ISR callbacks, TX staging, and floating-input caveat. |
| `shared-protocol-library` | Shared C protocol library | Allocation-free CRC, validation, pack/unpack, encoding, parsing, common use across Zynq/ESP32/tests, and Python parity. |
| `spi-bulk-link` | SPI bulk link | `axi_quad_spi` v3.2 slave, 256-byte FIFO, data-ready EMIO, whole-frame ring, underrun/CRC resync, 5/10/18 MHz measured ceiling, and the blocked drain rate. |
| `zynq-spi-driver` | Zynq SPI service | `la_spi_link` register driver, `la_spi_core` whole-frame ring, `SpiLinkTask` ISR FIFO refill, TX-only interrupt mask, and ring-space semaphore. |
| `esp32-spi-master` | ESP32 SPI master | SPI3 pins, manual CS setup delay, continuous 1 KiB DMA chunks into the incremental parser, idle-gap resync, and data-ready polling. |
| `link-cfg-routing` | Transport routing | `LINK_CFG` (0x1A) transport and clock bytes, IDLE-only latch, TxTask SPI routing with bounded retry, and commands/ACKs staying on UART. |

The `/reference/protocol` page is the authoritative visual table. Component
pages explain concepts and link back to the relevant table section rather than
duplicating every payload row.

### 7.6 `esp32-gateway`

Diagram order:

`UART2 driver ↔ LinkRxTask/LinkTxTask ↔ static rings ↔ NetServerTask ↔ Wi-Fi
TCP client`; branch valid frames to `live_rb`; show the SPI master merging into
the same relay; show the HTTP server/WebSocket live view, Wi-Fi/mDNS and UiTask
as services.

Component slugs:

| Slug | Visible name | Required explanation |
|---|---|---|
| `esp32-board-config` | Board configuration | Confirmed DOIT ESP32 DevKit V1, GPIO mapping, default baud, task cores/priorities/stacks, buffer sizes, and Kconfig authority. |
| `esp32-protocol-wrapper` | Protocol wrapper | How ESP-IDF compiles the shared C protocol implementation without copying it. |
| `esp32-uart-driver` | UART2 driver | Initialization, FIFO threshold, RTS/CTS, RX pull-up, event queue, read/write/flush operations, and counters. |
| `link-rx-task` | `LinkRxTask` | UART event consumption, frame parse/re-encode, classification, `link_to_net` production, live mirroring, pressure behavior, and watchdog. |
| `link-tx-task` | `LinkTxTask` | `net_to_link` consumption, bounded UART room wait, single ownership, and watchdog/health reporting. |
| `relay-policy` | Relay policy | Direction/role checks, control versus best-effort classification, mirror decision, drop/pressure reporting, and statistics. |
| `esp32-link-rings` | Static ring buffers | 32 KiB link-to-network, 16 KiB network-to-link, 8 KiB live mirror, NOSPLIT item ownership, and full policies. |
| `network-session-policy` | Network session state machine | Wi-Fi/client events, ordered connect/disconnect actions, backoff deadlines, counters, and pure host-test boundary. |
| `wifi-manager` | Wi-Fi manager | Station mode, event handling, retry/backoff, power-save decision, RSSI, and credential exclusion from Git. |
| `tcp-server` | Single-client TCP server | Port 5555, `TCP_NODELAY`, select loop, second-client rejection, relay staging, disconnect ABORT/LIVE_STOP, and stale-ring handling. |
| `mdns-service` | mDNS service | `la-esp32.local`, `_la-analyzer._tcp`, discovery purpose, and dependency version boundary. |
| `esp32-ui-watchdog` | UI, health, and watchdog | LED states, periodic counters, stack/heap reporting, task watchdog subscriptions, and reset purpose. |
| `esp32-web-server` | Embedded web server | `esp_http_server` on port 80, the single-page UI embedded in flash, `CONFIG_HTTPD_WS_SUPPORT`, and the no-external-assets rule. |
| `esp32-live-view` | Live view task | `live_rb` consumer, `/ws` broadcast to up to four clients, compact JSON event framing, 1 s stats message, dead-socket pruning, and the measured bench gate. |
| `wifi-drain-limit` | Wi-Fi drain and jitter limit | The measured 0.28 MB/s host drain, the resolved stale-client history, and the remaining P9.4 jitter/throughput work. Do not invent a root cause. |

Primary sources: `docs/rtos-design.md`, `docs/wiring.md`,
`fw-esp32/main/`, and `fw-esp32/components/`. Do not turn the near-40 kB/s echo
measurements into a universal one-way throughput limit or a profiled root cause.

### 7.7 `host-tools`

Diagram order:

`CLI command → transport → protocol/client → capture validation → streaming
sample reconstruction → selected exporter/viewer`.

For planned live mode, branch `client → decoded event iterator → text/CSV` and
mark the target producer as not implemented.

Component slugs:

| Slug | Visible name | Required explanation |
|---|---|---|
| `host-cli` | `la-cli` commands | `ports`, `ping`, `status`, `capture`, `abort`, and `live`; required target selection; options; exit/failure behavior; and current live boundary. |
| `host-transport` | TCP and serial transports | URI forms, raw byte-stream abstraction, deadlines/EOF, COM10+ normalization, USB-UART identification, and ambiguity policy. |
| `host-protocol` | Python protocol mirror | Constants, CRC, frame parser, typed packing/unpacking, event iterator, and parity with the C ABI. |
| `analyzer-client` | Analyzer client | Per-command sequences, ACK/NAK correlation, logs, deadlines, burst collection, chunk-gap checks, CRC/size verification, and cleanup. |
| `capture-export` | Capture reconstruction and export | Raw versus transition reconstruction, bounded streaming intent, `.sr` structure, VCD timescale, PNG sample cap, and output selection. |
| `host-live-client` | Live event client | DECODER_CFG/LIVE_START/STOP exchange, batching, event count/deadline stop, overrun handling, text/CSV output, mock-test status, and target dependency. |
| `mock-analyzer` | Mock analyzer | Full protocol peer used by host tests, scenarios it can prove, and hardware/timing behaviors it cannot prove. |
| `sigrok-bridge` | Optional SUMP bridge | Current package placeholder, intended TCP-to-virtual-COM translation, signed-driver constraint, and optional P10.2 status. |

Primary sources: `host/la_cli/`, `host/tests/`, `host/la_sigrok_bridge/`,
`docs/protocol.md`, and `docs/plan.md` P6/P7/P10.

## 8. Level 3 — mandatory component-page template

Every entry in the component catalog must render through the same component
detail template. A component record is incomplete unless it supplies every
field below. Use “Not applicable” with a reason only where a concept genuinely
does not apply.

1. **Name and one-sentence answer:** what the component is.
2. **Why it exists:** the problem it solves and what would fail without it.
3. **Where it sits:** parent subsystem plus immediate upstream and downstream
   components.
4. **Inputs:** signals, frames, queues, handles, configuration, or calls.
5. **Outputs:** signals, state changes, events, buffers, counters, or files.
6. **Step-by-step behavior:** normally 3–8 numbered steps.
7. **Important state:** states, counters, ownership, buffering, or timing.
8. **Configuration:** registers, constants, Kconfig values, protocol fields, or
   command options with valid ranges and defaults.
9. **Backpressure and failure behavior:** full queues, stalls, malformed data,
   overflow, timeout, disconnect, abort, or reset as applicable.
10. **Concurrency/ownership:** sole writer, ISR/task boundary, DMA/CPU owner, or
    “pure stateless helper” as applicable.
11. **Current status:** one or more approved status labels with exact scope.
12. **Evidence:** named tests or recorded bench run, date where material, and
    explicit limits of that evidence.
13. **Source anchors:** implementation, specification, and test files.
14. **Related links:** neighboring components and relevant reference page.

Use compact tables for signal/field mappings. Use a state diagram only when a
component has three or more real states. Use a sequence diagram for transfers
or ownership changes. Do not force every page into the same decorative layout.

## 9. Cross-cutting reference pages

### 9.1 Registers

Render all 21 registers from `docs/regmap.yaml`, in address order:

`ID`, `CTRL`, `STATUS`, `SAMPLE_DIV`, `MODE`, `CHAN_EN`, `TRIG_MASK`,
`TRIG_VALUE`, `TRIG_RISE`, `TRIG_FALL`, `PRE_SAMPLES`, `POST_SAMPLES`,
`TRIG_INDEX`, `WORD_COUNT_LO`, `WORD_COUNT_HI`, `IRQ_EN`, `IRQ_STAT`,
`PATGEN_CTRL`, `TIMEOUT`, `TXN_MAX_RUN`, and `LIVE_FLUSH_CYCLES`.

For each, show offset, access (`RO`, `RW`, or `W1C`), reset, description,
fields, constraints, and which component reads/writes it. Explain base address
`0x40000000`, self-clearing control bits, sticky status, W1C behavior,
PRE_SAMPLES maximum 32,768, and why the generated views must never be edited
directly. Do not duplicate the register constants by hand in several files;
keep one website data table sourced from the reviewed YAML values.

### 9.2 Protocol

Include the complete message catalog from `docs/protocol.md`, grouped as:

- discovery/control: PING, PONG, CONFIG, ARM, ABORT, GET_STATUS, PATGEN_CFG;
- live: DECODER_CFG, LIVE_START, LIVE_STOP, EVT_DECODED, EVT_OVERRUN;
- state/events: STATUS, EVT_TRIGGERED, LOG;
- burst: DATA_BEGIN, DATA_CHUNK, DATA_END;
- reliability: ACK, NAK.

The direction and current target support of every message must be visible.
Include exact CONFIG, DECODER_CFG, STATUS, DATA_BEGIN/CHUNK/END layouts and the
error-code table. Show the CRC16 frame boundary separately from the CRC32 whole
capture boundary.

### 9.3 Decoders

Start with the common transition tuple `(t_sample:u64, sample:u8)`, allocation-
free state, callback lifetime, nondecreasing timestamps, discontinuity reset,
and segment-gap behavior. Then give UART, SPI, and I²C separate sections with:

- configuration and channel requirements;
- edge/sample behavior;
- emitted event kinds and timestamp meaning;
- error behavior;
- functional native-test coverage;
- clear boundary: the libraries are native/host-tested, wired into the target
  decoder tasks, and bench-verified at the recorded rates (115200 UART,
  10 kHz SPI/I²C); the 1 MHz SPI case is a deliberate overload test, and the
  isolated-byte latency gate carries a Wi-Fi-jitter caveat.

### 9.4 Wiring

Use tables transcribed from `docs/wiring.md`, not improvised pin names. Include
JC, JE, JD-to-ESP32, ESP32-only loopback, power rules, and JTAG/SD boot notes.
Keep safety warnings prominent and readable.

### 9.5 Verification

Show the verification pyramid:

`models/unit tests → self-checking RTL simulation → native firmware logic tests
→ target Unity/firmware builds → FPGA implementation → bench E2E → release`.

List all canonical runner targets:

`gen`, `sim`, `lint`, `test-native`, `test-esp32-mocks`,
`test-esp32-target`, `bit`, `fw-zynq`, `jtag-zynq`, `fw-esp32`, `boot`,
`package`, `program`, `flash-esp`, `test-host`, `test-e2e`, `hil`, `release`,
`check-env`, and `query-board`.

For each target, state what it validates, whether it requires hardware or a
licensed tool, and where its ignored local report is written. Do not display an
ignored report as a public link. Use the current ledger for pass counts and
dates; state “not run in this checkout” when evidence is absent. Note that
`boot` packages BOOT.BIN and that the release packaging step stages the ESP32
`.bin` set and the Python wheel; the `[HUMAN]` SD boot and the `v1.0` tag are
still pending the ESP32-S3 transport port.

Include the 12 RTL block-to-testbench mappings and the major native/host/E2E
test suites. Explain that mocks and compilation are not target scheduling,
electrical, DMA coherency, or throughput evidence.

### 9.6 Roadmap

Render P0 through P10 individually, not as a single vague progress bar. Each
phase needs intended outcome, current status, completed evidence, remaining
gate, and links to affected subsystems. Current emphasis must remain:

- P0–P6 are complete, including the full P6.2/P6.3 matrices, the PulseView
  `.sr` evidence and the `v0.1-burst` tag.
- P7 is complete on bench: live UART/SPI/I²C decode, the segment pool and live
  SG DMA, the 60 s zero-drop gate, the isolated-byte latency gate and the P7.7
  decoded-pattern stop; `v0.2-live` tag.
- P8 is complete (P8.6 documented skip): fault injection, the 500-capture
  soak, the cached live pool and the trace pipeline with an annotated trace.
- P9.2 is complete; P9.1 is bench-clean for framed bursts at 5/10/18 MHz with
  the drain gate blocked on the ESP32 Wi-Fi relay (0.28 MB/s) and the
  ESP32-S3 USB-OTG port on order; P9.3 and P9.4 remain open.
- P10.1 is bench-verified. P10.2 is deferred (no signed virtual-COM pair
  driver). P10.3 documentation is this contract plus the tracked docs and the
  portal. P10.4 tooling exists; the final BOOT.BIN, [HUMAN] SD boot and
  `v1.0` tag wait for the ESP32-S3 transport port.

### 9.7 Tooling

Explain that `python tools/run.py <target>` is the canonical entry point and
resolves paths from its own location. Cover environment checks, register
generation, native Windows tools, reports under `build/reports`, FPGA/Vitis/
ESP-IDF stages, BOOT.BIN packaging, and generated/ignored directories. State
that WSL, Linux containers, Make, Verilator, and ESP-IDF Linux-host substitutes
are not part of this repository's workflow.

### 9.8 Glossary

At minimum define: logic analyzer, digital channel, sample, sample rate, fabric
clock, metastability, synchronizer, trigger, pre-trigger, post-trigger, FPGA,
PL, PS, Zynq, AXI4-Lite, AXI Stream, ready/valid, backpressure, TLAST, TKEEP,
FIFO, BRAM, DMA, scatter-gather, descriptor, DDR, MMIO, IRQ, ISR, RTOS, task,
queue, stream buffer, ring buffer, ownership, reference count, CRC, frame,
payload, sequence number, ACK, NAK, UART, RTS/CTS, TCP, mDNS, raw packing,
transition encoding, RLE, discontinuity, PulseView, sigrok, VCD, HIL, and W1C.

Definitions must be project-specific where useful. Example: define TLAST as
the AXI-stream marker used here to close a DMA packet, not just as a generic
protocol term.

## 10. Content data model

Create central typed content under `web-artifact/lib/docs/`. Use this shape as
the minimum contract; names may be adjusted only for TypeScript correctness.

```ts
type EvidenceStatus =
  | 'bench-verified'
  | 'target-tested'
  | 'simulation-tested'
  | 'host-tested'
  | 'implemented-unverified'
  | 'planned'
  | 'known-issue'
  | 'blocked';

type SourceRef = {
  label: string;
  path: string;
  kind: 'spec' | 'source' | 'test' | 'tool' | 'generated';
  note?: string;
};

type EvidenceRef = {
  status: EvidenceStatus;
  summary: string;
  scope: string;
  date?: string;
  sources: SourceRef[];
};

type InterfaceItem = {
  name: string;
  direction: 'input' | 'output' | 'bidirectional' | 'internal';
  form: string;
  description: string;
};

type ComponentDoc = {
  slug: string;
  name: string;
  codeName?: string;
  subsystem: SubsystemSlug;
  summary: string;
  why: string;
  upstream: string[];
  downstream: string[];
  inputs: InterfaceItem[];
  outputs: InterfaceItem[];
  steps: string[];
  state: string[];
  configuration: string[];
  failures: string[];
  ownership: string[];
  evidence: EvidenceRef[];
  sources: SourceRef[];
  related: string[];
};
```

Add equivalent `SubsystemDoc`, `RegisterDoc`, `ProtocolMessageDoc`,
`RunnerTargetDoc`, `RoadmapPhase`, and `GlossaryEntry` types. Validate duplicate
slugs, broken related links, orphan components, missing sources, and invalid
status keys with a small repository-local script or a build-time assertion.
Do not fetch or parse repository files at runtime.

## 11. Required website file structure

Use this structure unless the existing framework requires a small naming
adjustment:

```text
web-artifact/
  app/
    page.tsx
    layout.tsx
    globals.css
    subsystems/[slug]/page.tsx
    components/[slug]/page.tsx
    reference/registers/page.tsx
    reference/protocol/page.tsx
    reference/decoders/page.tsx
    reference/wiring/page.tsx
    verification/page.tsx
    roadmap/page.tsx
    tooling/page.tsx
    glossary/page.tsx
    not-found.tsx
  components/docs/
    site-header.tsx
    breadcrumbs.tsx
    status-badge.tsx
    source-links.tsx
    system-diagram.tsx
    subsystem-diagram.tsx
    context-strip.tsx
    interface-table.tsx
    evidence-panel.tsx
    related-links.tsx
  lib/docs/
    types.ts
    snapshot.ts
    subsystems.ts
    components.ts
    registers.ts
    protocol.ts
    verification.ts
    roadmap.ts
    glossary.ts
    validate.ts
```

Do not modify vendored `components/ui/*` files. Reuse existing primitives:
`Tabs` for mode/reference groupings, `Breadcrumb` for location, `Table` for
exact mappings, `Badge` for statuses, `Accordion` only for secondary detail,
`Sheet` for mobile navigation, and `Tooltip` only for supplemental definitions.
Essential qualifications must remain visible without hover or expansion.

## 12. Visual and interaction specification

Preserve the dark instrumentation/workbench aesthetic already established.
Extend it with these rules:

- The Level 1 diagram is the dominant shape of the first viewport.
- FPGA pages may use compact waveform motifs; RTOS pages may use ownership and
  scheduling lanes; protocol pages may use byte fields; host pages may use file
  flow. These are functional diagrams, not illustrations.
- Use a consistent visual vocabulary: solid border for implemented, double or
  highlighted border for bench/target evidence, dashed border for planned, and
  warning hatch or icon for known issues. Always include text.
- Never animate data indefinitely. Respect `prefers-reduced-motion` and use
  motion only for a direct selection transition.
- Main body text is at least 16 px; regular labels are at least 14 px; secondary
  metadata may be 12–13 px. Support 200% text enlargement.
- Desktop content width should favor readable technical pages rather than full-
  bleed paragraphs. Wide tables must scroll inside labeled containers.
- At 320 px width there must be no page-level horizontal overflow. Diagrams
  collapse to vertical ordered cards; tables use horizontal scrolling.
- Native links and buttons retain their semantics. No clickable `div` elements.
- Use visible focus, skip-to-content, semantic headings, table captions, SVG
  titles/descriptions, and ARIA only where native HTML is insufficient.
- Do not hide the status qualification in a tooltip, hover card, or color key.

## 13. Coverage manifest

Add a reviewed, static source-coverage manifest to the content data. Every
project-owned file in the following areas must map to at least one subsystem,
component, reference page, or tooling page:

- `fpga/rtl/*.sv`, `fpga/tb/*.sv`, `fpga/scripts/*.tcl`,
  `fpga/constraints/*.xdc`, and `fpga/board_files/**`;
- `common/la_proto/**` and `common/la_decode/**`;
- `fw-zynq/app/src/**` (including `live/la_spi_core.*`, `live/la_event_batch.*`,
  `drivers/la_spi_link.*`, `tasks/la_spi_task.c`), `fw-zynq/FreeRTOSConfig.h`,
  `fw-zynq/lscript.ld`, `fw-zynq/translation_table.S`, and
  `fw-zynq/scripts/**`;
- `fw-esp32/main/**`, `fw-esp32/components/**` (including `la_web/**` and
  `la_spi_link` paths), `fw-esp32/tests/**`, `fw-esp32/sdkconfig.defaults`, and
  `fw-esp32/scripts/**`;
- `host/la_cli/**`, `host/la_sigrok_bridge/**`, and `host/tests/**`;
- `tests/native/**`, `tests/hil/**`, and `tests/e2e/**` (including
  `test_p91_spi.py` and `test_p10_web.py`);
- `tools/*.py`, `tools/models/**`, `tools/win/**`, tool/board locks,
  `CMakeLists.txt`, `pyproject.toml`, `Justfile`, and workflow files;
- all normative documentation under `docs/` (including `docs/spi-link.md`,
  `docs/limits.md` and `docs/sigrok-bridge.md`).

Exclude generated, dependency, cache, build, captured-data, and Site-output
directories. A file may map to multiple pages. A placeholder directory or file
must map to a page that explicitly calls it planned/empty; it must not be
silently presented as implemented.

Add a validation check that fails when:

- a component slug has no Level 3 page;
- a Level 2 diagram node has no component record;
- a source reference points outside the repository or to a missing tracked file;
- a covered source file has no content destination;
- a status key is unknown;
- a related component link is broken;
- two records use the same slug.

The check is a documentation-maintenance guard, not a runtime crawler. It may
use a curated manifest rather than scanning ignored directories.

## 14. Ordered implementation work packets

Complete these packets sequentially. Do not start a second site initializer and
do not split overlapping edits across parallel agents.

| ID | Work | Files | Acceptance |
|---|---|---|---|
| D01 | Re-read repository rules, current status, current site, content review, and this specification. Refresh the Sites execution profile before any build. | Read-only plus ignored profile file | Baseline and existing Site identity are understood; no new registration or dependency install. |
| D02 | Extract the current theme/layout into reusable documentation shell components without changing the overview's corrected claims. | `app/layout.tsx`, `app/globals.css`, `components/docs/*` | Header, mobile navigation, footer, skip link, and content shell work on all routes. |
| D03 | Create typed content model, evidence snapshot, status vocabulary, seven subsystem records, and validation helpers. | `lib/docs/types.ts`, `snapshot.ts`, `subsystems.ts`, `validate.ts` | Seven unique subsystem slugs in the required order; build-time validation can fail clearly. |
| D04 | Refactor `/` into the Level 1 architecture surface with three selectable flows and links to all subsystem pages. Preserve corrected mode, rate, evidence, and open-work content. | `app/page.tsx`, `components/docs/system-diagram.tsx`, CSS | First viewport exposes the architecture; keyboard/mobile behavior works; diagrams are labeled illustrative. |
| D05 | Create the generic subsystem route/template and all seven subsystem content records and diagrams. | `app/subsystems/[slug]/page.tsx`, `components/docs/subsystem-diagram.tsx`, `lib/docs/subsystems.ts` | Every Level 1 node opens its matching Level 2 page; every listed component node is visible and linked. |
| D06 | Add all FPGA, physical-I/O, and DMA/memory Level 3 component records. | `lib/docs/components.ts` or split data files | Every component listed in §§7.1–7.3 satisfies the 14-field detail template and has correct source/test anchors. |
| D07 | Add all Zynq RTOS and protocol/link Level 3 component records. | Component data files | Implemented burst and planned live responsibilities are visually and textually distinct; every queue/owner/full policy is represented. |
| D08 | Add all ESP32 and host Level 3 component records. | Component data files | Gateway tasks/rings/network policy and host CLI/transport/client/export/live boundaries are complete; stale-client issue is not overstated. |
| D09 | Create the generic component route and reusable detail sections. | `app/components/[slug]/page.tsx`, `components/docs/*` | Every component slug resolves; breadcrumbs/context/inputs/outputs/steps/state/config/failure/ownership/evidence/sources/related links render. |
| D10 | Implement register, protocol, decoder, and wiring references from the reviewed normative values. | `app/reference/**`, `lib/docs/registers.ts`, `protocol.ts` | All registers/messages/decoder rules/pins listed in §9 are present and cross-linked; no runtime file parsing. |
| D11 | Implement verification, roadmap, tooling, glossary, and source coverage. | Corresponding routes/data; validation script | All runner targets and P0–P10 phases are represented; coverage validation passes; unverified gates remain explicit. |
| D12 | Complete responsive/accessibility pass, metadata, static parameter generation, not-found behavior, link review, and copy review. | Routes/components/CSS | 320 px and desktop layouts are coherent by source review; headings and link labels are meaningful; no essential content is hover-only. |
| D13 | Run the production build and documentation validation, review the diff, update `docs/status.md`, commit, push to `origin/main`, and verify the remote hash. | Website plus ledger | Build and validation pass or exact blocker is recorded; only intended files staged; origin/main matches HEAD. Do not publish the Site. |

## 15. Verification commands and evidence

Use the project-local Sites execution-profile script before building, following
the active Sites skill. Then run the website's existing production build using
its declared package manager. Do not replace the lockfile or reinstall packages
when `node_modules` is already valid.

Minimum checks:

1. Content-data validation/coverage check.
2. Production build from `web-artifact/`.
3. Route-generation review: all seven subsystem slugs and every component slug
   appear in generated route parameters.
4. Broken-link/source-path validation.
5. `git diff --check`.
6. `git diff --stat` and staged-file review.

Browser interaction testing was not requested by this instruction-writing task.
For the later implementation, perform browser QA only if the user requests it;
otherwise report responsive/accessibility results as source/build review, not
as observed browser proof.

Do not run `python tools/run.py release` for website prose/navigation changes:
it exercises unrelated native/RTL gates. Do not rerun bench tests. Cite the
existing evidence with its date and scope.

## 16. Final acceptance checklist

The implementation is complete only when every box below is true:

- [ ] `/` presents the seven-node Level 1 architecture in the first viewport.
- [ ] Capture-data, command, and status/error flows are separately understandable.
- [ ] Every Level 1 node links to exactly one Level 2 subsystem page.
- [ ] Every required Level 2 component node links to a Level 3 component page.
- [ ] Every Level 3 page supplies all 14 required detail fields.
- [ ] The physical I/O, FPGA, DMA/memory, RTOS, protocol/link, ESP32, and host areas are all covered.
- [ ] Registers, messages, decoder semantics, wiring, verification, roadmap, tooling, and glossary have dedicated reference views.
- [ ] All 21 registers and all protocol message types are present with exact values.
- [ ] All 12 RTL tests map to their RTL blocks.
- [ ] Every canonical runner target is explained with its actual validation boundary.
- [ ] Implemented burst/live behavior and deferred or open work are never
      blended; the sigrok bridge and P9.3 stretch are labeled deferred/open.
- [ ] JE→JC loopback evidence is not presented as external-source or 100 MS/s
      input validation; the external-input matrix remains visible as open.
- [ ] The ESP32 gateway is described with the measured Wi-Fi drain/jitter
      limit; the resolved stale-client history is labeled historical.
- [ ] The segment-pool, decoder-task and live-view pages reflect the
      bench-verified implementation, with their open limits explicit.
- [ ] Essential warnings and evidence qualifications are visible without hover.
- [ ] Navigation, breadcrumbs, related links, and previous/next links form no dead ends.
- [ ] Mobile diagrams become ordered vertical flows and the page has no intentional horizontal overflow.
- [ ] Body text, labels, focus treatment, headings, tables, and diagrams meet §12.
- [ ] Coverage validation reports no orphan source area, broken slug, duplicate slug, or missing source file.
- [ ] The production build passes.
- [ ] `docs/status.md` records exact checks, changed files, limitations, and next action.
- [ ] The intended change is committed and pushed to `origin/main`, with remote hash verified.
- [ ] The existing Site has not been published or replaced without separate user authorization.

## 17. Pasteable master prompt for the implementation model

> Expand the existing `web-artifact/` according to
> `docs/web-artifact-implementation-instructions.md`. Treat that file as the
> complete implementation contract and follow D01–D13 in order. First read
> `AGENTS.md`, `docs/status.md`, `docs/web-artifact-content-review.md`, and the
> current website source. Preserve the current Vinext stack, lockfile, Site
> project ID, visual identity, and all corrected evidence wording. Implement the
> seven-node Level 1 system diagram, seven data-driven Level 2 subsystem routes,
> every listed Level 3 component record and route, all reference pages, and the
> source-coverage validation. Do not invent project behavior or promote planned,
> simulated, host-tested, or internal-source behavior to bench-verified status.
> Use current source and the newest specific ledger evidence if the repository
> has advanced beyond the recorded baseline, and document any reconciled facts.
> Do not add packages, change firmware/RTL/specifications, rerun hardware tests,
> register a Site, or publish. Run the documentation validation and production
> build, update the required ledger, commit the intended files, push to
> `origin/main`, and verify the remote hash. Report what is complete and leave
> every remaining evidence gap explicit.
