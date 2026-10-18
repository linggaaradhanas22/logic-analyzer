import type { Snapshot } from './types';

export const SNAPSHOT: Snapshot = {
  reviewedOn: '18 Oct 2026',
  benchOn: '18 Oct 2026',
  sourceSha: 'b933946',
  repository: 'https://github.com/linggaaradhanas22/logic-analyzer',
  sourceBase:
    'https://github.com/linggaaradhanas22/logic-analyzer/blob/b933946',
  sourceTree:
    'https://github.com/linggaaradhanas22/logic-analyzer/tree/b933946',
};

export function sourceUrl(path: string): string {
  return `${SNAPSHOT.sourceBase}/${path}`;
}

export function treeUrl(path: string): string {
  return `${SNAPSHOT.sourceTree}/${path}`;
}
