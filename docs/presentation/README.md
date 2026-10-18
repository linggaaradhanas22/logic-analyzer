# Final project design presentation

> **Handover status (18 October 2026):** the deck was rebuilt with the real
> UART demo capture on slide 34 and its decoding close-up on slide 35.
> Slide 4 includes the hardware photograph, and slide 35 links to the video
> demonstration. Both the PPTX and PDF companions retain the clickable link.
> Two variants exist: Embedded (Lingga Aradhana
> Sahadewa only) and FPGA (Lingga Aradhana Sahadewa, Alvin Hadar, Bagas Budhi
> Permana). Both keep all 38 slides and both custom shows. The internal
> `../deliverables/` copy is the Embedded variant; the FPGA variant is in the
> FPGA class delivery folder. Current firmware `0x000A0000`, UI `w11`, SD boot
> and publication state are in [`../current-handover.md`](../current-handover.md) and
> [`../status.md`](../status.md).

Purpose: explain the engineering problem, final direct-USB device, architecture,
FPGA/firmware responsibility boundary and verified results to a technical
final-project evaluation audience. English; approximately 20–25 minutes per
subsystem route plus Q&A.

The product described here is Zybo Z7-10 -> direct Zynq USB -> Windows host ->
local browser UI. The ESP32 is not part of this presentation's final system.
Presentation source is published in `b933946`. Evidence comes from dated
bench/build reports and the 18 October UART capture. The hardware photo and
demonstration link are included in both class versions. No new hardware test
is implied by the source publication.

The consulting-pptx-skill parts generator supplies the HTML foundation. The
content-adapted HTML is the layout source; PowerPoint/PDF are exported from the
same rendered layout. Final files live in `../deliverables/`, as requested by
the user. Temporary artifacts and previews use `build/presentation/`, which is
removed after a verified publication. Small verification reports are retained
in `build/reports/presentation-delivery/`. Presenter notes
and source references are retained in the notes. Per user revision, the visible
slides omit source lines, the repeated footer label and skill attribution;
body-page numbers remain.

The revised headings and slide explanations use plain English for presenting
aloud. Technical task names, signal names, register fields and measured values
are retained where needed to explain the implementation accurately.

Status: complete class deliverable. The user authorized source commit/push on
18 October 2026; final publication verification is recorded in `docs/status.md`.

## Deliverables

- `../deliverables/presentation-script.md` / `.docx` / `.pdf`: the complete
  slide-by-slide speaking script. It covers all 38 slides with spoken
  paragraphs, pointing cues, likely questions, and a term sheet; each slide is
  tagged with its route and skipped-slide notes. Source of truth:
  `docs/presentation/presentation-script.md`.
- `../deliverables/logic-analyzer-design.pptx`: 38 slides, editable text,
  embedded presenter notes; diagrams and UI screenshot are embedded images.
- `../deliverables/logic-analyzer-design.pdf`: native Microsoft
  PowerPoint render of that PPTX; preferred PDF companion.
- `../deliverables/presenter-notes.md`: talk track and source references.
- `review.md`: one independent review and finding-by-finding decisions.
- `subsystem-review.md`: the new subsystem deck's independent review and
  adopted corrections for black-box interfaces and stand-alone comprehension.
- `../deliverables/comparison/`: original 24-slide PPTX/PDF/notes, preserved
  byte-for-byte at the user's request. Later publications do not overwrite it.

The internal `../deliverables/` files were refreshed with the Embedded variant
on 18 October 2026. The FPGA variant (`logic-analyzer-design.pptx` and
`logic-analyzer-design.pdf`) was copied to
`C:\Users\lingg\Projects\KCCI_technical\FPGA_Proj\Final Project Deliverables`;
the Embedded variant was copied to the Embedded class folder. The speaking
script was regenerated for the current 38-slide deck. No `-revised`, dated or
numbered output copies are created. The parent directory is an explicit user-authorized exception to the
usual project-root output rule.

## Present either subsystem independently

| Section | Actual PowerPoint slide numbers |
|---|---|
| Shared overview | 1–6 |
| FPGA system | 7–18 |
| Embedded system | 19–33 |
| Shared results and conclusion | 34–38 |

- FPGA system route: **1–18, then 34–38**; 23 slides.
- Embedded system route: **1–6, then 19–38**; 26 slides.
- In PowerPoint choose **Slide Show > Custom Slide Show** and select
  **FPGA system** or **Embedded system**. Both named shows are embedded in the
  same PPTX. The menu rows link to the subsystem boundaries; the FPGA recap
  also links directly to shared results.
