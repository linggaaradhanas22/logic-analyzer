import { componentBySlug } from './components';
import { subsystemBySlug } from './subsystems';

export type ResolvedLink = { label: string; href: string };

export function resolveLink(reference: string): ResolvedLink | null {
  if (reference.startsWith('/')) {
    const tail = reference.split('/').filter(Boolean).pop() ?? 'reference';
    const label = tail === 'registers' || tail === 'protocol' || tail === 'decoders' || tail === 'wiring'
      ? `Reference: ${tail}`
      : tail.replace(/-/g, ' ');
    return { label, href: reference };
  }
  const component = componentBySlug(reference);
  if (component) return { label: component.name, href: `/components/${component.slug}` };
  const subsystem = subsystemBySlug(reference);
  if (subsystem) return { label: subsystem.name, href: `/subsystems/${subsystem.slug}` };
  return null;
}

export function resolveLinks(references: string[]): ResolvedLink[] {
  return references
    .map(resolveLink)
    .filter((item): item is ResolvedLink => item !== null);
}
