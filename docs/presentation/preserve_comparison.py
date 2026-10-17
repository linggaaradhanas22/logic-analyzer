"""Preserve the user-requested 24-slide comparison deck exactly once."""
from pathlib import Path
import hashlib
import json
import shutil

from pptx import Presentation

ROOT = Path(__file__).resolve().parents[2]
DELIVERABLES = ROOT.parent/'deliverables'
COMPARISON = DELIVERABLES/'comparison'
FILES = ('logic-analyzer-design.pptx','logic-analyzer-design.pdf','presenter-notes.md')


def main():
    if not DELIVERABLES.is_dir():
        raise RuntimeError('Current deliverables are missing')
    if len(Presentation(DELIVERABLES/FILES[0]).slides) != 24:
        raise RuntimeError('The current deck is not the expected 24-slide comparison')
    COMPARISON.mkdir(exist_ok=True)
    evidence=[]
    for filename in FILES:
        src=DELIVERABLES/filename;dest=COMPARISON/filename
        expected=hashlib.sha256(src.read_bytes()).hexdigest()
        if dest.exists():
            if hashlib.sha256(dest.read_bytes()).hexdigest()!=expected:
                raise RuntimeError(f'Existing comparison differs; refusing to replace {dest}')
        else:
            shutil.copyfile(src,dest)
        assert hashlib.sha256(dest.read_bytes()).hexdigest()==expected
        evidence.append({'file':filename,'bytes':dest.stat().st_size,'sha256':expected})
    report=ROOT/'build/reports/presentation-delivery/comparison.json'
    assert report.parent.is_dir()
    report.write_text(json.dumps({'slides':24,'directory':str(COMPARISON),'files':evidence},indent=2),encoding='utf-8')
    print(f'Comparison preserved: 24 slides, {len(evidence)} hash-verified files in {COMPARISON}')


if __name__=='__main__':
    main()
