import {
  AlertTriangle,
  Ban,
  CheckCircle2,
  CircleDashed,
  FlaskConical,
  MonitorCheck,
  Target,
  Timer,
  type LucideIcon,
} from 'lucide-react';
import { EVIDENCE_STATUS_LABELS, type EvidenceRef, type EvidenceStatus } from '@/lib/docs/types';

const ICONS: Record<EvidenceStatus, LucideIcon> = {
  'bench-verified': CheckCircle2,
  'target-tested': Target,
  'simulation-tested': FlaskConical,
  'host-tested': MonitorCheck,
  'implemented-unverified': CircleDashed,
  planned: Timer,
  'known-issue': AlertTriangle,
  blocked: Ban,
};

export function StatusBadge({ status, label }: { status: EvidenceStatus; label?: string }) {
  const Icon = ICONS[status];
  return (
    <span className="doc-badge" data-status={status}>
      <Icon aria-hidden="true" />
      {label ?? EVIDENCE_STATUS_LABELS[status]}
    </span>
  );
}

export function EvidenceList({ items, heading }: { items: EvidenceRef[]; heading?: string }) {
  return (
    <div className="doc-evidence">
      {heading ? <h3>{heading}</h3> : null}
      {items.map((item, index) => (
        <article className="doc-evidence-item" data-status={item.status} key={`${item.status}-${index}`}>
          <div className="doc-status-row">
            <StatusBadge status={item.status} />
            {item.date ? <span className="doc-date">{item.date}</span> : null}
          </div>
          <p>{item.summary}</p>
          <p className="doc-scope">{item.scope}</p>
        </article>
      ))}
    </div>
  );
}
