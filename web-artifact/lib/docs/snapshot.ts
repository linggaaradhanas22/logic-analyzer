import type { Snapshot } from './types';

export const SNAPSHOT: Snapshot = {
  reviewedOn: '14 Sep 2026',
  benchOn: '14 Sep 2026',
  sourceSha: '748d966',
  repository: 'https://github.com/linggaaradhanas22/logic-analyzer',
  sourceBase: 'https://github.com/linggaaradhanas22/logic-analyzer/blob/748d966',
  sourceTree: 'https://github.com/linggaaradhanas22/logic-analyzer/tree/748d966',
};

export function sourceUrl(path: string): string {
  return `${SNAPSHOT.sourceBase}/${path}`;
}

export function treeUrl(path: string): string {
  return `${SNAPSHOT.sourceTree}/${path}`;
}
