"""Fill KCCI project forms by editing only authorized OOXML package parts.

The original scoring worksheet and all existing drawing/media parts are copied
byte-for-byte. Native XML editing also preserves shapes unsupported by openpyxl.
"""
from copy import deepcopy
from pathlib import Path
import hashlib
import json
import re
import zipfile
from lxml import etree as ET
from openpyxl import load_workbook

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/reports/project-documentation'
SOURCE = ROOT.parent / 'KCCI Project_Documentation_Forms.xlsx'
BACKUP = OUT / 'originals' / SOURCE.name
DEST = OUT / 'KCCI Project_Documentation_Forms.xlsx'
NS = 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'
REL = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
PKG = 'http://schemas.openxmlformats.org/package/2006/relationships'
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'

PLAN = {
    'B3': 'Logic Analyzer with Live Protocol Decoding',
    'B4': 'An individual project for the Embedded system and FPGA classes. Eight 3.3 V channels are sampled at up to 100 MS/s by the Zybo Z7-10 FPGA; FreeRTOS manages DMA, live UART/SPI/I2C decoding and direct USB delivery to a PC browser workspace.',
    'B5': 'Digital waveform and protocol inspection helps diagnose bus timing, framing and handshake faults. The project applies FPGA acquisition and embedded RTOS design together, with source-backed verification and a usable capture/live interface. Funded by KCCI.',
    'B6': '15 September–12 October 2026 (4 weeks; 5 hours/week; 20 hours total for both classes)',
    'B9': 'Individual developer\nFPGA + Embedded system',
    'C9': 'Lingga Aradhana Sahadewa',
    'D9': 'Sole member: RTL and register interfaces; FreeRTOS tasks, DMA and USB; protocol decoders; host/UI integration; validation, diagrams and documentation.',
    'B14': 'FPGA: synchronization, sample divider, triggers/pre-trigger ring, raw/transition packing, AXI interfaces and procedural Questa tests.\nEmbedded: FreeRTOS on CPU0 (8 application tasks, 1 kHz tick); queues/notifications, static pools and SG ownership; USB CDC-ACM and live decoders.\nHost: Python/JavaScript browser UI, waveform/cursors, SSE live events and exports.',
    'B15': 'SystemVerilog and FPGA timing; ARM C/FreeRTOS scheduling and synchronization; AXI DMA/cache ownership; USB/framing/CRC; UART/SPI/I2C; Python/JavaScript and evidence-based debugging.',
    'B16': 'Integrated burst capture and live protocol analysis for embedded debugging and teaching. Recorded results: 100 MS/s acquisition, 1.62 MB/s aggregate USB drain, clean 60 s UART/SSE gate and verified SD boot. Visual walkthrough completed (author-confirmed).',
    'B17': 'FPGA RTL/testbenches and bitstream/XSA; FreeRTOS firmware/BOOT.BIN; host UI/CLI; captures/exports and verification evidence; editable draw.io figures; current Word/PDF/Markdown report and four weekly KCCI logs.'
}

WEEKS = [
    ('15–21 September 2026', '21-09-2026', [
        'Investigated the gateway and ESP32-S3 transport limitations and prepared the direct-Zynq-USB migration plan (17 September).',
        'Implemented CDC-ACM, used numbered records/traces to investigate packet loss, and corrected endpoint setup/toggle behavior and transmit ownership/order handling (18–19 September).',
        'Verified three aggregate 64 MiB drain trials at 1.62 MB/s with byte-exact data and CRC checks (19 September).',
        'Adopted USB as the default and integrated device live events, host SSE, per-slot decoder controls and the unified waveform UI; recorded USB live/UI bench gates (20 September).',
        'Class link: FPGA sampling/packing and AXI contract; Embedded system DMA, USB, RTOS coordination and decoder integration.'
    ]),
    ('22–28 September 2026', '28-09-2026', [
        'Updated the direct-USB, UI-first operator runbook with power/jumper settings, COM identity, internal self-test, capture/live decoding, exports and troubleshooting (22 September).',
        'Added and smoke-tested hidden Windows UI start/stop launchers for easier operation (26 September).',
        'Cold-booted the USB-profile BOOT.BIN from SD; verified enumeration, ping/status and the 6/6 real-device UI E2E cases (26 September).',
        'Class link: FPGA/firmware image integration and startup; Embedded system host connection, service lifecycle and operator usability.'
    ]),
    ('29 September–5 October 2026', '05-10-2026', [
        'Corrected step-pattern division so the counter self-test remains legible in the Fit view (1 October).',
        'Changed waveform rendering to horizontal runs and vertical edges, and implemented exact forward/back cursor edge stepping (1 October).',
        'Verified the recorded host regression (167 passed), JavaScript page checks and offline browser behavior with zero page/console errors; no new bench result is claimed for that run (1 October).',
        'Class link: accurate presentation of FPGA-generated digital waveforms; Embedded system host controls, data interpretation and regression debugging.'
    ]),
    ('6–12 October 2026', '12-10-2026', [
        'Prepared and refined the final presentation with independent FPGA system and Embedded system routes and editable PowerPoint/PDF outputs (12 October).',
        'Prepared a complete slide-by-slide speaking script and its Word/PDF/Markdown exports (12 October).',
        'Reviewed the current source and dated reports, updated the project report, explained the actual RTOS tasks/ownership/queues/USB flow, and prepared current draw.io figures and weekly KCCI forms (12 October).',
        'Recorded the sole author\'s confirmation that the visual walkthrough is complete; its execution date and additional measurements were not specified (confirmation received 12 October).',
        'Class link: FPGA architecture/results and embedded RTOS implementation explained through one individual project.'
    ])
]


