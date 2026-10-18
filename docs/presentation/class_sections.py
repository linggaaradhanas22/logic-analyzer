"""Two independent classroom routes assembled on the existing slide layouts."""
from copy import deepcopy


def build(d, original):
    d.SLIDES.clear()
    box,arrow,text,svg=d.box,d.arrow,d.text,d.svg
    table,columns=d.table,d.columns
    navy,gray,white=d.NAVY,d.GRAY,d.WHITE

    def reuse(index,section,notes=None,body=None):
        slide=deepcopy(original[index])
        slide['category']=section
        if notes: slide['notes']=notes+' '+slide['notes']
        if body: slide['body']=body
        d.SLIDES.append(slide)

    def add(title,body,section,source,notes):
        d.add(title,body,section,source,notes)

    # Shared introduction: no class depends on the other's detailed section.
    reuse(0,None)
    d.SLIDES[-1]['body']=d.SLIDES[-1]['body'].replace('FINAL PROJECT DESIGN','FPGA SYSTEM + EMBEDDED SYSTEM')
    add('Choose the subsystem after the shared overview',
        table(['Section','What it covers','PowerPoint slides'],[
            ['Shared overview','The problem, device, signal path and hardware–software boundary','1–6'],
            ['FPGA system','Capture RTL, AXI interfaces, hardware integration and FPGA tests','7–18'],
            ['Embedded system','FreeRTOS, DMA driver, buffers, decoders, USB and firmware tests','19–33'],
            ['Shared results','The user interface, decoded UART frame, limits and conclusion','34–38'],
        ],[25,56,19]),1,'Classroom presentation routes',
        'For the FPGA system route, present slides 1–18, then 34–38. For the Embedded system route, present slides 1–6, then 19–38. The numbers here are actual PowerPoint slide numbers, not printed body-page numbers. Named custom slide shows skip the other detailed section automatically. Click the FPGA system or Embedded system row to jump to its opening boundary diagram.')
    reuse(2,1)
    reuse(3,1)
    d.SLIDES[-1]['body']=columns('Logic analyzer hardware on the bench',
        '<div class="screenshot"><img src="logic-analyzer-hardware.jpeg" '
        'alt="Powered Zybo Z7-10 logic analyzer with USB cables and acquisition leads"></div>',
        'Board connections',[
            '<b>Zybo Z7-10:</b> the FPGA captures digital inputs and the ARM processor runs FreeRTOS.',
            '<b>J10 USB:</b> analyzer data connection to the PC.',
            '<b>J12 USB:</b> board power, programming, and console connection.',
            '<b>JC inputs:</b> eight 3.3 V channels with a common ground to the target.',
        ])
    d.SLIDES[-1]['source'] += '; user-provided hardware photograph, 18 October 2026'
    d.SLIDES[-1]['notes'] = (
        'The photograph shows the assembled Zybo-based analyzer with USB cables '
        'and acquisition leads. J10 carries analyzer data; J12 provides power, '
        'programming, and console access. The connected leads carry the target '
        'signals into JC. Keep a common ground and use 3.3 V signals. '
        'The target circuit is outside the photograph.')
    reuse(4,1)
    add('DMA hardware and its firmware driver have different jobs',
        table(['Function','FPGA / hardware','ARM / firmware'],[
            ['Capture signals','Register-transfer level (RTL) logic samples inputs and detects triggers','Firmware chooses settings and starts or stops a capture'],
            ['Represent samples','RTL packs raw samples or repeated-value runs','C code reads the format and decodes protocols'],
            ['Move samples','AXI DMA IP (intellectual property) writes sample packets to DDR','The DMA driver assigns descriptors and checks completed transfers'],
            ['Use buffers','Hardware writes only to the buffers assigned to it','Firmware tracks readers and decides when each buffer can be reused'],
            ['Talk to the PC','The PS has the USB controller and external PHY','FreeRTOS tasks implement USB messages, commands and live events'],
        ],[23,38.5,38.5]),1,'docs/architecture.md; fw-zynq/app/src/drivers/la_dma.c; fpga/rtl/la_top.sv',
        'This is the separator both audiences need. PL means programmable logic; PS means processing system. The AXI DMA IP is hardware in the Vivado design, while its driver and descriptors are software responsibilities. DDR is served by the PS memory controller. USB is a PS peripheral, not an FPGA acquisition module. A hardware block can be a black box in a software talk even though the software configures it. UART/SPI/I²C protocol decoders are C firmware, not RTL protocol decoders.')

    # FPGA section: detailed hardware; firmware shown by its interface only.
    add('The FPGA system takes settings from a firmware black box',
        columns('FPGA system / hardware in focus',svg(
            box(25,65,165,105,'Inputs / JC',['8 digital channels'],white)+arrow(190,115,240,115)+
            box(240,55,270,180,'FPGA RTL + DMA IP',['Sample, trigger, pack','Stream → memory'],navy)+
            arrow(510,100,570,100,'HP0')+box(570,55,195,180,'PS + firmware',['BLACK BOX','Store / display'])+
            arrow(570,210,510,210,'Settings',True)+arrow(510,155,570,155,'IRQ',True)+
            box(25,355,735,90,'Hardware boundary',
                ['Settings enter; samples, status and interrupts leave.','IRQ = interrupt request; the gray block supplies control and storage.'],white)+
            text(25,482,'Navy = hardware focus; gray = black box; white = interface/context',15),
            'FPGA section boundary and black box'),
        'The interface is enough',[
            'Assume firmware writes valid settings, prepares capture memory and arms the FPGA.',
            'The hardware section covers RTL, clocking, AXI-Stream output and AXI DMA connections.',
            'Treat tasks, decoders, USB messages and the browser as services behind the black box.',
            'After the FPGA results, jump to slide 34 for shared results and the conclusion.',
        ]),2,'docs/architecture.md; docs/rtos-design.md',
        'Start the FPGA-specific talk here. The gray block includes PS memory/controller services, firmware and PC display; its internal task pipeline is not needed for this route. The hardware focus includes project RTL, AXI DMA IP and their interconnect. Explain the interface assumptions rather than task priorities. DMA accesses DDR through HP0. The IRQ and register arrows are logical interfaces, not a direct stream into CPU code.')
    pipeline=columns('Sample path and separate control path',svg(
        box(20,40,215,100,'Synchronizer',['Two flip-flops / input'],navy)+arrow(235,90,290,90)+
        box(290,40,210,100,'Sampler',['DIV + channel mask'],navy)+arrow(500,90,555,90)+
        box(555,40,205,100,'History ring',['Ordered samples'],navy)+arrow(655,140,655,210)+
        box(555,210,205,100,'Raw / run packer',['Sample words'],navy)+arrow(555,260,485,260)+
        box(290,210,195,100,'AXIS FIFO',['8 KiB → DMA'],navy)+
        arrow(395,140,125,210,'Sample / valid',True)+box(20,210,215,100,'Trigger logic',['Level + edge match'],navy)+
        arrow(125,310,125,365,'Match',True)+box(20,365,465,90,'Capture controller + registers',['ARM, sample count, flush, stop and IRQ'],white)+
        text(510,405,'Controller coordinates',18)+text(510,435,'ring and packet endings.',18)+
        text(20,482,'Navy = sample/trigger hardware; dashed arrows = control observations',15),
        'Correct FPGA sample path with trigger as a side observer'),
        'Hardware responsibilities',[
            'Samples go from the sampler into the history ring, then the encoder or raw packer and FIFO.',
            'Trigger logic observes the sampled input. It does not sit in the sample-data path.',
            'The controller starts and stops the ring and asks the packer to close a packet.',
            'Firmware supplies settings through registers; it does not sample each GPIO in software.',
        ])
    reuse(6,2,'Hardware view: follow the solid sample path, then describe the side-band trigger and controller.',pipeline)
    add('The sampler reads every enabled channel on the same clock tick',
        columns('From asynchronous pin to sample',svg(
            box(25,60,170,110,'JC input',['Asynchronous'],white)+arrow(195,115,250,115)+
            box(250,60,195,110,'Flip-flop 1',['May see an edge'],navy)+arrow(445,115,500,115)+
            box(500,60,250,110,'Flip-flop 2',['Synchronized value'],navy)+
            arrow(620,170,620,235)+box(300,235,450,110,'Sampler / channel mask',['sample = synchronized input & CHAN_EN'],navy)+
            text(25,390,'Sample rate = 100 MHz / (SAMPLE_DIV + 1)',25,weight=700)+
            text(25,435,'DIV 0 → 100 MS/s; DIV 9 → 10 MS/s; DIV 99 → 1 MS/s',20)+
            text(25,480,'Navy = RTL logic; white = input; all stages use the 100 MHz clock',15),
            'Synchronizer and divider implementation'),
        'Why the circuit is built this way',[
            'The two-flop synchronizer reduces metastability risk when inputs change near a clock edge.',
            'The divider generates a sample-valid pulse, rather than creating a new sampling clock.',
            'Disabled channels read as zero. All enabled bits belong to the same 8-bit sample.',
            'The 10 ns sample interval is not an analog measurement; short pulses may be missed.',
        ]),2,'fpga/rtl/la_input_sync.sv; la_sampler.sv; la_top.sv',
        'The FPGA uses FCLK_CLK0 at 100 MHz. The synchronizer is marked async_reg, and the pad-to-first-stage path has the documented timing exception. The optional majority filter exists in the module but is disabled in the implemented top. The divider produces an enable pulse in the same fabric clock domain; lower sampling rates are not additional generated clock domains. Pipeline latency exists before the sampled representation. A synchronizer reduces risk, not an absolute guarantee of pulse capture or electrical protection.')
    history=columns('Before and after the trigger',svg(
        '<rect x="50" y="100" width="300" height="230" fill="#E8EDF1"/><rect x="350" y="100" width="360" height="230" fill="#EEF4F8"/>'+
        text(75,145,'PRE-TRIGGER HISTORY',21,weight=700)+text(380,145,'POST-TRIGGER SAMPLES',21,weight=700)+
        '<path d="M50 260 H150 V200 H235 V260 H350 V200 H465 V260 H585 V200 H710" fill="none" stroke="#173A56" stroke-width="4"/><path d="M350 85 V345" stroke="#173A56" stroke-dasharray="6 5"/>'+
        text(350,70,'First matching sample',21,weight=700,anchor='middle')+
        text(50,385,'Trigger = level match AND rise condition AND fall condition',21,weight=700)+
        text(50,420,'A nonempty edge mask needs an edge on any selected channel.',19)+
        text(50,450,'An empty mask removes that edge requirement.',19)+
        text(50,480,'Gray = stored history; pale blue = samples from the trigger onward',15),
        'Hardware trigger and BRAM history'),
        'Trigger and history details',[
            'Edges compare the current sample with the previous valid sample, even when the divider is used.',
            'The BRAM ring keeps history while armed. Up to 32,768 pre-trigger samples can be requested.',
            'After a match, the ring preserves sample order while replaying history and accepting later samples.',
            'POST includes the trigger sample. POST = 0 keeps only the pre-trigger window.',
        ])
    reuse(7,2,'The trigger equations are level_ok && rise_ok && fall_ok; a nonempty rise or fall mask means any matching transition on that mask. The internal BRAM history ring has 65,536 entries; the public PRE limit is 32,768. Synchronous-read BRAM plus an output register preserves the pre/post seam. If downstream stalls exceed available retention, overflow is reported.',history)
    add('The capture controller decides when to wait, record and close a packet',
        columns('Implemented RTL state machine',svg(
            box(25,80,145,100,'IDLE',['Wait for ARM'],navy)+arrow(170,130,225,130,'ARM')+
            box(225,80,155,100,'ARMED',['Roll history'],navy)+arrow(380,130,435,130,'Trigger')+
            box(435,80,150,100,'CAPT',['Count samples'],navy)+arrow(585,130,640,130,'End')+
            box(640,80,125,100,'DRAIN',['Close tail'],navy)+
            '<path d="M700 180 V265 H95 V180" fill="none" stroke="#173A56" stroke-width="2" marker-end="url(#arr)"/>'+
            text(390,247,'Final word / bounded drain → DONE, then IDLE',19,anchor='middle')+
            text(25,340,'Live ARM skips ARMED and enters CAPT directly.',22,weight=700)+
            text(25,380,'Abort or trigger timeout → IDLE; overflow during CAPT → DRAIN.',19)+
            text(25,430,'Raw tails use TKEEP; packet endings use TLAST.',22)+
            text(25,475,'Navy = hardware states; DONE is an event/status, not a separate state',15),
            'RTL state machine with burst and stream transitions'),
        'What changes at each state',[
            'ARM latches the controller settings. A new ARM is ignored while the controller is busy.',
            'Burst output counts the retained history plus POST samples, including the trigger when POST is nonzero.',
            'The last sample closes a pending run or partial raw word. DRAIN waits for the closing transfer.',
            'Live packets close on the size limit or a 2 ms flush request; low sample rates wait for a valid sample.',
        ]),2,'fpga/rtl/la_capture_ctrl.sv; la_raw_packer.sv; la_txn_encoder.sv',
        'Use the actual four states IDLE, ARMED, CAPT and DRAIN, not the original planning diagram. The history count K is the smaller of the requested pre-count and history available at the trigger. The controller counts ring emissions and calculates ceil((K+POST)/4) for raw packet closure. Stream ARM enters CAPT immediately. Abort returns directly to IDLE without treating partial data as a completed normal capture. Overflow in CAPT stops production and requests a final flush before bounded draining. The software stop/reset sequence is inside the black box for this talk.')
    reuse(8,2,'FPGA class: explain the raw packer and run encoder, not the C protocol state machines.')
    add('AXI-Stream can wait, but the real input signals cannot',
        table(['Signal or buffer','RTL meaning','Design consequence'],[
            ['TVALID + TREADY','A word transfers only when both signals are high','Keep data and packet flags stable while the sink is stalled'],
            ['TKEEP','Each bit marks a valid byte in the 32-bit word','A final raw word can contain fewer than four samples'],
            ['TLAST','Marks the end of a DMA packet','Close word-aligned captures as well as partial tails'],
            ['8 KiB FIFO','Buffers short gaps in downstream service','About 82 µs of raw 100 MB/s data; sampling cannot pause indefinitely'],
            ['Overflow flag','Records that the ring or FIFO could not keep up','Stop/report the loss instead of inventing continuous sample history'],
        ],[23,38.5,38.5]),2,'fpga/rtl/la_axis_fifo.sv; la_raw_packer.sv; docs/plan.md §1.1',
        'AXIS means AXI4-Stream. A transfer is a handshake, not a TVALID pulse alone. TREADY backpressure can stall stored words, not the external target signals. The 8 KiB nominal data capacity divided by calculated raw 100 MB/s is 81.92 microseconds; worst-case run encoding can reach 400 MB/s and reduces runway to about 20 microseconds. The run encoder may compress or expand. These are buffer-budget calculations, not measured service deadlines. Protocol timing must never continue across an unreported acquisition gap.')
    add('AXI-Lite registers let firmware control the RTL without changing it',
        table(['Register group','Hardware behavior','Interface rule'],[
            ['Acquisition settings','SAMPLE_DIV, CHAN_EN, encoding and pre/post counts','Firmware writes a valid setup before ARM'],
            ['Trigger settings','Level mask/value and rising/falling masks','Trigger logic compares valid sampled values'],
            ['CTRL commands','ARM, ABORT, reset and mode selection','Command pulses self-clear; mode bits remain stored'],
            ['STATUS / IRQ','Busy, trigger, done, overflow and word counts','IRQ_STAT uses write-one-to-clear; ID is 0x4C410002'],
            ['AXI-Lite transfers','Accept address and data independently; use byte strobes','Finish the response even if address/data arrive on different cycles'],
        ],[23,39,38]),2,'docs/regmap.yaml; fpga/rtl/la_regs.sv; fpga/tb/tb_la_regs.sv',
        'The analyzer register base is 0x40000000. Do not confuse it with the historical proposed address. The procedural testbench checks independent AW/W arrival, response stalls, strobes, reads, invalid addresses, self-clearing commands and W1C IRQ bits. The capture controller latches its timing/encoding fields at ARM; not every combinational RTL input is a separate latched register. Acquisition reconfiguration is prevented by the firmware contract during a run. Generated C/SV/Python register definitions come from regmap.yaml; they are not hand-edited.')
    add('Vivado connects the capture RTL to DMA and the PS memory port',
        columns('Hardware integration / firmware hidden',svg(
            box(25,65,165,115,'la_top',['32-bit AXIS'],navy)+arrow(190,123,230,123)+
            box(230,65,180,115,'AXI DMA IP',['S2MM + SG','64-bit writes'],navy)+arrow(410,123,460,123)+
            box(460,65,140,115,'sc_dma',['Memory AXI'],navy)+arrow(600,123,650,123)+
            box(650,65,110,115,'PS',['HP0','DDR'])+
            box(25,285,165,100,'PS GP0',['Register master'])+arrow(190,335,230,335)+
            box(230,285,180,100,'sc_lite',['AXI-Lite control'],navy)+
            '<path d="M275 285 V225 H105 V180" fill="none" stroke="#173A56" stroke-width="2" stroke-dasharray="7 5" marker-end="url(#arr)"/>'+
            arrow(365,285,365,180,'',True)+
            text(25,430,'FCLK_CLK0: 100 MHz; reset release is synchronized.',21)+
            text(25,465,'Analyzer/DMA interrupts return to the PS; navy = PL, gray = PS.',17),
            'Vivado hardware blocks including DMA IP'),
        'Hardware integration choices',[
            'The DMA transfers stream data into memory. Firmware descriptor management stays inside the black box.',
            'The design uses ordinary scatter-gather DMA, not cyclic or simple-transfer mode.',
            'sc_lite connects GP0 to both register blocks. sc_dma connects sample/descriptor traffic to the HP0 memory port.',
            'JC is 3.3 V. Only the pad-to-first synchronizer stage has the documented asynchronous-input timing exception.',
        ]),2,'docs/architecture.md; fpga/scripts/build.tcl; fpga/constraints/zybo_z7_10.xdc',
        'S2MM means stream-to-memory-mapped; SG means scatter-gather. GP0 is the PS register master and HP0 the memory-facing slave port. The DMA status-control stream is disabled because this design does not supply it; leaving that interface enabled caused the recorded bring-up stall. The board has a synchronized reset scheme using proc_sys_reset. There are existing optional-profile blocks in the common bitstream, so reported resource counts cover the full routed system, not only these selected USB-first blocks. DDR controller internals and FreeRTOS are black boxes in this FPGA talk.')
    add('Self-checking tests check sample order as well as error handling',
        table(['FPGA test area','How it is checked','Recorded evidence'],[
            ['Sampling and trigger','Divider settings, masks and edge/level combinations','Part of 12/12 passing Questa cases; 2026-09-20'],
            ['History and encoding','Wrap, pre/post seam, run counts and raw byte tails','Scoreboards check exact sample order and word contents'],
            ['Stalls and overflow','Hold the stream sink, fill buffers, abort and flush','Check flags, packet endings and recovery; no silent loss'],
            ['Full acquisition path','AXI-Lite setup plus pattern-generator stimulus','Integration test reconstructs samples and checks expected patterns'],
            ['Wired loopback','JE pattern outputs return through JC inputs','18/18 cases at 1/10/100 MS/s over USB; 2026-09-19'],
        ],[23,42,35]),2,'fpga/tb/; fpga/rtl/la_patgen.sv; docs/status.md V10; usb-u05-regression.md',
        'The pattern generator is test hardware, not the UART/SPI/I²C decoder implementation. It supplies counter, walking, PRBS and protocol traffic through the selftest mux or external JE-to-JC wiring. The external matrix covers six patterns times three acquisition rates; protocol stimulus rates are scaled where needed. Procedural self-checking Questa tests verify RTL behavior; native C/Python tests and bench loopback provide different evidence layers. These are dated project results, not a new simulation run for this presentation.')
    reuse(20,2,'FPGA class results: show constrained timing closure and resource fit, not USB signaling speed. The current report dates are in the notes.')
    add('The FPGA delivers ordered sample packets, not decoded protocol messages',
        table(['Hardware contribution','What crosses the boundary','FPGA result or limit'],[
            ['Synchronous capture','8-channel samples at the selected rate','100 MS/s design; 10 ns sample interval'],
            ['Triggered history','Ordered pre/post-trigger window','Up to 32,768 requested pre-trigger samples'],
            ['Stream packaging','Raw/run words, valid-byte flags and packet endings','DMA receives defined packets; overflow is visible'],
            ['Implementation proof','Bitstream with checked clock/reset and interfaces','12/12 simulations; positive setup/hold slack at 100 MHz'],
        ],[25,40,35]),2,'fpga/rtl/; docs/limits.md; routed timing report',
        'This closes the standalone FPGA explanation. The device samples and packages signals in hardware. The black box supplies commands and memory service, then interprets and displays results. The protocol decoder code belongs to the embedded route. For the FPGA class, skip directly to slide 34 for the shared user result, limits and final conclusion. There is no need to describe the following task/USB implementation. In PowerPoint the FPGA class custom show makes this jump automatically.')
    d.SLIDES[-1]['body'] += '<p class="route-link">Continue to shared results → slide 34</p>'

    # Embedded section: hardware internals hidden; firmware APIs made explicit.
    add('The embedded system treats capture hardware as a black box',
        columns('Embedded system / firmware in focus',svg(
            box(25,65,230,145,'Capture hardware',['FPGA + DMA IP','BLACK BOX','Writes samples to DDR'])+
            arrow(255,125,325,125,'DDR data')+box(325,65,230,145,'ARM / FreeRTOS',['Owns buffer lifetime','Decodes + sends USB'],navy)+
            arrow(555,100,615,100,'USB')+box(615,65,145,145,'PC client',['BLACK BOX','CMD / ACK','Viewer'])+
            arrow(615,175,555,175,'CMD',True)+
            arrow(440,210,440,290)+arrow(440,290,145,290,'Register / DMA setup',True)+arrow(145,290,145,210,'',True)+
            text(25,370,'Inputs to firmware: completed lengths, sample bytes, status and IRQs.',20)+
            text(25,415,'Outputs from firmware: settings, descriptors, USB messages and errors.',20)+
            text(25,480,'Navy = firmware focus; gray = hardware/client black boxes',15),
            'Embedded section boundary without RTL internals'),
        'Start from the hardware contract',[
            'Assume the hardware accepts register settings and writes raw or run-encoded sample packets to DDR.',
            'Explain how software starts capture, handles completion and decides when buffers can be reused.',
            'Decoders and USB messages run in firmware. Sampling and trigger logic stay inside the hardware black box.',
            'The shared result pages begin at slide 34; no FPGA-detail slide is needed for this route.',
        ]),3,'fw-zynq/app/src/; docs/rtos-design.md; docs/protocol.md',
        'Begin the embedded-specific talk here, even if the entire FPGA section was skipped. DDR means the external memory written by the hardware DMA. Firmware receives sample bytes plus actual lengths and notifications; it does not need the RTL state-machine internals. Distinguish hardware DMA IP from the software DMA driver. The PC is also an interface black box: it sends commands/ACKs and consumes framed results. We explain the host user result later without a second detailed host-software course section.')
    reuse(13,3,'Embedded route: higher priority numbers run first. AcqTask owns acquisition and stays above priority-5 command/decoder/transport work.')
    d.SLIDES[-1]['body'] += '<p class="route-link">Higher number = higher priority. All tasks run on ARM CPU0.</p>'
    add('Firmware initializes the drivers before FreeRTOS starts the tasks',
        columns('Startup and interrupt handoff',svg(
            box(50,25,680,75,'Boot and platform setup',['FSBL loads PL and starts the ARM application'])+arrow(390,100,390,130)+
            box(50,130,680,75,'Create the application',['Memory mapping, static queues/tasks, register probe and drivers'],navy)+arrow(390,205,390,235)+
            box(50,235,680,75,'Connect interrupt handlers, then start the scheduler',['FPGA/DMA IRQ → acknowledge source → notify AcqTask'],navy)+arrow(390,310,390,340)+
            box(50,340,680,95,'Do the longer work in tasks',['AcqTask collects DMA completion; CmdTask parses commands','UsbTask services USB requests and transfer queues'],navy)+
            text(50,480,'Navy = firmware setup/task work; gray = boot service',15),
            'Firmware startup and ISR to task flow'),
        'Why the interrupt stays short',[
            'Drivers acknowledge the interrupt source so it does not keep firing.',
            'FPGA/DMA handlers notify AcqTask. They do not decode protocols or send a capture from the ISR.',
            'USB callbacks do the required bounded setup/receive staging and notify service work.',
            'The tasks use static storage, stack checks and periodic status reporting to catch failures.',
        ]),3,'fw-zynq/app/src/main.c; la_app.c; drivers/la_irq.c; drivers/la_usb_dev.c',
        'FSBL is the first-stage boot loader and is a platform service here. The ARM uses the BSP and FreeRTOS interrupt controller integration. The project has a documented SDT GIC initialization workaround so handler installation really takes effect. Interrupt handlers acknowledge/read hardware and perform ISR-safe notification or bounded required driver callbacks. USB receive completion needs pop/staging/release; do not promise zero byte copies in every callback. No command parser, protocol decoder or queue walking is performed by the FPGA/DMA ISR. All tasks shown are on CPU0; this is not an SMP or AMP implementation.')
    add('Commands are checked before they reach the acquisition hardware',
        columns('From USB bytes to a capture request',svg(
            box(25,50,210,100,'UsbTask',['RX bytes → rx_sb'],navy)+arrow(235,100,295,100)+
            box(295,50,220,100,'CmdTask',['Parse / validate'],navy)+arrow(515,100,575,100)+
            box(575,50,185,100,'ctrl_q',['Capture request'],navy)+arrow(665,150,665,225)+
            box(465,225,295,100,'AcqTask',['Apply settings / DMA','ARM or stop'],navy)+
            box(25,225,300,100,'TxTask',['ACK / NAK / STATUS','DATA_CHUNK acknowledgments'],navy)+arrow(295,150,175,225,'Responses',True)+
            text(25,400,'CONFIG → validated setup; ARM → acquisition; ABORT → stop.',23,weight=700)+
            text(25,440,'rx_sb = receive-byte stream buffer; AcqTask returns status notices.',19)+
            text(25,480,'Navy = firmware; arrows carry bytes, requests or responses',15),
            'Command parser and task dispatch'),
        'Keep one owner per operation',[
            'CmdTask owns the incremental parser and checks message lengths, fields and device state.',
            'Bad requests return NAK. A capture that exceeds the 8 MiB pool is rejected before arming.',
            'AcqTask owns DMA and acquisition changes; other tasks send it requests rather than editing descriptors.',
            'TxTask tracks capture acknowledgments and sends command responses without losing their order.',
        ]),3,'tasks/la_cmd_task.c; la_cmd_dispatch.c; la_acq_task.c; la_tx_task.c',
        'The shared wire frame has a type, sequence, payload length and CRC. CmdTask maintains the host-visible acquisition state and a validated configuration for the next run. AcqTask reconciles the state with notices such as armed, done and aborted. Configuration of active decoder slots is routed through AcqTask too; it is different from changing acquisition rate/encoding during a capture. Control queues are bounded and their full policies are documented; do not claim every overload condition can be silently accepted. ARM/ABORT, live start/stop and decoder configuration have distinct commands.')
    dma_body=columns('Hardware APIs used by AcqTask',svg(
        box(25,65,200,110,'la_top hardware',['Settings / status','0x40000000'])+arrow(225,120,285,120)+
        box(285,65,205,110,'DMA hardware',['Addresses / lengths','0x40400000'])+arrow(490,120,555,120)+
        box(555,65,200,110,'DDR buffers',['DMA writes samples'])+
        box(285,285,205,110,'AcqTask',['Descriptor owner','Prepare / collect'],navy)+
        arrow(365,285,365,175,'Setup',True)+
        '<path d="M315 285 V225 H125 V175" fill="none" stroke="#173A56" stroke-width="2" stroke-dasharray="7 5" marker-end="url(#arr)"/>'+
        arrow(490,340,655,340,'Descriptors')+arrow(655,340,655,175)+
        text(25,440,'Hardware IRQ → ISR acknowledgment → AcqTask notification',19)+
        text(25,480,'Navy = firmware; gray = hardware APIs, not their RTL internals',15),
        'Software control of analyzer and DMA register APIs'),
        'Descriptor and cache rules',[
            'A scatter-gather descriptor gives DMA a buffer address and capacity. Completion reports the actual byte length.',
            'Firmware sets both capture registers and DMA descriptors before ARM. Only AcqTask changes DMA ownership.',
            'Buffers/descriptors are 64-byte aligned; descriptors are non-cached. Cached live buffers are invalidated before reading.',
            'TKEEP marks valid stream bytes; TLAST marks packet end. Firmware uses the DMA-reported length, not buffer capacity.',
        ])
    reuse(9,3,'Software view: treat la_top and DMA as hardware APIs. IRQs are acknowledged by handlers, then delivered to AcqTask as task notifications.',dma_body)
    reuse(10,3,'Embedded route: this is the driver/task lifecycle. The sample pipeline and trigger internals remain black boxes.')
    d.SLIDES[-1]['body']=d.SLIDES[-1]['body'].replace('The measured USB transfer rate is','The measured acquire–transfer–verify throughput is')
    live_body=columns('Sample words to timed protocol events',svg(
        box(25,20,730,85,'Hardware data contract',['Raw: 1 byte / 8 channels; run word: count[23:0] | sample[7:0]'])+
        arrow(390,105,390,130)+box(25,130,730,80,'AcqTask publishes completed buffer handles',['16 × 256 KiB buffers; record the actual packet length'],navy)+
        arrow(390,210,390,240)+box(25,240,730,85,'Decoder reconstructs sample time',['sample_time += run_count; feed timed values to the C decoder'],navy)+
        arrow(390,325,390,355)+box(25,355,730,85,'TxTask sends timestamped events over USB',['Bytes, frame markers, acknowledgments and errors'],navy)+
        text(25,480,'Gray = hardware interface contract; navy = firmware work',16),
        'Embedded run-word contract and timeline reconstruction'),
        'What software does with a run',[
            'Each word gives the eight-channel value and a positive count of samples at that value.',
            'Run counts advance sample time. Splitting an unchanged run must not create an extra edge.',
            'UART also advances through a long constant run, so mid-bit observations and completed bytes are not missed.',
            'Each decoder reads through its own queue. It emits timed messages rather than a continuous raw waveform.',
        ])
    reuse(11,3,'The hardware format is restated here so this route does not require the FPGA encoder slide. One raw byte is one eight-channel sample; one 32-bit run word has run[23:0] in bits 31:8 and sample[7:0] in bits 7:0. Firmware converts counts to a sample timeline before protocol decoding.',live_body)
    reuse(12,3,'Embedded route: describe generation checks and reference counting, not the hardware FIFO implementation.')
    add('Fixed-size queues make overload behavior visible and predictable',
        table(['Object','Capacity / owner','What happens when it fills'],[
            ['Live sample pool','16 × 256 KiB buffers; AcqTask manages reuse','Stop acquisition if no buffer can be posted safely'],
            ['Decoder segment queue','8 handles per decoder; one reader per queue','Skip that decoder only, release its reference and count the gap'],
            ['Event blocks / ev_q','12 static blocks; ev_q holds 8 block handles','Drop whole events or batches and count the loss'],
            ['USB transmit pool','16 frame slots; UsbTask submits in FIFO order','Use bounded waits; report/drop live overload instead of starving commands'],
        ],[26,35,39]),3,'fw-zynq/app/src/la_app.h; drivers/la_usb_dev.h; docs/rtos-design.md P7/U05',
        'The 4 MiB live pool is sixteen segments with 256 KiB capacity, not a requirement to fill each packet. Actual lengths may be small because packets can close on a timer. seg_q stores handles; acquisition payloads are not copied to each decoder. The multi-producer ev_q is a FreeRTOS queue, not an unprotected message buffer. On the USB profile live event enqueue waits are limited to 10 ms, while burst transfers use their own bounded wait/window behavior. TxTask limits event work to four blocks per pass to keep the command path responsive. CTRL, notices and ACK queues have other documented policies and are not represented as unbounded here.')
    reuse(14,3,'Embedded route: the shared C code is the decoder implementation. The FPGA does not decode UART/SPI/I²C messages.')
    d.SLIDES[-1]['body']=table(['Protocol','How the C decoder works','Events it produces'],[
        ['UART','Use baud-derived bit centers to read RX; check parity and stop bits','Bytes, parity errors and framing errors'],
        ['SPI','Read data at the clock edge selected by polarity/phase settings; track chip select','Data bytes/words and chip-select changes'],
        ['I²C','Follow SCL/SDA edges, START/STOP and clock stretching','Address, direction, bytes, ACK and NACK'],
    ],[20,45,35])+'<div class="reuse"><b>Shared C code: common/la_decode</b><span>Live decode on device</span><span>Saved-capture decode on PC</span></div>'
    reuse(15,3,'Embedded route: explain the application frame and CRC/ACK handling after assuming the samples in DDR are valid.')
    d.SLIDES[-1]['body']=d.SLIDES[-1]['body'].replace('</svg>',text(25,330,'SOF = frame start; SEQ = sequence; LEN = payload length',17)+'</svg>',1)
    add('UsbTask owns endpoint service and handles session changes',
        table(['USB operation','Firmware responsibility','Why it matters'],[
            ['Enumeration / control','Serve descriptors and CDC-ACM requests on endpoint 0','Windows binds the standard serial driver'],
            ['Host-to-device data','Stage completed OUT bytes, then feed CmdTask through rx_sb','USB read boundaries do not need to match application frames'],
            ['Device-to-host data','Reserve a static TX slot; submit ready slots in enqueue order','Do not overwrite an in-flight frame or reorder capture chunks'],
            ['Reset / session change','Invalidate stale work, clear transfer state and request acquisition stop','Old completions must not be treated as part of a new session'],
        ],[26,39,35]),3,'tasks/la_usb_task.c; drivers/la_usb_dev.c; la_usb_core.c; docs/usb-link.md',
        'USB CDC-ACM is Communications Device Class / Abstract Control Model. The hardware controller/PHY is a PS peripheral behind the firmware driver. UsbTask handles class requests and submission; required vendor callbacks perform bounded staging. USB frame slots are atomically reserved and a ready FIFO preserves ordering. The portable session core handles bus reset, configuration, suspend and resume actions; implementation of a path is not evidence that every physical unplug/suspend scenario passed on the bench. Session generations prevent stale work crossing runs. The development USB identity remains for in-house use.')
    add('Firmware tests check parsing, buffer reuse, overload and recovery',
        table(['Test area','Recorded result','What the result supports'],[
            ['Portable C code','18/18 native tests; 2026-09-20','Commands, decoders, buffer rules, USB session and trace logic'],
            ['Release / PC tools','22/22 release prechecks; 180 host tests by 2026-10-18','Generated definitions and host protocol/UI behavior'],
            ['Repeated captures','500 captures passed over USB; 2026-09-19','Exact capture lengths and clean parser/checksum results'],
            ['USB recovery','Split/coalesced frames, garbage/CRC recovery and close during capture','Bounded resynchronization; close during capture returns to IDLE'],
        ],[25,36,39]),3,'docs/status.md U04/U05/W09/W11; usb-u04-protocol.md; usb-u05-regression.md',
        'These are recorded snapshots from different runs; the 180-test host suite is the final 18 October closeout run. Native tests run the portable C logic on Windows; they cannot prove ISR timing or physical USB behavior. Bench tests exercise the driver/hardware path. A corrupted frame length can require bounded flush traffic before resynchronization; do not call it instantaneous recovery. The 500-capture soak ran for about 527.8 seconds and checked each capture. Physical unplug/replug and suspend/resume remain separately documented human gates. FPGA simulation is evidence for the FPGA route, not firmware scheduling.')
    reuse(18,3,'Embedded-class result: this is application acquire/transfer/verify throughput, not a raw USB bus rate. The actual capture pool is 8 MiB; each trial uses sixteen 4 MiB captures.')
    reuse(19,3,'This closes the embedded-specific route with qualified live behavior. The following pages are shared user results and conclusion; no RTL detail is needed.')

    # Shared close: useful after either class route.
    result=columns('Actual UI / real UART demo capture',
        '<div class="screenshot"><img src="uart-demo-capture.png" alt="Logic analyzer UI configured for the real UART demo"></div>',
        'Complete-system result',[
            'The UART waveform and decoded bytes share one timeline. The capture used 10 MS/s, 64k raw, CH0 only, slot 0 UART at 115200 baud.',
            'The decoded frame is 4C 41 2D 53 33 0A: the bytes for LA-S3 followed by a newline.',
            'The current W11 image (firmware 0x000A0000) produced 1,464 live events and 1,220 real waveform packets with zero gaps on 18 October 2026.',
            'On 2026-09-26, the USB-profile image booted from SD without a JTAG load; all six UI end-to-end tests passed on that image.',
        ])
    reuse(16,4,'Shared close: this is the user-facing result of the hardware and firmware working together. The UART screenshot is a real 18 October 2026 capture from firmware 0x000A0000; the SD boot and six API/SSE workflows remain the dated 26 September 2026 evidence.',result)
    add('The decoded UART bytes match the transmitted message',
        '<div class="decode-result">'
        '<h3 class="decode-heading">Captured UART frame · CH0 · 10 MS/s</h3>'
        '<img class="decode-frame" src="uart-demo-frame.png" '
        'alt="Close-up of the real UART waveform with decoded bytes 4C 41 2D 53 33 0A">'
        '<div class="byte-map">' + ''.join(
            f'<div class="byte-card"><p class="byte-hex">{value}</p>'
            f'<p class="byte-char">{character}</p></div>'
            for value,character in [('4C','L'),('41','A'),('2D','-'),
                                    ('53','S'),('33','3'),('0A','Newline')]) + '</div>'
        '<div class="decode-facts">'
        '<div><h3>UART settings</h3><p>115200 baud · 8 data bits<br>No parity · 1 stop bit</p></div>'
        '<div><h3>Bit timing</h3><p>8.68 µs per bit<br>About 87 samples per bit</p></div>'
        '<div><h3>Decoded message</h3><p><b>LA-S3 + newline</b><br>All six bytes match the stimulus</p></div>'
        '</div><p class="demo-link">Video demonstration: '
        '<a href="https://youtu.be/1xx84Ndi4vQ?si=ldsuYEWvYVqu2u6-">'
        'youtu.be/1xx84Ndi4vQ</a></p></div>',
        4,'docs/hardwarex/assets/uart-demo/uart-demo-frame.png; '
        'build/reports/uart-demo-ui/screenshots.json; 18 October 2026',
        'This is an enlarged crop of the real external ESP32-S3 UART capture, '
        'not a generated waveform. Read the six decoded hexadecimal bytes from '
        'left to right: 4C is L, 41 is A, 2D is the ASCII hyphen, 53 is S, '
        '33 is 3, and 0A is a line feed. Together they reproduce LA-S3 followed '
        'by a newline. The upper trace shows the sampled input, and slot 0 '
        'places decoded-byte labels at their sample positions. UART sends a '
        'start bit, eight data bits least-significant bit first, and one stop '
        'bit in this 8N1 configuration. At 115200 baud a bit lasts approximately '
        '8.68 microseconds; 10 MS/s gives approximately 87 samples per bit. '
        'These timing values are calculated from the configured rates, not a '
        'separate jitter measurement. This checks that the complete acquisition '
        'and decoding path recovers the known six-byte message.')
    reuse(21,4)
    reuse(22,4)
    d.SLIDES[-1]['body']=d.SLIDES[-1]['body'].replace('1.62 MB/s transfer tests and working UI flows after SD boot','1.62 MB/s acquire–transfer–verify throughput; UI tests passed after SD boot')
    d.SLIDES[-1]['body'] += '<p class="route-link">Fast sampling is buffered in DDR; full-rate raw data is not streamed continuously over USB.</p>'
    reuse(23,None)

    assert len(d.SLIDES)==38
    overview=list(range(1,7)); fpga=list(range(7,19))
    embedded=list(range(19,34)); close=list(range(34,39))
    d.ROUTES={'FPGA system':overview+fpga+close,
              'Embedded system':overview+embedded+close}
    d.SLIDES[1]['jumps']={'FPGA system':7,'Embedded system':19,'Shared results':34}
    d.SLIDES[17]['jumps']={'Continue to shared results → slide 34':34}
    for slide in d.SLIDES:
        slide['notes']=slide['notes'].replace('FPGA class','FPGA system').replace('FPGA-class','FPGA-system').replace('Embedded-class','Embedded-system').replace('embedded-specific','embedded-system').replace('class route','system route')
