import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { EvidenceList } from '@/components/docs/status-badge';
import { DECODER_COMMON, DECODERS } from '@/lib/docs/decoders';

export const metadata = {
  title: 'Decoder reference — RTOS Logic Analyzer',
};

export default function DecodersPage() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Reference' }, { label: 'Decoders' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Reference · Decoder semantics</p>
          <h1>Decoder reference</h1>
          <p className="doc-answer">
            The decoders turn transition records into protocol events. They are small allocation-free
            state machines in the shared C library, so the same code runs in native tests, on the Zynq
            decoder tasks and in the Python models used for parity checks.
          </p>
        </header>

        <section className="doc-section">
          <h2>Common transition model</h2>
          <ul className="doc-prose">
            {DECODER_COMMON.map((rule) => (
              <li key={rule}>{rule}</li>
            ))}
          </ul>
        </section>

        {DECODERS.map((decoder) => (
          <section className="doc-section" key={decoder.slug}>
            <h2>{decoder.name}</h2>
            <EvidenceList items={[decoder.status]} />
            <div className="doc-grid cols-2" style={{ marginTop: 18 }}>
              <div className="doc-panel">
                <h3>Configuration</h3>
                <ul>
                  {decoder.configuration.map((item) => (
                    <li key={item}>{item}</li>
                  ))}
                </ul>
              </div>
              <div className="doc-panel">
                <h3>Edge and sample behavior</h3>
                <ul>
                  {decoder.behavior.map((item) => (
                    <li key={item}>{item}</li>
                  ))}
                </ul>
              </div>
              <div className="doc-panel">
                <h3>Emitted events</h3>
                <ul>
                  {decoder.events.map((item) => (
                    <li key={item}>{item}</li>
                  ))}
                </ul>
              </div>
              <div className="doc-panel">
                <h3>Error behavior</h3>
                <ul>
                  {decoder.errors.map((item) => (
                    <li key={item}>{item}</li>
                  ))}
                </ul>
              </div>
            </div>
            <div className="doc-panel" style={{ marginTop: 14 }}>
              <h3>Functional test coverage</h3>
              <ul>
                {decoder.tests.map((item) => (
                  <li key={item}>{item}</li>
                ))}
              </ul>
            </div>
          </section>
        ))}
      </div>
    </main>
  );
}
