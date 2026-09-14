export type EvidenceStatus =
  | 'bench-verified'
  | 'target-tested'
  | 'simulation-tested'
  | 'host-tested'
  | 'implemented-unverified'
  | 'planned'
  | 'known-issue'
  | 'blocked';

export const EVIDENCE_STATUS_LABELS: Record<EvidenceStatus, string> = {
  'bench-verified': 'Bench verified',
  'target-tested': 'Target tested',
  'simulation-tested': 'Simulation tested',
  'host-tested': 'Host tested',
  'implemented-unverified': 'Implemented; target proof open',
  planned: 'Planned',
  'known-issue': 'Known issue',
  blocked: 'Blocked',
};

export type SourceKind = 'spec' | 'source' | 'test' | 'tool' | 'generated' | 'doc';

export type SourceRef = {
  label: string;
  path: string;
  kind: SourceKind;
  note?: string;
};

export type EvidenceRef = {
  status: EvidenceStatus;
  summary: string;
  scope: string;
  date?: string;
  sources: SourceRef[];
};

export type InterfaceItem = {
  name: string;
  direction: 'input' | 'output' | 'bidirectional' | 'internal';
  form: string;
  description: string;
};

export type SubsystemSlug =
  | 'physical-io'
  | 'fpga-capture'
  | 'dma-memory'
  | 'zynq-rtos'
  | 'protocol-link'
  | 'esp32-gateway'
  | 'host-tools';

export type ComponentDoc = {
  slug: string;
  name: string;
  codeName?: string;
  subsystem: SubsystemSlug;
  summary: string;
  why: string;
  upstream: string[];
  downstream: string[];
  inputs: InterfaceItem[];
  outputs: InterfaceItem[];
  steps: string[];
  state: string[];
  configuration: string[];
  failures: string[];
  ownership: string[];
  evidence: EvidenceRef[];
  sources: SourceRef[];
  related: string[];
};

export type SubsystemDoc = {
  slug: SubsystemSlug;
  order: number;
  name: string;
  responsibility: string;
  status: EvidenceRef[];
  beginner: [string, string];
  boundary: string;
  diagram: string[];
  secondary: string[];
  sequence: string[];
  inputs: InterfaceItem[];
  outputs: InterfaceItem[];
  decisions: string[];
  components: string[];
  related: string[];
  sources: SourceRef[];
};

export type RegisterFieldDoc = {
  name: string;
  lsb: number;
  width: number;
  values?: Record<number, string>;
};

export type RegisterDoc = {
  offset: string;
  name: string;
  access: 'RO' | 'RW' | 'W1C';
  reset: string;
  description: string;
  fields: RegisterFieldDoc[];
  constraints: string[];
  usedBy: string[];
};

export type ProtocolMessageDoc = {
  type: string;
  name: string;
  direction: string;
  group: 'discovery' | 'live' | 'state' | 'burst' | 'reliability';
  payload: string;
  support: string;
  supportStatus: EvidenceStatus;
};

export type RunnerTargetDoc = {
  target: string;
  validates: string;
  requires: string;
  report: string;
  evidence: string;
  status: EvidenceStatus;
};

export type RoadmapPhase = {
  id: string;
  name: string;
  outcome: string;
  status: EvidenceStatus;
  statusNote: string;
  evidence: string;
  remaining: string;
  subsystems: SubsystemSlug[];
};

export type GlossaryEntry = {
  term: string;
  definition: string;
};

export type CoverageEntry = {
  path: string;
  destination: string;
  note?: string;
};

export type PayloadFieldDoc = {
  field: string;
  bytes: string;
  meaning: string;
};

export type PayloadLayoutDoc = {
  name: string;
  total: string;
  fields: PayloadFieldDoc[];
  note: string;
};

export type ProtocolErrorDoc = {
  code: string;
  name: string;
  meaning: string;
  where: string;
};

export type DecoderDoc = {
  slug: 'uart' | 'spi' | 'i2c';
  name: string;
  configuration: string[];
  behavior: string[];
  events: string[];
  errors: string[];
  tests: string[];
  status: EvidenceRef;
};

export type RtlTestDoc = {
  block: string;
  testbench: string;
  covers: string;
};

export type TestLayerDoc = {
  layer: string;
  tool: string;
  target: string;
  boundary: string;
};

export type Snapshot = {
  reviewedOn: string;
  benchOn: string;
  sourceSha: string;
  repository: string;
  sourceBase: string;
  sourceTree: string;
};
