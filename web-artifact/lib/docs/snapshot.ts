import type { Snapshot } from './types';

export const SNAPSHOT: Snapshot = {
  reviewedOn: '14 Sep 2026',
  benchOn: '14 Sep 2026',
  sourceSha: '10660e9',
  repository: 'https://github.com/linggaaradhanas22/logic-analyzer',
  sourceBase: 'https://github.com/linggaaradhanas22/logic-analyzer/blob/10660e9',
  sourceTree: 'https://github.com/linggaaradhanas22/logic-analyzer/tree/10660e9',
};

export function sourceUrl(path: string): string {
  return `${SNAPSHOT.sourceBase}/${path}`;
}

export function treeUrl(path: string): string {
  return `${SNAPSHOT.sourceTree}/${path}`;
}
