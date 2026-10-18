import type { Snapshot } from './types';

export const SNAPSHOT: Snapshot = {
  reviewedOn: '18 Oct 2026',
  benchOn: '18 Oct 2026',
  sourceSha: 'b487413',
  repository: 'https://github.com/linggaaradhanas22/logic-analyzer',
  sourceBase:
    'https://github.com/linggaaradhanas22/logic-analyzer/blob/b487413',
  sourceTree:
    'https://github.com/linggaaradhanas22/logic-analyzer/tree/b487413',
};

export function sourceUrl(path: string): string {
  return `${SNAPSHOT.sourceBase}/${path}`;
}

export function treeUrl(path: string): string {
  return `${SNAPSHOT.sourceTree}/${path}`;
}
