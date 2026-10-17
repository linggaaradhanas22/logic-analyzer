"""Publish stable presentation filenames, then remove presentation staging.

The user explicitly chose the parent deliverables directory. Only this
presentation's final files are replaced; validation reports stay in the repo.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import tempfile

from pptx import Presentation

ROOT = Path(__file__).resolve().parents[2]
STAGING = ROOT / 'build' / 'presentation'
DELIVERABLES = ROOT.parent / 'deliverables'
REPORTS = ROOT / 'build' / 'reports' / 'presentation-delivery'
FILES = {
    'logic-analyzer-design.pptx': 'logic-analyzer-design.pptx',
    'logic-analyzer-design-powerpoint.pdf': 'logic-analyzer-design.pdf',
    'presenter-notes.md': 'presenter-notes.md',
}
EVIDENCE = (
    'delivery-checks.json', 'render-report.json', 'ui-screenshot-evidence.json',
    'check-html.txt', 'check-pptx.txt', 'check-layout.txt', 'routes.json', 'storyline.md',
)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_verified(source, destination):
    """Replace a final output only after its complete copy has been verified."""
    expected = sha256(source)
    with tempfile.NamedTemporaryFile(dir=destination.parent, prefix='.presentation-',
                                     suffix='.tmp', delete=False) as temp:
        temporary = Path(temp.name)
    try:
        shutil.copyfile(source, temporary)
        if sha256(temporary) != expected:
            raise RuntimeError(f'Copy verification failed: {destination}')
        os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    if sha256(destination) != expected:
        raise RuntimeError(f'Published hash mismatch: {destination}')
    return {'file': destination.name, 'bytes': destination.stat().st_size,
            'sha256': expected}


def main():
    if not ROOT.parent.is_dir() or not (ROOT/'build/reports').is_dir():
        raise RuntimeError('Expected project parent/report directories are missing')
    if STAGING.is_symlink() or STAGING.resolve() != ROOT/'build/presentation':
        raise RuntimeError('Unexpected presentation staging location')
    checks = json.loads((STAGING/'delivery-checks.json').read_text(encoding='utf-8'))
    if checks['slides'] <= 0 or any(c['exitCode'] for c in checks['checks']):
        raise RuntimeError('The staging deck has not passed delivery checks')
    for source in (*FILES, *EVIDENCE):
        if not (STAGING/source).is_file():
            raise RuntimeError(f'Missing presentation artifact: {source}')
    deck = Presentation(STAGING/'logic-analyzer-design.pptx')
    if len(deck.slides) != checks['slides']:
        raise RuntimeError('Unexpected PowerPoint slide count')

    DELIVERABLES.mkdir(exist_ok=True)
    REPORTS.mkdir(exist_ok=True)
    replaced = [name for name in FILES.values() if (DELIVERABLES/name).is_file()]
    published = [replace_verified(STAGING/source, DELIVERABLES/destination)
                 for source,destination in FILES.items()]
    for filename in EVIDENCE:
        shutil.copyfile(STAGING/filename, REPORTS/filename)
    for check in checks['checks']:
        check['report'] = str((REPORTS/Path(check['report']).name).relative_to(ROOT)).replace('\\','/')
    (REPORTS/'delivery-checks.json').write_text(json.dumps(checks,indent=2),encoding='utf-8')
    report = {'outputDirectory': str(DELIVERABLES), 'overwriteExisting': True,
              'replacedExistingFiles': replaced,
              'slides': len(deck.slides), 'files': published,
              'stagingDirectory': str(STAGING), 'stagingRemoved': False}
    report_path = REPORTS/'publication.json'
    report_path.write_text(json.dumps(report,indent=2),encoding='utf-8')
    # Final files and validation evidence are verified before cleanup starts.
    shutil.rmtree(STAGING)
    if STAGING.exists():
        raise RuntimeError('Presentation staging cleanup did not finish')
    report['stagingRemoved'] = True
    report_path.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(f'Published {len(published)} files to {DELIVERABLES}; hashes match; staging removed.')


if __name__ == '__main__':
    main()
