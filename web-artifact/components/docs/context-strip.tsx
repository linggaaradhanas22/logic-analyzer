import Link from 'next/link';

export type ContextItem = { label: string; href: string; current?: boolean };

export function ContextStrip({ parent, items }: { parent?: ContextItem; items: ContextItem[] }) {
  return (
    <nav className="doc-context" aria-label="Where this sits">
      <strong>Where this sits</strong>
      {parent ? (
        <span>
          Parent: <Link href={parent.href}>{parent.label}</Link>
        </span>
      ) : null}
      {items.map((item) => (
        <Link className="doc-chip" data-current={item.current ? 'true' : undefined} href={item.href} key={item.href}>
          {item.label}
        </Link>
      ))}
    </nav>
  );
}
