import Link from 'next/link';
import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { StatusBadge } from '@/components/docs/status-badge';

export const metadata = {
  title: 'Using the analyzer — RTOS Logic Analyzer',
};

function Cmd({ children }: { children: string }) {
  return <pre className="doc-pre">{children}</pre>;
}

export default function UsingPage() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Using the analyzer' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Guide · Operating the analyzer</p>
          <h1>Using the analyzer</h1>
          <p className="doc-answer">
            This guide covers the bench workflow: wire the boards, load the firmware, verify the
            link, capture signals, decode traffic live, and open the results. Development setup
            (toolchain, venv, ESP-IDF, Vivado/Vitis) is in{' '}
            <Link className="inline-source" href="/tooling">tooling</Link>; the electrical contract is in{' '}
            <Link className="inline-source" href="/reference/wiring">wiring</Link>.
          </p>
          <div className="doc-status-row">
            <StatusBadge status="bench-verified" label="Burst capture and live decode verified on hardware" />
            <StatusBadge status="blocked" label="Large-capture drain limited by the ESP32 Wi-Fi relay" />
          </div>
        </header>

        <section className="doc-section">
          <h2>1. What you need</h2>
          <ul className="doc-prose">
            <li>Zybo Z7-10 with the project bitstream and firmware (JTAG development load or SD release boot).</li>
            <li>ESP32 DOIT DevKit V1 gateway flashed with the project firmware, on the same <strong>2.4 GHz</strong> Wi-Fi network as the PC (the classic ESP32 has no 5 GHz radio).</li>
            <li>PC with the project <code className="doc-inline-code">.venv</code> and PulseView (or any VCD viewer).</li>
            <li>Wiring: signals to <strong>JC</strong> (3.3 V only), UART ribbon <strong>JD-1..4 + GND</strong>, optional SPI jumpers <strong>JD-7..10 + JA-1</strong>, optional <strong>JE→JC</strong> loopback for self-test.</li>
            <li>[HUMAN] checks before power: common ground, RTS/CTS direction, no 5 V exposure, correct JP5 position.</li>
          </ul>
        </section>

        <section className="doc-section">
          <h2>2. Build and load</h2>
          <Cmd>{`python tools\\run.py bit          # bitstream + XSA (Vivado)
python tools\\run.py fw-zynq      # Zynq FreeRTOS application (Vitis)
python tools\\run.py fw-esp32     # ESP32 gateway firmware (ESP-IDF)`}</Cmd>
          <p className="doc-prose">
            <strong>Zynq, development (JP5 = JTAG):</strong>
          </p>
          <Cmd>{`python tools\\run.py jtag-zynq`}</Cmd>
          <p className="doc-prose">
            Programs the PL, runs the FSBL, downloads <code className="doc-inline-code">la_app.elf</code>{' '}
            and starts CPU0. The FTDI COM port shows the boot banner and periodic{' '}
            <code className="doc-inline-code">LA status:</code> lines.
          </p>
          <p className="doc-prose">
            <strong>Zynq, release (JP5 = SD):</strong> build <code className="doc-inline-code">BOOT.BIN</code>{' '}
            with <code className="doc-inline-code">python tools\run.py boot</code>, copy it to a
            FAT-formatted SD card, power-cycle with JP5 = SD.
          </p>
          <p className="doc-prose">
            <strong>ESP32 gateway:</strong> set the Wi-Fi credentials once per network with{' '}
            <code className="doc-inline-code">.venv\Scripts\python.exe tools\set_wifi.py</code> (the
            helper prompts and never commits them), rebuild with{' '}
            <code className="doc-inline-code">python tools\run.py fw-esp32</code>, then flash. This
            board&apos;s auto-reset does not assert GPIO0: hold BOOT, tap EN, release BOOT, then use{' '}
            <code className="doc-inline-code">esptool --before no_reset</code> and watch the console.
            Success signs: <code className="doc-inline-code">station started</code>,{' '}
            <code className="doc-inline-code">got ip</code>,{' '}
            <code className="doc-inline-code">net CONNECTED</code>,{' '}
            <code className="doc-inline-code">fifo_ovf=0</code>.
          </p>
        </section>

        <section className="doc-section">
          <h2>3. Verify the link</h2>
          <Cmd>{`.venv\\Scripts\\la-cli.exe ping   -t tcp://la-esp32.local:5555
.venv\\Scripts\\la-cli.exe status -t tcp://la-esp32.local:5555`}</Cmd>
          <p className="doc-prose">
            Expected: <code className="doc-inline-code">pong: fw ... pl 0x4C410002 uptime ...</code> and a
            STATUS snapshot with <code className="doc-inline-code">state idle</code>. If the ping times
            out right after a flash or reset, wait ~15 s and retry once; the first TCP session can need
            a listener refresh. <code className="doc-inline-code">la-cli ports</code> lists USB-UART
            candidates when you use a direct <code className="doc-inline-code">serial://COMx</code> link
            instead of the gateway.
          </p>
        </section>

        <section className="doc-section">
          <h2>Two independent modes</h2>
          <div className="doc-panel">
            <p>
              Burst capture and live decode are alternatives, not steps. <code className="doc-inline-code">capture</code>{' '}
              configures, arms, drains and stops its own session; you can use it without ever running{' '}
              <code className="doc-inline-code">live</code>. <code className="doc-inline-code">live</code> configures the
              stream, starts it and stops on its own (<code className="doc-inline-code">--count</code>,{' '}
              <code className="doc-inline-code">--timeout</code> or Ctrl+C); it does <strong>not</strong> need a
              preceding capture or ARM.
            </p>
            <p>
              They cannot run at the same time: both use the same acquisition engine and memory pool. Finish or
              abort one before starting the other; if a capture is armed, run{' '}
              <code className="doc-inline-code">la-cli abort -t ...</code> first. Every command latches its own
              configuration at CONFIG/ARM or DECODER_CFG/LIVE_START, so there is no &quot;assert once, then
              use&quot; step. With <code className="doc-inline-code">--patgen</code>, the CLI clears the
              pattern-generator enable before setting the new mode, so repeated self-test sessions relatch
              instead of keeping the previous pattern.
            </p>
          </div>
        </section>

        <section className="doc-section">
          <h2>4. First capture without external signals</h2>
          <p className="doc-prose">
            The pattern generator can drive the capture input internally, so you can prove the whole
            path before connecting anything:
          </p>
          <Cmd>{`.venv\\Scripts\\la-cli.exe capture -t tcp://la-esp32.local:5555 ^
    --rate 10M --words 4096 --mode raw --patgen counter --out selftest.sr`}</Cmd>
          <p className="doc-prose">
            <code className="doc-inline-code">--patgen</code> enables the internal self-test mux and
            latches the pattern generator; <strong>omit it for real signals</strong>, or the JC inputs
            are ignored. Modes: <code className="doc-inline-code">counter</code>,{' '}
            <code className="doc-inline-code">walking</code>, <code className="doc-inline-code">prbs</code>,{' '}
            <code className="doc-inline-code">uart</code>, <code className="doc-inline-code">spi</code>,{' '}
            <code className="doc-inline-code">i2c</code>. Open the resulting file in PulseView.
          </p>
        </section>

        <section className="doc-section">
          <h2>5. Capturing real signals</h2>
          <p className="doc-prose">
            Connect the signal to a JC channel (JC-1..4, JC-7..10 = CH0..CH7; 3.3 V only), then:
          </p>
          <Cmd>{`# 10 MS/s, 64k post-trigger samples, raw packing, rising edge on CH0
.venv\\Scripts\\la-cli.exe capture -t tcp://la-esp32.local:5555 ^
    --rate 10M --words 64k --mode raw --trig-rise 0 --out capture.sr

# transition encoding with 1000 pre-trigger samples, falling edge on CH2
.venv\\Scripts\\la-cli.exe capture -t tcp://la-esp32.local:5555 ^
    --rate 100M --words 16k --mode txn --pre 1000 --trig-fall 2 --out capture.vcd

# PNG snapshot
.venv\\Scripts\\la-cli.exe capture -t tcp://la-esp32.local:5555 ^
    --rate 1M --words 4096 --out capture.png`}</Cmd>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>capture options</caption>
              <thead>
                <tr>
                  <th scope="col">Option</th>
                  <th scope="col">Meaning</th>
                </tr>
              </thead>
              <tbody>
                <tr><td><code>--rate 1M|10M|100M</code></td><td>Requested sample rate; must map to <code>100 MHz / (div+1)</code>. The tool prints the actual rate when they differ.</td></tr>
                <tr><td><code>--words N</code></td><td>Post-trigger samples (4096, 64k, 1M).</td></tr>
                <tr><td><code>--pre N</code></td><td>Pre-trigger samples (0..32768).</td></tr>
                <tr><td><code>--mode raw|txn</code></td><td>Raw packs four samples per word; txn stores transition records.</td></tr>
                <tr><td><code>--trig-rise CH</code>, <code>--trig-fall CH</code></td><td>Trigger edge on one channel; with no trigger the capture starts immediately.</td></tr>
                <tr><td><code>--patgen MODE</code></td><td>Internal self-test source.</td></tr>
                <tr><td><code>--out FILE.sr|.vcd|.png</code></td><td>Export format follows the extension.</td></tr>
                <tr><td><code>--timeout S</code></td><td>Command deadline (default 30 s).</td></tr>
              </tbody>
            </table>
          </div>
          <p className="doc-scope">
            The host verifies chunk order, the whole-capture CRC32 and the exact sample count before
            writing the file. <code className="doc-inline-code">la-cli abort -t ...</code> cancels an
            armed capture.
          </p>
        </section>

        <section className="doc-section">
          <h2>6. Live decode</h2>
          <p className="doc-prose">
            Live mode streams decoded protocol events instead of raw samples. The decoder spec is{' '}
            <code className="doc-inline-code">type:key=value,...</code>:
          </p>
          <ul className="doc-prose">
            <li><code className="doc-inline-code">uart:rx=CH,baud=BAUD</code></li>
            <li><code className="doc-inline-code">spi:sck=CH[,mosi=CH][,miso=CH][,cs=CH][,cpol=0][,cpha=0][,word=8][,lsb=0][,cs_active=0]</code></li>
            <li><code className="doc-inline-code">i2c:scl=CH,sda=CH</code></li>
          </ul>
          <Cmd>{`# Self-test: patgen UART on CH0, decode it, stop after 20 events
.venv\\Scripts\\la-cli.exe live -t tcp://la-esp32.local:5555 ^
    --patgen uart --decoder uart:rx=0,baud=115200 --rate 100M --count 20

# Real UART traffic, compact encoding, CSV output
.venv\\Scripts\\la-cli.exe live -t tcp://la-esp32.local:5555 ^
    --decoder uart:rx=0,baud=115200 --compact --csv --count 100

# SPI: SCK CH0, MOSI CH1, CS CH2
.venv\\Scripts\\la-cli.exe live -t tcp://la-esp32.local:5555 ^
    --decoder spi:sck=0,mosi=1,cs=2

# Stop when the decoded bytes match "LA" (hex 4c41)
.venv\\Scripts\\la-cli.exe live -t tcp://la-esp32.local:5555 ^
    --decoder uart:rx=0,baud=115200 --stop-on 4c41`}</Cmd>
          <p className="doc-scope">
            Qualified rates: 115200 UART (repeatable 60 s zero-drop gate) and 10 kHz SPI/I²C. 1 MHz SPI
            is a deliberate overload test: the host receives <code className="doc-inline-code">EVT_OVERRUN</code>{' '}
            reports and the system returns to IDLE. Overruns are always reported, never silent.
          </p>
        </section>

        <section className="doc-section">
          <h2>7. Browser live view</h2>
          <p className="doc-prose">
            The gateway serves an embedded page on port 80 while a live session runs. Start a live
            session, then open <code className="doc-inline-code">http://la-esp32.local/</code> (or the
            IP from the ESP32 boot log). The page shows per-decoder tabs with decoded bytes, sample
            deltas and a stats bar, and reconnects automatically. No external assets or internet
            access are needed.
          </p>
        </section>

        <section className="doc-section">
          <h2>8. Task/queue trace</h2>
          <Cmd>{`.venv\\Scripts\\la-cli.exe trace -t tcp://la-esp32.local:5555 --start
# ... run the workload ...
.venv\\Scripts\\la-cli.exe trace -t tcp://la-esp32.local:5555 --dump ^
    --out trace.txt --png trace.png --title "live UART"`}</Cmd>
          <p className="doc-scope">
            The PNG is a relative-time timeline of task switches, queue/stream-buffer activity and app
            markers. The dump is best-effort: a few LOG records can drop under relay pressure.
          </p>
        </section>

        <section className="doc-section">
          <h2>9. SPI bulk transport</h2>
          <p className="doc-prose">
            By default burst <code className="doc-inline-code">DATA_*</code> frames travel over the
            UART. To route them over the SPI slave (JD-7..10 + JA-1 wired), select it once per session:
          </p>
          <Cmd>{`.venv\\Scripts\\la-cli.exe link -t tcp://la-esp32.local:5555 --spi --clock 18
.venv\\Scripts\\la-cli.exe capture -t tcp://la-esp32.local:5555 --rate 10M --words 64k --out big.sr`}</Cmd>
          <p className="doc-scope">
            Commands and ACKs stay on the UART. Framed transfers are bench-clean at 5/10/18 MHz (18 MHz
            is the measured ceiling). The sustained host drain is currently limited to ~0.28 MB/s by
            the ESP32 Wi-Fi relay, so very large captures take a long time even though the SPI wire is
            faster.
          </p>
        </section>

        <section className="doc-section">
          <h2>10. Command reference</h2>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>la-cli commands</caption>
              <thead>
                <tr>
                  <th scope="col">Command</th>
                  <th scope="col">Purpose</th>
                </tr>
              </thead>
              <tbody>
                <tr><td><code>ports</code></td><td>List USB-UART candidates.</td></tr>
                <tr><td><code>ping -t URI</code></td><td>Check the link and firmware identity.</td></tr>
                <tr><td><code>status -t URI</code></td><td>STATUS snapshot: state, counters, watermarks, latency.</td></tr>
                <tr><td><code>abort -t URI</code></td><td>Abort a running capture.</td></tr>
                <tr><td><code>capture -t URI [options]</code></td><td>Arm a burst capture and export .sr/.vcd/.png.</td></tr>
                <tr><td><code>live -t URI [options]</code></td><td>Stream decoded UART/SPI/I²C events.</td></tr>
                <tr><td><code>trace -t URI [--start|--stop|--dump]</code></td><td>Task/queue trace ring.</td></tr>
                <tr><td><code>link -t URI --spi|--uart [--clock MHz]</code></td><td>Select the burst transport.</td></tr>
              </tbody>
            </table>
          </div>
          <p className="doc-scope">
            Target URIs: <code className="doc-inline-code">tcp://la-esp32.local:5555</code> (gateway),{' '}
            <code className="doc-inline-code">tcp://&lt;ip&gt;:5555</code>, or{' '}
            <code className="doc-inline-code">serial://COMx</code> (direct 3.3 V USB-UART adapter on
            JD-1..4 + GND, [HUMAN]).
          </p>
        </section>

        <section className="doc-section">
          <h2>11. Troubleshooting</h2>
          <ul className="doc-prose">
            <li><strong>Ping times out after a flash/reset:</strong> wait ~15 s and retry once, then check the ESP32 console for <code className="doc-inline-code">net CONNECTED</code>.</li>
            <li><strong>No PONG at all:</strong> confirm the 2.4 GHz network, the JD-1..4 + GND ribbon, and that the Zynq firmware is running.</li>
            <li><strong>First capture after <code className="doc-inline-code">--patgen</code> looks wrong:</strong> self-test latches; omit <code className="doc-inline-code">--patgen</code> to capture real JC inputs.</li>
            <li><strong>ESP32 will not flash:</strong> manual BOOT/EN sequence plus <code className="doc-inline-code">esptool --before no_reset</code>.</li>
            <li><strong>Live stops with <code className="doc-inline-code">EVT_OVERRUN</code>:</strong> the event rate exceeded the link; lower the input rate, use <code className="doc-inline-code">--compact</code>, or accept the reported loss.</li>
            <li><strong>PulseView decoder-probe warnings:</strong> a bundled PulseView issue; .sr viewing is unaffected.</li>
          </ul>
          <p className="doc-prose">
            Known limits and their evidence are collected in{' '}
            <code className="doc-inline-code">docs/limits.md</code> and on the{' '}
            <Link className="inline-source" href="/roadmap">roadmap</Link> page.
          </p>
        </section>
      </div>
    </main>
  );
}
