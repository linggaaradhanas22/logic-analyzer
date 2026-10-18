"""Author the project deck on the consulting skill's existing HTML foundation.

All paths derive from this script. Generated artifacts stay in build/presentation.
This is presentation content/adaptation, not a replacement slide-layout engine.
"""
from pathlib import Path
import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build" / "presentation"
AUTHORS = os.environ.get("DECK_AUTHORS", "Lingga Aradhana Sahadewa")
NAVY = "#173A56"
GRAY = "#E8EDF1"
WHITE = "#FFFFFF"


def text(x, y, value, size=20, color=NAVY, weight=400, anchor="start"):
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" font-weight="{weight}" text-anchor="{anchor}">{html.escape(value)}</text>'


def box(x, y, w, h, title, lines=(), fill=GRAY):
    color = WHITE if fill == NAVY else NAVY
    border = f' stroke="{NAVY}" stroke-width="1.5"' if fill == WHITE else ""
    s = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}"{border}/>'
    s += text(x + 15, y + 29, title, 21, color, 700)
    for i, line in enumerate(lines):
        s += text(x + 15, y + 57 + 25 * i, line, 18, color)
    return s


def arrow(x1, y1, x2, y2, label="", dashed=False):
    s = f'<path d="M{x1},{y1} L{x2},{y2}" fill="none" stroke="{NAVY}" stroke-width="2.3" marker-end="url(#arr)"' + (' stroke-dasharray="7 5"' if dashed else '') + '/>'
    if label:
        s += text((x1 + x2) / 2, (y1 + y2) / 2 - 10, label, 17, anchor="middle")
    return s


def svg(body, label):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 780 490" role="img" aria-label="{html.escape(label)}" style="font-family:Arial,sans-serif"><defs><marker id="arr" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0,0 L8,4 L0,8" fill="{NAVY}"/></marker></defs>{body}</svg>'


def bullets(items):
    return '<ul>' + ''.join(f'<li>{x}</li>' for x in items) + '</ul>'


def columns(left_heading, left, right_heading, items, ratio="diagram"):
    return f'<div class="two {ratio}"><div><h3 class="colh">{left_heading}</h3>{left}</div><div><h3 class="colh">{right_heading}</h3>{bullets(items)}</div></div>'


def table(headers, rows, widths=None):
    cols = '<colgroup>' + ''.join(f'<col style="width:{w}%">' for w in widths) + '</colgroup>' if widths else ''
    return '<table class="eq">' + cols + '<thead><tr>' + ''.join(f'<th>{h}</th>' for h in headers) + '</tr></thead><tbody>' + ''.join('<tr>' + ''.join(f'<td class="ax">{c}</td>' if i == 0 else f'<td>{c}</td>' for i, c in enumerate(row)) + '</tr>' for row in rows) + '</tbody></table>'


SLIDES = []
ROUTES = {}


def add(title, body, category, source, notes, part="b09"):
    SLIDES.append(dict(title=title, body=body, category=category, source=source, notes=notes, part=part))


