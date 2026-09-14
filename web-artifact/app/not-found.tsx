/* oxlint-disable next/no-html-link-for-pages -- Vinext client Link navigation fails in production. */
import { SUBSYSTEMS } from '@/lib/docs/subsystems';

export const metadata = {
  title: 'Page not found — RTOS Logic Analyzer',
};

export default function NotFound() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <header className="doc-header">
          <p className="doc-eyebrow">404</p>
          <h1>That page is not part of this documentation.</h1>
          <p className="doc-answer">
            The address may be mistyped, or the component may have been renamed. Start from the system
            overview or pick a subsystem below.
          </p>
        </header>
        <section className="doc-section">
          <h2>Subsystems</h2>
          <div className="doc-index">
            {SUBSYSTEMS.map((subsystem) => (
              <a className="doc-index-card" href={`/subsystems/${subsystem.slug}`} key={subsystem.slug}>
                <strong>{subsystem.name}</strong>
                <p>{subsystem.responsibility}</p>
              </a>
            ))}
          </div>
        </section>
        <p>
          <a className="inline-source" href="/">
            Return to the system overview
          </a>
        </p>
      </div>
    </main>
  );
}
