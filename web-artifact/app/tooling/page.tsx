import Link from 'next/link';
import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { StatusBadge } from '@/components/docs/status-badge';

export const metadata = {
  title: 'Tooling and release packaging — RTOS Logic Analyzer',
};

const STAGES = [
  ['gen', 'Regenerate the C, SystemVerilog, Python and Markdown register views from docs/regmap.yaml and verify idempotence.'],
  ['lint / sim', 'Compile and run the 12 self-checking Questa RTL testcases. Run them sequentially: they share the ignored build/questa workspace.'],
  ['test-native / test-esp32-mocks / test-host', 'Native CTest suites, ESP32 component mocks and host pytest against the mock analyzer.'],
  ['bit', 'Vivado 2026.1 batch build of the block design, bitstream and XSA.'],
  ['fw-zynq', 'Vitis 2026.1 platform and FreeRTOS application build from the XSA.'],
  ['fw-esp32', 'Native ESP-IDF build of the gateway firmware (UART/SPI relay plus web view).'],
  ['jtag-zynq', 'XSDB load of the bitstream, FSBL and application over JTAG.'],
  ['boot', 'Stage the FSBL, bitstream and la_fw.elf and run bootgen to produce BOOT.BIN.'],
  ['package', 'Stage the ESP32 .bin set and build the host wheel; write build/release/release-manifest.json with sizes and SHA-256 hashes.'],
  ['test-e2e', 'Bench pytest suites: burst matrices, live gates, SPI link and web view, against the gateway target URI.'],
  ['release', 'Native release precheck: register generation plus the full native CTest suite.'],
];

export default function ToolingPage() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Tooling' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Tooling · Native Windows workflow</p>
          <h1>Tooling and release packaging</h1>
          <p className="doc-answer">
            Every build and test runs through one entry point: <code className="doc-inline-code">python tools/run.py &lt;target&gt;</code>.
            The runner resolves the project root from its own location, prepares child-process
            environments and writes a machine-readable report under ignored
            {' '}
            <code className="doc-inline-code">build/reports</code>.
          </p>
          <div className="doc-status-row">
            <StatusBadge status="host-tested" label="Local gates pass" />
            <StatusBadge status="blocked" label="v1.0 waits on the ESP32-S3 transport port" />
          </div>
        </header>

        <section className="doc-section">
          <h2>Rules that never change</h2>
          <ul className="doc-prose">
            <li>Native Windows only: no WSL, Linux containers, Make, Verilator or ESP-IDF Linux-host workflow.</li>
            <li>All project-owned sources, environments, workspaces, captures and release artifacts stay beneath the fixed project root.</li>
            <li>Register views are generated: edit <code className="doc-inline-code">docs/regmap.yaml</code>, then run <code className="doc-inline-code">gen</code>; never hand-edit the generated files.</li>
            <li>Missing hardware or licensed tools are recorded as blocked or unverified, never as passing.</li>
            <li>Generated environments, reports, logs, captures and build outputs are ignored by Git; a missing report on another checkout means not-run.</li>
          </ul>
        </section>

        <section className="doc-section">
          <h2>Build and test stages</h2>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>Canonical targets and what they do</caption>
              <thead>
                <tr>
                  <th scope="col">Target</th>
                  <th scope="col">Purpose</th>
                </tr>
              </thead>
              <tbody>
                {STAGES.map(([target, purpose]) => (
                  <tr key={target}>
                    <td><code>{target}</code></td>
                    <td>{purpose}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <p className="doc-scope">
            The full per-target validation boundary and current evidence are on the{' '}
            <Link className="inline-source" href="/verification">verification</Link> page.
          </p>
        </section>

        <section className="doc-section">
          <h2>Release packaging (P10.4)</h2>
          <p className="doc-prose">
            <code className="doc-inline-code">python tools/run.py boot</code> stages the FSBL, the
            bitstream and <code className="doc-inline-code">la_fw.elf</code>, writes the bootgen image
            file and runs bootgen natively; it verifies the image size and the Zynq sync header.
            {' '}
            <code className="doc-inline-code">python tools/run.py package</code> then stages the ESP32
            flash set (bootloader, partition table and gateway application), builds the host Python wheel
            and writes <code className="doc-inline-code">build/release/release-manifest.json</code> with
            sizes and SHA-256 hashes.
          </p>
          <p className="doc-prose">
            The final BOOT.BIN, the [HUMAN] SD-card boot with JP5=SD and the <code className="doc-inline-code">v1.0</code>
            {' '}
            tag are deliberately waiting for the ESP32-S3 USB-OTG transport port: P9.1 framed transfers are
            bench-clean, but the sustained drain gate is blocked on the current Wi-Fi relay. Release
            artifacts are tooling evidence, not a finished product claim.
          </p>
          <p className="doc-prose">
            Wi-Fi credentials are compiled into the ESP32 image from the ignored
            {' '}
            <code className="doc-inline-code">build/esp32/sdkconfig</code>; a release image must be built
            with the user&apos;s own credentials, and the tracked defaults contain none.
          </p>
        </section>

        <section className="doc-section">
          <h2>Environment</h2>
          <p className="doc-prose">
            The pinned versions live in <code className="doc-inline-code">tools/toolchain-lock.json</code>
            {' '}
            (Python 3.11, MSVC 19.44, CMake 4.4, Ninja 1.13, Questa 2025.2 FPGA Starter, Vivado/Vitis/
            Bootgen 2026.1, ESP-IDF v5.5.2, Arm GCC 12.2.1, PulseView 0.4.2) and the board part in
            {' '}
            <code className="doc-inline-code">tools/board-lock.json</code>.
            {' '}
            <Link className="inline-source" href="/reference/wiring">Wiring</Link>
            {' '}
            and troubleshooting notes are in <code className="doc-inline-code">docs/windows-dev.md</code>.
          </p>
        </section>
      </div>
    </main>
  );
}
