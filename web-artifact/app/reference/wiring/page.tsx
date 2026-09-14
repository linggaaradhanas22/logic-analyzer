import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { StatusBadge } from '@/components/docs/status-badge';

export const metadata = {
  title: 'Wiring reference — RTOS Logic Analyzer',
};

const JC = [
  ['JC-1', 'CH0'], ['JC-2', 'CH1'], ['JC-3', 'CH2'], ['JC-4', 'CH3'],
  ['JC-7', 'CH4'], ['JC-8', 'CH5'], ['JC-9', 'CH6'], ['JC-10', 'CH7'],
];
const JE = [
  ['JE-1', 'PATGEN0'], ['JE-2', 'PATGEN1'], ['JE-3', 'PATGEN2'], ['JE-4', 'PATGEN3'],
  ['JE-7', 'PATGEN4'], ['JE-8', 'PATGEN5'], ['JE-9', 'PATGEN6'], ['JE-10', 'PATGEN7'],
];
const JD = [
  ['JD-1', 'Zynq UART0 TX', 'ESP32 GPIO16 RX'],
  ['JD-2', 'Zynq UART0 RX', 'ESP32 GPIO17 TX'],
  ['JD-3', 'Zynq UART0 RTS', 'ESP32 GPIO21 CTS'],
  ['JD-4', 'Zynq UART0 CTS', 'ESP32 GPIO22 RTS'],
  ['JD-5/11', 'signal ground', 'ESP32 GND'],
];
const SPI = [
  ['JD-7', 'SPI SCLK (slave input)', 'ESP32 GPIO18'],
  ['JD-8', 'SPI SS_N (slave input, active low)', 'ESP32 GPIO4'],
  ['JD-9', 'SPI MOSI (slave input)', 'ESP32 GPIO23'],
  ['JD-10', 'SPI MISO (slave output)', 'ESP32 GPIO19'],
  ['JA-1', 'SPI data-ready (slave output)', 'ESP32 GPIO27'],
  ['JD-5/11, JA-5/11', 'signal ground', 'ESP32 GND'],
];

function PinTable({ caption, rows, headers }: { caption: string; rows: string[][]; headers: string[] }) {
  return (
    <div className="doc-table-wrap">
      <table className="doc-table">
        <caption>{caption}</caption>
        <thead>
          <tr>
            {headers.map((header) => (
              <th scope="col" key={header}>{header}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {rows.map((row) => (
            <tr key={row.join('-')}>
              {row.map((cell, index) => (
                <td className={index === 0 ? 'num' : undefined} key={cell}>{cell}</td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export default function WiringPage() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Reference' }, { label: 'Wiring' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Reference · Physical map</p>
          <h1>Wiring reference</h1>
          <p className="doc-answer">
            All signals are 3.3 V LVCMOS33. The Zybo JC header carries the eight acquisition channels,
            JE carries the pattern outputs, and JD carries both the UART command link and the SPI bulk
            link to the ESP32.
          </p>
          <div className="doc-status-row">
            <StatusBadge status="bench-verified" label="JE→JC loopback 18/18 on all eight channels" />
            <StatusBadge status="implemented-unverified" label="Arbitrary external sources open" />
            <span className="doc-scope">
              Continuity, common ground, RTS/CTS direction and no-5 V exposure are [HUMAN] checks before
              programming or HIL runs.
            </span>
          </div>
        </header>

        <section className="doc-section">
          <h2>Acquisition inputs (JC)</h2>
          <PinTable caption="JC channel map" headers={['Pin', 'Signal']} rows={JC} />
          <p className="doc-scope">
            LVCMOS33 inputs. The JC pairs are impedance-matched differential pairs used single-ended
            here; keep wires short and expect crosstalk at fast edges.
          </p>
        </section>

        <section className="doc-section">
          <h2>Pattern outputs (JE)</h2>
          <PinTable caption="JE pattern map" headers={['Pin', 'Signal']} rows={JE} />
          <p className="doc-scope">
            The same channel order as JC. JE-to-JC jumpers are the internal/external self-test loopback;
            with all eight installed the matrix runs at the default channel mask 0xFF.
          </p>
        </section>

        <section className="doc-section">
          <h2>Zynq to ESP32 UART (JD-1..4)</h2>
          <PinTable caption="UART link, 2 Mbps 8N1 with RTS/CTS" headers={['JD pin', 'Zynq UART0 EMIO', 'ESP32']} rows={JD} />
          <p className="doc-scope">
            Direction is named from the ESP32 perspective. GPIO16/17 are the UART2 default pins; GPIO21/22
            replace the default RTS/CTS (8/7) to stay on one side. The ESP32 is powered from its own USB;
            never connect the JD power pins.
          </p>
        </section>

        <section className="doc-section">
          <h2>P9.1 SPI bulk link (JD-7..10, JA-1)</h2>
          <PinTable caption="SPI link, Zynq slave / ESP32 master" headers={['Zybo pin', 'Signal', 'ESP32']} rows={SPI} />
          <p className="doc-scope">
            Bench-measured: framed captures are bit-exact at 5/10/18 MHz (20/20 at 18 MHz); 19/20 MHz lose
            chunks to slave FIFO underruns, so 18 MHz is the current ceiling. The 200 Ω series-resistor
            SCK/MISO edge characterization with a scope is still open. The UART keeps commands/ACKs.
          </p>
        </section>

        <section className="doc-section">
          <h2>ESP32-only loopback (no Zynq)</h2>
          <p className="doc-prose">
            To run the gateway loopback gate, remove the JD ribbon so the Zynq TX and the ESP32 TX cannot
            contend, then jumper GPIO17 (TX) to GPIO16 (RX) and GPIO22 (RTS) to GPIO21 (CTS). Run
            {' '}
            <code className="doc-inline-code">test-e2e --loopback</code>
            {' '}
            with the ESP32 console port; without the flag the gate is skipped so the Zynq E2E run stays
            safe.
          </p>
        </section>

        <section className="doc-section">
          <h2>Power and operating rules</h2>
          <ul className="doc-prose">
            <li>Keep the ESP32 on its own USB or an approved 3.3 V supply and connect signal ground.</li>
            <li>Do not power an ESP32 GPIO from a 5 V rail, and never connect JC/JE to 5 V without a level shifter.</li>
            <li>Keep acquisition leads short; inspect idle levels before enabling a pattern output.</li>
            <li>JP5 is JTAG for development and SD for release; power-cycle after changing it.</li>
            <li>[HUMAN] Confirm the board revision, header pin numbering and ground pin from the Zybo schematic.</li>
            <li>[HUMAN] Verify continuity, common ground, RTS/CTS direction, 3.3 V levels and no 5 V exposure before program, flash or HIL.</li>
          </ul>
        </section>
      </div>
    </main>
  );
}
