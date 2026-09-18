# Proposed direct Zynq USB link

Prepared: 2026-09-17. Status: **design proposal; not implemented or bench-verified**.
Baseline inspected: `b30adfd`. Overall authority: [plan.md](plan.md).
Execution sequence: [usb-migration-implementation-instructions.md](usb-migration-implementation-instructions.md).
Results and authorization: [status.md](status.md).

## 1. Purpose and boundary

Provide one bidirectional analyzer connection from Zybo Z7-10 PS USB0 to a
Windows PC through CDC-ACM. Preserve `la_proto` framing, commands, captures,
live decoding, trace and host exports. The ESP32 is absent from this candidate
data path. Retain the existing gateway build as a rollback option during
evaluation. Whether wireless remains an optional supported product profile
is an adoption decision, not a consequence of adding USB code.

```text
PC la-cli / existing serial transport
             | CDC-ACM bulk OUT commands / bulk IN framed replies and data
Zybo J10 -> USB3320 PHY -> PS USB0 -> UsbTask
                                      | RX stream -> CmdTask
                                      | TX buffer handles <- TxTask
PL -> ordinary AXI SG DMA -> DDR -> acquisition / decode -> TxTask
```

This is one analyzer data cable, not a promise of one cable for board power,
programming and data. Debug output must remain on the existing console;
unframed console text must never enter the CDC analyzer stream.

## 2. Evidence and corrections to the initial proposal

| Item | Evidence inspected on 2026-09-17 | Consequence |
|---|---|---|
| USB0 preset | `fpga/board_files/zybo-z7-10/A.0/preset.xml`: USB0 enabled, 60 MHz, MIO 28..39, active-low reset MIO 46; `fpga/scripts/build.tcl` applies the preset | Reuse the current PL design provisionally. Confirm the actual XSA/boot artifacts in U01; no automatic bitstream rebuild. |
| Generated FreeRTOS BSP | `build/vitis/ws/lap/ps7_cortexa9_0/fr/bsp/include/xparameters.h`: one `XUSBPS`, base `0xe0002000`, encoded interrupt `0x4015`, parent `0xf8f01000` | These are local generated observations, not a fresh-build or hardware test. Do not read the unrelated template `libsrc/common/src/xparameters.h`. |
| Driver present | Same BSP contains `libsrc/usbps/src/xusbps*.{c,h}`, matching `xusbps_g.c`, exported headers and generated `libusbps.a` | Use `usbps` / `XUsbPs`. Adding `usbpsu` / `XUsbPsu` is the wrong platform choice. Presence does not prove USB initialization. |
| Installed examples | `C:/AMDDesignTools/2026.1/data/embeddedsw/XilinxProcessorIPLib/drivers/usbps_v2_9/examples/` contains device interrupt, Chapter 9, storage and audio examples | Use matching-version examples as references. No CDC example was found in this installed directory; do not assume the `usbpsu` FreeRTOS examples apply. |
| External CDC reference | AMD has a Zynq-7000 CDC techtip with a bare-metal implementation, originally for SDK 2015.1 / ZC702 | The statement that no first-party CDC example exists is too broad. Audit the old example before reuse; it is not a tested Vitis 2026.1 FreeRTOS port. [AMD CDC techtip](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841624/Zynq-7000%2BAP%2BSoC%2BUSB%2BCDC%2BDevice%2BClass%2BDesign%2BExample%2BTechtip). |
| Controller model | Local `usbps` uses device queue heads and transfer descriptors (`dQH` / `dTD`) | Do not design against the initial proposal's unverified DWC2 identification or import DWC2 register code. Use the Zynq-7000 driver and hardware documentation. |
| PC compatibility | Existing `host/la_cli/transport.py` supports `serial://COMx` | No new host transport is planned. Partial writes, blocking reads, disconnect and timing behavior still require audit and tests. |
| Performance | No direct Zynq USB measurements exist in this checkout's ledger | 480 Mbit/s is a link rate. Neither 20-40 MB/s nor sub-millisecond application latency is an acceptance result or promise. |

