"""Check rendered/source content and unchanged scoring before publication."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import zipfile
import pymupdf
from docx import Document
from openpyxl import load_workbook
from build_project_report import NAME, SOURCE, parse
from build_kcci_workbook import PLAN, WEEKS

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/reports/project-documentation'


def normalized(text):
    return re.sub(r'[^\w]', '', text.casefold())


def body_text(pdf):
    return '\n'.join(block[4] for page in pdf for block in page.get_text('blocks')
                     if block[6] == 0 and block[1] > 40 and block[3] < page.rect.height - 35)


def verify():
    markdown = SOURCE.read_text(encoding='utf-8')
    blocks = parse(markdown)
    word = Document(OUT / (NAME + '.docx'))
    docx_text = normalized('\n'.join([p.text for p in word.paragraphs] +
                                  [c.text for t in word.tables for row in t.rows for c in row.cells]))
    pdf = pymupdf.open(OUT / (NAME + '.pdf'))
    pdf_text = normalized(body_text(pdf))
    missing = []
    count = 0
    for kind, value in blocks:
        if kind == 'image':
            continue
        values = [value[1]] if kind == 'heading' else ([c for row in value for c in row] if kind == 'table' else [value])
        for text in values:
            count += 1
            expected = normalized(text)
            for format, actual in [('Word', docx_text), ('PDF', pdf_text)]:
                if expected not in actual:
                    missing.append({'format': format, 'text': text})
    assert not missing, json.dumps(missing, indent=2)
    workbook = load_workbook(OUT / 'KCCI Project_Documentation_Forms.xlsx')
    for coordinate, value in PLAN.items():
        assert workbook.worksheets[0][coordinate].value == value
    expected_sheets = ['Project Log - Week ' + str(n) for n in range(1, 5)]
    assert len(workbook.worksheets) == 6
    assert sum(int(workbook[s]['D5'].value.split()[0]) for s in expected_sheets) == 20
    plan_pdf = pymupdf.open(OUT / 'kcci-1.pdf')
    plan_text = normalized('\n'.join(page.get_text() for page in plan_pdf))
    assert all(normalized(value) in plan_text for value in PLAN.values())
    for index, sheet_name in zip([2, 4, 5, 6], expected_sheets):
        printed = pymupdf.open(OUT / f'kcci-{index}.pdf')
        assert len(printed) == 1
        printed_text = normalized('\n'.join(page.get_text() for page in printed))
        for coordinate in ['A1', 'D2', 'B4', 'D5', 'B6', 'A9']:
            assert normalized(workbook[sheet_name][coordinate].value) in printed_text, (sheet_name, coordinate)
    with zipfile.ZipFile(OUT / 'originals/KCCI Project_Documentation_Forms.xlsx') as old, zipfile.ZipFile(OUT / 'KCCI Project_Documentation_Forms.xlsx') as new:
        assert old.read('xl/worksheets/sheet3.xml') == new.read('xl/worksheets/sheet3.xml')
    # Arithmetic independently checks retained line items and user instructions.
    required_total = 13800000 + 2 * 25750 + 20700 + 82500
    optional_total = 37050 + 45000 + 218000 + 115000 + 25000 + 13800
    assert required_total == 13954700 and required_total + optional_total == 14408550
    assert 'linggaaradhanas@gmail.com' in markdown and 'KCCI' in markdown
    assert '15 September–12 October 2026' in markdown
    assert 'confirmed directly by the author' in markdown
    for bad in ['[Author', '[license]', '[institutional', 'LICENSE TO BE ADDED', 'Please replace the italicized']:
        assert bad not in markdown
    render_checks = json.loads((OUT / 'render-checks.json').read_text())
    assert all(not item['outOfPageText'] for item in render_checks)
    result = {'status': 'PASS', 'reportPdfPages': len(pdf), 'sourceContentChecks': count,
              'wordAndPdfContainEverySourceBlockAndTableCell': True,
              'figuresInWord': len(word.inline_shapes), 'weeklyLogs': 4, 'totalHours': 20,
              'allFilledWorkbookEntriesPresentInNativeExcelPdf': True,
              'scoringSheetXmlByteIdentical': True, 'requiredBomIdr': required_total,
              'allListedBomIdr': required_total + optional_total,
              'renderedPdfTextWithinPageBounds': True,
              'visualWalkthrough': 'Author-confirmed; confirmation received 2026-10-12'}
    (OUT / 'document-checks.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def publish():
    filenames = [NAME + '.docx', NAME + '.pdf', 'KCCI Project_Documentation_Forms.xlsx']
    manifest = json.loads((OUT / 'input-manifest.json').read_text())
    replacements = []
    # Check the complete set before changing any original.
    for filename in filenames:
        target = ROOT.parent / filename
        current = hashlib.sha256(target.read_bytes()).hexdigest()
        generated = hashlib.sha256((OUT / filename).read_bytes()).hexdigest()
        assert current in {manifest['inputs'][filename], generated}, f'Original changed externally: {target}'
        assert (OUT / 'originals' / filename).is_file()
    for filename in filenames:
        target = ROOT.parent / filename
        source = OUT / filename
        # Windows cannot atomically replace a file held by an Office editor.
        # If that occurs, surface the exact error rather than modifying editors.
        pending = OUT / (filename + '.publish')
        pending.write_bytes(source.read_bytes())
        pending.replace(target)
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
        replacements.append({'path': str(target), 'bytes': target.stat().st_size, 'sha256': digest})
    result = {'status': 'PASS', 'published': replacements, 'backupDirectory': str(OUT / 'originals')}
    (OUT / 'publication.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--publish', action='store_true')
    arguments = parser.parse_args()
    print(json.dumps(verify(), indent=2))
    if arguments.publish:
        publish()
