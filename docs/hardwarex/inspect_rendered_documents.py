"""Render native Office PDFs to page previews and check printable content."""
from pathlib import Path
import json
import re
import pymupdf
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/reports/project-documentation'
PREVIEW = OUT / 'previews'
PREVIEW.mkdir(exist_ok=True)
checks = []
for pdf_path in sorted(OUT.glob('*.pdf')):
    doc = pymupdf.open(pdf_path)
    text = '\n'.join(page.get_text() for page in doc)
    (PREVIEW / (pdf_path.stem + '.txt')).write_text(text, encoding='utf-8')
    images = []
    outside = []
    for number, page in enumerate(doc, 1):
        pix = page.get_pixmap(matrix=pymupdf.Matrix(1.5, 1.5))
        filename = PREVIEW / f'{pdf_path.stem}-page-{number:02}.png'
        pix.save(filename)
        images.append(filename)
        for block in page.get_text('blocks'):
            if block[6] != 0:
                continue
            if block[0] < -1 or block[1] < -1 or block[2] > page.rect.width + 1 or block[3] > page.rect.height + 1:
                outside.append({'page': number, 'bbox': block[:4], 'text': block[4]})
    checks.append({'file': pdf_path.name, 'pages': len(doc), 'textCharacters': len(text),
                   'outOfPageText': outside, 'previews': [str(p) for p in images]})
    if not pdf_path.name.startswith('kcci-'):
        for group in range(0, len(images), 6):
            canvas = Image.new('RGB', (960, 1430), '#D8DEE3')
            draw = ImageDraw.Draw(canvas)
            for slot, path in enumerate(images[group:group + 6]):
                image = Image.open(path)
                image.thumbnail((450, 660))
                x = 15 + (slot % 2) * 480
                y = 30 + (slot // 2) * 470
                # Thumbnails fit a three-row contact sheet without overlap.
                image.thumbnail((450, 425))
                canvas.paste(image, (x, y))
                draw.text((x, y - 20), f'Page {group + slot + 1}', fill='#173A56')
            canvas.save(PREVIEW / f'report-contact-{group // 6 + 1}.png')
workbook_pages = [c for c in checks if c['file'].startswith('kcci-')]
if workbook_pages:
    canvas = Image.new('RGB', (1500, 1300), '#D8DEE3')
    draw = ImageDraw.Draw(canvas)
    for slot, check in enumerate(workbook_pages):
        image = Image.open(check['previews'][0])
        image.thumbnail((480, 590))
        x, y = 10 + (slot % 3) * 500, 30 + (slot // 3) * 650
        canvas.paste(image, (x, y))
        draw.text((x, y - 20), check['file'], fill='#173A56')
    canvas.save(PREVIEW / 'workbook-contact.png')
(OUT / 'render-checks.json').write_text(json.dumps(checks, indent=2), encoding='utf-8')
print(json.dumps([{k: v for k, v in c.items() if k != 'previews'} for c in checks], indent=2))