The AMD [usbps API reference](https://xilinx.github.io/embeddedsw.github.io/usbps/doc/html/api/group__usbps.html)
documents this driver family. The installed source version is the implementation
reference; online `master` documentation may differ. Record source versions,
licenses and any adapted files before importing example code. Project changes
must live in this repository, never in the installed AMD tool tree or generated BSP.

## 3. Board, power and host prerequisites

For the documented Zybo Z7 peripheral configuration, use Micro-AB **J10** with
JP1 and JP2 open and J11 unused. USB board power is listed at **J12**, selected
through JP6; J10 must not be assumed to power the board. Confirm the physical
board revision, jumpers, power source, VBUS/ID behavior and a data-capable
cable on the bench before loading USB firmware. Use a direct PC port for the
baseline, given the recorded S3 hub failure. That observation does not establish
a universal fault in all hubs. These requirements follow sections 1 and 9 of
the [Digilent Zybo Z7 manual](https://digilent.com/reference/_media/reference/programmable-logic/zybo-z7/zybo-z7_rm.pdf);
check them against the actual revision's schematic.

Keep J12/debug or a supported external power supply available during bring-up.
Record host Windows version, PC USB controller/port, board revision, cable,
power selection, boot method and serial number alongside every bench report.
Confirm negotiated high speed from host/controller evidence; successful COM
enumeration alone does not establish high speed.

## 4. USB device contract

Start with a single CDC-ACM function and no composite network, storage or audio
function. Proposed endpoint allocation (confirm against `XUsbPs` in U02):

| Endpoint | Direction | Function | Proposed packet size |
|---|---|---|---|
| EP0 | IN/OUT | Standard control and CDC requests | 64 bytes |
| EP1 | OUT | Host command byte stream | HS 512 bytes; FS 64 bytes |
| EP1 | IN | Framed analyzer response/data byte stream | HS 512 bytes; FS 64 bytes |
| EP2 | IN | CDC interrupt notifications | 16 bytes; interval encoded for each speed |

Use a CDC control interface and a data interface, with consistent interface
numbers, endpoint counts, configuration length, CDC functional descriptors
(header, ACM, union and call-management as applicable) and string lengths.
Implement speed-appropriate configuration, device qualifier and other-speed
descriptors. Full-speed operation is a compatibility case, not a passing HS
performance gate. Do not advertise capabilities that the firmware omits.

Use the documented Windows class/subclass binding for the single-function
device; verify `usbser.sys` binds without a custom INF on the actual PC.
Microsoft documents automatic binding for the appropriate CDC identifiers in
[USB serial driver installation](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-driver-installation-based-on-compatible-ids).
Choose and record a legitimately usable VID/PID and stable device serial
before enumeration; do not silently reuse AMD/Digilent product identifiers.
If identity allocation is unavailable, record that gate as blocked.

EP0 must validate direction, recipient, interface, value and length before
accessing data. Handle standard enumeration/configuration/reset/halt requests,
`SET_LINE_CODING`, `GET_LINE_CODING` and `SET_CONTROL_LINE_STATE`; define
`SEND_BREAK` behavior consistently with advertised capabilities. Store valid
line coding, but do not change the sampling clock or throttle USB to that baud.
DTR/RTS must not reset the board. Initial policy: line state is advisory and
traffic is enabled when configured; a DTR transition alone does not cancel a
capture. Test actual pyserial open/close behavior. Unsupported requests STALL
cleanly, and the next valid setup request must recover.

USB transfer boundaries are not `la_proto` frame boundaries. Accept split and
coalesced command frames. Preserve order across frame boundaries on IN. Test
zero/short packets and exact max-packet multiples; define ZLP behavior from the
driver and host transfer contract, without treating ZLP as an analyzer message.
Flush sparse replies within a bounded task-service interval (initial target
1 ms), independently of the bulk batching threshold. Measure end-to-end latency.

## 5. Integration and transport selection

Initial candidate selection is **build-time**, with a named gateway profile
and a direct-USB profile. Keep the gateway default until adoption. Carry the
selection through the canonical native-Windows firmware build and report it
in the build report and console banner. If new runner arguments are needed,
document and test them before treating their commands as available.

In direct-USB mode:

- Only UsbTask produces analyzer bytes into `app->rx_sb`; CmdTask remains its
  sole reader. Disable UART0 analyzer RX/TX and SPI service initialization in
  this profile, while retaining the separate debug console.
- TxTask remains the serializer for ACK/NAK, PONG/STATUS, DATA_*, EVT_* and LOG.
  Route every serialized frame to USB, not just the three burst frame types.
- Replace transport-specific capacity checks and wakeups as well as writes:
  current `tx_write()`, `tx_pump()`, SPI flush/ACK-window branches, live-event
  `tx_sb` space checks and `la_link_uart_tx_kick()` all need review.
- Preserve protocol ACK, retransmit and capture-buffer release semantics.
  USB endpoint completion does not equal a host protocol ACK. Do not enable
  `spi_bulk` just to obtain the SPI path's window bypass.
- Existing `LINK_CFG` selects the gateway's UART/SPI burst routing. In the
  direct-USB profile return the existing unsupported-command/error response
  before changing state; do not redirect any frames to absent hardware.
  No new USB `LINK_CFG` value or wire-protocol version is planned initially.
  Document this profile-specific behavior and test it.

CmdTask's queue into TxTask already provides serialized replies; preserve that
ownership rather than introducing a second USB writer. There must be no
simultaneous USB/UART command producers and no automatic mid-session fallback.
Optional wireless operation is a separate profile until explicitly redesigned.

## 6. RTOS, buffer ownership and cache contract

UsbTask owns controller servicing, class state and endpoint submission. The
repository requires notification-only/send-from-ISR interrupt work. Audit the
vendor interrupt dispatcher before connecting it: example callbacks may do
class processing and data copies in interrupt context. U02 must demonstrate
how to defer that work and prevent level-triggered interrupt storms without
losing pending setup/completion events. Any indispensable interrupt masking
or acknowledgement must be reconciled explicitly with `AGENTS.md`; do not
silently introduce a policy exception or copy the existing SPI ISR approach.
If a compliant deferred service cannot be established, stop at that gate.

Derive interrupt installation from the generated config and the project's
`la_irq_connect()`/FreeRTOS SDT path. `0x4015` is encoded metadata, not a raw
GIC vector. Verify trigger interpretation, effective priority, handler dispatch
and FreeRTOS syscall constraints; do not hard-code the quoted number as a vector.

Use bounded static storage. Before code, fill a sizing table with each pool's
count/element size, alignment, memory region, owner and worst-case occupancy;
include stacks, descriptors, in-flight buffers and control-message reserve.
Use existing memory/cache helpers and linker layout as the starting point.

| Object/path | Ownership transfer | Full/blocked policy |
|---|---|---|
| TX free pool / ready queue | TxTask obtains a free buffer, encodes/copies a complete frame, submits its handle; UsbTask retains it through IN completion | All-or-nothing admission. Bounded wait within the existing 2 s no-progress limit for burst/control traffic; propagate failure. Never enqueue a partial frame. |
| In-flight USB TX | Driver/controller reads a UsbTask-owned buffer | Reuse only after completion or confirmed endpoint stop/flush. An API accepting a pointer is not completion. Never submit `tx_write()`'s stack-local `wire[]` directly. |
| USB OUT buffers / `rx_sb` | Controller -> UsbTask -> CmdTask bytes; driver handle retained until all received bytes are accounted for | Preserve pending offset on partial stream-buffer writes; stop rearming/allow NAK backpressure when full. No silent byte truncation or parser resynchronization as a substitute for flow control. |
| EP0/notification state | UsbTask owns control state and dedicated storage | Reserve capacity independently of bulk queues. Coalesce repeat notifications if appropriate; never block setup on bulk backlog. |
| Live events | Existing decoder handle queues -> TxTask -> USB queue | Keep bounded overload handling and accurate overrun reports. Define stage/counter mapping in `protocol.md` before extending it; report pending loss when space returns. |
| IRQ wakeup | ISR -> task notification | Notification may coalesce; drain all pending hardware causes before sleeping. Prove re-enable races and sustained load do not lose events. |

Audit the installed driver's dQH/dTD alignment and cache operations. Specify
clean-before-device-read and invalidate-before-CPU-read, with cache-line-safe
ranges and memory barriers. Account for driver-maintained descriptors separately
from application buffers to avoid invalidating unrelated dirty data. Do not
confuse USB descriptors with the acquisition AXI DMA descriptors: acquisition
continues to use ordinary SG in burst and live modes, with cyclic mode disabled.

## 7. Reset, disconnect and recovery

Define states at least for detached, default/addressed, configured, suspended
and recovering. Bus reset/unconfigure/disconnect starts a new session generation.
Quiesce endpoints, invalidate old queued work, and return each buffer exactly
once only after hardware cannot access it. Ignore late completions belonging to
an older generation. Clear partial parser input and stale ACK/window state through
their owning tasks. Stop/abort acquisition through its existing control path;
do not manipulate capture ownership directly from USB callbacks.

After host loss, capture ownership must recover within the documented 2 s
no-progress bound plus bounded cleanup (initial total acceptance limit 3 s).
If the hardware exposes no reliable disconnect indication, document detection
through bus state/no-progress and verify the same cleanup bound. Test suspend
separately from disconnect: gate submissions and retain or abort a session
according to the documented timeout policy. Do not invent remote-wakeup support.

Re-enumeration may change the COM number. Reopen through a fresh host client,
then require PING, IDLE status and a bit-exact capture. A COM close without bus
reset must also expire abandoned work and allow a new session. Keep commands
and EP0 service responsive when IN stalls. Full-speed fallback, endpoint halt,
reset during active DMA and host process termination are separate tests.

## 8. Acceptance and remaining decisions

Implementation gates and commands are specified in the companion execution
guide. Required outcomes include Windows HS enumeration, exact burst data,
60 s zero-drop baseline live decoding, preserved 25 ms p99 isolated-byte gate,
trace/export compatibility, bounded failure recovery and >=1.5 MB/s verified
payload over 64 MiB (67,108,864 bytes). Use decimal MB/s and state timing bounds;
the existing P9 harness already transfers MiB despite its short 'MB' label.

Decisions still open: usable USB identity; demonstrated ISR deferral; static
memory sizing and cache arrangement; exact descriptor bytes; board/power bench
confirmation; host tuning justified by measurements; optional wireless support
and the live browser-view scope. None is a passing implementation result.

## 9. Frozen integration contract (U02, 2026-09-18)

U02 resolved the decisions required before U03 coding. This section is the
frozen contract for the direct-USB profile; `la_usb_core.{h,c}` implements the
portable session and completion model and its host tests are part of
`test-native`.

**Driver and interrupt path.** Use `usbps` / `XUsbPs` only (never `usbpsu` or
DWC2 register code). Init: `XUsbPs_LookupConfig(0xE0002000)` (the BSP
value; U03 confirms the controller registers on target), `CfgInitialize`,
`XUsbPs_DeviceMemRequired`, `ConfigureDevice`, `IntrSetHandler`,
`EpSetHandler`, `IntrEnable(UR|UI)`, `Start`. The GIC line is derived from
`XPAR_XUSBPS_0_INTERRUPTS` `0x4015` = SPI 21 = GIC ID 53, installed through
`la_irq_connect()` at priority `0xA0`, handler `XUsbPs_IntrHandler`.

**ISR deferral (amended during U03 bench).** The vendor RX interrupt loop
requires the endpoint callback to pop **and release** every completed
descriptor before it advances; a deferred pop leaves descriptors inactive and
spins the ISR forever (bench 2026-09-18: `ulPortInterruptNesting` stuck at 1,
`xTickCount` frozen, console dead). The implemented bring-up path therefore
leaves the UI and SLE sources masked and services setups and endpoint
completions from UsbTask (`la_usb_poll_setup`, `la_usb_poll_rx`,
`la_usb_poll_tx`, including the vendor cursor advance); only bus reset,
error and port change remain interrupt-driven, and their general handler is
notification-only. A polled service is a sleep-based periodic service, not a
busy-wait. The original notification-only-callback design remains the target
if a pop-and-release-safe deferral is demonstrated; `la_usb_core` keeps the
session/generation model and its host tests.

**Endpoints and descriptors.** EP0 control 64 B (2 buffers); EP1 OUT/IN bulk,
16 x 512 B buffers each at HS, 64-byte packets at FS; EP2 IN interrupt 16 B
for CDC notifications. One CDC-ACM function with IAD: device class `0x02`,
control interface `0x02/0x02/0x01` with header/call-management/ACM/union
functional descriptors, data interface `0x0A`; device, configuration,
other-speed, device-qualifier and string descriptors. The implemented
configuration descriptor is 75 bytes (`la_usb_desc.c`; the U02 draft said 66,
corrected during U03) and Windows bound `usbser.sys` to it on the bench
(2026-09-18). Automatic `usbser.sys` binding is the gate (the old AMD CDC
techtip needed a custom driver and is not copied).

**Identity.** Development/test identity `0x1209:0x0001` (pid.codes test PID)
with a fixed serial string and a warning in source that it is not universally
unique and must not ship. Release identity remains open for U07/U09; the test
PID must not appear in release artifacts.

**Class behavior.** `SET_LINE_CODING` (7-byte OUT data) is latched but never
changes the sampling clock or throttles USB; `GET_LINE_CODING` returns it;
`SET_CONTROL_LINE_STATE` is advisory (DTR/RTS never reset the board or cancel
a capture); `SEND_BREAK` is accepted and ignored consistently with the
advertised ACM capabilities. Unsupported requests stall EP0 cleanly and the
next setup recovers. Bulk IN uses `XUsbPs_EpBufferSend` without ZLT: the
analyzer stream is byte-oriented, tests must not depend on transfer
boundaries, and exact max-packet multiples are terminated by host read counts.
USB transfer boundaries are not `la_proto` frame boundaries; split and
coalesced command frames are accepted.

**Buffers, ownership and cache.** USB DMA memory is a 64 KiB pool in the
existing non-cached window at offset `0x00900000` (after the 8 MiB capture and
1 MiB SG pools), 64-byte aligned, sized against `XUsbPs_DeviceMemRequired` at
init; U03 adds it to `la_mem_geom` with host tests. TX frames use a static
8 x 1088-byte pool: TxTask fills a complete frame and hands the handle to
UsbTask; UsbTask owns it until IN completion, then returns it. Admission is
all-or-nothing with a bounded wait inside the existing 2 s no-progress limit;
a partial frame is never submitted. RX completions go through the 24-entry
ring (worst case of 2 EP0 plus 16 bulk OUT descriptors; the U02 draft said 16,
corrected during U03) into `app->rx_sb` (8 KiB, single writer UsbTask,
single reader CmdTask);
a full `rx_sb` stops pops/re-primes so the controller NAKs. Ring capacities
match the descriptor counts so a full ring cannot lose completions; overflow
is counted, never silent. Payloads live in non-cached DDR, so no application
cache maintenance is needed; if a buffer is ever moved to cached memory,
invalidate-before-CPU-read with cache-line-rounded lengths applies.

**Build profiles.** Build-time selection, gateway default:
`python tools/run.py fw-zynq -- --profile usb` defines `LA_TRANSPORT_USB=1`
and builds app component `la_app_usb`; the default builds `la_app` unchanged.
`jtag-zynq -- --profile usb` loads the matching ELF. The build report and
console banner record the active profile. In the USB profile UART0 analyzer
RX/TX and SPI service initialization are skipped, the debug console stays on
UART1, every frame class is routed to USB through the single TxTask writer,
and `LINK_CFG` returns the existing unsupported response before any state
change.

**Test selectors.** USB tests are named `test_usb_*` and run with
`python tools/run.py test-e2e -- --target serial://COMx -k test_usb`; common
tests stay unmodified, and ESP/TCP/SPI-only tests are excluded by name and
never counted as passed. A zero-selection or all-skipped run is not a pass.
