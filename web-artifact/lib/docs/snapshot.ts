import type { Snapshot } from './types';

export const SNAPSHOT: Snapshot = {
  reviewedOn: '22 Sep 2026',
  benchOn: '20 Sep 2026',
  sourceSha: '1bd7655',
  repository: 'https://github.com/linggaaradhanas22/logic-analyzer',
  sourceBase:
    'https://github.com/linggaaradhanas22/logic-analyzer/blob/1bd7655',
  sourceTree:
    'https://github.com/linggaaradhanas22/logic-analyzer/tree/1bd7655',
};

export function sourceUrl(path: string): string {
  return `${SNAPSHOT.sourceBase}/${path}`;
}

export function treeUrl(path: string): string {
  return `${SNAPSHOT.sourceTree}/${path}`;
}
