"""Publish the speaking script to the parent deliverables with stable names."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STAGING = ROOT / 'build' / 'presentation-script'
DELIVERABLES = ROOT.parent / 'deliverables'
REPORTS = ROOT / 'build' / 'reports' / 'presentation-delivery'
FILES = {
    'presentation-script.docx': 'presentation-script.docx',
    'presentation-script.pdf': 'presentation-script.pdf',
}
EXTRA = {'docs/presentation/presentation-script.md': 'presentation-script.md'}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_verified(source, destination):
    expected = sha256(source)
    with tempfile.NamedTemporaryFile(dir=destination.parent, prefix='.script-',
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
    return {'file': destination.name, 'bytes': destination.stat().st_size, 'sha256': expected}


def main():
    report = json.loads((STAGING / 'script-report.json').read_text(encoding='utf-8'))
    if report['slides'] != 37:
        raise RuntimeError('Script does not cover all 37 slides')
    for name in FILES:
        if not (STAGING / name).is_file():
            raise RuntimeError(f'Missing staged script file: {name}')
    DELIVERABLES.mkdir(exist_ok=True)
    REPORTS.mkdir(exist_ok=True)
    published = []
    for source_name, dest_name in FILES.items():
        published.append(replace_verified(STAGING / source_name, DELIVERABLES / dest_name))
    for source_name, dest_name in EXTRA.items():
        published.append(replace_verified(ROOT / source_name, DELIVERABLES / dest_name))
    shutil.copyfile(STAGING / 'script-report.json', REPORTS / 'script-report.json')
    (REPORTS / 'script-publication.json').write_text(json.dumps(
        {'outputDirectory': str(DELIVERABLES), 'files': published}, indent=2), encoding='utf-8')
    shutil.rmtree(STAGING)
    if STAGING.exists():
        raise RuntimeError('Script staging cleanup did not finish')
    print(f'Published {len(published)} script files to {DELIVERABLES}; hashes match; staging removed.')


if __name__ == '__main__':
    main()
