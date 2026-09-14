/* oxlint-disable next/no-html-link-for-pages -- Vinext client Link navigation fails in production. */
import { Check, Gauge, HardDrive, Radio } from 'lucide-react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { SystemDiagram } from '@/components/docs/system-diagram';
import { StatusBadge } from '@/components/docs/status-badge';
import { SUBSYSTEMS } from '@/lib/docs/subsystems';
import { ROADMAP } from '@/lib/docs/roadmap';

const EVIDENCE = [
  { value: '12/12', label: 'RTL simulation tests', note: 'Self-checking Questa testcases; simulation is not timing closure.' },
  { value: '18/18', label: 'Burst matrix configurations', note: 'Six patgen modes at 1/10/100 MS/s, internal and JE→JC loopback, three runs each.' },
  { value: '55', label: 'Live E2E gates passed', note: 'Zero-drop 60 s UART run, 10 kHz SPI/I²C, fault injection and decoded-pattern stop.' },
  { value: '500', label: 'Back-to-back captures soaked', note: '4096-sample counter captures with exact data and no overflow.' },
];

const OPEN_WORK = [
  'P9.1 sustained drain: the SPI wire delivers 1.25–2.25 MB/s, but the ESP32 Wi-Fi relay caps the host drain at ~0.28 MB/s, so the 64 MB / 1.5 MB/s gate waits for the ESP32-S3 USB-OTG transport port.',
  'P9.3 200 MS/s stretch is not started; the released MVP stays at 100 MS/s unless IDDR/CDC, bank voltage and signal integrity are proven.',
  'P9.4 Wi-Fi tuning remains open: sparse-reply jitter can push the isolated-byte latency gate past 25 ms under weak RSSI.',
  'The external-source rate/stimulus matrix and a scope check of the 200 Ω SPI edges are still open; JE→JC loopback is real pads but not an arbitrary signal source.',
  'P10.2 sigrok/SUMP bridge is deferred: no signed virtual-COM pair driver is installed, and `.sr`/VCD export remains the required integration.',
  'P10.4 release tooling exists, but the final BOOT.BIN, the [HUMAN] SD boot and the v1.0 tag wait for the ESP32-S3 transport port.',
];

