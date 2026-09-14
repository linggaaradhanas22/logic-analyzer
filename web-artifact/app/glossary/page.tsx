import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { GLOSSARY } from '@/lib/docs/glossary';

export const metadata = {
  title: 'Glossary — RTOS Logic Analyzer',
};

function anchor(term: string): string {
  return term.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
}

export default function GlossaryPage() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Glossary' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Reference · Terms</p>
          <h1>Glossary</h1>
          <p className="doc-answer">
            Short definitions for the project-specific and general terms used across this site. Where a
            term has a specific meaning here, the definition says so.
          </p>
        </header>

        <section className="doc-section">
          <h2>
            Terms <span className="doc-count">{GLOSSARY.length}</span>
          </h2>
          <dl className="doc-prose">
            {GLOSSARY.map((entry) => (
              <div key={entry.term} id={anchor(entry.term)} style={{ marginBottom: 18 }}>
                <dt>
                  <strong>{entry.term}</strong>
                </dt>
                <dd style={{ margin: '6px 0 0' }}>{entry.definition}</dd>
              </div>
            ))}
          </dl>
        </section>
      </div>
    </main>
  );
}