def q(name):
    return f'{{{NS}}}{name}'


def xml_bytes(root):
    return ET.tostring(root, encoding='utf-8', xml_declaration=True)


def set_cell(sheet, coordinate, text, style):
    data = sheet.find(q('sheetData'))
    number = int(re.search(r'\d+', coordinate).group())
    row = next((r for r in data if int(r.get('r')) == number), None)
    if row is None:
        row = ET.Element(q('row'), r=str(number))
        next_row = next((i for i, r in enumerate(data) if int(r.get('r')) > number), len(data))
        data.insert(next_row, row)
    cell = next((c for c in row if c.get('r') == coordinate), None)
    if cell is None:
        cell = ET.SubElement(row, q('c'), r=coordinate)
    for child in list(cell):
        cell.remove(child)
    cell.set('s', str(style))
    cell.set('t', 'inlineStr')
    string = ET.SubElement(cell, q('is'))
    value = ET.SubElement(string, q('t'), {'{http://www.w3.org/XML/1998/namespace}space': 'preserve'})
    value.text = text
    # Schema order for cells within an existing row must remain left-to-right.
    row[:] = sorted(row, key=lambda c: (len(re.sub(r'\d', '', c.get('r'))), c.get('r')))


def row_height(sheet, number, height):
    data = sheet.find(q('sheetData'))
    row = next((r for r in data if int(r.get('r')) == number), None)
    if row is None:
        row = ET.Element(q('row'), r=str(number))
        data.insert(next((i for i, r in enumerate(data) if int(r.get('r')) > number), len(data)), row)
    row.set('ht', str(height))
    row.set('customHeight', '1')


