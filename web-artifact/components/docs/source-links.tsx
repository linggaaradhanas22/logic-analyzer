import { sourceUrl } from '@/lib/docs/snapshot';
import type { SourceRef } from '@/lib/docs/types';

const KIND_LABELS: Record<SourceRef['kind'], string> = {
  spec: 'spec',
  source: 'source',
  test: 'test',
  tool: 'tool',
  generated: 'generated',
  doc: 'doc',
};

export function SourceLinks({ sources }: { sources: SourceRef[] }) {
  return (
    <ul className="doc-source-list">
      {sources.map((source) => (
        <li key={`${source.kind}-${source.path}`}>
          <span className="kind">{KIND_LABELS[source.kind]}</span>
          <a href={sourceUrl(source.path)} target="_blank" rel="noreferrer">
            {source.label}
          </a>
          <span className="doc-inline-code">{source.path}</span>
          {source.note ? <span className="note">{source.note}</span> : null}
        </li>
      ))}
    </ul>
  );
}
