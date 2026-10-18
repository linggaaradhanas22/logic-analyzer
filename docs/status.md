# Development Status and Handoff

This file is the execution tracker for the project. `docs/plan.md` remains the
normative implementation plan and design source of truth. Update this file at
the end of each development session so the next session can resume from the
same facts, evidence and blockers.

## Required Change Workflow

This workflow applies to every phase completion and every atomic tracked change
to source, tests, configuration, tooling or documentation. Build outputs,
reports, downloaded tools and captures remain ignored; summarize their results
here instead of committing them.

1. **Start synchronized.** Read `AGENTS.md`, `docs/plan.md` and this file. Run
   `git fetch origin`, `git status --short --branch` and
   `git log --oneline --decorate -5`. Work from `main`; if it is behind the
   remote and clean, fast-forward with `git pull --ff-only origin main`. Never
   reset or force-push to resolve divergence.
2. **Record the scope.** Update the current session log or phase row with the
   date, phase/task, intended files, acceptance checks and known blockers before
   making the change. Use `In progress` for work that spans sessions.
3. **Implement narrowly.** Keep the change atomic. Follow the project rules for
   native Windows paths, generated register files, ownership, DMA and tests.
   If work must pause, commit it as `WIP: ...`, mark it WIP here and list the
   exact continuation point rather than leaving an unexplained worktree.
4. **Verify and record evidence.** Run every relevant canonical target. Update
   this file with the exact command, PASS/FAIL/BLOCKED result, report path,
   relevant counts and remaining risks. A missing tool, XSA, board or hardware
   run is `BLOCKED` or `unverified`, never PASS.
5. **Review the commit set.** Run `git status --short --branch`,
   `git diff --check` and `git diff --stat`. Stage only intended files, then
   inspect `git diff --cached --stat` and `git diff --cached --name-only`.
   Confirm that no secret, local credential, generated environment, build
   output, log or capture is staged.
6. **Commit the atomic change.** Use a concise subject such as
   `P2: add trigger RTL` or `docs: update development status`. Include the
   status update in the same change. If the status must contain the new commit
   ID, add that ID in a follow-up status-only commit; do not leave the status
   update uncommitted.
7. **Push and verify.** Run `git push origin main`, then verify
   `git status --short --branch` is clean, `git log --oneline --decorate -2`
   shows the pushed commit and
   `git ls-remote origin refs/heads/main` matches `git rev-parse HEAD`.
   Record the pushed commit in the session log. If pushing fails, commit the
   local work, mark the push `BLOCKED` with the exact error and do not report
   the change as complete.
8. **Close the phase or change.** Set the phase/task to `Complete`, `WIP` or
   `Blocked`, list the next concrete action and leave no unexplained tracked
   files in the worktree. Re-read this document before the next task.

The definition of done is: relevant checks have evidence, blockers are honest,
the intended files and status are committed, `origin/main` contains the
commit, and the worktree is clean.

## Snapshot

### Project completion and source publication (2026-10-18, Complete)

**Project status: COMPLETE.** The delivered USB-first instrument and both
Embedded/FPGA class submissions are finished. All pending intended source was
committed and pushed as `b933946`; final publication evidence is below. Earlier
WIP entries retain their historical context and do not describe current status.

User authorizes committing and pushing all pending project work to origin/main
and marking the project done. Scope: W11 firmware/protocol/UI, ESP32 UART and
combined demos, runner relocation/setup fixes and tests, synchronized docs and
portal, report/presentation/workbook source revisions, and the hardware photo.
Generated environments, build outputs, captures, logs, compiled class artifacts,
and credentials remain excluded under the repository rules. Initial review:
main at 0b7271b; origin is the expected GitHub logic-analyzer repository;
`git fetch origin` PASS. Existing dated WIP entries below are historical and
will be superseded by this closeout. Completion applies to the delivered
USB-first class project. Optional gateway tuning, 200 MS/s, SUMP, public USB
identity, website deployment, and release tagging retain their documented scope.
Acceptance: source/change review, native/host/tooling/page/portal checks, current
report/delivery checks, completion/handover synchronization, intended-only commit,
push, remote hash match, and clean worktree. Next: finish review and run checks.

Closeout review/checks PASS: native 18/18 (`python tools/run.py test-native`),
ESP32 mocks 4/4 (`... test-esp32-mocks`), host 180/180 (`... test-host`), tooling
8/8 (`python -m unittest discover -s tests/tooling -v`), PAGE CHECK OK
(`node host/tests/page_check.mjs`), register generation/check (`... gen`), and
environment (`... check-env`). Commands used `.venv\Scripts\python.exe`.
Canonical reports are under `build/reports/{test-native,test-esp32-mocks,test-host,
gen,check-env}.json` and their logs; source audit:
`build/reports/closeout-source-audit.json` (81 intended files, no generated
artifact paths or credential-pattern findings). The demo queue's drop-newest
policy is now documented, and its README reflects the four-tab unified UI.
Portal validation PASS (7 subsystems, 79 components, 21 registers, 26 messages,
186 coverage entries). Initial `npm run validate` lacked an esbuild command
shim; prepending the existing pinned esbuild executable directory to the process
PATH resolved it without changing dependencies. Updated README, current handover,
plan, limits, presentation status, and portal closeout text for class-project
completion while retaining optional/deferred limitations. Next: final portal
build/lint, delivery validation, source commit/push, then pin portal source links
to that commit and record completion publication.

Final artifact/portal gates PASS: both report PDFs validate (19 pages/eight
images, class authors, hardware photo, two video links including page 2);
presentation delivery checks pass (38 slides, two custom shows, native renders,
photo/video URI, zero layout failures); both workbook variants retain four
one-page logs, all activity text, five hours/week, and scoring last. Evidence:
`docs/hardwarex/latex/latex_validation.json`,
`build/presentation/delivery-checks.json`, and
`build/reports/project-documentation/weekly-log-print-checks.json`.
Portal production build and changed-file lint PASS. Initial build/lint failed
because relocated `node_modules` junctions referenced missing package paths;
`npx --yes pnpm@10.28.2 install --frozen-lockfile --force` restored only ignored
dependencies. Package manifests/lockfile are unchanged. `npm run build`, targeted
`npm run lint -- <changed portal files>`, final validation, and
`git diff --check` now PASS. Source audit rechecked 83 intended files with no
generated-artifact or credential-pattern findings. No new RTL/bench measurement,
website deployment or release tag is claimed. Next: commit/push the full source
set, then pin the portal to that published source revision and close the ledger.

Source publication PASS: reviewed/staged exactly 83 intended files, with no
environment, build, log, capture or credential files. Commit **`b933946`**
(`W11: finalize waveform UI, demos and class deliverables`) pushed successfully
with `git push origin main`. `git ls-remote origin refs/heads/main` and
`git rev-parse HEAD` both returned
`b933946d3a139bff0feca849b5a688b66f1a72ba`; `git status --short --branch`
showed a clean synchronized main, and the decorated log confirmed origin/main.
This closeout metadata follow-up records that result and pins portal source
links to the published source commit. The handover and presentation README now
identify the published revision. No implementation work remains in class-project
scope; optional extensions/public product publication remain deferred as listed.

Closeout follow-up verification PASS: portal `npm run validate`, `npm run build`,
`npm run lint -- lib/docs/snapshot.ts`, and `git diff --check` after pinning
`b933946`. This follow-up contains only the ledger, handover, presentation README,
and portal snapshot. Push and clean-worktree/remote equality are checked again
after this commit; no further implementation or deliverable change is pending.

### Specifications-table video navigation (2026-10-18, verified and delivered; source WIP)

User requests a new YouTube-link row in both report specifications tables for
quick navigation. Added "Video demonstration" immediately after Hardware name
in `docs/hardwarex/latex/hardwarex-report.tex`, using the exact existing video
target. Updated `tools/validate_hardwarex_latex.py` to require the row and URI
on page 2 of each PDF. Preserve the demonstration-section links. Scope: shared
LaTeX, validator, report PDFs/manifests, both class-folder report copies, and this
ledger. Next: rebuild/validate, inspect page 2, and hash-verify delivery. Existing
WIP is preserved; no commit/push requested.

Verification/delivery PASS: `.venv\Scripts\python.exe
tools/build_hardwarex_latex.py` and `... tools/validate_hardwarex_latex.py`.
Both reports have 19 A4 pages, eight images, correct authors, no overfull text
or missing glyphs, and the exact clickable URI in the specifications table on
page 2. Both page-2 renders were visually reviewed:
`build/reports/specification-video-{embedded,fpga}.png`. Scoped
`git diff --check` PASS. `.venv\Scripts\python.exe
build/reports/publish_spec_link.py --deliver` replaced both class-folder report
PDFs and verified destination hashes. Evidence:
`build/reports/specification-video-delivery.json`; Embedded SHA-256
`5ed7f2d267c29d279c0c5bbf139a2fec4549ecd684d6427eda06c5da0b6fc7c3`;
FPGA `1e4a11a31908e3737f19e13d6869576bfb482259860fc3cec4543f19649a0790`.
No blockers remain. Source remains explicitly WIP without a commit/push request.
Next: user review of the quick-navigation link on page 2 of both reports.

### Hardware photo and demonstration video (2026-10-18, verified and delivered; source WIP)

User confirmed both class versions and placement: hardware photo in the report
Hardware description and PPT board-connections slide; clickable YouTube link
beside UART demonstration results. Source photo:
`C:\Users\lingg\Downloads\WhatsApp Image 2026-10-18 at 11.36.16.jpeg`, visually
confirmed against the attachment. Copy it to
`docs/hardwarex/assets/logic-analyzer-hardware.jpeg`; preserve the original.
Video target: `https://youtu.be/1xx84Ndi4vQ?si=ldsuYEWvYVqu2u6-`.
Scope: shared report/presentation sources, image/build inputs, hyperlink export
and validation, regenerated Embedded/FPGA report PDFs and PPTX/PDF companions,
class deliverable copies, presentation README, and this ledger. Acceptance:
correct image, working embedded URI links, author/custom-show preservation,
native renders/layout validation, and hash-verified delivery. Existing WIP is
preserved; no commit/push requested. Next: copy the photo, edit sources, and
build/check both author variants.

Source phase: copied the original JPEG to the agreed repository asset path.
Added a captioned hardware figure to the shared LaTeX report and the photo to
slide 4 without changing route positions. Added the supplied video target to
the report UART demonstration and slide 35. Extended HTML-to-PPTX export to
preserve anchor targets and added checks for the PPTX and native PDF URI.
Report checks now require the hardware image/caption and video link; its build
manifest includes the JPEG. Updated presentation documentation. Next: compile,
render, inspect image placement, and verify links in all six output documents.

Embedded/report verification: LaTeX build and validator PASS; Embedded report
18 pages and FPGA report 19 pages, each with eight images and the exact video
URI. Hardware-photo/report-video page PNGs in `build/reports/` were inspected.
Embedded presentation: 38 slides, correct author, 23/26-slide custom shows,
zero browser errors and geometry issues. Native slide 4 and 35 renders inspected.
First native PDF check exposed a missing URI despite a text-run PPTX hyperlink.
Added a text-box click action as well; rerender/check PASS. An attempted COM
ExportAsFixedFormat call failed argument binding and was reverted; the working
native SaveAs path is retained. Final `check_delivery.py --check-only` PASS,
including the exact link target in the saved PPTX and native PDF. Existing
uppercase/cover/back-cover warnings remain, with zero failures. Next: archive
and deliver Embedded, then build/check/deliver FPGA.

Final verification PASS for both author variants. Commands:
`.venv\Scripts\python.exe tools/build_hardwarex_latex.py`,
`.venv\Scripts\python.exe tools/validate_hardwarex_latex.py`, and the presentation
`create_deck.py`, `render_deck.mjs`, `export_pptx.py`, `render_powerpoint.ps1`,
`check_delivery.py --check-only` workflow (with each `DECK_AUTHORS` value and
the documented Playwright module path). Both PPTX/PDF pairs contain the correct
photo on slide 4 and exact clickable YouTube target on slide 35; all custom
shows/authors preserved. Report PDFs contain the photo in Hardware description
and the video beside the UART demonstration. Report build/validation manifests
are under `docs/hardwarex/latex/`; reviewed report PNGs under `build/reports/`.
Photo SHA-256 matches the Downloads original:
`694cfe060e5b70e9001ab6ceb160d3648ea8e0d496e93fb5251fb2d5c119dd3c`.

Delivery PASS: `.venv\Scripts\python.exe -X utf8
build/reports/publish_uart_closeup.py embedded` and `... fpga` replaced each
class folder's report PDF, PPTX, and presentation PDF and verified their hashes.
The FPGA presentation PDF was initially locked; after the user closed the PDFs,
retry succeeded. Internal `Logic-Analyzer/deliverables/` presentation PPTX/PDF/notes
also match Embedded. Reports with exact file hashes:
`build/reports/hardware-media-delivery-{embedded,fpga}.json`; archived native
slide 4/35 previews and check reports:
`build/presentation/variants/{embedded,fpga}/`. Scoped `git diff --check` PASS
(line-ending notices only). No blockers remain. No commit/push requested;
tracked changes remain explicitly source WIP. Next: user review of the updated
hardware photo and demonstration links in both class deliverables.

### KCCI weekly design/implementation logs (2026-10-18, verified and delivered; source WIP)

User confirmed: use the same integrated project progression in both Embedded
and FPGA workbooks, keep the existing dates and five hours per week, and move
Project Scoring Sheet to the last tab in both. Week 1 covers specifications,
architecture, and the implementation plan; Weeks 2–4 cover implementation and
verification details. Preserve each variant's members and responsibilities.
Scope: `docs/hardwarex/build_kcci_workbook.py`, this ledger, generated workbook
variants/check reports, and the two class deliverable XLSX copies. These are
phase-based activity summaries rather than the previous daily evidence timeline.
Acceptance: verify four log texts/dates/hours, final scoring tab, correct local
print-area sheet indices, unchanged scoring XML and original media/styles,
readable weekly print output, and hash-verified delivery. Existing report,
presentation, firmware and tooling WIP is preserved. No commit/push requested.
Next: rewrite the shared log content and reorder workbook tabs safely.

User expanded scope and confirmed the weekly-summary appendix in both
HardwareX class-report PDFs must match the revised workbook phases. Add
`docs/hardwarex/latex/hardwarex-report.tex`, both report builds/validation
records, and both class-folder report PDF copies to the intended changes.

Workbook build PASS: `.venv\Scripts\python.exe -X utf8
docs/hardwarex/build_kcci_workbook.py --variant both`. Both files now order the
implementation plan, Weeks 1–4, and scoring last. All dates/hours/activity texts
and local print-area indices pass checks. Scoring XML remains byte-identical
with SHA-256 `f8cebe3504d13fccb02fb9a27b029befd3a2e812520ac74856ae3375157f7877`.
Original fonts/styles/media remain preserved. Reports:
`build/reports/project-documentation/workbook-checks{,-fpga}.json`.
Next: native worksheet print review, matching report appendix, and delivery.

Print/report verification PASS: native Excel exported all four weekly logs for
both variants with `& build/reports/project-documentation/render_weekly_logs.ps1`.
Each log fits one page and contains all five activities and the five-hour footer.
Visual samples reviewed: FPGA Week 1 and Embedded Week 3. The matching report
appendix now describes requirements/architecture/plan, FPGA implementation,
firmware/host implementation, then integration/verification. Commands:
`.venv\Scripts\python.exe tools/build_hardwarex_latex.py` and
`.venv\Scripts\python.exe tools/validate_hardwarex_latex.py` PASS for both
18-page PDFs with no overfull text or missing glyphs. Both appendix page images
were reviewed. Additional checks:
`.venv\Scripts\python.exe -X utf8 build/reports/project-documentation/verify_and_deliver_logs.py`
PASS; report `weekly-log-print-checks.json` in the same directory. Existing
class workbooks were already Excel-resaved with scoring last; their package
part numbers differ from the original template. Verification resolves scoring
by worksheet name and confirms current form values and scoring cell formatting
match before replacement. No existing instructor/admin values will be lost.
Whitespace check PASS for builder, report source, and ledger. Next: deliver both
XLSX/report PDF pairs and verify hashes.

Delivery PASS: `.venv\Scripts\python.exe -X utf8
build/reports/project-documentation/verify_and_deliver_logs.py --deliver`
replaced `KCCI Project_Documentation_Forms.xlsx` and the class-specific
HardwareX report PDF in both `KCCI_technical/Embedded/Final Project Deliverables`
and `KCCI_technical/FPGA_Proj/Final Project Deliverables`. Dates, five hours/week,
member lists, administrative fields, and scoring contents remain preserved.
Both workbooks end with Project Scoring Sheet, and both report appendices match
the phase-based logs. Every destination hash matches its generated source.
Evidence: `build/reports/project-documentation/weekly-log-delivery.json`.
Embedded XLSX SHA-256:
`cfa383888b13f17d9f49865d2212f1941978dc30d2d2407d0def32c5608a9967`;
Embedded PDF `c9152d8a3f160c385abd30bdfa373e21f1fe0ec653a4920f516a3adbffb198c8`;
FPGA XLSX `b1573e917abe30966570931859cfb5e05c720bb089bdd674a2183a07a8fc2e46`;
FPGA PDF `9d0b433b06bc00cf85134aa022e93d640e5e0ed7704d7371bb08b022e0594a1d`.
No blockers remain. Tracked changes remain explicitly source WIP without a
commit/push request. Next: user/instructor review of the revised weekly logs.

### Combined ESP32 demo flash verification (2026-10-18, verified locally; source WIP)

User requests the same ready-to-copy setup verification for `demo-protocols`.
Shared runner fixes and its README commands are already in place. Scope:
run `python tools/run.py demo-esp32-protocols -- flash`, verify automatic port
selection and flash hashes, then inspect bounded serial startup/status output.
Only this ledger needs a tracked change unless verification exposes a defect.
Initial `git status --short --branch`: existing WIP preserved, main tracks
origin/main. Hardware GPIO waveforms and I2C loopback wiring are unverified.
No commit/push requested; source remains explicitly WIP. Next: flash the
connected ESP32-S3 and verify the combined demo startup.

Verification PASS: `python tools/run.py demo-esp32-protocols -- flash` builds
and selects COM10 automatically; bootloader, application and partition-table
flash hashes verified, followed by hard reset. Reports:
`build/reports/demo-esp32-protocols.{json,log}`. Bounded serial verification:
`.tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe -c` using
`serial.Serial('COM10',115200,timeout=5)` and `read(8192)` PASS, with an assertion
for `UART running batches=`. Observed `LA_PROTO_DEMO` UART counts 600 through
1000 and I2C successful transaction counts 234 through 390, with write `4C 41`
and read `41 33`. This confirms combined firmware execution and successful
on-board I2C traffic; SPI/electrical analyzer waveform verification remains
unverified. The ESP32 now runs the combined demo instead of the single-UART
demo. No further setup/source repair needed. Source remains explicitly WIP;
next: use the combined README wiring and analyzer settings for capture/live
decoding. Ledger whitespace check: `git diff --check -- docs/status.md`.

### ESP32 demo setup repair (2026-10-18, verified locally; source WIP)

Scope: repair the relocated project-local ESP-IDF tools; make `tools/run.py`
report environment-export errors and archive stale demo CMake workspaces;
add a canonical setup target and copyable demo README commands. Intended files:
`tools/run.py`, `fw-esp32/demo-{uart,protocols}/README.md`, and this ledger.
Acceptance: required tools install, UART and combined demo builds from a plain
Windows shell, relocation/error-path checks, and serial-port discovery before
any flash. Existing source changes are user WIP and remain preserved. Initial
checks: `git fetch origin` PASS; `git status --short --branch` and
`git log --oneline --decorate -5` show main at origin/main (0b7271b), with
pre-existing WIP. Diagnosis: CMake cache references the old root; IDF export
fails for missing RISC-V tools and dfu-util and the runner discards the error.
No commit/push requested; changes will remain explicitly WIP. Next: implement
the runner repair, install project-local tools, then build both demos.

Implementation: added `setup-esp32`, IDF-Python-based environment export with
actionable failures, and non-destructive stale-demo-workspace archiving in
`tools/run.py`. Both demo READMEs now use setup and port-auto-detected flash
commands. `python tools/run.py setup-esp32` PASS (114.875 s); evidence:
`build/reports/setup-esp32.{json,log}`. Port listing through the IDF Python PASS:
ESP32-S3 native USB is COM10 (`303A:1001`); Zybo is COM13 (`1209:0001`). First
UART build attempt exceeded the outer 120-second tool limit; no pass claimed.
Next: rerun with a longer limit and verify both builds and recovery paths.

Longer UART build exposed a second stale cache inside `bootloader/` even though
the top-level cache had already been regenerated by the earlier user attempts.
Build FAIL (not a toolchain failure); log: `build/reports/demo-esp32-uart.log`.
Extended relocation detection to check both application and bootloader caches.
Next: verify the real stale-bootloader recovery and clean builds.

Both `python tools/run.py demo-esp32-uart` and
`python tools/run.py demo-esp32-protocols` now PASS, including real relocation
archiving and complete bootloader rebuilds. UART `-- flash` PASS on COM10 with
flash hashes verified. Evidence: `build/reports/demo-esp32-{uart,protocols}.{json,log}`.
Espressif's generic auto-detection scanned Bluetooth and Zybo ports first;
added native Espressif USB preference for demo targets (explicit `-p`/ESPPORT
still wins), and documented it in both READMEs. Next: verify the final flash
path, regression checks, and a build from outside the project root.

Final verification PASS: `python -m unittest discover -s tests/tooling -v`
(8/8, new `tests/tooling/test_runner_idf.py` covers application/bootloader
relocation, fresh caches, export diagnostics/Windows PATH and native-port
selection); `python tools/run.py demo-esp32-uart -- flash` (automatic COM10,
all three flash hashes verified, no unrelated-port scan); and
`python C:/Users/lingg/Projects/Logic-Analyzer/Source_Code/tools/run.py demo-esp32-protocols`
from `fw-esp32/demo-uart/` (PASS, confirms runner paths are independent of caller
directory). Build/flash reports: `build/reports/demo-esp32-{uart,protocols}.{json,log}`;
setup evidence: `build/reports/setup-esp32.{json,log}`. `git diff --check --
tools/run.py docs/status.md` PASS (line-ending notice only).

Bounded IDF-Python pyserial read of COM10 confirms repeated
`LA_UART_DEMO: RUNNING batches=... TX=GPIO17 baud=115200`. The first assertion
expected a literal heartbeat label and failed despite valid RUNNING output;
corrected the verification assertion to the actual firmware status string,
then PASS. The board now runs the single-UART demo. Combined demo build is
verified; its flash and GPIO electrical waveforms were not checked in this
session. No local setup blockers remain. Source remains explicitly WIP with
the existing worktree changes; no commit or push requested. Next: user can copy
the updated README build/flash commands in a normal PowerShell window and
verify GPIO17 with the analyzer.

### Presentation decoding close-up (2026-10-18, verified and delivered; source WIP)

User requests a closer view of decoding results in addition to the full UI
screenshot. Scope: add a shared UART result slide immediately after slide 34,
using the real `uart-demo-frame.png` with a readable hex-to-text explanation.
Include it in both FPGA and Embedded custom shows and both author variants.
Intended files: presentation content/styles, route counts and checks, README,
this ledger, generated PPTX/PDF/notes and matching class-folder PPTX/PDF copies.
Acceptance: browser layout checks, native PowerPoint renders, custom-show
verification, visual review of the close-up, and hash-verified delivery. Preserve
the earlier report/W11 WIP. No commit/push requested. Next: implement slide 35
and update the two routes from 37 to 38 total slides.

Implementation phase: added slide 35 with the real full-width UART frame,
hexadecimal-to-character cards, 115200-baud 8N1 settings, and calculated bit
timing. The deck now has 38 slides; the FPGA custom show has 23 and Embedded has
26. Updated content/styles, route references, native render checks, delivery
checks, and README. Embedded build PASS with `create_deck.py`,
`render_deck.mjs`, `export_pptx.py`, `render_powerpoint.ps1`, and
`check_delivery.py --check-only` under `docs/presentation/`. Browser geometry
issues/page errors: zero. HTML/PPTX checks: zero failures; existing uppercase,
cover-line heuristic, and untitled back-cover warnings remain. Native slide 35
visually reviewed: waveform, six byte labels, settings and message are readable.
Next: archive/deliver Embedded, build/check FPGA, and verify its delivery.

Final verification/delivery PASS for both author variants. Commands for each:
`.venv\Scripts\python.exe -X utf8 docs/presentation/create_deck.py`,
`node docs/presentation/render_deck.mjs`,
`.venv\Scripts\python.exe -X utf8 docs/presentation/export_pptx.py`,
`& docs/presentation/render_powerpoint.ps1`, and
`.venv\Scripts\python.exe -X utf8 docs/presentation/check_delivery.py --check-only`.
Set `DECK_AUTHORS` to the appropriate author list and `PLAYWRIGHT_MODULE_DIR` as
documented in the presentation README. Both builds have 38 slides, correct
authors, 23/26-slide native custom shows including slide 35, zero browser errors
and zero geometry issues. Native PowerPoint close-up and title renders were
visually reviewed. `git diff --check -- docs/presentation docs/status.md` PASS.
Existing comparison files remain hash-identical. No new hardware run was needed.

`build/reports/publish_uart_closeup.py embedded` and `... fpga` (using the project
Python environment) archived checked variants and replaced the class-folder
`logic-analyzer-design.pptx` and `logic-analyzer-design.pdf`. The internal
`Logic-Analyzer/deliverables/` PPTX/PDF/notes were refreshed with Embedded.
Every copied artifact was SHA-256 verified. Reports:
`build/reports/presentation-closeup-{embedded,fpga}.json`; detailed checks and
review PNGs: `build/presentation/variants/{embedded,fpga}/`. Delivery hashes:
Embedded PPTX `bcf99d4f0d53677a3a1c31420742ddece2e6b6216160874b2bc886bb537a2a73`,
PDF `e742ee19079140fbb23d88ded1d627f8908870fb7dbc745c78ac3cee28e36b88`;
FPGA PPTX `801ed075ec6b3c5111555aba99f8ed7191bf723148a8c00fb37ab66ece0e6a87`,
PDF `bbb86428b858884763a13e4ef5df2c39ca15db8233d14b8aff945eb0aed0301a`.
No blockers remain. Source remains WIP without a commit/push request. Next:
presenter/user review of slide 35 in either class custom show.

### Instructor-facing HardwareX report revision (2026-10-18, verified and delivered; source WIP)

Scope: rewrite the shared LaTeX report for both Embedded and FPGA submissions.
Use clear academic prose, explain the RTOS mechanisms through the acquisition
workflow, and remove assistant-to-user commentary and literal instruction labels.
Preserve class-specific authors and RTL responsibilities. Intended files:
`docs/hardwarex/latex/hardwarex-report.tex`, report validator if its wording checks
need adjustment, both compiled PDFs and build/validation manifests. Acceptance:
build both variants, validate layout/authors/images, read the extracted PDF text,
inspect rendered pages, and refresh the matching class-folder report PDFs.
Verification commands and delivery paths will be recorded below. Existing W11
source changes are unrelated WIP and remain preserved. This revision is WIP;
no commit or push was requested.

Rewrite phase: updated `docs/hardwarex/latex/hardwarex-report.tex` throughout
the abstract, context, FPGA/DMA explanation, RTOS section, task/queue tables,
validation discussion, conclusion, contributions, and weekly summary. The RTOS
section explains preemption, blocked tasks, interrupt notifications, segment
references, queue overflow, semaphores, static storage, and cache invalidation,
with a UART data-flow example. Removed literal "Explicit RTOS" and development
handoff commentary. Class headings and credits now identify the correct
submission. `tools/validate_hardwarex_latex.py` checks the rewritten RTL credit
sentences. Verification pending: both report builds and PDF checks. Next: build,
review extracted text/layout, and refresh the two class-folder PDFs.

First build: `.venv\Scripts\python.exe tools/build_hardwarex_latex.py` compiled
both 18-page variants. PDF text/layout review found an overfull Priority header
and a floating RTOS figure interrupting the numbered workflow. Corrected the
header and kept the smaller RTOS figure before the sequence. Also simplified
the scope table, defined key interface terms, and corrected the UART caption
from "64k raw words" to the demonstrated 64,000 samples. Build/validation must
be repeated before delivery. No new hardware tests were run in this revision.

Final verification PASS:
`.venv\Scripts\python.exe tools/build_hardwarex_latex.py` and
`.venv\Scripts\python.exe tools/validate_hardwarex_latex.py`. Both PDFs have
18 A4 pages, seven images, embedded fonts, correct class-specific authors and
credits, and no overfull boxes or missing glyphs. Build/validation records:
`docs/hardwarex/latex/{latex_build_manifest,latex_validation}.json`; compile logs:
`build/latex-hardwarex/hardwarex-{embedded,fpga}.log`. Extracted text and page
images were inspected with the PDF reader, including the corrected task table
and uninterrupted UART workflow. The separate excerpt renderer was unavailable
because `pdftoppm` is missing; PDF-reader images provided visual review instead.
`git diff --check` PASS (line-ending notices only).

Delivery PASS after the user closed both locked destination PDFs. Replaced the
existing reports in `C:\Users\lingg\Projects\KCCI_technical\Embedded\Final Project
Deliverables\Logic Analyzer with Live Protocol Decoding_Lingga Aradhana Sahadewa.pdf`
and `C:\Users\lingg\Projects\KCCI_technical\FPGA_Proj\Final Project Deliverables\Logic
Analyzer with Live Protocol Decoding_Lingga Aradhana Sahadewa, Alvin Hadar, Bagas
Budhi Permana.pdf`. `Get-FileHash -Algorithm SHA256` confirms each destination
matches its compiled source: Embedded
`728f9db427bdf74b60c1827d44254defd34e629cb567a0fee49d835f59ef0dd2`;
FPGA `2367d7c6c0ca0bbc6ac618907883a525cd90524673bf9654f89e215e8681ec16`.
No delivery blocker remains. Source changes remain explicitly WIP without a
commit/push request. Next: instructor/user review of the revised reports.

**Current-state entry point:** [current-handover.md](current-handover.md),
updated 18 October 2026. The W11 SD image is installed and cold-boot verified;
JP5=SD, firmware 0x000A0000 / UI w11. Older session entries below retain the
state observed on their dates and do not override the current handover.

### Project relocation, LaTeX report, and class deliverable variants (2026-10-18, verified; committed)

User authorized: move `Logic-Analyzer` out of `KCCI_technical` to
`C:\Users\lingg\Projects\Logic-Analyzer`; rewrite the hardwarex document as a
LaTeX HardwareX-style class report with compiled PDF; add Alvin Hadar and Bagas
Budhi Permana alongside Lingga for the FPGA variant; replace the PPT synthetic
UI screenshot with the real UART demo capture; and populate both
`Final Project Deliverables` folders. Responsibilities: Lingga remains
responsible for all non-RTL work; Alvin has RTL capture FIFO/buffer and sample
packing; Bagas has RTL trigger/edge detection and AXI-Stream packet framing. The
team-name field remains blank and the scoring sheet is untouched. No report
DOCX, presentation script or presenter notes were copied to the class folders.

Move/verification PASS: the source folder moved to the new path; the `.venv`
console scripts were repaired with `pip install -e . --no-deps`;
`tools/check_env.py --write-lock` reported environment pass and regenerated
`tools/toolchain-lock.json`; absolute path references in `docs/plan.md`,
`logic-analyzer-implementation-plan-v0.4.md` and the status path table were
updated. Native 18/18, host 180/180, `node host/tests/page_check.mjs`,
`compileall`, and `tools/run.py gen` PASS. The old parent retains only an
ACL-protected `build/tmp/pytest-of-lingg` temporary directory that could not be
deleted without elevation; it is not part of or used by the relocated project.

LaTeX PASS: `docs/hardwarex/latex/hardwarex-report.tex` with
`hardwarex-embedded.tex` and `hardwarex-fpga.tex`; Tectonic 0.17.0 was
bootstrapped with the same SHA-256-pinned workflow used by the Hackathon project.
`tools/build_hardwarex_latex.py` and `tools/validate_hardwarex_latex.py` produce
and validate both PDFs. Both variants are 18 pages with seven images (four
diagrams and three real UART screenshots). Validation PASS: A4, embedded fonts,
1.5 cm margins, correct author block, required HardwareX-style headings, no
overfull boxes or missing glyphs, and no template instructions or journal-only
license/repository/OSHWA fields. The class-report LaTeX keeps the weekly
activity appendix, the BOM and the current diagrams; the report uses A4 and
removes the journal-only rows.

Presentation PASS: slide 34 now uses
`docs/hardwarex/assets/uart-demo/uart-demo-capture.png` with updated caption and
notes, replacing the synthetic offline illustration. The deck remains 37 slides
with both named custom shows (22 FPGA / 25 Embedded). Embedded and FPGA variants
were built with `DECK_AUTHORS`; the FPGA title slide lists Lingga, Alvin and
Bagas in that order, and PPTX core metadata matches each variant. Native
PowerPoint render and `check_delivery.py --check-only` PASS for both. The
internal `deliverables/` PPTX/PDF/notes were refreshed with the Embedded variant;
the comparison deck and the separate 12 October speaking script remain
untouched.

Workbook PASS: `docs/hardwarex/build_kcci_workbook.py --variant both`. The
Embedded variant keeps Lingga as sole member. The FPGA variant adds Alvin and
Bagas to the team table with RTL-only roles and changes each weekly log from
"Sole member" to "Team members: Lingga Aradhana Sahadewa, Alvin Hadar, Bagas
Budhi Permana"; the week-4 walkthrough line becomes "lead author's
confirmation". The scoring XML remains byte-identical
(`f8cebe3504d13fccb02fb9a27b029befd3a2e812520ac74856ae3375157f7877`) and original
fonts/styles/media are preserved. Outputs:
`build/reports/project-documentation/KCCI Project_Documentation_Forms_Embedded.xlsx`
and `..._FPGA.xlsx`, with `workbook-checks.json` and `workbook-checks-fpga.json`.

Class deliverables were hash-verified in
`KCCI_technical\Embedded\Final Project Deliverables` and
`KCCI_technical\FPGA_Proj\Final Project Deliverables`: matching LaTeX report
PDF, `logic-analyzer-design.pptx`, `logic-analyzer-design.pdf` and
`KCCI Project_Documentation_Forms.xlsx`. Embedded folders contain Lingga only;
FPGA folders add Alvin and Bagas. No report DOCX, script or notes were copied.
Source scope: relocated path documentation, LaTeX sources/scripts/assets,
presentation source changes, workbook builder, status and current handover.
Shortcut cleanup PASS: updated the OneDrive Desktop `LA UI.lnk` and
`Stop LA UI.lnk` arguments and working directory to the new project path, plus
the seven PulseView Start Menu shortcuts that pointed into the moved
`build/pulseview/` directory. All targets exist; the start/stop VBS launchers
resolve the project root from their own location. The UI was restarted
idle/disconnected on `127.0.0.1:8080` after the shortcut test. This shortcut and
status update is in a follow-up commit.

Pre-existing W11 source WIP outside this scope remains uncommitted and is
recorded here; no unrelated files were staged. Commit/push PASS: `1132e93`
(`docs: move project and add LaTeX report variants`) is on `origin/main`. The
remaining modified tracked files are the earlier W11/UI/demo source WIP and were
not part of this task's commit.

### UART demo UI screenshot set (2026-10-18, captured and verified; source WIP)

User requested stored UI screenshots of the logic analyzer configured for the
external UART demo, for later use in the report PDF and hardwarex document,
with no edit to either Word document in this step. Session start inspected
this ledger and `git status --short --branch`; earlier W11, UART/SPI/I2C demo,
report/presentation/learning WIP is preserved. Scope: capture real analyzer UI
views only, store reusable PNGs, and do not regenerate or modify the report,
workbook, deck or hardware documents.

Captured with a dedicated `UiServer` and headless Chromium against the running
bench: analyzer COM13 (firmware 0x000A0000 / PL 0x4C410002), ESP32-S3 UART
stimulus on GPIO17/CH0 at 115200 8N1, 10 MS/s, 64k raw, Pre 0, no trigger,
Patgen off, slot 0 `uart:rx=0,baud=115200`, channels 1-7 hidden. The ESP32-S3
was running the combined UART+SPI+I2C demo app; its UART payload and pin match
the UART-demo stimulus. Slots 1-2 were left empty (their light text is only the
UI placeholder hint). Real capture decoded six bytes `4C 41 2D 53 33 0A`
(`LA-S3\n`); the live session produced 1,464 events / 1,220 real waveform
packets with zero gaps and decoded the same frame. Zero page errors and both
byte-sequence checks passed in `build/reports/uart-demo-ui/screenshots.json`;
the UI was stopped and disconnected afterward. Evidence images were written to
the ignored `build/reports/uart-demo-ui/` and copied to
`docs/hardwarex/assets/uart-demo/`: `uart-demo-configured.png`,
`uart-demo-controls.png`, `uart-demo-capture.png`,
`uart-demo-capture-canvas.png`, `uart-demo-frame.png`,
`uart-demo-live.png`, `uart-demo-live-pane.png`.
No report/PDF/Word/workbook source or binary was edited; no commit or push was
requested. Next: embed the selected images in the manuscript/report when the
user authorizes the document update.

### W11 SD image and handover synchronization (2026-10-18, locally verified; source WIP)

User authorizes updating the SD-card image and synchronizing every active
handover/status document with the verified waveform-first UI, live raw preview
and Reset UI changes. Session start: ledger/plan/rules/worktree inspected,
`git fetch origin` PASS; HEAD and origin/main remain b487413. Earlier W11,
UART-demo, report/presentation/learning WIP is preserved. Scope: build/package
the USB-profile BOOT.BIN from the verified W11 ELF, install with byte/hash
verification on the user's identified card, cold-boot without JTAG, then update
current operator/development/architecture/limits/test/plan/UI/USB handoffs and
portal status metadata. Preserve dated historical evidence and report/deck
content; label superseded handoffs clearly and point to the current ledger.
Acceptance: image inputs/hash manifest, card read-back, firmware/PL identity,
SD-boot capture/live waveform evidence, documentation link/claim consistency
and applicable portal checks. Reports: build/reports/sd-w11/. No git commit,
push, website deployment or report/deck regeneration requested; source WIP.
Initial Windows volumes contain only internal OS/recovery partitions, including
an internal FAT32 MYASUS volume; no removable card is available. Asked user to
power down the Zybo and attach the actual microSD to a PC reader. Next: build
the image and identify all active status/handover entry points while awaiting
the card. Never infer an SD boot PASS from the previous JTAG test.

Image/package phase PASS: `.venv\Scripts\python.exe tools/run.py boot` and
`... tools/run.py package`. BOOT.BIN = 2,415,152 bytes, SHA-256
`6cb120ccc53c573edf9188a82d2fe1622def58836ffa46fdc40224eca847b892`;
staged la_fw.elf exactly matches the verified W11 ELF SHA-256
`f46389813150988c2104b2899a2f6dd1565a6f49572023d6f29ff4d515b55699`.
Updated host wheel 139,198 bytes and release-manifest.json generated. Previous
release artifacts preserved in build/reports/sd-w11/baseline/.

User attached the card: D: FAT32 removable, 15,923,150,848-byte volume on USB
disk 1 (serial 000000001532; not a system/boot disk). Its old BOOT.BIN matched
the known 0821afd9...f9619d image exactly. `build/reports/install_sd_w11.py`
backed it up as sd-w11/card-previous-BOOT.BIN, copied/flushed the new image via
a temporary file and atomically replaced BOOT.BIN; byte/SHA-256 read-back PASS.
Evidence: sd-w11/{preparation,card-installation}.json, canonical boot/package
reports and build/release/release-manifest.json. No format/partition change.
Cold boot still pending: user must safely eject, reinstall with JP5=SD and
power-cycle. Next: verify independent SD startup and finish document sync.

Cold-boot phase PASS: user safely returned the card, set JP5=SD and power-cycled.
No JTAG loader ran in this session. J10 enumerated on COM13 and `la-cli ping`
reported firmware 0x000A0000 / PL 0x4C410002 at uptime 17611 ms. SD image USB UI
suite PASS **6/6** in 11.35 s; independent browser/ESP32 verification PASS
**10/10**, zero page errors, including capture focus/Fit, real live waveform,
second-slot time alignment, Pause/Follow and Reset during streaming/armed
capture plus reconnect/Ping. Reports and screenshots archived under
build/reports/sd-w11/; sd-boot-verification.json records user-confirmed physical
boot basis, image/card hashes and checks. JP5 now remains SD; no JTAG reload
is needed after normal power cycling. Next: finish current-document/portal
handover consistency and record publication/source-WIP boundaries.

Documentation-sync phase PASS. Current entry point: new `docs/current-handover.md`
(bench state, hashes, W11 behavior, evidence boundaries, source/publication
state). Updated README, plan, operator guide, Windows guide, architecture, RTOS
design, live-web contract, limits, test plan, unified-UI plan, USB migration/
contract/pivot headers, historical v0.4 snapshot, report/presentation READMEs and
the historical web-artifact docs with current-status pointers. Synced portal
source: snapshot dates/baseline b487413 WIP notice, home/using status, roadmap,
verification runner/evidence rows, protocol frame bounds plus
WAVE_CFG/EVT_WAVEFORM, unified-UI component record and system diagram. Added
current JP5=SD/ESP32-demo bench notes to wiring. Preserved the dated 12 October
report/workbook/deck binaries and their claims; their READMEs explicitly
separate those deliverables from W11.
Checks: `.venv\Scripts\python.exe` local-link scan **LINKS OK**; portal
`validate-docs` PASS (7 subsystems, 79 components, 21 registers, **26 protocol
messages**, 11 roadmap phases, 55 glossary terms, 186 coverage entries);
`npm run build` PASS (all routes, client/RSC/SSR environments); targeted oxlint
PASS; touched portal source files normalized with the project's oxfmt formatter
(hence a wider formatting-only diff in those docs data files);
`git diff --check` PASS (existing line-ending notices only). No website deployment
or git commit/push was requested; `origin/main` remains b487413 and all
W11/demo/doc source is explicit WIP. Next: user uses the SD image/UI; publication or commit requires a separate
instruction. No further hardware rerun after the documented 6/6 + 10/10 gates.

### Combined UART+SPI+I²C demo and parallelism verification (2026-10-18, passed; source WIP)

User requests I²C and SPI demo code while keeping every demo pin on the same
DevKitC-1 J1 header row as GPIO17, and asks whether capture/live decoding can run
in parallel. Added `fw-esp32/demo-protocols/` (standalone ESP-IDF app) and the
`demo-esp32-protocols` runner target with isolated `build/esp32-protocols-demo/`
workspace. Pin map: UART GPIO17; SPI SCK/MOSI/CS GPIO12/11/10; I²C master
SCL/SDA GPIO9/8; I²C slave SCL/SDA GPIO4/5 with external 9↔4 and 8↔5 jumpers;
ground on J1/J3 to JC-5/11. All are 3.3 V J1-header pins. I²C uses two on-chip
controllers (master address target and slave at 0x50) so no sensor is required.
SPI 10 kHz mode 0 payload `4C 41`; I²C write `4C 41` then read `41 33`; UART
115200 `LA-S3\n`. Rates are deliberately conservative (UART 10 ms gap, SPI 5 ms,
I²C write/read 10 ms) to keep three live panes clean.

Build/flash: `.venv\Scripts\python.exe tools/run.py demo-esp32-protocols` PASS
(S3 image 0x404a0 bytes, 75% app partition free) and
`... demo-esp32-protocols -- -p COM10 flash` PASS with bootloader/app/partition
hash verification. ESP32 console confirms I²C loopback (`ok` increasing, write
`4C 41`, read `41 33`). The user powered both boards down, wired SPI/I²C and the
two I²C jumpers, then powered them back on.

Verification PASS (`build/reports/demo-protocols/verify_parallel.py`,
`verification.json`): one 10 MS/s 256k raw capture decoded all three slots:
UART 14 bytes beginning `4C 41 2D 53 33 0A`; SPI 20 events with CS assert/release
and repeated `4C 41`; I²C 21 events including START/ADDRESS(0x50)/ACK/BYTE/STOP
and read bytes `41 33`. Then all three live slots started together for four
seconds: **2,322 UART + 3,100 SPI + 3,121 I²C events**, 1,937 real waveform
packets and **zero** ring/client/format drops and zero preview omissions. The
hub was cleared before the live test so replay history could not be mistaken for
current loss. Parallelism limits recorded: all eight channels sample together;
up to three live decoder slots run concurrently; capture and live are mutually
exclusive; one capture job at a time; shared USB/UI throughput can still be
overloaded at higher rates. UI connection restored to idle/disconnected after
the test; source remains local WIP over b487413 with no commit/push requested.
Reports: `build/reports/demo-protocols/`; docs: `fw-esp32/demo-protocols/README.md`,
`docs/current-handover.md`.
Checks: `py_compile tools/run.py`, Markdown **LINKS OK**, and
`git diff --check` PASS (existing line-ending notices only). The analyzer UI was
left idle/disconnected after verification; source remains local WIP over
b487413 with no commit/push requested.

### Live pane blank / multi-tab feed fix (2026-10-18, passed; source WIP)

User reports live streaming counters increasing but no waveform in the pane.
Reproduced safely with a fresh temporary UI server: a single slot showed real
runs/events. Root cause is the unified UI's original single-client SSE policy
combined with `/api/state` polling: a stale or second browser tab could hold the
only `/events` feed while the visible tab still displayed HTTP-polled counters,
so the live panes received no data. This was not an FPGA/firmware waveform
failure.

Fix: `UiState` now allows up to four live browser tabs with fan-out
(`LiveEventHub(max_clients=4, client_queue=512)`), `hub.attach()` replays only
the newest queue-sized history instead of overfilling and counting replay as
loss, and `hub.begin_session()` clears stale event/waveform replay plus queued
frames and resets per-session counters at each live start. Legacy `la-web`
remains single-client by contract. The page now raises a warning if its SSE feed
disconnects. Tests added for newest-history replay, begin-session clearing,
multi-client fan-out, and legacy single-client 409 behavior.

Verification PASS: `.venv\Scripts\python.exe tools/run.py test-host` **180/180**
(41.88 s; existing warnings/pytest temp-link cleanup notice),
`node host\tests\page_check.mjs` PASS, JS/py compile checks PASS. Two independent
SSE readers against the restarted UI server each received **296 event messages
and 925 waveform messages** from the same live session:
`build/reports/demo-protocols/verify_two_feeds.py`. UI server restarted with the
fix; it is idle/disconnected for the user to reconnect. Source remains local WIP
over b487413, no commit/push requested.

### Waveform-first UI and reset (2026-10-17, locally verified; source WIP)

User authorizes readable initial captured waveforms, real live waveforms aligned
with decoding, and a Reset UI button restoring defaults. Session inspected
ledger/worktree and fetched origin; baseline remains b487413. Preserve earlier
demo/tooling and documentation WIP. Scope: renderer/default activity view,
non-overlapping annotations, bounded opt-in DMA-transition preview over direct
USB, host/SSE/browser integration, reset lifecycle, tests and operator docs.
Live preview will carry actual transition words with absolute sample indices;
missing/truncated preview ranges remain visibly unknown. Decode retains its
existing continuous pipeline. Reset stops acquisition, clears session memory
and browser settings, restores channel/form/view defaults and disconnects.
Acceptance: test-native/test-host, JS/real-browser checks and available USB
bench evidence, with unavailable hardware/tool steps recorded honestly.
Reports under build/reports/ui-waveform/. Source remains explicit WIP until
commit/push is requested. Next: implement capture/reset, then live transport.

Implementation phase WIP: captured view now has activity-focused startup,
connected high/low overview rails, higher rows and collision-free byte labels.
Reset endpoint/button stop work and clear store/hub/settings/view state. Added
opt-in WAVE_CFG 0x1C and EVT_WAVEFORM 0x25: bounded copies of DMA transition
words with sample rate, absolute start and omission count, reserved pool/queue
capacity for decoders, no ISR/SG mode changes. Host routes these separately
from decoded-event counts; per-slot live waveform rows share the decoder time
axis, retain bounded history and expose Pause/Follow/Fit/zoom and gaps. Native
and host regression additions cover payload validation, configuration guards,
callback routing, bounded fanout and reset. Initial JS syntax/page harness PASS.
Next: preserve the old in-memory capture/firmware, stop old UI to release the
native DLL, run canonical tests and rebuild firmware for actual bench checking.

First checks PASS: native 18/18, host 176/176 (existing matplotlib deprecation
warnings and a pytest temp-link cleanup PermissionError after the passing run),
USB firmware build PASS. Preserved prior capture + annotations and baseline
ELF/BOOT image in `build/reports/ui-waveform/baseline/`. Initial JTAG load blocked
by JP5=SD; user moved JP5 to JTAG and power-cycled, then jtag-zynq PASS. New
firmware 0x000A0000 loaded. First browser run found all-high input; after user
reseated/confirmed GPIO17/JC-1 + ground, real external browser run PASS: 18
capture bytes `LA-S3\n`, activity view span 5220/64000 samples, exact Fit, real
live run words + matching decoded bytes, Pause/Follow, reset while streaming,
zero page errors. Reports/screenshots: `build/reports/ui-waveform/browser-checks.json`
and capture-focus/capture-fit/live-waveform/reset-defaults PNGs.

Visual QA found excessive empty unconfigured live panes and stale event/status
labels under a continuously busy SSE feed. Hide unconfigured panes and also
refresh statistics from `/api/state`. Additional correctness hardening gives
each DMA segment an absolute acquisition timestamp so slots enabled mid-stream
or recovering from a dropped segment remain aligned; added native epoch/gap
tests and browser-model real-run/bounds checks. Reset now rejects an incomplete
stop, and disconnect disables the opt-in preview. Next: rerun affected checks,
reload final firmware and inspect final screenshots, including multi-slot live.

Final firmware native checks PASS 18/18 (including absolute-epoch/gap test),
USB build/reload PASS and canonical UI bench suite PASS 6/6 in 11.31 s
(`test-e2e -- --target serial://COM13 -k ui`, waveform SSE assertion included).
Final real-browser test at default 100 MS/s live PASS: capture activity/fit,
real signal and annotations, live status counters, Pause/Follow, matching
timestamps when a second UART slot starts mid-stream, and Reset while streaming;
zero page errors, zero client/format drops, 1322 waveform packets with no
omissions/gaps in the recorded observation. Inspected capture/live screenshots.

Reset hardening: capture cancellation now leaves serial reads in the capture
worker, which observes the cancel event then sends ABORT itself; avoids the
HTTP thread racing a second parser against capture. Added a cancellation +
post-cancel ping regression. Reset also clears job history; stale browser
upload/capture/decode/connect responses are generation-guarded. Pause now
freezes a bounded snapshot rather than drifting when rolling history expires.
Updated protocol contract and operator guide for W11, preview limits, actual
live rate, focus controls and Reset effects. Next: affected host/JS/cancellation
checks, final UI restart and runbook handoff; no source commit/push requested.

Close-out: final `python tools/run.py test-host` PASS **177/177** (41.86 s;
same pre-existing warnings/pytest temp-link cleanup issue). JS syntax checks
and `node host/tests/page_check.mjs` PASS, including real runs, gap boundaries,
frozen pause, reset clearing and long-idle two-second retention. `python -m
compileall -q host tools` and `git diff --check` PASS (line-ending notices only).
Final browser/real ESP32 trial PASS **10/10 checks**, including Reset while
armed waiting for a trigger and successful reconnect/Ping afterward; no page
errors. Observed 1312 waveform packets/7872 events with zero client/format
drops, preview omissions or gaps in that trial. Screenshots inspected at full
resolution. No new SPI/I2C external-preview or overload qualification claimed.

Changed scope: common/la_proto header/implementation; fw-zynq app context,
Acq/Cmd/decoder/Tx adapters and pure decoder core; host client/protocol,
live hub/session, UI server/render documentation and static app/index/timeline/
waveform; native protocol/dispatch/live tests, host client/API/live-waveform/page
tests and UI E2E assertions; docs/protocol.md, using-the-analyzer.md and ledger.
Prior UART-demo runner/source and report/presentation/learning WIP preserved.

Archived canonical native/host/build/load/UI-E2E reports plus browser screenshots
under `build/reports/ui-waveform/`; `finalize_checks.py` PASS and `summary.json`
records all canonical targets passing and running firmware 0x000A0000 / PL
0x4C410002. Loaded ELF SHA-256
`f46389813150988c2104b2899a2f6dd1565a6f49572023d6f29ff4d515b55699`.
Restarted the normal UI server on 127.0.0.1:8080 with current source, left it
idle/disconnected and released COM13 after identity verification. User should
hard-refresh the browser, select COM13 and Connect; Capture opens activity at
a readable scale, Apply live config -> Start shows waveform + bytes, Reset UI
restores defaults. **JP5 remains JTAG; this firmware is volatile.** SD BOOT.BIN
was not changed; after power loss reload via `python tools/run.py jtag-zynq`.
Functionality is ready locally. Tracked changes remain explicit WIP,
uncommitted/unpushed because publication was not requested. Next: user tries
the updated UI; future SD adoption/publication requires its own recorded step.

### External capture ARM acknowledgement failure (2026-10-17, recovered; WIP)

User reports UI `capture failed: DATA_END without an ARM acknowledgement` after
the external UART setup. Session start inspected ledger/worktree, fetched
origin and confirmed baseline `b487413`; earlier demo and documentation WIP are
preserved. UI `/api/state` confirms serial://COM13, matching firmware/PL IDs,
idle session and no captures. Scope: capture protocol diagnosis, bounded real
USB reproduction, evidence-backed recovery/fix and regression checks as needed.
Potential source files: host capture client/tests or firmware TX ordering only
if wire evidence justifies it. Diagnostic artifacts stay in `build/reports/`.
Current source shows host requires ARM ACK before DATA_END; firmware queues
the ACK before acquisition but TxTask starts jobs before draining control
frames, a possible ordering race, not yet established as the observed cause.
Next: temporarily release the UI port, trace a bounded capture, then reconnect
the UI. No commit/push requested; any tracked changes remain explicit WIP.

Diagnostic captures PASS after releasing/reopening COM13: 64,000 samples and
4,096 samples at 10 MS/s, external raw source, exact size/whole-capture CRC,
zero parser CRC/format/dropped-byte counts. Wire traces show CONFIG ACK then
the matching ARM ACK before DATA_BEGIN/chunks/DATA_END on both runs. Evidence:
`.venv\Scripts\python.exe build/reports/diagnose_capture_ack.py` and
`build/reports/capture-arm-diagnosis.json`. Restored UI serial://COM13 ownership.
Original error is not reproduced on these fresh connections; a stale-session/
lost-ACK explanation remains a hypothesis. No source fix is justified yet.
Next: verify repeated captures through the existing UI server and inspect CH0.

UI recovery verification PASS: three consecutive 64k raw external captures at
10 MS/s completed via `/api/capture`, exact 64,000 samples and no CRC/format/
dropped-byte errors. Evidence:
`.venv\Scripts\python.exe build/reports/verify_ui_capture_recovery.py` and
`build/reports/ui-capture-recovery.json`. First observer invocation had a local
job-response parsing KeyError (API returns the job directly); corrected the
ignored observer and reran, no product edit. Analyzer remains connected in UI
on COM13 and idle. Original missing-ACK cause remains unproven; reopening the
connection recovered the observed failure, with no protocol/firmware change.
No native/host regression required for this diagnosis-only session.

Separate physical-input finding: all three records contain 64,000 high CH0
samples and zero CH0 edges. Read-only COM10 console observation confirms the
S3 demo is still running: batches 107000/107500/108000, TX=GPIO17, 115200 baud.
Closed COM10 afterward. Thus UART stimulus software runs but the external
GPIO17-to-JC-1 signal is not observed; wiring/pin identification/common ground
and electrical output remain unverified. `git diff --check` PASS (existing
ledger line-ending notice). Only the ledger changed in tracked product scope;
earlier source WIP is preserved and no commit/push requested. Next: user refreshes
UI and retries Capture with Patgen off, then powers down to verify GPIO17/IO17
-> actual JC-1 (not JE/JD) and shared JC-5/11 ground if CH0 stays flat.

### External ESP32-S3 UART demonstration (2026-10-17, flashed and running; source WIP)

User authorizes programming the connected ESP32-S3 as the external signal source
before personally trying capture/live decode. Inspected this ledger, AGENTS.md,
plan and worktree; `git fetch origin` PASS and recent log confirms `b487413`
matches `origin/main`. Preserve the pre-existing modified ledger and untracked
hardwarex/learning/presentation work. Intended files: standalone ESP-IDF app
under `fw-esp32/demo-uart/`, canonical `demo-esp32-uart` runner target, and this
ledger. Build/configuration/artifacts stay under `build/esp32-uart-demo/`;
reports use `build/reports/demo-esp32-uart.{json,log}`. Acceptance: native S3
build, identified ESP32 programming port, flash hash verification and runtime
console showing the configured GPIO17/115200/8N1 repeated `LA-S3\n` workload.
Actual external waveform and capture/live decoding remain the user's bench
step, not an inferred PASS. No commit/push requested; source explicitly WIP.
Initial ports: ESP32 native gateway COM11 (`303A:4001`, `LA-GW-S3-01`), analyzer
COM13 (`1209:0001`), FTDI COM15 (`0403:6010`); no CH343/CP210x programming bridge.
Asked user to connect the UART-labelled port while the app is prepared. Next:
build the isolated source app and rescan for the programming bridge.

Implementation phase: added the standalone CMake/IDF app, SDK defaults, UART1
GPIO17 stimulus with a one-second UART0 heartbeat, README and canonical runner
target. The runner imports the installed IDF environment and places this app's
SDK configuration/build separately from the existing gateway workspace. No
protocol, decoder, RTL or Zynq behavior changed. Verification pending native
build/flash; source remains WIP.

Native build PASS: `.venv\Scripts\python.exe tools/run.py demo-esp32-uart`
(233.641 s, ESP-IDF 5.5.2, S3 image 0x328c0 bytes, 80% app partition free).
Evidence: `build/reports/demo-esp32-uart.{json,log}` and
`build/esp32-uart-demo/la_uart_demo.bin`. `git diff --check` PASS (existing
ledger line-ending notice). Repeated pyserial and present-device scans still
show COM11/COM13/COM15 only, no ESP32 UART bridge; no present PnP errors.
Flash/runtime verification BLOCKED awaiting the programming connection or
manual ROM-download entry. COM11 still advertises the existing gateway app;
no flash has been attempted and no runtime or GPIO waveform PASS is claimed.
Next: identify the ESP32 UART bridge after user connects it, then flash through
the canonical target and capture the boot/heartbeat. Source remains WIP.

User connected the UART cable, but pyserial still found no bridge. User then
performed BOOT/RESET on request: native gateway COM11 disappeared and S3 ROM
USB Serial/JTAG enumerated on COM10 (`303A:1001`, MAC E8:F6:0A:88:E1:B4).
Selected this positively identified S3 programming endpoint. Enabled IDF's
secondary USB Serial/JTAG console in demo defaults and the local SDK config so
runtime heartbeat can be verified over this working connection. README records
both programming paths. First combined flash invocation failed before hardware
access: IDF rejected `-p` after the `build` action. Fixed the new target to put
forwarded IDF global options/actions before its build action. Next: rerun
`.venv\Scripts\python.exe tools/run.py demo-esp32-uart -- -p COM10 flash`
and inspect runtime. Source remains WIP.

Flash PASS on COM10: the canonical command above rebuilt the USB-console image
(0x33400 = 209,920 bytes), identified ESP32-S3 revision v0.2 with 8 MB embedded
PSRAM and MAC e8:f6:0a:88:e1:b4, wrote bootloader/application/partition table,
verified all three hashes and issued hard reset. Evidence:
`build/reports/demo-esp32-uart.{json,log}`. Added an ignored bounded console
observer `build/reports/verify_esp32_uart_demo.py`; next: run it to check
increasing heartbeat counts and archive console/image hash evidence. Source WIP.

Runtime verification PASS after user pressed RESET/EN without BOOT. Initial
six-second observation had no output (unverified); after physical reset,
`.venv\Scripts\python.exe build/reports/verify_esp32_uart_demo.py` captured six
successive `LA_UART_DEMO: RUNNING` heartbeats, batches 26000/26500/27000/27500/
28000/28500 with TX=GPIO17 and baud=115200. Evidence:
`build/reports/esp32-uart-demo-{console.log,runtime.json}`. Final application
SHA-256 `6130151550383890055597fe74400d6376f868f26fe0d89b032f8893be971af2`;
flash target PASS in 114.203 s. `.venv\Scripts\python.exe -m py_compile
tools/run.py` PASS; `git diff --check` PASS with the existing ledger line-ending
notice. No new decoder/protocol/native-test gate required for this standalone
stimulus. Serial observer closed COM10; analyzer COM13 and the browser session
were not acquired or changed. Firmware is ready for the user's physical
capture: JC-1/CH0, 10 MS/s, 64k raw, Pre 0, no edge trigger, Patgen off;
`uart:rx=0,baud=115200`, expected `4C 41 2D 53 33 0A`. External waveform and
capture/live decoding remain unverified until the user performs them. The UART
bridge still does not enumerate; native USB programming/runtime worked. Source
and ledger remain explicit WIP, uncommitted/unpushed (not requested); preserve
all earlier work. Next: user Capture -> Decode capture, then Apply live config
-> Start in Slot 0 and observe the Live pane.

### Project report and KCCI workbook refresh (2026-10-12, published and verified; source WIP)

User authorizes completing the parent project PDF/Word report and KCCI Excel
forms, using the existing Markdown where appropriate and explicitly explaining
the RTOS from implementation evidence. Unknown personal, administrative,
authorship, cost and outcome entries require user confirmation; no invented
answers. Diagrams use the draw.io skill when needed. Session start: inspected
this ledger and `git status --short --branch`; `git fetch origin` PASS and
`git log --oneline --decorate -5` confirms `b487413` matches `origin/main`.
Pre-existing modified ledger and untracked `docs/hardwarex/`, `docs/learning/`
and `docs/presentation/` are preserved. Intended scope: refreshed report source,
document/workbook export tooling, source-backed RTOS explanation, deliverables
and this ledger. Generated working files remain beneath `build/`. Acceptance:
report/source cross-check, workbook field/formula/style preservation, PDF and
Word rendering, updated diagram inspection and explicit unknown-entry review.
No new bench results or commit/push requested; source remains explicit WIP.
Next: inspect the actual Word/PDF and all workbook sheets, compare historical
wireless manuscript with current USB implementation, then ask targeted questions.

Discovery completed locally. The original PDF has 15 pages; the matching Word
file has 142 paragraphs, three tables and one embedded diagram, with substantial
unfilled template instructions. `docs/hardwarex/manuscript.md` supplies most
technical sections but describes the historical wireless-first baseline and
still has author/license/affiliation/funding placeholders. The workbook has
three sheets: Project Implementation Plan, Project Log and Project Scoring
Sheet. All project-entry fields are blank; scoring/comment/signature fields
require evaluator information or an explicit instruction to leave them blank.

Read actual initialization, task constants/adapters, generated BSP scheduling
configuration and USB interrupt enable/service code. Authored the independent
technical draft `docs/hardwarex/rtos-implementation.md`: eight default-profile
application tasks, actual priorities/stacks, configurable decoder slots,
static allocation, handle/refcount ownership, queue-full behavior, burst buffer
lifetime, cached-DMA invalidation, USB semaphore/staging, tracing and honest
measurement scope. Corrects stale planning claims about task stacks, five-second
status cadence, a full-control-queue NAK guarantee, all-task watermark coverage,
CPU statistics and notification-only USB RX callbacks. Original documents,
historical manuscript and diagrams have not yet been modified.

Inspection command PASS: `.venv\Scripts\python.exe -X utf8
build\reports\inspect_project_documents.py`; reports under
`build/reports/project-documentation/{input-manifest.json,original-word.txt,
original-pdf.txt,original-workbook.json}`. The read-only Office/PDF parser
records original SHA-256 hashes. Installed `openpyxl 3.1.5` and `pymupdf 1.28.2`
in the existing project `.venv` for structured Office/PDF inspection/export.
`git diff --check` PASS (existing line-ending notice). PDF preview tool BLOCKED:
`ENOENT: uv_spawn 'pdftoppm'`; PyMuPDF is available for the rendering fallback.
No firmware, RTL, protocol, decoder or hardware test run for this documentation
draft. Scope remains WIP, uncommitted/unpushed. Next: obtain official duration,
team/member/adviser/institution details, author/contact/contribution/funding
information, log granularity/hours, evaluator-field policy, license/deposit
status and BOM price basis; then update the manuscript/diagrams and generate
verified Word/PDF/Excel deliverables.

User clarifications received 2026-10-12: one individual project serves both
Embedded system and FPGA classes; sole member/author/contributor Lingga Aradhana
Sahadewa, email `linggaaradhanas@gmail.com`. Official class-project duration
15 September–12 October 2026. Four separate weekly activity sheets, five hours
each (20 total, not doubled for the two classes). Leave team/adviser/institution
and other unrequested administrative fields blank. **Do not touch the Project
Scoring Sheet**, including its package XML/assets. KCCI funded the project;
retain the 17 September 2026 IDR price estimates. Omit optional license/deposit/
certification sections; do not claim a license or DOI. User confirms the visual
walkthrough is already complete; report this as user-confirmed on 12 October,
without inventing its execution date, detailed results or a machine report.
Next: author the USB-first report with class-specific contributions, weekly logs
from dated ledger evidence and current draw.io figures; preserve originals in
an ignored backup before replacing the user-requested Word/PDF/workbook files.

Report authoring phase WIP: preserved the earlier manuscript as
`docs/hardwarex/manuscript-wireless-2026-09.md`, authored the current
`manuscript.md` with both class responsibilities, confirmed author/funding/dates,
the implemented RTOS, current USB/UI/SD status, evidence boundaries and the
reclassified retained-price BOM (required USB subtotal Rp 13,954,700; all listed
items Rp 14,408,550). Added `build_report_diagrams.py`,
`build_project_report.py`, `build_kcci_workbook.py` and
`render_documents.ps1`. The workbook builder edits only authorized OOXML
parts, preserves scoring XML and original media/relationships verbatim, and
appends styles without changing existing font/style records. Four draw.io
pages generated as `diagrams/project-report_v2.drawio`; skill validation PASS
(0 errors, 0 warnings, score 0), using draw.io 31.4.5 at the resolved installed
path. First four PNGs inspected: found a return-command label overlapping the
USB lane and several overwide labels; shortened labels and moved the return
label into a clear corridor. Next: regenerate/inspect corrected figures, build
the workbook/report, use Word/Excel native PDF rendering and verify all pages
before publishing. No fresh hardware test or scoring-sheet edit.

Current report/weekly workbook staging PASS: `build_project_report.py`
generated a 183-word abstract, 149 Word paragraphs, 11 tables and four figures;
`build_kcci_workbook.py` generated six sheets (plan, original scoring and four
weekly logs), verified twenty total hours and byte-identical scoring XML
(SHA-256 `f8cebe35...f7877`). All other original package parts except the
explicit worksheet/workbook/style metadata allowlist are byte-identical;
original style/font records are retained. Corrected diagram rerender/inspection
PASS for all four native draw.io pages. Original binary backups are in
`build/reports/project-documentation/originals/`.

Office rendering: `& docs\hardwarex\render_documents.ps1 -SkipWord` PASS,
five authorized sheets each exported to one-page native Excel PDF; the scoring
sheet was skipped and the workbook closed without saving. Native Word
ExportAsFixedFormat timed out at 240 s, a diagnostic retry at 120 s, and SaveAs2
at 120 s; no native Word PDF success claimed. Only the hidden Word processes
created by those attempts were closed after checking PID/name/start time;
pre-existing Excel user instances were preserved. Word opening/repagination
verification later PASS with `-SkipExcel -WordOpenOnly` (15 pages). Added
`render_report_pdf.mjs`: Chromium/Edge PDF PASS (16 pages, four loaded figures,
zero page errors) from the same Markdown-derived content. Added PDF preview/
page-bound inspection and source/content/package verification/publication
tooling plus `docs/hardwarex/README.md`.

First visual QA: all report pages and five workbook pages inspected on contact
sheets; page-bound checks PASS. Found a too-narrow Hours column in the report's
weekly summary and lost weekly title styling. Fixed report column widths,
preserved explicit numbered lists and restored the weekly template title style.
Next: regenerate the affected artifacts, rerun native Office/open/render and
content checks, inspect final key pages, then atomically replace the requested
parent documents and verify hashes. Source/ledger remain explicit WIP; no
commit/push or new hardware run requested. The author's 12 October confirmation
closes the previously outstanding human visual walkthrough observation, with
execution date and extra detailed results left unspecified.

Final regeneration and publication PASS. Commands:
`.venv\Scripts\python.exe -X utf8 docs\hardwarex\build_project_report.py`,
`node docs\hardwarex\render_report_pdf.mjs`,
`.venv\Scripts\python.exe -X utf8 docs\hardwarex\build_kcci_workbook.py`,
`& docs\hardwarex\render_documents.ps1` (Word open/pagination and five native
Excel PDFs, no Word PDF-export attempt),
`.venv\Scripts\python.exe -X utf8 docs\hardwarex\inspect_rendered_documents.py`
and `.venv\Scripts\python.exe -X utf8
docs\hardwarex\verify_project_documents.py --publish`.

Deliverables replaced atomically at the original requested parent paths:
`../Logic Analyzer with Live Protocol Decoding_Lingga Aradhana Sahadewa.docx`
(1,275,756 bytes, Word opened/repaginated as 15 pages), matching-content PDF
(1,331,971 bytes, 16 Chromium A4 pages), and
`../KCCI Project_Documentation_Forms.xlsx` (67,598 bytes, six sheets: plan,
four weekly logs and unchanged scoring). SHA-256-verified publication is in
`build/reports/project-documentation/publication.json`. Verification PASS:
all 467 source blocks/table cells are present in both Word/PDF; every filled
workbook entry is present in its native Excel PDF; all printed text is within
page bounds; four weeks at five hours = twenty hours; independent BOM arithmetic
matches; scoring XML/assets and original styles/fonts remain preserved.
Inspected all sixteen final PDF pages on three contact sheets, the corrected
weekly table at full resolution, all five workbook pages and the restored
week-one title at full resolution. Four final draw.io figures also inspected.
Reports: `document-checks.json`, `workbook-checks.json`, `report-build.json`,
`render-checks.json` and previews alongside the publication report. Original
binary backups remain in that report directory's `originals/` folder.

`.venv\Scripts\python.exe -m py_compile` on the five Python report/workbook/
diagram/verification scripts PASS; `git diff --check` PASS with the pre-existing
line-ending notice. No firmware, RTL, protocol or hardware test was added/run.
Native Word PDF export remains a local tooling blocker documented in the README;
the delivered Chromium PDF passed content and full visual verification.
**Deliverable files are ready locally; documentation source/ledger remain
explicit WIP, uncommitted/unpushed because commit/push was not requested.**
Pre-existing learning/presentation work remains intact. Next: user reopens the
updated report/workbook; future document edits use the reproducible source and
preservation checks. The human visual walkthrough is author-confirmed complete;
no new latency numbers or detailed walkthrough pass counts are invented.

### Complete presentation speaking script (2026-10-12, published and verified; source WIP)

User requests understandable spoken explanations of every item in the current
37-slide presentation. Read the complete published PDF, all presenter notes,
the slide storyline and interface specifications. Intended files:
`docs/presentation/presentation-script.md`, script export/coverage tooling,
presentation README and this ledger. Publish stable Markdown, editable Word
and printable PDF script files in `../deliverables/`, preserving both
subsystem routes and the existing comparison. The script will explain every
diagram/table, acronym, calculation, result and operating limit; speaking
paragraphs are separate from pointing cues and deeper explanations.
Acceptance: 37 matched slide sections, route/term/numeric coverage checks,
document rendering and plain-English/factual review. No hardware test,
presentation content change or commit/push requested; source remains WIP.
Next: author the complete speaking script and its document exports.

Authored `docs/presentation/presentation-script.md` (about 9,600 words):
slide-by-slide spoken paragraphs for all 37 slides, pointing cues, likely
questions, route tags with skipped-slide notes, and a plain-language term
sheet. Built `build_script.py` (validated Markdown-to-HTML and DOCX export),
`render_script_pdf.mjs` (Chromium A4 PDF with page numbers) and
`publish_script.py` (stable-name publication). Validation PASS: exactly 37
slide sections numbered 1-37, both route names present, 27 key measured numbers
present, no forbidden text; readability statistics: 531 spoken sentences,
average 13.34 words, longest 37 words, five sentences over 30 words. DOCX
reopened in build and by Microsoft Word (40 pages, 9,606 words). Chromium PDF:
40 pages, 213,791 bytes; preview screenshots confirmed clean heading, quote and
table rendering. Published to `../deliverables/presentation-script.{md,docx,pdf}`
with SHA-256-verified copies and staging cleanup; report
`build/reports/presentation-delivery/script-publication.json`. One factual
correction was caught by validation and fixed: the exact 67,108,864 bytes and
41.4-41.5 s figures were added to the slide-32 script, and 12/12 was made
explicit on slide 16.

### Presentation arrow alignment and author (2026-10-12, published and verified; source WIP)

User requests a minor alignment fix to the USB + SSE arrow on the UART result
slide (PowerPoint slide 33 / body page 32), plus their name on the title slide.
Use **Lingga Aradhana Sahadewa**, confirmed by the parent project-document
filename. Intended files: presentation content/CSS, PowerPoint author metadata
and this ledger. Align the arrow with the two event counts, rebuild the stable
PPTX/PDF, visually inspect slides 1/33 and run the existing delivery checks.
Original comparison files remain hash-protected. No hardware change or
commit/push requested; source remains explicit WIP. Next: apply the edits and
regenerate the outputs.

Moved the USB + SSE arrow from (335,145)→(455,145) to (300,105)→(420,105)
in the diagram viewBox, aligning it with the event-count figures. Added the
full name as a bold title-slide author line and as PowerPoint author metadata.
Regeneration commands PASS: `capture_ui.py`, `create_deck.py`,
`render_deck.mjs`, `export_pptx.py`, `render_powerpoint.ps1` and
`check_delivery.py --check-only`. Native PowerPoint renders of slides 1 and 33
were inspected at full resolution; the name fits and the arrow/label are
aligned between the figures. The 37-slide deck and both native custom shows
(22/25 slides) remain valid; comparison hashes match; geometry/layout checks
have zero failures (HTML 0 FAIL / 1 WARN; PPTX 0 FAIL / 3 WARN).
`publish_deck.py` PASS: overwrote the stable PPTX/PDF/notes, verified hashes and
removed staging. PPTX size: 1,732,165 bytes. Reports remain under
`build/reports/presentation-delivery/`. Source/ledger remain explicitly WIP,
uncommitted/unpushed; no commit/push requested. No new hardware/firmware/RTL
test. Next: user opens the updated default deliverables.

### Dual-subsystem presentation restructure (2026-10-12, published and verified; source WIP)

User authorizes a shared deck with independent FPGA-class and embedded-system
sections, treating the other subsystem as a black box after the overview.
Preserve the current 24-slide PPTX/PDF/notes under `../deliverables/comparison/`
before publishing the new default deck. The parent output location remains
user-authorized; later default updates still overwrite the stable filenames.
Intended files: `docs/presentation/` content/section assembly, preservation,
PowerPoint export/navigation, checks/publishing, README and review, plus this
ledger. Acceptance: verified original-file hashes, clear class boundaries,
independent routes, source-backed FPGA/firmware detail, one fresh-eye review,
zero-failure slide/layout checks and native PowerPoint rendering. Existing
status/presentation WIP and untracked hardwarex/learning folders are preserved.
No new hardware test or commit/push requested. Next: preserve comparison files,
then build the class-specific sections.

Comparison preservation PASS: the previous 24-slide PPTX/PDF/notes are saved
unchanged under `../deliverables/comparison/`; hashes are in
`build/reports/presentation-delivery/comparison.json`. The new assembly has
37 slides, with a shared overview, independent hardware/firmware sections and
shared close. Initial HTML render: zero page errors / geometry issues.
Follow-up naming instruction: visible section/menu/custom-show labels are
**FPGA system** and **Embedded system**, not class names. Next: render the new
PowerPoint, verify both routes, review the black-box boundaries and publish.

Native route verification PASS: PowerPoint recognizes **FPGA system** (22
slides) and **Embedded system** (25 slides), with exact slide-ID sequences.
PowerShell's COM SlideIDs array includes a leading zero sentinel; the verifier
now ignores only that non-slide slot and checks all positive IDs. First
check-only run: HTML/PPTX zero FAIL, layout OK, internal navigation and both
independent routes pass, comparison hashes unchanged. One fresh-eye review of
all 37 slides and notes completed; draft scored 59/70 and both routes were
understandable. Adopted all twelve findings: repaired connected boundary/control
arrows, made the embedded run-word/timeline contract explicit, clarified the
shared throughput scope and buffered acquisition, exposed priority/trigger
conventions, expanded local interface terms and decoder mechanisms, and focused
the offline UI illustration on CH0 with cursor A. Next: re-render adopted
corrections, visually inspect all slides and publish after final checks.

Published new default `../deliverables/logic-analyzer-design.pptx` (37 slides,
1,732,071 bytes), native-rendered PDF (1,623,761 bytes) and notes (29,699 bytes).
The original 24-slide PPTX/PDF/notes remain unchanged in
`../deliverables/comparison/`, verified against the preservation hashes. Routes:
shared overview 1–6; FPGA system 7–18; embedded system 19–33; shared results /
close 34–37. Native PowerPoint custom shows contain exactly the intended 22
FPGA-route and 25 embedded-route slides. Menu/recap slide jumps also pass.

Final local commands PASS: `capture_ui.py` (CH0 only, eight decoded synthetic
events, cursor A at sample 3672, zero page errors); `create_deck.py` (37 slides /
35 printed body pages); `render_deck.mjs` (zero geometry errors);
`export_pptx.py` (editable text, notes, 2 named custom shows);
`render_powerpoint.ps1` (both native route names/counts/IDs, native PDF and all
37 slide PNGs); `check_delivery.py --check-only` (HTML 0 FAIL / 1 WARN, PPTX 0
FAIL / 3 WARN, layout OK, requested visible removals and comparison hashes
verified); and `publish_deck.py` (SHA-256-verified final files, stable names,
staging cleanup). Commands used `.venv\Scripts\python.exe -X utf8` for Python,
`node` for the renderer, and `&` for PowerShell. Full native-slide visual QA:
all seven contact sheets inspected, with the final embedded boundary checked
at full resolution after shortening its inbound command-arrow label.

Evidence: `build/reports/presentation-delivery/{publication.json,
delivery-checks.json,routes.json,storyline.md,comparison.json,check-html.txt,
check-pptx.txt,check-layout.txt,render-report.json,ui-screenshot-evidence.json}`;
review dispositions in `docs/presentation/subsystem-review.md`. Remaining
checker warnings are the reviewed English-label/cover/back-cover heuristics.
`Test-Path -LiteralPath "build\presentation"` is False and `git diff --check`
PASS with the existing line-ending notice. No new FPGA, firmware or hardware
verification was run; all bench results are dated existing evidence. Source
and this ledger are explicitly WIP, uncommitted/unpushed because commit/push
was not requested. Next: user selects a subsystem custom show and rehearses;
future default updates overwrite stable files while preserving comparison.

### Presentation delivery folder cleanup (2026-10-12, published and verified; source WIP)

User explicitly requests final presentation outputs in the parent directory's
`deliverables/` folder and overwrite-in-place for later updates. This is a
user-authorized exception to the usual project-root artifact location rule.
Inspected `../` and `build/presentation/`; the parent has no `deliverables/`
folder yet, and the presentation staging folder contains this session's
generated outputs/previews. Intended source files: presentation publisher,
checker, screenshot setup, README and this ledger. Publish the latest verified
PPTX, native PowerPoint PDF and presenter notes under stable names; preserve
small verification reports under `build/reports/presentation-delivery/`, then
remove the presentation staging folder. Acceptance: actual PPTX checks, byte
hash equality after publishing, clean staging removal and a repeat update to
the same filenames. Source remains WIP; no commit/push requested. Next: update
the publication step and move the current deliverables. First publication:
`.venv\Scripts\python.exe -X utf8 docs\presentation\check_delivery.py` PASS;
PPTX/native PDF/notes published to `../deliverables/` with matching SHA-256
hashes, small check reports archived, and `build/presentation/` removed.
`check_delivery.py` now runs this publisher automatically; the screenshot setup
recreates staging on the next update. No revision-suffixed output aliases are
created. Next: run one update from cleaned staging to verify the same output
files are replaced and staging is cleaned again.

Final verification: ran the complete documented sequence from removed staging
(`capture_ui.py`, `create_deck.py`, `render_deck.mjs`, `export_pptx.py`,
`render_powerpoint.ps1`, `check_delivery.py`) PASS. Screenshot setup recreated
the working folder; the native 24-slide PowerPoint rendered; HTML/PPTX checks
had zero FAIL and layout was OK. The final publication replaced all three
existing filenames, verified their SHA-256 hashes and removed staging again.
`../deliverables/` contains exactly `logic-analyzer-design.pptx` (1,069,062
bytes), `logic-analyzer-design.pdf` (992,313 bytes, native PowerPoint render),
and `presenter-notes.md` (18,159 bytes). `Test-Path -LiteralPath
"build\presentation"` returned False. Evidence:
`build/reports/presentation-delivery/publication.json` records all three
`replacedExistingFiles`, matching hashes and `stagingRemoved: true`; the other
small checks are alongside it. `git diff --check` PASS with the existing
line-ending notice. Source/ledger remain explicitly WIP, uncommitted and
unpushed; no commit/push requested. No hardware/RTL/firmware changes or new
bench evidence. Next: subsequent presentation updates use the documented
pipeline and overwrite the same parent deliverables automatically.

### Presentation visual and wording cleanup (2026-10-12, artifacts verified; source WIP)

User-requested revisions: remove the consulting-skill attribution, remove the
repeated `FINAL DIRECT-USB DESIGN` footer label, and remove visible slide source
lines. Follow-up steering also requests natural, easy-to-read English on every
slide; rewrite headings, explanations and table wording without changing the
design or measured results. Intended files: `docs/presentation/create_deck.py`,
`docs/presentation/check_delivery.py`, presentation README and this ledger.
Regenerate the existing 24-slide PPTX, HTML, PDFs and previews beneath
`build/presentation/`; retain page numbering. Acceptance: absence checks on
actual slide text, zero-failure mechanical/layout checks and native PowerPoint
render inspection. Session-start `git status --short --branch` shows the earlier
presentation/status WIP and pre-existing untracked `docs/hardwarex/` and
`docs/learning/`. No commit/push requested; source remains explicitly WIP.
Visible attribution/source/footer labels are removed in the template. Rewrote
all body-slide headings, explanatory bullets and tables in plain English;
technical identifiers and measured results are retained. Next: regenerate the
presentation, inspect the revised wording/layout and check the requested
removals in the actual PPTX. First rewritten export opened/rendered in
PowerPoint, but the layout checker found three footer collisions on the
six-row task table (slide 14). Reduced that table's vertical cell padding,
keeping the readable font size. Next: regenerate and recheck the corrected
layout.

Revision delivered locally: `build/presentation/logic-analyzer-design-revised.pptx`
and `logic-analyzer-design-revised.pdf`. The canonical PPTX/HTML/Chromium PDF,
native PowerPoint PDF, notes and all slide previews were also regenerated.
All 24 slides retain the ESP32-free architecture and original measured results.
The visible skill attribution, source lines and repeated footer label are
absent; body-page numbers remain. Headings, bullets and table descriptions use
plain English. Full-slide native-render inspection covered all 24 slides and
the final cover was checked at full resolution.

Verification: `.venv\Scripts\python.exe -X utf8
docs\presentation\create_deck.py` PASS; `node
docs\presentation\render_deck.mjs` PASS (zero page errors / geometry issues);
`.venv\Scripts\python.exe -X utf8 docs\presentation\export_pptx.py` PASS
(24 slides, editable text, notes; 1,069,062 bytes); `&
docs\presentation\render_powerpoint.ps1` PASS (native PDF + 24 slide renders);
`.venv\Scripts\python.exe -X utf8 docs\presentation\check_delivery.py` PASS
(requested-text absence assertions on actual slide shapes; HTML 0 FAIL / 1
WARN, PPTX 0 FAIL / 3 WARN, layout OK). Reports remain under
`build/presentation/{render-report.json,delivery-checks.json,check-html.txt,
check-pptx.txt,check-layout.txt}`. The remaining PPTX warnings are English cover
width heuristics and the intentional unnumbered back cover; native rendering
shows intact text. `git diff --check` PASS with the existing line-ending notice.
The source files and ledger are explicitly WIP, uncommitted and unpushed;
commit/push was not requested. No new hardware, firmware or RTL verification.
Next: user reviews or presents the revised deck.

### Final project presentation (2026-10-12, artifacts verified; source WIP)

**User-requested English final-project PowerPoint for the ESP32-free,
direct-Zynq-USB design; deliverable files are ready locally.** Baseline
`b487413` on `main` matches
`origin/main` after `git fetch origin`; the pre-existing untracked
`docs/hardwarex/` and `docs/learning/` are preserved. Intended authored files:
`docs/presentation/` (storyline, HTML, speaker notes, export tooling and review).
Generated PPTX, PDF, previews and check reports stay under
`build/presentation/`. Acceptance: source/evidence cross-check, consulting
slide-rule checks, rendered layout checks, full-slide visual inspection and
one independent content review. Results cite existing dated reports; no new
board, firmware or RTL verification is claimed. Commit/push was not requested,
so this documentation change is explicitly WIP until authorized. Discovery is
complete: the current local September 20 routed report supersedes older P9
figures (LUT 6,675 / 17,600; BRAM 37 / 60; WNS +0.576 ns; WHS +0.022 ns).
The 64 MiB throughput trial is an aggregate of sixteen 4 MiB captures, not a
64 MiB capture buffer. Authored 24-slide content plus HTML/PDF/PPTX export
scripts in `docs/presentation/`; local project `.venv` gained `python-pptx
1.0.2` for the export. First generator invocation hit Windows cp1252 console
encoding; the child now explicitly uses Python UTF-8 mode. Next: render, check
geometry/content, export PowerPoint and inspect every slide. First full build:
24-slide HTML/PDF/PPTX generated; browser geometry report has zero issues;
consulting HTML/PPTX checks have zero FAIL; licensed PowerPoint COM opened the
actual PPTX and exported all 24 slides and a PDF successfully. Visual review
found and fixed inherited small right-column text and theme shadows. One
independent review completed (15 findings, including optional improvements);
adopted clarification work is in progress, with an offline archived-capture UI
screenshot planned. No new hardware check is implied by screenshot rendering.

Final output: `build/presentation/logic-analyzer-design.pptx` (24 slides,
1,071,174 bytes, editable text and per-slide presenter notes),
`logic-analyzer-design-powerpoint.pdf` (native PowerPoint PDF), HTML/Chromium
PDF companion, `presenter-notes.md`, `storyline.md`, and all slide previews.
Authored files are the nine files in `docs/presentation/` (README, content
adaptation/export/render/check scripts and `review.md`) plus this ledger.
Fourteen of the independent review's fifteen findings were adopted fully or
partially; the optional reduction of architecture detail was declined because
the user requested depth. The actual UI screenshot uses declared synthetic
115200-baud UART data at 10 MS/s, opened offline; eight decoded events and zero
page errors were observed. It is explicitly not new bench evidence.

Final verification commands (all local, 2026-10-12):

- `.venv\Scripts\python.exe -X utf8 docs\presentation\capture_ui.py` PASS;
  `build/presentation/ui-screenshot-evidence.json` (offline synthetic data).
- `.venv\Scripts\python.exe -X utf8 docs\presentation\create_deck.py` PASS;
  reused the consulting skill's parts generator, 24 slides / 22 body pages.
- `node docs\presentation\render_deck.mjs` PASS; `render-report.json` records
  zero browser errors and zero text/SVG geometry issues.
- `.venv\Scripts\python.exe -X utf8 docs\presentation\export_pptx.py` PASS;
  saved/reopened 24-slide 16:9 package with editable text and notes.
- `& docs\presentation\render_powerpoint.ps1` PASS; Microsoft PowerPoint
  opened the actual PPTX, exported its PDF and all 24 1920x1080 slide images.
- `.venv\Scripts\python.exe -X utf8 docs\presentation\check_delivery.py`
  PASS; reports: `build/presentation/{check-html.txt,check-pptx.txt,
  check-layout.txt,delivery-checks.json}`. HTML 0 FAIL / 1 WARN; PPTX 0 FAIL /
  4 WARN; layout OK. HTML warning applies a Japanese uppercase-kicker rule to
  this English deck; PPTX warnings are three cover-line width heuristics
  (native renders show intact text) and an intentional unnumbered back cover.
- Visual QA: inspected all 24 actual PowerPoint slides on four native-render
  contact sheets; full-resolution checks included architecture/task/results
  slides and the final UI screenshot. Sources, page references, sample versus
  transfer rates and aggregate-versus-single-capture capacity were reviewed.
- `git diff --check` PASS (only the repository's existing line-ending notice).

No new FPGA, firmware, licensed synthesis/simulation or hardware test was run;
the results slides cite existing dated evidence. Remaining product gates are
the human browser/real-board visual check, public USB identity and authorized
release tag/publication. **Tracked source remains explicitly WIP, uncommitted
and unpushed because the user did not request commit/push.** The pre-existing
untracked directories remain intact. Next: user rehearses the deck; a further
content-review round or repository commit/push requires a follow-up request.

### Viewer legibility fixes: step patgen, orthogonal traces, edge stepping (2026-10-01)

**The counter self-test now renders square waves at Fit, logic traces are
orthogonal, and the edge buttons step cursor A through exact transitions.**
Three operator-reported issues from the section 5-6 walkthrough, fixed without
firmware, RTL or wire-protocol change:

- **Default view:** `la-ui` configured the step patgen modes (counter,
  walking, PRBS) with DIV 0, so at 100 MS/s CH0 toggled every sample (50 MHz)
  and Fit was a solid sub-pixel band. `host/la_ui/server.py` now scales the
  step DIV with the capture/live sample rate to 16 samples per step
  (`PATGEN_STEP_SAMPLES`, `patgen_div`) and passes it through
  `_configure_patgen`; protocol modes keep their nominal DIV.
  `host/la_cli/protocol.py` gains `PATGEN_DIV_SHIFT`.
- **Triangular CH0:** `_drawDetailChannel` connected consecutive samples with
  diagonals, turning a square wave into a sawtooth. The renderer now draws
  horizontal runs and vertical edges.
- **Dead edge buttons:** `findEdge` returned an envelope bucket time without
  placing cursor A, and re-centred to the same full-capture view. It now
  searches exact samples (existing detail or a fetched window) from cursor A,
  places the cursor and scrolls the edge into view. The envelope candidate
  rule also detects transitions exactly on a bucket boundary. `app.js` adds
  the `onSamplesNeeded` fetch bridge and a re-entrancy guard, and the operator
  guide's section 5-6 wording now states the step scaling and edge behavior.

Verification: `python tools/run.py test-host` PASS 167 passed (new
assertions: protocol modes DIV 0, step DIV 15 at 100 MS/s and 159 at 10 MS/s
over `/api/patgen` and the capture path); `python -m compileall -q tools host
tests` PASS; `node --check` on app.js/waveform.js/timeline.js PASS;
`host/tests/page_check.mjs` PASS with new `edgeStep`, `edgeCursor`,
`stepRendering`, `edgeEnvelope`, `boundaryWindow` and `edgeBoundary` checks.
Offline browser run against the TCP mock (Playwright/Chromium via a local
ignored harness, slowed-counter stream emulating the new DIV): the Fit and
zoom captures show the square-wave counter, and CH2 edge stepping walks
64/128/192/256 forward and 192/128 back with no console or page errors. No
hardware/JTAG/bench run was performed: the real Zynq counter capture and the
[HUMAN] visual click-through remain unverified. Next: run
`python tools/run.py test-e2e -- --target serial://COM13 -k ui` and the
section 5-6 walkthrough on the bench to close the visual gate, then continue
the operator runbook.

### Windows no-terminal UI launcher (2026-09-26)

**Added `tools/win/la-ui-start.vbs` and `tools/win/la-ui-stop.vbs` plus
machine-local desktop shortcuts.** The start script resolves the project root
from its own location, starts `.venv\Scripts\la-ui.exe` hidden, waits for the
port-8080 listener, opens the browser and reports a missing `.venv` or a
startup failure with a dialog; a second double-click only reopens the page.
The stop script terminates the Python process listening on port 8080 and
reports another owner instead of killing it. Smoke tested: hidden start ->
`127.0.0.1:8080` LISTENING with HTTP 200 (7,194 bytes) on `/`, a second start
reused the same listener PID, and stop freed the port; the not-running path
was exercised. Desktop shortcuts **LA UI** and **Stop LA UI** point at the
scripts (untracked, per machine). Docs updated:
`docs/using-the-analyzer.md` section 4, `docs/windows-dev.md`, `README.md`.
No UI, firmware, RTL or protocol change.

### USB-profile SD boot verified (2026-09-26, bench)

**The P10.4 `[HUMAN]` SD boot gate passes; the release tag remains
unauthorized.** Wrote the current USB-profile `BOOT.BIN` (2,413,488 bytes,
SHA-256 `0821AFD9...F9619D`, matching `build/release/release-manifest.json`)
to the FAT32 root of the bench microSD and cold-booted the Zybo with
JP5 = SD, JP6 = USB and no JTAG load. J10 enumerated
`USB\VID_1209&PID_0001\LA-USB-TEST-0001` (COM13, status OK); `la-cli ping`
returned `pong fw 0x00090000 pl 0x4C410002 uptime 41828 ms` and `la-cli
status` reported IDLE. `test-e2e --target serial://COM13 -k ui` passed
**6/6** (12.89 s, `build/reports/test-e2e.json`): ping/state, 64k capture
with envelope/exports/offline open, host UART decode, per-slot live with SSE,
trace dump and the 1M-sample capture. The first attempt scored 4/6 because the
external JE-1 -> JC-1 patgen loopback was not installed (live reads the real
input; captures use the internal selftest mux); an explicit
`CTRL_STREAM | CTRL_SELFTEST_EN` session decoded 10 UART events from the same
config, and reconnecting JE-1 -> JC-1 produced the 6/6 run. Evidence:
`build/reports/sd-boot-verification.md`. Open: the `[HUMAN]` browser visual
click-through, the `v1.0` tag, and the `web-artifact/` portal labels that
still say the SD boot is unverified (refresh with the next portal
publication). The USB identity `1209:0001` remains in-house only.

### UI-first operator runbook and portal update (2026-09-22)

**Complete in the repository; website publication is intentionally delegated.**
Replaced the mixed gateway/CLI-first operating guide with one direct-Zynq USB,
UI-first runbook: pre-power 3.3 V checks; JP5=JTAG, JP6=USB, J12 power/JTAG and
J10 data; the verified JTAG load; Windows/UI COM identification by the current
`1209:0001` development identity; `la-ui` startup and identity checks; a
100 MS/s binary-counter capture; waveform controls; UART/SPI/I2C capture and
live recipes; real-target wiring/capture/decode; export/offline-open semantics;
trace, troubleshooting and safe shutdown. The optional ESP32-S3 gateway remains
a separate supported `la-web` path and the physical USB-profile SD boot remains
explicitly unverified. Mirrored the operator sequence at the portal `/using`
route. Files: `docs/using-the-analyzer.md`,
`web-artifact/app/using/page.tsx`, `web-artifact/lib/docs/snapshot.ts` and this
ledger; no firmware, FPGA, protocol or bench behavior changed.

Source review against `host/la_ui`, the pattern generator, `tools/run.py`, the
USB/power contract and current limits corrected six draft issues before close:
the legacy `la-cli ports` filter does not include VID `0x1209`; a DIV=0 counter
must be sampled at 100 MS/s to show consecutive binary counts; a clean checkout
needs `bit` before `fw-zynq`; successful capture completion proves exact size
and whole-capture CRC but the page does not expose parser counters; SPI patgen
frames repeat; and the ESP32-S3 optional path needs both USB cables. A final
review also distinguished replay-ring rollover from active-client loss, removed
live-drop checks from burst acceptance, documented events CSV as annotations
only, and aligned server-error recovery with the actual browser/server split.

Verification: `npx oxlint app/using/page.tsx` PASS;
`npx oxfmt --check app/using/page.tsx` PASS; the documentation validator invoked
with the lockfile's `esbuild@0.27.3` binary PASS (7 subsystems, 79 components,
21 registers, 24 protocol messages, 11 roadmap phases, 55 glossary terms and
186 coverage entries); `npm run build` PASS and emitted `/using`; `git diff
--check` PASS. The packaged `npm run validate` command is currently BLOCKED
because `esbuild` is transitive and not root-linked in this pnpm workspace; the
same validator passed by direct lockfile-binary invocation. Repository-wide
`npm run lint` remains BLOCKED by 19 pre-existing errors in unrelated
`components/ui/` and `hooks/use-mobile.ts`; targeted lint is clean. No firmware,
FPGA, native or hardware test was required for this documentation/portal-only
change. No new browser or bench result is claimed; the existing `[HUMAN]`
visual click-through and SD boot gates remain open. Content commit `1bd7655`
contains the canonical runbook and `/using` update; the portal evidence snapshot
is pinned to that commit. The content and close-out changes were pushed to
`origin/main`; the pre-existing untracked `docs/hardwarex/` and `docs/learning/`
directories were not staged or modified. The website itself was not published,
as requested; next: the user's Codex publication workflow can deploy the updated
portal source.

### USB migration planning (2026-09-17)

**Planning complete; implementation not started.** Prepared
`docs/usb-migration-implementation-instructions.md` and `docs/usb-link.md`,
with navigation/authority updates in `docs/plan.md` and `README.md` and this
ledger. The user authorized writing the planning package, not USB firmware,
bench execution, adoption, website publication or release. Baseline:
`b30adfd` on `main`; `git fetch origin` succeeded after retrying the sandbox's
`.git/FETCH_HEAD` permission failure with approved access. Existing untracked
`docs/hardwarex/` and `docs/learning/` belong to earlier work and are excluded.
Acceptance: inspect local integration points and primary USB references,
check document links and execution gates, run `git diff --check`, review the
five-file change, commit/push and verify the remote. No hardware result is
claimed. The package defines U00-U09 and preserves `docs/plan.md` as normative;
the root v0.4 copy is now explicitly historical. Local inspection confirms USB0
and `usbps` / `XUsbPs` (including the generated library); first-party references
clarify J10 device mode, separate J12 power and the older AMD bare-metal CDC
example. No new firmware, test implementation or bench result exists.
Verification: `.venv/Scripts/python.exe build/reports/usb-planning-review.py`
PASS (20 local links, U00-U09 ordering/entry/exit gates, balanced code fences,
intended tracked scope and whitespace); report
`build/reports/usb-planning-review.json`. The review helper/report are ignored
local artifacts. Bare `python` was absent from PATH; the existing project
environment ran successfully with approved access after the sandbox could not
launch its base interpreter. `git diff --check` PASS. Manual source/contract
review checked driver family, port/power distinctions, transport routing,
ACK-window dependencies, packet prerequisites and honest evidence boundaries.
Native/firmware/bench tests are not run for this documentation-only change.
Planning commit `8eaf3ea15fbaaca1e2ecf4426ebc0e81b9077153` was pushed to
`origin/main`; `git ls-remote origin refs/heads/main` matched `git rev-parse HEAD`
after that push. `git diff --cached --check` passed and the staged set contained
exactly the five intended documents. Tracked files were clean; the full worktree
still contained the pre-existing untracked `docs/hardwarex/` and `docs/learning/`
folders, preserved rather than staged or deleted. This status-only close-out
records that verified planning commit and follows the same commit/push workflow.
Next: await the user's execution instruction; once authorized, begin U00.

### USB migration execution (2026-09-18)

**U00 complete locally.** The 2026-09-18 user instruction authorized executing
the first incomplete eligible U packet. U00 established and preserved the
rollback baseline at `b611390` (artifact hashes and copies under
`build/usb-migration/baseline/`, report `build/reports/usb-u00-baseline.md`)
and passed the required off-board checks (`test-native` 15/15, `test-host`
84/84). No firmware, bitstream or hardware state changed; the gateway profile
remains the default and rollback. Next: U01 local platform-feasibility evidence
and the `[HUMAN]` board/power/identity checklist; U02 contract design may
proceed locally, but no U03 device load until the U01/U02 entry gates pass.

### USB migration execution - U01 (2026-09-18)

**U01 complete.** The actual XSA and generated FreeRTOS BSP confirm `usbps` /
`XUsbPs` (USB0 enabled at 60 MHz, MIO 28..39, reset MIO 46, encoded IRQ
`0x4015`, MIT-licensed driver sources) and the installed `usbps_v2_9` examples
contain no CDC example. The old AMD CDC techtip is a bare-metal SDK 2015.1
reference that needed a custom Windows driver (it did not auto-bind
`usbser.sys`) and must not be imported. Physical checklist: REV D.0, JP1 open
(unsoldered), JP2 open (shunt removed), JP5=JTAG, JP6=USB (power from J12),
J11 empty, J10 connected through the USB hub by the user's direction with a
direct PC port as the first isolation step, FTDI `COM7` (0403:6010, serial
`210351BE7ACEB`), no board modification, fresh power with no image. Identity
basis: pid.codes test PID `0x1209:0x0001` for in-house development only, with
the release identity recorded as an open U07/U09 gate. Evidence:
`build/reports/usb-u01-feasibility.md`. No hardware PASS is claimed. Next: U02.

### USB migration execution - U03 (2026-09-18, WIP bench)

**U03 implementation complete locally; bench is WIP with a bulk-OUT packet
loss blocker, not a PASS.** Built the direct-USB CDC-ACM stack (`la_usb_core`,
`la_usb_desc`, `la_usb_dev`, `la_usb_task`), the `LA_TRANSPORT_USB`
app/profile integration, the build selection (`fw-zynq -- --profile usb`
builds `la_app_usb`; `jtag-zynq -- --profile usb` loads it), the USB-profile
`LINK_CFG` rejection and native tests (`test-native` 17/17). The gateway
profile still builds unchanged.

Bench (2026-09-18, J10 direct to a PC port and also through the hub):
complete enumeration with **high speed**, `usbser.sys` auto-binding
(`USB\VID_1209&PID_0001\LA-USB-TEST-0001`, COM12 Status OK) and clean
control transfers (144+ setups, 0 errors, including GET/SET_LINE_CODING).
`la-cli ping` and `la-cli status` have succeeded over USB. Fixed on the way:
(1) SDT `XUsbPs_LookupConfig` u16 truncation; (2) the DMA pool was not zeroed
before `XUsbPs_ConfigureDevice` (wild controller DMA); (3) the SLE/UI
suspend storm; (4) the vendor RX loop requires pop-and-release in the
callback - the driver now copies each completion into a bounded staging ring
and releases/re-primes inside the callback; (5) the driver never sets the
endpoint control registers, so EP1/EP2 stayed disabled (`EPCTRL1=0`, host
tokens timed out) - they are now enabled at SET_CONFIGURATION; (6) the
previous transfer's 0-length status completion could be mistaken for
`SET_LINE_CODING` data - dropped while waiting; (7) SET_INTERFACE and
CLEAR_FEATURE(ENDPOINT_HALT) require the device to reset the data endpoints'
toggles (the host resets its own), otherwise the device ACKs but discards
every other bulk packet - `usb_reset_data_endpoints()` flushes EP1 OUT/IN and
EP2 IN and re-primes (built, pending bench verification).

**Remaining blocker: EP1 bulk OUT loses roughly half of the host's
transfers.** Evidence: 50 writes at 50 ms -> 25 completions; 10 writes ->
9; 8 writes at 300 ms -> 4; 12 writes -> 6; one 140-byte write in FS mode
completed a 64-byte packet and the remaining 76 bytes never arrived; the dTD
ring is fully active and correct (all 16 tokens `0x02008080`, circular NLP,
pool buffers), `EPRDY=3`, `EPCTRL1=0x00800080`, `out_waiting=0`, no staging
overflow and no driver-entry errors (`bad=0`, `ovf=0`). The software and
hardware cursors stay in sync (`sw=hw`). A controlled two-batch test showed
the first four packets of a batch accepted (dTD 0-3) and then a silent
stall while the endpoint was still primed; a task-context sweep
(`la_usb_sweep_rx`, which also drains EPCOMPL for stranded IN completions)
was added to recover lost-interrupt stalls, but the loss persists (12 writes
-> 3-6 received). A 2-descriptor OUT ring loses the same; full 512-byte HS
transfers also lose half (8 x 512 -> 4 completions), so it is not a ring-size
or short-packet effect. Both HS and forced FS lose; control transfers are
clean. Device-side variables are now exhausted. **Bring-up experiment log:**
the full hypothesis/experiment/result matrix (21 experiments, including the
ones that ruled out speed, ring depth, transfer size, release cadence, the
sweep and host flow control) is preserved at
`build/reports/usb-migration/U03/usb-u03-bring-up-log.md` (ignored local
evidence). Surviving hypothesis: a data-toggle desync (fix 7 above), pending
bench verification; host-side checks (USB 2.0 port/hub/PC) remain a control
but are unavailable on this bench.
A status-print side effect remains too: the long status blocks corrupt
`.rodata` (short diagnostic lines are stable). No open/close or reset-cycle
evidence yet; the gateway remains the working data path. Next tactic:
verify the Zynq device-mode dQH/dTD field encoding against UG585 and the
AMD CDC techtip before further blind changes.

### USB migration execution - U03 offline hardening (2026-09-18, no bench)

**Bench-blocked work completed while J10/J12 are disconnected (commit
`4f8a9e7`, pushed to `origin/main`).** Preserved the
bench baseline (commit `d1e36e1`, `la_app_usb.elf` SHA-256
`1C1F3122...1EC31CB`, copies and hashes under
`build/reports/usb-migration/U03/`) and audited the `d1e36e1` toggle-reset fix
before changing it. Audit (`docs/usb-link.md` section 10): the EPCR `TXR`/`RXR`
bits are the documented "Data Toggle Reset" (`xusbps_hw.h:159,171`), while
`EPFLUSH` is documented only as an endpoint flush and the driver uses it only
in reset/configuration paths; the old code comment's "flush resets the toggle"
was an assumption. `docs.amd.com` (UG585) is a JavaScript application and its
USB register chapter could not be retrieved from this environment, so the
installed first-party driver headers plus USB 2.0 sections 9.4.5/9.4.7/9.4.10
are the documentation basis; no UG585 confirmation is claimed. Corrections:
`la_usb_toggle_reset()` writes the explicit EPCR bits, CLEAR_FEATURE resets
only the addressed endpoint/direction, SET_INTERFACE is interface-scoped and
validates `wIndex`/`wValue`, and `la_usb_reconfigure` no longer strands an
in-flight TX slot.

Added the offline experiment apparatus: a 512-entry bounded USB event trace
(`la_usb_trace.{h,c}`, `la_native_usb_trace` host test) covering setups, OUT
descriptor completions, staging, parser delivery, TX submit/done and
reset/session events with overwrite detection; JTAG retrieval
(`fw-zynq/scripts/usb_trace_dump.tcl`, `tools/usb_trace.py`, decoder verified
against a synthetic dump); the additive `TEST_RECORD` (0x1B) numbered-record
command with native and host coverage; and the bench-gated loss detector
(`tests/e2e/test_usb_bulk.py`, `-k test_usb`) that accounts missing,
duplicated, reordered and NAKed records separately. The USB profile is quiet
by default (`LA_USB_STATUS_PRINT=0`) so the unresolved `.rodata` print
corruption (experiment 7) is avoided rather than reproduced; it remains not
root-caused and is recorded as a confounder. Pre-registered bench predictions
and acceptance criteria are in `docs/usb-link.md` section 11.

Verification: `test-native` 18/18 (new trace ring test, TEST_RECORD protocol
vectors and dispatch cases); `test-host` 87 passed (new record-accounting
tests, including a mock that drops records); `python -m compileall -q tools
host tests` PASS; `fw-zynq -- --profile usb` PASS (`la_app_usb.elf` text
209072 / data 3720 / bss 281408, SHA-256 `A536912E...2B9E10A7`, copied to
`build/release/la_app_usb.elf`) and the default `fw-zynq` gateway profile PASS
(`la_app.elf` text 191144 / data 3688 / bss 243936) unchanged. No bench result
is claimed: the 12-write burst, the setup-request observation, the A/B reset
comparison, the 200-record stream and the trace dump remain BLOCKED until the
board is connected. Next: reconnect J10/J12, load the corrected USB ELF
(`jtag-zynq -- --profile usb`), run `test-e2e -- --target serial://COMx -k
test_usb`, dump `la_usb_trace_ring`, and record whether SET_INTERFACE or
CLEAR_FEATURE occurs at port open and whether the corrected reset path changes
the loss.

### USB migration execution - U03 bench session (2026-09-18)

**Bench ran with the instrumented image; the bulk-OUT loss is now measured and
localized, but not fixed (commit `e781dca`).** Loaded the USB-profile image
over JTAG
(`jtag-zynq -- --profile usb`, PL/FSBL/app PASS), COM12 enumerated
(VID 1209:0001), and ran the numbered-record detector. The pending 12-write
experiment: `sent=12 acked=6 missing=[2,4,6,8,10,12]`, zero write errors, zero
CRC/format errors, no duplicates and no reordering - exactly alternating loss,
repeatable. The JTAG trace (512 entries) shows only seven EP1 OUT completions
for the 13 host writes; the lost records never reached the controller, so the
first divergence is host -> controller (ACKed then discarded), not staging,
parser or response.

Endpoint-type and toggle experiments (each built, loaded and re-tested):
- Programming EPCR EP1 as BULK (the driver never sets the type; the vendor
  Chapter 9 example does) makes the core NAK every bulk OUT packet instead of
  ACK-discarding every other one.
- EPCR RXR/TXR writes (the documented "Data Toggle Reset" bits), before/after
  the flush, while disabled, and disable/enable cycles: no change.
- Removing the init-time EP1 prime and priming only after a serviced
  completion (vendor RX-loop pattern): no change.
- Without the SET_CONFIGURATION EPFLUSH of EP1 OUT the core NAKs every packet
  even with EPRDY=3 and an active dTD ring; the flush is load-bearing and is
  kept as the baseline behavior.

The instrumented baseline image (USB profile, ELF SHA-256
`F625E3AB...FCA513A9`, copied to `build/release/la_app_usb.elf`) reproduces the
failure exactly. The remaining ambiguity - whether the device expects DATA1 or
the host starts at DATA1 - needs a USB protocol analyzer; a direct USB 2.0 port
is the first host-side control (the bench uses an AMD xHCI USB 3.x root hub; no
other PC available). No bench PASS is claimed; the gateway remains the working
data path. Verification: `test-native` 18/18, `test-host` 87 passed,
`fw-zynq -- --profile usb` PASS. Evidence:
`build/reports/usb-migration/U03/usb-trace-final-decoded.txt`,
`usb-trace-final-baseline.txt`, `baseline-hashes.txt` and the detector failure
output. Next: move J10 to a direct USB 2.0 port (or obtain an analyzer), rerun
the 12-write detector, compare the trace; do not re-enable the type programming
until the PID/toggle behavior is observable.

### USB migration execution - U03 J10 hardware control (2026-09-18)

**J10/USB3320/PS USB0 hardware cleared by a stock-example control; the U03
alternating bulk-OUT loss is firmware/driver configuration, not the connector
or PHY.** Built and ran AMD's unmodified `usbps_v2_9` mass-storage example
(Chapter 9 + storage class + vendor `XUsbPs` driver) through a new bench-only
`--profile example` build/load path. The seven vendor files are copied
verbatim into `fw-zynq/example/` (SHA-256 recorded in `docs/usb-link.md`
section 13); the only repo-owned additions are build-level adaptations in
`fw-zynq/scripts/la_platform.py` (`XPS_USB0_BASEADDR=XPAR_XUSBPS_0_BASEADDR`,
`CLASS_STORAGE_DEBUG=1`, generated linker script), the `--profile example`
mapping in `fw-zynq/scripts/jtag_load.tcl` and `tools/run.py`, and the
link-only `fw-zynq/example/la_freertos_trace_stubs.c` for the BSP P8.3 trace
hooks. The vendor's `while (NumReceivedFrames < 1)` spin is kept as shipped
(the counter is never incremented, so the device serves forever), as is its
stock endpoint setup: `XUsbPs_EpEnable(1, OUT/IN)`, then EPCR1
`TXT_BULK|RXT_BULK|TXR|RXR`, then `XUsbPs_EpPrime(1, OUT)` at
SET_CONFIGURATION.

Bench (2026-09-18, J10 direct to the PC, same host as the U03 loss):
- Windows enumerated `USB\VID_0D7D&PID_0100` as a USB mass-storage device;
  `Get-Disk` reported `Xilinx PS USB VirtDisk`, 1,048,576 bytes, USB bus.
- A 10 s COM7 capture shows a continuous, orderly SCSI stream (TEST UNIT
  READY, MODE SENSE, READ CAPACITY, READ at offsets 0x0/0x400/0x8000) with no
  stalls or gaps: every host CBW (bulk OUT) reached the device and every
  response completed.
- Elevated raw-sector test (`build/reports/usb-migration/U03/example-write-test.txt`):
  65,536 bytes written to the raw physical drive in 23 ms, read back in 7 ms,
  **0 mismatched bytes** - multi-packet bulk OUT plus bulk IN, byte-exact.
- JTAG readback of the device `VirtFlash` at offset 0x40000 returned
  `18110A03 342D261F`, exactly the host pattern; sector 0 stayed zero.

Conclusion: the J10 path handles control, bulk IN and multi-packet bulk OUT
correctly with the vendor driver and the vendor's exact endpoint sequence.
That sequence includes the EPCR type/toggle-reset programming that appeared to
break the project image in the section-12 experiments, so the next U03 tactic
is to align `la_usb_reconfigure` with the vendor sequence and re-run the
numbered-record detector. No product firmware result is claimed; the gateway
remains the working data path. The board currently runs the example; reload
the product image with `jtag-zynq -- --profile usb` (or gateway) to resume
U03. Verification: `fw-zynq -- --profile example` PASS (`la_usb_example.elf`
text 78,598 / data 3,084 / bss 1,210,640; `build/reports/fw-zynq.json`),
`jtag-zynq -- --profile example` PASS, host write/read PASS, device-memory
cross-check PASS. The example profile is a bench control only and is not part
of the product build.

### USB migration execution - U02 (2026-09-18)

**U02 complete; contract frozen.** `docs/usb-link.md` section 9 is the frozen
integration contract: `usbps`/`XUsbPs` init sequence and GIC ID 53
(`0x4015`), notification-only endpoint callbacks with the bounded
`XUsbPs_EpBufferReceive` pop required by the vendor cursor advance, deferred
class processing in UsbTask, stale-generation dropping, CDC-ACM descriptor
set with IAD/`wTotalLength` 66 and automatic `usbser.sys` binding, test
identity `0x1209:0x0001` with release identity open, line-coding/DTR policy,
non-cached 64 KiB DMA pool at NC offset `0x00900000`, 8 x 1088-byte TX pool,
16-entry completion ring, `rx_sb` NAK backpressure, gateway-default build
profiles (`fw-zynq -- --profile usb`), and `test_usb_*` selectors. Portable
`la_usb_core.{h,c}` models the session lifecycle and completion ring; the new
`la_native_usb_core` host test covers generation bumps, action sets, invalid
transitions, suspend/resume, FIFO order, overflow counting, wrap and stale
drops. Evidence: `build/reports/usb-u02-contract.md`, `test-native` 16/16.
Next: U03 implementation, then a [HUMAN] JTAG enumeration session.

### USB migration execution - U03 host-side controls (2026-09-18)

**Host-side control setup complete locally; USBPcap needs a reboot before it
can capture.** Added the option-1 host controls from the U03 alternative list:
USBPcap/Wireshark URB capture plus a raw libusb bulk path that bypasses
`usbser.sys`. New `tools/usb_capture.py` (list/capture/decode) and
`tools/usb_raw_bulk.py` (list/info/raw/records, reusing the shared
`TEST_RECORD` accounting); `pyproject.toml` gained the pinned optional extra
`usb-bench` (pyusb 1.3.1, libusb-package 1.0.26.2); `docs/usb-link.md` section
14 records the setup, the driver-binding procedure and the pre-registered
observations. Installed: Wireshark 4.6.8 (winget), USBPcap 1.5.4.0 (silent,
SHA-256 `87A7EDF9...82CCB622`; USBPcapCMD copied into Wireshark extcap;
`-I` run, four AMD `ROOT_HUB30` roots, all standard HWIDs), Zadig 2.9
(`.tools\zadig\`, SHA-256 `4ECAA95D...F82039B`, ignored). The USBPcap class
filter is registered in the USB class `UpperFilters`, but the driver is still
`Stopped` and `\\.\USBPcapN` does not exist; a root-hub disable/enable and
`pnputil /restart-device` did not attach it, so per the USBPcap README
**Windows must be rebooted** before `usb_capture.py list` can show
`USBPcap1..4`. Wireshark warns that Npcap is missing and WinPcap 4.1.3 is
installed; Npcap is not needed for USBPcap captures or pcap reading and
WinPcap was left untouched.

Verification: `python -m compileall -q tools host tests` PASS;
`tools/usb_raw_bulk.py list` PASS (8 devices; the vendor example
`0d7d:0100` is attached, the product `1209:0001` is not);
`tools/usb_raw_bulk.py info --vid 0x0d7d --pid 0x0100` correctly reported the
WinUSB/Zadig guidance instead of crashing (the example is bound to
`usbstor.sys`); `tools/usb_capture.py list` BLOCKED (no USBPcap interfaces
before the reboot). No capture, raw-burst or firmware result is claimed.
Next: reboot Windows, verify `usb_capture.py list` and a short capture, load
`jtag-zynq -- --profile usb`, bind WinUSB with Zadig, run
`usb_raw_bulk.py records --count 12 --gap-ms 50` and the `usb_capture.py
decode` table, and compare both against the serial detector and the JTAG
trace. Restore `usbser.sys` after the raw test.

### USB migration execution - U03 host controls run (2026-09-19)

**The host controls ran; the data-toggle desync is confirmed as the
alternating bulk-OUT loss and a separate device TX-path defect surfaced.**
Rebuilt the USB ELF (`fw-zynq -- --profile usb`, text 209008 / data 3720 /
bss 281408) and loaded it over JTAG. USBPcap attach needed
`pnputil /restart-device` on each root hub (a reboot alone did not attach the
filter); three of the four AMD roots now expose `USBPcap3/4/5` and captured
real traffic (`smoke-hub1.pcap` 27,207 bytes, 546 bulk URBs decoded by
tshark; the `usb_capture.py` decode field list was corrected to tshark 4.6's
`usb.bmRequestType`). The root hub hosting J10
(`USB\ROOT_HUB30\5&7EC62FA&0&0`) is stuck in a persistent "pending system
reboot" state (`pnputil /restart-device`, `/scan-devices` and a parent
controller restart all fail or re-queue), so the device-hub URB capture is
BLOCKED; workaround is moving the external hub to a filtered port or a cold
shutdown.

Bench evidence. Serial baseline on COM12 reproduced the recorded failure twice
(`missing=[2,4,6,8,10,12]`, then `[14,16,18,20,22,24]`). The device trace
(125 setups in the ring window) contains **zero** `SET_INTERFACE` (0x0B) and
**zero** `CLEAR_FEATURE` (0x01) - Windows only issues line-coding/DTR
requests, so the toggle-reset path is never exercised (pre-registered
observation 1 answered). Raw libusb/WinUSB (Zadig `oem33.inf`) reproduced the
alternating loss three times: `sent=12 acked=6 missing=[0,2,4,6,8,10]`, zero
write errors/CRC/duplicates/reordering - `usbser.sys` and the CDC class path
are cleared. With the `--control` sequence (SET_LINE_CODING,
SET_CONTROL_LINE_STATE, SET_INTERFACE, CLEAR_FEATURE OUT/IN) the alternating
loss disappears: the device trace shows all 12 OUT completions and all 12
FRAMEs, confirming the data-toggle desync. Residual control-run misses
(`[0,9,10,11]`, `[0,11]`) are a separate device TX-path stall: TX_DONE entries
stop at slot 7 and include `slot=0xFFFFFFFF` no-in-flight completions, so ACKs
stop after eight frames. Restored `usbser.sys` with
`pnputil /delete-driver oem33.inf /uninstall` + `/scan-devices` (device is now
`USB Serial Device (COM13)`; status alternates success/timeout as recorded).
Evidence: `build/reports/usb-migration/U03/host-controls/summary.md` and the
pcap/trace files beside it (ignored local evidence). No firmware fix or clean
run is claimed. Next: change `la_usb_reconfigure` to reset the OUT toggle
exactly as the vendor example does (EPCR type + TXR/RXR, no EPFLUSH-only
reset) and re-test with `usb_raw_bulk.py records`; root-cause the ACK TX stall;
optionally move the J10 hub to a filtered root-hub port for the URB capture.

### USB migration execution - U03 toggle fix (2026-09-19)

**The alternating bulk-OUT loss is fixed on the product path; the full U03
detector suite passes twice.** `la_usb_reconfigure` now follows the vendor
Chapter 9 sequence exactly (`xusbps_ch9_storage.c:389`): EpEnable EP1 OUT/IN
and EP2 IN, one `XUsbPs_SetBits(EPCR1, TXT_BULK | RXT_BULK | TXR | RXR)` write,
then EpPrime EP1 OUT; the EPFLUSH and the init-time EP1 prime are removed so
EP1 stays disabled until SET_CONFIGURATION, as in the vendor example. The
section-12 type-only experiments failed because they kept the flush and omitted
the toggle reset; the section-14 host controls proved the Windows host starts
at DATA0. Burst capacity was also fixed: `LA_USB_STAGE_COUNT` 8 -> 16 and
`LA_USB_TX_FRAME_COUNT` 8 -> 16 (`la_usb_dev.h`), and
`AnalyzerClient.test_records` now drains ACKs after every write
(`client.py::_drain`, 1 ms non-blocking-safe recv). Without the concurrent
drain the 200-record stream overflowed the 8-slot TX pool - the section-14
"TX stall" was that overflow, not a controller fault.

Verification: `python tools/run.py test-e2e -- --target serial://COM13
-k test_usb` PASS 3/3 twice in a row (12-write, 12-write repeat x2, 200-record
stream; zero write errors, CRC/format errors, duplicates, reordering or NAKs);
`test-native` 18/18; `test-host` 87 passed; `fw-zynq -- --profile usb` PASS
(text 209072 / data 3720 / bss 294784) and `jtag-zynq -- --profile usb` PASS
(COM13 enumeration). No gateway-profile change. The raw libusb re-run was not
repeated; the device-hub USBPcap capture stays blocked by the section-14
pending PnP state. Next: decide whether U03 closes here (raw-path re-check and
U04+ gates in `docs/usb-migration-implementation-instructions.md` remain) or
the device-hub URB capture is worth a port move.

### USB migration execution - U03 closed, U04 WIP (2026-09-19)

**U03 closed locally; U04 burst coverage passes over USB, but the new
transport suite is WIP (1/5).** U03 evidence is
`build/reports/usb-u03-enumeration.md`: identity/descriptors, class and
line-coding requests, 20/20 open/close cycles with a PING, 10/10
`pnputil /restart-device` reset cycles with a PING, 11-byte short-packet
bursts and a 512-byte exact-multiple transfer (`OUT_DTD len=512`,
`RX_SB accepted=512`). Gaps: an explicit unsupported-class-request probe and
the raw-libusb re-run.

Two further USB transport bugs were found by the common suites and fixed in
`la_usb_dev.c/h`: (1) concurrent `la_usb_enqueue` producers could reserve the
same slot - reservation is now atomic under a critical section; (2) the pump
sent the lowest-index ready slot, so a slot freed mid-burst let a later frame
overtake an earlier one (device trace: DATA_CHUNK sequence 14, 16, 15) - a
FIFO ready queue now preserves enqueue order. With the fixes,
`tests/e2e/test_zynq_burst.py` 7 passed/1 skipped, `test_e2e_burst.py` 18/18
and `test_p92_sizes.py` 18 passed/1 skipped over `serial://COM13`, covering
PING/STATUS, CONFIG/ARM/ABORT, capacity NAK, short tails, multi-descriptor
captures and raw/transition packing.

U04 WIP: the new `tests/e2e/test_usb_transport.py` passes only
`test_usb_link_cfg_rejected` (every `LINK_CFG`, including `LINK_UART`, is
NAKed `unsupported` on the USB profile). `TRACE_CTRL START` is ACKed but the
dump times out and leaves the device unresponsive until a JTAG reload; the
raw-parser split/coalesced/garbage tests then fail as a cascade. On a fresh
device an isolated garbage + bad-CRC + 2 KiB-flush + PING probe did recover
with a PONG. Parser characteristic: a corrupted length field blinds the
receiver for up to `LA_PROTO_MAX_PAYLOAD` (1024) bytes before the CRC check
resynchronizes; recovery is deterministic but needs bounded flush traffic.
No `build/reports/usb-u04-protocol.md` yet. Next: fix or bound the USB trace
dump, re-run the transport suite on a fresh device, then continue U05.

### USB migration execution - U04 complete (2026-09-19)

**U04 passes; the trace dump is now chunked and paced.** The USB dump used to
stall for minutes because the CmdTask emitted thousands of LOG frames
back-to-back and saturated the 16-slot USB TX pool. Fixes: an additive chunked
`TRACE_CTRL` payload (`action:u8, start:u16 LE, count:u8`, firmware maximum 64
records per request, `END next total dropped`) with a host loop in
`AnalyzerClient.trace_dump`, and one `vTaskDelay(1)` per emitted line on the
USB profile so the pool keeps cycling. The USB event ring grew to 4096 entries
and the pump records `TX_SUBMIT`/`EP_STATE`. A full 4096-record dump completes
in ~34 s.

Verification (bench, `--target serial://COM13`): `test-e2e
tests/e2e/test_usb_transport.py` 5/5 (LINK_CFG rejection for every transport,
TRACE_CTRL start/stop/chunked dump, parser split/coalesced/garbage/bad-CRC
recovery with a bounded flush); `test_zynq_burst` 7 passed/1 skipped (soak);
P6.2 `test_e2e_burst` 18/18; `test_p92_sizes` 18 passed/1 skipped (live);
`test-native` 18/18; `test-host` 87; both profile builds PASS. Evidence:
`build/reports/usb-u04-protocol.md`. Parser note: a corrupted length field
blinds the receiver for up to `LA_PROTO_MAX_PAYLOAD` (1024) bytes before the
CRC check resynchronizes; recovery is deterministic with a bounded flush.
Next: U05 (external loopback matrix, live decoders/stop, recovery cases, soak)
then U06 (64 MiB USB drain >= 1.5 MB/s).

### USB migration execution - U05 and U06 (2026-09-19)

**U05 correctness gates pass over USB; the wiring-gated matrices are BLOCKED.
U06 fails the 1.5 MB/s drain gate (correctness exact, throughput short).**
Over `serial://COM13`: P6.2 internal selftest matrix 18/18; `test_zynq_burst`
7 passed/1 skipped; the 500-capture soak passed (527.8 s, exact length and
zero parser errors per capture); P9.2 sizes 18 passed/1 skipped; the U04
transport suite 6/6 including close-during-capture recovery to IDLE within
3 s. Blocked, not counted as passing: the external JE->JC matrix (jumpers not
installed), the live decoder/stop/p99 gates (live patgen wiring absent), the
elevation-gated reset-during-capture check (UAC unanswered) and the
[HUMAN] unplug/replug cases. Evidence:
`build/reports/usb-u05-regression.md`.

U06: a 4 MiB capture is byte-exact (4,194,304 bytes, 4,129 chunks, zero CRC or
format errors, DATA_END CRC match) but the rate is 0.03-0.85 MB/s depending on
the host read cadence, below the 1.5 MB/s gate. The client read cap was fixed
(500 ms -> 20 ms; a ~10x capture improvement) and the parser CRC is now
table-driven; an 8-deep multi-in-flight TX experiment reordered chunks under
long transfers and was reverted. Required design amendment: fix queued
IN-transfer batching, enlarge the DATA_CHUNK payload (protocol amendment) or
raise the USB task service rate. Evidence:
`build/reports/usb-u06-performance.{md,json}`. Next: U07 records the adoption
decision (performance-blocked), then U08/U09.

### USB migration execution - U07 decision: DEFER (2026-09-19, superseded by the revised entry below)

**Direct USB is not adopted as the primary transport.** Gate table: U03
enumeration/open-close/reset/short+exact transfers PASS; U04 commands, parser
and chunked trace PASS; U05 common burst matrices and the 500-capture soak
PASS (external/live gates BLOCKED on wiring); U06 64 MiB drain >= 1.5 MB/s
**FAIL** (0.03-0.85 MB/s, byte-exact). Measured limits, recovery behavior
(close-during-capture to IDLE within 3 s), power/cable arrangement (J10
through a USB 2.0 hub, J12 power, high speed) and the code footprint (USB
`la_app_usb` text 209,072 / data 3,720 / bss 294,784 versus gateway `la_app`
text 191,144 / data 3,688 / bss 243,936) are recorded in `docs/usb-link.md`
section 16. Disposition: the **gateway remains primary and supported**; P9.1,
P9.4 and P10.1 keep their current status; direct USB stays a WIP/experimental
profile with all findings preserved; the release identity gate stays open; no
wireless or browser feature is removed. Revisit by fixing the queued
IN-transfer batching, enlarging the DATA_CHUNK payload (protocol amendment) or
raising the USB task service rate, then re-running U06. Files updated:
`docs/usb-link.md`, `docs/plan.md`, `docs/limits.md`, this ledger.

### USB migration execution - U06 amendment and U07 adoption (2026-09-19, revised)

**U06 now PASSES and U07 is revised to ADOPT per the user's direction.** The
documented DATA_CHUNK amendment: `DATA_CHUNK` data 1016 -> 4092 bytes,
`MAX_PAYLOAD` 1024 -> 4096, USB TX frames 1088 -> 4160, UART TX stream buffer
4096 -> 8192, TxTask stack 4 KiB -> 12 KiB (the enlarged 4103-byte wire and
4096-byte chunk buffers overflowed the old stack and crashed the PS), the
client reads 64 KiB with a 20 ms cap (0 for the drain) and ACKs every 4
chunks, and the parser CRC is table-driven. Drain result: three consecutive
64 MiB trials at **1.62 MB/s** each (41.4-41.5 s per trial), byte-exact
(counter sequence, frame CRC and DATA_END CRC verified). Regressions after
the amendment: `test-native` 18/18, `release` 22/22, `test-host` 87, USB
bulk+transport 9/9, `test_zynq_burst` 7 passed/1 skipped, P6.2 matrix 18/18,
P9.2 18 passed/1 skipped, both firmware profiles build; the gateway/SPI bench
paths were not re-run (not wired) and stay unverified. Adoption: direct USB is
a supported transport, the gateway remains supported and the default build,
no wireless/browser feature removed, the release identity gate stays open.
Evidence: `build/reports/usb-u06-performance.{md,json}`; docs updated:
`docs/protocol.md`, `docs/usb-link.md` section 16, `docs/plan.md`,
`docs/limits.md`, `docs/using-the-analyzer.md`.

### USB migration execution - U05 wired bench (2026-09-19)

**The wiring-gated U05 gates now run and pass over USB.** With the JE->JC
loopback installed: P6.3 external matrix 18/18 (`--external-loopback
--channel-mask 0xFF`), P6.2 18/18, `test_zynq_burst` 7 passed/1 skipped, the
500-capture soak passed, P9.2 19/19 including the live compact comparison,
the live suite `--live --live-seconds 60` 8 passed/3 skipped, and the live
trace 1/1. The live 60 s UART zero-drop gate, isolated-byte p99, SPI/I2C
10 kHz decoders, decoded-pattern stop, raw stream and garbage-flood resync
all pass. Skipped with reasons: the relay-ring stall injection (no relay on
USB), the TCP-kill injection (needs a `tcp://` target) and the 1 MHz SPI
overload loss report. **Open item:** on direct USB the sustained 1 MHz SPI
flood leaves a multi-second backlog and can stall the device (a bounded 2 s
flood recovers at 2.1 s with 189,488 events); the 5 s gateway-window variant
is not a passing gate until that recovery gap is fixed. The `pnputil`
reset-during-capture probe reported a PnP "reboot needed" quirk; the device
stayed responsive and IDLE but a true bus reset was not proven. Physical
unplug/replug and suspend/resume remain [HUMAN]. Evidence:
`build/reports/usb-u05-regression.md`.

### USB migration execution - U05 live-path fault tolerance (2026-09-19, fixed)

**The direct-USB live-path stall is fixed.** Root cause: the TxTask's event
drain checked the UART stream buffer (unused by the USB profile) instead of
the USB pool, so under a sustained decoder flood it called `tx_write` once
per event and blocked up to 500 ms per frame waiting for a pool slot,
starving the command path for minutes. Fix: `la_usb_enqueue_wait()` with a
10 ms bound for live event frames (`EVT_DECODED`/`EVT_BLOCK`/`EVT_OVERRUN`),
`la_usb_tx_space()` as the real drain flow control, and a four-block cap per
TxTask pass so the control path is always serviced. Bench: the 5 s 1 MHz SPI
flood now collects 479,514 events and the device is IDLE at the stream end
(5.1 s); the live suite requires <= 3 s and passes 9 passed/2 skipped (the
relay-ring and TCP-kill injections remain gateway-only). Re-ran: `test-native`
18/18, `release` 22/22, `test-host` 87, USB bulk+transport+zynq burst 16
passed/1 skipped, P6.2 18/18, external matrix 18/18, P9.2 18 passed/1
skipped, U06 drain 1.59-1.63 MB/s, gateway and USB profiles build.

### USB migration execution - U08 and U09 (2026-09-19)

**U08 documentation reconciled; U09 packaging passes, SD boot and tag remain
blocked.** U08: `docs/plan.md` carries the U07 adopt decision,
`docs/protocol.md` documents the chunked `TRACE_CTRL` payload, the enlarged
frame/chunk bounds and `END next total dropped`, `docs/limits.md` records the
direct-USB drain, and `docs/using-the-analyzer.md` marks the direct-USB
profile adopted. README, architecture, RTOS design, wiring, SPI link and
test plan describe the unchanged gateway/S3 transport and needed no edit; the
test plan describe the unchanged gateway/S3 transport and needed no edit; the
portal/manuscript contain no direct-USB adoption claim, so no rebuild was
required. Evidence: `build/reports/usb-u08-doc-review.md`. U09 (gateway
release profile per the defer): `release` PASS 22/22 native tests,
`fw-zynq` gateway PASS, `boot` PASS (`BOOT.BIN` 2,396,368 bytes from the
current FSBL/`la_fw.elf`/bitstream), `package` PASS (BOOT.BIN, ESP32
bootloader/partition/`la_gateway.bin`, host wheel and SHA-256 manifest).
Blocked/not authorized: the `[HUMAN]` SD boot and on-image gates, the `v1.0`
tag and publication, and wiring photos. Evidence:
`build/reports/usb-u09-release.md`.

### Full-USB live pivot - planning (2026-09-19)

**User-directed scope change: USB becomes the default transport and the live
browser view moves to the PC host.** Wrote the execution package
`docs/usb-full-live-pivot.md` with packets V00-V06: scope freeze and
dispositions; host live-bridge contract (reuse the existing
`{"s":slot,"e":[[t,kind,"hex"],...]}` browser message, stdlib-only server,
bounded replay ring); portable host bridge with tests; CLI/browser
integration; USB live end-to-end gates; docs and profile-aware packaging; and
the USB-profile SD release. The gateway stays a supported optional profile
(build, `la_web`, wireless retained); P9.4, P10.1 and the ESP32-S3 transport
port become gateway-profile features, optional/deferred for the USB-first
default. The normative plan carries the pivot note and points to the package.
No code changed in this planning step. Next: V00 scope freeze (record the
dispositions and the open choices: WebSocket vs SSE, single vs multi client,
default profile flag) then V01 contract freeze.

### Full-USB live pivot - V00 scope freeze (2026-09-20)

**V00 complete locally; USB is the default product profile.** The dated
decision and dispositions are in `docs/plan.md` (V00 scope freeze section):
USB is the default transport/profile, the gateway is a supported optional
profile (build, `la_web`, wireless retained), P9.4/P10.1/the ESP32-S3 port are
gateway-profile features (optional/deferred), the live view moves to the PC
host, and the settled choices are **SSE** (reusing the
`{"s":<slot>,"e":[[<t>,<kind>,"<hex>"],...]}` browser message), **single
client first** with a multi-capable hub, and **USB as the default profile**
with `--profile gateway` fully working. Tooling: `fw-zynq`, `jtag-zynq`,
`boot` and `package` default to the USB profile (`la_app_usb`);
`fw-zynq/scripts/make_bootbin.py` and `tools/make_release.py` are
profile-aware, and the ESP32 `.bin` set is optional for the USB profile
(staged and labeled as the gateway profile when present) and required for
`--profile gateway`. Files: `docs/plan.md`, `docs/limits.md`, `README.md`,
`tools/run.py`, `tools/make_release.py`, `fw-zynq/scripts/la_platform.py`,
`fw-zynq/scripts/jtag_load.tcl`, `fw-zynq/scripts/make_bootbin.py`, this
ledger.

Verification: `python -m compileall -q tools host tests fw-zynq/scripts` PASS;
`la_platform.usage`, `make_bootbin.parse_args` and `make_release.parse_args`
report `usb` by default, accept `gateway`, and reject an unknown profile
(`boot blocked`/`package blocked`) PASS; `git diff --check` clean. No
firmware build, load, package or bench result is claimed in V00. The Vitis
workspace currently holds only the gateway `la_app.elf`, so a default `boot`
is expected to report the missing `la_app_usb.elf` until V06 rebuilds the USB
profile. Next: V01 host live-bridge contract (`docs/live-web.md`).

### Full-USB live pivot - V01 bridge contract (2026-09-20)

**V01 complete; the host live-web bridge contract is frozen.** New
`docs/live-web.md`: the bridge owns the analyzer target (one serial port; a
second owner fails honestly), consumes `AnalyzerClient.live(...)` events,
serves `/` (host page), `/events` (SSE) and `/status` (JSON snapshot) on
`127.0.0.1:8080` by default, and sends `LIVE_STOP` plus releases the port on
teardown. Settled: **SSE** (`text/event-stream`, stdlib only; WebSocket
rejected for V02), the browser payload is byte-compatible with `la_web_core`
(`{"s":<slot>,"e":[[<t>,<kind>,"<hex>"],...]}`, 16-byte cap, lowercase hex),
**single client** with `409 Conflict` for a second feed and a per-client
bounded queue (256 messages; drop and count, never block the pump), a
**4096-message bounded replay ring** with drop accounting, one stats message
per second, and the V02/V03/V04 acceptance checks. No wire-protocol change
(`docs/protocol.md` untouched) and no hardware result claimed. Verification:
documentation-only change; `git diff --check` clean. Next: V02 bridge core
with host tests.

### Full-USB live pivot - V02 bridge core (2026-09-20)

**V02 complete; the portable bridge and its host tests are in.** New
`host/la_live_web/`: `core.py` (a byte-for-byte port of `la_web_core`'s
`begin/event/end` formatting with the 16-byte data cap and no partial message;
the `LiveEventHub` with the 4096-message bounded replay ring, drop accounting
and the single-client policy), `server.py` (stdlib `ThreadingHTTPServer` with
`/`, `/events` SSE and `/status`, `409` for a second feed, per-client bounded
queue) and `static/index.html` (host page using `EventSource`; the data
formatting is unchanged from the gateway page). `AnalyzerClient.live()` gained
the additive `on_event(frame_events)` sink (dict records, called for each
decoded frame including the LIVE_STOP drain) so the bridge publishes in real
time. `pyproject.toml` packages `host.la_live_web` with the static page as
package data.

Verification: `python tools/run.py test-host` PASS 103 passed (was 87; 16 new
tests: native formatting vectors from `tests/native/esp32_web_host_tests.c`,
16-byte cap, truncation, ring overflow/replay, `409` conflict, slow-client
drops without blocking, `publish_frame` limit, stats shape, page/status/SSE
serving and the mock-analyzer live integration) and the full host suite is
green. No hardware result claimed; V03 wires the CLI and the [HUMAN] browser
smoke. Next: V03 (`la-cli live --serve` / `la-web`,
`docs/using-the-analyzer.md`).

### Full-USB live pivot - V03 CLI integration (2026-09-20)

**V03 implementation complete; the browser smoke is [HUMAN].**
`la-cli live ... --serve [--bind HOST] [--port N]` now starts the bridge
instead of printing events, and the new `la-cli web` command plus the `la-web`
console script are the standalone entry points; both share one implementation
(`_serve_live`, `_prepare_live`, `_configure_patgen`) and print the bound URL.
The serve path opens the target first (one owner), counts events without
retaining them, caps the feed exactly at `--count` even when the device's final
batch overshoots, prints the session summary, sends LIVE_STOP and releases the
port. The Windows bind is exclusive (`allow_reuse_address = False`) so a second
server reports `cannot bind the live web view` instead of silently sharing the
port. Files: `host/la_cli/commands.py`, `host/la_live_web/server.py`,
`host/tests/test_live_web.py`, `pyproject.toml` (`la-web` script),
`docs/using-the-analyzer.md` section 7 + command reference.

Verification: `python tools/run.py test-host` PASS 107 passed (20 in
`test_live_web.py`, including `live --serve` and `la-web` against the mock
analyzer, the `--count` feed limit and the bind-failure path). The [HUMAN]
browser smoke (open `http://127.0.0.1:8080`, render patgen UART events, reload
reconnect) has not been performed in a browser; it is scheduled with the V04
bench run. Next: V04 USB live E2E gates over `serial://COM13`.

### Full-USB live pivot - V04 USB live E2E gates (2026-09-20, bench)

**V04 passes on the bench; the [HUMAN] visual browser smoke is still open.**
New `tests/e2e/test_usb_live_web.py` runs a real `LiveWebServer` +
`LiveEventHub` with an SSE reader thread against `serial://COM13` (USB-profile
device, fw `0x00090000`, PL `0x4C410002`). `python tools/run.py test-e2e --
--target serial://COM13 --live-seconds 60 -k usb_live_web` PASS 8/8 in 90.55 s.
Numbers: 60 s UART 606,411 events / 606,411 feed events (0 client drops, 0
format drops, 0 overruns, 0 dropped segments, 606,405 decoded bytes, 124.7 kB/s
wire, IDLE); isolated byte -0.6 ms against the 25 ms budget; SPI 10 kHz 4,380
events with 0 frame errors; I²C 10 kHz 5,347 events / 668 complete frames / 0
NACKs; `--stop-on` "LA" match at sample 72,678 in 0.172 s; compact EVT_BLOCK
29,472 events at 57.1 kB/s; 1 MHz SPI overload served 14,469 events with 0
client drops and recovered to IDLE in 0.05 s; bridge close/reopen delivered
exactly 32 events on each session and `ping` still answered. Ring evictions
(`ring_dropped`, 25,841 in the 60 s run) are the bounded replay bound, not feed
loss. Evidence: `build/reports/usb-v04-live-web.md`,
`build/reports/usb-v04-live-web-evidence.txt` (helper
`build/reports/usb-v04-live-web-measure.py`), `build/reports/test-e2e.json`.
Still open: the [HUMAN] browser render/reload check, and the re-run against the
V06-packaged image. Next: V05 docs and profile-aware packaging.

### Full-USB live pivot - V05 docs and packaging (2026-09-20)

**V05 complete; the USB-first default and the host bridge are documented and
packaged.** `README.md`, `docs/using-the-analyzer.md` (section 7 rewritten for
the host live view plus `--serve`/`web`/`la-web` in the command reference),
`docs/windows-dev.md`, `docs/architecture.md`, `docs/test-plan.md` (P9.1 drain
row, P10.1 gateway label, a new V00-V04 pivot row, P10.4 profile-aware) and
`docs/limits.md` now state the USB default and the optional gateway profile.
Packaging: `pyproject.toml` ships `host.la_live_web` with `static/index.html`
as package data and both `la-cli`/`la-web` scripts; `tools/make_release.py`
verifies the bridge files and both entry points in the wheel, requires the
ESP32 `.bin` set only for `--profile gateway` (optional and labeled for USB),
and `fw-zynq/scripts/make_bootbin.py` is profile-aware. The `web-artifact/`
portal was updated only where the transport/default claim changed
(`snapshot.ts`, `subsystems.ts`, `roadmap.ts`, `verification.ts`,
`components/host-tools.ts`); measured evidence labels were preserved.

Verification: `python -m compileall -q tools host tests fw-zynq/scripts` PASS;
`python tools/run.py test-host` PASS 107 passed; portal `npm run validate`
PASS (7 subsystems, 78 components, 21 registers, 24 protocol messages, 11
roadmap phases, 55 glossary terms, 182 coverage entries); portal `npm run
build` completed and emitted all routes (not published); `git diff --check`
clean. Evidence: `build/reports/usb-v05-docs.md`. The full USB-profile
`boot`/`package` run is V06. Next: V06 release (build/load/package the USB
profile, [HUMAN] SD boot and tag authorization remain separate gates).

### Full-USB live pivot - V06 release build (2026-09-20, bench/tooling)

**V06 build and packaging pass; the [HUMAN] SD boot and the tag remain
blocked/unauthorized.** `python tools/run.py fw-zynq` (default profile) built
`la_app_usb.elf` (text 210,864 / data 3,720 / bss 453,504, SHA-256
`14A002EA...B4B567`); `boot` produced **BOOT.BIN 2,412,784 bytes** with
`BOOT profile: usb (application la_app_usb)` and staged `la_fw.elf`
1,196,464 bytes whose SHA-256 matches the built ELF; the staged `fsbl.elf`
matches the workspace FSBL (`9CF0104E...`); `package` wrote the manifest with
`"profile": "usb"`, the optional gateway ESP32 set (bootloader 21,056 B,
partition table 3,072 B, `la_gateway.bin` 950,768 B) and the 80,340-byte wheel
(`6CA23B6C...`) that now contains `host/la_live_web/*` plus the `la-cli` and
`la-web` console scripts; `release` PASS 22/22. `jtag-zynq` (default profile)
loaded the freshly built USB ELF and `ping serial://COM13` answered
`fw 0x00090000 pl 0x4C410002`; the V04 bridge suite re-ran **8/8** against that
image. Blocked, not claimed: the **[HUMAN] SD boot** (JP5=JTAG on the bench)
and the **`v1.0` tag/publication** (no release authorization). Evidence:
`build/reports/usb-v06-release.md`; per-target JSON under `build/reports/`.
The gateway profile build path is unchanged and its last build/bench evidence
(U05) stands; the V00 flip only changed the default selection.

### Full-USB live pivot - V00-V06 closed (2026-09-20)

**The pivot is implemented end to end.** V00 scope freeze and USB default,
V01 bridge contract, V02 portable bridge with host tests, V03 CLI/browser
integration, V04 USB live gates 8/8, V05 docs and profile-aware packaging,
V06 USB-profile build/package/release. Open items, all explicit: the [HUMAN]
visual browser smoke, the [HUMAN] SD boot of the USB-profile BOOT.BIN, tag
authorization, and the optional gateway-profile work (P9.4 Wi-Fi tuning, the
ESP32-S3 port) retained as deferred features. No wireless or browser feature
was removed and the U00-U09 evidence is untouched.

### Full-USB live pivot - V07 interactive browser Start/Stop (2026-09-20)

**User-directed follow-up: the bridge now starts idle and the page controls
the session.** New `host/la_live_web/session.py` (`LiveSessionController`:
`idle -> streaming -> stopping -> idle`, restartable, one session at a time,
re-arms the patgen before each start); `LiveWebServer` gained
`POST /control/start` (409 while streaming) and `POST /control/stop`
(idempotent), and `/status` now reports `session`, `sessions`, `error`,
`decoder`; the page has **Start**/**Stop** buttons, shows the session state and
error, and clears the panes on start. `AnalyzerClient.live()` gained the
additive `stop_event` polled by the pump so a Stop from another thread ends the
session promptly and still sends LIVE_STOP. The CLI serves interactively by
default (`la-cli live --serve` / `la-web` print the URL and wait);
`--autostart` keeps the scripted one-session behavior (tests, smoke). The
interactive path keeps draining after an `EVT_OVERRUN`; autostart ends on it.

Verification: `python tools/run.py test-host` PASS 109 passed (22 in
`test_live_web.py`, including controller start/stop/restart, the HTTP control
endpoints, 409-on-second-start, idempotent stop and the page/status shape);
bench with the V06-built USB image on `serial://COM13`: `la-web --patgen uart`
starts idle (`session=idle events=0`), Start streams (38,486 events / 1,880
messages in 4 s, 0 dropped), Stop returns `{"state":"idle","stopped":true}`
with the device back in IDLE, a second Start streams again (67,493 events,
`sessions=2`), and the V04 bridge suite still passes 8/8. `python -m
compileall` and `git diff --check` clean. Files: `host/la_live_web/{session.py,
server.py,core.py,static/index.html,__init__.py}`, `host/la_cli/{client.py,
commands.py}`, `host/tests/test_live_web.py`, `docs/live-web.md` (revised
V07), `docs/using-the-analyzer.md`, `docs/windows-dev.md`, `README.md`, the
portal `components/host-tools.ts`, this ledger. The [HUMAN] browser click
smoke (open page, Start, watch lines, Stop, reload reconnect) remains open and
is now the exact documented procedure.

### Full-USB live pivot - V08 page rendering performance (2026-09-20)

**User-reported freeze fixed.** The page went unresponsive during a stream
because it inserted one DOM node per record via `insertAdjacentHTML` and, once
the 1500-line cap was hit (~1.2 s at the measured rate), rebuilt the whole pane
with `innerHTML.split().slice().join()` on every message. The renderer now
coalesces records and draws once per animation frame (`requestAnimationFrame`,
2000-record budget), builds DOM nodes directly (no HTML parsing), keeps 2000
lines per pane and trims by removing child nodes in chunks of 500 (no pane
rebuild), preserves the scroll position when the user has scrolled up, and
counts over-budget records as `display-skipped` in the footer while the stats
bar keeps the authoritative received count. The measured load is ~9,952
events/s in ~491 messages/s over `serial://COM13` (115200 UART), so a frame
carries ~166 records and stays far inside the budget.

Verification: `python tools/run.py test-host` PASS 109 passed (the page test
now guards `requestAnimationFrame`/budget/child-trim and rejects
`insertAdjacentHTML` and `innerHTML.split`); the page script passes
`node --check`; bench `la-web` on the V06 image: 10 s stream 99,519 events /
4,913 messages, 0 client drops, Stop returned idle. Files:
`host/la_live_web/static/index.html`, `host/tests/test_live_web.py`,
`docs/live-web.md` (3b rendering budget), this ledger. The browser-side
smoothness check remains [HUMAN]: restart the bridge (the page is read at
server start) and confirm scrolling/clicking during a stream.

### Full-USB live pivot - V09 multi-decoder and per-slot control (2026-09-20)

**User-directed: up to three decoders (UART/SPI/I²C) with independent
Start/Stop per pane.** Host: repeatable `--decoder` (≤3, slot order),
`pack_decoder_off(slot)`, `AnalyzerClient.live()` accepts a list of configs
plus a `command_queue` drained by the pump mid-session, a per-slot
`LiveSessionController` (`start_slot`/`stop_slot`/`stop_all`/`start_all`),
`POST /control/start|stop?slot=N` (no slot = all; 400 invalid; 409
unconfigured/already streaming), and a page with per-pane Start/Stop, state
pills and a header Stop all. Firmware: `DECODER_CFG` is accepted in view CAPT
(streaming), a new `LA_CTRL_DECODER_CFG` carries one slot's config through
CmdTask to AcqTask, and `acq_apply_decoder_cfg` updates the segment consumer
count and hands the config to that slot's decoder task (AcqTask remains the
only writer). **Bug found on the bench and fixed:** the device latches decoder
configs across sessions, so a new session now states its slot set explicitly
(enabled configs + OFF for the other two), preventing a stale slot from
streaming. One honest incident: a helper script crashed before stopping its
bridge, leaving a stale `la-web` owning COM13; a second bridge then produced
USB write errors and a device re-enumeration. After killing the stale process
the 6-cycle loop test and the final evidence run were clean with continuous
uptime; no firmware defect in that incident.

Verification: `test-native` 18/18 (streaming accept/OFF/other-slot cases);
`test-host` 114 passed; `fw-zynq`/`jtag-zynq` PASS (USB default) and `ping`
answered; bench `serial://COM13`: start slot 0 → `[17723,0,0]`, start slot 1
mid-session → `[38264,19871,0]`, stop slot 0 → slot 0 flat at `44017` while
slot 1 grew `30181→50114`, stop last → idle `error=None`, I²C in slot 2 →
187,053 events with START/STOP/ADDRESS/ACK/BYTE kinds; V04 regression 8/8
(60 s); `release` 22/22; `boot` USB BOOT.BIN 2,413,488 bytes; `package` wheel
86,443 bytes. Evidence: `build/reports/usb-v09-multi-decoder.md`,
`build/reports/v09-slots-evidence.txt`. The [HUMAN] browser click-through of
the per-pane buttons remains open. Files: `fw-zynq/app/src/tasks/{la_cmd_task.c,
la_acq_task.c,la_cmd_dispatch.c}`, `fw-zynq/app/src/la_app.h`,
`tests/native/cmd_dispatch_host_tests.c`, `host/la_cli/{protocol.py,client.py,
commands.py}`, `host/la_live_web/{session.py,server.py,core.py,static/index.html}`,
`host/tests/{test_live_web.py,mock_analyzer.py}`, `docs/live-web.md`,
`docs/using-the-analyzer.md`, `docs/test-plan.md`, `README.md`, the portal
`components/host-tools.ts`/`subsystems.ts`/`verification.ts`, this ledger.

### Full-USB live pivot - V09 follow-up: page freshness (2026-09-20)

**User-reported "unconfigured" panes traced to a stale browser page, not the
bridge.** Verified on the bench: the server serves the V09 page (with
`applySlots`), `/status` and the SSE stats message both carry
`slots: ["idle","idle","idle"]` and the three `slot_labels`, and a Node
DOM-stub run of the served page script (`build/reports/v09-page-logic-check.js`)
turns the pills to `idle`, enables Start and fills the labels from that stats
message (`PAGE LOGIC OK`). Robustness/diagnosis added: the bridge now sends
one stats message **immediately on SSE attach** (the page shows real slot
state without waiting up to a second), and the page header shows
`page v09 | server v09` from the stats `bridge` field so a stale cached page
is obvious. Verification: `test-host` 114 passed; the Node page-logic check
PASS. The user must restart the bridge and hard-refresh the tab (Ctrl+Shift+R);
if the marker is missing or older, the browser is serving a cached page. No
firmware or protocol change. Files: `host/la_live_web/{core.py,server.py,
static/index.html}`, `host/tests/test_live_web.py`, `docs/live-web.md`, this
ledger.

### Full-USB live pivot - V09 fix: slot pill class wipe (2026-09-20)

**Root cause of the persistent "unconfigured" panes found and fixed.** The
page's first `updateSlotUI()` at load set `pill.className = "pill"`, deleting
the `slot-state` class that is also the selector for every later update; the
next update (stats or Stop all) then threw
`Cannot set properties of null (setting 'textContent')` and the panes stayed
unconfigured. Fixed by preserving the class with
`pill.classList.toggle("on", ...)` and null-guarding the label/button lookups.
Verified with a real-browser DevTools-protocol run against a live bridge
(`build/reports/v09-cdp-check.js`): `pillCount=3`,
`pills=["idle","idle","idle"]`, `startDisabled=false`, label
`uart:rx=0,baud=115200`, marker `page v09 | server v09`. The same check run
against the previous page revision reproduces the exact user error
(`build/reports/v09-page-logic-check.js` against `build/tmp/index-old.html`).
Host tests 114 passed with a new guard that the slot pill keeps its class.
Files: `host/la_live_web/static/index.html`, `host/tests/test_live_web.py`,
this ledger. The user's running bridge must be restarted and the tab
hard-refreshed to pick up the fix.

### Full-USB live pivot - V10 pattern-generator traffic variants (2026-09-20)

**User-directed: eight new `la_patgen` modes for realistic traffic and error
cases.** PATGEN_CTRL.MODE 7-14: `uart_text` (`Hello, LA!\r\n`), `uart_bin`
(0x00..0x0F ramp), `uart_ferr` (low stop bit → FRAME_ERR), `spi_multi`
(0xDEADBEEF under one CS), `spi16` (two 16-bit words 0x1234 0xABCD),
`i2c_read` (0xA1 + master NACK), `i2c_nack` (data NACK), `i2c_alt`
(address 0x3C, bytes 0x4F 0x4B). Modes 0-6 are unchanged; the generator still
drives one protocol at a time. Files: `docs/regmap.yaml` (+`gen`),
`fpga/rtl/la_patgen.sv`, `fpga/tb/tb_la_patgen.sv`,
`common/la_proto/src/la_proto.c` (mode max now `I2C_ALT`),
`tests/native/native_tests.c` (mode 7/14 accepted, 15 rejected),
`host/la_cli/{protocol.py,commands.py}`, `docs/using-the-analyzer.md`, the
portal `fpga-capture.ts` (`la-patgen`), this ledger.

Verification: `lint` 12/12 zero warnings; `sim` 12/12 with the extended
patgen testbench (generalized UART/SPI/I2C decode tasks and eight new
scenarios); `test-native` 18/18; `test-host` 115; `bit` PASS (timing met, DRC
clean); `fw-zynq`/`jtag-zynq` PASS with the new XSA and protocol validation;
bench over `serial://COM13`: all eight modes decode (text found, ramp found,
133 FRAME_ERRs, 4C41, DEADBEEF, 16-bit words, address 0x50/0x3C, 57 NACKs);
`release` 22/22; `boot` USB BOOT.BIN and `package` wheel 87,338 B rebuilt.
Evidence: `build/reports/v10-patgen-variants.md`,
`build/reports/v10-patgen-evidence.txt`. Notes: the spi16 evidence script read
the 2-byte payload big-endian (decoder emits little-endian bytes; values are
0x1234/0xABCD), and a session's first events may include a partial-frame
FRAME_ERR by design. The [HUMAN] SD boot remains separate.

### Unified analyzer UI - planning (2026-09-20)

**Planning complete; implementation not started.** User-directed scope: one
host-served browser workspace (`la-ui`) covering connection, burst capture
with a Saleae-style waveform viewer, live decoded events on a time-aligned
timeline, host-side capture decoding, exports, trace and the pattern
generator. Wrote `docs/unified-ui-plan.md` (packets W00-W09 with entry/exit
gates) and added the pointer in `docs/plan.md`. Frozen decisions were settled
with the user before writing (section 2 of the package): local browser page
via a stdlib-only server; vanilla JS + Canvas with no build step; full
waveform feature set (zoom/pan, ruler, cursors A/B, channel layout, capture
decode overlays, trigger/pre-post markers, edge search, screenshots, view
persistence); live stays decoded-events-only; host-side capture decode via
the existing `la_native` C ABI DLL; 1M-sample viewer target with a warning
above; direct-USB only (`serial://COMx`) with the ESP32/gateway path rejected
and no `LINK_CFG` control; single workspace; `la-cli` kept; existing PL
trigger only; browser downloads; offline `.sr`/`.vcd`/`.csv` open; one
capture at a time; legacy page kept at `/legacy`; host tests + [HUMAN]
browser smoke + bench E2E; `la-ui` entry point; timeline trace; Chrome/Edge;
dark theme; 100,000-event live bound; session-memory capture store; client
canvas PNG; auto-stop on mode switch; both CSV kinds; explicit Decode button.
The package is explicitly host-only: no firmware, PL, register-map or
protocol change, and any device-side need is a stop-and-ask condition. Files:
`docs/unified-ui-plan.md` (new), `docs/plan.md`, this ledger. Verification:
documentation-only change; `git diff --check` clean; the packet list, gates
and frozen-decision table were reviewed against the current host code
(`host/la_cli`, `host/la_live_web`, `common/la_decode`, `CMakeLists.txt`,
`tools/run.py`) and the recorded V04/V09 evidence. No firmware build, bench
run or UI result is claimed. Planning commit
`8b45a9be3af0eef5241b1ac2ac5cb5199b8b1bd4` was pushed to `origin/main`;
`git ls-remote origin refs/heads/main` matched `git rev-parse HEAD` and the
staged set contained exactly the three intended documents. Next: await the
user's execution instruction; begin W00/W01 only when authorized.

### Unified UI - W01 capture store, decimation and decode (2026-09-20)

**W01 complete locally.** New `host/la_ui/` package core: `store.py`
(session-memory `Capture`/`CaptureStore`, lazy numpy sample expansion for raw
and transition captures, >1M-sample warning), `render.py` (min/max envelope
binary `LAEN` plus exact sample windows), `decode.py` (ctypes binding to the
`la_native` C ABI for UART/SPI/I²C, spec parser matching the CLI grammar,
`LA_NATIVE_LIB`/`build/native` resolution, a 16M-sample decode bound and an
explicit `NativeLibraryMissing` error). Files: `host/la_ui/{__init__,store,render,decode}.py`,
`host/tests/{test_ui_store,test_ui_decode}.py`, this ledger. Verification:
`python tools/run.py test-native` PASS 18/18 (builds the DLL);
`.venv\Scripts\python.exe -m pytest host/tests -q` PASS 129 passed (14 new:
envelope parity against a Python reference at 1/2/4/8/16 buckets, clamping,
empty captures, windows, store lifecycle, UART/SPI/I²C capture decode parity
with `tools/models/decoders.py`, multi-slot decoding, spec errors and the
missing-library path). No device, bench or UI result is claimed. Next: W02
job runner, HTTP API and exports.

### Unified UI - W02 job runner, HTTP API and entry point (2026-09-20)

**W02 complete locally.** New `host/la_ui/jobs.py` (single-slot background
`JobRunner` with progress, result and cooperative cancel), `export.py`
(`.sr`/`.vcd` through the unchanged CLI writers, event and sample CSV with a
`# la-ui` metadata line), `open.py` (`.sr`/`.vcd`/`.csv` offline openers,
wired now and tested fully in W07), `server.py` (`UiState` connection/store/
jobs/live controller, the `/api/*` routes, `/legacy`, and the unchanged SSE,
`/status` and `/control/*` contract with an exclusive bind) and a temporary
`host/la_ui/static/index.html` shell. `la-cli ui` and the `la-ui` console
script share one implementation; `pyproject.toml` packages `host.la_ui` with
its static data. `host/la_cli/trace.py` gained `timeline_model()` for the UI
canvas. Files: `host/la_ui/{jobs,export,open,server}.py`,
`host/la_ui/static/index.html`, `host/la_cli/{commands,trace}.py`,
`pyproject.toml`, `host/tests/test_ui_api.py`, this ledger.

Verification: `.venv\Scripts\python.exe -m pytest host/tests -q` PASS 144
passed (15 new: page and `/legacy` routes, legacy `/status` fields,
connect/ping/status/disconnect, capture job plus meta/envelope/samples/export/
delete, capture validation, decode job and annotations, `.sr` export to open
round trip, live config and per-slot control with a real SSE reader, bad-spec
rejection, patgen route, trace job and model, job-runner single-slot and
cancel); `la-cli ui --help` PASS. The legacy V09 suite still passes unchanged.
No device or bench result is claimed and the workspace page is a W03
deliverable. Next: W03 workspace shell.

### Unified UI - W03 workspace page, waveform renderer and live timeline (2026-09-20)

**W03 complete locally (covers W03-W04 page scope).** New
`host/la_ui/static/{index.html,style.css,app.js,waveform.js,timeline.js}`
replace the temporary shell: dark single-workspace layout with the connection
bar, capture/decoder/channel/export/trace/patgen panels, the waveform canvas,
three live panes and the trace canvas. `waveform.js` renders the `LAEN`
envelope with zoom/pan, ruler, cursors A/B and measurements, channel
labels/colors/order/heights, trigger and pre/post markers, edge and 0/1/x
pattern search, annotation rows and a PNG screenshot; `timeline.js` has the
bounded (100k events/slot) live event timeline with auto-follow and the trace
lane/spans/markers renderer; `app.js` owns connection, capture jobs, decode,
exports, live per-slot control over SSE, trace, patgen, offline open,
`localStorage` settings and the page/server version marker. New
`host/tests/test_ui_page.py` and `host/tests/page_check.mjs` (a real DOM-stub
run under Node: `node --check` for every script, an unknown-id audit against
index.html, and a scripted exercise of the renderers).

Verification: `.venv\Scripts\python.exe -m pytest host/tests -q` PASS 149
passed (5 new page tests); `node host/tests/page_check.mjs` PAGE CHECK OK;
real headless Chrome via the DevTools protocol served the page, injected a
synthetic capture and reported 3 live panes, 8 channel rows, 30,198 lit
canvas pixels, the `page w10 | server w10` marker and **zero console errors**
(evidence `build/reports/ui-w03-browser-smoke.txt`). The [HUMAN] click-through
remains scheduled with W09. Next: W05/W06/W07 wiring tests, docs and bench.

### Unified UI - W05/W06/W07 export, trace and offline-open coverage (2026-09-20)

**W05-W07 complete locally.** Export coverage: `test_ui_export.py` (valid
sigrok zip with metadata and `logic-1-1`, VCD probes/timescale/rate comment,
event CSV rows sorted with the `# la-ui` metadata line, sample CSV rows and
round trip, `.sr`/`.vcd` sample round trips, transition-capture export
reconstruction). Trace coverage: `test_ui_trace.py` (the `timeline_model`
lanes/spans/marker groups/ISR lane-less markers/ordering). Offline open:
`test_ui_open.py` (exact `.sr` round trip, `.vcd` rate recovery from the
la-cli comment and the GCD heuristic without it, the trailing-constant-run
warning, sample CSV round trip, event CSV annotations, bad zip/header/
timescale/empty inputs, probe-count mismatch). Page: the annotation row,
trace lanes and the live stats path are exercised by `page_check.mjs`
(annotationSlots, traceLanes, stats/statsPills/slotStartDisabled checks), and
`app.js` now shares one `applySlots()` for `/api/state` and SSE stats so slot
pills and buttons update promptly. Files: `host/la_ui/static/app.js`,
`host/tests/{test_ui_export,test_ui_trace,test_ui_open}.py`,
`host/tests/page_check.mjs`, this ledger.

Verification: `.venv\Scripts\python.exe -m pytest host/tests -q` PASS 165
passed (16 new); `node host/tests/page_check.mjs` PAGE CHECK OK. The VCD
trailing-constant-run limitation is inherent to the format and is surfaced as
a warning; `.sr` is the exact-count path. Next: W08 docs, packaging and the
regression suite.

### Unified UI - W08 docs, portal and packaging (2026-09-20)

**W08 complete locally.** Documentation: `README.md` (doc map plus a `la-ui`
quick start), `docs/using-the-analyzer.md` (new section 7 Unified UI, later
sections renumbered, command-reference row), `docs/windows-dev.md` (Unified UI
subsection), `docs/architecture.md` (host-only layer note),
`docs/test-plan.md` (W00-W09 row), `docs/live-web.md` (unified-UI note; the
legacy contract is unchanged) and `docs/unified-ui-plan.md` (implementation
status). Packaging: `tools/make_release.py` now requires the `host/la_ui`
package, its static files and the `la-ui` entry point in the wheel. Portal:
`web-artifact/lib/docs/components/host-tools.ts` gained the `unified-ui`
component, `coverage.ts` the `host/la_ui` and plan paths, and `subsystems.ts`
the host-tools boundary/secondary text.

Verification: portal `npm run validate` PASS (7 subsystems, 79 components, 21
registers, 24 protocol messages, 11 roadmap phases, 55 glossary terms, 186
coverage entries); `npm run build` completed and emitted all routes (not
published); `python tools/run.py package` PASS with the wheel (133,161 bytes)
containing `host/la_ui/*` and the `la-ui` entry point. Evidence:
`build/release/release-manifest.json`. Next: W09 bench E2E and browser smoke.

### Unified UI - W09 bench E2E, browser smoke and release tooling (2026-09-20)

**W09 complete on the bench; the [HUMAN] visual click-through remains the only
open UI item.** New `tests/e2e/test_ui.py` drives the real `UiServer` API
against `serial://COM13` (fw 0x00090000, pl 0x4C410002):
`python tools/run.py test-e2e -- --target serial://COM13 -k ui` PASS 6/6 in
11.6 s - connect/ping/state, a 64,000-sample patgen counter capture with
envelope/detail/export/offline reopen, a host decode of a patgen UART capture
(0x4c/0x41 BYTE events), per-slot live control with a real SSE reader (slot 0
UART events, slot 1 joined mid-session, slot 0 stopped alone, a capture
auto-stopped the session), the trace dump model, and a **1,000,000-sample
capture in 1.58 s** (32,780-byte envelope, no warnings).

The V04 regression initially failed the SPI/I²C gates because the UI's live
test left a latched SPI slot on the device and the older V04 tests do not
state their full slot set (the V09 latch). Fixed at the source:
`AnalyzerClient.configure_decoder()` sends one DECODER_CFG and waits for its
ACK, `UiState.disconnect()` states OFF for all three slots before closing, and
the mock analyzer ACKs DECODER_CFG in every scenario like the real device.
After the fix: `test-e2e --target serial://COM13 --live-seconds 60 -k
usb_live_web` PASS 8/8 (90.1 s), and a UI run followed immediately by the V04
I²C gate passes without manual cleanup.

Browser: a headless-Chrome DevTools-protocol run against the real device
(UiServer connected to COM13) connected, captured 64,000 samples (28,461 lit
canvas pixels), decoded 65 events, rendered the annotation row, streamed
37,656 live events and returned to idle with **zero console errors and zero
failed fetches** (`build/reports/ui-w09-browser-bench.txt`); the local page
smoke `build/reports/ui-w03-browser-smoke.txt` remains valid.

Release regression: `test-native` 18/18, `release` 22/22, `test-host` 166
passed, `boot` PASS (BOOT.BIN 2,413,488 bytes, USB profile), `package` PASS
(wheel 133,161 bytes with `host/la_ui` and the `la-ui` entry point), portal
validate/build PASS. The `[HUMAN]` browser click-through (open the page and
click through capture/decode/live/trace/export visually) and the `[HUMAN]` SD
boot of the USB BOOT.BIN remain open; no `v1.0` tag was created. Evidence:
`build/reports/ui-w09-bench.txt`, `build/reports/ui-w09-browser-bench.txt`,
`build/reports/test-e2e.json`.

| Item | Current value |
|---|---|
| Observed date | 2026-10-18; use docs/current-handover.md for the operational snapshot |
| Project root | `C:\Users\lingg\Projects\Logic-Analyzer\Source_Code` |
| Plan source | `docs/plan.md` is normative; the root `logic-analyzer-implementation-plan-v0.4.md` is the historical v0.4 snapshot. The 2026-09-17 USB planning addendum is in `docs/plan.md` only. |
| Branch | `main` |
| Last implementation change | Completed W11 waveform-first UI, actual transition preview, reset/cancellation, external S3 UART/SPI/I²C demos, and both class deliverable source sets |
| Remote | `origin` -> `https://github.com/linggaaradhanas22/logic-analyzer.git` |
| Remote state | User authorized all-source commit/push on 2026-10-18; final commit and remote verification are recorded in the closeout at the top of this ledger |
| Current pending work | No implementation work remains in the delivered USB-first class-project scope. Website deployment, public USB identity/tagged release, optional gateway SPI fault/tuning, SUMP and 200 MS/s are deferred. See docs/current-handover.md. |
| Workflow | Native Windows only; use `python tools/run.py <target>` |
| Phase | Class project complete (2026-10-18), including W11 and both class report/presentation/workbook sets. W11 SD cold boot PASS (E2E 6/6, browser/ESP32 10/10). Historical P0-P10 measurements remain below; optional/stretch limits are not presented as completed. |

Generated environments, reports, logs and build outputs are intentionally
ignored by Git. The reports described below are local evidence and must be
recreated after a clean checkout.

## Phase Matrix

This is the **historical P0-P10 close-out matrix**. Dated measurements and
then-open entry work are preserved; they are not the current next-action list.
W11 and the 18 October SD refresh are summarized at the top and in
`docs/current-handover.md`. Do not resume completed bring-up from old cells.

| Phase | Status | What exists | Remaining entry work |
|---|---|---|---|
| P0 Environment and scaffolding | Complete locally | Native runner, checker, CI scaffold, locks, CMake tests, Questa harness, ESP-IDF smoke build and board query | Hosted CI and physical hardware gates are not evidence of completion |
| P1 Specifications | Complete | Complete register/protocol/RTOS/decoder/wiring specifications, generated constants, C protocol library, UART/SPI/I2C decoders, independent models and native parity tests | Begin P2 RTL |
| P2 FPGA RTL | Complete locally | `la_input_sync`, `la_sampler`, `la_trigger`, `la_pretrig_ring`, `la_txn_encoder`, `la_raw_packer`, `la_capture_ctrl`, `la_axis_fifo`, `la_patgen`, `la_regs`, `la_top` with 12/12 self-checking Questa tests; P2.12 gates reviewed (zero warnings; `gen --check` clean); BRAM-friendly rework (synchronous-read ring/FIFO with one-beat output registers, enable-latched patgen periods) verified in Questa and used by the P3 bitstream; 2026-09-10 burst-close rework: `la_capture_ctrl.pkt_words_o` gives the raw packer the exact burst packet word count, the packer tags the final word with tlast (word-aligned bursts close their DMA packet), and `la_pretrig_ring` gates the first run cycle so a leftover presented sample cannot leak into a new capture; raw bursts are now exactly K+POST samples | Open caveats: regress runner has no per-test Python vector hook yet (independent Python reconstruction rides on the P4.10 host path); Questa FSE quirks documented in docs/test-plan.md |
| P3 Vivado integration | Complete on bench | Board files, full PS7/SmartConnect/SG-DMA BD, constraints, `la_top` heartbeat output, and self-checking blink cadence test. The reset-fixed 2026-09-10 build passed timing (WNS +0.255 ns, WHS +0.024 ns), DRC and bitstream/XSA generation; JTAG programming passed and the user observed LD0 blinking at 1 Hz. Root cause of the prior dark LED and GP0 timeouts was `proc_sys_reset/aux_reset_in` (active-low) tied to constant 0, holding all PL peripherals in reset; it is now tied high. | Proceed with P4 target gates; retain JP5=JTAG for development and use SD only for release boot verification. |
| P4 Zynq FreeRTOS core | Complete on bench | `lap` platform + full application: `la_app.{h,c}` static tasks/queues/stream buffers; `drivers/la_irq` (FreeRTOS port handler install via `la_irq_connect()` SDT workaround, PL ID 29 derivation, notification-only ISRs, latency stats), `drivers/la_dma` (finite ordinary SG chain per ARM, reset/rebuild, actual lengths + RXEOF, error abort), `drivers/la_link_uart` (UART0 EMIO 8N1, FCM RTS/CTS, RX StreamBuffer, TX staging single in flight; 921600 at P4, 2 Mbps since the P7.6 gate), `tasks/la_acq_task` (CONFIG/ARM/ABORT/timeout, IRQ + DMA, TxJob handoff with buffer-busy gate, ARM preserves the sticky SELFTEST/STREAM bits, abort soft-resets the PL datapath), `tasks/la_cmd_task` (parser + dispatch + queue mapping + PATGEN_CFG, ACK posted before the register latch), `tasks/la_tx_task` (window, chunk map, packer, CRC, retransmit), `tasks/la_status_task` (watermarks/counters/latency); pure cores `la_burst_plan` and `la_irq_stats` added with host tests; additive `PATGEN_CFG` (0x17) protocol extension; `tests/hil/test_zynq_serial.py` + `hil` runner target (LA_HIL_PORT) written. 2026-09-10 DMA/PL fixes: `C_SG_INCLUDE_STSCNTRL_STRM=0` in the BD (status stream tied off had stalled every S2MM transfer), `la_pretrig_ring.stream_en` separates a PRE=0 burst from live pass-through, and the raw packer closes its counted final word with tlast. Bench: the new `tests/e2e` burst suite passes 7/7 (consecutive runs, four-BD 200 KB chain, short raw/txn tails, abort while armed, abort mid-capture, capacity NAK) with real counter data, exact sample counts, zero CRC/format errors and no overflow; the earlier 16-BD `raw 1000000 @1M` capture is all strictly consecutive | P4.10 HIL via a USB-UART adapter is optional now that the ESP32 gateway has proven the on-wire path; P4.3 DMA coherency is proven by the counter captures (DMA writes followed by CPU readback) |
| P5 ESP32 bridge | Complete on bench | `la_board` pins the confirmed DOIT ESP32 DevKit V1 (classic ESP32, GPIO16 RX/17 TX/21 CTS/22 RTS) and makes the link baud a Kconfig option (`LA_LINK_BAUD`, default 2000000 since the P7.6 gate); `la_proto` compiles the shared protocol library for xtensa; `la_link` has the UART2 RTS/CTS driver (RX pull-up included) plus LinkRx/LinkTx tasks over static 32/16 KiB NOSPLIT rings (8 KiB live mirror ring; P10 consumer open); `la_net` has Wi-Fi station association, single-client TCP :5555 relay and mDNS via the pinned `espressif/mdns` 1.12.0 managed component; UiTask + task WDT implemented; standalone Unity app under `fw-esp32/tests` with the `test-esp32-target` runner target. Bench: associated (RSSI about -60), `la-cli ping/status` and five burst captures succeeded over TCP with zero relay drops and `fifo_ovf=0`; target Unity suite passes 5/5 (relay classification, static queue/notification, NOSPLIT ring, UART2 loopback, task WDT); P5.5 loopback gate passes 10 MB at 921600 (44.4 kB/s echo) and 2 Mbps (39.5 kB/s echo) with zero CRC errors and `fifo_ovf=0` | P8.4 TCP-kill/stale-client fault injection (observed: after a host timeout the gateway discards the next session's frames until reset); P8.2 should profile the relay path (the ~40 kB/s figure is a round-trip echo, not one-way live capacity; at 2 Mbps the 115200 live stream reached zero drops in two runs but dropped 4-19% in three later runs under Wi-Fi stalls, so P8.2/P8.4 must make it repeatable) |
| P6 Host tool and burst E2E | Complete on bench (P6.1-P6.4) | `la-cli` transports (tcp/serial with COM>=10 prefixing), wire client (ping/status/capture with ACK window, gap and CRC checks), streaming RLE reconstruction, `.sr`/VCD/PNG export and port listing; 49 host tests. New `tests/e2e/test_e2e_burst.py` plus `--external-loopback`/`--channel-mask` options: all six patgen modes at 1M/10M/100M with three consecutive captures each, checked bit-exactly (UART periodic frame model, step-mode sequence models, SPI/I2C through the native decoders) with zero CRC/format/FIFO-overflow. P6.2 passes 18/18 over the internal selftest mux (full `test-e2e` 25 passed + 1 skipped); P6.3 passes 18/18 over the JE->JC loopback with six jumpers (channels 0-5 masked, channels 6-7 open); the `.sr` writer now emits `capturefile=logic-1`/`total probes=8`/`probe1..8` so PulseView 0.4.2 loads the file, screenshot in `docs/images/p6-pulseview-uart.png`; `v0.1-burst` tags the close-out | Full 8-channel P6.3 rerun when JE-9/JE-10 jumpers are available (default `--channel-mask 0xFF`); then begin P7 live decode |
| P7 Live decode pipeline | Bench-verified at 2 Mbps; UART/SPI/I2C live decoders; 60 s zero-drop, overload, fault-injection, isolated-byte and decoded-pattern stop gates pass | P7.1 pool/ring, P7.2 live SG DMA + AcqTask publish/repost, P7.3 decoder task, P7.4 dispatch/TxTask batching and the host `la-cli live` STREAM CONFIG are complete. Bench 2026-09-11: after fixing the stream-trigger IRQ storm (RTL one-shot), the release-path bug (decoder released the pool directly, leaving the ring's `published`/`dma_posted` stale) and the eager pool-exhaustion stop, the live path runs end-to-end. `test-e2e --live --live-baud 9600` passes 3/3 including a 10 s zero-drop continuous run; at 115200 the Zynq decodes with `dropped_segments=0` and reports `EVT_OVERRUN(stage=1, dropped=25328)` because the 921600 Zynq->ESP32 link (~40-90 kB/s) cannot carry the ~122 kB/s event stream. Full 8-channel P6.3 external matrix passes 18/18; the 2 Mbps link was verified and the 60 s 115200 gate passed twice, but three later runs (one isolated, one after an ESP32 reset) dropped relay frames (4-19%, Zynq clean), so the gate is not yet repeatable. Host SPI/I2C live decoder configs and the 10 kHz live gates pass | None in P7: P7.6 isolated-byte p99 passes (patgen mode 6 + Zynq `live_max_us`, 428 us worst over 20 shots), the SPI overload/fault-injection gates pass, and P7.7 `stop_on` is complete and bench-verified 2026-09-13 (decoded-pattern stop in `DECODER_CFG`, `EVT_TRIGGERED` match report, host `--stop-on`; full live suite 55 passed) |
| P8 RTOS hardening | Complete | P8.4: link overrun reporting (stage=2), host read-stall, TCP-kill (4/4), garbage-flood resync, 1 MHz SPI overload, WDT reset hook and the 500-capture soak. P8.2: 64 KiB static `link_to_net` with 16 KiB lwIP windows and `touse`/`rep` counters (heap exhaustion fixed). P8.1: TxTask priority 4 -> 5 and bounded event/segment drains. P8.5: cached live pool (`LA_MEM_LIVE_CACHED 1`) with L1/L2 invalidation on every DMA-written read; ~28% more decoder throughput on the 1 MHz SPI overload. P8.3: trace ring + kernel task/queue/stream hooks + app markers, `TRACE_CTRL` (0x18) LOG dump, `la-cli trace` timeline renderer and the annotated live-UART zoom; the context-switch masking bug fixed. P8.6: documented skip (decoder demand vs link bottleneck, AMP design sketch and revisit conditions in `rtos-design.md`) | None in P8; the ESP32 TCP-accept degradation was resolved 2026-09-13 (link-deadlock fix in P9.4) |
| P9 Performance | In progress (P9.2 complete; P9.1 bench-clean for framed bursts at 5/10/18 MHz, drain transport blocked) | P9.1 PL: `axi_quad_spi` v3.2 slave on JD-7..10 with the JA-1 data-ready EMIO GPIO, AXI-Lite at 0x4080_0000, IRQ 2, and `docs/spi-link.md` as the interface contract. The 2026-09-13 rebuild fixed the data-ready drive (the BD pin is scalar `ps7/GPIO_O`; the EMIO width must be set after the enable), timing closed (WNS +0.247 ns, WHS +0.019 ns), DRC 0 errors, LUT 36.74 %, BRAM 61.67 %, 26 bonded IOB. P9.1 Zynq side: `la_spi_link` register driver (DGIER.GIE, SPICR-based slave check, occupancy-1 TFO/RFO decoding), `la_spi_core` whole-frame ring and the `la_spi_task` service (GIC 63 enabled, TX-only IRQ mask, bulk FIFO refill, data-ready hysteresis, ring-space semaphore) with host tests; ESP32 side: `LinkSpiTask` frame-sized reads with a manual CS setup delay, plus the additive `LINK_CFG` (0x1A) burst-transport routing on both sides and the host CLI. Bench: SPI 4 KB captures bit-exact at 10 MHz (crc=0, counter pattern verified) and the UART 64 KB control bit-exact; the SPI 64 KB capture still sees ~7 wire CRC errors per 66 KB (byte counts match, UART bit-exact -> physical layer). Remaining: the 200-ohm SCK/MISO edge characterization and wiring check, 20 MHz, 64 MB drain >=1.5 MB/s. P9.2: versioned compact live events (`EVT_CFG` 0x19 + `EVT_BLOCK` 0x24) with u16 delta timestamps and a u64 escape, host parser/CLI `--compact`, native/host round-trip tests, 51.6-53.3% measured wire-byte saving on the 115200 UART stream; burst raw-vs-transition sizes quantified; full-duplex SPI bandwidth recalculated. Plus the stalled-burst robustness fixes (TxTask ACK timeout, bounded AcqTask wait) | P9.1 remaining: the framed-burst link is bench-clean (5/10/18 MHz, 4 KB-256 KB bit-exact, fault recovery passes) and the 4 MiB capture completes byte-verified; the ESP32-S3 USB CDC host transport is implemented locally (2026-09-17) and the 64 MB drain measurement is pending; the S3 USB-OTG is full-speed (~1.2 MB/s bulk ceiling), below the plan's 1.5 MB/s target. 19/20 MHz lose chunks to slave FIFO underruns (18 MHz ceiling). P9.3 200 MS/s stretch; P9.4 Wi-Fi tuning (isolated-byte latency jitter under weak RSSI, trace-dump LOG drops under relay pressure) |
| P10 UX and release | Complete except the release tag (P10.1 bench-verified; P10.2 deferred; P10.3 docs + portal complete and public; P10.4 tooling verified and the USB-profile SD boot verified 2026-09-26) | `la_web` serves an embedded single-page UI on port 80 and a `/ws` WebSocket streaming the decoded events from the live mirror ring (per-decoder tabs, stats); `la_web_core` is host-tested; `CONFIG_HTTPD_WS_SUPPORT=y` in `sdkconfig.defaults`; bench gate `tests/e2e/test_p10_web.py` passes over the phone hotspot. P10.2: `docs/sigrok-bridge.md` records the deferral (no signed virtual-COM pair driver installed; no security setting changed). P10.3: current `README.md`/`docs/` set plus the publicly published `web-artifact/` three-level portal (7 subsystems, 78 components, register/protocol/decoder/wiring references, verification/roadmap/tooling/glossary, coverage validation, production build, route review) and the `docs/using-the-analyzer.md` + portal `/using` operating guide; `python tools/run.py boot` built BOOT.BIN 2,389,904 bytes from the current FSBL/bitstream/`la_fw.elf`, and the new `python tools/run.py package` staged the ESP32 `.bin` set plus the host wheel with a SHA-256 manifest | `v1.0` tag authorization (P10.4); wiring photos remain [HUMAN]; the portal release was verified publicly on 2026-09-14 and its SD-boot labels need refreshing |

Do not mark a target phase complete because a tool or directory exists. The
phase requires the command or bench evidence named by the plan.

## Verified Gates

These results were observed at the fixed project root using the project
`.venv`; dates and reports are recorded per row. This is retained phase history;
unqualified older counts below are not W11 counts. Latest: native 18/18, host
180/180, SD USB UI E2E 6/6 and SD browser/external UART 10/10. Archived W11
reports are in `build/reports/ui-waveform/` and `build/reports/sd-w11/`;
root-level report filenames may be overwritten by later canonical runs.

| Command or check | Result | Evidence or note |
|---|---|---|
| `python tools/run.py check-env -- --write-lock` | PASS | `build/reports/env-check.json`; no missing P0 prerequisites |
| `python tools/run.py query-board` | PASS | `build/reports/query-board.json` and `board-query.json` |
| `python tools/run.py query-board -- --write-lock` | PASS | Updates `tools/board-lock.json` |
| `.venv\Scripts\python.exe tools\run.py gen` (twice) | PASS | Generator then `--check`; `build/reports/gen.json` |
| `.venv\Scripts\python.exe tools\run.py test-native` | PASS | Native CTest: 7/7 (`la_native_tests`, `la_native_regs` P4.4 driver, `la_native_txwin` P4.9 window tracker, `la_native_acq` P4.7 FSM, `la_native_mem` P4.3 geometry, ctypes ABI and model parity; streaming-CRC32 coverage inside `la_native_tests`); `build/reports/test-native.json` |
| `.venv\Scripts\python.exe tools\run.py test-esp32-mocks` | PASS | CTest mock target: 1/1; `build/reports/test-esp32-mocks.json` |
| `.venv\Scripts\python.exe tools\run.py release` | PASS | Native release precheck: 8/8 CTest tests; `build/reports/release.json` |
| `.venv\Scripts\python.exe tools\run.py test-host` | PASS | Pytest: 1 passed; `build/reports/test-host.json` |
| `.venv\Scripts\python.exe tools\run.py lint` | PASS | Questa compile-only regression, 12/12 testcases, zero warnings; `build/reports/lint.json` |
| `.venv\Scripts\python.exe tools\run.py sim` | PASS | Questa simulation and result-file/PASS checks, 12/12 testcases; `build/reports/sim.json` |
| `python tools/regress.py --self-test` | PASS | Failure, timeout and missing-PASS detection |
| `python tools/run.py fw-esp32` | PASS | Native ESP-IDF v5.5.2 `esp32` build; output under `build/esp32` |
| `.venv\Scripts\python.exe -m compileall -q tools host tests` | PASS | Python syntax check |
| Absolute runner from outside the root | PASS | `test-host` passed using an absolute runner path |
| `python tools/run.py bit` (2026-09-06, re-run after PS7 TTC0/QSPI changes) | PASS | Vivado 2026.1 batch: BD validated, wrapper, synth, impl, bitstream, `write_hw_platform -force -fixed -include_bit`; `build/reports/bit.json`, `build/release/{logic_analyzer.bit,logic_analyzer.xsa,timing_summary.rpt,utilization.rpt,drc.rpt}`; zero failing endpoints (WNS +0.278 ns, "all user specified timing constraints are met"); DRC no errors; utilization 33.51 % LUT / 60.0 % BRAM tiles |
| `python tools/run.py fw-zynq` (2026-09-06, after Vitis Embedded install repair; re-run with the P4.3 linker/geometry) | PASS | Vitis 2026.1 platform `lap` created from `logic_analyzer.xsa` with auto FSBL domain and `fr` freertos domain on `ps7_cortexa9_0` (TTC0 tick), FSBL `fsbl.elf` built, FreeRTOS policy header applied to both generated copies + direct ninja kernel rebuild + export lib sync; `la_app` hello + P4.3 boot contract built and linked with the repo lscript.ld (installed in place over the generated one): `FW-ZYNQ PASS: build/vitis/ws/la_app/build/la_app.elf` (text 63156 / data 3024 / bss 96312); ELF symbols verify the carve: `__la_fw_end` 0x00127E80 < `__la_nc_base` 0x3F000000, window ends at DDR end 0x40000000; machine install also gained `Vitis\data\emulation\platforms\{versal,zynq,zynqmp}` (installer repair) |
| P4.4 la_regs driver host test (2026-09-06) | PASS | `la_native_regs` (CTest): fake-MMIO driver test incl. ID readback, raw read/write, out-of-range read-0/write-ignore parity with la_regs.sv, and all selftest branches (OK, BAD_ID, BAD_SAMPLE_DIV_RESET, BAD_SAMPLE_DIV_ROUNDTRIP, BAD_SAMPLE_DIV_CLEAR); `docs/regmap.yaml` base fixed 0x43C00000 -> 0x40000000 (matches the P3 BD map) and regenerated idempotently (`gen` + `--check` clean); on-target AXI round-trip remains a bench gate |
| `lint` then `sim` after the P3 BRAM/period rework (2026-09-06) | PASS | 12/12 testcases again after the synchronous-read ring/FIFO and enable-latched patgen period changes |
| `test-native` with the P4.9 dispatch/packer cores (2026-09-06) | PASS | CTest 9/9 (`la_native_tests` incl. new typed-payload byte vectors, `la_native_regs`, `la_native_txwin`, `la_native_acq`, `la_native_mem`, new `la_native_cmd` 11 dispatch scenarios, new `la_native_txpack` burst generator checks, ctypes ABI + model parity); `build/reports/test-native.json` |
| `release` with the P4.9 cores (2026-09-06) | PASS | Native release precheck: gen up-to-date, full CTest 10/10; `build/reports/release.json` |
| `test-esp32-mocks` with the P5.2 relay core (2026-09-06) | PASS | CTest 2/2 (`la_esp32_mocks` smoke + new `la_esp32_mocks_relay`: classification, forward/mirror counters, congestion drop/pressure, unknown-type proxy, host-leg all-control, guards); `build/reports/test-esp32-mocks.json` |
| `fw-esp32` with the `la_link` component (2026-09-06) | PASS | Native ESP-IDF v5.5.2: `la_link` component compiles (`libla_link.a`), `la_gateway.elf` + `la_gateway.bin` emitted under `build/esp32`; GCC found a mixed-&&/|| precedence condition MSVC accepted - fixed with explicit parens |
| `test-esp32-mocks` with the P5.3 session core (2026-09-06) | PASS | CTest 3/3 (`la_esp32_mocks`, `la_esp32_mocks_relay`, new `la_esp32_mocks_net`: defaults/clamps, happy session with ordered disconnect actions, backoff deadline math + attempt counters, wifi-loss-with-client, single-client rejection, stop-from-every-state, guards); `build/reports/test-esp32-mocks.json` |
| `fw-esp32` with the `la_net` component (2026-09-06) | PASS | `libla_net.a` + `libla_link.a` compiled under xtensa-esp32-elf; elf/bin emitted under `build/esp32` |
| `test-host` with the P6.1 host tool (2026-09-06) | PASS | Pytest 36/36: protocol vectors + parser resync, client/mock flows (raw and txn transfers, NAK, gap, bad CRC, timeout, EOF), export writers (.sr zip equality, VCD, PNG) and click CLI scenarios; stable across repeated runs; `build/reports/test-host.json` |
| `test-host` with P7.5 live + P10.4 tooling (2026-09-06) | PASS | Pytest 42/42 (added 6 live cases: DECODER_CFG pack layout, live client full collection + overrun, early count stop, CLI text/CSV, bad spec); `build/reports/test-host.json` |
| `boot` (2026-09-06) | PASS | `python tools/run.py boot`: bootgen 2026.1 built BOOT.BIN (2265028 bytes) from fsbl.elf + la_fw.elf + logic_analyzer.bit staged under `build/release`; sync header verified. Tool-chain evidence on the current hello ELF - not a release candidate |
| `lint` then `sim` with LD0 heartbeat (2026-09-10) | PASS | 12/12 compile and procedural simulation cases; `tb_la_top` overrides the blink divider and checks reset/cadence. Reports: `build/reports/{lint,sim}.json`. |
| `release` after P3.3 closure (2026-09-10) | PASS | Register generation was current and the full native CTest suite passed 12/12. Report: `build/reports/release.json`. |
| `bit` after reset-polarity fix (2026-09-10) | PASS | Vivado 2026.1 generated the reset-fixed bitstream/XSA; zero timing failures (WNS +0.255 ns, WHS +0.024 ns), DRC zero errors. Report: `build/reports/bit.json`; signoff files under `build/release/`. |
| `fw-zynq` with native UART1 params and PL probe (2026-09-10) | PASS | Vitis set `freertos_stdout` and `freertos_stdin` to `ps7_uart_1`; generated UART headers were already correct, policy/BSP rebuild passed, and `la_app.elf` was emitted. Report: `build/reports/fw-zynq.json`. |
| `jtag-zynq` USB-only load (2026-09-10) | PASS | XSDB used JTAG SRST, programmed the PL, verified latched boot mode 0 and PS/PL level shifters 0xF, ran the FSBL to `FsblHandoffJtagExit`, downloaded `la_app.elf`, and started CPU0. Report: `build/reports/jtag-zynq.json`. |
| P3.3/P4 basic bench evidence (2026-09-10) | PASS / partial | User observed LD0 blinking. COM6 at 115200 showed recurring FreeRTOS ticks, `FPGA0_CLK=0x00400400`, `FPGA_RST=0`, CPU MMIO `PL ID=0x4C410002 STATUS=0x00000001`, and stack high-water `133/200`; ticks continued afterward. XSDB independently read the same PL ID/IDLE status and live DMA registers. P3.3 is complete; P4.2's 10-minute soak and P4.4's full write/read/restore self-test remain open. |
| `test-native` with the P4.5-P4.9 cores (2026-09-10) | PASS | CTest `^la_native` 11/11: the existing cores plus new `la_native_plan` (word accounting, exact split, short tail, guard BD, capacity/BD/offset limits, timeout rounding) and `la_native_irqstats` (empty/min/max/avg, sub-microsecond rounding, guards), extended `la_native_cmd` (PATGEN_CFG accept/NAK, ARM pool-overflow rejection) and `la_native_tests` (PATGEN_CFG little-endian vector + validation); `build/reports/test-native.json` |
| `test-host` with the PATGEN_CFG extension (2026-09-10) | PASS | Pytest 44/44: protocol PATGEN_CFG pack/unpack vectors and validation errors, `AnalyzerClient.configure_patgen` ACK flow against the mock, and a hardware-tolerant `ports` test; `build/reports/test-host.json` |
| `release` after the P4 wiring (2026-09-10) | PASS | Register generation current; full native CTest 14/14 (`build/reports/release.json`) |
| `lint` then `sim` after the P4 wiring (2026-09-10) | PASS | 12/12 compile and procedural cases each; no RTL changed, rerun for a complete evidence set (`build/reports/{lint,sim}.json`) |
| `test-esp32-mocks` (2026-09-10) | PASS | CTest 3/3 (`build/reports/test-esp32-mocks.json`) |
| `fw-zynq` with the full P4 firmware (2026-09-10) | PASS | Vitis 2026.1 built `la_app.elf` from 32 imported sources (text 139084 / data 3656 / bss 123984); report `build/reports/fw-zynq.json` |
| `jtag-zynq` with the P4 ELF (2026-09-10) | PASS | PL programmed, FSBL to handoff, `la_app.elf` downloaded, CPU0 started; report `build/reports/jtag-zynq.json` |
| P4.4 on-target register selftest (2026-09-10) | PASS | Live COM6 capture during the JTAG load: `LA P4 FreeRTOS core on UART1`, `LA memory: fw end 0x00142B80 nc base 0x3F000000`, `LA P4.4 register selftest: OK (ID 0x4C410002)` - ID and SAMPLE_DIV write/read-back through the real AXI fabric |
| P4.2/P4.5 task soak on bench (2026-09-10) | PASS (link IRQs disabled) | With UART0 interrupts disabled over JTAG (the JD header has no peer, so RX floats and storms), StatusTask printed 123 iterations every 5 s for 10 minutes with no fatal/assert/overflow; stable watermarks `acq=460 cmd=452 tx=211 status=177` words free, all counters zero. The link IRQ path itself is unverified on wire |
| P4.10 HIL preflight (2026-09-10) | BLOCKED | `tests/hil/test_zynq_serial.py` collects and skips without `--port`; `la-cli ports` lists only COM6 (onboard FTDI UART1 console). The analyzer link needs a 3.3 V USB-UART adapter on JD-1..4 + GND; nothing was reported as passing |
| `fw-esp32` with the P5.1-P5.4 adapters (2026-09-10) | PASS | Native ESP-IDF v5.5.2 `esp32` build: `la_board`, `la_proto` (shared `common/la_proto` source), `la_link` (UART2 RTS/CTS driver + LinkRx/LinkTx tasks) and `la_net` (Wi-Fi/NetServer/mDNS) compile under xtensa-esp32-elf; `la_gateway.bin` 0xc9dc0 bytes (21% partition free); `espressif/mdns` 1.12.0 provisioned from the component registry into the ignored `fw-esp32/managed_components/` |
| `test-esp32-mocks` after the P5 adapters (2026-09-10) | PASS | CTest 3/3; `build/reports/test-esp32-mocks.json` |
| `test-native` after the P5 adapters (2026-09-10) | PASS | CTest 11/11; `build/reports/test-native.json` |
| ESP32 flash attempt (2026-09-10) | BLOCKED | COM5 was held by the Arduino IDE `serial-monitor`; after it closed, esptool detected ROM boot mode `0x13` (auto-reset did not assert GPIO0). Manual download mode was deferred by the user; nothing was flashed and no target behavior is claimed |
| ESP32 flash + boot capture (2026-09-10) | PASS | `idf.py -p COM5 flash` succeeded on retry (the DOIT board's auto-reset is flaky: one attempt reported boot mode `0x13`). COM5 capture: `UART2 921600 8N1 RTS/CTS tx=17 rx=16 rts=22 cts=21`, link tasks with 32/16/8 KiB rings, Wi-Fi station start and backoff (no credentials), UiTask every 1 s with `fifo_ovf=0`. The first capture also showed RX noise (`line_err` climbing while the Zybo was unpowered, `rx_bytes` counting noise); an internal pull-up on GPIO16 (IDF sets one on CTS but not RX) fixed it and the reflash showed `line_err=0 rx_bytes=0` |
| P4.5 interrupt dispatch root cause + fix (2026-09-10) | PASS | XSDB showed IRQ 59 permanently pending with `UART0 ISR=0x16F`, CPU spinning in `FreeRTOS_IRQ_Handler`, and the vector table entry for GIC 59 holding `StubHandler` (ref `XScuGicInstance`) instead of `XUartPs_InterruptHandler`. Cause: the SDT BSP's `XConnectToInterruptCntrl()` no-ops until `XSetupInterruptSystem()` sets `ScuGicInitialized`, and the FreeRTOS CA9 port's init path never does, so `xPortInstallInterruptHandler()` reported success while connecting nothing. Fix: `la_irq_connect()` routes the first connection through `XSetupInterruptSystem()`; after it, COM6 printed recurring `LA status:` ticks with no storm and no console starvation |
| Zynq task stacks after capture overflow (2026-09-10) | PASS | A capture triggered `vApplicationStackOverflowHook` because the CA9 port runs ISRs on the interrupted task's stack; `la_app.h` stacks raised to 4/4/4/2 KiB and the FreeRTOSConfig fragment sets idle/timer to 2 KiB. Rebuilt, reloaded and re-captured with healthy watermarks (`acq=972 cmd=892 tx=690 status=433` words free) and no overflow |
| P5.3/P4.9 end-to-end over the gateway (2026-09-10) | PASS | ESP32 associated (RSSI about -60) and listening on :5555; `la-cli ping` returned `pong: fw 0x00040000 pl 0x4C410002`; `la-cli status` returned `state idle status_reg 0x1`; five burst captures (`1M/4096 raw`, `10M/16k raw` x3, `10M/16k txn`) completed with host-side DATA_END CRC verification and exports under `build/captures/`. ESP32 UiTask during a capture: `link fwd=12 drop=0 press=0 fifo_ovf=0 line_err=0`, `net clients=1/0 rx_frames=7 tx_frames=10`. Open: the 10M raw captures returned 2053 words instead of 4096 and the txn export reconstructed a bogus 16777215-sample run (P4 acquisition/data issue, not transport) |
| P4 acquisition root cause (2026-09-10) | PASS (diagnosis) | XSDB on the loaded design: PL reported the full word count while `BD_STS=0`, the capture buffer still held the `0xDEADBEEF` probe pattern, and `S2MM_SR=0x00010008` (running, no errors) - the S2MM never completed a descriptor. The BD had `C_SG_INCLUDE_STSCNTRL_STRM=1` (Vivado default) with `S_AXIS_S2MM_STS` tied off in the BD. A controlled JTAG test (marker + one 1024-byte BD) then showed the driver start order `CURDESC -> RUNSTOP -> TAILDESC` transfers correctly once the status stream is disabled, while a TDESC-before-RUNSTOP order does not |
| `bit` after the DMA status-stream and burst-close fixes (2026-09-10) | PASS | `C_SG_INCLUDE_STSCNTRL_STRM {0}` added to the DMA config; Vivado 2026.1 rebuilt BD, synth, impl, bitstream and XSA with zero failing endpoints (WNS +0.377 ns, WHS +0.024 ns, all user constraints met); `build/reports/bit.json`; signoff under `build/release/` |
| `test-native` / `test-host` / `test-esp32-mocks` / `release` after the P4 fixes (2026-09-10) | PASS | CTest 11/11; pytest 44/44; ESP32 mocks 3/3; release precheck 14/14 (`build/reports/{test-native,test-host,test-esp32-mocks,release}.json`) |
| `lint` then `sim` after the burst-close RTL rework (2026-09-10) | PASS | 12/12 compile and procedural cases each; `tb_la_raw_packer` gained burst-aligned, burst-partial and live-to-burst scenarios; `tb_la_top` scenario B now asserts exactly 75 words, one tlast on the last word and exactly 300 samples; `build/reports/{lint,sim}.json` |
| `fw-zynq` with the P4 firmware fixes (2026-09-10) | PASS | Vitis 2026.1 rebuilt `la_app.elf` (text 139340 / data 3656 / bss 134544) with the PATGEN_CFG ACK and ARM-preserves-sticky-CTRL fixes; `build/reports/fw-zynq.json` |
| Exact P4 acquisition E2E through the gateway (2026-09-10) | PASS | PATGEN counter (DIV=99) over the internal selftest mux, host CRC/format checks clean: `raw 4096 @10M` -> 1024 words = 4096 samples; `raw 16000 @10M` -> 4000 words = 16000 samples with the counter stepping every 10 samples; `raw 16000 @1M` -> 4000 words = 16000 samples with the counter stepping every sample; `txn 16000 @10M` -> 1601 words reconstructing exactly 16000 samples in runs of 10. Zero parser CRC/format errors; evidence `build/captures/p4fix-{raw-4k-10m,raw-16k-10m,raw-16k-1m,txn-16k-10m}.sr` |
| Multi-BD capture through the gateway (2026-09-10) | PASS | `raw 1000000 samples @1M` (250000 words = 1 MB, 16 ordinary SG BDs): host CRC/format clean, elapsed 25.6 s, and every one of the 1M samples is the strictly consecutive counter value (`(d[i+1]-d[i]) mod 256 == 1`). This exercises the finite multi-BD chain and short final BD |
| `lint` then `sim` after the ring `stream_en` fix (2026-09-10) | PASS | 12/12 compile and procedural cases each; `tb_la_pretrig_ring` gained the `stream_en` port, the PRE=0 burst phase (`burst-zero-pre`: no emission before the trigger, trigger sample + post after) and the model mirrors `stream_en`; `build/reports/{lint,sim}.json` |
| `bit` after the ring `stream_en` fix (2026-09-10) | PASS | Vivado 2026.1 rebuilt the bitstream/XSA with zero failing endpoints; `build/reports/bit.json` |
| `fw-zynq`/`jtag-zynq` with the abort soft-reset fix (2026-09-10) | PASS | `la_app.elf` rebuilt and loaded; `build/reports/{fw-zynq,jtag-zynq}.json` |
| `test-e2e` P4.6 burst suite through the gateway (2026-09-10) | PASS | New `tests/e2e/test_zynq_burst.py` + `test-e2e` runner target (LA_E2E_TARGET/--target): 7/7 - ping/idle, 3 consecutive 16k raw runs, 4-BD 200 KB raw chain, raw 4096 + txn 16000 short tails, abort while armed, abort mid-capture, capacity-overflow NAK; every counter stream exact, zero CRC/format errors, no overflow |
| `test-esp32-target` Unity suite on the target (2026-09-10) | PASS | New `fw-esp32/tests` Unity app + runner target; flashed with reset disabled after manual download mode; 5/5: `la_relay` classification, static queue + task notification, static NOSPLIT ring buffer, UART2 internal loopback of a maximum frame, task-WDT subscribe/reset/delete; capture `build/reports/test-esp32-target-capture.log` |
| P5.5 ESP32 loopback gate (2026-09-10) | PASS | TX<->RX and RTS<->CTS jumpers, JD ribbon removed; `test_esp32_loopback.py` streams 10000 frames (10.07 MB) with per-frame verification and zero parser CRC/format errors. At 921600: 44.4 kB/s echo in 226.7 s, `fifo_ovf=0`; at 2 Mbps (`CONFIG_LA_LINK_BAUD=2000000`, verified in the boot log): 39.5 kB/s echo in 254.8 s, `fifo_ovf=0`. The similar rates show the gateway relay processing, not the baud, is the current ceiling (P8 note) |
| `test-e2e` P6.2 burst matrix (2026-09-11) | PASS | `tests/e2e/test_e2e_burst.py` internal selftest: 18/18 (counter, walking-1, PRBS8, UART, SPI, I2C at 1M/10M/100M, three consecutive captures each), every capture parser/CRC/format clean with FIFO overflow clear; the full `test-e2e` run is 25 passed + 1 skipped (the P5.5 ESP32 loopback gate needs its own jumpers). Evidence: `build/reports/test-e2e.json` |
| `test-e2e` P6.3 external JE->JC loopback (2026-09-11) | PASS | Same matrix with `--external-loopback --channel-mask 0x3F`: 18/18 through six jumpers (channels 0-5; JE-9/JE-10 -> JC-9/JC-10 not yet wired). Real pads + input synchronizers, `SELFTEST_EN=0`. Evidence: `build/reports/test-e2e.json` |
| `test-host` after the `.sr` metadata fix (2026-09-11) | PASS | Pytest 49/49; the raw export test now asserts `capturefile=logic-1`, `total probes=8` and `probe1..8`; `build/reports/test-host.json` |
| P6.4 PulseView export (2026-09-11) | PASS | `build/captures/p6-uart-1m.sr` (4 ms UART frame, trimmed to a start bit) opens in PulseView 0.4.2 and renders on CH0; screenshot `docs/images/p6-pulseview-uart.png`; annotated tag `v0.1-burst` |
| 2 Mbps link builds (2026-09-11) | PASS | Zynq `LA_LINK_BAUD 2000000` (`la_app.elf` text 165548 / data 3656 / bss 162544) and ESP32 `CONFIG_LA_LINK_BAUD 2000000` (`la_gateway.bin` 0xcabe0, built `sdkconfig.h` verified) |
| 2 Mbps gateway flash + Zynq reload (2026-09-11) | PASS | Manual BOOT/EN download mode then esptool `--before no_reset --after hard_reset` wrote bootloader/app/partition to COM5 with hash verification (ESP32-D0WD-V3); `jtag-zynq` reloaded the 2 Mbps ELF; `la-cli ping` over the new link returned `pong: fw 0x00070000 pl 0x4C410002` |
| `test-e2e --live --live-seconds 60` at 2 Mbps (2026-09-11) | PASS | Two consecutive full runs: 28 passed + 1 skipped (the P5.5 loopback needs its own jumpers), including the P6.2/P6.3 matrices 18/18, Zynq burst 7/7 and live 3/3. The 60 s zero-drop gate decoded the exact repeating `LA-TEST\n` stream with `dropped_segments=0`, no EVT_OVERRUN and zero new relay drops (`fwd` rose by 30235 then 60483 while `drop` stayed at 246 from the earlier buggy run, `fifo_ovf=0`). Evidence: `build/reports/test-e2e.json` |
| Live harness patgen latch fix (2026-09-11) | PASS | `_configure_patgen_uart` now writes 0 before the enable value: la_patgen latches mode/DIV only on the EN rising edge, so the old helper silently kept the previous test's mode (the stale-mode probe reproduced exactly one FRAME_ERR at t=8246; the fixed helper decoded 64/64 BYTE events) |
| Host SPI/I2C live decoder configs (2026-09-11) | PASS | `pack_decoder_cfg_spi`/`pack_decoder_cfg_i2c` (20-byte DECODER_CFG layouts per protocol.md) plus the `la-cli live --decoder spi:.../i2c:...` spec parser and mock `last_decoder` capture; `test-host` 55/55 (`build/reports/test-host.json`) |
| P7.6 SPI/I2C live gates at 10 kHz (2026-09-11) | PASS | patgen SPI DIV=99 (SCK=CH0, MOSI=CH1, CS_N=CH2) and I2C DIV=39 (SCL=CH3, SDA=CH4) over the JE->JC loopback; `test-e2e --live -k test_live` 5/5 in 18.6 s, including the new gates: SPI MOSI 0x4C/0x41 with CS assert/release, I2C START/0x50/ACK/0x4C/ACK/0x41/ACK/STOP, no NACK/frame error, `dropped_segments=0`, and >=20% headroom against the 2 Mbps payload ceiling (MISO is not driven by the patgen; the P9 SPI link provides it) |
| P7.6 60 s UART zero-drop gate repeat (2026-09-11) | FAIL (flaky) | Three consecutive runs after the earlier two passes dropped ESP32 relay frames: full suite at 93697 B (`drop` +16926) and 16464 B (`drop` +4105), and an isolated 60 s run at 168044 B (`drop` +5763, ~19% of frames). The Zynq decoded 604162 events with `dropped_segments=0`, `fifo_ovf=0` and `line_err=0`; the relay's best-effort path drops silently under Wi-Fi/TCP stalls (RSSI fell from -61 to -67). P8.2/P8.4 work item |
| P8.4 link-overrun report (2026-09-11) | PASS | `la_relay` accumulates `pending_drops` on every best-effort drop; LinkRxTask emits one `EVT_OVERRUN(stage=2 link)` and clears the count only after the ring accepts it. `test-esp32-mocks` 3/3, `test-host` 57/57, `fw-esp32` PASS (`la_gateway.bin` 0xcace0); flashed to COM5 (hash verified) and `la-cli ping` OK |
| P8.4 host read-stall fault injection (2026-09-11) | PASS | `test-e2e --live -k test_live_link_overrun_reported_on_stall`: a 3 s host read stall fills the relay ring; after resuming, the gateway reports `EVT_OVERRUN(stage=2)` with `dropped >= 1`. Full live suite 6/6 in 23.1 s |
| P7.6 60 s gate after the link report (2026-09-11) | FAIL (reported, not silent) | Isolated 60 s run: `EVT_OVERRUN(stage=2, dropped=33)` stops the host with an explicit link-overrun error instead of a corrupted stream. Preventing the drops remains P8.2 |
| P8.2 relay buffering tuning (2026-09-11) | PASS | `link_to_net` 32 -> 64 KiB (static), lwIP windows kept at 16 KiB, `touse`/`rep` counters added. The 32 KiB-window variant was rejected: during live streaming the heap fell to 3,552 B, TCP stalled, 29,364 frames dropped with no reports enqueued and the gateway entered the stale-client state; reverting to 16 KiB windows with the 64 KiB static ring restored a 65,508 B heap floor. Isolated 60 s gate 3/3 with `drop=0` and peak `to_net_max_used=40144` B; `fw-esp32` PASS (`la_gateway.bin` 0xcad30), flashed with hash verification |
| Full `test-e2e` after the heap fix (2026-09-11) | PASS | 31 passed + 1 skipped in 119 s: both burst matrices, the 10 kHz SPI/I2C gates, the 60 s zero-drop gate and the stall fault injection. Console watcher: minimum heap 65,508 B, `fifo_ovf=0`, `line_err=0`; the only drops (721 frames, 11 stage=2 reports) came from the deliberate 3 s stall test |
| P8.4 TCP-kill/stale-client fault injection (2026-09-11) | PASS | `AnalyzerClient.kill()` RST-aborts the transport (`SO_LINGER 0`); `test_live_tcp_kill_reconnect` starts a live flood, RSTs the client mid-stream, and requires a fresh connection to answer a ping: 4/4 passes. The earlier stale-client states were heap-exhaustion symptoms, not an independent bug |
| Host live overrun-collection fix (2026-09-11) | PASS | An `EVT_OVERRUN` sharing a read with the count-stop event was discarded, making `test_live_command_text` flaky; the client now always keeps overruns and trims only the decoded overshoot. `test-host` 58/58 and `test_live.py` 8 consecutive runs green |
| P8.1 consumer-starvation fix (2026-09-11) | PASS | Under 1 MHz SPI overload the host received zero events while decoder counters climbed: TxTask (prio 4) never ran because AcqTask kept the decoders' `seg_q` non-empty. TxTask raised to prio 5 (equal round-robin) and the `ev_q` drain bounded by TX StreamBuffer space; the host now receives events plus stage-1 loss reports and the command path stays responsive |
| P8.4 SPI overload fault injection (2026-09-11) | PASS | `test_live_spi_overload_reports_loss`: patgen SPI at 1 MHz (~230 k events/s) yields stage-1 `EVT_OVERRUN` reports with nonzero counters, returns to IDLE and recovers to a clean 10 kHz decode |
| P8.4 garbage-flood resync (2026-09-11) | PASS | `test_live_garbage_flood_resync`: 2 KiB of garbage plus a filler and a bad-CRC frame, then PING/STATUS are answered; `test-host` 59/59 with the `raw_send` hook |
| Full `test-e2e` with all fault injections (2026-09-11) | PASS | 34 passed + 1 skipped in 124 s: 9 live tests (UART 60 s zero-drop, SPI/I2C 10 kHz, SPI overload, garbage, TCP kill, stall, raw), both burst matrices |
| P8.4 WDT fault injection (2026-09-11) | PASS | The `CONFIG_LA_LINK_WDT_TEST_HOOK` build hangs LinkRxTask at start; the console showed `task_wdt: Task watchdog got triggered - LinkRx (CPU 1)` then `Aborting.` and `Rebooting... rst:0xc (SW_CPU_RESET)`. `CONFIG_ESP_TASK_WDT_PANIC=y` is now the tracked default; the normal firmware was restored and the live suite passes 9/9 |
| P8.4 500-capture soak (2026-09-11) | PASS | `test-e2e --soak-count 500 -k test_soak_back_to_back`: 500 back-to-back 4096-sample counter captures in 232 s, every capture exact and CRC/format clean, the sticky FIFO-overflow bit clear and the firmware IDLE at the end |
| P7.6 isolated-byte 25 ms p99 (2026-09-11) | PASS | Patgen mode 6 (one `LA-TEST\n` frame per enable, then idle high; new bitstream, `gen`/`lint`/`sim`/`test-native`/`test-host` clean) plus the Zynq-side `live_max_us` counter: 428 us worst case over 20 single-shot sessions against the 25 ms budget. The host-side `test_live_uart_isolated_byte_latency` checks the frame decodes and the final byte arrives with the in-stream baseline. Evidence: `la status: ... live_max_us=428` on COM7 |
| ESP32 response-latency fix (2026-09-11) | PASS | The `link_to_net` ring is not a select() descriptor, so a 100 ms select timeout delayed sparse replies by up to 100 ms; `LA_NET_SELECT_TIMEOUT_US` 100000 -> 5000 cut the measured PATGEN_CFG round trip from 93-125 ms to ~15 ms (`ping` 16 ms) |
| Full `test-e2e` after the patgen/latency work (2026-09-11) | PASS | 35 passed + 2 skipped (loopback, unrequested soak) in 103 s with the new bitstream and Zynq ELF: both burst matrices and all 10 live gates |
| P8.5 cached live pool local gates (2026-09-12) | PASS | `gen --check` up to date; `test-native` 12/12 (`la_native_mem` gained the `la_mem_apply_range` prefix/alignment/range cases); `test-host` 61/61 (two new live-client cases for `stop_on_overrun=False` and `collect_events=False`); `release` 15/15; `test-esp32-mocks` 3/3; `compileall` and `git diff --check` clean. Reports: `build/reports/{gen,test-native,test-host,release,test-esp32-mocks}.json` |
| P8.5 cached live pool firmware (2026-09-12) | PASS | `fw-zynq` built `la_app.elf` for both variants (`LA_MEM_LIVE_CACHED=0/1`); boot banners captured during `jtag-zynq` reloads: `LA live pool: non-cached` and `LA live pool: cached (NORM_WB) + invalidate` (`build/reports/p85-{noncached,cached,final}-console.log`); `ping` returned `fw 0x00080000` |
| P8.5 A/B decoder throughput (2026-09-12) | PASS | Same-source A/B on the 1 MHz SPI overload, 20 s windows, host drains without retaining events and does not stop on overruns. Non-cached: 973,085 / 1,006,094 / 1,007,818 words/s, 4,620-4,787 dropped segments. Cached: 1,288,915 / 1,282,279 / 1,284,519 words/s, 3,326-3,337 dropped segments (~28% more words/s, ~30% fewer drops; decoder 96-99% busy). Cached is the tracked default. Evidence: `P8.5 decoder throughput: ...` output and the console banners; documented in `docs/rtos-design.md` |
| Full `test-e2e` with the cached live pool (2026-09-12) | PASS | 36 passed + 2 skipped in 143.6 s (loopback, unrequested soak) with the cached build: both burst matrices (TxTask invalidate path), all 10 live gates including the 60 s zero-drop UART gate (AcqTask invalidate path, bit-exact), the P8.5 throughput test and the Zynq burst suite. Report: `build/reports/test-e2e.json` |
| P8.3 tracing local gates (2026-09-12) | PASS | `test-native` 13/13 (new `la_native_trace` ring/registration/wrap cases and the `la_native_cmd` TRACE_CTRL accept/NAK cases plus the `la_native_tests` byte vector); `test-host` 69/69 (trace payload, client control/dump against the mock, parser/renderer incl. the zoom window and the CLI `trace --dump --out --png`); `fw-zynq` built the trace-hooked kernel (`vTaskSwitchContext` and `xQueueGenericSend` disassembly shows the `la_trace_*` calls); compileall and `git diff --check` clean. Reports: `build/reports/{test-native,test-host,fw-zynq}.json` |
| P8.3 masking-bug fix and trace stability (2026-09-12) | PASS | Root cause: `vPortClearInterruptMask()` executes `cpsie i`, so the first trace lock re-enabled interrupts inside the SWI/IRQ switch path and a nested tick saved the handler context into the selected TCB (corrupted `pxTopOfStack`, data abort at `0x13B3C4`); the fix saves/restores CPSR.I (`trace_irq_lock`). After the fix: 18 consecutive `test_p83_live_uart_trace` runs pass (12 before and 6 after the lazy idle registration), three 3 s trace sessions during the 1 MHz SPI overload keep balanced IN/OUT and the full DMA/segment/decode/TX token set, and `ping` still answers after each. Annotated trace: `docs/images/p83-live-uart-trace.png` (5 ms zoom, `build/traces/` regenerated by the test) |
| Full `test-e2e` with P8.3 tracing (2026-09-12) | PASS | 37 passed + 2 skipped in 125.3 s with the fixed trace build: the P8.3 trace gate plus the P8.5 throughput gate, both burst matrices and all live gates including the 60 s zero-drop UART run. Report: `build/reports/test-e2e.json` |
| P9.2 compact events local gates (2026-09-12) | PASS | `test-native` 13/13 (`la_native_live` gained the EVT_BLOCK header/delta/escape/count/capacity cases; `la_native_cmd` the EVT_CFG accept/NAK cases; `la_native_tests` the EVT_CFG byte vector); `test-host` 73/73 (EVT_CFG pack/unpack, EVT_BLOCK round trip incl. escape/truncation/trailing, client `event_format` with the mock `live_compact` scenario, CLI `--compact`); `fw-zynq` PASS; compileall and `git diff --check` clean |
| P9.2 burst raw-vs-transition sizes (2026-09-12) | PASS | 18/18 rows in `build/reports/p92-sizes.json`: step/UART from the P6.2 bit-exact models, SPI/I2C from 4096-sample bench captures. Ratios 4.00 (counter@1M) down to 0.003 (UART@100M); no fixed compression ratio. |
| P9.2 compact live wire bytes (2026-09-12) | PASS | Two isolated runs of `test_compact_events_reduce_live_wire_bytes` on a healthy gateway: v1 242,330 B/19,346 events = 12.53 B/event vs compact 118,357 B/19,527 events = 6.06 B/event (51.6% saving); a second run measured 53.3%; both runs decoded identical bytes. |
| P9.2 stalled-burst robustness (2026-09-12) | PASS (diagnosis and fix) | A vanished host used to wedge the firmware: TxTask waited forever for ACKs and AcqTask blocked in `acq_wait_buffer_free` (XSDB showed both tasks in `xTaskGenericNotifyWait`, PL DONE with the full word count). TxTask now abandons after 2 s without progress and AcqTask bounds the wait at 5 s; the bench no longer wedges. |
| Full `test-e2e` after P9.2 (2026-09-12) | BLOCKED | 30 passed + 2 skipped + 26 failed in 82.8 s: the burst matrices passed, then the ESP32 gateway reset/aborted every subsequent TCP connection (`WinError 10054/10053`) during the live tests. The ESP32 console showed `net CONNECTED clients=5/3 disc=5`, heap 86,684 B, tasks alive, no panic; an RTS reset restored service. The same degradation reproduced after ~21 connections in the size-test session. Not a code PASS; see the blocker below. |
| `bit` with the P9.1 SPI slave (2026-09-13) | PASS | Vivado 2026.1 rebuilt the BD with `axi_quad_spi` v3.2 (slave mode, 256-deep FIFO, AXI-Lite at 0x4080_0000, IRQ into `xlconcat` In2, JA-1 PS EMIO data-ready), bitstream and XSA; zero failing endpoints (WNS +0.182 ns, WHS +0.018 ns), DRC 0 errors; LUT 36.74 %, BRAM 61.67 %, 26 bonded IOB. Report `build/reports/bit.json`; signoff under `build/release/`. The SPI pins are not yet wired on the bench |
| `test-native` with the P9.1 SPI driver (2026-09-13) | PASS | CTest 14/14 including the new `la_native_spi` (AXI Quad SPI slave register model: reset/start, TX full, RX FIFO order/empty, IRQ write-1-to-clear/enable, slave-mode and error checks, stop); `build/reports/test-native.json` |
| `fw-zynq` from the P9.1 XSA (2026-09-13) | PASS | Vitis 2026.1 rebuilt the platform with the AXI Quad SPI BSP (`libspi.a`) and built `la_app.elf` (text 176332 / data 3656 / bss 228784); geometry symbols `__la_fw_end` 0x001644E0 < `__la_nc_base` 0x3F000000. Report `build/reports/fw-zynq.json`. The ELF is not loaded on the bench |
| `test-native` with the P9.1 SPI core (2026-09-13) | PASS | CTest 15/15 including the new `la_native_spi_core` (whole-frame ring: order, wrap compaction, feed wrap, capacity rejection, underrun counter, guards); `build/reports/test-native.json` |
| `fw-zynq` with the P9.1 SPI service task (2026-09-13) | PASS | Vitis 2026.1 built `la_app.elf` (text 183304 / data 3688 / bss 241312; `__la_fw_end` 0x0016B5F0 < `__la_nc_base` 0x3F000000) with `la_spi_task.c`; the SPI service task, frame ring and JA-1 data-ready GPIO are compiled but not bench-verified. Report `build/reports/fw-zynq.json` |
| `fw-esp32` with the P9.1 SPI master (2026-09-13) | PASS | ESP-IDF v5.5.2 xtensa build: `la_gateway.bin` 0xd1d40 (18% partition free) with `LinkSpiTask`, the SPI relay refactor and the Kconfig `LA_SPI_CLOCK_HZ`; the SPI pins are not wired on the bench |
| `test-host` with the LINK_CFG routing (2026-09-13) | PASS | Pytest 77/77 (LINK_CFG pack/unpack vectors, client ACK against the mock, stale-NAK handling and the prior suite); `build/reports/test-host.json` |
| `fw-zynq` with the LINK_CFG routing (2026-09-13) | PASS | `la_app.elf` text 184200 / data 3688 / bss 241312; report `build/reports/fw-zynq.json` |
| P7.7 protocol-aware stop local gates (2026-09-13) | PASS | `test-native` 15/15 (`la_native_tests` DECODER_CFG stop-on layout/round-trip/limit vectors, `la_native_live` rolling match across one-word segments + single-byte + reconfigure + no-false-hit, `la_native_cmd` stop-on latch and oversized-pattern NAK); `test-host` 83/83 (per-type pack layout, client stops only with `stop_on_match`, CLI `--stop-on`, printable-byte rendering); `release` 18/18; `test-esp32-mocks` 3/3; `fw-zynq` PASS (text 186712 / data 3688 / bss 241840); compileall and `git diff --check` clean. Reports under `build/reports/` |
| P7.7 stop-on bench gate (2026-09-13) | PASS | `jtag-zynq` loaded the P7.7 ELF (fw 0x00090000) over the P9.1 bitstream; `test-e2e --live -k test_live_stop_on_decoded_pattern` passed: the UART patgen stream ends on the decoded "LA" match, `EVT_TRIGGERED` carries the matching sample index, the drained segments' events still arrive and STATUS returns to IDLE. CLI check `la-cli live --stop-on 4c41` printed `stop-on match at sample 69386`. Full `test-e2e --live`: 55 passed + 3 skipped + 1 known flake (P8.3 trace dump LOG loss 116/4096 = 2.83% > the 2% best-effort tolerance; passes in isolation; the P9.4 relay-pressure item, not a P7.7 regression). Report `build/reports/test-e2e.json` |
| P9.1 link fixes + bench gates (2026-09-13) | PASS (drain rate BLOCKED) | Five bench-found defects fixed (continuous-read ESP32 master, session flush, ISR FIFO refill, SPSC ring critical sections, relay retry, 2 s producer wait) plus the runtime `LINK_CFG` clock. `test_p91_spi_capture_sizes` PASS (4/16/64 KB at 10 MHz, bit-exact, zero parser errors; 64/128/256 KB also verified manually), `test_p91_spi_clock_sweep` PASS (5/10/18 MHz; 20/20 captures clean at 18 MHz), `test_p91_spi_fault_recovery` PASS (40 MHz fails honestly, next 10 MHz capture clean). A 4 MiB capture at 10 MHz completes with every byte verified. `test_p91_spi_drain_rate` FAILS the gate honestly: 0.28 MB/s host-verified vs the 1.5 MB/s target; the ESP32 Wi-Fi relay is the limiter (the SPI wire delivers 1.25 MB/s at 10 MHz, 2.25 MB/s at 18 MHz). 19/20 MHz lose chunks to slave FIFO underruns. Reports: `build/reports/test-e2e.json` (P9.1 rows recorded there when run with `--target`) |
| P10.1 live web view bench gate (2026-09-14) | PASS | `test_p10_web_page_served` (HTTP GET `/` returns the embedded page, text/html, with the `/ws` reference) and `test_p10_ws_streams_events` (the `/ws` WebSocket carried the decoded `LA-TEST\n` bytes as `{"s":0,"e":[[t,1,"hex"],...]}` plus the 1 s stats message during a live UART session) both PASS on the phone hotspot (SSID `Lingg`, gateway 172.20.10.2). Local: `test-esp32-mocks` 4/4 (new `la_esp32_mocks_web`), `fw-esp32` PASS. The SPI bulk link is not wired in this reassembly, so the P9.1 tests were not part of this run |

| `python tools/run.py boot` (2026-09-14) | PASS | Rebuilt BOOT.BIN (2,389,904 bytes) from the current P9.1 bitstream, FSBL and the current `la_app.elf` (1,039,624 bytes) via native bootgen 2026.1; sync header and size verified. Tooling evidence only: the [HUMAN] SD boot is not performed |
| `python tools/run.py package` (2026-09-14) | PASS | New target (`tools/make_release.py`): staged `bootloader.bin` 26,304 B, `partition-table.bin` 3,072 B and `la_gateway.bin` 910,032 B under `build/release/esp32`; built `la_host-0.0.0-py3-none-any.whl` 57,811 B and verified it contains `host/la_cli`, `host/la_sigrok_bridge`, `tools/run.py` and the `la-cli` entry point; wrote `build/release/release-manifest.json` with sizes and SHA-256 hashes |
| Portal content validation (2026-09-14) | PASS | `web-artifact` package script `validate` (esbuild bundle + node): 7 subsystems, 78 components, 21 registers, 24 protocol messages, 11 roadmap phases, 55 glossary terms, 182 coverage entries; no duplicate slug, orphan component, broken related link, invalid status key or uncovered source. `oxlint` is clean on the new files (vendored `components/ui/*` findings are pre-existing) |
| Portal production build (2026-09-14) | PASS | `vinext build` (Vinext 1.0.0-beta.5 / Vite 8.0.13) completed all five build stages and emitted 11 routes |
| Portal route render review (2026-09-14) | PASS | `wrangler dev --config dist/server/wrangler.json` on 127.0.0.1:8788: `/`, `/subsystems/fpga-capture`, `/subsystems/host-tools`, `/components/la-trigger`, `/components/la-input-sync`, `/reference/{registers,protocol,decoders,wiring}`, `/verification`, `/roadmap`, `/tooling` and `/glossary` all returned HTTP 200 with content (44-111 KB). This is a source/build review, not browser QA; the Site was not published |
| P10.2 driver availability assessment (2026-09-14) | DEFERRED (per plan condition) | `Win32_PnPSignedDriver` lists no com0com/Eltima/HHD virtual COM-pair driver and no pair driver is installed; the Secure Boot state could not be read unelevated. The plan requires a compatible signed driver without changing security settings, so the optional SUMP bridge is deferred and recorded in `docs/sigrok-bridge.md`; no driver or security setting was changed |
| Portal publication (2026-09-14) | PASS publicly | The validated production build was packaged and published through the existing Site project ID. The deployment succeeded, the Site is public, and the production URL serves the current portal release. |
| USB-profile SD boot (2026-09-26) | PASS | Cold boot from the bench microSD with JP5=SD (no JTAG): J10 enumerated `1209:0001` (COM13), `ping` returned `fw 0x00090000 pl 0x4C410002`, `status` reported IDLE, and `test-e2e --target serial://COM13 -k ui` passed 6/6 in 12.89 s on the SD image. `BOOT.BIN` 2,413,488 B, SHA-256 `0821AFD9...F9619D`, matches `build/release/release-manifest.json`. The live gate needs the external JE-1 -> JC-1 patgen jumper (installed for the passing run); the `v1.0` tag remains unauthorized. Evidence: `build/reports/sd-boot-verification.md`, `build/reports/test-e2e.json`. |

Run `lint` and `sim` sequentially. They share the ignored
`build/questa` workspace, and concurrent cleanup can produce Windows file-lock
errors that are not code failures.

## Environment Lock

The authoritative observed versions and paths are in
`tools/toolchain-lock.json`. Important pinned values are:

| Tool or dependency | Observed value |
|---|---|
| Python | 3.11.15 in `.venv` |
| MSVC | 19.44.35228 x64 |
| CMake | 4.4.3 |
| Ninja | 1.13.0 Kitware build |
| Questa | 2025.2 FPGA Starter Edition |
| Vivado/Vitis/Bootgen | 2026.1 |
| ESP-IDF | v5.5.2, commit `30aaf64524299d3bde422ca9a2848090d1bc5d0f` |
| Arm GNU | 12.2.1 |
| PulseView | 0.4.2, project-local under `build/pulseview` |
| Board repository | Digilent `vivado-boards`, commit `36f34ab687b7fa9c778b779d027f3bce63b3ace9` |
| Board part | `digilentinc.com:zybo-z7-10:part0:1.2` |
| Target part | `xc7z010clg400-1` |

The lock has `missing_prerequisites: []`. Its status is
`environment-observed`; smoke statuses for later gates remain blocked where
the implementation is not present.

## Blockers and Caveats

- `python tools/run.py fw-zynq` previously BLOCKED by the Vitis install - RESOLVED 2026-09-06. Two install fixes were needed (recorded in the session log): the platform service needed `Vivado\bin` discoverable and, decisively, the full Vitis Embedded data - the machine originally carried the `.vitis_for_hls` flavor without `Vitis\data\emulation\platforms\*` or xsct; after the AMD Unified Installer repair ("Vitis Embedded Development" + SoCs/Zynq-7000) the service progressed and `fw-zynq` PASSES (ELF evidence in Verified Gates). Residual caveats from the bring-up, all recorded in the code and session log: (1) `vitis.bat` still omits `Vivado\bin` from PATH for the spawned server on this layout (guard builds `C:/AMDDesignTools/Vivado/Vitis` from `RDI_INSTALLVER`) - not needed now because the repair install's platform flow no longer hits that step first, but re-check if a fresh machine fails with "Error in generating Processor List"; (2) the fixed deep project root plus AMD's non-long-path-aware Arm GCC forces short Vitis names (`lap`, `fr`) - documented in the plan P0 path-length gate; (3) platform.build() regenerates FreeRTOSConfig.h from its template, so the policy fragment is applied after build to both header copies and the kernel is rebuilt by direct ninja with export-lib sync (la_platform.py); (4) QSPI is disabled in the PS7 config because the 2026.1 SDT-flow FSBL build fails on the missing `XPAR_XQSPIPS_0_QSPI_BUS_WIDTH` macro and the design boots from SD/JTAG; TTC0 is enabled for the FreeRTOS tick; (5) crashed runs can orphan `vitis-server` java processes that hold file locks - kill `java.exe` with `RigelApp` in its command line before retrying; (6) `ensure_clean_workspace` clears read-only attributes (Python >=3.12 rmtree does not).
- `test-esp32-target` requires a confirmed ESP32 module, a selected COM port and physical hardware. The module is confirmed (DOIT ESP32 DevKit V1, classic ESP32-WROOM-32, COM5) and the gateway is flashed, but the Unity target-test app is not written yet.
- `program`, `flash-esp` and `hil` are human-operated hardware gates. Do not report them as passing without a bench run.
- `espressif/mdns` is not in the IDF v5.5.2 core: it is a component-manager download pinned by `fw-esp32/dependencies.lock` (1.12.0) and cached in the ignored `fw-esp32/managed_components/`. A fresh checkout needs registry access for the first `fw-esp32` build.
- ESP32 flash is a human-operated gate and this DOIT board's auto-reset does not assert GPIO0: every flash after the first needed the manual sequence (hold BOOT, tap EN/RST, release BOOT) plus esptool `--before no_reset`. On 2026-09-10 the 921600 gateway, the Unity test app and the 2 Mbps gateway were all flashed this way and verified. A second gotcha: pyserial asserts DTR/RTS when opening COM5, which resets the ESP32; console readers must open the port unopened, clear the initial line states and then open (`tests/e2e/test_esp32_loopback.py` `_ConsoleWatch`).
- Wi-Fi credentials are set only in the ignored `build/esp32/sdkconfig` (set with the tracked `tools/set_wifi.py` helper or menuconfig; never committed). The gateway associated successfully on 2026-09-10; a fresh checkout must set them again before the network gate.
- P4.10 HIL via a USB-UART adapter is optional now: the on-wire UART0 path is proven through the ESP32 gateway (clean PING/ABORT/LIVE_STOP frames observed in the Zynq RX FIFO over JTAG, then `la-cli` ping/status/captures over TCP). The adapter route remains available for `LA_HIL_PORT=COMx` runs.
- P4 acquisition correctness is resolved on 2026-09-10 (see the session log): the AXI DMA S2MM status stream was enabled but tied off, the raw packer never closed word-aligned bursts with tlast (and leaked the deferred tlast into the next capture), and the pre-trigger ring leaked one leftover sample at the start of a run; a PRE=0 burst also used live pass-through until `stream_en` separated the modes. The P4.6 bench suite now passes 7/7. Remaining P4-adjacent work is P8 measurement, not correctness.
- ESP32 session recovery: the stale-client states observed on 2026-09-11 are explained by the heap exhaustion fixed in P8.2 (with 3.5 KB free the gateway could not process the disconnect or accept new connections). With the heap fix, the deterministic P8.4 RST-kill fault injection (`test_live_tcp_kill_reconnect`) passes 4/4: after an abrupt client RST mid-live the gateway accepts and serves a fresh connection. The test stays in the live suite as a regression guard.
- **P9.1 SPI bench state (2026-09-17, open): the S3 receives every other payload byte.**
  The S3 SPI master and GPIO11/GPIO13 pass the new `LA_SPI_LOOPBACK_TEST`
  (0/64 mismatches), the five SPI jumpers pass a continuity check, and the
  Zynq `axi_quad_spi` slave delivered bit-exact captures with the classic
  ESP32 on 2026-09-13. With the S3 the captured chunks equal the known-good
  UART stream's even bytes plus a constant phase (a 2:1 byte drop). Tried and
  rejected as fixes: SPI mode 1, 64-byte chunks, an explicit zero TX DMA
  buffer, ready-optional continuous reads. Bench prerequisites: plug the
  native-USB cable **directly into the PC** (a hub breaks every usbser.sys CDC
  port) and keep **both** USB cables connected (a single cable browns out under
  Wi-Fi TX). Next: observe SCK/CS/MISO with a scope or the Zybo's own analyzer,
  or rerun the classic ESP32 on the same wiring as an A/B reference.
- **P9.1 SPI bench state (2026-09-13): framed bursts are bench-clean at 5/10/18 MHz; the drain gate is transport-blocked.** Five bench-found defects were fixed: (1) the ESP32 master's 5-byte header/body reader lost framing permanently after one offset (622 of 625 headers read as zeros, collapsing to ~5 bytes per transaction) - it now clocks continuous 1 KiB DMA chunks into the incremental parser; (2) a new capture could inherit the previous capture's partial frame - the Zynq now flushes its ring/FIFOs at every burst job start and the ESP32 resets the SPI parser after a 50 ms idle gap; (3) the task-scheduled FIFO refill could not bridge the half-empty window above 10 MHz (2 underruns/16 KB at 10 MHz, ~150/capture at 20 MHz) - the SPI ISR now refills the TX FIFO directly; (4) the higher-priority SPI task could preempt `la_spi_core_push` mid-compaction and feed from a stale head (CRC errors scaled with capture size: 0 at 4 KB, 44 at 64 KB) - push and feed now share a critical section; (5) the ESP32 relay's NOSPLIT ring refused fragmented puts after a positive capacity answer and silently dropped 39 of 94 parsed frames - control frames now retry within the pressure bound. The SPI producer's wait for ring space is bounded by the 2 s stalled-host timeout instead of a ~20 ms cap that aborted every long capture (1331 dropped frames in one drain attempt). `LINK_CFG` byte 1 now selects the master clock at run time (1-50 MHz), so the bench sweeps rates without a reflash. Measured: 4/16/64/128/256 KB framed captures bit-exact with zero parser errors at 10 MHz; 16 KB bursts clean at 5/10/18 MHz (20/20 at 18 MHz); 19 and 20 MHz lose chunks to slave FIFO underruns (18 MHz is the measured ceiling); the 40 MHz fault injection fails honestly and the next capture recovers; a 4 MiB capture completes with every byte verified. **The sustained host-verified drain is 0.28 MB/s**, limited by the ESP32 Wi-Fi relay (the SPI wire delivers 1.25 MB/s at 10 MHz and 2.25 MB/s at 18 MHz), so the plan's >=1.5 MB/s / 64 MB drain gate is **BLOCKED on the ESP32 -> host transport**; the plan's ESP32-S3 USB-OTG path is the documented replacement.
- **ESP32 gateway TCP-accept degradation (RESOLVED 2026-09-13).** The observed symptom (after a relay flood the gateway stayed `CONNECTED` and healthy but new sessions got no response while an ESP32 reset restored service) was a **link deadlock**, not socket/PCB exhaustion: LinkRxTask blocked forever in the `LA_RELAY_PRESSURE` path while the 64 KiB `link_to_net` ring was full (host stalled), the unread UART RX buffer kept RTS deasserted, the Zynq UART TX stalled and PONGs stopped. The fixes are recorded in the 2026-09-13 session log (bounded pressure wait, event-less UART drain, polled non-blocking sockets, stale-listener refresh, reconnect grace, host stale-NAK handling, single-client test-harness correction). Remaining P9.4 work: the isolated-byte latency gate occasionally exceeds the 25 ms budget under weak RSSI (one 92 ms observation in a full-suite run; passes in isolation), and the P8.3 trace dump is best-effort (a few LOG records can drop under relay pressure; the host tolerates ≤2% and the gateway stays healthy).
- **P9.1 S3 transport state (2026-09-17):** the gateway firmware now targets
  `esp32s3` (DevKitC-1 N16R8) with the S3 pin map, the TinyUSB CDC-ACM host leg
  (`la_usb`), the console on UART0 and the USB-Serial-JTAG secondary console
  disabled. Local gates pass; the bench steps (flash, `ping` over
  `serial://COMx`, P9.1 capture/clock gates, 64 MB drain) are not yet run.
  Physical limit: the S3 USB-OTG is full-speed (12 Mbps), so the drain cannot
  reach the plan's 1.5 MB/s gate; record the measured ceiling and then either
  revise the gate with justification or add a faster last hop.
- **P10 close-out state (2026-09-14).** P10.2 is **deferred** by the plan's own condition: no compatible signed virtual-COM pair driver is installed and the Secure Boot state could not be read unelevated, so no driver or security setting was changed (`docs/sigrok-bridge.md`). P10.3 documentation is current in the tracked `docs/` set and the `web-artifact/` portal validates, builds and serves all routes; the user requested publication on 2026-09-14, so the remaining step is a Codex `sites-hosting` publish that reuses the existing Site project ID (the live Site is still version 1 and `/roadmap` returns 404). P10.4 `boot` and `package` passed on the current artifacts (BOOT.BIN 2,389,904 bytes; ESP32 `.bin` set; wheel; SHA-256 manifest); the [HUMAN] SD boot with JP5=SD was verified on 2026-09-26 (`build/reports/sd-boot-verification.md`); only the `v1.0` tag remains unauthorized. Wiring photos remain a [HUMAN] documentation item; the portal uses the transcribed pin tables instead.
- The ESP32 relay echo ceiling is about 40-44 kB/s round-trip and does not improve from 921600 to 2 Mbps (P5.5 measured 44.4 kB/s at 921600 and 39.5 kB/s at 2 Mbps with `fifo_ovf=0`). This is not one-way live capacity: at 2 Mbps the relay forwarded the full ~120 kB/s 115200 event stream in two runs with zero drops, but three later runs dropped 4-19% of EVT_DECODED frames under Wi-Fi/TCP stalls (RSSI -61 -> -67; `drop` +4105/+5763/+16926; the Zynq stayed clean: `dropped_segments=0`, `fifo_ovf=0`, `line_err=0`). The drops are no longer silent: since 2026-09-11 the relay reports them as `EVT_OVERRUN(stage=2 link)` (deterministic 3 s host read-stall test passes), so the host stops honestly instead of decoding a corrupted stream. P8.2 tuning settled on a 64 KiB static `link_to_net` ring with the lwIP windows back at 16 KiB; the full `test-e2e` suite is green with zero drops in the 60 s gate and a 65.5 KB heap floor. The 32 KiB-window variant was rejected: it exhausted the ESP32 heap during live streaming (3.5 KB free), stalling TCP and dropping 29 k frames without reports, which was the actual cause of the earlier combined-suite failures. The stale-client state observed during that exhaustion remains a P8.4 fault-injection item. At 921600 the wire itself was a real limit (the Zynq reported `EVT_OVERRUN(stage=1, dropped=25328)` in 10 s).
- The `la_irq_connect()` SDT workaround depends on BSP internals (`ScuGicInitialized` in `xinterrupt_wrap.c`); re-check it on any Vitis/BSP upgrade and prefer a supported fix if AMD provides one.
- P6.3 external loopback is complete for all eight channels: with JE-1..4 -> JC-1..4 and JE-7..10 -> JC-7..10 jumpered, the matrix passes 18/18 at the default `--channel-mask 0xFF` (2026-09-11).
- The unconnected JD header leaves `uart0_rx` floating; the earlier storm was actually caused by the SDT handler-dispatch bug (all vectors `StubHandler`) plus a transient floating-RX glitch, not by the floating line alone. With handlers connected, the storm clears; the optional pad pull-up for `uart0_rx`/`uart0_ctsn` remains a defensive measure if the header must tolerate no peer.
- The PL interrupt has no SDT-generated ID (`la_top_0` is a module reference); the firmware uses GIC 61 encoded as 29, derived from `xlconcat/In0 -> IRQ_F2P[0]` (the DMA on `In1` is `XPAR_FABRIC_AXI_DMA_0_INTR = 30`, pinning the mapping). Re-derive if the P3 interrupt wiring changes.
- PulseView 0.4.2 is detected, but its version probe emits bundled Python 3.4 decoder syntax warnings. Resolve or explicitly accept this before relying on interactive decoder integration in P6.
- `build/reports` is ignored. If a report is absent on another machine, the corresponding check is `not-run`, not passed.
- Do not modify the neighboring DDR3 PHY project.

## Next Session Checklist

1. Read `AGENTS.md`, `docs/current-handover.md`, `docs/plan.md` and the latest
   entry at the top of this ledger; inspect `git status --short --branch`.
2. The class project is complete. Read the closeout publication record rather
   than resuming earlier WIP; preserve any new user changes before maintenance.
3. Normal startup: JP5=SD, JP6=USB, J12 power, J10 data. The installed W11
   BOOT.BIN SHA-256 begins 6cb120cc; expected firmware/PL are 0x000A0000 /
   0x4C410002. Enumerate COM ports instead of assuming the number forever.
4. ESP32-S3 runs the combined UART/SPI/I²C stimulus (COM10 console), not the
   gateway. Analyzer endpoint is COM13 on this bench. Keep common ground and
   GPIO17 -> JC-1; no JE-to-JC loopback on an externally driven input.
5. UI w11: hard-refresh one browser tab, Connect, Capture/Decode or Apply live
   config -> Start. Reset UI clears in-memory work and disconnects. Use exports
   for records to retain. See the operator guide for preview limits.
6. Checks follow the new task: protocol/decoder changes require test-native;
   host/UI changes require affected host/page/browser checks; hardware gates
   need actual bench evidence. Do not rerun unrelated licensed tools by habit.
7. SD image and current UI behavior passed on 18 October (6/6 + 10/10). Public
   USB identity, release tag/publication, optional gateway faults, stretch work
   and new external SPI/I2C preview qualification remain separate.
8. Follow the user's next instruction; commit/push or website publication only
   when requested, otherwise record WIP with evidence and next action.

### Historical continuation checklist — superseded

The following dated USB/gateway instructions are retained for provenance only.
They do not describe the current boot mode, default transport or pending work.

**USB execution handoff (2026-09-18):** U00-U02 are complete - baseline
preserved at `b611390`, feasibility recorded in
`build/reports/usb-u01-feasibility.md`, and the integration contract frozen in
`docs/usb-link.md` section 9 with the portable `la_usb_core` host-tested
(`build/reports/usb-u02-contract.md`, `test-native` 16/16). U03 implementation
is complete locally and the bench enumeration is **WIP** (see the U03
subsection): the device negotiates HS through the hub and Windows binds
`usbser.sys`, but it lands in Code 10 after the string-descriptor stage and
no transfer/open-close/reset evidence exists yet. The gateway profile stays
the default and rollback; the S3 SPI fault remains open. Execution is
continuous per the user's 2026-09-18 instruction (stop on request): finish
U03 enumeration/transfers next. Read
`docs/usb-migration-implementation-instructions.md` and `docs/usb-link.md` and
follow the user's current execution instruction. Older dated bench/portal
instructions below are historical context, not new authorization to flash or
publish.

1. Read `AGENTS.md`, `docs/plan.md` and this file before editing.
2. Confirm the checkout root and run `python tools/run.py check-env -- --write-lock`.
3. Run `python tools/run.py query-board -- --write-lock` and confirm the board lock still matches the installed board files.
4. Run `python tools/run.py gen`, `test-native`, `test-esp32-mocks`, `test-host`, `lint` and `sim`; run `lint` and `sim` sequentially.
5. Bench state (2026-09-14, after the P10.1 session): the setup is on the phone hotspot SSID `Lingg`; the ESP32 gateway (COM5) runs the P10.1 firmware (live web view on port 80, `/ws`, Wi-Fi credentials in the ignored `build/esp32/sdkconfig`) at **172.20.10.2** (mDNS `la-esp32.local`); the Zybo runs the JA-1-fixed P9.1 bitstream plus the P9.1 `fw-zynq` loaded over JTAG (Zynq FTDI COM7, 2 Mbps UART link). The UART jumpers (JD-1..4 + GND) and the JE-1 -> JC-1 patgen jumper are installed; the **SPI bulk jumpers (JD-7..10 + JA-1) are not connected** in this reassembly, so the P9.1 capture tests fail until they are rewired. Web view: open `http://172.20.10.2/` (or `http://la-esp32.local/`) and run `la-cli live --decoder uart:rx=0,baud=115200` in parallel to see decoded bytes stream into the page. Verify the analyzer link with `la-cli ping -t tcp://172.20.10.2:5555`; after a flash or reset the first TCP session can need one listener refresh, so retry once after ~15 s. **2026-09-17 S3 update:** the gateway is now the ESP32-S3-DevKitC-1; flash `build/esp32/la_gateway.bin` over the CP2102 "UART" port (`idf.py -B build/esp32 -p COMx flash`), plug the native "USB" port into the PC, and verify with `la-cli ping -t serial://COMx` before running the P9.1 gates over the CDC port.
6. Pause point (2026-09-14, after the P10 close-out): P7, P8, P9.2, P10.1, P10.2 (documented deferral), P10.3 (tracked docs + portal) and P10.4 tooling are complete. P9.1 remains bench-clean for framed bursts; the ESP32-S3 USB CDC host transport is implemented locally and the bench flash + drain measurement are next (the S3 USB-OTG full-speed ceiling is ~1.2 MB/s); P9.3 (200 MS/s decision) and P9.4 (Wi-Fi tuning) remain open; the USB-profile SD boot was verified on 2026-09-26 and only the `v1.0` tag awaits authorization. `la-cli link --spi --clock 18` selects the burst transport and clock, `la-cli live --compact` the compact encoding, `la-cli live --stop-on HEX` the decoded-pattern stop, `la-cli trace` the trace ring; `python tools/run.py package` stages the release artifact set. The portal is validated/built and publication was requested 2026-09-14; publish it from Codex (Sites connector) reusing the existing Site project ID; run its checks from `web-artifact/` with `esbuild scripts/validate-docs.ts --bundle --platform=node --format=esm --outfile=dist/validate-docs.mjs` plus `node dist/validate-docs.mjs`, then `node_modules\.bin\vinext.CMD build`.
7. Keep ESP-IDF generated configuration at `build/esp32/sdkconfig`; do not commit a source-level `sdkconfig`. Change Wi-Fi credentials with `.venv\Scripts\python.exe tools\set_wifi.py`. The ESP32 auto-reset does not assert GPIO0: use the manual BOOT/EN sequence plus esptool `--before no_reset` for flashes, and keep console readers from asserting DTR/RTS.
8. After source or protocol changes, rerun the relevant native and simulation gates and update this document with the command, result and report path.

## Continuation Anchors

| Need | Start here |
|---|---|
| Normative requirements | `docs/plan.md` |
| Session state and blockers | `docs/status.md` |
| Current boot/image/wiring and continuation | `docs/current-handover.md`; SD W11 verified, not U03 bring-up |
| Direct USB execution history | `docs/usb-migration-implementation-instructions.md` (U00-U09 historical; USB adopted/default) |
| Implemented direct USB interface and W11 additions | `docs/usb-link.md`, `docs/protocol.md`, `docs/unified-ui-plan.md` |
| Canonical commands and environment import | `tools/run.py` |
| Tool versions and smoke state | `tools/toolchain-lock.json` |
| Board part and source commit | `tools/board-lock.json` |
| Native protocol/decoder/ownership tests | `tests/native/`, `fw-zynq/tests/host/`, `CMakeLists.txt` |
| Questa flow | `tools/regress.py`, `fpga/tb/testlist.json`, `fpga/tb/tb_smoke.sv` |
| ESP-IDF build | `fw-esp32/`, `docs/windows-dev.md` |
| Vivado/XSA entry point | `fpga/scripts/build.tcl` |
| HardwareX paper package | `docs/hardwarex/` (`template.md`, `manuscript.md`, `diagrams/`) |


## Session Log

| Date | Work completed | Result |
|---|---|---|
| 2026-10-18 | W11 USB SD image refreshed, previous image backed up, FAT32 card BOOT.BIN atomically replaced and hash-verified; user cold-booted with JP5=SD. Current handover/status docs synchronized. | PASS: BOOT.BIN 2,415,152 bytes / SHA-256 6cb120ccc53c573edf9188a82d2fe1622def58836ffa46fdc40224eca847b892; firmware 0x000A0000 / PL 0x4C410002; SD USB UI E2E 6/6 and browser/ESP32 10/10. Reports build/reports/sd-w11/. Source/doc work remains WIP; no remote publication requested. |
| 2026-09-26 | Windows no-terminal UI launcher. Added `tools/win/la-ui-start.vbs` (hidden `.venv\Scripts\la-ui.exe`, port-8080 wait, browser open, error dialogs) and `tools/win/la-ui-stop.vbs` (terminates the Python listener on 8080, reports other owners), created the machine-local **LA UI** and **Stop LA UI** desktop shortcuts, and documented the launcher in `docs/using-the-analyzer.md` section 4, `docs/windows-dev.md` and `README.md`. | PASS (local smoke): `cscript tools\win\la-ui-start.vbs nobrowser` returned 0 with `127.0.0.1:8080` LISTENING and the page served HTTP 200 (7,194 bytes); a second start reused the listener (same PID); `cscript tools\win\la-ui-stop.vbs quiet` reported stopped and freed the port; the not-running and port-in-use paths were exercised. No firmware, RTL, protocol or UI change. |
| 2026-09-26 | USB-profile SD boot verification (bench). Regenerated `BOOT.BIN` with `boot` (2,413,488 B, SHA-256 `0821AFD9...F9619D`, matching the release manifest), wrote it to the FAT32 root of the bench microSD, and cold-booted the Zybo with JP5=SD and no JTAG load. J10 enumerated `USB\VID_1209&PID_0001\LA-USB-TEST-0001` (COM13), `la-cli ping` returned `fw 0x00090000 pl 0x4C410002`, `status` reported IDLE, and `test-e2e -k ui` passed 6/6 (12.89 s) on the SD image. The first attempt scored 4/6 because the external JE-1 -> JC-1 patgen loopback was absent (live reads the real input; captures use the internal selftest mux); a `CTRL_STREAM \| CTRL_SELFTEST_EN` session proved the live datapath (10 UART events) and reconnecting JE-1 -> JC-1 produced the 6/6 run. Docs updated to mark the P10.4 SD gate verified. | PASS (bench): `build/reports/sd-boot-verification.md`, `build/reports/test-e2e.json`. No firmware, RTL or protocol change. Open: `[HUMAN]` browser click-through, `v1.0` tag, portal SD-boot labels. |
| 2026-09-18 | U03 J10 hardware control (bench-only stock example). Added a `--profile example` path that builds AMD's unmodified `usbps_v2_9` mass-storage example (`xusbps_intr_example.c` + Chapter 9 + storage class, copied verbatim into `fw-zynq/example/`) against the existing `lap` platform, plus `fw-zynq/example/la_freertos_trace_stubs.c` for the BSP P8.3 trace hooks and the `example` mappings in `fw-zynq/scripts/la_platform.py`, `fw-zynq/scripts/jtag_load.tcl` and `tools/run.py`. The example keeps the vendor's `while (NumReceivedFrames < 1)` spin (counter never incremented, so it serves forever) and vendor endpoint sequence (`XUsbPs_EpEnable`, EPCR1 `TXT_BULK|RXT_BULK|TXR|RXR`, `XUsbPs_EpPrime` OUT). Bench on J10 direct to the PC: Windows enumerated `USB\VID_0D7D&PID_0100` as a mass-storage device (`Xilinx PS USB VirtDisk`, 1,048,576 bytes), a 10 s COM7 trace showed a continuous SCSI command stream (TEST UNIT READY / MODE SENSE / READCAP / READ at 0x0/0x400/0x8000) with no stalls, and an elevated raw test wrote 65,536 bytes and read them back with 0 mismatches; the device `VirtFlash` JTAG readback matched the host pattern exactly (`18110A03 342D261F` at offset 0x40000). | PASS (hardware control): J10/PHY/USB0 cleared for control, bulk IN and multi-packet bulk OUT; the U03 alternating loss is project firmware/driver configuration. Evidence `build/reports/usb-migration/U03/example-write-test.txt`, `fw-zynq -- --profile example` PASS (`la_usb_example.elf` text 78,598 / data 3,084 / bss 1,210,640), `jtag-zynq -- --profile example` PASS. The example profile is not part of the product build; the board runs the example until the next `jtag-zynq -- --profile usb`. No product firmware fix claimed. Committed and pushed as `cf1320f U03: clear J10 hardware with the stock mass-storage example`; `git ls-remote origin refs/heads/main` matched `git rev-parse HEAD`. |
| 2026-09-18 | U03 direct-USB implementation + bench bring-up (WIP). New `la_usb_core` (session model), `la_usb_desc` (CDC-ACM descriptors, `wTotalLength` 75, test PID `0x1209:0x0001` warning), `la_usb_dev` (XUsbPs wrapper: zeroed 64 KiB non-cached DMA pool at NC offset 0x00900000, 8 x 1088-byte TX pool, polled setup/RX/TX service with the vendor cursor advance, reset/error/port-change ISR notification only), `la_usb_task` (EP0 standard/CDC requests, `rx_sb` producer, TX pump, session actions), `LA_TRANSPORT_USB` integration in `la_app`/`main`/`la_tx_task`/`la_cmd_dispatch` (LINK_CFG unsupported NAK)/`la_status_task`, build profiles in `tools/run.py` + `fw-zynq/scripts` + `jtag_load.tcl`, and native `la_native_usb_desc`/`la_native_usb_core` tests. Bench: HS on the hub and on a direct PC port, complete enumeration and `usbser.sys` binding (`USB\VID_1209&PID_0001\LA-USB-TEST-0001`, COM12 Status OK), 25 control requests with 0 errors (SET_CONFIGURATION, GET/SET_LINE_CODING, SET_CONTROL_LINE_STATE). Fixed: SDT `LookupConfig` u16 truncation, missing DMA-pool zeroing (wild controller DMA), SLE/UI suspend storm, and the deferred-pop vendor RX spin. | WIP/BLOCKED on EP1 bulk OUT packet loss after a successful enumeration and control path. Bench session 2 (same day): fixed the driver's missing endpoint enables (EPCTRL1/2 were 0, so host tokens to EP1 timed out), the EP0 stale-status/class-data confusion, the deferred-release ring desync (now pop+copy into a bounded staging ring + release in the callback), and confirmed HS enumeration, `usbser.sys` binding (COM12 OK), `la-cli ping`/`status` successes. Remaining: ~50% of host bulk OUT transfers never complete a dTD (50 writes -> 25 completions; 10 -> 9; 8 at 300 ms -> 4; a 140-byte FS write delivered only its first 64-byte packet) while the dTD ring is fully active (`0x02008080` tokens, circular NLP, `EPRDY=3`, `EPCTRL1=0x00800080`, `out_waiting=0`, `bad=0`, `ovf=0`); both HS and forced FS lose. Long status prints also still corrupt `.rodata` (short lines are stable). No open/close/reset-cycle evidence; no hardware PASS claimed. Local gates PASS: `test-native` 17/17; USB and gateway profiles build. Next: resolve the EP1 OUT completion loss (driver dTD/dQH handling or a different device-mode approach) and the status-print fault. |
| 2026-09-18 | U02 integration contract frozen (continuous execution, stop on request). Audited the generated `XUsbPs` driver: `EpBufferReceive` pops without re-activating, the vendor RX dispatcher advances its descriptor cursor per callback (so the pop must stay in interrupt context), `EpBufferRelease` re-activates, the reset path has an infinite fallback unless a general handler is registered, and `EpStall`/`EpBufferSendWithZLT` exist. Froze `docs/usb-link.md` section 9: GIC ID 53 via `la_irq_connect`, notification-only callbacks with deferred class processing and stale-generation dropping, CDC-ACM descriptors with IAD/`wTotalLength` 66 and automatic `usbser.sys` binding, test identity `0x1209:0x0001` (release identity open), advisory line coding/DTR, no-ZLP byte stream, non-cached 64 KiB DMA pool at NC offset `0x00900000`, 8 x 1088-byte TX pool, 16-entry completion ring, `rx_sb` NAK backpressure, gateway-default `fw-zynq -- --profile usb` builds and `test_usb_*` selectors. Added portable `la_usb_core.{h,c}` and host tests. | PASS (local): `test-native` 16/16 including the new `la_native_usb_core`; report `build/reports/usb-u02-contract.md`, canonical `build/reports/test-native.json`. No target code, build, load or hardware run; U03 not started. Next: U03 driver/task implementation and local profile builds, then the [HUMAN] JTAG enumeration session. |
| 2026-09-18 | U01 board and platform feasibility complete (continuous execution authorized by the 2026-09-18 user instruction, stop on request). Inspected the actual `logic_analyzer.xsa` (`system.hwh`: USB0 enabled, 60 MHz, MIO 28..39, reset MIO 46, USB1 disabled), the generated FreeRTOS BSP (`XPAR_XUSBPS_0_BASEADDR 0xe0002000`, encoded IRQ `0x4015`, MIT-licensed `usbps`/`XUsbPs` sources, no CDC example) and the installed `usbps_v2_9` examples; evaluated the old AMD CDC techtip (bare-metal SDK 2015.1, custom Windows driver, click-through RefDesLicense, AR#61313 ULPI caveat) as reference only. Recorded the physical checklist: REV D.0, JP1 unsoldered/open, JP2 shunt removed, JP5=JTAG, JP6=USB (power from J12), J11 empty, J10 on the USB hub by user direction (direct PC port is the first isolation step), FTDI `COM7` serial `210351BE7ACEB`, no board modification, fresh power with no image. Identity basis: pid.codes test PID `0x1209:0x0001` for in-house use only; release identity open. Host check: `la-cli ports` shows only COM7 and the gateway is not attached, so the optional gateway baseline is BLOCKED, not failed. | PASS (local evidence + checklist): `build/reports/usb-u01-feasibility.md`. No firmware built, no image loaded, no hardware PASS claimed. Open: HS vs FS until U03, release VID/PID, hub-first deviation recorded in the report. Next: U02 contract freeze. |
| 2026-09-18 | U00 execution baseline complete (first authorized U packet). Authorized scope: execute the first incomplete eligible U packet (U00) of `docs/usb-migration-implementation-instructions.md`; hardware load, product adoption, publication and release were not authorized. Synced: `git fetch` clean, `main` at `b611390` matching `origin/main`, worktree clean except the preserved untracked `docs/hardwarex/` and `docs/learning/`. Preserved and hashed the rollback identity under ignored `build/usb-migration/baseline/` (P9.1 JA-1-fixed bitstream `06488a9...`, XSA `0719cbd...`, loaded Zynq ELF `9067cfa...`, BOOT.BIN `bc1d1a1...`, S3 gateway `0c96ed8...`, classic package set, timing/DRC/utilization, toolchain/board locks) with `SHA256SUMS.txt`; no rebuild ran, so `build/vitis/ws` is untouched. Ran the required off-board baseline checks. | PASS: `test-native` 15/15 (`build/reports/test-native.json`) and `test-host` 84/84 (`build/reports/test-host.json`), copies under `build/reports/usb-migration/U00/`; report `build/reports/usb-u00-baseline.md`; no pre-existing failures to isolate. No flash, JTAG load, E2E or bench step ran. Open blockers carried: S3 SPI 2:1 byte drop (open, not relabeled fixed), S3 USB-OTG full-speed ~1.2 MB/s below the 1.5 MB/s gate, U01 `[HUMAN]` board/power/identity checklist, U02 ISR-deferral/identity decisions, P10.4 release hold. Next: U01 local feasibility evidence plus the physical checklist; U02 design may proceed locally, no U03 load until U01/U02 gates pass. Committed and pushed as `72a4bc3 U00: establish the USB migration execution baseline`; `git ls-remote origin refs/heads/main` matched `git rev-parse HEAD`, and this status-only close-out records that verified push. |
| 2026-09-17 | Prepared the direct-Zynq USB planning package: new `docs/usb-migration-implementation-instructions.md` (U00-U09) and `docs/usb-link.md`; linked from `docs/plan.md` and `README.md`; updated this ledger and clarified the root v0.4 snapshot. Reviewed preset/generated BSP/installed `usbps_v2_9`, app routing and primary AMD/Digilent/Microsoft references. Corrected the proposed driver, power and CDC-example assumptions; specified ownership/IRQ feasibility, failure recovery, exact performance measurement, adoption, documentation and release gates. | Planning complete, pushed as `8eaf3ea`; remote hash verified. Documentation checks PASS: `.venv/Scripts/python.exe build/reports/usb-planning-review.py` (20 local links, ten ordered packets with entry/exit gates; `build/reports/usb-planning-review.json`), `git diff --check`, `git diff --cached --check`; manual scope and technical review complete. Firmware and hardware unverified; no implementation authorized or started. Tracked worktree clean after push; existing untracked `docs/hardwarex/` and `docs/learning/` preserved and excluded. This status-only close-out records the push. Next implementation action, when authorized: U00. |
| 2026-09-17 | P9.1 ESP32-S3 bench bring-up (CDC host transport works; SPI bulk link still open). (a) CDC: `la-cli ping/status/capture` work over `serial://COM11` on a **direct PC USB port**; behind a Genesys Logic USB hub every usbser.sys CDC device (ours and the ESP32 ROM's USB-Serial-JTAG) fails (`GetCommState` hangs, then errors 22/31 with re-enumeration) while the vendor-driver CH343/FTDI ports work - the hub is unusable for the CDC link. (b) Power: with only the CH343 cable connected the S3 browns out (`E BOD: Brownout detector was triggered`, `rst:0x3`) under Wi-Fi TX peaks; both USB cables must be connected. (c) Two stack overflows found on the bench: LiveView (the ~1 KiB `la_proto_parser_t` on its 3 KiB stack; now a static parser) and UiTask (3072 B with <120 B headroom; now 4096 B) - both panicked and re-enumerated USB. (d) `host/la_cli/transport.py` imported `serial` inside `__init__`, so its exception handlers raised `NameError` on any serial fault; now a module-level import. (e) The DevKitC-1 RGB LED (WS2812) is cleared with one black frame at boot (RMT, both known GPIOs). (f) New Kconfig `LA_SPI_LOOPBACK_TEST` (MOSI<->MISO jumper, Zynq SPI wires off) proves the S3 SPI master and GPIO11/GPIO13: **0/64 mismatches**. (g) Bench results: a 4 KB UART+CDC burst capture is bit-exact; the **SPI bulk link still fails** - the S3 receives the payload with every other byte dropped (the chunk dump matches the known-good UART stream's even bytes plus a constant phase), the five SPI wires pass a continuity check, the Zynq slave shows tx/underrun/half-empty activity, and the same slave delivered bit-exact captures with the classic ESP32 on 2026-09-13; root cause not yet identified (mode 0/1, 64/1024-byte chunks, an explicit TX DMA buffer and ready-optional reads all still fail). | Local gates PASS: `fw-esp32` (`build/reports/fw-esp32.json`), `test-esp32-mocks`, `test-native`, `test-host` (84 passed), compileall clean. Bench PASS: CDC ping/status/abort, UART+CDC 4 KB capture, SPI loopback self-test. Bench OPEN: the SPI bulk link (evidence above); next step is signal-level instrumentation (scope or the Zybo's own analyzer) or an A/B rerun with the classic ESP32 to find the byte-drop cause. |
| 2026-09-17 | P9.1 ESP32-S3 transport port (implementation complete, bench gates pending). Scope: switch `fw-esp32` to the `esp32s3` target (16 MB flash, single-app-large partition, console on UART0 so the native USB port stays free), add the target-conditional S3 pin map in `la_board.h` matching the bench wiring (UART TX17/RX16/RTS15/CTS14, SPI2/FSPI SCK12/MOSI11/MISO13/CS10/ready9, `LA_BOARD_SPI_HOST` 2->1), and add a TinyUSB CDC-ACM host leg (new `la_usb` component, `espressif/esp_tinyusb` managed component gated to esp32s3) that owns the analyzer stream while the host has the CDC port open; the TCP leg yields to it. Host side needs no change (`la-cli`/e2e already accept `serial://COMx`). Intended files: `fw-esp32/sdkconfig.defaults`, `la_board.h`, `main/{main.c,ui_task.c,CMakeLists.txt,Kconfig.projbuild}`, new `components/la_usb/`, `la_link` (shared host-frame forward), `la_net` (arbitration + counters), plus `docs/{wiring,spi-link,architecture,limits,using-the-analyzer}.md` and this file. Acceptance: `fw-esp32` builds for esp32s3 and flashes; `test-esp32-mocks`, `test-native`, `test-host` stay PASS; bench `la-cli ping -t serial://COMx` then the P9.1 capture/clock gates over the USB CDC port; the 64 MB drain is measured and recorded honestly. Known risk: ESP32-S3 USB-OTG is full-speed (12 Mbps, bulk ceiling ~1.2 MB/s), so the plan's >=1.5 MB/s drain gate cannot be met over this transport; the measured ceiling is recorded instead. | PASS locally: `fw-esp32` builds for esp32s3 (`build/reports/fw-esp32.json`; `la_gateway.bin` 0xe7280 bytes, 38% of the 1500K partition free; the target switch used `idf.py set-target esp32s3` after backing up the ignored sdkconfig and the Wi-Fi lines survived); `test-esp32-mocks` 4/4, `test-native` 15/15, `test-host` 84/84, `git diff --check` clean. Bench gates not yet run: flash the S3 over the CP2102 "UART" COM port, `la-cli ping -t serial://COMx` on the native USB CDC port, then `test-e2e --target serial://COMx -k test_p91` and the 64 MB drain. Risk recorded before the work: the S3 USB-OTG is full-speed only (12 Mbps, bulk ceiling ~1.2 MB/s), so the plan's >=1.5 MB/s drain gate cannot be met over this transport; the measured ceiling is to be recorded honestly. |
| 2026-09-17 | HardwareX documentation package (WIP, not yet committed). (a) `docs/hardwarex/template.md`: Markdown rendering of the HardwareX article template (Version 2, June 2021) with all sections and italic instructions preserved. (b) `docs/hardwarex/diagrams/system-block-diagram.drawio`: new 10-page draw.io block diagram in the skill's hardware-architecture house style - L1 system context, L2 physical deployment, L2 end-to-end data and control paths, and one Level-3 page per subsystem (signals and physical I/O, FPGA capture pipeline, AXI DMA and DDR, Zynq FreeRTOS control, framed UART + SPI link, ESP32 wireless gateway, host tools); per-page legends are generated only from the roles actually used. (c) `docs/hardwarex/diagrams/figures/`: 10 PNG (scale 2) and 10 SVG exports. (d) `docs/hardwarex/manuscript.md`: filled manuscript with all ten figures embedded as relative Markdown image links (verified resolvable), the specifications table, design-file summary, a 12-row bill of materials priced in IDR from Indonesian marketplaces (Zybo Z7-10 Rp 13,800,000 Tokopedia pre-order, ESP32 DevKit V1 Rp 45,000, USB cables Rp 25,750 each, jumper set Rp 20,700, microSD 16 GB Rp 82,500, 5 V adapter Rp 37,050, Wi-Fi router Rp 218,000; required subtotal Rp 14,254,750 = about US$805, Rp 14,408,550 = about US$814 with optional/planned items; reference rate US$1 = Rp 17,699.87), build and operation instructions, and the validation summary taken from this ledger. | Diagram: `validate.py --score` reports 0 errors (1250 score; remaining warnings are enclosure-boundary and edge-crossing notices expected for enclosed block diagrams); all 10 pages rendered with the draw.io CLI and visually reviewed through two fix rounds (legend collisions, labels wider than column gaps, edges routed through unrelated boxes). Prices sourced from Tokopedia listings on 2026-09-17 (Zybo Z7-10 Rp 13,800,000 pre-order at Invok Lab, in-stock alternative Rp 14,500,000; ESP32 DevKit V1 CP2102 Rp 45,000; micro-USB 1 m Rp 25,750; jumper set Rp 20,700; microSD 16 GB class 10 Rp 82,500; 5 V 2 A adapter Rp 37,050; Mercusys MW306R router Rp 218,000; ESP32-S3-DevKitC-1 Rp 115,000; CP2102 USB-UART Rp 25,000; breadboard Rp 13,800); reference rate US$1 = Rp 17,699.87 from open.er-api.com; the BOM notes the Digilent overseas alternative (US$314 board = about Rp 5,557,800 before shipping/import duty). WIP: files are uncommitted pending the user's review; the open license row, author/affiliation/CRediT fields, funding statement and the repository DOI deposit remain [HUMAN] items. |
| 2026-09-14 | Portal navigation repair. The published Vinext client Link runtime threw `TypeError: e is not a function`, so clicking an otherwise valid internal link did not navigate, although direct URLs rendered. Replaced portal-internal `next/link` use with ordinary anchors so navigation uses normal document requests and does not invoke the failed client handler; the repaired build is publicly published. | Local documentation validation PASS (7 subsystems, 78 components, 21 registers, 24 protocol messages, 11 roadmap phases, 55 glossary terms, 182 coverage entries); production `vinext build` PASS (11 routes); no remaining `next/link` or `<Link>` use under the portal `app/` and `components/` trees. `oxlint` reports only the established vendored `components/ui` and `hooks/use-mobile.ts` findings; the compatibility change adds no lint findings. Public verification PASS: clicking the homepage `Roadmap` link loads `/roadmap` with the `Roadmap` heading and no browser errors. |
| 2026-09-14 | Portal publication. Reused the existing Sites project ID, packaged the validated `web-artifact/` build, and published its current portal release. The Site access remains public. | Existing production build packaged successfully; the Sites deployment succeeded and returned the production portal URL. |
| 2026-09-14 | I2C worked example. Both guides now include a step-by-step I2C walkthrough: probe wiring (SCL -> JC-1/CH0, SDA -> JC-2/CH1, GND -> JC-5 or JC-11; never the bus VCC; 3.3 V only), the separate JD-1..4 PC link, the live decode command (`--decoder i2c:scl=0,sda=1 --rate 10M`) with an internal-patgen dry run (`--patgen i2c --decoder i2c:scl=3,sda=4`), and the full waveform capture (`--rate 10M --words 64k --mode raw --pre 1000 --out i2c.sr`) with the one-edge-channel trigger limitation and export notes. | PASS: docs-only change; portal `validate` and `vinext build` PASS. Committed and pushed. |
| 2026-09-14 | Analyzer usage guide and self-test capture option. (a) New `docs/using-the-analyzer.md`: bench operating guide covering required hardware and wiring, host setup, Zynq build/load (JTAG and SD), ESP32 credentials/build/flash, link verification, a first self-test capture, real-signal capture options, live decode decoder specs and examples, the browser live view, task/queue trace, SPI bulk transport, a full `la-cli` command reference and troubleshooting. (b) Portal: new `/using` route with the same guide, linked from the header navigation; the portal contract's route table now includes `/using`; `README.md` links the guide and shows the first three `la-cli` commands. (c) New `la-cli capture --patgen MODE` option: it enables the internal self-test mux (`CTRL.SELFTEST_EN`) and latches the pattern generator so a first capture needs no external signal; the host test `test_capture_command_patgen_selftest` asserts the PATGEN_CFG latch and the CONFIG control flag. (d) Both guides now state explicitly that burst capture and live decode are independent, mutually exclusive modes (neither requires the other; each command latches its own configuration), and the CLI clears the pattern-generator enable before setting a new mode so repeated `--patgen` sessions relatch instead of keeping the previous pattern; the mock records `patgen_history` and the test asserts the zero-then-enable sequence. | Local gates PASS: `test-host` 84/84 (new CLI test), portal `validate` (7/78/21/24/11/55/182), `vinext build` with the `/using` route, `/using` HTTP 200 (64,255 bytes) and a headless Edge screenshot check, `compileall`, `git diff --check`. The SPI drain and release-tag items are unchanged. Committed and pushed in this session. |
| 2026-09-14 | Portal Level 1 diagram usability fix. The seven-node system diagram used a fixed seven-column grid, so long status badges and headings overflowed their cards and bled into neighbors, and selecting a flow highlighted every node in the "Capture data" path without showing any order or direction. Fixed: the diagram is now an auto-fit grid (min 250 px cards, wrapping to 4/2/1 columns), cards use `min-width: 0` with `overflow-wrap: anywhere`, status badges wrap and show the short status label with the scope sentence as separate text, active nodes carry a "Step N" badge plus "receives from / sends to" lines, and dimmed nodes say they are not part of the selected flow; the legend explains the new vocabulary. | PASS by source/build review and a headless Edge screenshot of `http://127.0.0.1:8788/`: 7 cards, 7 step badges, 7 route lines, no horizontal overflow; `vinext build` PASS. The site was not published. |
| 2026-09-14 | P10.2/P10.3/P10.4 close-out (docs currency, portal, release tooling). (a) Docs currency: rewrote `README.md` for the current state (problem, specifications, commands, docs map, limits); updated `docs/architecture.md` (P9.1 SPI slave, three interrupt sources, DMA status-stream fix, 2026-09-13 utilization/timing, current artifacts), `docs/wiring.md` (P9.1 bench state and P10.1 web view), `docs/plan.md` (new "Implementation status (2026-09-14)" section) and `docs/test-plan.md` (P7-P10 acceptance); added `docs/limits.md` (electrical/sampling/link/host/tooling limits with evidence pointers) and `docs/sigrok-bridge.md` (P10.2 assessment and revisit conditions); the `host/la_sigrok_bridge` docstring now records the deferral. (b) Portal contract: revised `docs/web-artifact-implementation-instructions.md` to the current ledger (P7-P10.1 statuses, SPI bulk-link and web-view components, resolved stale-client history, updated snapshot/baseline, coverage areas, acceptance checklist). (c) Portal implementation: new typed content under `web-artifact/lib/docs/` (types, snapshot, 7 subsystem records, 78 component records across seven subsystem files, 21 registers, 24 protocol messages with payload layouts and errors, UART/SPI/I2C decoder docs, 20 runner targets with the 12 RTL mappings, 11 roadmap phases, 55 glossary terms, 182 coverage entries, build-time validation); new shell under `web-artifact/components/docs/` (header with mobile Sheet, footer, breadcrumbs, status badges, context strip, interface tables, evidence panel, source links, Level 1 flow diagram, subsystem diagram); rewritten `/` plus new `/subsystems/[slug]`, `/components/[slug]`, `/reference/{registers,protocol,decoders,wiring}`, `/verification`, `/roadmap`, `/tooling`, `/glossary` and `not-found` routes with a documentation CSS layer. (d) P10.4 tooling: new `tools/make_release.py` and `python tools/run.py package` target (ESP32 `.bin` set, host wheel, SHA-256 manifest); `boot` rebuilt BOOT.BIN from the current artifacts. | Local gates PASS: `boot` (BOOT.BIN 2,389,904 bytes), `package` (ESP32 set + wheel 57,811 B + manifest), portal `validate` (7/78/21/24/11/55/182), `vinext build`, route render review (12 routes HTTP 200), `compileall`, `git diff --check`, oxlint clean on new files. Bench BLOCKED as recorded: [HUMAN] SD boot and `v1.0` tag wait for the ESP32-S3 transport port (user decision); the portal was not published; wiring photos remain [HUMAN]. Committed and pushed in this session (see the snapshot row for the final hashes). |
| 2026-09-14 | P10.1 live web view. New `fw-esp32/components/la_web`: `esp_http_server` on port 80 serves one embedded single-page UI (`web/index.html`, per-decoder tabs, byte/annotation rendering, stats bar, auto-reconnect) and a `/ws` WebSocket streams the decoded events that LinkRxTask already mirrors into the 8 KiB live ring. Pure `la_web_core` formats each `EVT_DECODED` frame into compact JSON (`{"s":slot,"e":[[t,kind,"hex"],...]}`) with bounds/truncation handling; host-tested as CTest `la_esp32_mocks_web` (layout, hex, truncation, slot and unbound guards). `la_link` gained `la_link_receive_live`/`la_link_release_live`; the LiveViewTask broadcasts to up to 4 clients, prunes dead sockets, and emits a 1 s stats message (relay counters + heap); UiTask logs `web=clients/sent/dropped`. `CONFIG_HTTPD_WS_SUPPORT=y` added to `sdkconfig.defaults` (and the ignored sdkconfig) because the WS APIs are gated; the static JSON buffer (2560 B) and LiveView stack (3072 B) were trimmed after a 320-byte DRAM overflow; `la_gateway.bin` 0xde2d0 (13% partition free). New bench gate `tests/e2e/test_p10_web.py` (stdlib WebSocket client): page fetch + WebSocket event/stats stream. | Local gates PASS: `test-esp32-mocks` 4/4, `fw-esp32`, compileall. Bench PASS on the phone hotspot (SSID `Lingg`, gateway 172.20.10.2): `test_p10_web.py` 2/2 - the page is served and the WebSocket carried the decoded `LA-TEST\\n` bytes plus the stats message during a live UART session. The SPI bulk jumpers are not connected in this reassembly, so the P9.1 capture gates were not part of this run (documented in the bench-state checklist). Visual check: the page rendered the live `LA-TEST` bytes in Decoder 0 with per-byte sample deltas and the stats bar (`events 2743 | dropped 50`); the ESP32 counters read `web=1/3019/50` (3019 event frames sent, 50 dropped while no browser was attached). `la_esp32_mocks_web` also gained a real frame round-trip (encode -> parse -> JSON). |
| 2026-09-13 | P9.1 SPI link bring-up and bench characterization. Five defects fixed after the earlier 4 KB-only pass: (a) the ESP32 master's 5-byte header/body reader lost framing permanently after one offset (622 of 625 headers read as zeros and throughput collapsed to ~5 bytes/transaction) - replaced with continuous 1 KiB DMA chunks into the incremental parser plus a 50 ms idle-gap parser reset; (b) a new capture could inherit the previous capture's partial frame - the Zynq flushes its ring/FIFOs at every burst job start; (c) slave FIFO underruns above 10 MHz (2 per 16 KB at 10 MHz, ~150 per capture at 20 MHz) - the SPI ISR refills the TX FIFO directly; (d) the higher-priority SPI task could preempt `la_spi_core_push` mid-compaction and feed from a stale head (CRC errors scaled with capture size: 0 at 4 KB, 44 at 64 KB) - push and feed now share a critical section; (e) the ESP32 relay's NOSPLIT ring silently dropped 39 of 94 parsed frames after a positive capacity answer - control frames now retry within the pressure bound. The SPI producer's wait for ring space is bounded by the 2 s stalled-host timeout instead of a ~20 ms cap (1331 dropped frames in one drain attempt). Additive `LINK_CFG` byte 1 selects the ESP32 master clock at run time (1-50 MHz), so the bench sweeps rates without a reflash (protocol.md/spi-link.md updated; host `pack_link_cfg` clock field, `la-cli link --spi --clock`, host test). New `tests/e2e/test_p91_spi.py` plus conftest `--p91-clock`/`--p91-drain-mb`. Bench: 4/16/64 KB captures bit-exact at 10 MHz, 64/128/256 KB clean manually, 16 KB bursts clean at 5/10/18 MHz (20/20 at 18 MHz), 40 MHz fault injection fails honestly with a clean recovery, and a 4 MiB capture completes with every byte verified. | Local gates PASS: `test-host` 83/83, `fw-esp32`, `fw-zynq`, `jtag-zynq`, compileall, `git diff --check`. Bench PASS: `test_p91_spi_capture_sizes`, `test_p91_spi_clock_sweep`, `test_p91_spi_fault_recovery`. Bench BLOCKED: `test_p91_spi_drain_rate` measures 0.28 MB/s host-verified vs the 1.5 MB/s target - the ESP32 Wi-Fi relay is the binding limit (the SPI wire delivers 1.25 MB/s at 10 MHz and 2.25 MB/s at 18 MHz); the plan's ESP32-S3 USB-OTG path is the documented replacement. 19/20 MHz lose chunks to slave FIFO underruns (18 MHz is the measured burst ceiling). |
| 2026-09-13 | P7.7 protocol-aware stop. `DECODER_CFG` byte 2 is `stop_on_len` and the type's reserved bytes carry the decoded-byte pattern (UART 9..11 max 3, SPI 13..19 max 7, I2C 6..19 max 14); unused pattern bytes must be zero and an oversized length is NAK bad-field. `la_decoder_core` matches BYTE record data with a rolling window and latches the matching `t_sample`; the decoder task reports the first match to AcqTask (`LA_NOTIFY_STOP_ON`), which reuses the LIVE_STOP quiesce/drain path (DMA aborted at once, still-owned completed segments drained by the decoders) and posts `LA_NOTICE_STOP_ON`; CmdTask forwards `EVT_TRIGGERED(match_index)` plus a LOG line and returns the view to IDLE. Host: `pack_decoder_cfg_*` gained `stop_on`, `AnalyzerClient.live(stop_on_match=...)` keeps the match index, `la-cli live --stop-on HEX`, mock `live_stop_on` scenario. Drive-by fix: the CLI text view rendered non-printable decoded bytes as U+FFFD, which crashed on a cp1252 console; it now renders '.'. Firmware version 0x00090000. | Local gates PASS: `test-native` 15/15, `test-host` 83/83, `release` 18/18, `test-esp32-mocks` 3/3, `fw-zynq` (text 186712 / data 3688 / bss 241840), compileall and `git diff --check` clean. Bench PASS: `jtag-zynq` loaded the P7.7 ELF over the P9.1 bitstream; `test-e2e --live -k test_live_stop_on_decoded_pattern` passed; `la-cli live --stop-on 4c41` printed `stop-on match at sample 69386`; full `test-e2e --live` 55 passed + 3 skipped + 1 known flake (P8.3 trace LOG loss 116/4096 = 2.83% > the 2% best-effort tolerance; passes in isolation). Committed and pushed as `923801d P7: add the decoded-pattern stop (P7.7)`; annotated tag `v0.2-live` pushed. |
| 2026-09-13 | P9.1 SPI bench session (firmware + physical layer). Loaded the JA-1-fixed bitstream and rebuilt `fw-zynq`; found and fixed the whole SPI service path: (a) the slave reported `NOT_SLAVE` because SPISR's Slave_Mode_Select is a selection status, not the configuration, so `la_spi_check` now verifies SPICR (SPE set, master clear); (b) the IP never asserted `ip2intc_irpt` because `DGIER.GIE` was never set (now set in `la_spi_start`); (c) the SPI task's GIC line (IRQ 63) was never enabled - `la_irq_connect` only installs the port handler for later callers, so the task now sets the level/priority and calls `vPortEnableInterrupt`; (d) DRR_NOT_EMPTY is level-persistent per RX byte and storms the CPU, so the mask is TX-only and the reserved MOSI FIFO is left to overflow; (e) the TFO/RFO occupancy field is occupancy-1 with an 8-bit field, so the driver's 7-bit mask overfilled the bulk refill and silently dropped bytes (host tests updated); (f) the TX refill reads TFO once and pushes in bulk (a per-byte status read doubled the MMIO and starved the FIFO at 10 MHz); (g) TxTask bypasses the UART ACK window for SPI (the 8-chunk window emptied the FIFO at every window boundary) and paces on a new SPI-task ring-space semaphore instead of a 1 ms poll (which capped the producer below the 10 MHz link rate); (h) the data-ready line uses hysteresis (rise only with a full FIFO buffered, fall only when fully drained) after a first-frame-only assertion caused a one-byte underrun at the first frame boundary. ESP32: frame-sized reads (5-byte header, then a length-framed body) replace blind 256-byte bursts, and CS is driven manually with a 2 us setup delay (`cs_ena_pretrans` only applies to half-duplex); the gateway runs 10 MHz + `LA_SPI_READY_REQUIRED=y`. | Local gates PASS: `test-native` 15/15, `fw-zynq`, `fw-esp32`, `jtag-zynq`. Bench PASS: SPI 4096-sample capture bit-exact at 10 MHz (crc=0, counter pattern verified, no underruns) and the UART 64 KB control capture bit-exact. Bench OPEN: the SPI 64 KB capture still loses ~1 chunk to ~7 wire CRC errors per 66 KB; the byte counts match and the UART path is bit-exact, so the residual is 10 MHz signal integrity on the current wiring - the plan's SCK/MISO edge characterization and a wiring check come before 20 MHz and the 64 MB drain. Committed and pushed as `b73166d P9: bring up the SPI bulk link at 10 MHz`. |
| 2026-09-13 | P9.1 JA-1 data-ready root cause and PL fix. The bench symptom (Zynq `GPIO_DATA2=0x00000001`, OEN/DIRM set, while the ESP32 reads GPIO27 = 0) was not wiring: the P9.1 bitstream never drove JA-1. `fpga/scripts/build.tcl` connected `{ps7/GPIO_O[0]}` to `spi_ready`, but the BD pin is the scalar PS7 interface pin `ps7/GPIO_O`, and `try_net` only logs and continues on a missing pin, so `build/reports/bit.log` shows `CONNECT SKIP (missing pin) ps7/GPIO_O[0] -> spi_ready` and synthesis emitted `[Synth 8-3848] Net spi_ready ... does not have driver` (the port was tied off). The EMIO GPIO width override was also silently lost: `CONFIG.PCW_GPIO_EMIO_GPIO_WIDTH {1}` shared one `set_property -dict` with `CONFIG.PCW_GPIO_EMIO_GPIO_ENABLE {1}`, and the enable transition resets the width to its 64-bit default (reproduced with a live Vivado BD probe; the built XCI had width 64 and `GPIO_O[63:0]`). Fix: the width is set in its own `set_property` after the enable, and the data-ready connects `ps7/GPIO_O` (1-bit after the width fix). Rebuilt: `OK ps7/GPIO_O -> spi_ready`, the generated netlist drives `spi_ready` from `.GPIO_O(spi_ready)`, timing closed (WNS +0.247 ns, WHS +0.019 ns, 0 failing endpoints), DRC 0 errors, LUT 36.74 %, BRAM 61.67 %, 26 bonded IOB (N15 `spi_ready[0]` OUTPUT LVCMOS33 bank 35). The ESP32 GPIO27 input has a pull-down, so the old undriven line read 0 exactly as observed; the pin-numbering/reseat hypothesis is withdrawn. | Local gate PASS: `python tools/run.py bit` (`build/reports/bit.json`, 620.7 s; `build/release/{logic_analyzer.bit,logic_analyzer.xsa,timing_summary.rpt,drc.rpt,utilization.rpt}`). Bench OPEN: reload the bitstream/XSA and rebuild `fw-zynq` from the new XSA, then JA-1/GPIO27 level, SPI frame integrity, 10/20 MHz characterization and the 64 MB drain >=1.5 MB/s. Committed and pushed as `ca69d46 P9: drive the JA-1 data-ready from the EMIO GPIO output`. |
| 2026-09-13 | P9.1 bench session (PL + Zynq + ESP32 SPI). The PL SPI slave boots (`LA P9.1 SPI slave selftest: OK`), the Zynq service task runs (`LA SPI slave: OK, FIFO 256 B, ring 8192 B`), and the ESP32 SPI master starts at the configured clock. Findings: (a) the UART link died because the UART jumpers were removed during the SPI rewiring; both UART controllers were proven good (Zynq UART0 internal loopback 0x55/0x55; new ESP32 `UART2 internal loopback selftest: PASS`) and restoring the jumpers recovered the full burst suite (7 passed + 1 skipped) on the P9.1 firmware. (b) The JA-1 data-ready wire does not deliver: the Zynq GPIO output register is high (`GPIO_DATA2=0x00000001`, OEN/DIRM set) while the ESP32 reads GPIO27 = 0; the line needs rewiring. (c) With the ESP32 reading continuously (ready-optional fallback), SPI bytes flow (43.5 MB read) but frames corrupt (3 parsed / 71 CRC) because a fixed-size burst can underrun the 256-byte slave FIFO mid-frame; the intended data-ready handshake (or a length-framed read) is required before the 64 MB drain gate. Fixes kept from the session: the Zynq TX half-empty interrupt is enabled only while data is queued (it is level-persistent and stormed the CPU when always enabled), the SPI hand-off stream buffer is 16 KiB with a 5 ms LinkRx wake, the ESP32 SPI burst matches the FIFO (256 B), and Kconfig `LA_SPI_READY_REQUIRED`/`LA_LINK_UART_SELFTEST` bring-up options were added. | Local gates PASS: `fw-zynq`, `fw-esp32`. Bench PASS: P9.1 boot selftests, both UART internal loopbacks, UART burst suite 7+1 skipped on the P9.1 firmware. Bench OPEN: data-ready wiring, SPI frame integrity, 10/20 MHz, 64 MB drain. Committed and pushed as `bc5bdb0 P9: harden the SPI bring-up and diagnose the bench link`. |
| 2026-09-13 | P9.1 burst transport routing. Additive `LINK_CFG` (0x1A): `transport:u8` + reserved[3], accepted in IDLE and latched; `la_proto` pack/unpack, dispatch outcome/accessor, CmdTask ACK + `app->spi_bulk` latch; TxTask routes `DATA_BEGIN/CHUNK/END` through `la_spi_enqueue` when SPI is selected (brief retry on a full 8 KiB ring, counted drop after ~100 ms) while commands/ACKs stay on the UART. Host: `protocol.pack/unpack_link_cfg`, `AnalyzerClient.configure_link`, `la-cli link --spi/--uart`, mock ACK and tests; `protocol.md` row/section and `spi-link.md` routing note. | Local gates PASS: `test-native` 15/15, `test-host` 77/77, `fw-zynq` (`la_app.elf` text 184200 / data 3688 / bss 241312), `fw-esp32` (`la_gateway.bin` 0xd1d40), `test-esp32-mocks` 3/3, `release` 18/18, compileall, `git diff --check`. Bench gates remain (wiring, flash, 10/20 MHz, 64 MB drain). Committed and pushed as `05626cd P9: add the burst transport routing`. |
| 2026-09-13 | P9.1 ESP32 SPI master link. `la_board.h` pins/config (SPI3, sck=18, mosi=23, miso=19, cs=4, ready=27, `LA_SPI_CLOCK_HZ` Kconfig default 10 MHz, 1 KiB bursts, task core 1 prio 19); `la_link` now requires `esp_driver_spi` and `heap`. `la_link_tasks.c`: `relay_frame()` refactor shared by the UART and SPI parsers (separate parser + scratch per transport, single relay producer), new `LinkSpiTask` (DMA-capable burst buffer, `spi_bus_initialize` + `spi_device_transmit`, data-ready GPIO polling with a pull-down, bytes handed to LinkRxTask through a 4 KiB stream buffer), LinkRxTask drains the SPI stream buffer and tracks `spi_bytes`/`spi_frames`/`spi_crc_errors`; UiTask logs `spi=bytes/frames/crc`. `fw-esp32` PASS (`la_gateway.bin` 0xd1d40, 18% partition free). Zynq bulk routing and the bench gates remain. | Local gates PASS: `fw-esp32`, `test-native` 15/15, `test-esp32-mocks` 3/3, `test-host` 75/75, `release` 18/18, compileall, `git diff --check`. No bench run. Committed and pushed as `773b6d5 P9: add the ESP32 SPI master link`. |
| 2026-09-13 | P9.1 SPI frame-service core and slave task. New pure core `fw-zynq/app/src/live/la_spi_core.{h,c}` (whole-frame all-or-nothing ring with wrap compaction, `ready`/space/used, feed, queued/dropped/underrun counters; host test `tests/native/spi_core_host_tests.c`, CTest `la_native_spi_core`). New `fw-zynq/app/src/tasks/la_spi_task.c`: owns the `axi_quad_spi` device (`XPAR_XSPI_0_BASEADDR` 0x4080_0000), resets/starts the slave, connects the GIC 63 (encoded 31) notification-only ISR (latch + write-1-clear + task notify), enables TX-half-empty/DTR-underrun/DRR-overrun/DRR-not-empty interrupts, drains RX, feeds the 256-byte TX FIFO from the frame ring, and drives the JA-1 data-ready EMIO GPIO (bank 2 pin 54) high while data is queued; priority 7 (above AcqTask) so the FIFO is refilled before it drains. App wiring: `la_spi_service_start()` from `la_app_init`, `la_spi_enqueue()` producer API, `la_spi_snapshot()` counters, and a non-fatal `la_spi_check` boot report in `main.c`. `fw-zynq` built `la_app.elf` (text 183304 / data 3688 / bss 241312; `__la_fw_end` 0x0016B5F0 < `__la_nc_base` 0x3F000000). Bench (10/20 MHz, 64 MB drain) and TxTask bulk routing remain. | Local gates PASS: `test-native` 15/15, `test-host` 75/75, `release` 18/18, `fw-zynq`, compileall. No bench run. Committed and pushed as `3bc6ce3 P9: add the SPI frame service and slave task`. |
| 2026-09-13 | P9.1 platform integration verified. `fw-zynq` rebuilt the Vitis platform from the P9.1 XSA: the BSP now includes the AXI Quad SPI driver (`libspi.a`, `xspi_g.c`) and `la_spi_link.c` compiles into `la_app.elf` (text 176332 / data 3656 / bss 228784; `__la_fw_end` 0x001644E0 < `__la_nc_base` 0x3F000000). The ELF is not loaded: the bench still runs the P9.2 bitstream, and the SPI boot check/task wiring lands with the SpiLinkTask step so the ELF and bitstream are loaded together. | Local gate PASS: `fw-zynq` (`build/reports/fw-zynq.json`, `la_app.elf`). No bench run. |
| 2026-09-13 | P9.1 Zynq SPI slave driver core. New `fw-zynq/app/src/drivers/la_spi_link.{h,c}`: platform-neutral register access to the `axi_quad_spi` v3.2 slave (offsets/bits from the installed IP: SPICR 0x60, SPISR 0x64, DTR/DRR 0x68/0x6C, TFO/RFO 0x74/0x78, SRR 0x40, IPISR/IPIER 0x20/0x28), soft reset (`SRR=0x0A`), slave start (SPE=1, master clear, self-clearing FIFO resets with a bounded poll), byte push/pop with TX-full/RX-empty guards, occupancy, IRQ status/clear/enable and a slave-mode/error self-check. Host test `tests/native/spi_link_host_tests.c` models the IP registers (computed SPISR, FIFO order, W1C IPISR, SRR reset) and covers reset/start, TX full, RX order/empty, IRQ masking/clear, error and mode detection and stop; registered as CTest `la_native_spi`. The `SpiLinkTask` ring/frame service and the `fw-zynq` platform rebuild from the P9.1 XSA are the next P9.1 steps. | Local gate PASS: `test-native` 14/14 (`build/reports/test-native.json`); `git diff --check` clean. No firmware or bench run. Committed and pushed as `a6edd5d P9: add the Zynq SPI slave driver core`. |
| 2026-09-13 | P9.1 PL integration (first change set). New design contract `docs/spi-link.md`: JD-7..10 = SCLK/SS_N/MOSI (in)/MISO (out), JA-1 = data-ready (PS EMIO GPIO); PL `axi_quad_spi` v3.2 in slave mode (standard 8-bit, 256-deep FIFO, `ext_spi_clk`/`s_axi_aclk` 100 MHz), AXI-Lite at 0x4080_0000, IRQ into `xlconcat` In2; the slave streams encoded `la_proto` frames with CRC/underrun detection while the UART keeps commands/ACKs; 20 MHz budget ~2.2 MB/s net vs the 1.5 MB/s target. `fpga/scripts/build.tcl` adds the IP (SmartConnect M02, reset/clock, external ports, EMIO GPIO slice `GPIO_O[0]` -> `spi_ready`), `fpga/constraints/zybo_z7_10.xdc` adds U14/U15/V17/V18/N15, and `docs/wiring.md` records the physical map. Vivado 2026.1 rebuilt BD/bitstream/XSA: zero failing endpoints (WNS +0.182 ns, WHS +0.018 ns), DRC 0 errors, LUT 36.74 %, BRAM 61.67 %, 26 bonded IOB. Firmware (`la_spi_link` Zynq driver/task, ESP32 SPI master DMA) and the 10/20 MHz + 64 MB drain bench gates are the next P9.1 steps. | Local gate PASS: `python tools/run.py bit` (`build/reports/bit.json`, 787.9 s); reports under `build/release/`. No firmware or bench run. Committed and pushed as `5bd6550 P9: add the SPI slave link to the PL`. |
| 2026-09-13 | P9.4 gateway degradation root-caused and fixed. Bench diagnosis: after the P8.3 trace-dump flood the ESP32 stayed alive (`loops` climbing, heap ~86 KB, `net CONNECTED`) but `accept()` returned EAGAIN while the Zynq kept completing handshakes; the Zynq console showed its TX frozen while RX incremented on each ping. Root cause: LinkRxTask blocked forever in the `LA_RELAY_PRESSURE` path when the 64 KiB `link_to_net` ring was full (host stalled / Wi-Fi), so its UART RX buffer stayed full, RTS stayed low, and the Zynq TX path (PONGs, status) stalled - the "gateway dead" symptom. Fixes: (a) bounded pressure wait (2 s then a counted drop, `pressure_timeouts`); (b) LinkRxTask drains the UART driver buffer even when its event queue loses an event (`rx_rescue_bytes`); (c) the relay drives non-blocking accept/recv/send every 2 ms instead of relying on lwIP `select` (which stopped waking the listening socket after the flood), keeps a zero-timeout select probe (`sready`/`serr`), sets TCP_NODELAY, and rebuilds a stale listener after 3 s (`refresh`); (d) fast reconnects get a 250 ms grace wait plus a MSG_PEEK liveness check before the single-client rule rejects; (e) the host ignores stale NAKs whose inner type does not match the awaited request; (f) the P9.2 live comparison shares the module client (it used to open a second client against the single-client gateway); (g) `parse_trace(require_complete=False)` accepts the best-effort LOG dump and the P8.3 gate allows <=2% in-flight loss. Bench: the trace flood that killed the gateway three times now survives repeated runs, and `test-e2e --live` is 55 passed + 3 skipped (repeatable; one earlier run had a single isolated-byte latency spike from Wi-Fi jitter). | Local gates PASS: `fw-esp32`, `test-native` 13/13, `test-esp32-mocks` 3/3, `test-host` 75/75, `release` 16/16, compileall and `git diff --check` clean. Bench PASS: repeated trace-flood stress with pings after each run, full `test-e2e --live` green. Committed and pushed as `eaaee66 P9: fix the gateway link deadlock and reconnect race`. |
| 2026-09-12 | P8.6 documented skip and P9.2 compact events. (a) P8.6: measured decoder demand (<10% of one core at every gate; the 1 MHz SPI overload is the only saturation case) against the ~120 kB/s ESP32 relay and the ~200 kB/s link, so CPU1 offload would not move any gate; recorded the AMP design sketch (CPU1 release via 0xFFFFFFF0 + SEV, OCM SPSC rings, CPU0-owned pool with borrowed handles, cache/coherency rules) and the revisit conditions in `rtos-design.md`. (b) P9.2 format: `EVT_CFG` (0x19) selects v1 records or compact `EVT_BLOCK` (0x24); the block has a 12-byte header (version, slot, count, t_base) and per-record `t_delta:u16, kind:u8, len:u8, data`, with a u64 escape for deltas >= 0xFFFF. `la_event_batch` builds both encodings and `la_event_batch_last_sample` serves the P8.2 latency tracker for either. (c) Host: `iter_block_events`, `live(event_format=...)` (always sets the format because the firmware latch persists), `la-cli live --compact`, mock `live_compact` scenario, tests. (d) Measurements: burst raw-vs-transition sizes for all six workloads (step/UART from the P6.2 bit-exact models, SPI/I2C from the bench) and the compact live saving: v1 12.53 B/event vs compact 6.06 B/event = 51.6% (53.3% repeat) with identical decoded data; full-duplex SPI bandwidth recalculated (10 kHz gates keep headroom; compact raises the zero-drop ceiling to ~60-80 kHz). (e) Robustness: TxTask abandons a burst after 2 s without progress and AcqTask bounds the buffer wait at 5 s (a vanished host used to wedge both tasks forever); the bench also confirmed that a live session needs a patgen EN toggle after a previous LIVE_STOP. | Local gates PASS: `test-native` 13/13, `test-host` 73/73, `fw-zynq` PASS, compileall and `git diff --check` clean. Bench PASS (isolated): 18/18 size rows, two compact live runs (51.6%, 53.3%). Bench BLOCKED: full `test-e2e` 30 passed + 26 failed when the ESP32 stopped accepting connections (`WinError 10053/10054`; heap/tasks healthy; RTS reset recovers) - recorded as an open blocker. Committed and pushed as `bcfba82 P9: add the compact event encoding and close P8.6`. |
| 2026-09-12 | P8.3 task/queue tracing. (a) Pure `la_trace_core` (4096 x 16-byte overwriting ring, 24-entry object table, host tests) plus the `la_trace` firmware glue; FreeRTOSConfig.h hooks `traceTASK_SWITCHED_IN/OUT`, `traceQUEUE_SEND/RECEIVE` and `traceSTREAM_BUFFER_SEND/RECEIVE` (the `*_FAILED` hooks stay unwired to avoid idle-poll noise), and app markers cover PL/DMA IRQ entry, live segment publication, decoder feed and UART TX frames. Idle/timer TCBs register lazily with their kernel names. (b) Additive `TRACE_CTRL` (0x18) with START/STOP/DUMP; DUMP stops tracing and emits the ring as `LOG` lines (`LA TRACE`, `TASK`/`QUEUE`, `REC`, `END`). (c) Host: `protocol.pack_trace_ctrl`, `AnalyzerClient.trace_control/trace_dump`, `host/la_cli/trace.py` parser + timeline renderer (relative ms, optional zoom), `la-cli trace --start/--stop/--dump --out --png`, mock scenario and tests. (d) Bench: `tests/e2e/test_p83_trace.py` starts tracing, decodes 64 UART events, stops and dumps on one connection; the annotated 5 ms zoom `docs/images/p83-live-uart-trace.png` shows DMA IRQ -> acq SEG/queue -> dec0 DEC -> tx stream/TX. (e) Critical bug found on the bench: the first trace lock used the port mask pair, whose `vPortClearInterruptMask()` executes `cpsie i`; called from the SWI/IRQ context switch it re-enabled interrupts and a nested tick saved the handler context into the selected TCB (`pxTopOfStack` corruption, PC inside `vPortClearInterruptMask`, LR/SPSR holding ring addresses, data abort at 0x13B3C4). The fix saves/restores CPSR.I in `trace_irq_lock`; documented in `la_trace.c`. | Local gates PASS: `test-native` 13/13, `test-host` 69/69, `fw-zynq` (hooks verified in the ELF disassembly), compileall and `git diff --check`. Bench PASS: 18 consecutive P8.3 runs, three 3 s SPI-overload trace sessions with balanced IN/OUT, full `test-e2e` 37 + 2 skipped. Committed and pushed as `a13e9c6 P8: add the task/queue trace pipeline (P8.3)`. |
| 2026-09-12 | P8.5 cached-vs-non-cached decoder experiment. (a) `la_mem_geom` gained `la_mem_apply_range` (host-tested prefix/alignment/range cases) and the `LA_MEM_LIVE_CACHED` switch; `main.c` programs the first 4 MiB of the window `NORM_WB_CACHE` in the cached build and prints `LA live pool: cached (NORM_WB) + invalidate` (or `non-cached`) at boot. (b) Cache contract: `la_acq_task` invalidates each completed live segment (64-byte-rounded length) before any decoder reads it; `la_tx_task` invalidates the used capture range at burst job start. (c) Instrumentation: `decoder_words[]`/`decoder_busy_us[]` accumulated around `la_decoder_core_feed_segment`, printed as `LA status: dec words ... busy_us ...`. (d) Host: `AnalyzerClient.live()` gained `stop_on_overrun` and `collect_events` for a bounded full-window overload drain, with two host tests; new `tests/e2e/test_p85_cache.py` plus `--zynq-console`/`--p85-seconds` options measure the throughput from the console counters. (e) Firmware version bumped to `0x00080000`. Bench A/B (1 MHz SPI overload, 20 s, saturated decoder): non-cached 973,085 / 1,006,094 / 1,007,818 words/s with 4,620-4,787 dropped segments; cached 1,288,915 / 1,282,279 / 1,284,519 words/s with 3,326-3,337 dropped segments (~28% more throughput, ~30% fewer drops). The cached pool is the tracked default; the full `test-e2e` suite passes 36 + 2 skipped with both burst matrices and the 60 s zero-drop gate, proving the invalidation contract. Results documented in `docs/rtos-design.md` (P8.5 results). | Local gates PASS: `gen --check`, `test-native` 12/12, `test-host` 61/61, `release` 15/15, `test-esp32-mocks` 3/3, `fw-zynq` for both variants, compileall and `git diff --check`. Bench PASS: banner-captured JTAG reloads, A/B throughput, full suite. Committed and pushed as `2b899ef P8: cache the live pool and measure decoder throughput`. |
| 2026-09-11 | P7.6 isolated-byte latency: patgen single-shot mode, Zynq-side latency counter and the ESP32 select-timeout fix. (a) New patgen mode 6 `UART_SINGLE` (`docs/regmap.yaml` + `gen`; one `LA-TEST\n` frame per enable edge, then idle high) with a `tb_la_patgen` scenario; `la_proto` and host mode validation extended to 6. (b) Zynq P8.2 instrumentation: `live_arm_time`/`live_sample_div`/`live_max_latency_us`; TxTask walks each event block to its newest record and records the worst byte-to-send latency; StatusTask prints `live_max_us`. (c) The host live client gained `on_started` and per-event `arrival` timestamps. (d) The ESP32 `LA_NET_SELECT_TIMEOUT_US` dropped 100 ms -> 5 ms: the `link_to_net` ring is not a select() descriptor, so sparse replies waited a full timeout. Bench: the PATGEN_CFG round trip fell from 93-125 ms to ~15 ms; 20 single-shot isolated-byte sessions measured `live_max_us=428` against the 25 ms budget; the new `test_live_uart_isolated_byte_latency` passes and the full suite is 35 passed + 2 skipped. | Local gates PASS: `gen --check`, `lint`/`sim` 12/12, `test-native` 12/12, `test-host` 59/59, `bit` (timing clean), `fw-zynq`, `fw-esp32`, compileall and `git diff --check`. Bench PASS: isolated-byte 428 us max, full suite. Committed and pushed as `3a491cf P7: add the single-shot patgen mode and latency instrumentation`. |
| 2026-09-11 | P8.4 WDT fault injection and the 500-capture soak. (a) `CONFIG_LA_LINK_WDT_TEST_HOOK` (default n) added to the gateway Kconfig and `la_link_tasks.c`: when enabled, LinkRxTask hangs at start instead of servicing the watchdog. Built and flashed the hook image: the console showed `E la_link: P8.4 WDT test hook active` then `task_wdt: Task watchdog got triggered - LinkRx (CPU 1)`, `Aborting.` and `Rebooting... rst:0xc (SW_CPU_RESET)`. `CONFIG_ESP_TASK_WDT_PANIC=y` is now the tracked default so a hung watched task resets the gateway; the normal image was rebuilt, reflashed and the live suite passes 9/9. (b) `--soak-count` option plus `test_soak_back_to_back` in `test_zynq_burst.py`: 500 back-to-back 4096-sample counter captures passed in 232 s, every capture exact and parser-clean, FIFO overflow clear, IDLE at the end. | Local gates PASS: `fw-esp32` PASS (both images), `test-host` 59/59, compileall and `git diff --check` clean. Bench PASS: WDT reset observed, normal image restored, 500-capture soak. Committed and pushed as `P8: verify the watchdog reset and add the 500-capture soak`. |
| 2026-09-11 | P8.4 garbage-flood and SPI-overload fault injection, plus the P8.1 starvation fix they exposed. (a) `AnalyzerClient.raw_send()` hook; `test_live_garbage_flood_resync` sends 2 KiB of garbage, a filler and a bad-CRC frame, then requires PING/STATUS to be answered. (b) `test_live_spi_overload_reports_loss` drives the patgen SPI at 1 MHz (~230 k events/s) and requires reported loss, no crash, IDLE reset and recovery to a clean 10 kHz decode. (c) The overload initially produced zero host frames while the decoder counters climbed: TxTask at priority 4 never ran because AcqTask kept `seg_q` non-empty, and its `ev_q` drain was unbounded. Fixes: `LA_PRIO_TX` 4 -> 5 (equal round-robin with the decoders/CmdTask), the event drain bounded by TX StreamBuffer space (block returned to `ev_q` when there is no room), and the decoder segment drain bounded to 4 segments per turn. Bench: the host now receives events and stage-1 overrun reports; full `test-e2e` 34 passed + 1 skipped. | Local gates PASS: `test-host` 59/59, `fw-zynq` PASS, compileall and `git diff --check` clean. Bench PASS: overload, garbage and full suite. Committed and pushed as `20051b2 P8: bound TxTask drains and fix overload starvation`. |
| 2026-09-11 | P8.4 TCP-kill/stale-client fault injection plus a host overrun-collection fix. (a) `Transport.kill()` (TCP RST via `SO_LINGER 0`) and `AnalyzerClient.kill()` added as documented P8.4 hooks with a host test for the killed-transport path. (b) New `test_live_tcp_kill_reconnect`: starts a live flood, RSTs the client mid-stream, and requires the gateway to serve a fresh connection; 4/4 passes. The stale-client states seen earlier were heap-exhaustion symptoms (fixed by the 16 KiB-window revert), not an independent bug. (c) Fixed a real client race: an `EVT_OVERRUN` arriving in the same read as the count-stop event was discarded, which made `test_live_command_text` flaky; the client now always collects overruns and trims only the decoded overshoot (8 consecutive live-test runs green). | Local gates PASS: `test-host` 58/58, compileall and `git diff --check` clean. Bench PASS: full live suite 7/7, kill/reconnect 4/4. Committed and pushed as `69ece1b P8: add TCP-kill recovery test and fix live overrun collection`. |
| 2026-09-11 | P8.2 heap root cause and full-suite closure. The 32 KiB lwIP-window variant of the tuning exhausted the ESP32 heap during live streaming: the UI log showed `heap=3552` and `drop=30149` with `rep` frozen (stage=2 reports could not enqueue), the TCP path stalled and the gateway stayed in the stale-client state (`net CLIENT`), which also explains the earlier combined-suite drops. Reverted `CONFIG_LWIP_TCP_SND_BUF_DEFAULT`/`TCP_WND_DEFAULT` to 16384 while keeping the 64 KiB static `link_to_net` ring; rebuilt (`la_gateway.bin` 0xcad30) and flashed. Bench: full `test-e2e --live --live-seconds 60` is 31 passed + 1 skipped in 119 s with a background console watcher; minimum heap 65,508 B, `fifo_ovf=0`, `line_err=0`, and the only drops (721 frames / 11 stage=2 reports) came from the deliberate 3 s stall fault injection. Results recorded in `rtos-design.md`. | Local gates PASS: `fw-esp32` PASS. Bench PASS: full suite green including the 60 s zero-drop gate, the SPI/I2C gates and the stall injection. Committed and pushed as `a3e746b P8: keep lwIP windows at 16 KiB (heap fix)`. |
| 2026-09-11 | P8.2 relay buffering tuning and first measurements. (a) `LA_LINK_TO_NET_BYTES` 32 -> 64 KiB; `CONFIG_LWIP_TCP_SND_BUF_DEFAULT`/`TCP_WND_DEFAULT` 16 -> 32 KiB in `sdkconfig.defaults` and the ignored sdkconfig; new `la_link_counters_t.to_net_max_used` (peak occupancy tracked in LinkRxTask) and `overrun_reports`; UiTask logs `touse`/`rep`. (b) Rebuilt and flashed the gateway (manual BOOT/EN, hash verified): the boot log confirms `to_net 65536 B`, `tcp tx/rx win: 32768`, `got ip 192.168.100.37`, `listening on :5555`, `heap=86736`. (c) Bench: the isolated 60 s zero-drop gate passes 3/3 with `drop=0` and peak `touse=40144` (the old 32 KiB ring dropped at this workload); the full `test-e2e` suite (burst matrix first) hit the 64 KiB ceiling once (`touse=65536`, `drop=785`, `rep=24`) and stopped on 12 `EVT_OVERRUN(stage=2)` reports; the following isolated runs added zero drops (`drop` unchanged while `fwd` rose ~60 k frames). Results recorded in `rtos-design.md`. | Local gates PASS: `fw-esp32` PASS (`la_gateway.bin` 0xcad40), compileall and `git diff --check` clean. Bench PASS (isolated): 60 s gate 3/3. Bench FAIL (reported): combined suite over the 64 KiB stall. Committed and pushed as `d89fdb2 P8: tune the gateway relay buffers (64 KiB ring, 32 KiB lwIP)`. |
| 2026-09-11 | P8.4 link-overrun reporting (silent relay loss made visible). (a) `la_relay` gained `pending_drops` accounting (`la_relay_pending_drops`/`la_relay_clear_pending_drops`, also cleared by `la_relay_reset_counters`) so every best-effort drop is queued for a report. (b) LinkRxTask builds one `EVT_OVERRUN(stage=2 link)` per accumulated window with `la_proto_pack_evt_overrun` and enqueues it best-effort: if the ring is still full the count stays pending and is retried after the next forwarded frame; the report clears only after a successful put. (c) The host live client gained the documented `read_stall_s` P8.4 fault-injection hook; the mock gained a `live_link_overrun` scenario and the E2E suite the deterministic 3 s read-stall test. (d) Bench: `fw-esp32` built (`la_gateway.bin` 0xcace0), flashed (manual BOOT/EN, hash verified), `ping` OK; the stall test passes (stage=2, dropped>=1), the live suite is 6/6, and the isolated 60 s gate now fails with `EVT_OVERRUN(stage=2, dropped=33)` instead of a corrupted stream. Preventing the drops remains P8.2. | Local gates PASS: `test-esp32-mocks` 3/3, `test-host` 57/57, `fw-esp32` PASS, compileall and `git diff --check` clean. Bench PASS: stall fault injection, live suite 6/6; the 60 s gate fails honestly via the new report. Committed and pushed as `e40ba19 P8: report gateway link overruns (EVT_OVERRUN stage 2)`. |
| 2026-09-11 | P7.6 host SPI/I2C live decoder support and the 10 kHz live gates. (a) `host/la_cli/protocol.py` gained `pack_decoder_cfg_spi` (signed sck/mosi/miso/cs plus cpol/cpha/word/lsb/cs_active) and `pack_decoder_cfg_i2c` (scl/sda), matching the protocol.md 20-byte DECODER_CFG union and the C validation. (b) `la-cli live --decoder` now parses `spi:sck=...` and `i2c:scl=...` specs (UART unchanged); the mock records the DECODER_CFG payload; host tests 50 -> 55. (c) `tests/e2e/test_e2e_live.py` gained the 10 kHz SPI and I2C live gates over the JE->JC loopback: SPI (SCK=CH0, MOSI=CH1, CS_N=CH2) checks 0x4C/0x41 MOSI words and CS assert/release after skipping the initial partial frame; I2C (SCL=CH3, SDA=CH4) checks START/0x50/ACK/0x4C/ACK/0x41/ACK/STOP with no NACK; both assert `dropped_segments=0` and a conservative >=20% link headroom. (d) Bench: `test-e2e --live -k test_live` 5/5 in 18.6 s. The full 60 s suite then failed three times: the ESP32 relay dropped 4-19% of EVT_DECODED frames (drop counters +4105/+5763/+16926, RSSI -67) while the Zynq stayed clean (`dropped_segments=0`, `fifo_ovf=0`, `line_err=0`), so the 60 s zero-drop gate is not repeatable and P8.2/P8.4 must harden the relay. | Local gates PASS: `test-host` 55/55, compileall and `git diff --check` clean. Bench PASS: SPI/I2C live gates 2/2 isolated and the live suite 5/5. Bench FAIL (flaky, recorded): 60 s zero-drop gate with relay drops. Committed and pushed as `a0650b6 P7: add host SPI/I2C live decoder configs and 10 kHz gates`. |
| 2026-09-11 | Moved the analyzer link to 2 Mbps and closed the P7.6 60 s zero-drop gate. (a) Both sides now default to 2 Mbps: `LA_LINK_BAUD 2000000` in `la_app.h` (used by `la_app.c` and the boot banner in `main.c`; the PS UART0 reference clock is 100 MHz so the divisor is exactly 50), `CONFIG_LA_LINK_BAUD` default 2000000 in `main/Kconfig.projbuild`, and the `la_board.h` fallback. (b) Rebuilt both firmwares, flashed the gateway (manual BOOT/EN download mode, esptool `--before no_reset --after hard_reset`, hash verified) and reloaded the Zynq via `jtag-zynq`; `la-cli ping` over the 2 Mbps link returned `fw 0x00070000 pl 0x4C410002`. (c) Root-caused the live FRAME_ERR failure: `_configure_patgen_uart` re-enabled the patgen without a disable, and `la_patgen` latches mode/DIV only on the EN rising edge, so a preceding burst test's mode stayed latched; a probe reproduced exactly one FRAME_ERR at t=8246 in the stale state and a clean 64/64 BYTE decode after a disable-enable. The live helper now toggles EN. (d) Bench: `test-e2e --live --live-seconds 60` passes 28 + 1 skipped twice in a row (P6.2/P6.3 matrices 18/18, Zynq burst 7/7, live 3/3); the 60 s gate decoded the exact repeating `LA-TEST\n` stream with `dropped_segments=0`, no EVT_OVERRUN and zero new relay drops (`drop` stayed at 246 from the earlier stale-mode run, `fifo_ovf=0`). | Local gates PASS: `test-native` 12/12, `test-esp32-mocks` 3/3, `test-host` 50/50, `fw-zynq` PASS (`la_app.elf` text 165548), `fw-esp32` PASS (`la_gateway.bin` 0xcabe0), `gen` up to date, compileall and `git diff --check` clean. Bench PASS: 2 Mbps link, flash, JTAG reload, 60 s zero-drop gate. Committed and pushed as `a388028 P7: move the analyzer link to 2 Mbps and fix the live patgen latch`; the toolchain lock refresh followed as `e8caf6a`. |
| 2026-09-11 | P7 hardware-independent cores (first change set, pushed as `b045366`): promoted `segment_pool` from `fw-zynq/tests/host` to `fw-zynq/app/src/live/la_segment_pool` with a FreeRTOS critical-section lock branch; new `la_live_ring` (P7.2: 16 x 256 KiB live slot/BD bookkeeping over the pool, zero-consumer immediate recycle, per-decoder reference release, pool-exhaustion detection), `la_decoder_core` (P7.3: decoder config publication, txn-word feeding into `common/la_decode` with run-time advance, sequence-gap discontinuity reset, event counters), `la_event_batch` (P7.4: EVT_DECODED record builder with 1024-byte payload cap and drop-newest accounting); `la_proto` EVT_OVERRUN helper; `la_acq_fsm` stream/live lifecycle and `la_cmd_dispatch` DECODER_CFG/LIVE_START/LIVE_STOP acceptance with CmdTask ACK/forward wiring; host tests for every core. | Local gates PASS: `test-native` 12/12, `test-host` 49/49, `fw-zynq` PASS, compileall and `git diff --check` clean. Committed and pushed as `b045366 P7: add live decode pipeline cores (pool, ring, decoder, batch)`. |
| 2026-09-11 | P7 live bench session (continued after the trigger fix). Loaded the P7 ELF (`fw 0x00070000`) and ran the live gates. Fixed three firmware defects found on the bench: (a) AcqTask stopped the stream the moment the DMA ring had no posted descriptor, before consumers could release; the eager stop is removed and the PL FIFO overflow -> DONE path now carries the accurate overflow event; (b) `la_decoder_task` released the pool directly, so the ring's slot state/`published`/`dma_posted` never updated and the ring leaked after one cycle - releases now go through `la_live_ring_release` under a critical section shared with AcqTask's complete/repost/abort; (c) TxTask reported decoder drops only after draining `ev_q`, which never empties under overload, so EVT_OVERRUN arrived after teardown - reports are now queued before the drain. Host fixes: the live deadline is a normal stop (`_pump(deadline_ok=True)`), frames that arrive while waiting for the LIVE_STOP ACK are collected (count trimmed), `--live-baud` scales the patgen DIV for link-headroom runs, and the E2E tests align to the first frame after the initial partial one. Bench: `.venv\\Scripts\\python.exe tools\\run.py test-e2e -- --target tcp://192.168.100.37:5555 --live --live-baud 9600` 3/3 including a 10 s zero-drop continuous run; at `--live-baud 115200` the Zynq decodes with `dropped_segments=0` but reports `EVT_OVERRUN(stage=1, dropped=25328)` because the 921600 Zynq->ESP32 link cannot carry ~122 kB/s of EVT records (the gateway is at 921600, not the 2 Mbps the gate assumes, and the P5.5 relay ceiling applies regardless); the full 8-channel P6.3 external matrix (`--external-loopback`, default mask 0xFF) passes 18/18. P8.4 stale-client observed twice more: after a client disconnect during a live flood the ESP32 stops accepting TCP until reset. | Local gates PASS: `test-native` 12/12, `test-host` 50/50, `fw-zynq` PASS, `lint`/`sim` 12/12 each (trigger fix), `bit` PASS, compileall and `git diff --check` clean. Bench PASS: live 3/3 at 9600 with zero drops, honest EVT_OVERRUN at 115200, P6.3 18/18. Committed and pushed as `b985e99 P7: fix live pipeline on bench (ring release, repost, overrun reporting)`. |
| 2026-09-11 | WIP bench session (paused mid-verification): loaded the P7 ELF (fw version bumped to `0x00070000` for identification) over JTAG and confirmed `ping`/`status` over the gateway. The first live attempts hung the firmware: console status ticks stopped, `ping` timed out. XSDB on the hung CPU showed continuous IRQ handling (`FreeRTOS_IRQ_Handler`, `xTaskGenericNotifyFromISR`, `la_irq_stats_record`) with GIC IRQ 61 active/pending and `LA IRQ_STAT = 0x02` (TRIGGERED) never clearing. Root cause: in stream mode `la_capture_ctrl` fires `ev_triggered` on every cycle while `trig_i` is high, and `la_trigger` makes the trigger trivially true when `trig_mask`/edges are zero (the `la-cli live` default), so `IRQ_STAT.TRIGGERED` re-armed every cycle and the PL IRQ line never deasserted (also blocking the live flush cadence). Fix: `trig_seen_q` one-shot latch (cleared on reset/arm) so the optional live trigger fires once per arm; new `tb_la_capture_ctrl` scenario 9 holds `trig_i` high and asserts exactly one pulse plus a re-arm pulse. Local gates PASS: `lint` 12/12, `sim` 12/12, `bit` (new bitstream/XSA in `build/release`), `git diff --check`. Not yet done: `fw-zynq` rebuild from the new XSA, `jtag-zynq` reload, live gate rerun. | WIP: bench verification pending; the board still runs the old bitstream with the hung firmware and needs the next session's reload. Committed and pushed as `c299780 WIP: fix live stream trigger IRQ storm (RTL one-shot)`. |
| 2026-09-11 | P7 task adapters (second change set): `la_dma` live ring calls (`la_dma_prepare_live`/`collect_live`/`post_live`/`free_bd_count`, BD ID = segment slot, no cyclic mode); AcqTask live lifecycle (LIVE_START applies the STREAM CONFIG, starts the 16-segment pool ring and BD ring, publishes decoder configs, arms the PL; completion IRQs publish segments and push them to each decoder `seg_q` with per-decoder reference release on queue-full; repost loop on completion and on decoder release; pool exhaustion and PL FIFO overflow stop the stream with `EVT_OVERRUN(stage=0)`; LIVE_STOP/ABORT quiesce, drain consumers and abort the ring); `la_decoder_task` (cfg_q/seg_q drain, `la_decoder_core` feed, static event blocks via `block_q`/`ev_q`, 2 ms batch flush, drop-newest accounting); TxTask drains `ev_q` into `EVT_DECODED` and reports decoder drops as `EVT_OVERRUN(stage=1)`; STATUS carries the per-decoder events/dropped_segments counters; `la_app` creates the pool, ring, block/ev/seg/cfg queues and three decoder tasks. Host: `AnalyzerClient.live(config=...)` sends the STREAM CONFIG, `la-cli live --patgen ...` configures the pattern generator, and `tests/e2e/test_e2e_live.py` adds the `--live` bench harness (UART decode, `--live-seconds` zero-drop stream, decoder-free raw smoke). | Local gates PASS: `test-native` 12/12 (`la_native_live` included), `test-host` 50/50, `fw-zynq` PASS (la_app.elf text 165556 / data 3656 / bss 162544; the app imports `common/la_decode` and `app/src/live`), compileall clean, `git diff --check` clean. Bench BLOCKED/unverified: no live hardware run; the P7.2/P7.6 gates need the 2 Mbps gateway flash and the JE->JC jumpers. Committed and pushed as `e03b9db P7: wire live decode tasks, SG DMA ring and live E2E harness`. |
| 2026-09-11 | Completed P6. (a) P6.2/P6.3: added `tests/e2e/test_e2e_burst.py` and the `--external-loopback`/`--channel-mask` options; the matrix covers counter, walking-1, PRBS8, UART, SPI and I2C at 1M/10M/100M with three consecutive captures each, using bit-exact waveform models (the RTL UART gap is 11 bit periods, not the commented 10; the patgen DIV is scaled per rate so protocol waveforms stay well sampled) and the tested native SPI/I2C decoders via `build/native/la_native.dll`. Bench: P6.2 18/18 internal and the full `test-e2e` 25 passed + 1 skipped; P6.3 18/18 external over six JE->JC jumpers with channels 6-7 masked. (b) P6.4: the `.sr` writer now emits `capturefile=logic-1`, `total probes=8` and `probe1..8` (PulseView rejected the old metadata with "No channels enabled"); captured a 4 ms UART frame and screenshotted PulseView into `docs/images/p6-pulseview-uart.png`. Docs: `wiring.md`/`windows-dev.md` record the external-loopback flow and channel mask. | Local gates PASS: `test-host` 49/49, `compileall -q tools host tests`, `git diff --check`. Bench PASS: P6.2/P6.3 matrices, P6.4 screenshot. Committed and pushed as `a1c880a P6: complete host tool, burst E2E, external loopback and PulseView export`; annotated tag `v0.1-burst` pushed. Next: P7 live decode pipeline. |
| 2026-09-11 | Made the ad-hoc Wi-Fi sdkconfig helper durable: added tracked `tools/set_wifi.py` (project-root derived, optional `--sdkconfig`, prompts for SSID, hides the password via `getpass`, writes only the two `CONFIG_LA_WIFI_*` lines in the git-ignored `build/esp32/sdkconfig`) and `host/tests/test_set_wifi.py` (update existing lines, append missing lines, Kconfig escaping, file patch round trip). The test caught a real bug in the draft: `re.sub` interpreted `\"`/`\\` in the replacement template and corrupted passwords containing quotes or backslashes; the replacement is now a lambda. Docs: `windows-dev.md` documents the helper and a "Changing Wi-Fi networks" runbook (PC, gateway, Zynq and readiness check, including the manual `--before no_reset` flash path); the status blocker and checklist name it. | Local gates PASS: `test-host` 49/49 (`build/reports/test-host.json`), `compileall -q tools host tests` clean, `git diff --check` clean, `tools/set_wifi.py --help` works. No firmware rebuild, flash or hardware run. Committed and pushed as `e7a028b tools: add durable Wi-Fi sdkconfig helper and network runbook`; the Temp copies under `%TEMP%\opencode` are superseded. |
| 2026-09-11 | Completed `docs/web-artifact-implementation-instructions.md`, a deterministic implementation contract for expanding the explainer into three levels: a seven-node whole-system diagram, seven subsystem routes, 71 component-detail topics, eight cross-cutting reference areas, typed content/evidence schemas, source-coverage validation, responsive/accessibility rules, 13 ordered work packets and a pasteable lower-cost-model prompt. Preserved `docs/web-artifact-content-review.md` as the historical correction record and clarified that its W01-W10 pass is already represented by `e7828c7`. | Documentation consistency checks PASS: register count 21 from `docs/regmap.yaml`; seven required subsystem sections; 13 work packets; 25 final acceptance checks; `git diff --check` clean. Reviewed the current architecture, protocol, register, RTOS, decoder, wiring, verification, source-tree and website structure. No website source, dependencies, runtime, deployment, firmware, RTL or hardware were changed or tested. No publication requested or performed. Specification committed and pushed as `4b137d8 docs: specify three-level web explainer`. Next: give the implementation model the master prompt in the new specification; publication remains a separate request. |
| 2026-09-11 | Implemented W01-W09 from `docs/web-artifact-content-review.md` in `web-artifact/app/page.tsx`, `web-artifact/app/layout.tsx` and `web-artifact/app/globals.css`: corrected the intro, 32,768 pre-trigger limit, per-stage evidence boundaries, burst/live availability, planned ownership example, calculated/measured rates, current proof cards, milestone/open-work copy, pinned source links and readable responsive labels. | `pnpm build` from `web-artifact/` PASS (Vinext production build completed; no report file is emitted). `git diff --check` PASS. No hardware/native reruns because this is a prose/layout-only change; no browser QA requested or performed. No publication requested; existing Site identity/deployment remains unchanged. Next: review the rendered page in a browser when visual QA is authorized, then publish only under a separate publication request. |
| 2026-09-11 | Completed content review and lower-cost-model handoff in `docs/web-artifact-content-review.md`: 13 prioritized findings, evidence boundaries, exact replacement copy, 10 ordered tasks, dependencies, acceptance checks and a pasteable implementation prompt. Compared the published explainer against repository snapshot `4d4434f046068c74da552d257b1bd821ef993d62`. | Review deliverable complete; local documentation is WIP/uncommitted because this turn requested review and planning only. Website source and deployment unchanged. Checks: read-only Sites version lookup (version 1/source `905dc566b9cbb87dfefe12c1e517effbfe751687`), `git ls-tree` comparison (matching page blob `46203ef7107a926e99d9d51d7a094923cdc4e6e8`), source/ledger/register/firmware/test inspection, `git diff --check` PASS. No browser or hardware tests rerun and no new benchmark claimed. Next action: a subsequent implementation task may execute W01-W10 and follow the normal commit/push workflow; publishing requires a publication request. |
| 2026-09-10 | Closed the remaining P4 and P5 bench gates. (a) P4.6: added `tests/e2e/` with the `test-e2e` runner target (LA_E2E_TARGET/`--target`) and `test_zynq_burst.py`; the suite passes 7/7 (consecutive runs, four-BD chain, short raw/txn tails, abort while armed, abort mid-capture, capacity-overflow NAK). The abort tests exposed a real PRE=0 bug: `la_pretrig_ring` derived stream mode from `pre_req == 0`, so a PRE=0 burst emitted uncounted samples during ARMED and flooded the FIFO; added an explicit `stream_en` input (from the controller's stream mode) and a `burst-zero-pre` ring test. `la_acq_task` now soft-resets the PL datapath on abort so partial data cannot leak into the next descriptor chain; `AnalyzerClient.arm()` and its mock test were added. (b) P5.2: added the standalone `fw-esp32/tests` Unity app (relay, static queue/notification, NOSPLIT ring, UART2 internal loopback, task WDT) and the `test-esp32-target` runner target + `run_target_tests.py` capture script; 5/5 PASS on target. Moved `la_link_uart.h` to the component's public include dir. (c) P5.5: added `test_esp32_loopback.py` (raw-socket frame echo with a 256-frame window, console `fifo_ovf` watch) and the `LA_LINK_BAUD` Kconfig option; 10 MB loopback passes at 921600 (44.4 kB/s echo) and 2 Mbps (39.5 kB/s echo), both with zero CRC errors and `fifo_ovf=0`; the near-identical rates show the relay processing is the ceiling, not the baud. Docs: `wiring.md` records the loopback jumpers and the target-test flow. Bench friction recorded: the DOIT board's auto-reset does not assert GPIO0, so flashes after the first required manual download mode plus esptool `--before no_reset`; opening COM5 with pyserial's default line states resets the board, so the console watch opens with DTR/RTS held low. | Local gates PASS: `lint`+`sim` 12/12 each, `bit`, `fw-zynq`, `jtag-zynq`, `test-native` 11/11, `test-host` 45/45, `test-esp32-mocks` 3/3, `release` 14/14, `fw-esp32`, `test-esp32-target` build. Bench PASS: P4.6 suite 7/7, `test-esp32-target` runner PASS 5/5 (clean report after the `--no-reset` flash), P5.5 10 MB at both bauds. The loopback test is gated behind `--loopback` so the default `test-e2e` run stays safe with the Zynq connected. Final state: 921600 gateway restored, JD ribbon connected, loopback jumpers removed, `test-e2e` 7 passed + 1 skipped. Open: P6 E2E matrix and external loopback, PulseView screenshot/tag, P8.4 stale-client recovery (observed again: a host timeout leaves the gateway discarding the next session until an ESP32 reset), relay throughput ceiling for P8. Change set committed and pushed as `565ed01 P4: close burst bench gates (PRE=0 stream_en, abort reset, E2E suite)` and `64bf873 P5: add ESP32 target Unity tests and loopback gate`; `origin/main` verified against `git ls-remote`. |
| 2026-09-10 | Root-caused and fixed the P4 acquisition bugs (10M raw shortfall, txn garbage). (a) JTAG probe of the loaded design showed the PL word count complete while the S2MM BD never completed, the buffer untouched and no DMA error: the AXI DMA had `C_SG_INCLUDE_STSCNTRL_STRM=1` (Vivado default) with `S_AXIS_S2MM_STS` tied off in the BD, so the SG engine stalled every transfer. Fixed in `fpga/scripts/build.tcl` (`C_SG_INCLUDE_STSCNTRL_STRM {0}`) and rebuilt. (b) The raw packer's word-aligned flush deferred tlast forever, and the deferred `last_pending_q` leaked into the next capture, whose first word then closed the DMA packet after one word (single-BD chain halts, FIFO fills at ~2048 words). Fixed: `la_capture_ctrl` latches `pkt_words_o = ceil((K+POST)/4)` at the trigger, `la_raw_packer` tags the counted final word with tlast (aligned bursts close on the last word; a flush with nothing buffered emits nothing in burst mode) and drops any live-mode deferred tlast at a burst start. (c) `la_pretrig_ring` gated `out_valid` with `run_d` so a leftover presented sample cannot leak into the first run cycle. (d) Firmware: `la_cmd_task` now ACKs PATGEN_CFG before latching the register, and `la_acq_task` writes ARM together with the sticky SELFTEST/STREAM bits (a plain ARM write cleared SELFTEST_EN). Tests: `tb_la_raw_packer` gained burst-aligned, burst-partial and live-to-burst scenarios; `tb_la_top` scenario B asserts exactly 75 words, one tlast on the last word and exactly 300 samples. Bench: rebuilt bitstream (timing clean) and ELF, reloaded over JTAG, then four exact captures through the gateway with the internal counter patgen (host CRC/format clean): raw 4096 @10M = 1024 words, raw 16000 @10M = 4000 words (counter steps every 10 samples), raw 16000 @1M = 4000 words (every sample), txn 16000 @10M = 1601 words reconstructing exactly 16000 samples. | Local gates PASS: `test-native` 11/11, `test-host` 44/44, `test-esp32-mocks` 3/3, `release` 14/14, `lint`+`sim` 12/12 each, `bit` (zero failing endpoints, WNS +0.377 / WHS +0.024), `fw-zynq`, `jtag-zynq`. Bench PASS: exact acquisition through the ESP32 gateway with zero CRC/format errors, plus a 16-BD `raw 1000000 @1M` capture (250000 words, 1M strictly consecutive samples). Change set committed and pushed as `7e27e96 P5: add ESP32 gateway adapters and bench bring-up fixes` and `7180cf5 P4: fix burst acquisition chain (DMA status stream, tlast close, ring run edge)`; `origin/main` verified against `git ls-remote`. |
| 2026-09-10 | Bench session: P5 gateway end-to-end plus two P4 firmware fixes. (a) ESP32 flashed with the Wi-Fi build and associated; `la-cli ping/status` and five burst captures ran over TCP through the gateway to the Zynq and back with host CRC verification (`build/captures/p5-e2e-*.sr`); ESP32 relay counters during a capture were `fwd=12 drop=0 press=0 fifo_ovf=0 line_err=0`, `clients=1/0 rx_frames=7 tx_frames=10`. (b) Root-caused the Zynq UART0 storm with XSDB: IRQ 59 stayed pending with `ISR=0x16F`, the CPU spun in `FreeRTOS_IRQ_Handler`, and the GIC vector table entry for 59 was `StubHandler`/`XScuGicInstance`. Cause: the SDT BSP's `XConnectToInterruptCntrl()` skips every connection until `XSetupInterruptSystem()` sets `ScuGicInitialized`, which the FreeRTOS CA9 port never does, so `xPortInstallInterruptHandler()` silently connected nothing. Fix: new `la_irq_connect()` sends the first connection through `XSetupInterruptSystem()`; `la_irq.c` and `la_link_uart.c` use it. (c) A capture then hit `vApplicationStackOverflowHook` because ISRs run on the interrupted task's stack; raised `la_app.h` stacks to 4/4/4/2 KiB and set idle/timer to 2 KiB in the `fw-zynq/FreeRTOSConfig.h` policy fragment. Verified clean frames in the Zynq RX FIFO over JTAG (`A5 01 ...` PING, `A5 12 ...` ABORT, `A5 16 ...` LIVE_STOP), COM6 status ticks, and the successful TCP captures. Docs: `rtos-design.md` documents the dispatch workaround and stack rationale. | Bench PASS: association, TCP relay, ping/status, five captures with CRC verification, no storm, no overflow. Open: 10M raw captures returned 2053/4096 words and the txn export reconstructed a bogus 16777215-sample run (P4 acquisition/data bugs); the gateway kept a stale client after a host timeout until an ESP32 reset (P8.4). Local gates: `fw-zynq` PASS (text 139276); `test-native` and `test-esp32-mocks` rerun clean. Change set uncommitted WIP pending the user's commit/push instruction. |
| 2026-09-10 | Flashed the P5 gateway and fixed a bench artifact. `idf.py -p COM5 flash` succeeded on retry (this DOIT board's auto-reset is flaky: an attempt reported ROM boot mode `0x13`, a retry connected). COM5 boot capture confirmed `la_link_uart: UART2 921600 8N1 RTS/CTS tx=17 rx=16 rts=22 cts=21`, the link tasks with 32/16/8 KiB rings, Wi-Fi station start plus backoff (no credentials), mDNS start and UiTask every 1 s. The first capture showed `line_err`/`rx_bytes` climbing while the Zybo was unpowered: IDF enables a pull-up on CTS but not RX, so GPIO16 floated. Added `gpio_set_pull_mode(GPIO16, GPIO_PULLUP_ONLY)` in `la_link_uart.c`; rebuilt, reflashed and re-captured with `line_err=0 rx_bytes=0`, `fifo_ovf=0`, healthy stacks (rx=2972 tx=2428 net=4236 ui=860) and heap ~121 KB. | Bench PASS for flash/boot/task health. `fw-esp32` rebuild PASS (`la_gateway.bin` 0xc9f40). No Wi-Fi association, TCP relay or loopback gate has run yet; the Zybo was powered off (FTDI COM6 absent) during this session. Change set still uncommitted WIP pending the user's commit/push instruction. |
| 2026-09-10 | Implemented the P5.1-P5.4 ESP32 gateway adapters around the already-tested `la_relay`/`la_net_policy` cores. (a) Board confirmation and pinning: the gateway is a DOIT ESP32 DevKit V1 (classic ESP32-WROOM-32, CP2102, 4 MB); new `la_board` component holds UART2 GPIO16 RX/GPIO17 TX/GPIO21 CTS/GPIO22 RTS, baud 921600, 16 KiB RX buffer, 64-byte FIFO threshold, task cores/priorities/stacks and the GPIO2 LED. (b) New `la_proto` wrapper component compiles `common/la_proto` for xtensa with no copies. (c) `la_link` adapters: `la_link_uart.c` (UART2 8N1 hardware RTS/CTS, rx full threshold 64, 20-entry event queue) and `la_link_tasks.c` (static 32 KiB `link_to_net`/16 KiB `net_to_link` NOSPLIT rings, 8 KiB live mirror ring, LinkRxTask core 1 prio 20 parse/re-encode/classify/mirror with pressure blocking, LinkTxTask core 1 prio 18 with bounded TX-room wait, control-frame encoder with its own buffer/sequence). (d) `la_net` adapters: `net_wifi.c` (STA, RAM storage, PS off, GOT_IP->WIFI_UP / DISCONNECTED->WIFI_LOST events into a static queue), `net_server.c` (NetServerTask core 0 prio 12 owning the policy, 100 ms select loop, single-client accept/reject, non-blocking staging send, host-stream parser with TCP backpressure, ordered ABORT/LIVE_STOP execution, stale-ring drain with no client), `net_mdns.c` (`la-esp32.local`, `_la-analyzer._tcp` :5555) via the pinned `espressif/mdns` 1.12.0 managed component. (e) P5.4 `ui_task.c` (LED states, 1 s counters/stack/heap log, task WDT subscriptions) and `app_main` wiring. (f) `sdkconfig.defaults` pins target esp32, 4 MB flash, 1 kHz tick, trace/runtime stats, task WDT 5 s and 16 KiB lwIP windows; `main/Kconfig.projbuild` holds the non-committed Wi-Fi credentials. Docs: wiring.md now records the confirmed module and approved pins; rtos-design.md documents both adapter layers. | Local gates PASS: `fw-esp32` (la_gateway.bin 0xc9dc0), `test-esp32-mocks` 3/3, `test-native` 11/11, `git diff --check` clean. Bench BLOCKED: flash COM5 failed on a busy port and then boot mode `0x13` (auto-reset did not enter download mode); the user deferred the manual BOOT/EN sequence, so no target behavior is claimed. Next: manual download mode, flash, Wi-Fi credentials, P5.5 loopback and `test-esp32-target` Unity tests. This change set is uncommitted WIP pending the user's commit/push instruction. |
| 2026-09-10 | Created and published the single-page web explainer under `web-artifact/`, grounded in `docs/plan.md`, `docs/architecture.md`, and the current phase ledger. It includes an interactive five-stage signal path, burst/live mode tabs, RTOS ownership model, throughput reality check, verification proof points, and the explicit JD USB-UART blocker. | Complete for the web artifact. `pnpm run build` PASS; local preview returned HTTP 200 at `http://localhost:3000/`; packaged archive `build/rtos-logic-analyzer-site.tar.gz` saved as Sites version 1; private production deployment succeeded at `https://rtos-logic-analyzer-explained.linggaaradhanas.chatgpt.site`. Repository `git fetch origin` remains unavailable in this session because `.git/FETCH_HEAD` is read-only; source was pushed to the Sites repository with commit `905dc566b9cbb87dfefe12c1e517effbfe751687`. Parent repository change committed as `4489da46d9c86c79dd115b4bd43d05b71903253d` and pushed to `origin/main`; a status-only follow-up will record that final commit. |
| 2026-09-10 | Completed the P4 Zynq FreeRTOS core locally. (a) P4.5 `drivers/la_irq`: PL/DMA handlers installed through the FreeRTOS port with explicit 0xA0 GIC priorities, PL IRQ derived as GIC 61/encoded 29 from the xlconcat/IRQ_F2P wiring, notification-only ISRs, XTime ISR/wake latency stats; `la_irq_stats` pure core with host tests. (b) P4.6 `drivers/la_dma`: finite ordinary SG chain per ARM in the non-cached descriptor pool, reset/rebuild, actual lengths + RXEOF collection, error abort; new pure `la_burst_plan` (worst-case words, BD split, capacity/limits, timeout ms) with host tests and a NAK-overflow ARM guard in `la_cmd_dispatch`. (c) P4.7/P4.9 wiring: `la_app` static objects plus `tasks/la_acq_task` (CONFIG/ARM/ABORT/timeout, IRQ+DMA, TxJob handoff with buffer-busy gate), `tasks/la_cmd_task` (parser/dispatch/queue mapping/PATGEN), `tasks/la_tx_task` (window, chunk map, packer, CRC, retransmit, shared sequence space) and `tasks/la_status_task` (watermarks/counters/latency). (d) P4.8 `drivers/la_link_uart`: UART0 EMIO 921600 8N1 with FCM RTS/CTS, ISR RX StreamBuffer and TX staging single-in-flight. (e) Additive `PATGEN_CFG` type 0x17 across protocol.md, `la_proto`, host tool/mock and tests so HIL/E2E can select patgen modes. (f) `tests/hil/test_zynq_serial.py` and the `hil` runner target (`LA_HIL_PORT`/`--port`). Bench: `jtag-zynq` loaded the full firmware; live COM6 capture showed `LA P4.4 register selftest: OK (ID 0x4C410002)`; a 10-minute soak (123 status iterations) ran with UART0 IRQs disabled because the unconnected JD RX floats and storms. Root-caused that storm (`ISR=0x16F`, GIC 59 pending, CPU starved in `FreeRTOS_IRQ_Handler`) with XSDB probes. | Local gates PASS: `test-native` 11/11, `test-host` 44/44, `test-esp32-mocks` 3/3, `release` 14/14, `lint`+`sim` 12/12, `fw-zynq` (text 139084 / data 3656 / bss 123984), `jtag-zynq`, `boot` (BOOT.BIN 2340656). Bench PASS: P4.4 on-target selftest, P4.2/P4.5 10-minute no-assert soak, stable stacks `acq=460 cmd=452 tx=211 status=177`. P4.10 HIL BLOCKED: no 3.3 V USB-UART adapter on JD (only the onboard FTDI console COM6). On-target DMA burst/coherency and on-wire link remain unverified. Pushed in `f2dec10`. |
| 2026-09-10 | Continued the USB-only Zybo bring-up. Root-caused the common LD0-dark/GP0-timeout failure to `fpga/scripts/build.tcl`: `proc_sys_reset/aux_reset_in` is active-low (confirmed in the generated IP parameters) but was tied to constant 0, permanently asserting `peripheral_aresetn` and `la_top.srst`. Tied it high, rebuilt bitstream/XSA, rebuilt Vitis platform/app, and added a CPU-side PL probe. Hardened `jtag_load.tcl` with boot-mode checking, DAP recovery, explicit A9 halts, JTAG SRST and level-shifter verification. Corrected SLCR diagnostic offsets and all unsupported `%lu` console formats. Added the workflow to `docs/windows-dev.md`. | Bench/local PASS: `bit` (WNS +0.255 ns, WHS +0.024 ns, DRC clean), repeated `fw-zynq`, repeated `jtag-zynq`, XSDB PL/DMA reads, sequential `lint`/`sim`, `release` 12/12 and concurrent COM6 capture. User confirms LD0 blinks; firmware reports ID `0x4C410002`, IDLE status `0x1`, stack high-water `133/200`, and continues ticking. P3.3 complete and pushed in `c460596`; next target work is the P4.2 10-minute soak then full P4.4 register self-test. |
| 2026-09-09 | WIP: native-Windows USB-only Zynq development loop. Added `fw-zynq/scripts/jtag_load.tcl` and the canonical `python tools/run.py jtag-zynq` target: XSDB connects/auto-launches `hw_server`, clears sticky DAP AP faults with `rst -dap`, verifies JP5's latched boot mode is JTAG (`0`), resets the PS, programs `logic_analyzer.bit`, downloads/runs the generated FSBL to its `FsblHandoffJtagExit` breakpoint, downloads `la_app.elf`, and starts Cortex-A9 #0. The first attempt exposed an OCM AP timeout fixed by DAP recovery; the second exposed boot mode `1` (QSPI) because JP5 had not been power-cycled; after the user power-cycled with JP5=JTAG, the register read `0` and the complete loader PASSED. COM6 then streamed FreeRTOS ticks 19-26 and SLCR values, proving the USB-only load/run/console path. Follow-up fixes corrected the diagnostic offsets to ARM_CLK_CTRL `0xF8000120` and FPGA0_CLK_CTRL `0xF8000170`; fixed `la_platform.py` to extract `parameter_name` from Vitis parameter dictionaries and apply both `freertos_stdout`/`freertos_stdin`; the next `fw-zynq` build PASSED and logged both params set to `ps7_uart_1`, with all generated UART headers already correct. | Paused WIP, uncommitted. The board is still running the earlier successfully JTAG-loaded ELF; the latest rebuilt ELF (correct FPGA0 clock diagnostic and native UART param fix) has not yet been loaded. Exact continuation: run `.venv\Scripts\python.exe tools\run.py jtag-zynq`, read COM6 and confirm recurring `FPGA0_CLK`/`FPGA_RST` values, then investigate the PL AXI read without putting a blocking `Xil_In32(0x40000000)` back into firmware. Existing uncommitted blink/UART changes remain preserved. Reports: `build/reports/{fw-zynq,jtag-zynq}.{json,log}`. |
| 2026-09-09 | First [HUMAN] bench action: P3.3 JTAG programming of `build/release/logic_analyzer.bit` on the Zybo Z7-10 (FTDI JTAG target `Digilent/210351BE7A8AA`). The first `program.tcl` run FAILED with `[Labtoolstcl 44-10] Device arm_dap_0 is not programmable`: on a Zynq chain `get_hw_devices` lists the ARM DAP (`arm_dap_0`, `PART=arm_dap`) before the PL part, and the script blindly programmed device index 0. Probe confirmed the chain exposes exactly `arm_dap_0` + `xc7z010_1`. Fixed `fpga/scripts/program.tcl` to select the first device whose PART matches `xc7*` (with an error listing the whole chain when none matches). Re-run PASSED: `Selected programmable device xc7z010_1 (xc7z010)`, `program_hw_devices` completed, `[Labtools 27-3164] End of startup status: HIGH` (DONE asserted - configuration successful). Command: `C:\AMDDesignTools\2026.1\Vivado\bin\vivado.bat -mode batch -source fpga\scripts\program.tcl -tclargs <project root>`. | P3.3 programming element PASSED on bench: `logic_analyzer.bit` is configured in the PL over FTDI JTAG. The "verify blink" element of P3.3 remains OPEN: the design has no LED driver - `la_top` exposes no LED output, `zybo_z7_10.xdc` maps none, and the StatusTask heartbeat LED from `docs/rtos-design.md` is not implemented, so there is nothing visible to verify yet. Blink needs a small RTL LED output + XDC pin with re-lint/sim/bit (or a revised gate) before P3.3 fully closes; then the P4.2 console-hello bench follows. |
| 2026-09-06 | P7.5 host-side live decode + P10.4 boot packaging + P7.1 pool audit, hardware-independent: (a) `host/la_cli/protocol.py` gained DECODER_CFG UART packing (20 B wire) and an EVT_DECODED batch parser (slot/kind/t_sample/len/data records); `client.live()` configures a decoder slot, LIVE_STARTs and collects events with a merged pump (LIVE_START-ACK may share a TCP read with the first batch), count truncation, EVT_OVERRUN stop and LIVE_STOP teardown; `la-cli live --decoder uart:rx=0,baud=115200 [--csv --count --rate]` prints timestamped events or CSV plus overrun counters - the burst-phase firmware NAKs DECODER_CFG/LIVE_START as unsupported, which the CLI reports honestly until the P7 firmware live path exists. Mock scenario `live` streams three EVT_DECODED batches of decoded "LA-TEST\n" plus an EVT_OVERRUN; 6 new pytest cases (pack layout, full collection + overrun, early count stop, CLI text/CSV, bad spec). (b) P7.1 audit: `fw-zynq/tests/host/segment_pool` already implements the P7.1 pool contract (16 segments, FREE/DMA_OWNED/CPU_OWNED, generation + sequence counters, publish-with-consumer-count, abort rules, Windows-thread concurrent last-release and consumer-blocks-abort races in `la_native_tests`); no gap requiring new code - recorded for the P7.3 task-wiring phase. (c) P10.4 tool step: `fw-zynq/scripts/make_bootbin.py` + new `python tools/run.py boot` target stage fsbl.elf (export copy preference) + la_fw.elf + the bitstream under build/release, write bootgen.bif, run bootgen 2026.1 natively and verify the image (size + sync header); SD boot itself stays the [HUMAN] P10.4 gate. Installed `la-cli` console script verified (`ports`, help). | Local gates PASS: `test-host` 42/42 (stable over repeated runs), `release` 12/12 (gen up-to-date, full CTest), `boot` PASS - BOOT.BIN 2265028 bytes (fsbl 531664 + la_fw 435192 hello-firmware ELF + bit 2083945; sync header verified; tool-chain evidence only - the ELF is the P4.1/P4.3 hello build, not a release candidate), compileall clean, `git diff --check` clean. |
|---|---|---|
| 2026-09-06 | P6.1 host tool implemented end to end, hardware-independent: `host/la_cli/` now contains `protocol.py` (wire constants, CRC16/CRC32 with reference vectors, frame encode + stateful FrameParser, fixed-payload pack/unpack), `transport.py` (tcp://host:port, serial://COMx with COM>=10 device-prefix, explicit-target rule, serial candidate listing for CP210x/CH340/FTDI via VID), `client.py` (AnalyzerClient: ping/status/abort + burst capture with per-command ACK correlation, batch ACK every 16 chunks + end drain, contiguous-offset gap detection, DATA_END CRC32/IEEE verification, timeouts and EOF detection), `convert.py` (streaming raw/txn sample reconstruction - {run[23:0], sample[7:0]} RLE never expands in memory, .sr zip writer (version/metadata/logic-1-1, unitsize 1), VCD writer via pyvcd with rate-derived timescale, PNG renderer via matplotlib Agg) and `commands.py` click CLI (`la-cli ports|ping|status|capture --rate --words --mode --pre --trig-rise/--trig-fall --out|abort`, SI suffix parsing, rate->sample_div mapping with actual-rate note). `host/tests/` gained a mock analyzer (`mock_analyzer.py` speaking protocol.md over an ephemeral TCP port with scenarios ping/status/noise/capture_raw/capture_txn/nak_config/silent/chunk_gap/chunk_bad_crc/close_on_ping and ACK-drain accounting) and 35 new pytest cases across wire vectors, parser resync, client flows (raw+t+txn transfer equality, NAK/bad-field, gap + corrupted-CRC detection with parser counters, timeout, EOF), exports (.sr zip content and byte equality, VCD structure, PNG, txn reconstruction) and CLI behaviour. | Local gates PASS: `test-host` 36/36 (stable over repeated runs; fixed a recv-boundary race where ARM-ACK and the first data frames shared one TCP read by merging ARM-ACK handling into the capture pump, and a mock ACK-drain race by waiting on the drain in the test). Notes: pyvcd 0.4.0's importable package is `vcd` (writer.VCDWriter) on this environment; matplotlib emits Pyparsing deprecation warnings only. On-wire behaviour against the real Zynq remains the P4.10 serial HIL gate. |
| 2026-09-06 | P5.3 network session policy core, hardware-independent: `fw-esp32/components/la_net` is now a real IDF component (`idf_component_register` with `src/la_net_policy.c`, public `include/la_net_policy.h`; `main` REQUIRES la_link + la_net so both compile in the build gate) implementing the session lifecycle policy in pure C: IDLE/CONNECTING/BACKOFF/CONNECTED/CLIENT_ACTIVE states; immediate first attempt; exponential backoff (default 1 s doubling to 60 s cap, configurable and clamped) driven by explicit BACKOFF_TICK events against a caller monotonic clock; single-client rule with CLIENT_REJECT; client disconnect and Wi-Fi loss with an active client both emit the ordered [LINK_ABORT, LINK_LIVE_STOP] actions (protocol.md); STOP from any state with the matching stop actions; counters for attempts/successes/accepted/rejected/disconnects. No ESP-IDF/socket/timer calls; adapters (WifiMgr/NetServerTask) feed events and execute actions. MSVC tests `tests/native/esp32_net_host_tests.c` (`la_esp32_mocks_net`, 8 scenarios: defaults/clamps, happy session incl. ordered disconnect actions, full backoff sequence with exact deadline math and counter parity, wifi-loss-with-client, single-client rejection, stop from every state, guards). Design note in `docs/rtos-design.md` ("ESP32 network session policy core (P5.3)"). | Local gates PASS: `test-esp32-mocks` 3/3 (new `la_esp32_mocks_net`), `fw-esp32` PASS - `la_net` component compiles under xtensa-esp32-elf (`libla_net.a` + `libla_link.a` present, elf/bin emitted; fixed a NULL-config default bug the host tests caught: multiplier defaulted to 1 instead of 2). Wi-Fi/lwIP/mDNS driver tasks and the loopback gate remain P5.5+/target steps. |
| 2026-09-06 | P5.2 link relay core, hardware-independent: `fw-esp32/components/la_link` is now a real IDF component (`idf_component_register` with `src/la_relay.c`, public `include/la_relay.h`; `main` REQUIRES it so the build gate compiles it) implementing the pure relay policy: per-leg type tables (Zynq->host leg: EVT_DECODED best-effort + mirror; host->Zynq leg: empty tables = all control), drop-newest for best-effort when the ring lacks space, PRESSURE + counter for control frames (never dropped; unknown parseable types proxy verbatim), FORWARD outcomes counted only after the caller reports the real put/mirror result (NOSPLIT fragmentation), frame counters for UiTask. No FreeRTOS/ESP-IDF/la_proto calls in the core; tasks supply tables and capacity/report callbacks. MSVC-hosted tests in `tests/native/esp32_relay_host_tests.c` (`la_esp32_mocks_relay` CTest; `tools/run.py` mock filter widened to `^la_esp32_mocks`) cover classification, forward+mirror counters, congestion drop/pressure, unknown-type proxy, all-control host leg and argument guards. Design note recorded in `docs/rtos-design.md` ("ESP32 link relay core (P5.2)"). | Local gates PASS: `test-esp32-mocks` 2/2 (new `la_esp32_mocks_relay`), `test-native` 9/9, `fw-esp32` PASS - the la_link component compiles under xtensa-esp32-elf gnu17 (`libla_link.a`, elf + `la_gateway.bin` emitted; fixed a -Werror=parentheses condition the GCC toolchain found that MSVC accepted), `git diff --check` clean. UART/ring task adapters, RTS/CTS driver work, Unity target tests and loopback remain P5 bench/target steps. |
| 2026-09-06 | P4.9 core completed, hardware-independent: (a) `common/la_proto` typed wire helpers for the fixed payloads the burst sender and its host peer exchange - PONG(16 B), ACK(2 B), NAK(3 B), STATUS(52 B), DATA_BEGIN(16 B), DATA_END(4 B) and the DATA_CHUNK payload builder (u32 LE byte offset + up to 1016 data bytes, offset-overflow guarded) with pack/unpack and byte-level little-endian vector tests inside `la_native_tests`; (b) `fw-zynq/app/src/tasks/la_cmd_dispatch.{h,c}` CmdTask receive-side router (PING->PONG, CONFIG latch + validate, ARM/ABORT forward decisions with optimistic view mirror reconciled by AcqTask lifecycle notices, GET_STATUS->snapshot request, inbound ACK/NAK routing: chunk ACK/NAK -> TX window, other NAK -> burst failure; error mapping length/bad-field/bad-state/busy/unsupported; ABORT in IDLE idempotent ACK; P7.x DECODER_CFG/LIVE_START/STOP and STREAM ARM NAK unsupported) + `la_native_cmd` host tests (11 scenarios); (c) `fw-zynq/app/src/tasks/la_tx_pack.{h,c}` TxTask burst generator (job -> DATA_BEGIN/CHUNK*/END payloads, read-callback backed so it owns no capture memory, streamed CRC32/IEEE == one-shot of the capture bytes, deterministic `chunk_at` rebuilds for retransmission without CRC/offset side effects, word-aligned 1016-byte chunk grid, empty-job BEGIN+END, failure-atomic next()) + `la_native_txpack` host tests (multi-chunk job with 31 chunks and reconstruction equality, boundary jobs at 0/1/254/255/508 words, capacity/read/range guards). Both cores documented in `docs/rtos-design.md` ("CmdTask command dispatch core (P4.9)", "TxTask burst transfer packer (P4.9)"). | Local gates PASS: `test-native` 9/9 (with .venv python; new `la_native_cmd` + `la_native_txpack` CTest targets, typed-payload vectors inside `la_native_tests`), `release` 10/10 (gen up-to-date, full CTest), `git diff --check` clean. P4.9 remaining on-target wiring (TxTask/CmdTask FreeRTOS tasks, seq assignment + window slot->chunk-index mapping, ACK accounting) stays a later bench-gated step; `fw-zynq` app build not re-run (new modules are not yet imported into the Vitis app sources). |
| 2026-09-06 | P4.1 blocked by environment; recorded evidence and prepared WIP inputs. | P4.1 `platform.py`/`app.py` implemented against the official Vitis 2026.1 API shape (create_platform_component/add_domain/build, find_platform_in_repos, create_app_component/import_files/build, ELF verification), FreeRTOSConfig policy fragment (appended to the BSP-generated header with an original kept for diff; static allocation, timers, trace facility, stack-overflow checks, assertions; run-time stats deferred to P8) and `fw-zynq/app/src/main.c` hello (static tasks, idle/timer memory hooks, overflow/malloc hooks, stack high-water probe). All UNVERIFIED: the Vitis platform service fails for every XSA/preset including AMD's own example (root-caused the next row). | BLOCKED (installation). |
| 2026-09-06 | P4.3 memory-map core + P4.7 AcqTask FSM core, both hardware-independent. P4.7: `fw-zynq/app/src/tasks/la_acq_fsm.{h,c}` burst state machine (IDLE/ARMED/CAPT; configure/arm/trigger/timeout/abort/complete; stream config rejected -2 until P7.x; events EVT_TRIGGERED with trig_index, JOB with raw STATUS snapshot + word count, ABORTED with cause) + `la_native_acq` host tests (happy burst, stream rejection, illegal transitions, abort paths, overflow passthrough, init). P4.3: `fw-zynq/app/src/la_mem_geom.{h,c}` (1 GiB DDR 0x0010_0000..0x4000_0000; NC window 0x3F00_0000 16 MiB in 1 MiB MMU sections; capture pool 8 MiB + SG pool 1 MiB, 64-byte aligned) with full-branch `la_mem_geometry_check_full` + per-section `la_mem_apply_noncached`, `la_native_mem` host tests; `fw-zynq/lscript.ld` = verbatim 2026.1 template + `__la_nc_base/size/end` + `__la_fw_end` + link-time ASSERT (firmware below the window) - the app script installs it over the generated script because import_files refuses the overwrite; `main.c` boot contract (geometry check on `__la_fw_end`, then `Xil_SetTlbAttributes` NORM_NONCACHE per section). Docs: rtos-design.md "Cache, MMU and Buffer Contract" now names the concrete layout. | Local gates PASS: `test-native` 7/7, `release` 8/8, compileall and `git diff --check` clean; `python tools/run.py fw-zynq` PASS with the P4.3 ELF (link ASSERT held; nm shows fw end 0x00127E80 vs NC base 0x3F000000). On-target coherency (DMA write + CPU readback) and the FSM's real IRQ/register behaviour remain bench gates, honestly unverified. |
| 2026-09-06 | P4.9 core continued: TX window tracker `fw-zynq/app/src/tasks/la_tx_win.{h,c}` after recording the sender policy in `docs/rtos-design.md` ("UART burst TX window"): one slot per in-flight DATA_CHUNK, depth 16 (protocol.md window), ACK clears the matching slot, NAK flags retransmit-pending without adding a slot (resend keeps the sequence, so no duplicate sequences in the window), stale/duplicate ACKs ignored, retransmits pop oldest-first; pure logic with no FreeRTOS/UART calls; TxTask owns it as single writer. `la_native_txwin` host tests cover the full window, ack/stale/duplicate/out-of-order semantics, nak-retransmit flow, sequence wraparound (0xFF/0x00) and mixed nak ordering. | Local gates PASS: `test-native` 5/5, `release` 6/6 (with .venv python), compileall and `git diff --check` clean. Remaining P4.9 core: TxTask/TxJob packing and CmdTask command dispatch (host-testable); on-wire behaviour stays a bench gate. |
| 2026-09-06 | P4.9 core start, hardware-independent: added the streaming CRC-32/IEEE API to `common/la_proto` (`LA_CRC32_IEEE_INIT`, `la_crc32_ieee_update`, `la_crc32_ieee_finish`) so TxTask can checksum chunked DATA_BEGIN/CHUNK/END payloads exactly as protocol.md specifies (CRC32/IEEE over the concatenated bytes, init 0xFFFFFFFF, reflected); `la_crc32_ieee` refactored on top of the same primitives (identical results). `la_native_tests` now proves chunked==one-shot for splits {0,1,2,3,7,31,101,1024}, byte-at-a-time streaming, zero-length no-op updates and the 0xCBF43926 standard vector. | Local gates PASS: `test-native` 4/4 (with .venv python). Remaining P4.9 core: TxTask/TxJob packing and CmdTask command dispatch - still hardware-independent; on-wire behaviour stays a bench gate. |
| 2026-09-06 | P4.4 register driver + selftest (hardware-independent, pursued while the P4.2 bench is pending): `docs/regmap.yaml` base corrected 0x43C00000 -> 0x40000000 (the P1 default never matched the P3 BD assignment; header/pkg/model/docs regenerated, idempotent `gen` + `--check` clean); `fw-zynq/app/src/drivers/la_regs_drv.{h,c}` written as pure C99 with injected MMIO accessors (host: fake memory; target later: Xil_In32/Out32 + XPAR_LA_TOP_0_BASEADDR at the P4.7 call site) and `la_regs_selftest` = ID equality + SAMPLE_DIV at-reset, two full-width write/read-back patterns and clear-to-reset; `tests/native/regs_host_tests.c` + `la_native_regs` CTest target exercise raw access, RTL out-of-range parity (read 0/ignore write) and every selftest branch. | Local gates PASS: `test-native` 4/4 (new `la_native_regs`), `release` 5/5 (gen + full CTest), compileall clean. On-target AXI round-trip is a bench gate (P4.4 row of the phase matrix) and stays unverified. Next: P4.2 [HUMAN] bench or P4.3/other hardware-independent work. |
| 2026-09-06 | P4.1 unblocked and completed locally (pushed as `e276d77`). Installer repair applied by the user (Vitis Embedded Development + SoCs/Zynq-7000 -> `Vitis\data\emulation\platforms\{versal,zynq,zynqmp}` present). Fix chain from the first `fw-zynq` run to PASS: (a) renamed `fw-zynq/scripts/platform.py` to `la_platform.py` - the file shadowed the stdlib `platform` module the Vitis CLI imports (`module 'platform' has no attribute 'system'`); (b) `freertos10_xilinx` is not the API OS name - 2026.1 accepts `standalone|freertos|linux`, so OS_NAME=`freertos`; (c) the freertos BSP validation requires a timer hardware instance ("freertos10_xilinx requires at least one timer hardware instance"), the zybo preset disables both PS7 TTCs, so `build.tcl` now forces `PCW_EN_TTC0`/`PCW_TTC0_PERIPHERAL_ENABLE`=1; (d) AMD Arm GCC is not long-path aware (proven at 269 chars) and the fixed deep root pushed Vitis CMake trycompile paths past 260 - plan path-length gate applied: platform `la_platform`->`lap`, domain `freertos`->`fr`, and the header-lookup/copy logic uses deterministic short paths; (e) `write_hw_platform` needed `-force` on re-runs (bit had previously only passed on a fresh release dir); (f) QSPI disabled in the PS7 config (2026.1 SDT-flow FSBL fails on `XPAR_XQSPIPS_0_QSPI_BUS_WIDTH`, and the design boots SD/JTAG); (g) `platform.build()` regenerates FreeRTOSConfig.h from its template each call and the ninja tree has no header depfiles, so the policy fragment (static allocation on, stats-formatting/run-time-stats off until P8, tick 1 kHz, stack-overflow check, assert) is applied to both the bsp/include and gen_bsp/include copies, the freertos kernel object set is deleted and rebuilt by direct ninja, and the refreshed archives are copied over the platform export lib the app links; (h) `ensure_clean_workspace` now clears read-only attributes (Python >=3.12 rmtree) and a stray-file filter keeps the app from resolving two FreeRTOSConfig copies. Gate evidence: `FW-ZYNQ PASS: build/vitis/ws/la_app/build/la_app.elf` (text 58808/data 3024/bss 96312), FSBL built, platform build clean. Kill orphaned `java.exe ... RigelApp` servers before retries (they hold ws file locks). | P4.1 complete locally (`python tools/run.py fw-zynq` PASS). Next: commit/push this ledger and fix set, then P4.2 first [HUMAN] bench session: program `build/release/logic_analyzer.bit` and run the hello on hardware (console + blink, 10 min no-assert), then P4.3. |
| 2026-09-06 | Root-caused the P4.1 Vitis blocker by replaying the failing platform creation against a server launched with console/JVM-DEBUG logging (log4j2 override via `VITISNG_SERVER_OPTS`), which surfaced the Java stack traces the CLI/IDE hide: (1) the SDT/processor-list step needs `Vivado\bin` on PATH - `vitis.bat` omits it here because its guard builds `C:/AMDDesignTools/Vivado/Vitis` from `RDI_INSTALLVER` while the layout is `C:\AMDDesignTools\2026.1\Vivado`; prepending it advances every input (official zcu102 preset and the repo XSA) past the generic "Error in generating Processor List" / "Invalid project location" symptoms; (2) with PATH fixed, `PlatformServiceUtils.populateResources` throws `FileNotFoundException` for `Vitis\data\emulation\platforms\zynq\sw\a9_linux\qemu` (and zynqmp/a53_linux for zcu102): the whole `data/emulation/platforms` tree and classic xsct are absent, the install carries the `.vitis_for_hls` marker and no Vitis uninstall record - an HLS-flavored install lacking the embedded platform data component. Verified non-causes: workspace location/format (C:\tmp, C:\vp, C:/vitis_workspaces, slashes - identical), license check (server logs show license checkout OK; `IDE_SKIP_SERVER_LICENSE=1`), SDT engine itself (`sdtgen` standalone on the shipped zcu102.xsa completes). | BLOCKED (installation, root-caused). Next human action: AMD Unified Installer repair/add of the full Vitis (embedded/emulation data + xsct), then `python tools/run.py fw-zynq` (prepending `Vivado\bin` to PATH if the launcher still omits it), then the first bench session. Status ledger updated only; repo code unchanged. |
| 2026-09-06 | Closed the P3 ledger: recorded the P3.1/P3.2 gates, phase and caveats. | P3.1/P3.2 complete locally; next action is P4 (Vitis platform/app from the XSA). P3.3 and hardware gates remain [HUMAN]. |
| 2026-09-06 | P3.1/P3.2 implemented: full `build.tcl` (board project on `zybo-z7-10`, PS7 board preset from the pinned `preset.xml` + UART0 EMIO full RTS/CTS, `la_top` RTL module reference, `axi_dma` S2MM-only SG with 26-bit lengths, two SmartConnect fabrics because this 2026.1 install serves no `axi_interconnect` definition, proc_sys_reset, xlconcat IRQ fan-in, explicit address map la_top 0x4000_0000 / DMA 0x4040_0000 / S2MM+SG full DDR via HP0, wrapper, synth/impl/bitstream, `write_hw_platform`) and `zybo_z7_10.xdc` (JC CH0-7, JE PATGEN0-7, JD UART0 pins; async-input false-path exception). First full run failed the placer: the async-read ring/FIFO memories (512 Kb history) exceeded the part's 6000 LUTRAM cells; after the BRAM rework the flow passed with zero failing endpoints (WNS +0.278 ns at 100 MHz), DRC no errors, utilization 33.5 % LUT / 24.2 % FF / 60.0 % BRAM tiles. Recorded utilization and integration notes in `docs/architecture.md`, acceptance matrix and caveats in `docs/test-plan.md`. | Local gates PASS: `python tools/run.py bit`; artifacts under `build/release`. Remaining honest caveats (recorded in the docs): SmartConnect replaces the plan-named axi_interconnect (absent from the installed catalog); ps7 EMIO UART0 scalar connects override the inferred `UART_0` interface with expected BD warnings; board-preset negative DDR DQS skews flagged PSU-1..4 until a PS boot check on hardware; `IRQ_F2P` is one shared fabric line for la_top and DMA interrupts. |
| 2026-09-06 | P3 fabricability rework: converted `la_pretrig_ring` and `la_axis_fifo` to synchronous-read block RAM (one-beat output register, capacity/drop/order semantics preserved, +1 emission latency documented) and latched the `la_patgen` UART/SPI/I2C period products at the enable edge (removed the DSP-multiplier-per-cycle compare path that caused the first timing failures). Ring TB model and FIFO TB compare stay value-exact (tkeep-masked data compare for the byte-enable RAM). | Local gates PASS: `lint` then `sim`, 12/12 testcases, zero warnings, after each rework step; pushed as `3d940bb`. |
| 2026-09-06 | P3 prep on P2 RTL: exported the la_patgen channels as `la_top.patgen_o[7:0]` (drives the JE header pads per `docs/wiring.md`); added `(* async_reg *)` to the `la_input_sync` 2-FF stages (P3.2 CDC placement point); extended `tb_la_top` to wire and check `patgen_o` (idle zero when disabled; exact +1-per-cycle counter steps on JE during scenario B without shifting the trigger phase the level-trigger test depends on). | Local gates PASS: `lint` then `sim` sequentially, 12/12 testcases, zero warnings (attributes compile clean in Questa FSE). Pushed as `cb41d63`. |
| 2026-09-05 | Created the native-Windows Phase 0 scaffold; installed and locked prerequisites; added board files, runner environment import, native/Questa/ESP-IDF checks; fixed ESP-IDF output placement and board-query lock handling; created and pushed baseline commit `6dd22b3`. | Local Phase 0 gates pass; later implementation and hardware gates remain explicitly blocked. |
| 2026-09-05 | Defined the mandatory status, commit and `origin/main` push workflow in `AGENTS.md`, `README.md` and this document. | Committed as `46f4ac7`, pushed to `origin/main`, and verified against the remote hash. |
| 2026-09-05 | Implemented P1.1-P1.7: normative register constraints and generated views; typed protocol framing/config helpers; UART/SPI/I2C decoders with discontinuity events; independent Python vectors and ctypes parity tests; RTOS, protocol, decoder, wiring and verification specifications. | Local gates PASS: generator idempotence, native 3/3, release 4/4, ESP32 mocks 1/1, host 1 passed, Questa lint/sim, compileall and `git diff --check`. Reports are under `build/reports`; hardware, target RTOS scheduling, DMA coherency and physical wiring remain unverified by design. |
| 2026-09-05 | Pushed implementation commit `52962c1` to `origin/main` and verified `git status --short --branch` is clean, `git log` points at the pushed commit and `git ls-remote origin refs/heads/main` matches `52962c156b15a66bfe91f11c89e4f2b50f24cc46`. | P1 is complete. Next action is P2 RTL; hardware and target-only gates remain explicitly unverified. |
| 2026-09-05 | Extended the Questa regression runner with per-test PASS markers (`PASS: <case name>`) so `testlist.json` can host one self-checking test per RTL block; registered P2.1-P2.3 with their testbenches. Implemented `la_input_sync` (2-flop synchronizer per channel, optional 2-of-3 majority filter; latency 2/3 cycles, glitch rejection tested), `la_sampler` (free-running divider f_s = FCLK/(DIV+1), chan_en masking, aligned sample_valid), `la_trigger` (level on TRIG_MASK/TRIG_VALUE and "empty edge mask = true" rise/fall semantics between valid samples; no spurious edges on the first sample) with procedural self-checking scoreboards in Questa. Named design conventions: single FCLK, synchronous active-high `srst`, sample/`sample_valid` pulse interface, `trig` pulse per qualifying valid sample. | Local gates PASS: `lint` and `sim` (4/4 testcases) run sequentially; reports under `build/reports`. Questa Altera FSE notes: `default_nettype none` with ANSI ports fails compilation (avoided); module-level assigns compile where an equivalent local-declaration always_comb did not (workaround used in TBs); RTL files carry no `timescale` yet (P2.4 TBs will add TB-only timescale as today). |
| 2026-09-05 | Implemented P2.4 `la_pretrig_ring` with its design specification in the module header: DEPTH=65536 (2^16) dual-port circular history; stream pass-through mode (pre_req==0) and burst windowed mode with exact seam (no duplicate/lost/invented sample; trigger sample starts POST); window intact regardless of armed-run length because DEPTH > max(K)+1; overflow sticky with documented stall budget DEPTH-(K+1) samples and never-silent gap marking; run-edge lifecycle and status outputs pre_len/trig_index. Self-checking testbench at DEPTH=512 exercises stream pass-through with stalls, burst K<pre, early trigger (K=trig_index), 2000-sample history wrap, K clamped to DEPTH-1 with a permanently full ring (same-cycle read/write at full capacity), downstream stall overflow with exact 49-drop count, double trigger during a stalled replay and abort mid-replay with a clean rerun. The scoreboard mirrors admit/drop rules and compares every emission against a value FIFO of admitted samples. | Local gates PASS: `lint` and `sim` (5/5 testcases); reports under `build/reports`. Found and fixed an RTL bug during bring-up (run_d edge detection missing after refactor); testbench driver discipline added (drivers must quiesce in_valid after the last pulse to avoid duplicate samples). |
| 2026-09-05 | Implemented P2.5 `la_txn_encoder` ({run[23:0], sample[7:0]} words; run in bits [31:8]) and P2.6 `la_raw_packer` (4 samples/word, oldest in bits [7:0], tkeep/tlast, PACKET_WORDS count-based tlast, flush-deferred tlast never emits empty beats). Encoder semantics documented: words on value change, max_run split, packet flush and capture end; never zero-length; same-value words across splits/flushes never create false decoder edges; exact sample accounting; flush parked behind a stalled word with the input blocked so no sample straddles a packet boundary; same-value absorption while m_ready is low. Encoder TB uses a clocked event model (predicted word list from the accepted sample stream and flush events; every emission must match) plus run-total == accepted-sample accounting over alternating, 60000-sample constant-run with max_run=100, mid-run and no-op flushes, change-stall and flush-behind-word back-pressure scenarios, max_run=1 and reset. Packer TB reconstructs the accepted byte stream from emitted beats (tkeep-aware) and checks packet length and tlast structure under continuous traffic, flush tails, word-aligned deferred flushes, m_ready stalls and idle no-op flushes. | Local gates PASS: `lint` and `sim` (7/7 testcases); reports under `build/reports`. TB work confirmed two testbench disciplines: clocked scoreboards reading pre-edge values (negedge observation of acceptance is off-by-one when flush/ready gates change), and input quiescence between sample bursts. |
| 2026-09-06 | Closed the P2 ledger: ran the P2.12 gates (`lint` then `sim`, 12/12 testcases, zero Questa warnings after fixing a redundant-digit literal in `la_patgen`); re-ran `gen` (up to date), `test-native` (3/3), `test-esp32-mocks` (1/1) and `test-host` (1 passed); added the P2 acceptance matrix and caveats to `docs/test-plan.md`; updated the phase matrix, snapshot and this log. | Local gates PASS. P2 is complete locally; next action is P3 Vivado integration. Remaining honest caveats: regress.py has no per-test Python vector hook (independent Python reconstruction of the la_top UART capture is deferred to the P4.10 host/HIL path), and the la_top raw-burst drain can carry up to one pipeline word beyond the K+POST target (assertions bounded; wave-level accounting recommended before P3 bitstream bring-up). |
| 2026-09-05 | Implemented P2.11 `la_top`, the full acquisition integration: SELFTEST mux (patgen vs ch_in), 2-flop synchronizer, sampler, trigger, pre-trigger ring, txn encoder/raw packer mode mux into the AXIS FIFO (m_axis out), capture controller, AXI-Lite register block and STATUS composition with IRQ events and sticky DONE/TRIGGERED/ABORTED/FIFO_OVF latches cleared at ARM. The integration TB drives la_top through the AXI-Lite BFM in SELFTEST mode with an AXIS sink and word-to-sample reconstruction: scenario A (txn-mode burst on the UART patgen, trigger on the ch0 start edge) reconstructs the sample stream from {run, sample} words and independently decodes 8N1 UART at 115200 baud, finding "LA-TEST\n" with a strict bit-for-bit search; exactly one tlast; controller word count equals the sink; STATUS DONE and IRQ behavior verified. Scenario B (raw-mode counter burst with a level trigger on 0xA5) verifies DONE, single tlast, sink == controller word counts and a long strictly-consecutive counter run with the trigger sample at window position K; the exact drain-total count is asserted bounded (K+POST +/- one pipeline word) because the raw-burst run-drop boundary can carry a few extra samples - an accounting question recorded for the next session, not a stream-integrity issue (reconstruction is exact and consecutive). Bring-up found and fixed real bugs: la_raw_packer had lost its s_ready assignment in a rewrite (its standalone TB passed vacuously until an activity assertion was added), the field-index `[MASK[0]]` idiom indexes bit 1 for mask=1 (replaced with $clog2 in la_top), and the register semantics of sticky CTRL bits mean a plain ARM write clears SELFTEST_EN (scenarios now write SELFTEST|ARM together). | Local gates PASS: `lint` and `sim` (12/12 testcases); reports under `build/reports`. Next: P2.12 final regression review, then close the P2 ledger. |
| 2026-09-05 | Implemented P2.10 `la_regs`, the AXI-Lite slave over the generated la_regs_pkg map: independent AW/W arrival with response and read stalls, byte strobes applied before field masks (un-strobed bytes preserved, reserved bits cleared), self-clearing ARM/ABORT/SOFT_RESET pulses, sticky SELFTEST_EN/STREAM, PRE_SAMPLES/TXN_MAX_RUN clamping, IRQ_EN masked and IRQ_STAT write-1-to-clear with external event pulses (DONE/TRIGGERED/OVERFLOW bit indexes derived from the generated masks) and the irq output = (IRQ_EN & IRQ_STAT) != 0; RO ID/STATUS/TRIG_INDEX/WORD_COUNT pass-throughs; out-of-range addresses read 0 and ignore writes with OKAY responses. TB uses a procedural AXI-Lite BFM over AW-first, W-first and simultaneous patterns with BREADY/RREADY stalls, strobe preservation, self-clearing pulse capture, masked-field and clamp readbacks, IRQ W1C/event behavior and out-of-range checks. | Local gates PASS: `lint` and `sim` (11/11 testcases); reports under `build/reports`. BFM found the byte-select bug (nibble vs byte expansion) in apply_wr; expected-value corrections for masked writes documented in the TB comments. |
| 2026-09-05 | Implemented P2.9 `la_patgen` with the PATGEN_CTRL modes: counter, walking-1 and PRBS8 (x^8+x^6+x^5+x^4+1) step-timed by DIV+1; UART 115200 bit-bang on ch0 transmitting "LA-TEST\n" 8N1 repeatedly; SPI 1 MHz bit-bang (ch0 SCK, ch1 MOSI, ch2 CS_N) repeating the 0x4C 0x41 frame; I2C 400 kHz bit-bang (ch3 SCL, ch4 SDA) repeating START / write 0xA0 / ACK / 0x4C / ACK / 0x41 / ACK / STOP with the ACK bits driven low for decoder self-test. Rates derive from a CLK_HZ parameter (100 MHz default) scaled by DIV+1; configuration latches at the enable edge and all machines restart there. TB records the output history and independently decodes it: model checks for counter/walking-1/PRBS8 with DIV scaling, a start-synchronized mid-bit UART sampler recovering "LA-TEST\n" twice, MOSI sampled on SCK rise across three SPI frames, and an I2C slot-stream + START/STOP/repeat check. | Local gates PASS: `lint` and `sim` (10/10 testcases); reports under `build/reports`. |
| 2026-09-05 | Implemented P2.8 `la_axis_fifo` (32-bit x 2048 default, tkeep/tlast transport, count/full outputs, sticky overflow flag with overflow_clr for beats dropped at full by a non-compliant writer; handshake-stall propagation keeps loss never-silent) and P2.7 `la_capture_ctrl` (IDLE/ARMED/CAPT/DRAIN FSM; config latched at arm; burst emission countdown K+POST computed from PRE_SAMPLES and the sample counter, matching the ring's window exactly; POST includes the trigger sample, POST=0 captures the pre window only; timeout on valid samples in burst ARMED; stream mode runs straight into CAPT with timer flushes (LIVE_FLUSH_CYCLES, 0=disabled) and a 65536-word flush in txn mode while raw mode self-closes at the packer; overflow (ring or FIFO) stops the capture, reports the overflow event and drains the partial stream with a flush; DRAIN completes on the closing tlast word or a small no-progress bound so empty/word-aligned endings cannot deadlock; word counters 64-bit from arm through the closing word). FIFO TB checks wrap order/transport, fill with read stalls, non-compliant drop with sticky overflow and clear, 1:1 count accuracy and resets. Capture-ctrl TB emulates ring, encoder and register sides: complete/window-only/empty bursts with exact emission and word counts, timeout, mid-capture abort, overflow stop with drain, stream timer cadence with optional trigger, and the 65536-word txn flush. | Local gates PASS: `lint` and `sim` (9/9 testcases); reports under `build/reports`. RTL fixes found by the TBs: pre_req must latch at arm (regression from refactor), stream mode must enter CAPT at arm or its flush cadence never runs, burst target races avoided by computing K at the trigger edge from the controller's own sample counter instead of sampling ring pre_len. |
