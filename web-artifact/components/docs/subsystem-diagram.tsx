import Link from 'next/link';

export type DiagramNode = {
  label: string;
  slug?: string;
  state?: 'verified' | 'planned' | 'issue';
};

export function SubsystemDiagram({
  nodes,
  caption,
  secondary,
}: {
  nodes: DiagramNode[];
  caption?: string;
  secondary?: string[];
}) {
  return (
    <div>
      <ol className="doc-flow">
        <li className="doc-flow-row">
          {nodes.map((node, index) => (
            <span key={`${node.label}-${index}`} style={{ display: 'inline-flex', alignItems: 'center', gap: 10 }}>
              {node.slug ? (
                <Link className="doc-flow-node" data-state={node.state} href={`/components/${node.slug}`}>
                  {node.label}
                </Link>
              ) : (
                <span className="doc-flow-node" data-state={node.state}>
                  {node.label}
                </span>
              )}
              {index < nodes.length - 1 ? (
                <span className="doc-flow-arrow" aria-hidden="true">
                  →
                </span>
              ) : null}
            </span>
          ))}
        </li>
      </ol>
      {secondary?.length ? (
        <div className="doc-flow-secondary">
          {secondary.map((line) => (
            <span key={line}>{line}</span>
          ))}
        </div>
      ) : null}
      {caption ? <p className="doc-flow-caption">{caption}</p> : null}
    </div>
  );
}
