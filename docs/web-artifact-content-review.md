# Web artifact content review and implementation handoff

Review date: 2026-09-11, Asia/Jakarta.

Artifact: [Inside the RTOS Logic Analyzer](https://rtos-logic-analyzer-explained.linggaaradhanas.chatgpt.site).

This is the historical content review that preceded the corrected overview now
committed as `e7828c7`. Tasks W01-W10 below describe that correction pass and
are retained as its evidence record. The next expansion is specified separately
in `docs/web-artifact-implementation-instructions.md`. No new deployment was
made for either documentation handoff.

## Scope and evidence boundary

- Reviewed the complete page source, metadata and content-related CSS, then compared claims against the register specification, current firmware, tests, plan and dated execution evidence.
- Repository baseline: `4d4434f046068c74da552d257b1bd821ef993d62` (`4d4434f`). The worktree was clean at review start.
- Sites reports one saved version: version 1, source commit `905dc566b9cbb87dfefe12c1e517effbfe751687`, deployment `appgdep_6aa24ea482488191968e65f4375f4d7a`.
- The published source repository's `app/page.tsx` and the parent repository's current `web-artifact/app/page.tsx` have identical Git blob `46203ef7107a926e99d9d51d7a094923cdc4e6e8`. Thus this is a review of the published content, not an unpublished redesign.
- This was source/evidence inspection, not a fresh browser interaction or visual-layout test. CSS observations below are source-based. No FPGA, firmware, bench or throughput tests were rerun.
- Hardware results below are attributed to recorded runs. They are not new measurements from this review.

## Overall assessment

The page has a useful explanatory structure: signal journey, operating modes, RTOS ownership, transport limits, then evidence. Its strongest ideas are the separation of fast acquisition from slower transmission, explicit buffer ownership, and visible overload reporting. Keep that structure and the existing visual identity.

Its principal weakness is evidence accuracy. It understates the working burst/gateway path, overstates external-input verification, advertises the wrong configurable pre-trigger depth, and blends future live functionality with current behavior. The first version was written before substantial P4/P5 bench progress; several problems are subsequent drift, while the pre-trigger number and broad verification labels were incorrect even at first publication.

The explainer also assumes that readers already know what a logic analyzer does. A beginner should first learn that it records digital high/low states over time to debug communication and timing. The technical architecture can then explain how this project accomplishes that.

## Findings, ranked by importance

P1 means a misleading specification or material project-state claim. P2 means a significant explanatory, evidence or readability problem. These priorities apply to the website review, not to a new firmware defect report.

| ID | Priority | Finding and reader impact | Evidence and exact edit location |
|---|---|---|---|
| F01 | P1 | The prominent missing-USB-UART blocker is obsolete. It tells readers that acquisition has never worked on wire, although gateway burst captures now prove that path. | Page `.blocker-note`, line 178. Current `docs/status.md`, P4/P5 phase rows and the entries for `test-e2e`, target Unity and P5.5 loopback; baseline lines 162–164 and 202–205. |
| F02 | P1 | ESP32 is described as missing UART/network adapters and loopback testing. P4 remains partial and P5–P7 are collapsed into “next,” hiding completed bridge work and the already implemented host client. | Page `stages` bridge entry, lines 39–43; `milestones`, lines 54–60. Status snapshot/phase matrix describes P4/P5 bench completion and P6 next. |
| F03 | P1 | “PRE-HISTORY ≤ 64K samples” is not the configurable pre-trigger limit. The register maximum is 32,768; ring storage depth is a separate internal property. A reader could expect twice the supported history. | Page `.scope-metrics`, line 128. `docs/regmap.yaml`, PRE_SAMPLES at baseline lines 117–124; `fpga/rtl/la_regs.sv` clamps writes to the generated maximum at lines 244–247. |
| F04 | P1 | “Released sampling target” implies an existing release and can be read as a demonstrated input rate. 100 MS/s remains the acquisition design target; the full P6 rate/stimulus matrix and external loopback are open. | Page input detail and hero fact, lines 21 and 123. `docs/plan.md` P6.2/P6.3; current status next-session checklist; `git tag --list` returned no tags in this checkout. |
| F05 | P1 | “Bench verified” applies to the whole input stage even though physical JE→JC testing remains open. One badge also cannot express working burst firmware alongside unimplemented live task wiring. | Page `Stage.status`, `StatusPill`, input/FPGA/RTOS/host entries. `tests/e2e/test_zynq_burst.py` explicitly uses the internal self-test mux; status P6/P7 rows distinguish remaining work. |
| F06 | P1 | The live panel and reference-count ownership graphic use present tense without a nearby implementation boundary. Readers may infer that the three decoder tasks and live segment circulation run on target. | Page live panel and ownership card, lines 150–162. `fw-zynq/app/src/tasks/la_cmd_dispatch.c`, lines 300–306 rejects DECODER_CFG, LIVE_START and LIVE_STOP as unsupported. Status P7 says target pipeline open. |
| F07 | P2 | The throughput arithmetic is useful but is not explicitly grouped as a calculated budget. It omits the observed gateway echo ceiling, and a 2 Mbps row can imply that faster baud fixes the current bottleneck. | Page `.rate-card`, line 168; status P5.5 records 44.4 kB/s echo at 921600 and 39.5 kB/s at 2 Mbps with zero CRC errors and FIFO overruns. These are echo measurements, not one-way live-event capacity. |
| F08 | P2 | The proof section foregrounds old timing and local test counts while omitting the strongest current evidence: 7/7 burst E2E, 5/5 ESP32 target tests and a verified million-sample capture. A single “verified” label hides what was actually exercised. | Page `proof` and milestone state rendering. Status baseline lines 157–164. The displayed +0.255 ns belongs to an earlier build; another ledger run records +0.377 ns; the present ignored timing report contains +0.411 ns dated Sep 10 22:49:02. None is a measured sampling-rate guarantee. |
| F09 | P2 | The synthetic waveforms, UART timestamps, segment ID/generation and count can be mistaken for measured telemetry. The 62%-wide reference-count bar has no denominator or defined meaning. | Page `Waveform`, scope label, `.event-stream`, `.ownership-card`; the refcount width is literally `62%` at line 161. The page has no capture connection feeding these displays. |
| F10 | P2 | The introduction never defines a logic analyzer. The five-stage line does not clearly show that FPGA logic and the ARM processor share one Zynq chip on the Zybo, while ESP32 is the second board. “Native Windows” is more prominent than the debugging use case. | Hero and signal-journey copy. `docs/plan.md` Goals, Architecture and Repository Layout describe those roles. |
| F11 | P2 | “Interrupts only wake tasks” is too absolute. UART handlers move bytes through ISR-safe stream-buffer operations and service the UART. “Makes asynchronous edges safe to sample” also overpromises: synchronization addresses clock-domain entry, not unlimited pulse capture or voltage protection. | Page RTOS detail/principles and input description. `fw-zynq/app/src/drivers/la_link_uart.c`, RX at lines 8–28 and TX at 31–45; `docs/rtos-design.md` ISR section; `docs/wiring.md` 3.3 V limits. |
| F12 | P2 | Sources are named but not linked, and the same date covers several materially different hardware revisions. Reusing the first matching sentence in the ledger will produce further errors. | Footer and “Current state” heading. `docs/status.md` still has stale snapshot metadata and a caveat saying the Unity app is unwritten, despite later 5/5 evidence. `docs/architecture.md` retains earlier timing and bring-up notes. |
| F13 | P2 | Key qualifications are rendered too small: status pills and milestone states are 10 px, stage metadata 11 px. Content cannot correct an overclaim effectively if its qualification is hard to read. | `web-artifact/app/globals.css`: `.status-pill`, `.milestone-state`, `.detail-meta`, `.scope-header`, `.mode-sequence`, `.event-stream`. This is a source-based readability finding, not a completed accessibility audit. |

## Evidence rules for the implementation model

Read this section before changing claims. Use the plan for intended architecture and acceptance criteria; use the current source and latest specific dated test evidence for implemented behavior. A phase label is useful context, but it does not prove every possible behavior of that subsystem.

When sources disagree, prefer the most recent specific evidence for the claim. Do not copy historical failures into current status, and do not assume all text within the ledger was refreshed together. In particular:

- Keep +0.255 ns as historical if discussed; do not describe it as the latest result. Prefer removing timing margin from the four headline cards rather than picking a newer number without its exact run provenance.
- Ignore the old statement that ESP32 Unity firmware is unwritten: the source and 5/5 run contradict it.
- “P4/P5 complete on bench” does not mean the project is released, every external pin/rate is validated, or stale-client recovery is fixed.
- The 1 MS/s and 10 MS/s counter captures used internally generated signals. They do not close the external JC input acceptance matrix or the 100 MS/s end-to-end target.
- The ~40 kB/s echo observations suggest a processing limitation, but they do not identify one profiled root cause or establish a universal one-way throughput limit.
- Do not embed local Wi-Fi credentials, host IPs, machine paths, source write credentials or a whole raw status-log dump in the website. Use only the selected non-secret evidence summaries.

## Proposed replacement content

Use this wording as the default. Small grammatical adjustments are fine; keep the scope of each claim intact.

### Hero

Headline: **Capture digital signals. Understand what happened.**

Lead: “A logic analyzer records when digital signals are high or low, helping you debug timing and communication. This project combines a Zybo Z7-10, an ESP32 and a Python host tool. Burst capture works through the wireless gateway; live UART, SPI and I²C decoding on the Zynq is the next major feature.”

Adjacent project focus: “FreeRTOS coordinates acquisition, transmission and buffer ownership.”

Rate label: **100 MS/s design target**.

Qualification: “Recorded counter captures at 1 and 10 MS/s; full rate and external-input validation remain open.”

Scope caption: “Illustrative waveforms — not a connected capture.”

Pre-history value: **Up to 32,768 pre-trigger samples**.

### Signal journey

Heading: **From digital pins to a capture file**.

Context: “FPGA logic and the ARM processor share the Zynq chip on the Zybo. The ESP32 is a separate wireless gateway.”

| Stage | Description | Detail | Evidence label |
|---|---|---|---|
| Inputs | Eight 3.3 V digital inputs enter through JC. Synchronizers reduce metastability risk when signals enter the FPGA clock domain. | Supported pulse capture depends on sampling and input validation. External JE→JC loopback is still pending. Use appropriate level shifting for 5 V signals. | External input validation pending |
| FPGA capture | Hardware samples the inputs, detects the trigger and retains pre-trigger history. Samples are packed raw or encoded as runs of unchanged values. | Ordinary scatter-gather DMA transfers the stream to DDR memory without CPU copying of every sample. Internal counter captures have passed exact-data bench checks. | Internal capture bench verified |
| Zynq / FreeRTOS | Four application tasks manage acquisition, commands, transmission and status. AcqTask controls the DMA descriptors. | Interrupt handlers acknowledge hardware, notify tasks or move UART bytes through ISR-safe buffers. Live decoder task wiring remains open. | Burst core bench verified |
| ESP32 gateway | The ESP32 bridges the flow-controlled UART link to a single Wi-Fi TCP client. Its link, network and watchdog tasks run under ESP-IDF FreeRTOS. | Gateway burst captures, five target Unity tests and the 10 MB loopback tests have passed. Session recovery after a timeout remains an observed issue. | Gateway bench verified |
| Host tools | The Python client configures captures, checks transferred data and exports .sr, VCD or PNG files for analysis. | .sr files are for PulseView; VCD files can be viewed in GTKWave. The live UART command is mock-tested; matching live firmware is not implemented yet. | Burst used on bench; live mock-tested |

The labels are deliberately more specific than a single green “verified” badge. Use the existing three color categories, but let each stage supply its own readable evidence label. If this becomes crowded, use a short label plus a visible qualification immediately below it.

### Burst and live modes

Burst status: “Working on the bench through the ESP32 gateway.”

Burst explanation: “Configure the capture and arm it. The FPGA retains pre-trigger history, records the requested samples after the trigger and finishes the capture packet. The DMA writes into a finite set of memory buffers, and the firmware drains the completed recording to the host.”

Plain-language terms: “A trigger chooses when the event of interest begins. Pre-trigger history lets you inspect the lead-up.”

Packing example: “Raw mode stores every eight-bit sample. Transition mode stores a sample value plus how many sampling intervals it lasted. For example, `00 00 00 01 01` can be described as `(3 × 00), (2 × 01)`. Actual transition records are four bytes each: quiet signals can shrink; rapidly changing signals can expand.”

Live status: “Planned target pipeline; decoder libraries and host UART command already have local tests.”

Live explanation: “The planned live path passes completed segments to UART, SPI and I²C decoder tasks. These tasks will produce timestamped protocol events and release their buffer references when done. Current target firmware rejects live-start and decoder-configuration commands.”

Live latency note: “The design includes a 2 ms FPGA packet-flush deadline so sparse traffic need not fill a large buffer. The 25 ms p99 UART-event latency goal still needs target measurement.”

For a technical expansion only: at extremely low sampling rates, timer expiry waits for the next valid sample; 2 ms is not an unconditional end-to-end latency guarantee. Do not turn that detail into a claimed measured result.

### Ownership example

Caption: “Planned live-buffer ownership — illustrated example.”

Sequence: **Free → DMA owns → CPU consumers own → Free**.

Supporting text: “Suppose UART and SPI decoders both reference one segment. The reference count starts at two. UART finishes: one remains. SPI finishes: zero remains, so acquisition may requeue that buffer.”

Show the discrete count sequence **2 → 1 → 0**. Remove the percentage bar. Explain that this contract has native tests; on-target live integration remains open.

Replace “Interrupts notify. Tasks do the work.” with “Interrupts acknowledge and hand off. Tasks perform acquisition processing and decoding.” Keep the UART ISR detail in the stage explanation above.

### Rates and transport

Label the existing chart **Calculated capacity budget**.

- Raw at 100 MS/s: `100,000,000 samples/s × 1 byte = 100 MB/s`.
- Worst-case transition stream: `100,000,000 records/s × 4 bytes = 400 MB/s`.
- UART at 921600 baud, 8N1: `921600 / 10 = 92,160 B/s`, before framing and flow-control delays.
- UART at 2 Mbps, 8N1: `2,000,000 / 10 = 200,000 B/s`, before framing and flow-control delays.

Keep the first two chart bars and the 2 Mbps theoretical UART bar if desired; state the current 921600 default in text. If the UART bar uses a minimum visible width, label that visual concession explicitly or use a point marker instead of implying exact scale.

Put recorded throughput in a separate small table headed **Measured gateway loopback, 10 Sep 2026**:

| Configured UART baud | Recorded echo throughput | Test result |
|---|---|---|
| 921600 | 44.4 kB/s | 10.07 MB, zero parser CRC/format errors, no FIFO overruns |
| 2 Mbps | 39.5 kB/s | 10.07 MB, zero parser CRC/format errors, no FIFO overruns |

Qualification: “These are round-trip echo measurements through the gateway, not one-way live-event capacity. Raising baud did not improve this test; the relay path needs profiling.”

Practical example: “At the theoretical 2 Mbps UART ceiling, draining 64 MB takes at least 320 seconds before protocol overhead.” Mark it calculated; do not estimate this time from the echo rates.

### Current evidence and next work

Replace the four headline proof cards with:

| Value | Label | Qualification |
|---|---|---|
| 7/7 | Burst E2E bench tests | Internal counter source; multi-buffer, short-tail, abort and capacity cases |
| 5/5 | ESP32 target Unity tests | Relay, queue/notification, ring buffer, UART loopback and watchdog |
| 12/12 | FPGA simulation cases | Procedural, self-checking Questa tests |
| 44/44 | Host tests | Protocol, export and client logic; not all hardware scenarios |

Keep 14/14 native release-precheck evidence as secondary text if useful. Label it “native precheck,” not a product release. A million-sample example is also valuable: “A 1 MS/s raw capture transferred one million consecutive counter samples across 16 DMA descriptors; the recorded run took 25.6 seconds.” Do not turn that elapsed value into an unspecified link-speed benchmark.

Milestones:

| Phase | Visible status |
|---|---|
| P0–P1 | Local tooling and specifications complete |
| P2 | Capture RTL verified in simulation |
| P3 | Integration and board bring-up verified |
| P4 | Burst core verified on bench |
| P5 | Gateway and target tests verified on bench; recovery issue remains |
| P6 | Host client implemented; full burst matrix and external loopback next |
| P7 | Decoder libraries and host UART command locally tested; target live path open |
| P8–P10 | Hardening, performance and release gates remain; boot packaging tooling exists |

Replace “The honest blocker” with **Open work and known limits**:

1. Complete the P6 stimulus/rate matrix at 1, 10 and 100 MS/s, external JE→JC loopback, PulseView evidence and burst release gate.
2. Resolve the observed stale-client behavior after a host timeout; subsequent sessions have required an ESP32 reset in recorded runs.
3. Profile gateway processing and implement/measure the live decode path before advertising live-rate or latency guarantees.

State that a separate USB-UART adapter is an optional diagnostic route because the gateway already exercises the analyzer link.

## Ordered tasks for a lower-cost implementation model

All tasks below are proposals until the user instructs a model to execute them. Do not start them merely by reading this review.

| Task | Priority | Files / anchors | Concrete work | Depends on | Acceptance |
|---|---|---|---|---|---|
| W01 | P1 | `app/page.tsx`: hero, `stages` input entry, `.scope-metrics`; `app/layout.tsx` metadata | Apply the new intro, design-target wording, illustrated-scope caption and exact 32,768 pre-trigger limit. | None | No “released sampling target” or “≤ 64K samples”; no claim that external 100 MS/s operation is proven. Page purpose is understandable before acronyms appear. |
| W02 | P1 | `app/page.tsx`: `Stage`, `StatusPill`, `stages`, signal-journey heading | Supply precise per-stage evidence labels and replacement descriptions. Identify FPGA and ARM as parts of one Zynq. Start the journey at `inputs` instead of `rtos`. | W01 | Every stage states its role and verification boundary. Inputs are not labeled bench verified. ESP32 is no longer described as missing adapters. |
| W03 | P1 | `app/page.tsx`: `milestones`, `.milestone-state`, `.blocker-note` | Replace old phase grouping and obsolete adapter blocker with the proposed milestone table and current open-work list. Do not infer status solely from a generic `done` string. | W02 | P4/P5 working; P6 next; P7 target live open; stale-client and processing-limit caveats visible; optional USB-UART route identified. |
| W04 | P1 | `app/page.tsx`: burst/live `TabsContent` | Add explicit mode availability, correct burst responsibility wording, basic trigger explanation and compact raw/transition example. Use future tense for target live functionality and label demo UART events. | W02 | Current firmware support matches the page. The live command's local/mock coverage is distinct from running target decoders. Example run counts reconstruct exactly five samples. |
| W05 | P2 | `app/page.tsx`: `.ownership-card`, RTOS principles | Label planned ownership, show `Free → DMA → CPU → Free`, replace the arbitrary bar with the discrete 2→1→0 example, and correct ISR wording. | W04 | No unexplained percent; two consumers must release before recycle; no blanket claim that UART ISRs only notify. |
| W06 | P2 | `app/page.tsx`: `.rate-card`; `app/globals.css`: rate/table styles | Separate calculated budget from measured loopback. Add baud/default context, echo qualifications and the calculated drain-time example. | W01 | Units correct; 100/400 MB/s are targets/calculations; 44.4/39.5 kB/s are attributed echo results; no implied lossless live-rate promise. |
| W07 | P2 | `app/page.tsx`: `proof`, evidence section | Use the proposed four headline cards and optional scoped million-sample proof. Remove historical WNS from the headline cards. | W03 | Every metric identifies test type and scope. Seven tests are not described as the full P6 acceptance matrix. |
| W08 | P2 | `app/page.tsx`: snapshot constant and footer/source links | Add a fixed evidence revision and source links. Use one constant for source SHA/date so repeated references cannot disagree. Link precise claims to pinned repository documents or tests. | W03, W07 | “Reviewed 11 Sep 2026; bench evidence recorded 10 Sep 2026; source 4d4434f” is visible. Do not use the viewer's current date as the evidence date. All links have meaningful labels. |
| W09 | P2 | `app/globals.css`; minimal stage markup | Raise essential labels and caveats to at least 14 px (prefer rem), body copy to at least 16 px; allow labels to wrap. Preserve responsive behavior. Remove `role="listitem"` from the native button; if retaining list semantics, put that role on the wrapper. | W02–W08 | Long evidence labels are readable, not clipped. Keyboard-operable stage buttons retain native button semantics. Layout needs browser validation when authorized; do not declare it tested from CSS inspection. |
| W10 | P2 | Site build, diff review, `docs/status.md` | Run the site's required production build once after all changes. Check copy against the checklist below and record actual verification. Follow repository commit/push rules for the implementation; publish only if requested. | W01–W09 | Build passes or exact blocker recorded. No firmware changes, hardware claims upgraded without evidence, secret files staged, or new Site registered. |

The most economical sequence is three batches: W01–W03 correctness; W04–W07 explanation/evidence; W08–W10 provenance/readability/validation. One model can do these sequentially in the same checkout. Parallel edits to this small page are unnecessary.

## Implementation boundaries and verification checklist

Work from the project root containing `AGENTS.md`. Website changes belong in `web-artifact/`; inspect its existing scripts and current applicable Sites skill before building. Preserve the existing framework, dependencies, lockfile, component library, theme and one-page structure. No new packages, remote data loading, telemetry, login features, hardware control, database or generated imagery are needed.

The current request authorizes the review and handoff, not implementation or republishing. A subsequent instruction to execute the change list authorizes those listed source edits. Existing-Site publication remains a separate action unless the user also requests publishing. Reuse `.openai/hosting.json` project ID `appgprj_6aa24a82db78819188d5e0ab9886ffcf`; never register a replacement.

The parent GitHub repository owns the tracked `web-artifact/` source. Its former separate Sites Git metadata was moved to ignored `build/web-artifact-git/.git`. Do not blindly run `git init`, stage an embedded repository, overwrite the parent remote or push the entire firmware repository to Sites. If publication is later requested, resolve this source-repository arrangement under the current hosting skill before obtaining source credentials. No publication work is needed to implement and review the content changes.

Suggested pinned source links, using the observed GitHub remote and baseline:

- `https://github.com/linggaaradhanas22/logic-analyzer/blob/4d4434f046068c74da552d257b1bd821ef993d62/docs/status.md`
- `https://github.com/linggaaradhanas22/logic-analyzer/blob/4d4434f046068c74da552d257b1bd821ef993d62/docs/plan.md`
- `https://github.com/linggaaradhanas22/logic-analyzer/blob/4d4434f046068c74da552d257b1bd821ef993d62/docs/regmap.yaml`
- `https://github.com/linggaaradhanas22/logic-analyzer/blob/4d4434f046068c74da552d257b1bd821ef993d62/tests/e2e/test_zynq_burst.py`

Add section or line anchors after verifying them at the pinned revision. Repository access is subject to the reader's GitHub access. Do not link an ignored local report as if it were a publicly available URL.

Acceptance checklist:

- [ ] Input capture, firmware burst, ESP32 bridge, host burst, host live and target live each have distinct, evidence-supported status.
- [ ] The configurable pre-trigger limit is 32,768, and no normative register/RTL file was edited to fit old copy.
- [ ] 100 MS/s is a design target; full external-input/rate matrix is open; no new release claimed.
- [ ] P4/P5 success is visible; the optional adapter route is not a current blocking dependency.
- [ ] Internal self-test captures are not used as evidence that all physical input channels passed external tests.
- [ ] Live ownership, UART events and waveforms are explicitly illustrative/planned where applicable.
- [ ] No reference-count percentage remains; the 2→1→0 example has named consumers.
- [ ] Calculated and measured rates have separate labels, correct units and declared test scope.
- [ ] Session recovery and throughput profiling remain open; no speculative firmware fix was added.
- [ ] Current evidence cards use 7/7, 5/5, 12/12 and 44/44 with the specified qualifiers.
- [ ] Source revision and evidence dates are pinned; links support the adjacent claims.
- [ ] Body text and necessary caveats are readable, and stage buttons remain operable.
- [ ] Production build completes; any browser checks performed are reported accurately. This review did not perform them.
- [ ] Only website/documentation changes are in the diff; no unnecessary native/RTL/bench reruns for content-only changes.
- [ ] Required ledger reflects the exact commands/outcomes. Follow AGENTS.md for commit/push or explicit WIP; do not publish without a publication request.

## Pasteable instruction for the next model

> Implement tasks W01–W10 in `docs/web-artifact-content-review.md`. Use the supplied replacement content and acceptance checks; this is a content correction of the existing web artifact, not a redesign or firmware task. Read `AGENTS.md` and the current `docs/status.md` first. The review used repository snapshot `4d4434f046068c74da552d257b1bd821ef993d62`; if subsequent project evidence changes any proposed fact, reconcile that specific fact and report the difference. Preserve the site identity, stack, dependencies and visual theme. Complete all three batches, validate the production build, and follow the required status/commit/push workflow. Do not rerun hardware tests for prose edits, add features, or publish a new Sites version unless I separately request publication. Report the changed claims and checks, and leave unresolved evidence gaps explicit.
