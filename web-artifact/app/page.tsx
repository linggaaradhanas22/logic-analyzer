'use client';

import { useState } from 'react';
import {
  Activity, ArrowDown, Binary, Box, Check, ChevronRight, CircuitBoard,
  Cpu, Gauge, HardDrive, Laptop, Radio, ShieldCheck, Wifi, type LucideIcon,
} from 'lucide-react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';

type Stage = {
  id: string; number: string; short: string; label: string; eyebrow: string;
  description: string; detail: string; stat: string; evidence: string;
  status: 'verified' | 'local' | 'planned'; icon: LucideIcon;
};

const sourceSha = '4d4434f046068c74da552d257b1bd821ef993d62';

const evidenceRevision = {
  reviewedOn: '11 Sep 2026',
  benchOn: '10 Sep 2026',
  sourceSha,
  sourceRevisionUrl: `https://github.com/linggaaradhanas22/logic-analyzer/tree/${sourceSha}`,
  sourceBase: `https://github.com/linggaaradhanas22/logic-analyzer/blob/${sourceSha}`,
};

const sources = {
  status: `${evidenceRevision.sourceBase}/docs/status.md`,
  plan: `${evidenceRevision.sourceBase}/docs/plan.md`,
  regmap: `${evidenceRevision.sourceBase}/docs/regmap.yaml`,
  e2e: `${evidenceRevision.sourceBase}/tests/e2e/test_zynq_burst.py`,
};

const stages: Stage[] = [
  {
    id: 'inputs', number: '01', short: 'Signals', label: '8 digital inputs',
    eyebrow: 'The thing being observed',
    description: 'Eight 3.3 V digital inputs enter through JC. Synchronizers reduce metastability risk when signals enter the FPGA clock domain.',
    detail: 'Supported pulse capture depends on sampling and input validation. External JE→JC loopback is still pending. Use appropriate level shifting for 5 V signals.',
    stat: '8 channels · 3.3 V', evidence: 'External input validation pending', status: 'planned', icon: Activity,
  },
  {
    id: 'fpga', number: '02', short: 'Capture', label: 'FPGA fabric',
    eyebrow: 'The fast, deterministic layer',
    description: 'Hardware samples the inputs, detects the trigger and retains pre-trigger history. Samples are packed raw or encoded as runs of unchanged values.',
    detail: 'Ordinary scatter-gather DMA transfers the stream to DDR without CPU copying every sample. Internal counter captures have passed exact-data bench checks.',
    stat: '100 MS/s design target', evidence: 'Internal capture bench verified', status: 'verified', icon: CircuitBoard,
  },
  {
    id: 'rtos', number: '03', short: 'Coordinate', label: 'Zynq + FreeRTOS',
    eyebrow: 'The real-time control centre',
    description: 'The FPGA and ARM processor share the Zynq chip on the Zybo. Four FreeRTOS application tasks manage acquisition, commands, transmission and status.',
    detail: 'AcqTask controls DMA descriptors. Interrupt handlers acknowledge hardware, notify tasks or move UART bytes through ISR-safe buffers. Live decoder task wiring remains open.',
    stat: '4 application tasks', evidence: 'Burst core bench verified', status: 'verified', icon: Cpu,
  },
  {
    id: 'bridge', number: '04', short: 'Bridge', label: 'ESP32 gateway',
    eyebrow: 'The cable-to-network translator',
    description: 'The ESP32 bridges the flow-controlled UART link to a single Wi-Fi TCP client.',
    detail: 'Gateway burst captures, five target Unity tests and the 10 MB loopback tests have passed. Session recovery after a timeout remains an observed issue.',
    stat: 'UART → Wi-Fi TCP', evidence: 'Gateway bench verified', status: 'verified', icon: Wifi,
  },
  {
    id: 'host', number: '05', short: 'Explore', label: 'Host tools',
    eyebrow: 'Where captured time becomes useful',
    description: 'The Python client configures captures, checks transferred data and exports .sr, VCD or PNG files for analysis.',
    detail: '.sr files are for PulseView; VCD files can be viewed in GTKWave. The live UART command is mock-tested; matching live firmware is not implemented yet.',
    stat: '.sr · .vcd · .png · CSV', evidence: 'Burst used on bench; live mock-tested', status: 'local', icon: Laptop,
  },
];

