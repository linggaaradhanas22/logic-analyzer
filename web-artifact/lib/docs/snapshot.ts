import type { Snapshot } from './types';

export const SNAPSHOT: Snapshot = {
  reviewedOn: '20 Sep 2026',
  benchOn: '20 Sep 2026',
  sourceSha: 'e829ead',
  repository: 'https://github.com/linggaaradhanas22/logic-analyzer',
  sourceBase: 'https://github.com/linggaaradhanas22/logic-analyzer/blob/e829ead',
  sourceTree: 'https://github.com/linggaaradhanas22/logic-analyzer/tree/e829ead',
};

export function sourceUrl(path: string): string {
  return `${SNAPSHOT.sourceBase}/${path}`;
}

export function treeUrl(path: string): string {
  return `${SNAPSHOT.sourceTree}/${path}`;
}
