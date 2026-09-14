import Link from 'next/link';
import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { StatusBadge } from '@/components/docs/status-badge';
import { ROADMAP } from '@/lib/docs/roadmap';
import { subsystemBySlug } from '@/lib/docs/subsystems';

export const metadata = {
  title: 'Roadmap — RTOS Logic Analyzer',
};

export default function RoadmapPage() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Roadmap' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Roadmap · P0 to P10</p>
          <h1>Roadmap</h1>
          <p className="doc-answer">
            The plan defines phases P0 through P10. This page shows each phase individually: what it was
            meant to produce, what is recorded as evidence, and what remains open. A phase is not complete
            because a tool exists; it is complete when the plan&apos;s gate has recorded evidence.
          </p>
        </header>

        {ROADMAP.map((phase) => (
          <section className="doc-section" id={phase.id.toLowerCase()} key={phase.id}>
            <div className="doc-status-row" style={{ justifyContent: 'space-between' }}>
              <h2 style={{ margin: 0 }}>
                {phase.id} — {phase.name}
              </h2>
              <StatusBadge status={phase.status} />
            </div>
            <p className="doc-answer" style={{ fontSize: 16 }}>{phase.outcome}</p>
            <div className="doc-grid cols-3" style={{ marginTop: 16 }}>
              <div className="doc-panel">
                <h3>Current status</h3>
                <p>{phase.statusNote}</p>
              </div>
              <div className="doc-panel">
                <h3>Recorded evidence</h3>
                <p>{phase.evidence}</p>
              </div>
              <div className="doc-panel">
                <h3>Remaining</h3>
                <p>{phase.remaining}</p>
              </div>
            </div>
            <div className="doc-status-row" style={{ marginTop: 12 }}>
              {phase.subsystems.map((slug) => {
                const subsystem = subsystemBySlug(slug);
                if (!subsystem) return null;
                return (
                  <Link className="doc-chip" href={`/subsystems/${slug}`} key={slug}>
                    {subsystem.name}
                  </Link>
                );
              })}
            </div>
          </section>
        ))}
      </div>
    </main>
  );
}
