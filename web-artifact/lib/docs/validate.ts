import { COMPONENTS } from './components';
import { SUBSYSTEMS } from './subsystems';
import { COVERAGE } from './coverage';
import { EVIDENCE_STATUS_LABELS, type EvidenceRef } from './types';

const VALID_STATUSES = new Set(Object.keys(EVIDENCE_STATUS_LABELS));
const SUBSYSTEM_SLUGS = new Set(SUBSYSTEMS.map((item) => item.slug));

function isCovered(path: string): boolean {
  return COVERAGE.some((entry) => {
    if (entry.path === path) return true;
    if (entry.path.endsWith('/**')) {
      return path.startsWith(entry.path.slice(0, -2));
    }
    return false;
  });
}

function checkEvidence(errors: string[], where: string, evidence: EvidenceRef[]): void {
  if (!evidence.length) {
    errors.push(`${where}: no evidence item`);
  }
  for (const item of evidence) {
    if (!VALID_STATUSES.has(item.status)) {
      errors.push(`${where}: invalid status key ${item.status}`);
    }
    if (!item.summary || !item.scope) {
      errors.push(`${where}: evidence item missing summary or scope`);
    }
  }
}

function checkSources(errors: string[], where: string, paths: string[]): void {
  if (!paths.length) {
    errors.push(`${where}: no sources`);
  }
  for (const path of paths) {
    if (!isCovered(path)) {
      errors.push(`${where}: source not covered by the manifest: ${path}`);
    }
  }
}

export function validateDocs(): string[] {
  const errors: string[] = [];
  const seen = new Set<string>();

  for (const component of COMPONENTS) {
    const where = `component ${component.slug}`;
    if (seen.has(component.slug)) {
      errors.push(`duplicate component slug: ${component.slug}`);
    }
    seen.add(component.slug);
    if (!SUBSYSTEM_SLUGS.has(component.subsystem)) {
      errors.push(`${where}: unknown subsystem ${component.subsystem}`);
    }
    for (const field of ['summary', 'why'] as const) {
      if (!component[field]) errors.push(`${where}: empty ${field}`);
    }
    if (component.steps.length < 3) errors.push(`${where}: fewer than 3 steps`);
    if (!component.inputs.length) errors.push(`${where}: no inputs`);
    if (!component.outputs.length) errors.push(`${where}: no outputs`);
    if (!component.state.length) errors.push(`${where}: no state`);
    if (!component.configuration.length) errors.push(`${where}: no configuration`);
    if (!component.failures.length) errors.push(`${where}: no failures`);
    if (!component.ownership.length) errors.push(`${where}: no ownership`);
    checkEvidence(errors, where, component.evidence);
    checkSources(
      errors,
      where,
      component.sources.map((source) => source.path),
    );
    for (const related of component.related) {
      const route = related.startsWith('/');
      if (!route && !seen.has(related) && !COMPONENTS.some((item) => item.slug === related) && !SUBSYSTEM_SLUGS.has(related as never)) {
        errors.push(`${where}: broken related link ${related}`);
      }
    }
  }

  for (const subsystem of SUBSYSTEMS) {
    const where = `subsystem ${subsystem.slug}`;
    if (!subsystem.components.length) errors.push(`${where}: no components`);
    for (const slug of subsystem.components) {
      if (!seen.has(slug)) errors.push(`${where}: listed component missing: ${slug}`);
    }
    const actual = COMPONENTS.filter((item) => item.subsystem === subsystem.slug).map((item) => item.slug);
    for (const slug of actual) {
      if (!subsystem.components.includes(slug)) {
        errors.push(`${where}: component not listed in the subsystem index: ${slug}`);
      }
    }
    checkEvidence(errors, where, subsystem.status);
    checkSources(
      errors,
      where,
      subsystem.sources.map((source) => source.path),
    );
    for (const related of subsystem.related) {
      if (!related.startsWith('/')) {
        errors.push(`${where}: related link should be a route: ${related}`);
      }
    }
  }

  const coveragePaths = new Set<string>();
  for (const entry of COVERAGE) {
    if (coveragePaths.has(entry.path)) errors.push(`duplicate coverage path: ${entry.path}`);
    coveragePaths.add(entry.path);
    if (!entry.destination.startsWith('/') && !seen.has(entry.destination) && !SUBSYSTEM_SLUGS.has(entry.destination as never)) {
      errors.push(`coverage ${entry.path}: unknown destination ${entry.destination}`);
    }
  }

  return errors;
}

export function assertDocsValid(): void {
  const errors = validateDocs();
  if (errors.length) {
    throw new Error(`documentation content validation failed:\n- ${errors.join('\n- ')}`);
  }
}
