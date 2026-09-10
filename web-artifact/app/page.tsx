'use client';

import { useState } from 'react';
import {
  Activity, ArrowDown, Binary, Box, Check, ChevronRight, CircuitBoard,
  Cpu, Gauge, HardDrive, Laptop, Radio, ShieldCheck, Wifi, type LucideIcon,
} from 'lucide-react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';

type Stage = {
  id: string; number: string; short: string; label: string; eyebrow: string;
  description: string; detail: string; stat: string;
  status: 'verified' | 'local' | 'planned'; icon: LucideIcon;
};

const stages: Stage[] = [
  {
    id: 'inputs', number: '01', short: 'Signals', label: '8 digital inputs',
    eyebrow: 'The thing being observed',
    description: 'Eight 3.3 V signals enter through the Zybo JC header. A two-stage synchronizer makes asynchronous edges safe to sample.',
    detail: 'The released target is 100 million samples per second. A built-in pattern generator can create counter, UART, SPI and I²C traffic for self-test.',
    stat: '8 channels · 3.3 V', status: 'verified', icon: Activity,
  },
  {
    id: 'fpga', number: '02', short: 'Capture', label: 'FPGA fabric',
    eyebrow: 'The fast, deterministic layer',
    description: 'Custom logic samples the pins, watches for a trigger, preserves pre-trigger history and packs the result into a stream.',
    detail: 'The stream can stay raw or use transition records. A non-cyclic scatter-gather DMA engine moves it into DDR without making the CPU copy every sample.',
    stat: '100 MS/s target', status: 'verified', icon: CircuitBoard,
  },
  {
    id: 'rtos', number: '03', short: 'Coordinate', label: 'Zynq + FreeRTOS',
    eyebrow: 'The real-time control centre',
    description: 'FreeRTOS separates acquisition, commands, transmission and health reporting into small tasks with explicit ownership.',
    detail: 'Interrupts only wake tasks. Capture buffers move by handle, queues have defined full policies, and ordinary DMA descriptors are recycled only after every consumer releases them.',
    stat: '4 core tasks running', status: 'verified', icon: Cpu,
  },
  {
    id: 'bridge', number: '04', short: 'Bridge', label: 'ESP32 gateway',
    eyebrow: 'The cable-to-network translator',
    description: 'An ESP32 receives the framed analyzer link over a flow-controlled UART and will relay it to one Wi-Fi client.',
    detail: 'The portable relay and connection policies already pass native tests and compile in ESP-IDF. The physical UART/network task adapters and loopback bench gate are still ahead.',
    stat: 'UART → Wi-Fi TCP', status: 'local', icon: Wifi,
  },
  {
    id: 'host', number: '05', short: 'Explore', label: 'Host tools',
    eyebrow: 'Where captured time becomes useful',
    description: 'The Python command-line tool controls captures over serial or TCP, checks framing and turns the result into familiar analysis files.',
    detail: 'Burst captures export to PulseView .sr, VCD or PNG. The live command can already parse and print decoded UART events; its matching target pipeline is planned.',
    stat: '.sr · .vcd · .png · CSV', status: 'local', icon: Laptop,
  },
];

const milestones = [
  { phase: 'P0–P1', label: 'Foundations & specifications', state: 'done' },
  { phase: 'P2', label: 'FPGA capture pipeline', state: 'done' },
  { phase: 'P3', label: 'Vivado integration & board blink', state: 'done' },
  { phase: 'P4', label: 'Zynq FreeRTOS core', state: 'partial' },
  { phase: 'P5–P7', label: 'ESP32 and live decode path', state: 'next' },
  { phase: 'P8–P10', label: 'Measurement, performance & release', state: 'later' },
];

const proof = [
  { value: '12/12', label: 'FPGA simulations', note: 'self-checking Questa cases' },
  { value: '+0.255 ns', label: 'Timing margin', note: '100 MHz routed design' },
  { value: '14/14', label: 'Native release checks', note: 'portable logic and contracts' },
  { value: '44/44', label: 'Host tests', note: 'protocol, capture and live flows' },
];

function StatusPill({ status }: { status: Stage['status'] }) {
  const copy = { verified: 'Bench verified', local: 'Locally verified', planned: 'Planned' }[status];
  return <span className={`status-pill status-${status}`}>{copy}</span>;
}

