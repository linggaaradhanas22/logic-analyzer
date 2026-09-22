/* oxlint-disable next/no-html-link-for-pages -- Vinext client Link navigation fails in production. */
import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { StatusBadge } from '@/components/docs/status-badge';

export const metadata = {
  title: 'Using the analyzer - RTOS Logic Analyzer',
  description:
    'Power, load, connect and verify the direct-USB logic analyzer through the unified browser UI, then capture, decode and export real signals.',
};

function Cmd({ children }: { children: string }) {
  return <pre className="doc-pre">{children}</pre>;
}

function Inline({ children }: { children: React.ReactNode }) {
  return <code className="doc-inline-code">{children}</code>;
}

export default function UsingPage() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs
          items={[
            { label: 'System', href: '/' },
            { label: 'Using the analyzer' },
          ]}
        />
        <header className="doc-header">
          <p className="doc-eyebrow">
            Operator runbook · Direct USB · Unified UI
          </p>
          <h1>From power-off to a trusted capture</h1>
          <p className="doc-answer">
            Start with the verified JTAG development boot, connect the Zybo
            directly to the PC over J10, prove the complete path with an
            internal counter waveform, and only then attach a real 3.3 V target.
            The browser workspace handles connection, capture, waveform
            inspection, decode, live events, export and trace in one page.
          </p>
          <div className="doc-status-row">
            <StatusBadge
              status="bench-verified"
              label="Direct USB and unified UI bench-verified"
            />
            <StatusBadge
              status="known-issue"
              label="Development USB identity 1209:0001"
            />
            <StatusBadge
              status="blocked"
              label="Physical SD boot remains unverified"
            />
          </div>
          <p className="doc-scope">
            Current default: JP5 = JTAG, JP6 = USB, J12 power/JTAG/console, J10
            analyzer data. The ESP32 gateway is optional and is not a supported
            unified-UI target.
          </p>
        </header>

        <section className="doc-section" aria-labelledby="route-title">
          <h2 id="route-title">The known-good route</h2>
          <div className="doc-flow-row" aria-label="Startup sequence">
            <span className="doc-flow-node" data-state="verified">
              Safety check
            </span>
            <span className="doc-flow-arrow" aria-hidden="true">
              -&gt;
            </span>
            <span className="doc-flow-node" data-state="verified">
              Power from J12
            </span>
            <span className="doc-flow-arrow" aria-hidden="true">
              -&gt;
            </span>
            <span className="doc-flow-node" data-state="verified">
              Load with JTAG
            </span>
            <span className="doc-flow-arrow" aria-hidden="true">
              -&gt;
            </span>
            <span className="doc-flow-node" data-state="verified">
              J10 USB Serial Device
            </span>
            <span className="doc-flow-arrow" aria-hidden="true">
              -&gt;
            </span>
            <span className="doc-flow-node" data-state="verified">
              Connect la-ui
            </span>
            <span className="doc-flow-arrow" aria-hidden="true">
              -&gt;
            </span>
            <span className="doc-flow-node" data-state="verified">
              Counter self-test
            </span>
            <span className="doc-flow-arrow" aria-hidden="true">
              -&gt;
            </span>
            <span className="doc-flow-node" data-state="verified">
              Real target
            </span>
          </div>
          <p className="doc-flow-caption">
            Do not skip the counter self-test. It separates analyzer setup
            faults from target-signal faults before the two systems are
            connected.
          </p>
        </section>

        <nav className="doc-section" aria-labelledby="contents-title">
          <h2 id="contents-title">Runbook sections</h2>
          <div className="doc-index">
            {[
              ['01', 'Before power', '#before-power'],
              ['02', 'Power and load', '#power-load'],
              ['03', 'Find the COM port', '#find-port'],
              ['04', 'Open and check the UI', '#open-ui'],
              ['05', 'First self-test', '#self-test'],
              ['06', 'Protocol recipes', '#protocol-tests'],
              ['07', 'Capture a target', '#real-target'],
              ['08', 'Export and shut down', '#export-shutdown'],
              ['09', 'Troubleshooting', '#troubleshooting'],
            ].map(([order, name, href]) => (
              <a className="doc-index-card" href={href} key={href}>
                <span className="order">{order}</span>
                <strong>{name}</strong>
              </a>
            ))}
          </div>
        </nav>

        <section className="doc-section" id="before-power">
          <h2>1. Before applying power</h2>
          <div className="doc-grid cols-2">
            <article className="doc-panel">
              <h3>Board setup</h3>
              <ul>
                <li>
                  <strong>JP5 = JTAG.</strong> Power-cycle once after moving it.
                </li>
                <li>
                  <strong>JP6 = USB.</strong> The recorded bench powers the
                  board through J12.
                </li>
                <li>
                  <strong>JP1 and JP2 open; J11 unused.</strong>
                </li>
                <li>
                  <strong>J12 to the PC:</strong> power, JTAG and FTDI debug
                  console.
                </li>
                <li>
                  <strong>J10 directly to the PC:</strong> analyzer CDC-ACM
                  data.
                </li>
                <li>
                  Leave all JC acquisition inputs disconnected for the first
                  test.
                </li>
              </ul>
            </article>
            <article className="doc-panel">
              <h3>Electrical and host checks</h3>
              <ul>
                <li>
                  JC, JE and JD are 3.3 V LVCMOS only. Never apply 5 V directly.
                </li>
                <li>J10 does not replace J12 board power.</li>
                <li>Use data-capable cables and a direct PC port for J10.</li>
                <li>Use native Windows and Chrome or Edge.</li>
                <li>
                  Install the project <Inline>.venv</Inline> and host package.
                </li>
                <li>
                  Run <Inline>python tools/run.py test-native</Inline> once for
                  host capture decode.
                </li>
              </ul>
            </article>
          </div>
          <p className="doc-scope">
            The recorded board is a Zybo Z7-10 Rev D.0. Check the schematic for
            another revision. The FPGA pins have no project-added input
            protection; see{' '}
            <a className="inline-source" href="/reference/wiring">
              wiring
            </a>{' '}
            before probing a target.
          </p>
        </section>

        <section className="doc-section" id="power-load">
          <h2>2. Power and load the current image</h2>
          <div className="doc-prose">
            <ol>
              <li>With power removed, set JP5 and JP6.</li>
              <li>Connect J12 and apply board power.</li>
              <li>Connect J10 directly to the PC.</li>
              <li>
                From the repository root, load the default direct-USB profile:
              </li>
            </ol>
          </div>
          <Cmd>{`python tools\\run.py jtag-zynq`}</Cmd>
          <p className="doc-prose">
            This resets the PS, programs the PL, runs the FSBL, downloads{' '}
            <Inline>la_app_usb</Inline> and starts CPU0. It writes{' '}
            <Inline>build/reports/jtag-zynq.json</Inline>. The image is
            volatile, so repeat this after every board power loss.
          </p>
          <p className="doc-prose">
            On a clean checkout, build the bitstream/XSA before the firmware:
          </p>
          <Cmd>{`python tools\\run.py bit
python tools\\run.py fw-zynq
python tools\\run.py jtag-zynq`}</Cmd>
          <div className="doc-evidence">
            <div className="doc-evidence-item" data-status="blocked">
              <p>
                <strong>Do not substitute an assumed SD boot.</strong>
              </p>
              <p>
                <Inline>python tools/run.py boot</Inline> builds a USB-profile
                BOOT.BIN, but physical startup from the SD card with JP5 = SD
                remains an open human release gate.
              </p>
            </div>
          </div>
        </section>

        <section className="doc-section" id="find-port">
          <h2>3. Find the analyzer COM port</h2>
          <p className="doc-prose">
            After the load, J10 enumerates as{' '}
            <strong>USB Serial Device (COMx)</strong>. The current in-house
            identity is VID:PID <Inline>1209:0001</Inline>.
          </p>
          <p className="doc-prose">
            Confirm the port under{' '}
            <strong>Device Manager &gt; Ports (COM &amp; LPT)</strong>, or use
            the UI&apos;s Ports list and <strong>Rescan</strong> in the next
            step. The legacy la-cli port filter covers USB-UART adapter families
            but does not currently include the project&apos;s 0x1209 direct-USB
            identity.
          </p>
          <div className="doc-grid cols-2">
            <article className="doc-panel">
              <h3>Choose this</h3>
              <p>
                <Inline>USB Serial Device</Inline>, VID:PID{' '}
                <Inline>1209:0001</Inline>, on J10.
              </p>
            </article>
            <article className="doc-panel">
              <h3>Do not choose this</h3>
              <p>
                The J12 FTDI console, VID:PID <Inline>0403:6010</Inline>. It
                carries logs, not analyzer frames.
              </p>
            </article>
          </div>
          <p className="doc-scope">
            COM13 was used on the recorded bench, but Windows may assign another
            number. Identify the port by device identity rather than copying the
            old COM number.
          </p>
        </section>

        <section className="doc-section" id="open-ui">
          <h2>4. Open and check the UI</h2>
          <Cmd>{`.\\.venv\\Scripts\\la-ui.exe`}</Cmd>
          <p className="doc-prose">
            Open <Inline>http://127.0.0.1:8080/</Inline> in Chrome or Edge. The
            command prints the URL; it does not launch a browser automatically.
          </p>
          <div className="doc-grid cols-3">
            <article className="doc-panel">
              <h3>Connect</h3>
              <ol>
                <li>
                  Select the <Inline>1209:0001</Inline> device in Ports.
                </li>
                <li>
                  Use <strong>Rescan</strong> if it appeared after page load.
                </li>
                <li>
                  Confirm Target becomes <Inline>serial://COMx</Inline>.
                </li>
                <li>
                  Click <strong>Connect</strong>.
                </li>
              </ol>
            </article>
            <article className="doc-panel">
              <h3>Check identity</h3>
              <ol>
                <li>
                  The link pill must say <strong>linked</strong>.
                </li>
                <li>
                  Firmware should be <Inline>0x00090000</Inline>.
                </li>
                <li>
                  PL should be <Inline>0x4C410002</Inline>.
                </li>
                <li>
                  Click <strong>Ping</strong> and check nonzero uptime.
                </li>
              </ol>
            </article>
            <article className="doc-panel">
              <h3>Check idle state</h3>
              <ol>
                <li>
                  Bottom bar says <Inline>session idle</Inline>.
                </li>
                <li>
                  <Inline>events 0</Inline> before live testing.
                </li>
                <li>
                  <Inline>drops 0</Inline>.
                </li>
                <li>No warning text is present.</li>
              </ol>
            </article>
          </div>
          <p className="doc-scope">
            Stop if Connect or Ping fails. The UI exclusively owns the COM port
            while connected; do not run la-cli or la-web against it at the same
            time.
          </p>
        </section>

        <section className="doc-section" id="self-test">
          <h2>5. Run the first self-test</h2>
          <p className="doc-prose">
            The internal counter bypasses the JC pins and proves the
            acquisition, DDR/DMA, USB, host, server and waveform renderer before
            any target is attached.
          </p>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>Capture panel settings</caption>
              <thead>
                <tr>
                  <th scope="col">Control</th>
                  <th scope="col">Set to</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Rate</td>
                  <td className="num">100 MS/s</td>
                </tr>
                <tr>
                  <td>Words</td>
                  <td className="num">4096</td>
                </tr>
                <tr>
                  <td>Mode</td>
                  <td>raw (4 samples/word)</td>
                </tr>
                <tr>
                  <td>Pre</td>
                  <td className="num">0</td>
                </tr>
                <tr>
                  <td>Trig rise / fall</td>
                  <td>blank</td>
                </tr>
                <tr>
                  <td>Patgen</td>
                  <td>counter</td>
                </tr>
                <tr>
                  <td>Timeout</td>
                  <td className="num">30</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p className="doc-prose">
            Click <strong>Capture</strong>, then require every pass sign:
          </p>
          <div className="doc-grid cols-2">
            <article className="doc-panel">
              <h3>Data result</h3>
              <ul>
                <li>
                  Status says <Inline>captured 4096 samples (raw)</Inline>.
                </li>
                <li>Viewer title reports 4096 samples at 100,000,000 Hz.</li>
                <li>All eight channel rows are visible.</li>
                <li>CH0 toggles fastest; each higher counter bit is slower.</li>
              </ul>
            </article>
            <article className="doc-panel">
              <h3>Health result</h3>
              <ul>
                <li>The session returns to idle.</li>
                <li>No red capture-failure toast appears.</li>
                <li>
                  Completion means exact size and whole-capture CRC passed.
                </li>
                <li>Zoom, pan, Fit, cursors and edge stepping work.</li>
              </ul>
            </article>
          </div>
          <p className="doc-scope">
            If this fails, keep the target disconnected. Fix the base path
            before debugging external signals.
          </p>
        </section>

        <section className="doc-section" id="protocol-tests">
          <h2>6. Try capture decode and live decode</h2>
          <p className="doc-prose">
            For each capture recipe use 10 MS/s, 64k words, raw mode, zero
            pre-trigger and no trigger. Select one Patgen mode, capture, enter
            the Slot 0 decoder text, then click <strong>Decode capture</strong>.
          </p>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>Internal protocol recipes</caption>
              <thead>
                <tr>
                  <th scope="col">Try</th>
                  <th scope="col">Patgen</th>
                  <th scope="col">Slot 0</th>
                  <th scope="col">Expect</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>UART</td>
                  <td>
                    <code>uart</code>
                  </td>
                  <td>
                    <code>uart:rx=0,baud=115200</code>
                  </td>
                  <td>Repeating LA-TEST newline bytes</td>
                </tr>
                <tr>
                  <td>SPI</td>
                  <td>
                    <code>spi</code>
                  </td>
                  <td>
                    <code>spi:sck=0,mosi=1,cs=2</code>
                  </td>
                  <td>Repeating 4C 41 frames; first may be partial</td>
                </tr>
                <tr>
                  <td>I2C</td>
                  <td>
                    <code>i2c</code>
                  </td>
                  <td>
                    <code>i2c:scl=3,sda=4</code>
                  </td>
                  <td>START, address, ACK, 4C 41, STOP</td>
                </tr>
              </tbody>
            </table>
          </div>
          <div className="doc-grid cols-2">
            <article className="doc-panel">
              <h3>Inspect a saved capture</h3>
              <ol>
                <li>
                  Click <strong>Decode capture</strong>.
                </li>
                <li>Require a nonzero event count.</li>
                <li>Zoom into the annotation row.</li>
                <li>Compare event bytes with the recipe.</li>
              </ol>
            </article>
            <article className="doc-panel">
              <h3>Run UART live</h3>
              <ol>
                <li>Enter the UART Slot 0 text.</li>
                <li>
                  Click <strong>Apply live config</strong>.
                </li>
                <li>
                  Select UART under Pattern generator and click{' '}
                  <strong>Arm</strong>.
                </li>
                <li>
                  Click Slot 0 <strong>Start</strong>; require events up and the{' '}
                  <strong>client</strong> drop count to remain zero.
                </li>
                <li>
                  Click Slot 0 <strong>Stop</strong>; require session idle.
                </li>
              </ol>
            </article>
          </div>
          <p className="doc-scope">
            Burst capture and live decode are alternate uses of the acquisition
            engine. Starting a capture stops live automatically; finish or abort
            a capture before starting live. Host capture decode needs the native
            DLL; device-side live decode does not. The bounded replay ring may
            discard old history after it fills; a rising ring count alone does
            not mean the active browser lost events.
          </p>
        </section>

        <section className="doc-section" id="real-target">
          <h2>7. Capture a real target</h2>
          <div className="doc-grid cols-2">
            <article className="doc-panel">
              <h3>Wire with power removed</h3>
              <ol>
                <li>Stop live mode and Disconnect.</li>
                <li>Remove target power.</li>
                <li>Connect target ground to JC-5 or JC-11 first.</li>
                <li>Connect only 3.3 V signal lines to JC.</li>
                <li>Reconnect the UI, then power the target.</li>
              </ol>
            </article>
            <article className="doc-panel">
              <h3>JC channel order</h3>
              <dl className="doc-kv">
                <dt>CH0..CH3</dt>
                <dd>JC-1, JC-2, JC-3, JC-4</dd>
                <dt>CH4..CH7</dt>
                <dd>JC-7, JC-8, JC-9, JC-10</dd>
                <dt>Ground</dt>
                <dd>JC-5 or JC-11</dd>
                <dt>Target VCC</dt>
                <dd>Do not connect it to the analyzer</dd>
              </dl>
            </article>
          </div>
          <div className="doc-prose">
            <ol>
              <li>
                Set Capture <strong>Patgen = off</strong>, or the real JC pins
                are ignored.
              </li>
              <li>
                Start with 10 MS/s, 64k, raw mode, Pre 0 and blank triggers.
              </li>
              <li>
                For a triggered capture, put one channel number 0..7 in Trig
                rise or Trig fall.
              </li>
              <li>Use Pre to preserve up to 32,768 samples before the edge.</li>
              <li>
                Capture and check exact sample count, idle return and warnings.
              </li>
              <li>
                Enter the target&apos;s real decoder mapping and click Decode
                capture.
              </li>
            </ol>
          </div>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>
                Decoder examples - change channels and options to match the
                target
              </caption>
              <thead>
                <tr>
                  <th scope="col">Bus</th>
                  <th scope="col">Decoder text</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>UART</td>
                  <td>
                    <code>uart:rx=0,baud=115200</code>
                  </td>
                </tr>
                <tr>
                  <td>SPI</td>
                  <td>
                    <code>
                      spi:sck=0,mosi=1,miso=2,cs=3,cpol=0,cpha=0,word=8,lsb=0
                    </code>
                  </td>
                </tr>
                <tr>
                  <td>I2C</td>
                  <td>
                    <code>i2c:scl=0,sda=1</code>
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
        </section>

        <section className="doc-section" id="export-shutdown">
          <h2>8. Export, reopen and shut down</h2>
          <div className="doc-grid cols-2">
            <article className="doc-panel">
              <h3>Save the result</h3>
              <ul>
                <li>
                  <strong>.sr:</strong> exact-count archive for PulseView.
                </li>
                <li>
                  <strong>.vcd:</strong> transition waveform; trailing constant
                  time is not recoverable.
                </li>
                <li>
                  <strong>.png:</strong> the visible canvas.
                </li>
                <li>
                  <strong>Events CSV:</strong> decoded annotations.
                </li>
                <li>
                  <strong>Samples CSV:</strong> expanded sample values.
                </li>
                <li>
                  <strong>Open:</strong> reopen .sr/.vcd/sample CSV as signal
                  captures without a device; events CSV contains annotations
                  only.
                </li>
              </ul>
            </article>
            <article className="doc-panel">
              <h3>End cleanly</h3>
              <ol>
                <li>Stop all live slots and wait for session idle.</li>
                <li>Export anything needed beyond this in-memory session.</li>
                <li>
                  Click <strong>Disconnect</strong>; decoder slots are cleared.
                </li>
                <li>Press Ctrl+C in PowerShell to stop la-ui.</li>
                <li>Remove target power, then signals, then common ground.</li>
                <li>Remove Zybo power; reload with JTAG next time.</li>
              </ol>
            </article>
          </div>
        </section>

        <section className="doc-section" id="troubleshooting">
          <h2>9. Troubleshooting</h2>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>Start with the earliest failed step</caption>
              <thead>
                <tr>
                  <th scope="col">Symptom</th>
                  <th scope="col">Check</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>la-ui.exe is missing</td>
                  <td>
                    Install the project into .venv using the tooling guide.
                  </td>
                </tr>
                <tr>
                  <td>Port 8080 is busy</td>
                  <td>
                    Stop another la-ui/la-web process, or start with{' '}
                    <code>--port N</code>.
                  </td>
                </tr>
                <tr>
                  <td>No 1209:0001 COM port</td>
                  <td>
                    Check J12 power, JP6=USB, jtag-zynq PASS, J10 cable and
                    direct PC port; reconnect J10.
                  </td>
                </tr>
                <tr>
                  <td>Only FTDI ports appear</td>
                  <td>
                    Those are J12 debug interfaces, not the J10 analyzer
                    endpoint.
                  </td>
                </tr>
                <tr>
                  <td>COM port is busy</td>
                  <td>
                    Stop every UI, CLI, terminal or monitor process that owns
                    it.
                  </td>
                </tr>
                <tr>
                  <td>Identity differs</td>
                  <td>
                    Reload the current default image with{' '}
                    <code>python tools/run.py jtag-zynq</code>.
                  </td>
                </tr>
                <tr>
                  <td>Counter capture is flat</td>
                  <td>
                    Use Patgen counter, blank triggers and the current
                    firmware/PL IDs.
                  </td>
                </tr>
                <tr>
                  <td>Real capture shows test data</td>
                  <td>Set Capture Patgen to off.</td>
                </tr>
                <tr>
                  <td>Capture stays armed</td>
                  <td>Abort; the selected trigger edge did not occur.</td>
                </tr>
                <tr>
                  <td>Decode library missing</td>
                  <td>
                    Run <code>python tools/run.py test-native</code>, restart
                    la-ui and reconnect.
                  </td>
                </tr>
                <tr>
                  <td>Wrong decoded events</td>
                  <td>
                    Check channel mapping, sample rate, baud, SPI mode, word
                    size and idle voltage.
                  </td>
                </tr>
                <tr>
                  <td>Live client drops or overrun</td>
                  <td>
                    Lower the event rate or active slot count. A rising ring
                    count alone is normal replay-history rollover.
                  </td>
                </tr>
                <tr>
                  <td>UI says server error</td>
                  <td>
                    Confirm la-ui is still running in PowerShell. Restart it if
                    needed, then reload the browser and reconnect.
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
        </section>

        <section className="doc-section">
          <h2>CLI and optional gateway paths</h2>
          <p className="doc-prose">
            For automation, stop the UI and use the same J10 port explicitly:
          </p>
          <Cmd>{`.\\.venv\\Scripts\\la-cli.exe ping -t serial://COM13
.\\.venv\\Scripts\\la-cli.exe status -t serial://COM13
.\\.venv\\Scripts\\la-cli.exe capture -t serial://COM13 --rate 10M --words 4096 --mode raw --patgen counter --out selftest.sr`}</Cmd>
          <p className="doc-prose">
            Replace COM13 with the identified port. The optional ESP32-S3
            gateway uses the separate <Inline>--profile gateway</Inline> image,
            JD wiring and <Inline>tcp://la-esp32.local:5555</Inline> with la-cli
            or la-web. The supported unified-UI path is the default direct-USB{' '}
            <Inline>serial://COMx</Inline> path; use la-web for the gateway.
          </p>
          <p className="doc-scope">
            On the recorded ESP32-S3 bench setup, keep both of the
            gateway&apos;s USB cables connected; one cable alone can brown out
            under Wi-Fi transmit peaks.
          </p>
        </section>

        <section className="doc-section">
          <h2>Current boundaries</h2>
          <ul className="doc-prose">
            <li>
              Direct Zynq USB measured 1.62 MB/s over three byte-exact 64 MiB
              trials.
            </li>
            <li>
              The MVP is qualified at 100 MS/s; 200 MS/s is not implemented or
              proven.
            </li>
            <li>Pre-trigger depth is limited to 32,768 samples.</li>
            <li>
              The 1209:0001 development identity is not a final release USB
              identity.
            </li>
            <li>The physical SD boot and final release tag remain open.</li>
            <li>
              Automated UI browser and bench gates pass; the human visual
              click-through remains open.
            </li>
          </ul>
          <p className="doc-scope">
            Canonical source: <Inline>docs/using-the-analyzer.md</Inline>. See{' '}
            <a className="inline-source" href="/roadmap">
              roadmap
            </a>{' '}
            for evidence and open gates,{' '}
            <a className="inline-source" href="/tooling">
              tooling
            </a>{' '}
            for environment setup, and{' '}
            <a className="inline-source" href="/reference/wiring">
              wiring
            </a>{' '}
            for the electrical map.
          </p>
        </section>
      </div>
    </main>
  );
}
