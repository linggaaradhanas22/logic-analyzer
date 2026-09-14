import Link from 'next/link';
import { notFound } from 'next/navigation';
import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { ContextStrip } from '@/components/docs/context-strip';
import { EvidenceList } from '@/components/docs/status-badge';
import { InterfaceTable } from '@/components/docs/interface-table';
import { RelatedLinks } from '@/components/docs/related-links';
import { SourceLinks } from '@/components/docs/source-links';
import { SubsystemDiagram, type DiagramNode } from '@/components/docs/subsystem-diagram';
import { componentsForSubsystem } from '@/lib/docs/components';
import { SUBSYSTEMS, subsystemBySlug } from '@/lib/docs/subsystems';

export function generateStaticParams() {
  return SUBSYSTEMS.map((subsystem) => ({ slug: subsystem.slug }));
}

export default async function SubsystemPage({ params }: { params: Promise<{ slug: string }> }) {
  const { slug } = await params;
  const subsystem = subsystemBySlug(slug);
  if (!subsystem) notFound();

  const index = SUBSYSTEMS.findIndex((item) => item.slug === subsystem.slug);
  const previous = index > 0 ? SUBSYSTEMS[index - 1] : undefined;
  const next = index < SUBSYSTEMS.length - 1 ? SUBSYSTEMS[index + 1] : undefined;
  const components = componentsForSubsystem(subsystem.slug);
  const nodeState = (name: string): DiagramNode['state'] => {
    const component = components.find((item) => item.name.startsWith(name) || item.slug === name);
    if (!component) return undefined;
    if (component.evidence.some((item) => item.status === 'blocked')) return 'issue';
    if (component.evidence.some((item) => item.status === 'planned')) return 'planned';
    if (component.evidence.some((item) => item.status === 'bench-verified' || item.status === 'target-tested')) return 'verified';
    return undefined;
  };
  const diagramNodes: DiagramNode[] = subsystem.diagram.map((label) => {
    const component = components.find((item) => item.name === label || item.slug === label);
    return { label, slug: component?.slug, state: component ? nodeState(component.name) : undefined };
  });

  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: subsystem.name }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">
            Level 2 · Subsystem {String(subsystem.order).padStart(2, '0')}
          </p>
          <h1>{subsystem.name}</h1>
          <p className="doc-answer">{subsystem.beginner[0]}</p>
          <p className="doc-why">{subsystem.beginner[1]}</p>
          <div className="doc-status-row">
            {subsystem.status.map((item, itemIndex) => (
              <span className="doc-badge" data-status={item.status} key={itemIndex}>
                {item.status.replace(/-/g, ' ')}
              </span>
            ))}
          </div>
          <p className="doc-scope">{subsystem.status[0]?.scope}</p>
        </header>

        <ContextStrip
          parent={{ label: 'System overview', href: '/' }}
          items={SUBSYSTEMS.map((item) => ({
            label: item.name,
            href: `/subsystems/${item.slug}`,
            current: item.slug === subsystem.slug,
          }))}
        />

        <section className="doc-section">
          <h2>Boundary</h2>
          <p className="doc-prose">{subsystem.boundary}</p>
        </section>

        <section className="doc-section">
          <h2>Subsystem diagram</h2>
          <SubsystemDiagram nodes={diagramNodes} secondary={subsystem.secondary} caption="Ordered left to right; dashed nodes are planned, double borders carry bench or target evidence." />
        </section>

        <section className="doc-section">
          <h2>What happens</h2>
          <ol className="doc-prose">
            {subsystem.sequence.map((step) => (
              <li key={step}>{step}</li>
            ))}
          </ol>
        </section>

        <section className="doc-section">
          <h2>Inputs and outputs</h2>
          <div className="doc-grid cols-2">
            <InterfaceTable caption="Inputs" items={subsystem.inputs} />
            <InterfaceTable caption="Outputs" items={subsystem.outputs} />
          </div>
        </section>

        <section className="doc-section">
          <h2>Design decisions and constraints</h2>
          <ul className="doc-prose">
            {subsystem.decisions.map((decision) => (
              <li key={decision}>{decision}</li>
            ))}
          </ul>
        </section>

        <section className="doc-section">
          <h2>Status and evidence</h2>
          <EvidenceList items={subsystem.status} />
        </section>

        <section className="doc-section">
          <h2>
            Components <span className="doc-count">{components.length}</span>
          </h2>
          <div className="doc-index">
            {components.map((component) => (
              <Link className="doc-index-card" href={`/components/${component.slug}`} key={component.slug}>
                <strong>{component.name}</strong>
                <p>{component.summary}</p>
                <span className="order">{component.evidence[0]?.status.replace(/-/g, ' ')}</span>
              </Link>
            ))}
          </div>
        </section>

        <section className="doc-section">
          <h2>Sources</h2>
          <SourceLinks sources={subsystem.sources} />
        </section>

        <section className="doc-section">
          <h2>Related</h2>
          <RelatedLinks items={subsystem.related.map((reference) => ({ label: reference, href: reference }))} />
        </section>

        <nav className="doc-prevnext" aria-label="Subsystem order">
          {previous ? (
            <Link href={`/subsystems/${previous.slug}`}>
              <span className="dir">Previous subsystem</span>
              <span className="name">{previous.name}</span>
            </Link>
          ) : null}
          {next ? (
            <Link className="next" href={`/subsystems/${next.slug}`}>
              <span className="dir">Next subsystem</span>
              <span className="name">{next.name}</span>
            </Link>
          ) : null}
        </nav>
      </div>
    </main>
  );
}