const milestones = [
  { phase: 'P0–P1', label: 'Local tooling and specifications complete', state: 'done', stateLabel: 'complete' },
  { phase: 'P2', label: 'Capture RTL verified in simulation', state: 'done', stateLabel: 'verified' },
  { phase: 'P3', label: 'Integration and board bring-up verified', state: 'done', stateLabel: 'verified' },
  { phase: 'P4', label: 'Burst core verified on bench', state: 'done', stateLabel: 'verified' },
  { phase: 'P5', label: 'Gateway and target tests verified on bench; recovery issue remains', state: 'partial', stateLabel: 'verified with open recovery issue' },
  { phase: 'P6', label: 'Host client implemented; full burst matrix and external loopback next', state: 'next', stateLabel: 'next' },
  { phase: 'P7', label: 'Decoder libraries and host UART command locally tested; target live path open', state: 'partial', stateLabel: 'target path open' },
  { phase: 'P8–P10', label: 'Hardening, performance and release gates remain; boot packaging tooling exists', state: 'later', stateLabel: 'open' },
];

const proof = [
  { value: '7/7', label: 'Burst E2E bench tests', note: 'internal counter source; multi-buffer, short-tail, abort and capacity cases' },
  { value: '5/5', label: 'ESP32 target Unity tests', note: 'relay, queue/notification, ring buffer, UART loopback and watchdog' },
  { value: '12/12', label: 'FPGA simulation cases', note: 'procedural, self-checking Questa tests' },
  { value: '44/44', label: 'Host tests', note: 'protocol, export and client logic; not all hardware scenarios' },
];

function StatusPill({ status, label }: { status: Stage['status']; label: string }) {
  return <span className={`status-pill status-${status}`}>{label}</span>;
}

function SignalPath({ active, onSelect }: { active: string; onSelect: (id: string) => void }) {
  return (
    <div className="signal-path" role="list" aria-label="Analyzer signal path">
      <div className="trace-line" aria-hidden="true"><span className="trace-pulse" /></div>
      {stages.map((stage, index) => {
        const Icon = stage.icon;
        return (
          <div className="stage-wrap" role="listitem" key={stage.id}>
            <button type="button" className={`stage-node ${active === stage.id ? 'is-active' : ''}`}
              onClick={() => onSelect(stage.id)} aria-pressed={active === stage.id}>
              <span className="stage-number">{stage.number}</span>
              <span className="stage-icon"><Icon aria-hidden="true" /></span>
              <span className="stage-short">{stage.short}</span>
            </button>
            {index < stages.length - 1 && <ChevronRight className="stage-chevron" aria-hidden="true" />}
          </div>
        );
      })}
    </div>
  );
}

function Waveform() {
  return (
    <svg className="waveform" viewBox="0 0 1080 152" role="img" aria-label="Eight sampled digital waveforms">
      <defs><linearGradient id="traceGradient" x1="0" x2="1"><stop offset="0" stopColor="#82ff9e" /><stop offset="0.55" stopColor="#37d9c9" /><stop offset="1" stopColor="#6cb7ff" /></linearGradient></defs>
      {[18, 36, 54, 72, 90, 108, 126, 144].map((y, index) => (
        <path key={y} className="wave-path" style={{ animationDelay: `${index * 75}ms` }}
          d={`M0 ${y} H${72 + index * 13} V${y - 10} H${142 + index * 18} V${y} H${250 + index * 8} V${y - 10} H${330 + index * 17} V${y} H${462 + index * 5} V${y - 10} H${590 + index * 14} V${y} H${720 + index * 4} V${y - 10} H${825 + index * 11} V${y} H1080`} />
      ))}
    </svg>
  );
}

