import { COMPONENTS } from '../lib/docs/components';
import { COVERAGE } from '../lib/docs/coverage';
import { GLOSSARY } from '../lib/docs/glossary';
import { PROTOCOL_MESSAGES } from '../lib/docs/protocol';
import { REGISTERS } from '../lib/docs/registers';
import { ROADMAP } from '../lib/docs/roadmap';
import { SUBSYSTEMS } from '../lib/docs/subsystems';
import { validateDocs } from '../lib/docs/validate';

const errors = validateDocs();
if (errors.length) {
  console.error(`documentation validation FAILED (${errors.length}):`);
  for (const error of errors) console.error(`- ${error}`);
  process.exit(1);
}
console.log(
  [
    'documentation validation PASS:',
    `${SUBSYSTEMS.length} subsystems,`,
    `${COMPONENTS.length} components,`,
    `${REGISTERS.length} registers,`,
    `${PROTOCOL_MESSAGES.length} protocol messages,`,
    `${ROADMAP.length} roadmap phases,`,
    `${GLOSSARY.length} glossary terms,`,
    `${COVERAGE.length} coverage entries.`,
  ].join(' '),
);