def build_variant(variant: str):
    assert variant in ('embedded', 'fpga')
    label = {'embedded': 'Embedded', 'fpga': 'FPGA'}[variant]
    dest = OUT / f'KCCI Project_Documentation_Forms_{label}.xlsx'
    plan_values = dict(PLAN)
    weeks = WEEKS
    member_line = 'Sole member: Lingga Aradhana Sahadewa'
    heights = {3: 28, 4: 57, 5: 57, 6: 30, 8: 24, 9: 76,
               10: 12, 11: 12, 12: 12, 13: 12, 14: 102, 15: 47, 16: 70, 17: 66}
    if variant == 'fpga':
        plan_values.update({
            'B9': 'Team lead\nFPGA + Embedded system',
            'C9': 'Lingga Aradhana Sahadewa',
            'D9': 'Team lead: RTL integration and register interfaces; FreeRTOS tasks, DMA and USB; protocol decoders; host/UI integration; validation, diagrams and documentation.',
            'B10': 'RTL design',
            'C10': 'Alvin Hadar',
            'D10': 'RTL design: capture FIFO/buffer and sample packing.',
            'B11': 'RTL design',
            'C11': 'Bagas Budhi Permana',
            'D11': 'RTL design: trigger/edge detection and AXI-Stream packet framing.',
        })
        weeks = [
            (period, date, [
                activity.replace("sole author's", "lead author's")
                for activity in activities
            ])
            for period, date, activities in WEEKS
        ]
        member_line = 'Team members: Lingga Aradhana Sahadewa, Alvin Hadar, Bagas Budhi Permana'
        heights.update({10: 36, 11: 36})
    assert OUT.is_dir()
    BACKUP.parent.mkdir(exist_ok=True)
    if not BACKUP.exists():
        BACKUP.write_bytes(SOURCE.read_bytes())
    with zipfile.ZipFile(BACKUP) as original:
        parts = {n: original.read(n) for n in original.namelist()}
    styles = ET.fromstring(parts['xl/styles.xml'])
    xfs = styles.find(q('cellXfs'))
    fonts = styles.find(q('fonts'))
    font = deepcopy(fonts[0])
    if font.find(q('sz')) is not None:
        font.find(q('sz')).set('val', '10')
    if font.find(q('b')) is not None:
        font.remove(font.find(q('b')))
    font_id = len(fonts)
    fonts.append(font)
    fonts.set('count', str(len(fonts)))
    style_cache = {}

    def wrap_style(base):
        if base not in style_cache:
            xf = deepcopy(xfs[base])
            xf.set('fontId', str(font_id))
            xf.set('applyFont', '1')
            alignment = xf.find(q('alignment'))
            if alignment is None:
                alignment = ET.SubElement(xf, q('alignment'))
            alignment.set('wrapText', '1')
            alignment.set('vertical', 'top')
            alignment.set('horizontal', 'left')
            xf.set('applyAlignment', '1')
            style_cache[base] = len(xfs)
            xfs.append(xf)
        return style_cache[base]

    baseline = load_workbook(BACKUP)
    plan = ET.fromstring(parts['xl/worksheets/sheet1.xml'])
    for coordinate, value in plan_values.items():
        set_cell(plan, coordinate, value, wrap_style(baseline.worksheets[0][coordinate].style_id))
    for row, height in heights.items():
        row_height(plan, row, height)
    parts['xl/worksheets/sheet1.xml'] = xml_bytes(plan)
    workbook = ET.fromstring(parts['xl/workbook.xml'])
    sheets = workbook.find(q('sheets'))
    sheets[1].set('name', 'Project Log - Week 1')
    # Scoring remains at its original index and original sheet ID/relationship.
    rels = ET.fromstring(parts['xl/_rels/workbook.xml.rels'])
    content = ET.fromstring(parts['[Content_Types].xml'])
    existing_ids = {r.get('Id') for r in rels}
    template = ET.fromstring(parts['xl/worksheets/sheet2.xml'])
    defined = workbook.find(q('definedNames'))
    area = next((n for n in defined if n.get('name') == '_xlnm.Print_Area' and n.get('localSheetId') == '1'), None)
    if area is not None:
        area.text = "'Project Log - Week 1'!$A$1:$D$10"

    for week, (period, date, activities) in enumerate(weeks, 1):
        sheet = deepcopy(template)
        values = {'A1': f'Project Log — Week {week}', 'D2': date,
                  'B4': 'Logic Analyzer with Live Protocol Decoding',
                  'D5': '5 hours', 'B6': f'Embedded system + FPGA | {period}',
                  'A9': f'Period: {period}\n{member_line}\n\n' +
                        '\n\n'.join(f'{i}. {a}' for i, a in enumerate(activities, 1)) +
                        '\n\nWeekly allocation: 5 hours. Activities summarized from the project ledger; dates are activity/evidence dates, not daily attendance.'}
        for coordinate, value in values.items():
            base_style = baseline.worksheets[1][coordinate].style_id
            set_cell(sheet, coordinate, value, base_style if coordinate == 'A1' else wrap_style(base_style))
        merges = sheet.find(q('mergeCells'))
        ET.SubElement(merges, q('mergeCell'), ref='A9:D9')
        merges.set('count', str(len(merges)))
        for row, height in {1: 32, 2: 24, 4: 47, 5: 27, 6: 39, 7: 25, 8: 8, 9: 365}.items():
            row_height(sheet, row, height)
        if week == 1:
            part = 'xl/worksheets/sheet2.xml'
        else:
            sid = week + 2
            part = f'xl/worksheets/sheet{sid}.xml'
            rid = f'rIdWeeklyLog{week}'
            assert rid not in existing_ids
            ET.SubElement(sheets, q('sheet'), name=f'Project Log - Week {week}',
                          sheetId=str(sid), attrib={f'{{{REL}}}id': rid})
            ET.SubElement(rels, f'{{{PKG}}}Relationship', Id=rid,
                          Type=REL + '/worksheet', Target=f'worksheets/sheet{sid}.xml')
            ET.SubElement(content, f'{{{CT}}}Override', PartName='/' + part,
                          ContentType='application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml')
            if area is not None:
                # Added sheets follow the original three sheets.
                new_area = deepcopy(area)
                new_area.set('localSheetId', str(week + 1))
                new_area.text = f"'Project Log - Week {week}'!$A$1:$D$10"
                defined.append(new_area)
        parts[part] = xml_bytes(sheet)
    xfs.set('count', str(len(xfs)))
    parts['xl/styles.xml'] = xml_bytes(styles)
    parts['xl/workbook.xml'] = xml_bytes(workbook)
    parts['xl/_rels/workbook.xml.rels'] = xml_bytes(rels)
    parts['[Content_Types].xml'] = xml_bytes(content)
    # Remove only stale calculation-chain metadata if one exists; this template has none.
    assert 'xl/calcChain.xml' not in parts
    with zipfile.ZipFile(dest, 'w', zipfile.ZIP_DEFLATED) as package:
        for name, data in parts.items():
            package.writestr(name, data)
    reopened = load_workbook(dest)
    assert reopened.sheetnames == ['Project Implementation Plan', 'Project Log - Week 1',
                                  'Project Scoring Sheet', 'Project Log - Week 2',
                                  'Project Log - Week 3', 'Project Log - Week 4']
    with zipfile.ZipFile(BACKUP) as old, zipfile.ZipFile(dest) as new:
        changed = [n for n in old.namelist() if old.read(n) != new.read(n)]
        allowed = {'xl/worksheets/sheet1.xml', 'xl/worksheets/sheet2.xml', 'xl/styles.xml',
                   'xl/workbook.xml', 'xl/_rels/workbook.xml.rels', '[Content_Types].xml'}
        assert set(changed) <= allowed, changed
        assert old.read('xl/worksheets/sheet3.xml') == new.read('xl/worksheets/sheet3.xml')
        old_xfs = ET.fromstring(old.read('xl/styles.xml')).find(q('cellXfs'))
        old_fonts = ET.fromstring(old.read('xl/styles.xml')).find(q('fonts'))
        assert all(ET.tostring(a) == ET.tostring(b) for a, b in zip(old_xfs, xfs))
        assert all(ET.tostring(a) == ET.tostring(b) for a, b in zip(old_fonts, fonts))
        score_hash = hashlib.sha256(new.read('xl/worksheets/sheet3.xml')).hexdigest()
    for ws in reopened:
        if ws.title.startswith('Project Log'):
            assert ws['D5'].value == '5 hours'
            assert ws['D4'].value is None and ws['B5'].value is None
    assert reopened.worksheets[0]['B7'].value is None
    if variant == 'fpga':
        assert reopened.worksheets[0]['C10'].value == 'Alvin Hadar'
        assert reopened.worksheets[0]['C11'].value == 'Bagas Budhi Permana'
        assert reopened.worksheets[0]['D9'].value.startswith('Team lead:')
        for ws in reopened:
            if ws.title.startswith('Project Log'):
                assert ('Team members: Lingga Aradhana Sahadewa, Alvin Hadar, '
                        'Bagas Budhi Permana') in ws['A9'].value
    report = {'status': 'PASS', 'variant': variant, 'sheets': reopened.sheetnames,
              'weeks': 4, 'hoursPerWeek': 5,
              'totalHours': 20, 'originalScoringSheetSha256': score_hash,
              'scoringSheetXmlByteIdentical': True, 'existingStyleAndFontRecordsUnchanged': True,
              'changedExistingPackageParts': changed,
              'allOtherOriginalPackagePartsByteIdentical': True, 'output': str(dest)}
    checks_name = 'workbook-checks.json' if variant == 'embedded' else f'workbook-checks-{variant}.json'
    (OUT / checks_name).write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=('embedded', 'fpga', 'both'),
                        default='both')
    args = parser.parse_args()
    variants = ('embedded', 'fpga') if args.variant == 'both' else (args.variant,)
    for variant in variants:
        build_variant(variant)


if __name__ == '__main__':
    main()
