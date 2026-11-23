# IS19 project notes (knowledge base)

Consolidated from the files uploaded on 13 Oct 2026 (W7, break week). Every
fact here points back to its source so it can be checked. Student IDs are
deliberately left out because the module asks for public repos.

| File | What it covers |
|---|---|
| [01_project_and_assessment.md](01_project_and_assessment.md) | Mission, rules, deliverables, grading, dated timeline, what the prof marks down |
| [02_prof_clarifications.md](02_prof_clarifications.md) | Everything said in the briefing, lecture 1 and the consultation that is not in the written briefs |
| [03_hardware.md](03_hardware.md) | Kit inventory (from photos), Robo Pico port map, power tree, our pin map, datasheet numbers |
| [04_design_review_summary.md](04_design_review_summary.md) | Condensed W6 design review (FR/NFR, tasks, statecharts, assumptions) |
| [05_buddy3_line_and_barcode.md](05_buddy3_line_and_barcode.md) | Deep dive on Buddy 3's subsystem: line following and Code 39 decoding |
| [06_review_findings_and_open_questions.md](06_review_findings_and_open_questions.md) | Cross-check of the design against the sources, gaps, risks, questions to ask, Tribunal 1 prep |
| [07_lecture_digest.md](07_lecture_digest.md) | Lecture key facts (W1 to W6) for the W12 quizzes, plus a Barr-C cheat sheet |

## Where we are (13 Oct 2026)

- W6 design review (hurdle) was submitted: `INF2004_Design Review_IS19.pdf`, 30 pages, PDF dated Sat 10 Oct 09:16.
- Now in W7 (break week). Next graded event: **Tribunal 1, Mon 19 Oct, 11:00 to 13:00 (lecture slot, Zoom), 15%**. It uses the W6 report, and the prof will introduce one requirements change that must be incorporated.
- All 14 assumptions in the design (A1 to A14) are still marked Open.

## Source inventory

| Upload | Contents | Relevance |
|---|---|---|
| `INF2004_Design_Review_IS19.zip` | Submitted design review PDF + Fig 1 to Fig 10 PNGs | Our design; the PDF figures match the PNGs |
| `Transcript.zip` | `project transcript.txt` (project briefing), `lecture 1 main zoom transcript.txt`, `lecture 1 part 2 transcript.txt` (embedded C), `embedded prof consultation 1.txt` | Prof's spoken requirements and clarifications |
| `Materials_for_project.zip` | Project Briefing, Project Write-up, Barcode & Line Spec, Barcode Sample, Barr-C 2018, datasheets (Pico W, Robo Pico x2, TCRT5000, IR sensor manual, LM393, HC-SR04, LSM303DLHC, OKY3231/GY-511), Project Mapping.xlsx, 9 alternative-project briefs | Requirements and hardware facts |
| `lectures.zip` | W0 overview, W1 intro, W2.1 GPIO, W2.2 Interrupts, W3.1 DigComm, W3.2 Timers, W3.3 ADC, W4.1 Embedded OS, W4.2 State Machine, Lecture 5 (v2 and v4), Lecture 6, lab simulator HTML pages | Quiz content and the method the tribunals expect |
| `hardware_images_not_all_are_relevant.zip` | 28 photos + `PicoCar.pptx` | Kit identification (see 03_hardware.md for which photos are old-kit parts) |

Not included in the uploads but referenced: `Week_6_Design_Review_Template.docx`
(listed in the materials table of contents), the µT-Kernel template repository
(TEAM.md, `config/config.h`, `cyw43_utk_task`), the Tribunal 1 template, and the
lecture videos (.mp4).

The alternative-project briefs and `Project Mapping.xlsx` only matter for
non-default projects; IS19 is on the default robot car. For the record, the
IS-eligible alternatives were the Ottie eBadge, Secure Configurable Edge
(chain of trust) and Edge Host Watch.