function SignalPath({ active, onSelect }: { active: string; onSelect: (id: string) => void }) {
  return (
    <div className="signal-path" role="list" aria-label="Analyzer signal path">
      <div className="trace-line" aria-hidden="true"><span className="trace-pulse" /></div>
      {stages.map((stage, index) => {
        const Icon = stage.icon;
        return (
          <div className="stage-wrap" key={stage.id}>
            <button type="button" role="listitem" className={`stage-node ${active === stage.id ? 'is-active' : ''}`}
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
  const [activeId, setActiveId] = useState('rtos');
  const active = stages.find((stage) => stage.id === activeId) ?? stages[2];
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
          <h1>A logic analyzer with a <em>real-time operating system</em> in the data path.</h1>
          <p className="hero-lede">This project turns a Zybo Z7-10 and an ESP32 into an eight-channel instrument that captures digital signals, decodes protocols and moves results to a Windows host—without hiding overloads or ownership mistakes.</p>
          <div className="hero-actions"><a className="primary-action" href="#journey">Follow one signal <ArrowDown aria-hidden="true" /></a><span className="hero-fact"><strong>100 MS/s</strong> released sampling target</span></div>
        </div>
        <div className="hero-scope" aria-label="Live signal scope illustration">
          <div className="scope-header"><span><span className="live-dot" /> CAPTURE PIPELINE</span><span>8 CH · 100 MHz</span></div>
          <Waveform />
          <div className="scope-metrics"><div><span>TRIGGER</span><strong>CH0 ↑</strong></div><div><span>PRE-HISTORY</span><strong>≤ 64K samples</strong></div><div><span>PACKING</span><strong>raw / transitions</strong></div></div>
        </div>
      </section>

      <section className="journey section-shell" id="journey">
        <div className="section-heading"><div><p className="kicker"><span>02</span> Follow the data</p><h2>One edge. Five jobs.</h2></div><p>Select a stage to see why each device exists in the system.</p></div>
        <SignalPath active={activeId} onSelect={setActiveId} />
        <div className="stage-detail" key={active.id}>
          <div className="detail-identity"><span className="detail-icon"><ActiveIcon aria-hidden="true" /></span><div><span className="detail-eyebrow">{active.eyebrow}</span><h3>{active.label}</h3></div></div>
          <div className="detail-copy"><p>{active.description}</p><p>{active.detail}</p></div>
          <div className="detail-meta"><StatusPill status={active.status} /><span>{active.stat}</span></div>
        </div>
      </section>

      <section className="modes-section section-shell">
        <div className="section-heading compact-heading"><div><p className="kicker"><span>03</span> Two ways to observe</p><h2>Freeze the moment—or keep listening.</h2></div></div>
        <Tabs defaultValue="burst" className="mode-tabs">
          <TabsList className="mode-list" aria-label="Capture mode"><TabsTrigger value="burst" className="mode-trigger"><HardDrive /> Burst capture</TabsTrigger><TabsTrigger value="live" className="mode-trigger"><Radio /> Live decode</TabsTrigger></TabsList>
          <TabsContent value="burst" className="mode-panel">
            <div className="mode-copy"><span className="mode-index">A</span><h3>Capture around an exact trigger</h3><p>The FPGA keeps recent history in block RAM, then closes a finite DMA chain after the trigger. The host receives a complete, checked recording for deep inspection.</p></div>
            <div className="mode-sequence" aria-label="Burst capture sequence"><span>ARM</span><i /><span>history</span><i /><span className="accent">TRIGGER</span><i /><span>tail</span><i /><span>export</span></div>
            <div className="mode-outcome"><Check /> Best for glitches, boot sequences and repeatable tests.</div>
          </TabsContent>
          <TabsContent value="live" className="mode-panel">
            <div className="mode-copy"><span className="mode-index">B</span><h3>Decode while traffic is happening</h3><p>Ordinary DMA segments circulate only after all consumers release them. UART, SPI and I²C decoder tasks turn transitions into events, while counters make overload visible.</p></div>
            <div className="event-stream" aria-label="Example live UART events"><span><b>12.004 ms</b> UART RX <code>0x4C</code> “L”</span><span><b>12.091 ms</b> UART RX <code>0x41</code> “A”</span><span><b>12.178 ms</b> UART RX <code>0x2D</code> “-”</span></div>
            <div className="mode-outcome planned"><Gauge /> Target pipeline is the next major implementation phase.</div>
          </TabsContent>
        </Tabs>
      </section>

      <section className="rtos-section" id="why-rtos"><div className="section-shell rtos-grid">
        <div className="rtos-intro"><p className="kicker"><span>04</span> Why FreeRTOS?</p><h2>Speed is only useful if the data remains trustworthy.</h2><p>A fast sampler can still fail through stale buffers, blocked links or overloaded decoders. The RTOS architecture makes those failure boundaries explicit and testable.</p></div>
        <div className="ownership-card"><div className="ownership-head"><span>SEGMENT 07</span><span className="generation">GEN 0031</span></div><div className="ownership-flow"><div><CircuitBoard /><span>DMA owns</span></div><ChevronRight /><div className="active-owner"><Cpu /><span>CPU owns</span></div><ChevronRight /><div><Box /><span>Free again</span></div></div><div className="refcount"><span>consumer references</span><strong>2</strong><i style={{ width: '62%' }} /></div><p>No descriptor returns to hardware while a decoder or transmitter still holds its payload.</p></div>
        <div className="principles-grid"><article><ShieldCheck /><h3>Small interrupts</h3><p>Interrupts notify. Tasks do the work.</p></article><article><Box /><h3>Handle ownership</h3><p>Large buffers move by reference, not by copy.</p></article><article><Gauge /><h3>Visible pressure</h3><p>Queue policies and loss counters expose overload.</p></article><article><Activity /><h3>Measured timing</h3><p>Latency, stack margin and throughput become evidence.</p></article></div>
      </div></section>

      <section className="reality section-shell">
        <div className="section-heading"><div><p className="kicker"><span>05</span> Engineering reality</p><h2>Capture is faster than transport.</h2></div><p>The design treats this mismatch as a first-class constraint—not a footnote.</p></div>
        <div className="rate-card"><div className="rate-row raw"><div><span>Raw acquisition</span><strong>100 MB/s</strong></div><i /></div><div className="rate-row transition"><div><span>Worst-case transition stream</span><strong>400 MB/s</strong></div><i /></div><div className="rate-row link"><div><span>2 Mbps UART before overhead</span><strong>0.2 MB/s</strong></div><i /></div><p>That is why burst mode stores to DDR, live mode uses bounded packets, and every stage needs honest backpressure and loss reporting.</p></div>
      </section>

      <section className="status-section section-shell" id="status">
        <div className="section-heading"><div><p className="kicker"><span>06</span> Current state · 10 Sep 2026</p><h2>What is real today.</h2></div><p>“Built” means a test or bench observation exists. Hardware-only gaps stay visible.</p></div>
        <div className="proof-grid">{proof.map((item) => <article key={item.label}><strong>{item.value}</strong><span>{item.label}</span><small>{item.note}</small></article>)}</div>
        <div className="milestone-card">{milestones.map((milestone) => <div className={`milestone milestone-${milestone.state}`} key={milestone.phase}><span className="milestone-dot">{milestone.state === 'done' ? <Check /> : null}</span><div><strong>{milestone.phase}</strong><span>{milestone.label}</span></div><span className="milestone-state">{milestone.state === 'done' ? 'verified' : milestone.state === 'partial' ? 'core built · HIL blocked' : milestone.state === 'next' ? 'next' : 'later'}</span></div>)}</div>
        <div className="blocker-note"><Radio aria-hidden="true" /><div><strong>The honest blocker</strong><p>The analyzer’s dedicated UART on the JD header still needs a 3.3 V flow-control-capable USB-UART adapter. Until that link is exercised, DMA capture on wire and end-to-end burst behavior remain unverified.</p></div></div>
      </section>

      <footer className="footer"><div className="section-shell footer-inner"><div><span className="brand footer-brand"><span className="brand-mark"><Binary /></span>LA<span className="brand-dim">/RTOS</span></span><p>Zybo Z7-10 · Zynq-7010 · ESP32 · FreeRTOS · native Windows</p></div><p className="footer-note">Built from the project plan and execution ledger.<br />Targets are not presented as measurements.</p></div></footer>
    </main>
  );
}
