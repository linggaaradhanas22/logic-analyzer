import Link from 'next/link';
import { notFound } from 'next/navigation';
import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { ContextStrip } from '@/components/docs/context-strip';
import { EvidenceList, StatusBadge } from '@/components/docs/status-badge';
import { InterfaceTable } from '@/components/docs/interface-table';
import { RelatedLinks } from '@/components/docs/related-links';
import { SourceLinks } from '@/components/docs/source-links';
import { COMPONENTS, componentBySlug, componentsForSubsystem } from '@/lib/docs/components';
import { resolveLink } from '@/lib/docs/links';
import { subsystemBySlug } from '@/lib/docs/subsystems';

export function generateStaticParams() {
  return COMPONENTS.map((component) => ({ slug: component.slug }));
}

function ReferenceList({ title, references }: { title: string; references: string[] }) {
  if (!references.length) return null;
  return (
    <div>
      <h3>{title}</h3>
      <ul className="doc-prose">
        {references.map((reference) => {
          const link = resolveLink(reference);
          return (
            <li key={reference}>
              {link ? <Link className="inline-source" href={link.href}>{link.label}</Link> : reference}
            </li>
          );
        })}
      </ul>
    </div>
  );
}

export default async function ComponentPage({ params }: { params: Promise<{ slug: string }> }) {
  const { slug } = await params;
  const component = componentBySlug(slug);
  if (!component) notFound();

  const subsystem = subsystemBySlug(component.subsystem);
  const index = COMPONENTS.findIndex((item) => item.slug === component.slug);
  const previous = index > 0 ? COMPONENTS[index - 1] : undefined;
  const next = index < COMPONENTS.length - 1 ? COMPONENTS[index + 1] : undefined;
  const siblings = componentsForSubsystem(component.subsystem);

  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs
          items={[
            { label: 'System', href: '/' },
            { label: subsystem?.name ?? component.subsystem, href: `/subsystems/${component.subsystem}` },
            { label: component.name },
          ]}
        />
        <header className="doc-header">
          <p className="doc-eyebrow">Level 3 · Component</p>
          <h1>{component.name}</h1>
          <p className="doc-answer">{component.summary}</p>
          <p className="doc-why">{component.why}</p>
          <div className="doc-status-row">
            {component.evidence.map((item, itemIndex) => (
              <StatusBadge status={item.status} key={`${item.status}-${itemIndex}`} />
            ))}
          </div>
          <p className="doc-scope">{component.evidence[0]?.scope}</p>
        </header>

        <ContextStrip
          parent={subsystem ? { label: subsystem.name, href: `/subsystems/${subsystem.slug}` } : undefined}
          items={siblings.map((item) => ({
            label: item.name,
            href: `/components/${item.slug}`,
            current: item.slug === component.slug,
          }))}
        />

        <section className="doc-section">
          <h2>Where it sits</h2>
          <div className="doc-grid cols-2">
            <ReferenceList title="Upstream (feeds this component)" references={component.upstream} />
            <ReferenceList title="Downstream (this component feeds)" references={component.downstream} />
          </div>
        </section>

        <section className="doc-section">
          <h2>Inputs and outputs</h2>
          <div className="doc-grid cols-2">
            <InterfaceTable caption="Inputs" items={component.inputs} />
            <InterfaceTable caption="Outputs" items={component.outputs} />
          </div>
        </section>

        <section className="doc-section">
          <h2>Step-by-step behavior</h2>
          <ol className="doc-prose">
            {component.steps.map((step) => (
              <li key={step}>{step}</li>
            ))}
          </ol>
        </section>

        <section className="doc-section">
          <h2>Important state</h2>
          <ul className="doc-prose">
            {component.state.map((item) => (
              <li key={item}>{item}</li>
            ))}
          </ul>
        </section>

        <section className="doc-section">
          <h2>Configuration</h2>
          <ul className="doc-prose">
            {component.configuration.map((item) => (
              <li key={item}>{item}</li>
            ))}
          </ul>
        </section>

        <section className="doc-section">
          <h2>Backpressure and failure behavior</h2>
          <ul className="doc-prose">
            {component.failures.map((item) => (
              <li key={item}>{item}</li>
            ))}
          </ul>
        </section>

        <section className="doc-section">
          <h2>Concurrency and ownership</h2>
          <ul className="doc-prose">
            {component.ownership.map((item) => (
              <li key={item}>{item}</li>
            ))}
          </ul>
        </section>

        <section className="doc-section">
          <h2>Current status and evidence</h2>
          <EvidenceList items={component.evidence} />
        </section>

        <section className="doc-section">
          <h2>Source anchors</h2>
          <SourceLinks sources={component.sources} />
        </section>

        <section className="doc-section">
          <h2>Related</h2>
          <RelatedLinks
            items={component.related
              .map((reference) => resolveLink(reference))
              .filter((item): item is { label: string; href: string } => item !== null)}
          />
        </section>

        <nav className="doc-prevnext" aria-label="Component order">
          {previous ? (
            <Link href={`/components/${previous.slug}`}>
              <span className="dir">Previous component</span>
              <span className="name">{previous.name}</span>
            </Link>
          ) : null}
          {next ? (
            <Link className="next" href={`/components/${next.slug}`}>
              <span className="dir">Next component</span>
              <span className="name">{next.name}</span>
            </Link>
          ) : null}
        </nav>
      </div>
    </main>
  );
}