def base_content():
    add("A Zynq-based logic analyzer makes digital timing visible",
        '<div class="cover-copy"><p class="eyebrow">FINAL PROJECT DESIGN</p><div class="cover-title">RTOS Logic<br>Analyzer</div><p class="cover-desc">The FPGA captures digital signals.<br>FreeRTOS manages capture and decoding.<br>USB connects the device to the PC viewer.</p><p class="cover-author' + (' long' if len(AUTHORS) > 40 else '') + '">' + html.escape(AUTHORS) + '</p><p class="cover-meta">Zybo Z7-10 · October 2026</p></div>' +
        '<div class="cover-art">' + svg(text(35,65,"DIGITAL SIGNAL → EXPLAINABLE BEHAVIOR",21,weight=700) +
        '<path d="M35 170 H105 V105 H175 V170 H245 V105 H315 V170 H385 V105 H455 V170 H525 V105 H595 V170 H690" fill="none" stroke="#173A56" stroke-width="5"/>' +
        box(35,245,195,105,"FPGA",["Capture timing"],NAVY) + arrow(230,295,285,295) + box(285,245,195,105,"Firmware",["Decode meaning"]) + arrow(480,295,535,295) + box(535,245,205,105,"PC",["Inspect results"],WHITE) + text(35,420,"8 channels   /   100 MS/s   /   USB-first",24,weight=700),"FPGA to firmware to PC") + '</div>',
        None,"README.md; docs/plan.md, V00 scope freeze",
        "Open with the device's purpose: observe what actually happened on digital wires, then turn those samples into readable protocol behavior. RTOS means real-time operating system. FPGA means field-programmable gate array. The final product contains the Zybo board and the PC; no ESP32 is needed. The FPGA and ARM processors are two parts of the same Zynq chip. This is an engineering explanation backed by existing simulation, routed-build and bench records.","b01")

    add("Fast capture needs hardware; decoding and control need firmware",
        table(["Topic","What this presentation explains","Read next"],[
            ["1 · Problem","Why we need to see the signals, not just the software logs.","P.2–3"],
            ["2 · Architecture","How the FPGA, firmware and PC work together.","P.4–15"],
            ["3 · Results","What the device can do and what the tests confirmed.","P.16–20"],
            ["4 · Conclusion","The limits of the working device and the remaining release checks.","P.21–22"]],[24,60,16]),
        None,"README.md; docs/status.md, USB pivot / unified UI / SD boot",
        "Use this page as the route through the presentation. Start with the debugging problem and why a split architecture is needed. Most of the talk walks a sample from the input header through the FPGA, DMA, DDR, tasks and USB. End with the results and the honest boundary of what has been qualified. Printed page numbers exclude the cover and back cover.","b02")

    add("Software logs do not show everything that happened on the wires",
        table(["Debugging problem","What logs can miss","How the analyzer helps"],[
            ["Signal timing","The order of clock and data changes","Samples all channels together; 10 ns intervals at 100 MS/s"],
            ["Occasional failures","What the signals did just before a failure","Saves samples from before and after a trigger"],
            ["Protocol errors","Bad frames, missing acknowledgments or wrong bytes","Shows UART, SPI and I²C messages with sample timestamps"],
            ["Fast signals","Signal changes between software observations","Captures in hardware, independently of software scheduling"]],[25,35,40]),
        1,"Project motivation: README.md; supported capabilities: docs/decoders.md, docs/regmap.md",
        "These are engineering motivations, not a market survey or a claim that logging is useless. A log explains software state, but an external signal can have a different timing history. UART is universal asynchronous receiver/transmitter; SPI is serial peripheral interface; I²C is inter-integrated circuit. The device records digital high/low states; it does not measure analog voltage, rise time or signal integrity like an oscilloscope.")

    add("The Zybo captures signals and sends results straight to the PC",
        columns("Board connections",svg(box(30,75,180,130,"Target board",["3.3 V signals","Common ground"],WHITE) + arrow(210,140,280,140,"JC") + box(280,65,220,150,"Zybo Z7-10",["Zynq-7010","FPGA + ARM"],NAVY) + arrow(500,140,580,140,"J10 USB") + box(580,75,180,130,"Windows PC",["la-ui server","Browser"],WHITE) + box(280,290,220,100,"J12",["Power / JTAG / console"]) + arrow(390,290,390,215) + text(30,448,"Navy = analyzer board; gray = support port; white = connected equipment",15),"Final direct USB physical system"),
        "What the device does",[
            "The FPGA samples <b>eight 3.3 V channels</b> at up to <b>100 million samples/s</b>.",
            "You can record a triggered capture or watch decoded messages in live mode.",
            "J10 carries USB data. J12 provides board power, JTAG and the console connection.",
            "The PC displays waveforms and live messages, and saves captures to files."
        ]),1,"README.md; docs/using-the-analyzer.md; docs/wiring.md; docs/plan.md, V00",
        "Point to the wiring: target signals enter JC with a common ground. The separate J10 connector carries direct Zynq USB data. J12 is the FTDI power/JTAG/console connector; a console COM port is not the analyzer endpoint. This is the ESP32-free final system requested for the deliverable. The UI is served locally by the Python host process, not by the FPGA or an embedded wireless server. At the maximum rate, one sample interval is 10 ns.")

    add("The Zynq captures data in hardware and controls it in software",
        columns("How data moves through the system",svg(
            box(25,65,160,105,"Digital inputs",["CH0–CH7 / JC"],WHITE) + arrow(185,115,245,115) + box(245,55,240,125,"FPGA / PL",["Sample, trigger, encode","100 MHz clock"],NAVY) + arrow(485,115,550,115,"AXIS") + box(550,65,205,105,"AXI DMA",["Stream → DDR"],NAVY) + arrow(655,170,655,250) + box(550,250,205,115,"DDR memory",["Burst / live pools"]) + arrow(550,305,485,305) + box(245,240,240,135,"ARM / PS firmware",["FreeRTOS tasks","Control + live decode"]) + arrow(245,323,185,323) + arrow(185,288,245,288,"USB",True) + box(25,250,160,115,"PC host",["Server + browser"],WHITE) + arrow(365,240,365,180,"",True) + text(386,220,"AXI-Lite",17) + text(25,430,"Solid = sample/results data; dashed = commands/register control",17) + text(25,465,"Navy = programmable logic; gray = processor/memory; white = external",15),"Data and control architecture"),
        "The main parts of the Zynq",[
            "The <b>programmable logic (PL)</b> is the FPGA hardware that samples the signals.",
            "The <b>processing system (PS)</b> contains the ARM processor. FreeRTOS runs on CPU0.",
            "<b>Direct memory access (DMA)</b> writes samples to <b>DDR memory</b> without the CPU copying each sample.",
            "AXI is the on-chip interface: <b>AXI-Stream</b> carries samples, while <b>AXI-Lite</b> carries register commands."
        ]),2,"docs/architecture.md; fpga/scripts/build.tcl; fw-zynq/app/src/la_app.h",
        "Follow the solid arrows clockwise. The input is synchronized and sampled in PL, packed into a 32-bit AXI4-Stream (AXIS), moved by S2MM DMA to DDR, and then read by the CPU for burst transfer or live decoding. The ARM never polls one GPIO per sample. In the opposite direction, user settings arrive over USB, are validated in firmware, and reach the FPGA as AXI-Lite register writes. DMA and registers have their own interrupt signals back to the processor. DDR is served by the Zynq PS memory controller, not a custom DDR PHY.")

    add("The FPGA captures the signals; firmware manages buffers and decoding",
        table(["Responsibility","FPGA / programmable logic","ARM / FreeRTOS firmware"],[
            ["Read the inputs","Synchronize and sample every channel on the same clock","Choose the channels and sample rate"],
            ["Capture an event","Detect the trigger and keep the requested samples","Check capture size, start or stop capture, and report status"],
            ["Format and decode","Pack raw samples or store repeated values as runs","Recover sample timing and decode UART, SPI and I²C"],
            ["Manage memory","Send sample packets to DMA","Assign DDR buffers to DMA and reuse them only when safe"],
            ["Send results","Provide status registers and interrupts","Build USB messages and control the transfer rate"]],[24,38,38]),
        2,"fpga/rtl/la_top.sv; fw-zynq/app/src/tasks/; docs/rtos-design.md",
        "This is the central design choice. Hardware is responsible for the 10 ns observation schedule because software scheduling must not affect signal capture. Firmware is responsible for semantic decisions, ownership and USB. The DMA IP is hardware, but firmware decides which descriptors and buffers it may use. Protocol decoding is C firmware, not a set of FPGA UART/SPI/I²C decoders. Host-side offline decoding reuses the shared C decoder library. The boundary supports changing protocol behavior without rebuilding the acquisition datapath.")

    add("The FPGA samples, triggers and packages data for DMA",
        columns("The FPGA capture pipeline",svg(
            box(25,40,220,100,"Input synchronizer",["Two flip-flops / channel"],NAVY) + arrow(245,90,300,90) + box(300,40,215,100,"Sampler",["100 MHz / (DIV + 1)"],NAVY) + arrow(515,90,570,90) + box(570,40,185,100,"Trigger",["Level + edges"],NAVY) + arrow(665,140,665,220) + box(535,220,220,100,"History ring",["Pre/post sample order"],NAVY) + arrow(535,270,480,270) + box(280,220,200,100,"Mode selection",["Raw or run encoding"],NAVY) + arrow(280,270,225,270) + box(25,220,200,100,"Output FIFO",["8 KiB / AXIS"],NAVY) + arrow(125,320,125,385) + box(25,385,200,75,"To AXI DMA",[],NAVY) + box(280,375,475,85,"Capture controller + AXI-Lite registers",["ARM, ABORT, packet close, status, interrupts"]) + text(280,485,"Navy = sample datapath; gray = coordination",16),"FPGA acquisition pipeline"),
        "What each stage does",[
            "Input signals are not aligned to the FPGA clock. The synchronizer reduces the chance of unstable reads.",
            "The sampler reads all enabled channels together at the chosen sample rate.",
            "The history ring keeps samples in order, including the transition from before to after the trigger.",
            "The FIFO buffers short delays. If it fills up, the device reports an overflow instead of hiding lost data."
        ]),2,"fpga/rtl/la_top.sv; docs/architecture.md; docs/limits.md",
        "Explain that the trigger observes the sampled values while the ring stores their ordered history; the diagram is a functional view rather than a claim that the trigger transforms samples. Acquisition uses a single 100 MHz fabric clock. Configuration is coordinated at arm/enable boundaries. A synchronizer is not an electrical protection circuit and cannot guarantee observation of pulses shorter than a sample interval. The FPGA cannot pause the real-world source when downstream TREADY is low, so an 8 KiB FIFO only provides a bounded stall budget: about 82 microseconds at raw 100 MB/s.")

    add("A triggered capture includes samples from before and after the event",
        columns("Triggered sample window",svg(
            '<rect x="50" y="105" width="300" height="255" fill="#E8EDF1"/><rect x="350" y="105" width="360" height="255" fill="#EEF4F8"/>' +
            text(75,150,"PRE-TRIGGER HISTORY",21,weight=700) + text(380,150,"POST-TRIGGER WINDOW",21,weight=700) +
            '<path d="M50 275 H150 V210 H235 V275 H350 V210 H465 V275 H585 V210 H710" fill="none" stroke="#173A56" stroke-width="4"/><path d="M350 95 V375" stroke="#173A56" stroke-width="2" stroke-dasharray="6 5"/>' +
            text(350,75,"First sample that matches the trigger",21,weight=700,anchor="middle") + arrow(70,400,710,400,"Sample time →") + text(50,450,"History: up to 32,768 samples",20) + text(50,480,"Gray = retained history; pale blue = post-trigger samples",16),"Pre and post trigger timing illustration"),
        "How the trigger works",[
            "Firmware sets which signal levels and rising or falling edges should trigger the capture.",
            "After arming, the FPGA marks the first matching sample and reports its position.",
            "The history ring supplies the requested samples from before the trigger, in the original order.",
            "Capture stops after the requested post-trigger samples, or when it times out or is aborted."
        ]),2,"docs/limits.md; docs/regmap.md; fpga/rtl/la_trigger.sv, la_pretrig_ring.sv",
        "The waveform here is an explanatory illustration, not a measured trace. A trigger is computed in the FPGA on sampled values. Level matching is combined with optional any-rising and any-falling conditions; empty edge masks do not require an edge. The public maximum pre-trigger request is 32,768 samples although the internal ring holds 65,536 entries. With zero pre-history, the burst starts at the trigger sample. Do not call this a hardware protocol trigger: the decoded-byte stop discussed later is a firmware live-stop mechanism.")

    add("Repeated samples can be stored as runs to save space",
        columns("Two ways to store the same samples",svg(
            text(30,60,"SAMPLE VALUES",22,weight=700) +
            ''.join(box(30+i*84,90,76,63,v,[],NAVY) for i,v in enumerate(["03","03","03","03","03","07","07","07"])) +
            box(30,225,330,120,"Raw packing / 8 bytes",["8 sample bytes","2 × 32-bit words"],WHITE) +
            box(410,225,340,120,"Run encoding / 8 bytes",["(run=5, sample=03)","(run=3, sample=07)"],WHITE) +
            text(30,395,"32-bit word: 24-bit run count | 8-bit sample vector",23,weight=700) +
            text(30,435,"Both use 8 bytes here. Longer runs can save space.",20) + text(30,470,"A change at every sample can make run encoding 4× larger.",19),"Raw packing versus run encoding example"),
        "How timing is preserved",[
            "Raw mode stores four 8-bit samples in each 32-bit word. Flags identify valid bytes in the last word.",
            "Run encoding stores an 8-bit value for all channels and the number of samples with that value.",
            "The decoder uses the run length to recover timing. Splitting a run does not add a signal edge.",
            "In live mode, a 2 ms timer asks for a packet to close. At low sample rates, it waits for the next valid sample."
        ]),2,"docs/protocol.md; docs/plan.md §1.1; fpga/rtl/la_txn_encoder.sv, la_raw_packer.sv",
        "Hexadecimal values 03 and 07 illustrate the entire 8-channel vector. In this short example both encodings cost 8 bytes; compression becomes useful with longer repeated runs. Each encoded word always costs four bytes, so a value change at every sample can expand a raw one-byte sample stream to four bytes per sample. At 100 MS/s that worst case is 400 MB/s, which leaves no stall margin on a 32-bit 100 MHz stream. Live flushing emits the pending unchanged run before TLAST. The timer is based on the fabric clock; at very low selected sample rates a flush waits for a real valid sample.")

    add("DMA writes samples to memory; firmware controls when buffers are reused",
        columns("DMA connections",svg(
            box(25,75,200,105,"la_top / AXIS",["32-bit data","TKEEP + TLAST"],NAVY) + arrow(225,125,285,125) + box(285,75,205,105,"S2MM DMA",["Scatter-gather"],NAVY) + arrow(490,125,555,125,"HP0") + box(555,75,200,105,"PS DDR",["Capture memory"]) +
            box(285,280,205,110,"AcqTask",["Descriptor owner","Prepare / collect"]) + arrow(390,280,390,180,"AXI-Lite",True) + arrow(490,335,655,335,"BDs") + arrow(655,335,655,180) +
            arrow(125,180,285,315,"",True) + text(30,240,"PL IRQ",17) + arrow(455,180,455,280,"",True) + text(478,239,"DMA IRQ",17) + text(25,430,"Registers: la_top 0x40000000 · DMA 0x40400000",19) + text(25,467,"Solid = data/descriptor access; dashed = control/IRQ notifications",17),"AXI DMA data and control planes"),
        "Rules for using the buffers",[
            "A scatter-gather (SG) descriptor tells DMA where to write and how much space is available. DMA reports how many bytes it wrote.",
            "Burst mode uses a fixed descriptor chain. Live mode reuses a buffer only after every decoder has released it.",
            "Descriptors and sample buffers start on 64-byte boundaries. Descriptors are kept outside the CPU cache.",
            "Before reading a cached live buffer, firmware invalidates its old cache entries so the CPU sees the DMA-written data."
        ]),2,"docs/architecture.md; docs/rtos-design.md, P4.6/P8.5; fw-zynq/app/src/drivers/la_dma.c",
        "S2MM means stream-to-memory-mapped. HP0 is the processor's high-performance memory port. The DMA's memory side is 64-bit while the incoming stream is 32-bit. A buffer descriptor (BD) is bookkeeping, not a payload copy. The ordinary live SG ring is managed explicitly by AcqTask; it is not hardware cyclic DMA. Neither simple-transfer calls nor cyclic mode are used. Actual lengths matter because timer-closed packets often occupy only a fraction of a 256 KiB segment. Keep descriptor coherency distinct from payload coherency. AXI-Lite commands do not carry the acquisition sample stream.")

    add("Burst mode records a capture, then sends it to the PC",
        columns("Steps in a burst capture",svg(
            box(50,25,680,72,"Check the settings",["CmdTask rejects captures larger than the 8 MiB buffer"]) +
            arrow(390,97,390,125) + box(50,125,680,72,"Prepare DMA, then arm the FPGA",["AcqTask sets up the descriptors before capture starts"]) +
            arrow(390,197,390,225) + box(50,225,680,72,"Trigger and capture",["FPGA emits ordered samples; DMA writes the completed buffer"],NAVY) +
            arrow(390,297,390,325) + box(50,325,680,72,"Transfer and verify",["TxTask sends DATA_BEGIN → DATA_CHUNK → DATA_END"]) +
            arrow(390,397,390,425) + text(390,460,"Release the buffer only after transfer completion",22,weight=700,anchor="middle"),"Burst lifecycle"),
        "Why transfer follows capture",[
            "At 100 MS/s, raw samples arrive at <b>100 MB/s</b>. The measured USB transfer rate is <b>1.62 MB/s</b>.",
            "DDR holds the capture while the slower USB link transfers it to the PC.",
            "Firmware blocks the next capture until the current buffer is no longer being transferred.",
            "Checksums detect corrupted data. Firmware rejects requests that are too large for the capture buffer."
        ]),2,"docs/rtos-design.md, P4.6–P4.9; docs/limits.md; build/reports/usb-u06-performance.md",
        "The 100 MB/s figure is the calculated raw byte production rate: 100 million sample vectors per second times one byte. It is not a measured USB rate. A full 8 MiB raw buffer covers approximately 83.9 ms at that rate. The chain consists of bounded descriptors, at most 64 KiB each, within the 8 MiB pool. Once the FPGA and DMA finish, AcqTask gives TxTask a job handle. The capture stays busy until the job finishes or is abandoned safely. The U06 throughput result used multiple 4 MiB captures, not a single 64 MiB hardware capture.")

    add("Live mode decodes data as it arrives",
        columns("The live decoding path",svg(
            box(25,35,210,95,"FPGA + DMA",["Run-encoded packets"],NAVY) + arrow(235,82,300,82) + box(300,35,220,95,"Live DDR pool",["16 × 256 KiB"]) + arrow(520,82,580,82) + box(580,35,175,95,"AcqTask",["Publish handles"]) +
            arrow(665,130,665,190) + box(485,190,270,95,"Decoder tasks",["UART / SPI / I²C"]) + arrow(485,235,415,235,"ev_q") + box(205,190,210,95,"TxTask",["Pack event blocks"]) + arrow(205,235,140,235) + box(25,190,115,95,"USB",["To host"]) +
            arrow(565,285,565,360) + text(565,388,"Release the buffer after decoding",20,weight=700,anchor="middle") + text(30,448,"The 2 ms timer also closes packets when signals stay unchanged.",22) + text(30,483,"Navy = hardware; gray = memory/tasks; event queues carry handles",16),"Live decode task pipeline"),
        "What the host receives",[
            "Each active decoder reads the captured samples through its own queue.",
            "The decoders produce bytes, frame markers, acknowledgments and error messages.",
            "Events include a sample timestamp and decoder slot. At low sample rates, packet closure waits for a valid sample.",
            "The live browser shows decoded messages. It does not continuously display the full raw waveform."
        ]),2,"docs/rtos-design.md, P7 live path; docs/live-web.md; fw-zynq/app/src/tasks/la_decoder_task.c",
        "Contrast this with burst: a segment is decoded as soon as its packet completes, rather than waiting for a finite capture to drain. Three decoder slots have independent configurations and can be started or stopped through the host. Transitions are converted into a monotonic sample timeline before the protocol state machines consume them. Events travel in static event blocks through ev_q to TxTask, then through UsbTask to the host. One segment may have multiple decoder readers, so its reference count governs reuse. A decoded-pattern match can ask AcqTask to stop the live run after decoding; it is not a sample-edge hardware trigger.")

    add("A buffer is reused only after every decoder has finished with it",
        columns("Who can use a buffer",svg(
            box(30,80,200,115,"FREE",["No current readers","Can be posted"],WHITE) + arrow(230,137,295,137) + box(295,80,220,115,"DMA_OWNED",["DMA may write","CPU cannot reuse"],NAVY) + arrow(515,137,570,137) + box(570,80,190,115,"CPU_OWNED",["Actual length","Reader references"]) +
            '<path d="M665,195 V310 H130 V195" fill="none" stroke="#173A56" stroke-width="2.3" marker-end="url(#arr)"/>' + text(395,292,"Last reader releases → AcqTask may repost",20,weight=700,anchor="middle") +
            box(30,365,730,85,"Handle = slot + generation; segment = sequence + actual length",["Stale handles are rejected; sequence gaps reset decoder state"]) + text(30,482,"White = available; navy = hardware-owned; gray = CPU-owned",16),"Segment ownership lifecycle"),
        "What happens when data is lost",[
            "AcqTask tracks which decoders are using each buffer before sending them its handle.",
            "If one decoder's queue is full, that decoder skips the buffer. The device counts the loss and releases its reference.",
            "If a decoder detects missing samples, it resets its state and marks the gap in the event stream.",
            "If no buffer is free, capture stops. An abort stops DMA and waits for the decoders to release their buffers."
        ]),2,"docs/rtos-design.md, Ownership/P7; fw-zynq/app/src/live/la_segment_pool.c, la_live_ring.c",
        "Walk around the state cycle. FREE becomes DMA_OWNED when AcqTask posts the descriptor. Completion changes it to CPU_OWNED and records the valid byte length, sequence and generation. Each successful consumer has a reference. A failed queue publish must release its reserved reference immediately. Only the last reader permits reuse, and AcqTask alone reposts the descriptor. This is zero-copy for acquisition payload handoff, not a claim that the whole system never copies: event packing, protocol framing and USB staging use bounded copies. Unknown missing samples are not decoded as contiguous input.")

    add("FreeRTOS gives acquisition priority and keeps commands responsive",
        table(["Task / priority","Main job","How it avoids blocking other work"],[
            ["AcqTask / 6","Control DMA, capture registers and buffer handoffs","Wake on FPGA/DMA notifications; do not wait on decoders"],
            ["Decoder tasks / 5","Decode UART, SPI and I²C samples","Pass buffer handles; count dropped events if queues fill"],
            ["CmdTask / 5","Check commands and send them to the right task","Route capture commands and transfer acknowledgments"],
            ["TxTask / 5","Send capture data, events and command responses","Limit event work per turn so commands get time to run"],
            ["UsbTask / 5","Handle USB requests, receive data and submit transfers","Use fixed-size queues and send frames in order"],
            ["StatusTask / 1","Check the watchdog, counters and remaining stack space","Report every 500 ms"]],[25,34,41]).replace('class="eq"','class="eq task-table"',1),
        2,"fw-zynq/app/src/la_app.h; docs/rtos-design.md, P8.1; tasks/la_usb_task.c",
        "These priorities come from the implemented constants rather than the older initial plan. Higher numbers have higher priority. CPU0 runs a static FreeRTOS task set with a 1 kHz tick; this does not mean the FPGA sample rate is 1 kHz. TxTask was raised from priority 4 to 5 after an overload case starved it below continuously ready decoders. USB, transmit, commands and decoders now share priority 5 while acquisition remains priority 6. TxTask's stack is 12 KiB after the larger 4096-byte frame payload amendment. ISRs acknowledge hardware and notify tasks; the USB vendor callback also performs the necessary bounded receive pop/staging/release operations. It does not run decoders or the command parser.")

    add("The device and PC use the same UART, SPI and I²C decoders",
        table(["Protocol","Signals and settings used","What the decoder reports"],[
            ["UART","RX signal, baud rate, data bits, parity and stop bits","Received bytes, parity errors and framing errors"],
            ["SPI","Clock, data and chip-select signals; clock polarity and phase","Data bytes or words, and chip-select changes"],
            ["I²C","SCL and SDA signals, START/STOP conditions and clock stretching","Address, read/write direction, bytes, ACK and NACK"]],[24,39,37]) +
        '<div class="reuse"><b>common/la_decode</b><span>Device: decode during a live session</span><span>PC: decode a saved capture</span></div>',
        2,"docs/decoders.md; common/la_decode/; host/la_ui/decode.py",
        "Explain the division of labor: the FPGA produces sample runs; the decoder wrapper reconstructs time and feeds protocol state machines in common/la_decode. UART schedules mid-bit observations and advances through long unchanged runs so a completed byte is not lost during idle. SPI uses configurable CPOL/CPHA and channel assignments; CPOL is clock polarity and CPHA is clock phase. I²C follows edges and stretching rather than assuming a rigid clock period. The host loads the same C library through a native application binary interface (ABI) for post-capture decoding. Native tests compare this behavior with independent Python stimulus models. The hardware-qualified live cases are 115200 baud UART and 10 kHz SPI/I²C, not the full 100 MS/s input bandwidth.")

    add("USB carries commands, capture data and live events",
        columns("USB connection and message format",svg(
            box(25,45,220,95,"PS USB0 + PHY",["J10 / high-speed"],NAVY) + arrow(245,92,300,92) + box(300,45,220,95,"Windows USB",["usbser.sys / COM"]) + arrow(520,92,580,92) + box(580,45,175,95,"Host client",["AnalyzerClient"],WHITE) +
            text(25,205,"ONE APPLICATION FRAME",22,weight=700) +
            ''.join(box(x,235,w,65,t,[],WHITE) for x,w,t in [(25,90,"SOF"),(115,100,"TYPE"),(215,90,"SEQ"),(305,95,"LEN"),(400,240,"PAYLOAD"),(640,115,"CRC16")]) +
            box(25,355,730,95,"Burst integrity",["DATA_CHUNK: offset + up to 4092 data bytes","DATA_END: whole-capture CRC32"]) + text(25,485,"Navy = device hardware; gray = driver/framing; white = host/fields",15),"Direct USB application protocol"),
        "How transfers are checked",[
            "The device uses USB CDC-ACM, which appears as a COM port with the standard Windows driver.",
            "The receiver assembles incoming bytes into frames, checks their length and verifies their checksums.",
            "During capture transfer, the PC sends acknowledgments. The throughput test acknowledged every four chunks.",
            "If live traffic is too heavy, the device counts lost events and reports the overload. Commands still get time to run."
        ]),2,"docs/protocol.md; docs/usb-link.md; build/reports/usb-u06-performance.md",
        "CDC-ACM expands to Communications Device Class, Abstract Control Model. This is high-speed USB enumeration, but measured application throughput is 1.62 MB/s, not the USB nominal signaling speed. The application maximum payload is 4096 bytes; a chunk reserves four bytes for offset, leaving 4092 data bytes. CRC means cyclic redundancy check: CRC16 protects each application frame and CRC32 verifies the concatenated capture data. SOF is start of frame, SEQ is sequence and LEN is payload length. The host can receive arbitrary split/coalesced USB reads because the parser is incremental. UsbTask submits static TX frames in enqueue order, with generation-aware session handling. Receive staging and event queues are bounded. USB reset and frame parsing are different state machines.")

    add("The browser lets you inspect waveforms and decoded messages",
        columns("Actual UI / synthetic UART, offline",'<div class="screenshot"><img src="ui-offline-waveform.png" alt="Current waveform UI rendering declared synthetic UART data offline"></div>',
        "What runs where",[
            "You can connect, capture, decode, view live messages, export files and inspect task traces in one browser window.",
            "The Python server manages the USB port, runs one capture job at a time and holds captures in memory.",
            "The viewer uses a summary for large captures. When you zoom in, it fetches the exact samples.",
            "Server-Sent Events (SSE) sends live messages to the browser. You can also open saved captures offline."
        ]),3,"host/la_ui/; screenshot: offline synthetic 115200-baud UART at 10 MS/s, 2026-10-12; not a new bench capture",
        "The screenshot is the current application rendering explicitly synthetic UART data offline: LA-TEST followed by newline, CH0, 115200 baud, 8N1 and 10 MS/s. It produced eight decoded byte events with no page errors. It is a UI illustration, not a new hardware acquisition or performance result. The PC runs the server and browser workspace, local at 127.0.0.1:8080 by default. HTTP is Hypertext Transfer Protocol. Capture APIs run one background job at a time; mode switches stop live sessions before starting capture. Server-Sent Events (SSE) carries decoded live messages to the browser. Min/max envelopes summarize each channel bucket; the exact detail window is fetched as the user zooms. An envelope is not permission to estimate exact transition timing from a coarse bucket. Exports include sigrok .sr, Value Change Dump (VCD), comma-separated values (CSV) and PNG.")

    add("Tests cover the FPGA, firmware, host tools and SD boot",
        table(["Test area","Recorded result","What was tested"],[
            ["FPGA simulation","12/12 Questa tests passed; 2026-09-20","Sample order, triggers, encoding, packet endings, stalls and overflow"],
            ["C code and host tools","2026-09-20: native 18/18 + release 22/22; 2026-10-18: host 180","Protocol messages, decoders, buffer handling and Windows host tools"],
            ["Physical loopback","18/18 external JE→JC tests passed over USB; 2026-09-19","1/10/100 MS/s with counter, walking, PRBS, UART, SPI and I²C patterns"],
            ["Long-running tests","500 captures passed; 60 s UART live test passed","Repeated capture checks and live decoding at the tested data rate"],
            ["SD-card boot","Cold SD boot + 6/6 UI tests passed; 2026-09-26","Boot without JTAG, then run capture, decoding, live and export workflows"]],[25,34,41]),
        3,"Recorded snapshots, not one final-revision run: docs/status.md; usb-u05-regression.md; sd-boot-verification.md",
        "This table combines dated evidence snapshots, not one new all-tests run. The 180 host-test count is the final 18 October suite; native 18 and release 22 are recorded firmware-era gates. The external loopback matrix is different from the internal self-test mux: it passes through real header wiring. The September 26 SD boot used the packaged USB image, then passed all six UI end-to-end cases after the live loopback jumper was installed. E2E means end-to-end. Automated API/SSE and headless browser checks are evidence; the human visual walkthrough was separately author-confirmed on 12 October and accepted on 18 October. No hardware was programmed or retested to build this presentation.")

    add("USB transferred 64 MiB at 1.62 MB/s in each test",
        columns("USB transfer tests / MB/s",svg(
            ''.join('<path d="M'+str(180+i*250)+' 90 V360" stroke="#D0D8DE"/>' + text(180+i*250,390,str(i),18,anchor="middle") for i in range(3)) +
            ''.join(text(30,140+i*90,f"Trial {i+1}",22,weight=700) + f'<rect x="180" y="{112+i*90}" width="{1.62*250}" height="42" fill="#173A56"/>' + text(602,141+i*90,"1.62",24,weight=700) for i in range(3)) +
            '<path d="M555 90 V360" stroke="#607685" stroke-width="2" stroke-dasharray="6 5"/>' + text(555,65,"Target: 1.5 MB/s",20,anchor="middle") +
            text(30,445,"Each test used sixteen separate 4 MiB raw captures.",22) + text(30,480,"Navy = measured speed; dashed line = target",16),"USB measured throughput chart"),
        "What the numbers include",[
            "Each trial transferred <b>67,108,864 bytes</b> in <b>41.4–41.5 s</b>.",
            "Every capture matched the expected counter values and passed the frame and whole-capture checksum checks.",
            "The time includes commands, capture, USB transfer and verification on the Windows PC.",
            "The test used 4092-byte data chunks, 64 KiB host reads and an acknowledgment every four chunks."
        ]),3,"build/reports/usb-u06-performance.md, 2026-09-19; test_usb_drain.py; MB/s is decimal",
        "Be precise about the result. Three consecutive trials each exceeded the required 1.5 MB/s. The total is 64 MiB per trial, but it is sixteen 4 MiB captures, not a 64 MiB device memory buffer. MB/s is decimal and MiB is binary. Wall-clock timing includes the full acquire-and-verify workflow, so it is not a USB raw bus benchmark. The larger application chunk and host pacing removed excessive per-chunk service overhead; TxTask's stack also grew to hold the larger frame. This result supports buffered bursts; it does not mean raw 100 MS/s input can be streamed indefinitely over USB.","m05")

    add("The 60-second UART test showed no reported event loss",
        columns("60 s UART run / received event counts",svg(
            text(35,65,"DEVICE DECODED",22,weight=700) + text(35,120,"606,411",56,weight=700) +
            arrow(300,105,420,105,"USB + SSE") + text(470,65,"HOST FEED",22,weight=700) + text(470,120,"606,411",56,weight=700) +
            table_svg_live() + text(35,480,"115200-baud UART pattern-generator loopback / 2026-09-20",17),"Qualified live event delivery"),
        "Test results and limits",[
            "The 60 s run reported <b>zero client drops, zero overruns and zero dropped segments</b>.",
            "The total includes <b>606,405 byte events and 6 other events</b>. Separate SPI and I²C tests passed at 10 kHz.",
            "The 1 MHz SPI test deliberately overloaded the system. It reported lost data and returned to IDLE.",
            "Old messages are removed when replay history fills up. That is different from losing messages to the connected viewer."
        ]),3,"build/reports/usb-v04-live-web.md / evidence.txt, 2026-09-20; docs/status.md, V04",
        "The device and feed counts agree. There are 606,411 total events and 606,405 byte events because protocol/control event types also exist. The test reported 124.7 kB/s of wire traffic. SPI produced 4,380 events with no frame errors and I²C 5,347 events across 668 complete frames with no NACKs. These workloads are the measured scope. Do not infer that 100 MHz acquisition means the ARM can decode arbitrary protocols at that rate. The overloaded 1 MHz SPI test returned to IDLE in 0.05 s in V04, but it is intentionally allowed to drop decoder events and report them. The report's negative isolated-byte latency is an instrumentation artifact, so no new direct-USB sub-millisecond latency claim is made here.","m05")

    add("The FPGA design fits the Zybo and meets timing at 100 MHz",
        columns("FPGA resources used / total available",svg(
            text(25,75,"Lookup tables",22,weight=700) + '<rect x="25" y="105" width="650" height="54" fill="#E8EDF1"/><rect x="25" y="105" width="246.55" height="54" fill="#173A56"/>' + text(700,143,"37.93%",21,weight=700,anchor="end") +
            text(25,240,"Block RAM tiles",22,weight=700) + '<rect x="25" y="270" width="650" height="54" fill="#E8EDF1"/><rect x="25" y="270" width="400.85" height="54" fill="#173A56"/>' + text(700,308,"61.67%",21,weight=700,anchor="end") +
            text(25,190,"6,675 of 17,600 LUTs",20) + text(25,355,"37 of 60 BRAM tiles",20) + text(25,435,"0%",18) + text(675,435,"100%",18,anchor="end") + text(25,480,"Navy = used; gray = remaining capacity",17),"Routed FPGA resource capacity"),
        "Timing results from the routed design",[
            "Vivado built the design for <b>xc7z010clg400-1</b> with a <b>10 ns clock period</b>.",
            "Worst setup slack was <b>+0.576 ns</b>; worst hold slack was <b>+0.022 ns</b>.",
            "There were <b>no failing setup or hold paths</b>, and the design-rule checks reported no errors.",
            "These results support the 100 MHz design. The 200 MS/s version has not been tested."
        ]),3,"build/release/timing_summary.rpt + utilization.rpt, 2026-09-20; docs/status.md, V10 bit PASS",
        "Use the current local routed reports, which are newer than the older P9 resource numbers in architecture.md. LUT is lookup table and BRAM is block RAM. Both utilization bars use the full 0–100 percent available-capacity scale. The block RAM usage is relatively higher because the history ring and output FIFO map into BRAM. WNS is worst negative slack even when its reported number is positive; WHS is worst hold slack. The current report states all user-specified constraints are met. Asynchronous inputs have documented synchronizer/false-path treatment; a timing report is not analog input bandwidth or signal-integrity qualification. No synthesis or routing was rerun for this presentation.","m05")

    add("The device has tested limits; product release is deferred",
        table(["Area","Current status","Limit or next step"],[
            ["Input voltage","Eight 3.3 V CMOS inputs; no built-in input protection","Use a common ground and short leads. Add a level shifter for 5 V signals."],
            ["Capture size and rate","100 MS/s; 8 MiB capture buffer; up to 32,768 pre-trigger samples","Reject captures that exceed the buffer. Test the 200 MS/s design before claiming it."],
            ["Live decoding","Tested at 115200-baud UART and 10 kHz SPI/I²C","Higher traffic can lose data; the device counts and reports those losses."],
            ["Visual UI check","Automated browser tests and the visual walkthrough are complete","Use exported captures for records; the live view is a bounded preview."],
            ["Public product release","SD boot passed; USB identity is still for in-house use","Choose a public USB identity and authorize a tagged product release."]],[25,37,38]),
        4,"docs/limits.md; docs/status.md, 2026-10-18; build/reports/sd-w11/",
        "This is an operational boundary and completion checklist grounded in the project records, not a generic risk slide. The class project is complete. The W11 SD boot passed 18 October 2026, and the visual walkthrough was author-confirmed on 12 October and accepted on 18 October. The v1.0 tag remains unauthorized and the USB development identity 1209:0001 is for in-house use; those are deferred product-release items, not open implementation work. The 200 MS/s path is a stretch item rather than an achieved specification. Distinguish device burst capacity, live decoder throughput and browser presentation limits.")

    add("The FPGA handles timing; firmware and the PC make the data useful",
        table(["Goal","How the design meets it","What the tests showed"],[
            ["Capture signal timing","The FPGA samples signals, detects triggers and stores their history.","100 MS/s operation, wired loopback tests and timing closure"],
            ["Keep data safe","FreeRTOS tracks buffers and controls when DMA can reuse them.","Repeated captures passed; missing data and overloads are reported"],
            ["Read protocol messages","Shared C decoders turn samples into UART, SPI and I²C events.","Live decoding passed at the tested rates; decoder code is also tested on the PC"],
            ["Inspect and save results","Direct USB connects the device to a browser workspace on the PC.","1.62 MB/s transfer tests and working UI flows after SD boot"]],[25,39,36]),
        4,"README.md; docs/status.md; build/reports/usb-u06-performance.md; sd-boot-verification.md",
        "Close the technical argument: a logic analyzer has to both observe fast physical timing and provide understandable results. The FPGA handles the first requirement. FreeRTOS firmware makes memory ownership, decoding and transport explicit. The PC supplies the workspace and offline tools. The result is a functioning direct-USB instrument within the stated MVP limits. The main engineering lesson is that data integrity depends on correct boundaries and ownership as much as on the maximum clock rate. For a short delivery, keep the slides through this conclusion and use the detailed architecture pages to answer questions.")

    add("RTOS Logic Analyzer",'<div class="back"><div class="back-title">RTOS Logic Analyzer</div><p>Zybo Z7-10 · Direct Zynq USB · Windows host</p><p>Final project design presentation</p></div>',None,"", "Invite questions about the FPGA/firmware boundary, descriptor lifetime, why acquisition and USB have different rates, and which results are bench-qualified.","b10")


