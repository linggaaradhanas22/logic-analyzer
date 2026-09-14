import Link from 'next/link';

export function RelatedLinks({ items }: { items: { label: string; href: string }[] }) {
  if (!items.length) return null;
  return (
    <div className="doc-status-row">
      {items.map((item) => (
        <Link className="doc-chip" href={item.href} key={item.href}>
          {item.label}
        </Link>
      ))}
    </div>
  );
}
