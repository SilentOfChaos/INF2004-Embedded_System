# 14 — Resume prompt

Paste the block below into a new chat, then upload the 15 Markdown files
(`handoff_markdown.zip`) and, if the new assistant can read ZIPs,
`handoff_artifacts.zip`.

```text
You are taking over the IS19 project for INF2004 (Embedded Systems): a
line-following robot car on a Raspberry Pi Pico W with a Cytron Robo Pico,
running µT-Kernel 3.0 from the course template. The attached files
00_README.md … 14_resume_prompt.md are a handover pack written on
2026-10-16. Read 00_README.md first: it defines the tags ([Verified],
[User-stated], [Reported], [Proposed], [Inferred], [Unknown]) and the
source keys (REPO, EV, TPKG, KB, RPT, PREV, TR, RP, TPL).

Facts to keep straight:
- Code: public GitHub repo purplemushmallow/INF2004_IS19, branch main at
  commit 58f7d61302e686ff2e8c84df1a5016c7807ac99c. Authoritative files:
  TEAM.md (ownership, pins, tasks, interfaces), docs/decisions.md
  (D-01…D-81, open questions in section E, review logs F/G/H),
  app_program/app_config.h (every tunable value and its basis),
  testing/TRACEABILITY.md, testing/*/TEST_LOG.md.
- Status: implemented and host-tested (71/71 host unit tests with gcc and
  clang, with and without sanitizers; 7 firmware build profiles, 0
  warnings). NOTHING has been run on a Pico W or the car. Every
  hardware-dependent value is marked UNVALIDATED. No on-target, manual,
  integration or system test has a result.
- Approval: the user (one team member) answered the design questions and
  decided G15–G17 on 16 Oct. The owners of S3, S4 and S5 have not confirmed
  D-79, D-80, D-81. The team's agreement to the code is unknown. No
  instructor has approved A11 (MQTT hooks) or A14 (Safety pin override);
  A12 (1 ms tick) is unmeasured.
- Open: G1, G2, G4, G5, G11–G14, G18 (decisions §E), plus the items and
  risks in 11_open_questions_and_risks.md.

The user's standing rules (13 Oct, verbatim in 08 §1): unmeasured values
are typical/datasheet starting values labelled UNVALIDATED in central
configuration with their basis, never presented as measured; choose the
simplest behaviour consistent with the report and template, make it a
named setting and record it in docs/decisions.md; implement the approved
improvements; do not silently change requirements; keep a bounded failure
path for barcode decoding; if a choice would materially change
requirements or safety or contradict the report/template, ask instead;
"ask me anything to clarify if u need again, do not make ur own
assumption".

Repository rules: the repo is public — no student IDs, email addresses,
Wi-Fi credentials (config/wifi_secrets.h stays uncommitted), course
materials or transcripts. Only the owner edits a subsystem's folders
(TEAM.md). Never edit or delete old test results; add rows.

Before doing anything, tell me which of the pack's open items you think
I should handle first and ask any question you need. Do not invent
measurements or test results. If you cannot see the repository, say so
and work only from the pack, marking anything you cannot check as
[Unknown].
```
