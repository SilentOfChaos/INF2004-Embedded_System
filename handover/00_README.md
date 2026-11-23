# 00 — README: IS19 handover pack

This pack hands over the INF2004 (Embedded Systems) group project of team
**IS19** so that another assistant (ChatGPT) or a person can continue without
the Claude conversation that produced it. Read the files in order. Each file
can be read alone, but the tags and source keys below are defined only here.

Written 2026-10-14 to 2026-10-16 (UTC) by Claude, at the user's request:
"Create a detailed, evidence-based handoff pack for this INF2004 Embedded
Systems project …" and "carry on, finish the entire handover and push to main
as "handover" folder" [Verified: TR L4275, L4429]. Files 00–04 were first
pushed unfinished by the user (PR #8, "Incomplete handover (fixing code
first)"); a teammate's review was then reviewed and its fixes merged, and this
version replaces those drafts [Verified: git log `180fc7b`, `cc97bbf`; TR
L5187].

## 1. Orientation in one paragraph

[Verified: REPO] The public repository `purplemushmallow/INF2004_IS19`
(https://github.com/purplemushmallow/INF2004_IS19) holds the IS19 car
firmware on the course's µT-Kernel template: five subsystems plus a shared
Navigation task, host unit tests, test plans, `TEAM.md`, a decisions log
(`docs/decisions.md`, D-01 to D-81) and a traceability matrix
(`testing/TRACEABILITY.md`). The first version was written by Claude in this
session on 13 Oct; a teammate's Claude Code session (Buddy 2's) reviewed it
and prepared 17 fix commits on 13–15 Oct; this session reviewed those
independently on 16 Oct, agreed with all of them, merged them, and then
implemented three decisions the user made (G15, G16, G17). [Verified: EV] On
`main` at `58f7d61302e686ff2e8c84df1a5016c7807ac99c`, all seven firmware build
profiles compile with 0 warnings and 71 host unit tests pass with gcc and
clang, each with and without sanitizers. [Verified: `app_config.h`,
`testing/*/TEST_LOG.md`] **Nothing has been run on a Pico W or on the car.**
Every hardware-dependent value is marked `UNVALIDATED`; every on-target,
manual, integration and system test is defined but has no result.
[Verified: TEAM.md §7] The template exceptions A11 (MQTT hooks) and A14
(Safety pin override) have no recorded instructor approval, and the 1 ms
tick (A12) is unmeasured.

## 2. Tags used in every file

