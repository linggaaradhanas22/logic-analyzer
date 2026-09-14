
/* oxlint-disable next/no-html-link-for-pages -- Vinext client Link navigation fails in production. */
export function RelatedLinks({ items }: { items: { label: string; href: string }[] }) {
  if (!items.length) return null;
  return (
    <div className="doc-status-row">
      {items.map((item) => (
        <a className="doc-chip" href={item.href} key={item.href}>
          {item.label}
        </a>
      ))}
    </div>
  );
}