export default function Home() {
  const [activeId, setActiveId] = useState('inputs');
  const active = stages.find((stage) => stage.id === activeId) ?? stages[0];
  const ActiveIcon = active.icon;

  return (
    <main>
      <nav className="topbar" aria-label="Page navigation">
        <a className="brand" href="#top" aria-label="Logic Analyzer overview"><span className="brand-mark"><Binary aria-hidden="true" /></span><span>LA<span className="brand-dim">/RTOS</span></span></a>
        <div className="nav-links"><a href="#journey">Signal journey</a><a href="#why-rtos">Why RTOS</a><a href="#status">Status</a></div>
        <span className="build-tag"><span /> Native Windows build</span>
      </nav>

      <section className="hero section-shell" id="top">
        <div className="hero-copy">
          <p className="kicker"><span>01</span> Project overview</p>
          <h1>Capture digital signals. <em>Understand what happened.</em></h1>
          <p className="hero-lede">A logic analyzer records when digital signals are high or low, helping you debug timing and communication. This project combines a Zybo Z7-10, an ESP32 and a Python host tool. Burst capture works through the wireless gateway; live UART, SPI and I²C decoding on the Zynq is the next major feature.</p>
          <p className="hero-focus">FreeRTOS coordinates acquisition, transmission and buffer ownership.</p>
          <div className="hero-actions"><a className="primary-action" href="#journey">Follow one signal <ArrowDown aria-hidden="true" /></a><span className="hero-fact"><strong>100 MS/s design target</strong><span>Recorded counter captures at 1 and 10 MS/s; full rate and external-input validation remain open.</span></span></div>
        </div>
        <div className="hero-scope" aria-label="Illustrative logic analyzer waveform, not a connected capture">
          <div className="scope-header"><span><span className="live-dot" /> ILLUSTRATIVE CAPTURE VIEW</span><span>8 CH · 100 MHz</span></div>
          <Waveform />
          <div className="scope-metrics"><div><span>TRIGGER</span><strong>CH0 ↑</strong></div><div><span>PRE-TRIGGER</span><strong>32,768 samples</strong></div><div><span>PACKING</span><strong>raw / transitions</strong></div></div>
          <p className="scope-caption">Illustrative waveforms — not a connected capture.</p>
        </div>
      </section>

      <section className="journey section-shell" id="journey">
        <div className="section-heading"><div><p className="kicker"><span>02</span> System path</p><h2>From digital pins to a capture file</h2></div><p>FPGA logic and the ARM processor share the Zynq chip on the Zybo. The ESP32 is a separate wireless gateway.</p></div>
        <SignalPath active={activeId} onSelect={setActiveId} />
        <div className="stage-detail" key={active.id}>
          <div className="detail-identity"><span className="detail-icon"><ActiveIcon aria-hidden="true" /></span><div><span className="detail-eyebrow">{active.eyebrow}</span><h3>{active.label}</h3></div></div>
          <div className="detail-copy"><p>{active.description}</p><p>{active.detail}</p></div>
          <div className="detail-meta"><StatusPill status={active.status} label={active.evidence} /><span>{active.stat}</span></div>
        </div>
      </section>

      <section className="modes-section section-shell">
        <div className="section-heading compact-heading"><div><p className="kicker"><span>03</span> Two ways to observe</p><h2>Freeze the moment—or keep listening.</h2></div></div>
        <Tabs defaultValue="burst" className="mode-tabs">
          <TabsList className="mode-list" aria-label="Capture mode"><TabsTrigger value="burst" className="mode-trigger"><HardDrive /> Burst capture</TabsTrigger><TabsTrigger value="live" className="mode-trigger"><Radio /> Live decode</TabsTrigger></TabsList>
          <TabsContent value="burst" className="mode-panel">
            <div className="mode-copy"><span className="mode-index">A</span><span className="mode-status verified">Working on the bench through the ESP32 gateway.</span><h3>Capture around an exact trigger</h3><p>Configure the capture and arm it. The FPGA retains pre-trigger history, records the requested samples after the trigger and finishes the capture packet. The DMA writes into a finite set of memory buffers, and the firmware drains the completed recording to the host.</p><p className="mode-note">A trigger chooses when the event of interest begins. Pre-trigger history lets you inspect the lead-up.</p></div>
            <div className="mode-visual"><div className="mode-sequence" aria-label="Burst capture sequence"><span>ARM</span><i /><span>history</span><i /><span className="accent">TRIGGER</span><i /><span>tail</span><i /><span>export</span></div><div className="packing-example" aria-label="Transition packing example"><span>Five samples</span><code>00 00 00 01 01</code><ChevronRight aria-hidden="true" /><code>(3 × 00), (2 × 01)</code></div><p className="illustration-caption">Raw mode stores every eight-bit sample. Transition mode stores a sample value plus how many sampling intervals it lasted. Actual transition records are four bytes each: quiet signals can shrink; rapidly changing signals can expand.</p></div>
            <div className="mode-outcome"><Check /> Best for glitches, boot sequences and repeatable tests.</div>
          </TabsContent>
          <TabsContent value="live" className="mode-panel">
            <div className="mode-copy"><span className="mode-index">B</span><span className="mode-status planned">Planned target pipeline; decoder libraries and host UART command already have local tests.</span><h3>Decode while traffic is happening</h3><p>The planned live path passes completed segments to UART, SPI and I²C decoder tasks. These tasks will produce timestamped protocol events and release their buffer references when done.</p><p className="mode-note">The design includes a 2 ms FPGA packet-flush deadline so sparse traffic need not fill a large buffer. The 25 ms p99 UART-event latency goal still needs target measurement.</p></div>
            <div className="mode-visual"><div className="event-stream" aria-label="Illustrative live UART events, not a target capture"><span><b>12.004 ms</b> UART RX <code>0x4C</code> “L”</span><span><b>12.091 ms</b> UART RX <code>0x41</code> “A”</span><span><b>12.178 ms</b> UART RX <code>0x2D</code> “-”</span></div><p className="illustration-caption">Illustrative UART events — local/mock-tested; matching target decoder pipeline is not implemented.</p></div>
            <div className="mode-outcome planned"><Gauge /> Current target firmware rejects live-start and decoder-configuration commands.</div>
          </TabsContent>
        </Tabs>
      </section>

      <section className="rtos-section" id="why-rtos"><div className="section-shell rtos-grid">
        <div className="rtos-intro"><p className="kicker"><span>04</span> Why FreeRTOS?</p><h2>Speed is only useful if the data remains trustworthy.</h2><p>A fast sampler can still fail through stale buffers, blocked links or overloaded decoders. The RTOS architecture makes those failure boundaries explicit and testable.</p></div>
        <div className="ownership-card"><div className="ownership-head"><span>PLANNED LIVE-BUFFER OWNERSHIP</span><span className="generation">ILLUSTRATED EXAMPLE</span></div><div className="ownership-flow"><div><Box /><span>Free</span></div><ChevronRight aria-hidden="true" /><div><CircuitBoard /><span>DMA owns</span></div><ChevronRight aria-hidden="true" /><div className="active-owner"><Cpu /><span>CPU consumers own</span></div><ChevronRight aria-hidden="true" /><div><Box /><span>Free</span></div></div><div className="ownership-count"><div><span>Reference count</span><strong>2 → 1 → 0</strong></div><p>Suppose UART and SPI decoders both reference one segment. The reference count starts at two. UART finishes: one remains. SPI finishes: zero remains, so acquisition may requeue that buffer.</p></div><p className="ownership-support">Two consumers must release the segment before acquisition may requeue it. This contract has native tests; on-target live integration remains open.</p></div>
        <div className="principles-grid"><article><ShieldCheck /><h3>Small interrupts</h3><p>Interrupts acknowledge and hand off. Tasks perform acquisition processing and decoding.</p></article><article><Box /><h3>Handle ownership</h3><p>Large buffers move by reference, not by copy.</p></article><article><Gauge /><h3>Visible pressure</h3><p>Queue policies and loss counters expose overload.</p></article><article><Activity /><h3>Measured timing</h3><p>Latency, stack margin and throughput become evidence.</p></article></div>
      </div></section>

      <section className="reality section-shell">
        <div className="section-heading"><div><p className="kicker"><span>05</span> Engineering reality</p><h2>Capture is faster than transport.</h2></div><p>The design treats this mismatch as a first-class constraint—not a footnote.</p></div>
        <div className="rate-card"><div className="rate-heading"><div><span className="rate-kicker">Calculated capacity budget</span><h3>Design arithmetic, before protocol overhead</h3></div><span className="rate-scale-note">Bars compare the 100/400 MB/s design budget; UART uses point markers for legibility.</span></div><div className="rate-row raw"><div><span>Raw at 100 MS/s</span><strong>100 MB/s</strong></div><i aria-hidden="true" /><small>100,000,000 samples/s × 1 byte</small></div><div className="rate-row transition"><div><span>Worst-case transition stream</span><strong>400 MB/s</strong></div><i aria-hidden="true" /><small>100,000,000 records/s × 4 bytes</small></div><div className="rate-row link"><div><span>UART at 921600 baud (default, 8N1)</span><strong>92,160 B/s</strong></div><i aria-hidden="true" /><small>921600 / 10; before framing and flow-control delays</small></div><div className="rate-row link"><div><span>UART at 2 Mbps (8N1)</span><strong>200,000 B/s</strong></div><i aria-hidden="true" /><small>2,000,000 / 10; before framing and flow-control delays</small></div><p className="rate-summary">That is why burst mode stores to DDR, live mode uses bounded packets, and every stage needs honest backpressure and loss reporting.</p><p className="rate-example"><strong>Calculated example:</strong> At the theoretical 2 Mbps UART ceiling, draining 64 MB takes at least 320 seconds before protocol overhead.</p></div>
        <div className="measured-loopback"><div className="measured-heading"><h3>Measured gateway loopback, 10 Sep 2026</h3><span>Round-trip echo</span></div><div className="table-wrap"><table><thead><tr><th scope="col">Configured UART baud</th><th scope="col">Recorded echo throughput</th><th scope="col">Test result</th></tr></thead><tbody><tr><td>921600</td><td>44.4 kB/s</td><td>10.07 MB, zero parser CRC/format errors, no FIFO overruns</td></tr><tr><td>2 Mbps</td><td>39.5 kB/s</td><td>10.07 MB, zero parser CRC/format errors, no FIFO overruns</td></tr></tbody></table></div><p>These are round-trip echo measurements through the gateway, not one-way live-event capacity. Raising baud did not improve this test; the relay path needs profiling.</p></div>
      </section>

      <section className="status-section section-shell" id="status">
        <div className="section-heading"><div><p className="kicker"><span>06</span> Current evidence</p><h2>What is real today.</h2></div><p>Reviewed {evidenceRevision.reviewedOn}; bench evidence recorded {evidenceRevision.benchOn}; source <a className="inline-source" href={evidenceRevision.sourceRevisionUrl} target="_blank" rel="noreferrer">{evidenceRevision.sourceSha.slice(0, 7)}</a>.</p></div>
        <div className="proof-grid">{proof.map((item) => <article key={item.label}><strong>{item.value}</strong><span>{item.label}</span><small>{item.note}</small></article>)}</div>
        <div className="proof-secondary"><p><strong>Secondary evidence:</strong> 14/14 native precheck evidence, labeled as a native precheck rather than a product release.</p><p><strong>Recorded example:</strong> A 1 MS/s raw capture transferred one million consecutive counter samples across 16 DMA descriptors; the recorded run took 25.6 seconds.</p></div>
        <div className="milestone-card">{milestones.map((milestone) => <div className={`milestone milestone-${milestone.state}`} key={milestone.phase}><span className="milestone-dot">{milestone.state === 'done' ? <Check /> : null}</span><div><strong>{milestone.phase}</strong><span>{milestone.label}</span></div><span className="milestone-state">{milestone.stateLabel}</span></div>)}</div>
        <div className="blocker-note"><Gauge aria-hidden="true" /><div><strong>Open work and known limits</strong><ol><li>Complete the P6 stimulus/rate matrix at 1, 10 and 100 MS/s, external JE→JC loopback, PulseView evidence and burst release gate.</li><li>Resolve the observed stale-client behavior after a host timeout; subsequent sessions have required an ESP32 reset in recorded runs.</li><li>Profile gateway processing and implement/measure the live decode path before advertising live-rate or latency guarantees.</li></ol><p className="optional-route"><strong>Optional diagnostic route:</strong> A separate USB-UART adapter can exercise the analyzer link, but the gateway already exercises it.</p></div></div>
        <div className="source-row"><span>Evidence sources at the pinned revision</span><div className="source-links"><a href={sources.status} target="_blank" rel="noreferrer">Execution ledger</a><a href={sources.plan} target="_blank" rel="noreferrer">Implementation plan</a><a href={sources.regmap} target="_blank" rel="noreferrer">Register specification</a><a href={sources.e2e} target="_blank" rel="noreferrer">Burst E2E test</a></div></div>
      </section>

      <footer className="footer"><div className="section-shell footer-inner"><div><span className="brand footer-brand"><span className="brand-mark"><Binary /></span>LA<span className="brand-dim">/RTOS</span></span><p>Zybo Z7-10 · Zynq-7010 · ESP32 · FreeRTOS · native Windows</p></div><p className="footer-note">Targets are not presented as measurements.<br />Source revision {evidenceRevision.sourceSha.slice(0, 7)} · reviewed {evidenceRevision.reviewedOn}</p></div></footer>
    </main>
  );
}