def table_svg_live():
    s = ''
    for i, (a,b) in enumerate([("Client drops","0"),("Overruns / lost segments","0 / 0"),("USB framed traffic","124.7 kB/s")]):
        y = 240 + i * 67
        s += text(35,y,a,23) + text(725,y,b,28,weight=700,anchor="end")
        if i < 2:
            s += f'<path d="M35 {y+17} H730" stroke="#CED6DC"/>'
    return s + text(35,435,"Traffic includes timestamps, events and application framing.",18)


CSS = """
:root{--page-bg:#E8EDF1;--slide-bg:#FFFFFF;--ink:#233B4C;--navy:#173A56;--accent:#173A56;--muted:#607685;--rule:#CAD4DC;--hairline:#DFE5E9;--soft:#E8EDF1;--font-sans:Arial,sans-serif;--font-serif:Arial,sans-serif}
@page{size:338.67mm 190.5mm;margin:0}
.s{width:338.67mm;height:190.5mm;padding:24px 58px 0;margin:20px auto;display:flex;flex-direction:column;position:relative;overflow:hidden;background:white;box-shadow:none}
.s .bar{height:29px;border-bottom:1.5px solid #173A56;margin:0;padding:0 0 10px;display:flex;justify-content:space-between;align-items:center}
.s .logo{font-size:13px;letter-spacing:.09em;font-family:Arial,sans-serif;font-weight:700}
.s .topics{display:flex;gap:5px;align-items:center}
.s .topic{font-size:11px;padding:5px 7px;color:#607685;background:#F2F5F7;border:0}
.s .topic.active{color:#FFFFFF;background:#173A56}
.s h1{font:700 30px/1.22 Arial,sans-serif;margin:19px 0 0;max-width:1160px;min-height:73px;display:flex;align-items:flex-start}
.s .c{margin:6px 0 0;display:flex;flex-direction:column;gap:18px;flex:0 0 auto;height:493px;min-height:0;justify-content:center}
.s .two{display:grid;grid-template-columns:1.8fr 1fr;gap:33px;flex:1;min-height:0;align-items:stretch}
.s .two>div{display:flex;flex-direction:column;min-height:0}
.s .two .colh{font:700 21px/1.2 Arial,sans-serif;height:33px;min-height:33px;padding:0 0 9px;margin:0 0 17px;border-bottom:2px solid #173A56;box-sizing:border-box;display:block;color:#173A56}
.s .two svg{width:100%;height:calc(100% - 50px);display:block;flex:1;overflow:visible}
.s .screenshot{display:flex;flex-direction:column;justify-content:center;flex:1;gap:24px}
.s .screenshot img{width:100%;height:auto;max-height:430px;object-fit:contain;display:block}
.s .screenshot p{font:400 19px/1.4 Arial,sans-serif;margin:0}
.s .decode-result{display:flex;flex-direction:column;gap:22px;width:100%}
.s .decode-heading{font:700 21px/1.2 Arial,sans-serif;color:#173A56;margin:0}
.s .decode-frame{display:block;width:100%;height:auto}
.s .byte-map{display:grid;grid-template-columns:repeat(6,1fr);gap:12px}
.s .byte-card{background:#E8EDF1;padding:12px 15px}
.s .byte-hex{font:700 30px/1.15 Arial,sans-serif;color:#173A56;margin:0 0 6px}
.s .byte-char{font:400 23px/1.2 Arial,sans-serif;margin:0}
.s .decode-facts{display:grid;grid-template-columns:repeat(3,1fr);gap:30px;border-top:2px solid #173A56;padding-top:17px}
.s .decode-facts h3{font:700 21px/1.2 Arial,sans-serif;color:#173A56;margin:0 0 9px}
.s .decode-facts p{font:400 21px/1.4 Arial,sans-serif;margin:0}
.s .demo-link{font:400 20px/1.3 Arial,sans-serif;margin:0}
.s .demo-link a{color:#173A56;text-decoration:underline}
.s .two ul{font:400 20px/1.35 Arial,sans-serif;margin:0;padding-left:21px;display:flex;flex-direction:column;justify-content:center;flex:1;gap:22px}
.s .two li{margin:0;padding-left:3px}
.s table{font:400 20px/1.3 Arial,sans-serif;width:100%;table-layout:fixed;border-collapse:collapse;flex:0 0 auto;height:auto}
.s th{font:700 23px/1.2 Arial,sans-serif;padding:0 15px 15px 0;border-bottom:2px solid #173A56;background:none;vertical-align:bottom;text-align:left;color:#173A56}
.s td{font:400 20px/1.3 Arial,sans-serif;padding:16px 15px 16px 0;border-bottom:1px solid #CAD4DC;vertical-align:middle;color:#233B4C}
.s td.ax{font:700 25px/1.18 Arial,sans-serif;border-right:0;color:#173A56;padding-right:23px}
.s tr:last-child td{border-bottom:0}
.s .task-table td{padding-top:11px;padding-bottom:11px}
.s .route-link{font:700 20px/1.3 Arial,sans-serif;color:#173A56;margin:0}
.s .reuse{display:flex;align-items:center;justify-content:space-between;font-size:18px;padding:18px 0 0;border-top:2px solid #173A56;gap:20px}
.s .foot{position:absolute;left:58px;right:58px;bottom:19px;border-top:1px solid #CAD4DC;padding-top:8px;font:400 10px Arial,sans-serif;display:flex;justify-content:flex-end}
.s.cover{background:#F4F7FA;display:block;padding:0}
.s .cover-copy{position:absolute;left:64px;top:74px;width:520px}
.s .eyebrow{font-size:14px;letter-spacing:.12em;font-weight:700;margin:0 0 22px}
.s .cover-title{font:700 65px/1.05 Arial,sans-serif;letter-spacing:-2px}
.s .cover-desc{font:400 24px/1.55 Arial,sans-serif;margin:30px 0;color:#233B4C}
.s .cover-author{font:700 22px/1.3 Arial,sans-serif;margin:18px 0 0;color:#173A56}
.s .cover-author.long{font:700 17px/1.35 Arial,sans-serif}
.s .cover-meta{font:400 16px/1.5 Arial,sans-serif;margin-top:35px}
.s .cover-art{position:absolute;left:606px;top:130px;width:630px;height:430px}
.s .cover-art svg{width:100%;height:100%}
.s .back{position:absolute;left:65px;top:265px}
.s .back-title{font:700 54px/1.1 Arial,sans-serif}
.s .back p{font:400 22px/1.4 Arial,sans-serif;margin:20px 0}
@media print{html,body{background:white}.s{margin:0!important;break-after:page;page-break-after:always}.s:last-child{break-after:auto;page-break-after:auto}}
"""


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--skill-root",type=Path,default=Path.home()/".config/opencode/skills/consulting-pptx-skill")
    args = p.parse_args()
    assert (ROOT / "build").is_dir(), "Project build directory must exist"
    OUT.mkdir(exist_ok=True)
    for filename in ("uart-demo-capture.png", "uart-demo-frame.png", "logic-analyzer-hardware.jpeg"):
        assets = ROOT / "docs" / "hardwarex" / "assets"
        source = (assets / filename if filename.endswith(".jpeg")
                  else assets / "uart-demo" / filename)
        if not source.is_file():
            raise FileNotFoundError(f"Real UART screenshot missing: {source}")
        shutil.copyfile(source, OUT / filename)
    base_content()
    from class_sections import build
    build(sys.modules[__name__],list(SLIDES))
    foundation = OUT / "foundation.html"
    subprocess.run([sys.executable,"-X","utf8",str(args.skill_root/"scripts/new_deck.py"),"--parts",','.join(s['part'] for s in SLIDES),"--title","RTOS Logic Analyzer","-o",str(foundation)],check=True)
    doc = foundation.read_text(encoding="utf-8")
    shells = re.findall(r'<section class="(?:s|slide)[^"]*">.*?</section>',doc,re.S)
    page = 0
    authored = []
    for s, shell in zip(SLIDES,shells):
        cover = s['part'] in ('b01','b10')
        if not cover:
            page += 1
        topics = ''.join(f'<span class="topic{" active" if s["category"]==i else ""}">{t}</span>' for i,t in enumerate(["Shared overview","FPGA system","Embedded system","Shared results"],1)) if s['category'] else ''
        header = '' if cover else f'<div class="bar"><div class="logo">RTOS LOGIC ANALYZER</div><div class="topics">{topics}</div></div><h1>{s["title"]}</h1>'
        inside = s['body'] if cover else f'<div class="c">{s["body"]}</div><div class="foot"><span>{page}</span></div>'
        # The generator-selected shell identifies the applicable archetype.
        authored.append(f'<section class="s{" cover" if cover else ""}" data-part="{s["part"]}" data-slide="{len(authored)+1}" aria-label="{html.escape(s["title"])}">{header}{inside}</section>')
    doc = re.sub(r'<main class="deck skin-warm">.*?</main>','<main class="deck">\n'+'\n'.join(authored)+'\n</main>',doc,flags=re.S)
    doc = doc.replace('</style>',CSS+'\n</style>').replace('lang="ja"','lang="en"')
    (OUT/"logic-analyzer-design.html").write_text(doc,encoding="utf-8")
    (OUT/"slides.json").write_text(json.dumps(SLIDES,indent=2,ensure_ascii=False),encoding="utf-8")
    (OUT/"routes.json").write_text(json.dumps(ROUTES,indent=2),encoding="utf-8")
    notes = ['# Presenter notes and evidence map\n','One deck, two independent subsystem routes.\n','FPGA system: slides 1–18, then 34–38.\n','Embedded system: slides 1–6, then 19–38.\n','PowerPoint: Slide Show > Custom Slide Show > choose the subsystem.\n','Each route uses the shared overview and close. Budget about 20–25 minutes per route, plus Q&A.\n','Evidence: dated build/bench reports and the real 18 October 2026 UART capture; no new hardware test performed for this slide revision.\n']
    for i,s in enumerate(SLIDES,1):
        notes += [f'## Slide {i}: {s["title"]}\n',s['notes']+'\n',f'Sources: {s["source"] or "Presentation back cover"}\n']
    (OUT/"presenter-notes.md").write_text('\n'.join(notes),encoding="utf-8")
    (OUT/"storyline.md").write_text('# Storyline and visual treatment\n\n'+'\n'.join(f'{i}. {s["title"]} — section {s["category"]}, {s["part"]}' for i,s in enumerate(SLIDES,1))+'\n',encoding="utf-8")
    print(f'Authored {len(SLIDES)} slides; output: {OUT}')


if __name__ == "__main__":
    main()