| Tag | Meaning in this pack |
|---|---|
| [Verified: source] | Checked directly against the named evidence in this session (file contents, command output, git history, GitHub API). The source is named. |
| [User-stated] | Said by the user in this session; not independently checked. |
| [Reported: who] | Said by another AI or person (the teammate's review package, the previous handoff pack); not checked here. |
| [Proposed] | Suggested by someone; not accepted or not implemented. |
| [Inferred] | Claude's own conclusion. |
| [Unknown] | Unresolved, or the evidence is not available in this session. |

Status words are kept apart: **proposed → accepted → implemented →
inspected/verified → tested → submitted**. "Implemented" never implies
"tested", and "tested" here means **host unit tests on a PC** unless a file
says otherwise. Three kinds of acceptance are kept apart too: the user (one
team member, who answered the questions in this session), the whole team, and
the instructors. This session has evidence only of the first.

## 3. Source keys

| Key | What it is | Where it is | In the public repo? |
|---|---|---|---|
| REPO | GitHub repo `purplemushmallow/INF2004_IS19`, branch `main`, commit `58f7d61302e686ff2e8c84df1a5016c7807ac99c`, tree `caeb76a31d2d56da058e6f0a8c44fc5b64b686e6`. Paths are relative to the repo root. | GitHub | yes |
| EV | Build and test logs made for this pack: `final_main` (16 Oct, commit `b7bf737`, which differs from `58f7d61` only in `docs/decisions.md`), `reviewed_merge` (16 Oct, `698e39d`), `teammate_v2` (16 Oct, tree `9c3b4113`), `per_branch` and `probes` (16 Oct), `ee9601b` (14 Oct), `template_baseline` (13 Oct), `template_import_check.txt` (16 Oct) | `handoff_artifacts.zip` → `evidence/` | yes |
| TPKG | The teammate's package, uploaded 2026-10-16: `IS19_Independent_Review_Actions.md`, `IS19_consistency_PRs_v2.zip` (17 patches, PR texts, `open_prs.sh`, evidence), `INF2004_IS19_corrected_v2.zip` (merged tree) | User's uploads | its commits are in REPO history; the zips are not |
| KB | Knowledge base `docs/notes/01`–`07`, written by Claude in this session from the uploads (PR #1) | REPO `docs/notes/` | yes |
| RPT | `INF2004_Design Review_IS19.pdf`, the team's submitted W6 design review, 30 pages, sha256 `98bda1250182eb7ff9253dd342d94419bbc1e198b52dc106c021d2763c08c14f`; quoted from a `pdftotext` extract | User's upload | **no** (lists student IDs) |
| PREV | Previous handoff pack `INF2004_final_merged_handoff_1.zip` (`00_README.md` … `10_final_content.md`), from another chat before coding | User's upload | **no** (lists student IDs) |
| TR Lnnnn | This Claude session's transcript (JSONL), 1-based line number | Container only | **no** (private conversation) |
| RP | `Robo_Pico.pdf` (Cytron Robo Pico datasheet) from `Materials_for_project.zip` | User's upload | no (course material) |
| TPL | The course template `INF2004_ProjectTemplate-main.zip`; the user gave its URL as https://github.com/sirfonzie/INF2004_ProjectTemplate | User's upload; imported unchanged as REPO commit `e870827` | yes (as imported) |

Hashes of everything that is not in the public repository: 03 §4.

## 4. Files in this pack

| File | Content |
|---|---|
| `00_README.md` | This file |
| `01_requirements_and_sources.md` | A. Project and assessment context; FR1–FR19, NFR1–NFR19, A1–A14 verbatim from RPT |
| `02_current_state.md` | Where everything stands; the implementation-status table |
| `03_artifact_and_version_inventory.md` | Every file and version, which is authoritative, hashes, exclusions |
| `04_hardware_and_wiring.md` | B. Pins, Robo Pico port table with PDF page numbers, power, kit, UNVALIDATED values |
| `05_software_architecture_and_interfaces.md` | C. Tasks, priorities, cores, kernel objects, IF-01…IF-18, message formats, builds |
| `06_subsystem_behaviour.md` | D. What each subsystem's code does; Appendix A: `app_config.h` verbatim |
| `07_safety_and_lifecycle.md` | E. System modes, fault latch, pin override, watchdog, RESET handshake, fault causes |
| `08_changes_decisions_and_rationale.md` | F. The user's answers and rules, instructor clarifications, D-01…D-81 with status, the review rounds; Appendix A: `docs/decisions.md` verbatim |
| `09_corrections_disagreements_and_dead_ends.md` | Errors found and fixed, review disagreements, superseded facts, rejected approaches |
| `10_verification_and_test_evidence.md` | G. Exactly what was built and tested, with numbers, logs and limits |
| `11_open_questions_and_risks.md` | Every open question and finding, each with a disposition |
| `12_next_steps.md` | Next steps already named in the sources (none added) |
| `13_finalised_content.md` | Wording that was finalised (report fixes, the user's rules and answers, commit trailers) |
| `14_resume_prompt.md` | Text to paste into a new chat |
| `handoff_markdown.zip` | The 15 Markdown files |
| `handoff_artifacts.zip` | Public-safe supporting files: evidence logs, probes, scripts, helper programs, labelled superseded versions, manifest |

## 5. Authoritative files at a glance (details in 03)

| Purpose | Authoritative file | Tag |
|---|---|---|
| Requirements, assumptions, W6 design | RPT (contains the four post-submission fixes, 13 §1) | [Verified: RPT text; TR L1073] |
| Condensed design | `docs/notes/04_design_review_summary.md` (RPT wins on wording) | [Verified: REPO] |
| Ownership, pins, tasks, interfaces, object budget, template exceptions | `TEAM.md` | [Verified: REPO] |
| Every design choice, change and review | `docs/decisions.md` (D-01…D-81; §E open questions; §F, §G, §H review logs) | [Verified: REPO] |
| Requirement → design → code → test | `testing/TRACEABILITY.md` | [Verified: REPO] |
| Every tunable value and its basis | `app_program/app_config.h` | [Verified: REPO] |
| Kernel configuration changes | `config/config.h` | [Verified: REPO] |
| Test definitions and (empty) results | `testing/*/TEST_LOG.md`, `testing/README.md` | [Verified: REPO] |
| Host unit tests | `testing/host/` | [Verified: REPO] |
| Build and test results | `handoff_artifacts.zip` → `evidence/` | [Verified: EV] |

## 6. What this pack cannot give you

- [Verified: TR L1404, L2648, L4436] The session's context was compacted
  three times (2026-10-13 09:38:30Z, 10:46:00Z, 2026-10-14 01:20:29Z). Facts
  here were re-checked against files, git, fresh command output and the raw
  transcript where possible. Recovery is **not exhaustive**: reasoning that
  was never written to a file or message may be lost.
- [Verified: PREV `00_README.md`] The report DOCX PREV inspected,
  `INF2004_Week6_Design_Review_IS19_final (10).docx`, was never available
  here; the PDF (RPT) was used.
- [Verified: EV, test logs] No on-target (`make TEST=1` on a Pico), hardware,
  timing, CPU-load or run-time RAM measurement exists. RAM figures are
  estimates from the linker map.
- [Verified: GitHub API] No teammate reviewed PRs #2–#7 on GitHub. The
  teammate review of 13–15 Oct (TPKG) happened outside GitHub, in Buddy 2's
  Claude Code session; its commits were merged here without a pull request,
  on the user's choice "Merge branch into main, no PRs" [Verified: TR L5713].
- [Unknown] Whether the other members have read or accepted the code, the
  decisions of 16 Oct (G15–G17), or the review results.
- [Unknown] Any instructor answer (A11, A12, A14, the KB 06 questions).
- [Unknown] Anything that happened outside this session after 2026-10-16.

## 7. Privacy rules followed in this folder

[Verified: `docs/notes/README.md`] The repository is public and the KB's rule
is "Student IDs are deliberately left out because the module asks for public
repos". This folder contains no student IDs, no email addresses, no Wi-Fi
credentials (`config/wifi_secrets.h` was never committed), no course
materials, no lecture or consultation transcripts, and neither RPT nor PREV
nor the teammate's zips. Members' names already appear in `TEAM.md`.
[Verified: git log] Note: the 17 teammate commits keep their author line
(Buddy 2's name and commit email) in the public git history, as his package
asked ("so each commit keeps its author").
