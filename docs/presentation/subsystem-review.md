# Independent subsystem-deck review

One fresh-eye review read all 37 PDF slides and the full notes on 2026-10-12.
The reviewer judged both independent routes understandable; the embedded route
needed a more explicit sample-format contract on the visible slides. Draft
score: 59/70 (before corrections). No second review score is claimed. The deck
later grew to 38 slides with the real UART screenshot, hardware photograph, and
decoded-frame close-up; this review remains the historical 12 October record.

| # | Slides | Finding | Decision and correction |
|---|---|---|---|
| 1 | 7 | IRQ/settings arrows end in empty space; “sample → trigger → pack” implies a serial trigger stage | Adopt: direct connected firmware-to-register and hardware-to-PS IRQ arrows; “sample, trigger, pack” |
| 2 | 15 | Control fan-out omits la_top; memory SmartConnect is not shown; clock label crosses the connector | Adopt: separate sc_lite and sc_dma blocks; fan-out to RTL and DMA; clock/reset text moved clear |
| 3 | 19/25/28 | Embedded route skips the FPGA page defining run words | Adopt: restate raw/run layout and show count-to-sample-time-to-C-decoder processing on slide 25 |
| 4 | 36 | FPGA route sees 1.62 MB/s without the workflow scope | Adopt: shared close labels acquire–transfer–verify throughput and explains buffered sampling |
| 5 | 19 | PC is a command source but USB only points outward | Adopt: explicit incoming CMD/ACK and outgoing USB result paths |
| 6 | 23 | AcqTask's capture-register control is missing | Adopt: capture/DMA API diagram with both control branches; IRQ→ISR→task-notification explanation |
| 7 | 20 | Priority ordering is only in the notes | Adopt: visible “Higher number = higher priority; all tasks on CPU0” |
| 8 | 10 | “Any selected channel” edge-mask semantics missing | Adopt: explain nonempty masks and empty-mask bypass on the slide |
| 9 | 23/29/30 | Important interface shorthand lacks local explanation | Adopt: valid-byte/packet-end explanation, descriptor wording, RX stream-buffer and SOF/SEQ/LEN definitions; RTL/IP/IRQ definitions added |
| 10 | 28 | Decoder inputs/outputs are visible but algorithms mostly in notes | Adopt: UART bit centers, SPI selected edges, I²C transitions; identify shared C implementation |
| 11 | 34 | Inactive channels dominate the screenshot; annotation details are small | Adopt: use normal UI controls to show CH0 and decoded annotations with cursor A; synthetic/offline qualification remains visible |
| 12 | 24/32 | Transfer-rate wording differs from the actual timed workflow | Adopt: consistent acquire–transfer–verify throughput label |

Optional audience-facing wording suggestions were also adopted on the menu and
hardware headings. Route references use actual PowerPoint positions; footer
body-page numbers exclude covers. Native custom shows and internal slide jumps
are checked separately. No attribution, source footer or course-specific
section label was restored. The original 24-slide comparison files remain
hash-identical. Remaining limits are the documented hardware/measurement scope
and human visual/release gates, rather than a new hardware verification claim.
