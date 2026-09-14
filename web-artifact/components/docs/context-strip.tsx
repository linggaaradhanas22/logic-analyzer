
/* oxlint-disable next/no-html-link-for-pages -- Vinext client Link navigation fails in production. */
export type ContextItem = { label: string; href: string; current?: boolean };

export function ContextStrip({ parent, items }: { parent?: ContextItem; items: ContextItem[] }) {
  return (
    <nav className="doc-context" aria-label="Where this sits">
      <strong>Where this sits</strong>
      {parent ? (
        <span>
          Parent: <a href={parent.href}>{parent.label}</a>
        </span>
      ) : null}
      {items.map((item) => (
        <a className="doc-chip" data-current={item.current ? 'true' : undefined} href={item.href} key={item.href}>
          {item.label}
        </a>
      ))}
    </nav>
  );
}