export default function Home() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <header className="doc-header">
          <p className="doc-eyebrow">System overview</p>
          <h1>Capture digital signals. Understand what happened.</h1>
          <p className="doc-answer">
            A logic analyzer records when digital signals are high or low so you can debug timing and
            communication. This project builds one from a Zybo Z7-10 (Zynq-7010), an ESP32 Wi-Fi gateway
            and a native-Windows Python host tool: eight 3.3 V channels are sampled in the FPGA, captured
            around a trigger into DDR through ordinary AXI DMA descriptors, relayed over a CRC-protected
            link, and exported or live-decoded by the host.
          </p>
          <p className="doc-why">
            The problem it solves is the mismatch between fast capture and slow transport: the FPGA can
            produce 100 MB/s of samples, while a UART carries 0.2 MB/s and the current Wi-Fi relay about
            0.28 MB/s. The design makes every buffer owner, queue policy and loss report explicit, so
            overload is visible instead of silently corrupting data.
          </p>
          <div className="doc-status-row">
            <StatusBadge status="bench-verified" label="Burst + live paths bench-verified" />
            <StatusBadge status="blocked" label="P9.1 drain gate blocked on the S3 transport port" />
            <span className="doc-scope">
              Reviewed 14 Sep 2026 against the execution ledger; the newest bench evidence is 14 Sep 2026.
            </span>
          </div>
        </header>

        <section className="doc-section" id="subsystems">
          <h2>Level 1 — how the whole system moves data</h2>
          <SystemDiagram />
        </section>

        <section className="doc-section">
          <h2>Two ways to observe</h2>
          <Tabs defaultValue="burst" className="doc-tabs">
            <TabsList className="doc-tablist" aria-label="Capture mode">
              <TabsTrigger className="doc-tab" value="burst">
                <HardDrive aria-hidden="true" /> Burst capture
              </TabsTrigger>
              <TabsTrigger className="doc-tab" value="live">
                <Radio aria-hidden="true" /> Live decode
              </TabsTrigger>
            </TabsList>
            <TabsContent className="doc-tabpanel" value="burst">
              <p className="doc-eyebrow">Working on the bench through the ESP32 gateway</p>
              <h3>Capture around an exact trigger</h3>
              <p>
                Configure and arm the capture. The FPGA retains up to 32,768 pre-trigger samples, records
                the requested post-trigger samples, and closes the packet with a counted final word. The
                DMA writes the packet into a bounded DDR buffer; the firmware drains the recording to the
                host, which verifies CRC32 and reconstructs raw or transition-encoded samples.
              </p>
              <p className="doc-scope">
                Raw mode stores four 8-bit samples per word. Transition mode stores
                {' '}
                <code className="doc-inline-code">{'{run[23:0], sample[7:0]}'}</code>
                {' '}
                records, so quiet signals shrink and rapidly changing signals expand; worst case is 400 MB/s
                at 100 MS/s.
              </p>
            </TabsContent>
            <TabsContent className="doc-tabpanel" value="live">
              <p className="doc-eyebrow">Bench-verified on the target</p>
              <h3>Decode while traffic is happening</h3>
              <p>
                Completed DMA segments move through a 16-segment pool to one task per protocol. Each task
                emits timestamped UART, SPI or I²C events, which TxTask batches into EVT_DECODED (or the
                compact EVT_BLOCK format) and streams to the host and the ESP32 web view.
              </p>
              <p className="doc-scope">
                Qualified rates: 115200 UART with a repeatable 60 s zero-drop gate, and 10 kHz SPI/I²C.
                1 MHz SPI is a deliberate overload test with reported loss; 25 ms p99 isolated-byte latency
                measured 428 us worst case, with one Wi-Fi-jitter outlier still under P9.4.
              </p>
            </TabsContent>
          </Tabs>
        </section>

        <section className="doc-section">
          <h2>Capture is faster than transport</h2>
          <div className="doc-grid cols-2">
            <div className="doc-panel">
              <h3>Calculated capacity, before protocol overhead</h3>
              <dl className="doc-kv">
                <dt>Raw at 100 MS/s</dt>
                <dd>100 MB/s (100,000,000 samples/s × 1 byte)</dd>
                <dt>Worst-case transitions</dt>
                <dd>400 MB/s (100,000,000 records/s × 4 bytes)</dd>
                <dt>UART at 2 Mbps</dt>
                <dd>~200 kB/s (2,000,000 / 10), before framing and flow control</dd>
                <dt>SPI at 10/18 MHz</dt>
                <dd>~1.25 / 2.25 MB/s measured on the wire</dd>
                <dt>Host drain today</dt>
                <dd>~0.28 MB/s through the ESP32 Wi-Fi relay</dd>
              </dl>
              <p className="doc-scope">
                This is why burst mode stores to DDR, live mode uses bounded packets and the compact event
                format, and every stage reports loss instead of pretending.
              </p>
            </div>
            <div className="doc-panel">
              <h3>Measured gateway loopback (10 Sep 2026)</h3>
              <div className="doc-table-wrap">
                <table className="doc-table">
                  <caption>Round-trip echo, 10.07 MB with zero parser errors and no FIFO overruns</caption>
                  <thead>
                    <tr>
                      <th scope="col">Configured baud</th>
                      <th scope="col">Echo throughput</th>
                    </tr>
                  </thead>
                  <tbody>
                    <tr>
                      <td className="num">921600</td>
                      <td className="num">44.4 kB/s</td>
                    </tr>
                    <tr>
                      <td className="num">2 Mbps</td>
                      <td className="num">39.5 kB/s</td>
                    </tr>
                  </tbody>
                </table>
              </div>
              <p className="doc-scope">
                These are round-trip echo measurements through the relay, not one-way live capacity;
                raising the baud did not improve them, which pointed at relay processing.
              </p>
            </div>
          </div>
        </section>

        <section className="doc-section">
          <h2>Strongest current evidence</h2>
          <div className="proof-grid">
            {EVIDENCE.map((item) => (
              <article key={item.label}>
                <strong>{item.value}</strong>
                <span>{item.label}</span>
                <small>{item.note}</small>
              </article>
            ))}
          </div>
          <p className="doc-scope">
            Also recorded: 83/83 host tests, a 4 MiB SPI capture with every byte verified, the 500-capture
            soak, and the P10.1 web-view bench gate. Local tests and mocks are labeled separately from
            target and bench evidence.
          </p>
        </section>

        <section className="doc-section">
          <h2>Open work and known limits</h2>
          <div className="blocker-note">
            <Gauge aria-hidden="true" />
            <div>
              <strong>The system is honest about what is not finished.</strong>
              <ol>
                {OPEN_WORK.map((item) => (
                  <li key={item}>{item}</li>
                ))}
              </ol>
              <p className="optional-route">
                Full details are in the <a className="inline-source" href="/roadmap">roadmap</a> and
                the <a className="inline-source" href="/verification">verification</a> pages; the
                tracked limits document is <code className="doc-inline-code">docs/limits.md</code>.
              </p>
            </div>
          </div>
        </section>

        <section className="doc-section">
          <h2>Subsystems</h2>
          <div className="doc-index">
            {SUBSYSTEMS.map((subsystem) => (
              <a className="doc-index-card" href={`/subsystems/${subsystem.slug}`} key={subsystem.slug}>
                <span className="order">
                  {String(subsystem.order).padStart(2, '0')} · {subsystem.components.length} components
                </span>
                <strong>{subsystem.name}</strong>
                <p>{subsystem.responsibility}</p>
              </a>
            ))}
          </div>
        </section>

        <section className="doc-section">
          <h2>Roadmap at a glance</h2>
          <div className="milestone-card">
            {ROADMAP.map((phase) => (
              <div className={`milestone milestone-${phase.status === 'bench-verified' || phase.status === 'host-tested' ? 'done' : phase.status === 'blocked' ? 'partial' : ''}`} key={phase.id}>
                <span className="milestone-dot">
                  {phase.status === 'bench-verified' || phase.status === 'host-tested' ? <Check /> : null}
                </span>
                <div>
                  <strong>{phase.id}</strong>
                  <span>{phase.statusNote}</span>
                </div>
                <span className="milestone-state">{phase.status.replace(/-/g, ' ')}</span>
              </div>
            ))}
          </div>
        </section>
      </div>
    </main>
  );
}
