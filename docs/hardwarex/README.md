# Current project report and KCCI forms

> **Handover status (18 October 2026):** the current report is the LaTeX
> HardwareX-style class report in `latex/`, compiled with the pinned Tectonic
> 0.17.0 workflow. Two PDF variants exist: Embedded (Lingga Aradhana Sahadewa)
> and FPGA (Lingga Aradhana Sahadewa, Alvin Hadar, Bagas Budhi Permana). The
> matching presentation and KCCI workbook were copied to both class delivery
> folders. The older Word/PDF/workbook under `../` and the 12 October packaged
> set remain historical and were not modified. For current firmware `0x000A0000`,
> UI `w11`, SD boot, live-waveform behavior and source/publication state, read
> [`../current-handover.md`](../current-handover.md) and [`../status.md`](../status.md).

`manuscript.md` is the report content source for the Embedded system and FPGA
classes; `latex/hardwarex-report.tex` is the current compiled report source.
`rtos-implementation.md` supplies the expanded code review. The earlier
wireless-first text is preserved in `manuscript-wireless-2026-09.md`.

Author: Lingga Aradhana Sahadewa. Class-project period: 15 September–12 October
2026. KCCI funded the project. The author confirmed completion of the visual
walkthrough on 12 October; the execution date was not specified.

## Outputs

The current compiled report PDFs are generated from `latex/`:

- `latex/hardwarex-embedded.pdf` — Embedded-system variant, Lingga only.
- `latex/hardwarex-fpga.pdf` — FPGA variant, all three authors.

Final class deliverables were copied to:

- `C:\Users\lingg\Projects\KCCI_technical\Embedded\Final Project Deliverables`
- `C:\Users\lingg\Projects\KCCI_technical\FPGA_Proj\Final Project Deliverables`

Each folder contains the matching LaTeX report PDF, presentation PPTX/PDF and
KCCI workbook. The report DOCX, presentation script and presenter notes were not
copied. The workbook has its implementation plan, four weekly activity sheets and
the untouched original scoring sheet. The FPGA variant lists Alvin Hadar and
Bagas Budhi Permana in the team table with RTL-only responsibilities; all other
work remains Lingga Aradhana Sahadewa's. The embedded variant keeps Lingga as the
sole member. The old publisher outputs under `../` remain historical, as does the
untouched `build/reports/project-documentation/originals/` backup.

## Regeneration

Scripts resolve the repository root from their own locations. Python
dependencies in the existing `.venv`: python-docx, openpyxl, lxml, PyMuPDF and
Pillow. Diagram rendering uses the installed Windows draw.io executable and
the draw.io skill's hardware-architecture preset/validator. PDF printing uses
the existing Playwright-core/Edge installation. Office checks use native Word
and Excel via PowerShell COM; no running user Office instance is closed.

From the repository root, the current LaTeX report and workbook variants are
rebuilt with:

```powershell
.venv\Scripts\python.exe tools\bootstrap_tectonic.py --directory .tools\tectonic
.venv\Scripts\python.exe tools\build_hardwarex_latex.py
.venv\Scripts\python.exe tools\validate_hardwarex_latex.py
.venv\Scripts\python.exe -X utf8 docs\hardwarex\build_kcci_workbook.py --variant both
```

`build_hardwarex_latex.py` compiles `hardwarex-embedded.tex` and
`hardwarex-fpga.tex`, publishes both PDFs beside the LaTeX source and records
`latex/latex_build_manifest.json`. `validate_hardwarex_latex.py` checks the
rendered A4 PDFs, author blocks, images, margins, fonts and compile logs, and
writes `latex/latex_validation.json`. The Tectonic binary is pinned by SHA-256
and cached under the ignored `.tools/tectonic/` directory.

The historical Word/PDF/Chromium workflow remains available for the preserved
12 October deliverables:

```powershell
.venv\Scripts\python.exe -X utf8 docs\hardwarex\build_report_diagrams.py
.venv\Scripts\python.exe -X utf8 docs\hardwarex\build_project_report.py
node docs\hardwarex\render_report_pdf.mjs
& docs\hardwarex\render_documents.ps1
.venv\Scripts\python.exe -X utf8 docs\hardwarex\inspect_rendered_documents.py
.venv\Scripts\python.exe -X utf8 docs\hardwarex\verify_project_documents.py
```

Those historical binaries are not modified by the LaTeX rebuild. The default
Office script checks Word opening/pagination and renders only the authorized
Excel sheets, read-only. Word native PDF export timed out locally for both
ExportAsFixedFormat and SaveAs2, so the historical delivered PDF was printed from
the same Markdown-derived content through Chromium. `-TryWordPdf` remains an
opt-in diagnostic, not a required build step.

## Preservation and verification

The workbook builder uses native OOXML edits rather than saving the whole
template through a library that could discard unsupported drawings. It changes
only the implementation/log worksheets and necessary workbook/style metadata,
appends weekly sheets and styles, and copies the scoring worksheet, original
style/font records, all original drawing/media parts and other package parts
unchanged. `workbook-checks.json` records the embedded variant and
`workbook-checks-fpga.json` records the FPGA variant; both retain the unchanged
scoring XML hash `f8cebe35...f7877`.

The LaTeX validator checks the rendered PDFs against the current source: A4
pages, embedded fonts, 1.5 cm margins, seven images (four diagrams and three real
UART screenshots), required HardwareX-style headings, the correct author block
for each variant, and no overfull boxes, missing glyphs, template instructions or
journal-only license/repository fields. The report content is 18 pages in each
variant. The current editable diagram source is
`diagrams/project-report_v2.drawio` with four pages; `assets/figures/` contains
the PNG copies used by LaTeX and `assets/uart-demo/` contains the real 18 October
2026 UART UI screenshots.

The historical 12 October `document-checks.json` and `publication.json` remain
valid for the preserved Word/PDF/workbook set. The original diagram and report
are historical, not current default diagrams. No report refresh is evidence of
new FPGA, firmware, protocol or bench testing. The user-confirmed walkthrough is
explicitly separate from machine-measured results. The current source is committed
with the 18 October project move and LaTeX/class-deliverable update; the
historical Word/PDF binaries remain unchanged.