- Each section opens with an explicit black-box diagram. FPGA RTL and AXI DMA
  hardware belong to the FPGA explanation; FreeRTOS/DMA driver, ownership,
  C decoders and USB service belong to the embedded explanation.
- The slide-range references use actual PowerPoint positions. Printed
  body-page numbers exclude the unnumbered cover and back cover.

The slide 34 image is the real 18 October 2026 UART demo capture from firmware
`0x000A0000`: 10 MS/s, 64k raw, CH0 only, slot 0 `uart:rx=0,baud=115200`, decoded
bytes `4C 41 2D 53 33 0A` (`LA-S3` newline). It is the same UI used in the
HardwareX-style report, not the older synthetic offline illustration.

Slide 35 enlarges `uart-demo-frame.png` across the slide and maps each decoded
hexadecimal byte to its character: `4C 41 2D 53 33 0A` → `LA-S3` plus newline.
It includes the 115200-baud 8N1 settings and calculated bit timing (8.68 µs,
approximately 87 samples at 10 MS/s). Both custom shows include this result.
The speaking script was updated on 18 October for slide 35 and the current
38-slide deck; its Markdown, DOCX, and PDF deliverables match the published
PPTX and presenter notes.

The shared hardware photo is stored at
`docs/hardwarex/assets/logic-analyzer-hardware.jpeg` and appears on slide 4.
The demonstration URL is
`https://youtu.be/1xx84Ndi4vQ?si=ldsuYEWvYVqu2u6-`. The presentation exporter
preserves the HTML link as a PowerPoint hyperlink, and delivery checks verify
the target in both the PPTX and native PowerPoint PDF.

## Regenerate and verify

Requires the existing project host environment, `python-pptx==1.0.2`, the
consulting skill's installed Playwright/Chromium, and Microsoft PowerPoint for
native rendering. All scripts resolve the project root from their own path.

```powershell
$env:PLAYWRIGHT_MODULE_DIR = "$env:USERPROFILE\.config\opencode\skills\consulting-pptx-skill\node_modules\playwright"
$env:DECK_AUTHORS = "Lingga Aradhana Sahadewa"

# Speaking script (Markdown source -> DOCX + PDF -> deliverables)
.venv\Scripts\python.exe -X utf8 docs\presentation\build_script.py
node docs\presentation\render_script_pdf.mjs
.venv\Scripts\python.exe -X utf8 docs\presentation\publish_script.py

# Slide deck (Embedded variant)
.venv\Scripts\python.exe -X utf8 docs\presentation\capture_ui.py
.venv\Scripts\python.exe -X utf8 docs\presentation\create_deck.py
node docs\presentation\render_deck.mjs
.venv\Scripts\python.exe -X utf8 docs\presentation\export_pptx.py
& docs\presentation\render_powerpoint.ps1
.venv\Scripts\python.exe -X utf8 docs\presentation\check_delivery.py --check-only
.venv\Scripts\python.exe -X utf8 docs\presentation\check_delivery.py
```

For the FPGA variant, preserve the Embedded PPTX/PDF first, then rerun the
layout/export/render steps with the three-author environment:

```powershell
$env:DECK_AUTHORS = "Lingga Aradhana Sahadewa, Alvin Hadar, Bagas Budhi Permana"
.venv\Scripts\python.exe -X utf8 docs\presentation\create_deck.py
node docs\presentation\render_deck.mjs
.venv\Scripts\python.exe -X utf8 docs\presentation\export_pptx.py
& docs\presentation\render_powerpoint.ps1 -PdfName logic-analyzer-design-fpga.pdf -PreviewPrefix powerpoint-fpga
.venv\Scripts\python.exe -X utf8 docs\presentation\check_delivery.py --check-only
```

`check_delivery.py` verifies the real UART screenshot, the hardware photograph,
the decoded-frame close-up, and the exact demonstration URI in both the PPTX and
the native PowerPoint PDF. The two named custom shows contain 23 and 26 slides,
including the decoding close-up.

Inspect the native PowerPoint PDF and `build/presentation/previews/powerpoint-*.png`
slides after the check-only command and before running the final command. The
check-only stage also verifies the two independent custom shows, all requested
visible text removals and the comparison hashes. The final command checks the artifacts,
publishes the stable filenames to `../deliverables/`, verifies the copied
hashes, archives the small verification reports, and removes the temporary
presentation folder. The first screenshot command recreates staging on the
next update. Remaining checker warnings are reviewed in `docs/status.md`;
English uppercase labels and the unnumbered back cover are intentional.
