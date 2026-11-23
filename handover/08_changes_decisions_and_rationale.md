# 08 — Changes, decisions and rationale (coverage F)

Tags and source keys: see `00_README.md`. Three kinds of acceptance are
kept apart: **the user** (the team member running this session), **the
team**, **the instructors**. This session has evidence only of the user's.

## 1. The user's answers that govern the code (13 Oct 2026)

[Verified: TR L1073 (questions), L1081 (answers)] Claude asked seven
questions before writing code. The questions, condensed (option letters
and wording of the options kept):

1. Template repo: (a) send its GitHub URL, (b) upload it as a zip, (c) write against standard µT-Kernel 3.0 now.
2. Where the firmware goes: (a) this repo, restructured to the template layout, notes moved into a docs folder; (b) a new repo; (c) elsewhere.
3. Hand-over: (a) one PR with everything from your account; (b) one PR per buddy, so each teammate reviews and merges their own.
4. Unmeasured hardware values: (a) marked placeholders, car won't start until filled; (b) "Use datasheet or typical starting values, each marked "unvalidated", so you can bench-test straight away."
5. Behaviour the report leaves open: (a) ask each one; (b) "take the simplest option that fits the report, make it a named setting, and list every one in a decisions file for the team to approve before merging."
6. Suggested design changes (ignore barcode reads while turning at a junction, filter glitch edges, classify wide/narrow per character, decode on the 30th edge): (a) code the report exactly; (b) "include them and record each change in TEAM.md"; (c) item by item.
7. Host unit tests: (a) yes; (b) no / later.

The user's answer, verbatim:

> 1) https://github.com/sirfonzie/INF2004_ProjectTemplate
> 2) a
> 3) **b
> 4) b
> 5) b
> 6) b
> 7) a
> Additional rules:
> For 4(b), typical/datasheet starting values are explicitly authorised by me, but every unmeasured value must be clearly labelled UNVALIDATED, kept in central configuration, and documented with its source/basis. Do not present any of them as measured values.
> For 5(b), choose the simplest behaviour consistent with the final report and template. Make every adjustable choice a named setting and record every design choice in a decisions file. If two choices materially change requirements/safety or contradict the report/template, ask me instead.
> For 6(b), implement the proposed improvements and record every architecture change in TEAM.md/the decisions log. Do not silently change requirements. For barcode completion, immediate decoding once the expected complete symbol has been captured is fine, but retain a bounded timeout/failure path rather than relying solely on an exact edge count.
>
> ask me anything to clarify if u need again, do not make ur own assumption

(The answer message also attached the template zip.)

How the code follows them [Verified: REPO]:

| Rule | Where it shows |
|---|---|
| 4(b) UNVALIDATED, central, with basis | `app_config.h` tags (04 §5); rule text at the top of the file |
| 5(b) simplest behaviour, named settings, decisions file | `docs/decisions.md` D-01…D-81; open choices in §E |
| 5(b) "ask me instead" | G15–G17 put to the user on 16 Oct (§4 below); G11–G14, G18 still listed as open, code keeps the report's behaviour |
| 6(b) improvements, recorded | D-26, D-27, D-53, glitch filter in D-25; TEAM.md §7 |
| 6(b) bounded failure path | D-26: 120 ms / 64 edges / 2000 ms |
| 3(b) one PR per buddy | PRs #2–#7 on GitHub (13 Oct), merged by the user; [Verified: GitHub API at the time, see 00 §6] no teammate reviewed them on GitHub |
| 5(b) "for the team to approve before merging" (wording of Claude's option) | [Unknown] team approval; the PRs were merged by the user |

## 2. Later instructions from the user

| Date | Instruction (verbatim) | Effect | Tag |
|---|---|---|---|
| 13 Oct | "just push all into main first, i will answer ltr" (about G1, G2, G4, G5 and the GitHub usernames) | code merged with those questions open; current behaviour recorded in §E | [Verified: TR] |
| 13–14 Oct | the hand-off pack request (rules in 00) and "carry on, finish the entire handover and push to main as "handover" folder" | this folder | [Verified: TR L4275, L4429] |
| 16 Oct | "from my teammate's claude opus 5.5 max, if u agree, fix it, if disagree, give rationale, act as an independent reviewer. do not blindly follow." | independent review, decisions §H | [Verified: TR L5187] |
| 16 Oct | "Merge branch into main, no PRs"; G15 "Fault after 4 failed reads (Recommended)"; G16 "Unseen side = no room (Recommended)"; G17 "Overrange = collision (Recommended)" | D-79, D-80, D-81; merged to `main` directly | [Verified: TR L5713] |

## 3. Instructor clarifications that shaped decisions

[Verified: KB 02 `docs/notes/02_prof_clarifications.md`, itself
[Reported] from auto-generated transcripts] Those that the code relies on:

| Clarification (KB 02 wording) | Used in |
|---|---|
| "Barcode is on either the left or the right of the line; the side is fixed" / "Hump is on the opposite side" | not assumed in code; KB 06 question still open (04 §6) |
| "The barcode always comes before the junction; act only when the junction is reached" | held letter, act at junction (06 §1) |
| Default mapping "A = forward, B = left, C = right, D = U-turn" | `letter_map.c` default (FR6, A7) |
| "All of A–Z must be decodable, and the map must be changed over Wi-Fi with no recompiling" | `remap` command (D-66) |
| "the decoder must work in both directions" | `bc_decode.c` reversed decode |
| "Never decode only once; if a read fails, keep trying on the next barcode" | a failed read never clears a held command (D-53); G4 (reverse and re-read) open |
| Obstacle "On the line, and only on a straight section" | rectangular bypass (D-36) |
| End of run: "search (for example about 5 s, or a full 180° sweep) and stop" | 5 s search → COMPLETE (FR18, D-33) |
| "Once the car is placed and switched on it cannot be touched, except to restart" | START button only (G2) and KB 06 question "whether pressing START after placing the car is allowed" (open) |
| IMU: "magnetometer (digital compass), not a gyroscope"; "Combine encoder and IMU heading" | D-40 heading fusion; D-77 compass turn rate |
| "Hump height from tilt: tilt the car to known heights … build a mapping" | hump table, UNVALIDATED (A4) |
| "Safety task data for stopping also needs to come from the ultrasonic" | OBST_AHEAD stops the car via Navigation (report design) |
| "Integer sizes … floats for integer data are penalised" | fixed-point maths (D-13) |
| "AI … Use it freely … but own every line" | each owner must review their part (W8-4 in TPKG; 12) |

No instructor has seen or approved this code, A11, A12 or A14.
[Unknown: no evidence in this session]

## 4. Review rounds

| Round | Who, where | What | Outcome | Record |
|---|---|---|---|---|
| 13 Oct self-review | Claude, this session | code against RPT, template, the user's rules | fixes before PRs #2–#7 | TR only (not a section of the decisions log) |
| 13 Oct consistency review | teammate's Claude Code session (Buddy 2's account) | code vs report, rules, TEAM.md | 12 patches: D-71…D-77, quiet zone off, robust on-target checks, TRACEABILITY | decisions §F |
| 14 Oct independent review | an outside reviewer's list ("feedback" items OA, S, I-01…I-10, C, D3, W8, E), checked in the teammate's session | I-01, I-05, I-06 fixed (D-78, D-53, host Makefile); I-02/03/04/10 raised as G15–G18 | 5 more patches (17 in total) | decisions §G; TPKG §2 |
| 16 Oct second independent review | Claude, this session, at the user's request | all 17 patches re-applied, rebuilt, probed | all 17 agreed; 2 wording corrections, 2 notes; G15–G17 decided by the user and implemented | decisions §H; 09 §2 |

[Unknown] Who wrote the 14 Oct "feedback" list that TPKG answers (TPKG
calls it "the reviewer"); the list itself was not uploaded here.

## 5. Status of every decision D-01 … D-81

"Implemented" = in the code at `58f7d61`. Nothing is "tested" on the car.
Host-tested items are listed in 10. Origin "the user's rules" means Claude
made the choice under the 13 Oct answers above; it does not mean the user
reviewed that particular choice.

| D | Title | Section | Origin | Status |
|---|---|---|---|---|
| D-01 | Repository layout | A | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-02 | Navigation folder | A | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-03 | Common helpers | A | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-04 | Central configuration | A | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-05 | 1 ms tick | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; **unmeasured** (A12) |
| D-06 | Priorities and cores | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-07 | Cyclic release | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-08 | Interrupt hand-off | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-09 | One GPIO interrupt dispatcher | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-10 | Line ADC | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-11 | IMU I2C | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-12 | NeoPixel | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-13 | Fixed point | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-14 | MQTT hooks (A11) | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented, switchable; **instructor agreement pending** (A11/A14) |
| D-15 | Safety pin override (A14) | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented, switchable; **instructor agreement pending** (A11/A14) |
| D-16 | Broker change (NFR14) | B | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-17 | Kernel object limits | A | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-18 | Coding standard | A | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-19 | Stacks | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-20 | Timing probe | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-21 | Button debounce | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-22 | Junction re-arm | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-23 | Level adaptation | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b); review finding G7 (KB 06) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-24 | ADC self-test range | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-25 | Barcode capture start | C | Claude 13 Oct (quiet zone 120 ms) → changed by teammate 13 Oct to first-bar start, quiet zone off | implemented (off); G14 open |
| D-26 | Capture limits | C | approved improvement (Q6 b), Claude 13 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-27 | Wide/narrow classification (approved improvement) | C | approved improvement (Q6 b), Claude 13 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-28 | Encoder decoding | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-29 | Motor PWM | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-30 | Soft start | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b); review finding G9 (KB 06) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-31 | Moves and turns | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-32 | Line-following speeds | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-33 | Search pattern | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-34 | U-turn direction | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-35 | Newer letter | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-36 | Obstacle bypass | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-37 | Stop settle | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-38 | Hump timeout | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-39 | Self-test time | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-40 | Heading fusion | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-41 | IMU calibration | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-42 | Bad IMU data | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b); amended by teammate (D-76) and 16 Oct (D-81) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-43 | Heading events | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-44 | Watchdog | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-45 | Pin override switch | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-46 | Acknowledgement wait | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-47 | Alarm | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-48 | Scan thresholds | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-49 | Message size | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-50 | Publish pump | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-51 | Route log | C | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-52 | Brief line loss | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-53 | Events while turning (approved improvement) | D | approved improvement (Q6 b), Claude 13 Oct; extended by teammate 13 and 14 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-54 | Letters without an action | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-55 | Keepalive | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-56 | Turns | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-57 | Health and SELF-TEST | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded; open question G11 |
| D-58 | STEADY class | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded; open question G13 |
| D-59 | Queue depths | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-60 | Route log replay | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-61 | Ultrasonic readings | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-62 | Obstacle profile | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-63 | Stored line levels | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-64 | Mode exits | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded; open question G12 |
| D-65 | Buttons | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-66 | Command format | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-67 | Hump reports while turning | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-68 | Full queues | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-69 | Console echo | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-70 | Reconnect back-off | D | Claude, 13 Oct, under the user's rules (Q4 b / Q5 b / Q6 b) | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded; Wi-Fi rejoin unchanged (G5, §E) |
| D-71 | Following speed range (NFR17) | D | teammate's Claude Code session, 13–14 Oct; reviewed and agreed by Claude 16 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-72 | Obstacle profile event (FR10) | D | teammate's Claude Code session, 13–14 Oct; reviewed and agreed by Claude 16 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-73 | Barcode input pull | D | teammate's Claude Code session, 13–14 Oct; reviewed and agreed by Claude 16 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-74 | Side scan uses valid readings only (report 6.5) | D | teammate's Claude Code session, 13–14 Oct; reviewed and agreed by Claude 16 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-75 | Ping gap (NFR5) | D | teammate's Claude Code session, 13–14 Oct; reviewed and agreed by Claude 16 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-76 | Bad IMU data while calibrating (report 6.4) | D | teammate's Claude Code session, 13–14 Oct; reviewed and agreed by Claude 16 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-77 | Turn rate on flat ground (report 6.4, FR17) | D | teammate's Claude Code session, 13–14 Oct; reviewed and agreed by Claude 16 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-78 | Barcode symbol boundary (14 Oct) | D | teammate's Claude Code session, 13–14 Oct; reviewed and agreed by Claude 16 Oct | implemented; host-tested where a test is named; not run on the car; team/instructor approval not recorded |
| D-79 | Line-sensor read failures (G15, decided 16 Oct) | D | user decision 16 Oct (G15, recommended option), implemented by Claude | implemented, host-tested; **owner (S3) to confirm** |
| D-80 | Scan coverage (G16, decided 16 Oct) | D | user decision 16 Oct (G16, recommended option), implemented by Claude | implemented, host-tested; **owner (S5) to confirm** |
| D-81 | Accelerometer overrange and jolts (G17, decided 16 Oct) | D | user decision 16 Oct (G17, recommended option), implemented by Claude | implemented, host-tested; **owner (S4) to confirm** |

---

## Appendix A — `docs/decisions.md` verbatim (REPO `58f7d61`)

Relative links inside it point into the repository.

````markdown
# IS19 decisions log

Every choice the W6 report ([summary](notes/04_design_review_summary.md))
left open, every approved improvement, and every change to the template is
recorded here. The code refers to these entries as `D-nn`.

Rules for this code, given on 13 Oct 2026 to the Claude Code session that
wrote its first version (a team member's instructions; the whole team
has not confirmed them yet):

- Values that have not been measured on the car are typical or datasheet
  starting values. Each one is marked **UNVALIDATED** in
  [`app_program/app_config.h`](../app_program/app_config.h), kept there,
  and given its source. None of them is a measured value.
- Where the report leaves a choice open, the simplest behaviour consistent
  with the report and the template is used. Each such choice is a named
  setting where it can be adjusted, and is recorded below.
- The approved improvements (ignore barcode reads while turning at a
  junction, filter glitch edges, classify wide/narrow per character, decode
  as soon as the symbol is complete with a bounded timeout as well) are
  implemented and recorded below.
- Report-defined pins, task owners, periods, priorities, cores, safety
  behaviour and interfaces are not changed. Items that need the instructor
  (A11, A12, A14) are implemented as the report describes and stay marked
  pending.
- A choice that would materially change a requirement or safety, or
  contradict the report or the template, is asked, not made in the code.
  Until it is decided the code keeps the report's behaviour where the
  report gives one, says which reading it follows where the report can
  be read two ways, and the question is listed in section E.

The code was checked against the report and these rules on 13 Oct 2026;
what was found and changed is in section F. An independent review on
14 Oct, and what changed after it, is in section G. A second
independent review of those changes, on 16 Oct, is in section H.

Tags used in `app_config.h`: `[REPORT]` from the W6 report,
`[DATASHEET]` from a datasheet, `[TEMPLATE]` from the course template,
`[DECISION D-nn]` this log, `[UNVALIDATED]` to be measured.

## A. Repository and process

**D-01 Repository layout.** This repository was restructured to the course
template's layout (`sirfonzie/INF2004_ProjectTemplate`); the earlier
planning notes moved to `docs/`. One pull request per buddy so each owner
reviews their own code; shared code goes in a base pull request first.

**D-02 Navigation folder.** The report makes Navigation a shared task, so it
lives in `app_program/navigation/`, not in a buddy's folder. Buddy 3's
steering and letter map stay in `subsystem3/` and Navigation calls them.
The template makefile already builds `app_program/*/*.c`.

**D-03 Common helpers.** Code every subsystem needs (fixed-point maths,
GPIO interrupt dispatcher, PWM set-up, time, register addresses) is in
`app_program/common/`, owned by the team.

**D-04 Central configuration.** Every tunable value is in
`app_program/app_config.h`, one section per subsystem, each tagged with
its source. Every value that describes the car, its sensors or the course,
or that has to be tuned on the car, is tagged `[UNVALIDATED]` until it is
measured, also when a datasheet or a decision gave the starting value.
Register values, bus addresses, protocol timings and the scale factors
that follow from a register setting are datasheet definitions, not
measurements, and keep `[DATASHEET]` alone.
Relations the report fixes are checked when compiling (`#error`): every
period is a whole number of ticks and `CFG_TICK_MS` equals the kernel's
`CNF_TIMER_PERIOD` (D-05), cruise and hump speeds lie inside NFR17's range
(D-71), and the scan period leaves NFR5's ping gap (D-75).

**D-17 Kernel object limits.** `CNF_MAX_MBFID` raised from 12 to 20 in
`config/config.h`; IS19 uses 14 message buffers (TEAM.md section 6).

**D-18 Coding standard.** IS19 files follow Barr-C: `g_` for module
globals, `p_` pointers, `b_` booleans, 4-space indentation, constants on the
left of `==`, `/*** end of file ***/`. Exceptions: template files and
template-style files keep their own style (`app_main.c`, `app.h`,
`test_report.h`, the library files A11 extends), and exported kernel-object
IDs keep the template's interface names (`mbf_xxx`, `flg_xxx`).

## B. Kernel and architecture

**D-05 1 ms tick.** `CNF_TIMER_PERIOD` 1 ms instead of the template's 10 ms,
so 5 ms periods are whole ticks (report section 3). Assumption A12 (CPU
≤ 60%, 5 ± 1 ms) must still be checked on the car. `CFG_TICK_MS` mirrors it.

**D-06 Priorities and cores.** The report's task table (priorities 5–12,
cores) is used as written instead of the template's default bands.

**D-07 Cyclic release.** Each periodic task waits on an event-flag bit set
by a cyclic handler, so its period does not drift with its run time.

**D-08 Interrupt hand-off.** Interrupt handlers only time-stamp or count:
barcode edges and ultrasonic echoes go into message buffers with `TMO_POL`;
encoder counts are one of the report's two justified shared variables. No
printing in interrupts (NFR15).

**D-09 One GPIO interrupt dispatcher.** All GPIO interrupts share IO_BANK0
(IRQ 13), so `common/gpio_irq.c` owns it and calls per-pin handlers. Its
NVIC line is enabled by the Navigation task, which runs on core 0, so every
GPIO interrupt is served on core 0 (report section 3). Each event is
acknowledged before its handler runs; passes per interrupt are bounded.

**D-10 Line ADC.** The kernel's `adca` device driver (already enabled in the
template) is used, owned by the Line task.

**D-11 IMU I2C.** A small polled I2C1 driver (`subsystem4/drv_i2c1.c`) with
every wait bounded, instead of switching on the kernel's IIC device, which
would change shared configuration.

**D-12 NeoPixel.** Driven through the SDK's PIO code in its own file
(`drv_neopixel.c`), which may not include the kernel headers (a `size_t`
clash). It uses the SDK include paths of the Wi-Fi build; without them it
compiles to a stub.

**D-13 Fixed point.** No floating point (Barr-C; the build has no maths
library): CORDIC `atan2`, Q15 sine table, integer square root. Units: centi-
degrees, mm, mm/s, mg. The CORDIC scales small inputs up first, so its
accuracy (about 0.01 degree) holds for any input size (host test COM-U02).

**D-14 MQTT hooks (A11).** The MQTT code calls hooks with weak defaults that
keep the template's behaviour; subsystem 1 defines them in
`mqtt_bridge.c`. They only use message buffers with `TMO_POL`: IF-13
publish, IF-14 commands, IF-15 broker. A 10 ms pump publishes queued
messages, keeps a message lwIP has no room for (`ERR_MEM`) and retries it,
and drops messages while the session is down. **Pending instructor
agreement.**

**D-15 Safety pin override (A14).** As the report describes: on a fault
Safety sets OUTOVER = low and OEOVER = enable on GP8–GP11 in one register
write each, and clears them only after Motion acknowledges it has zeroed
its commands. `CFG_SAFETY_PIN_OVERRIDE` 0 leaves braking to Motion alone.
**Pending instructor agreement.**

**D-16 Broker change (NFR14).** The simplest way with no reflash: an MQTT
command `{"cmd":"broker","ip":"…","port":…}`. The new address is kept in
RAM only, so a reset goes back to `config/wifi_secrets.h`. The command is
acknowledged first and applied `CFG_CMD_BROKER_DELAY_MS` (300 ms) later,
so the ack still leaves on the old broker. Limitation: it can only be sent
while the car is reachable on its current broker (see G1).

## C. Values chosen (all in `app_config.h`)

**D-19 Stacks.** 2048 bytes (template default) except Navigation and
Telemetry, 3072, for the larger state machine and the text formatting.

**D-20 Timing probe.** Optional pin the Line task toggles every cycle, for
checking A12 on a logic analyser. Off (−1); GP6 is the spare pin to claim.

**D-21 Button debounce.** 30 ms stable level (see D-65).

**D-22 Junction re-arm.** After a junction, 4 samples with at least one
sensor not fully dark before another junction can be reported: one event
per crossbar.

**D-23 Level adaptation.** A reading in the dark band (≥ 850‰) pulls that
sensor's black level 1/64 of the way towards it; one in the white band
(≤ 150‰) pulls the white level. Levels stay within ±600 counts of the
stored values; if they come within 400 counts of each other the stored
values are restored. Frozen while crossing a hump through IF-11 (review
finding G7; the report itself only says the levels adapt), switchable with
`CFG_NAV_HUMP_FREEZE_LEVELS`.

**D-24 ADC self-test range.** Readings outside 5–4090 at start-up mean a
floating or shorted input.

**D-25 Barcode capture start.** As report 6.3: WAITING ends at the first
bar (`CFG_BC_QUIET_MS` 0). The first version also required 120 ms of white
before a capture could start (a quiet zone). That was not one of the
approved improvements and could miss a barcode with less white before it,
so it was switched off on 13 Oct; a positive `CFG_BC_QUIET_MS` still
selects it, and whether to use it is open question G14. Edges closer than
1 ms cancel each other (approved improvement: glitch filter).

**D-26 Capture limits.** 64 edges or 2000 ms end a capture as failed, as
does 120 ms without an edge. With decoding at the 30th edge (approved
improvement), these keep a bounded failure path: a capture never waits for
an exact edge count.

**D-27 Wide/narrow classification (approved improvement).** Per character,
bars and spaces separately: `*` and every letter have exactly 2 wide bars of
5 and 1 wide space of 4 in either reading direction, so the 2 widest bars
and the widest space are wide, each at least 1.5 times the widest narrow
element of its kind. This tolerates a speed change along the barcode and a
comparator that widens bars and narrows spaces (±40%, design-note test
UT-B3-05, host test S3-U04). The first version (3 widest of all 9
elements) failed that test and was replaced. `CFG_BC_CLASSIFY_PER_CHAR` 0
selects the report's rule (wide if more than twice the narrowest element).

**D-28 Encoder decoding.** Rising edges of channel A only (lowest interrupt
load, A8); `CFG_ENC_QUADRATURE_X4` 1 counts all edges. Counts per turn must
be measured in the chosen mode.

**D-29 Motor PWM.** 10 kHz, half the driver's 20 kHz maximum.

**D-30 Soft start.** Duty may rise by at most 100‰ per 20 ms cycle, to
limit the current step on the power bank (review finding G9, A6); slowing
down is immediate. `CFG_MOTOR_RAMP_PM` 1000 turns it off.

**D-31 Moves and turns.** Encoder-counted moves at 150 mm/s, pivot turns at
120 mm/s, half speed for the last 20 mm.

**D-32 Line-following speeds.** Cruise 250 mm/s, hump 150 mm/s, within
NFR17's 150–350 mm/s.

**D-33 Search pattern.** Sweep 45° towards the side the line was last seen,
90° back across, 45° back to the centre, repeated until the 5 s limit. The
same sweep looks for the new line after a junction turn (3 s limit, report
section 5).

**D-34 U-turn direction.** Left.

**D-35 Newer letter.** A newer letter read before the junction replaces the
held one.

**D-36 Obstacle bypass.** Rectangular path: turn 90° to the chosen side;
drive sideways to the obstacle's edge plus a 75 mm gap plus half the car
(FR11: pass 5–10 cm away); turn back parallel; drive until the side scan
reports the obstacle passed (at most 800 mm); 150 mm more for the rear of
the car; turn in; drive up to 300 mm to find the line (report 6.5); put
the axle on the line and turn along it.

**D-37 Stop settle.** 200 ms after stopping before the scan starts.

**D-38 Hump timeout.** Leave HUMP CROSSING after 5 s if the IMU never
reports the hump passed.

**D-39 Self-test time.** 3 s for every sensor to report healthy; then
FAULT (report section 5).

**D-40 Heading fusion.** Odometry heading, nudged 2% towards the compass at
each IMU heading update on flat ground only (the tilt distorts the compass
on a hump).

**D-41 IMU calibration.** 100 samples (1 s) at rest.

**D-42 Bad IMU data.** 5 samples in a row of invalid data (a failed read,
or a total acceleration below 300 mg) latch a fault (report 6.4), in
CALIBRATING as well as MONITORING (D-76). A total above 3500 mg is not
invalid data but a jolt (D-81, decided 16 Oct).

**D-43 Heading events.** Every second IMU cycle (20 ms).

**D-44 Watchdog.** 200 ms timeout. Armed only after the radio driver
reports its start-up complete, or after 15 s (A1). Check-in deadlines:
Line 50 ms, Navigation 50 ms (10 periods), Motion 100 ms, IMU 50 ms, Scan
325 ms (5 periods).

**D-45 Pin override switch.** See D-15.

**D-46 Acknowledgement wait.** Safety waits up to 500 ms for Motion's zero
acknowledgement after RESET, then stays latched.

**D-47 Alarm.** Buzzer at 2 kHz and red LED, 250 ms on and 250 ms off,
while a fault is latched; LED green when released; brightness 32 of 255.

**D-48 Scan thresholds.** During a scan, returns within 500 mm belong to the
obstacle. While bypassing, the servo looks sideways (180° or 0°); the
obstacle counts as passed once it has been seen beside the car and then 2
valid side readings (`CFG_SCAN_BEHIND_COUNT`) are above 400 mm (D-74). One
ping is skipped after every servo move.

**D-49 Message size.** JSON payloads up to 320 bytes. lwIP builds each
PUBLISH in its MQTT output buffer; the default 256 bytes is smaller than a
worst-case telemetry record (259 bytes of JSON plus topic and header), so
`MQTT_OUTPUT_RINGBUF_SIZE` is 512 in `lwipopts.h` (A11). A message that does
not fit is dropped and counted, never sent cut short (host test S1-U08).

**D-50 Publish pump.** Every 10 ms, at most 4 messages per call
(`MQTT_UTK_PUMP_MS`, `MQTT_UTK_PUMP_BURST` in `mqtt_utk.c`); one more lwIP
timeout slot for the pump's timer (`MEMP_NUM_SYS_TIMEOUT`).

**D-51 Route log.** 48 entries in RAM, cleared at each START.

## D. Behaviour

**D-52 Brief line loss.** While the line is not seen, steering keeps the
last error, so the car keeps turning towards where the line was.

**D-53 Events while turning (approved improvement).** Barcode and junction
events count only in LINE FOLLOWING and HUMP CROSSING: the junction
crossbar also passes under the barcode sensor while turning (review
finding G3). A read that starts while the car is not following stays
ignored to its end, even if its letter or failure arrives after following
has resumed (added 13 Oct, host test NAV-U10). A read counts only if the
car was following from its start to its result (14 Oct): if the car stops
following during a read (line lost, a junction, an obstacle, a fault), the
read is ignored even if its result arrives after following has resumed,
and a read that started in READY stays ignored after START. LINE
FOLLOWING ↔ HUMP CROSSING is still following. The state that counts is
the one Navigation is in when it handles the event; Line posts the start
of a read at most one 5 ms cycle (plus the 1 ms glitch filter) after its
first bar, so a read whose first bar came in that last moment before
following resumed is counted. Capture-start time stamps would remove that
window; they are not added (host test NAV-U10). A failed read never clears
a held command.

**D-54 Letters without an action.** Reported and logged, but not held: a
junction after it still finds no command (FR5 → SAFE HOLD), or keeps a
command held earlier.

**D-55 Keepalive.** While a move runs, Navigation sends a keepalive every
5 ms tick it sends nothing else, so Motion's 100 ms update rule (report
6.2) never stops a long move.

**D-56 Turns.** Pivot turns (wheels in opposite directions), counted by the
encoders (FR19).

**D-57 Health and SELF-TEST.** Line, IMU and Scan each set a health level
(IF-17) that SELF-TEST waits for. Obstacle Scan starts in RUN OVER and
only pings for health until START, so SELF-TEST can see a working sensor
and nothing in front of a car waiting in READY starts a scan. Fig 10 draws
MONITORING as the initial state; this is how the code reads it (open
question G11).

**D-58 STEADY class.** FR17's classes have none for driving at constant
speed, so a seventh class, STEADY, is reported instead of forcing one of
the others. This adds to FR17's list (open question G13).

**D-59 Queue depths.** Each receiver sizes its queues for bursts at its own
rate: Navigation's queues 8–16, Motion's 16, Safety's 16, Scan's 8,
ultrasonic echoes 8, barcode edges 64, Telemetry's 24, publish 8,
commands 4, broker 2.

**D-60 Route log replay.** After a summary, the log is published again at 4
entries per 200 ms tick, so the publish queue is never flooded.

**D-61 Ultrasonic readings.** An echo shorter than 2 cm, or none within
40 ms, is invalid; an echo beyond 4 m is a valid "nothing in range". The
40 ms wait must be longer than our module's echo pulse when nothing
returns, or open space reads as invalid (UNVALIDATED; manual test S5-U34).

**D-62 Obstacle profile.** The obstacle is the run of neighbouring returns
around the closest one while each is within 500 mm and within 150 mm of its
neighbour. Returns outside the run up to 300 mm beyond its front set the
clearance on their side. The side passed on must have room for the car and
the gap; if both have room, the larger clearance wins (left on a tie).
Settings: `CFG_SCAN_OBSTACLE_MM`, `CFG_SCAN_JUMP_MM`, `CFG_SCAN_DEPTH_MM`.

**D-63 Stored line levels.** The report's "levels stored in the lab" are
constants in `app_config.h`, not flash; until measured they are UNVALIDATED
placeholders (300 white, 3000 black).

**D-64 Mode exits.** As the report: SAFE HOLD → RESET → READY; FAULT →
RESET (cause cleared, Motion acknowledged) → SELF-TEST. The system
statechart gives COMPLETE no exit, so it stays until power-off. Report 6.5
says Obstacle Scan stays in RUN OVER "until a new START", which suggests a
new run can follow COMPLETE; the code follows the system statechart (open
question G12).

**D-65 Buttons.** START (Navigation, every 5 ms) and RESET (Safety, every
10 ms) are polled and debounced, not interrupt-driven.

**D-66 Command format.** Flat JSON on `inf2004/IS19/cmd`:
`remap` (letter `A`–`Z`, action `none`/`straight`/`left`/`right`/`uturn`),
`default_map`, `summary`, `stop`, `broker`. The sender's `seq` is echoed in
the ack on `inf2004/IS19/ack`; anything not understood gets `ok` 0.
Navigation acknowledges remaps once the map has changed (NFR8).

**D-67 Hump reports while turning.** A hump that ends while the car is not
following the line is not logged (report 6.4).

**D-68 Full queues.** A full control queue is a fault (report section 4):
the sender reports `QUEUE_FULL` to Safety. A full telemetry queue only drops
and counts (NFR10); a full command queue drops the command, which then gets
no ack, so the sender can retry.

**D-69 Console echo.** `CFG_TLM_ECHO_CONSOLE` 1 prints every message on the
console, for bench tests without a broker. Off by default.

**D-70 Reconnect back-off.** The broker connection retries after 1, 2, 4 …
30 s (report 6.1, BACKING OFF), back to 1 s once connected, replacing the
template's fixed 3 s. The Wi-Fi rejoin in the template's radio task
(`cyw43_utk.c`, fixed 5 s) is not changed (see G5).

**D-71 Following speed range (NFR17).** NFR17 says line following "shall
work at speeds from 0.15 to 0.35 m/s": the range in which the car must be
shown to follow the line (SYS-17), not a rule that its speed may never be
lower. While following, the speed Navigation sends to Motion after the
steering slow-down is kept inside that range (a design choice, 13 Oct), so
the car is only commanded at speeds that NFR17 and report 6.3's barcode
timing (120 ms is longer than a wide space at 0.15 m/s) cover. Without it,
a full steering error on a hump (150 mm/s less 40%) commanded 90 mm/s.
This limits the commanded speed only: it says nothing about the real speed
(wheel slip, a hump), which SYS-17 and S2-U34 measure. Wording corrected
14 Oct. Host tests NAV-U03, NAV-U14.

**D-72 Obstacle profile event (FR10).** FR10 asks the car to report the
obstacle's position, width, closest point and both clearances; before
13 Oct only the width and closest point reached MQTT (in the route log).
After PLANNING, Obstacle Scan posts the whole profile to Telemetry
(IF-12, `TLM_MSG_OBSTACLE`), which publishes it at once on
`inf2004/IS19/event` as `{"ev":"profile","x":…,"y":…,"w":…,"close":…,
"cl":…,"cr":…,"side":…,"rev":…}` (mm; `rev` 1 for the scan after
reversing; `cl` or `cr` −1 when that side was never seen, D-80). Host
test S1-U11; manual test S5-U33.

**D-73 Barcode input pull.** GP7's internal pull goes to the white level
(pull-down when `CFG_BC_DARK_LEVEL` is 1), so an unplugged sensor reads
white and never starts a capture. The first version used a pull-up, which
reads dark with that setting. This is not unplug detection: an unplugged
sensor, or one that always reads white, is not reported, and the pull says
nothing about the sensor's health (14 Oct). The module's real polarity and
levels are checked by hand (S3-U31, S3-U35).

**D-74 Side scan uses valid readings only (report 6.5).** While bypassing,
an invalid side reading (missing, malformed or timed out) used to count as
"nothing in range", that is, clear, so missed echoes could end the bypass
early. Now the obstacle counts as behind only after it was seen beside the
car and then `CFG_SCAN_BEHIND_COUNT` valid readings were clear; invalid
readings are skipped, and three in a row still give NO SAFE ROUTE. Host
test S5-U05.

**D-75 Ping gap (NFR5).** A ping is never sent less than
`CFG_US_MIN_PING_GAP_MS` (60 ms) after the previous one. Releases are
65 ms apart, but one that runs late leaves the next closer than 60 ms; that
ping waits one period. Manual test S5-U32.

**D-76 Bad IMU data while calibrating (report 6.4).** "Invalid sensor data
in any IMU state latches a fault": in CALIBRATING an invalid sample used to
restart calibration without a fault. Now invalid samples are left out of
the average and counted as in MONITORING; `CFG_IMU_INVALID_LIMIT` (5) in a
row latch `FAULT_CAUSE_IMU_DATA`, and the IMU starts again from
identification so the cause can clear before RESET. A jolt while
calibrating is not invalid data: it restarts the average (D-81). Manual
test S4-U37.

**D-77 Turn rate on flat ground (report 6.4, FR17).** The turn rate is
"taken from the compass on flat ground": on a hump it keeps its last value
and the next one is measured from the first heading after the hump,
smoothed over `CFG_IMU_RATE_SMOOTH` (4) updates. So while the car is on a
hump (motion class climbing or descending) the reported turn rate, in
telemetry and in the IMU's heading events, is the last flat-ground value
held, not a new measurement (noted 14 Oct). Manual test S4-U38.

**D-78 Barcode symbol boundary (14 Oct).** A capture's symbol is its
first 30 edges, the same boundary as the decode at the 30th edge (D-26).
An edge lost inside it (levels that do not alternate, or the Barcode
ISR's full-queue flag while fewer than 30 edges are stored) fails the
capture with reason "edge lost", even if the stored widths would decode;
before, a decode that succeeded won over a loss already recorded. Edges
and losses after the 30th edge belong to whatever follows the barcode
and never stop a complete symbol from decoding. The ISR's flag only says
that the dropped edge is newer than every edge still queued, so the Line
task marks the capture when the flag arrives (a loss among queued edges
then counts as inside, the safe side) and again after it has taken the
queued edges (for a capture they started). Left open: an edge dropped
while the Line task is emptying the queue is seen at its next cycle, so a
symbol completed in that same short window still decodes; that needs
chatter fast enough to keep the queue full while it is being emptied. A
drop marker in each queued edge would close it; not added. Host tests
S3-U16, S3-U17; manual test S3-U36.

**D-79 Line-sensor read failures (G15, decided 16 Oct).** When the ADC
read fails, the Line task marks the line record invalid (`b_valid` 0, new
`seq` and time), so Navigation treats it as no line: it keeps the last
error for the moment (D-52) and an invalid record never ends a line
search. `CFG_LINE_READ_FAIL_LIMIT` (4, that is 20 ms) failed reads in a row
report `FAULT_CAUSE_LINE_DATA` to Safety once (detail: the kernel's main
error code); Safety brakes and latches as for any fault (FR15's latch,
the same pattern report 6.4 uses for bad IMU data). RESET needs no extra
check for this cause: it re-runs SELF-TEST, which waits for healthy line
readings. Only failed reads count; a reading at the ADC rail (a sensor
fully on black) is not a failure. Host test S3-U18.

**D-80 Scan coverage (G16, decided 16 Oct).** Report 6.5 treats missing,
malformed and timed-out readings as invalid "because clearance cannot be
confirmed". An invalid reading in the coarse or fine scan is repeated
once at the same angle (the three-in-a-row rule still counts both). A
side with no valid reading outside the obstacle's run was never seen:
its clearance is −1 (`SCAN_CLEAR_UNKNOWN`) and it has no room, so FR11's
"reverse 10 cm and scan once more" follows if neither side has room, then
NO SAFE ROUTE. A coarse scan with no hit reports the path clear only if
all five readings are valid; otherwise the path is not confirmed and is
handled like no room (reverse once, scan again, then NO SAFE ROUTE with
reason "invalid"). A valid "nothing in range" (D-61) counts as seen, so
this rule relies on open space giving a valid reading, not a timeout
(manual test S5-U34); MONITORING already relies on it, since three
invalid readings in a row stop the car there too. Host test S5-U06;
manual test S5-U35.

**D-81 Accelerometer overrange and jolts (G17, decided 16 Oct).** In
MONITORING one sample above `CFG_IMU_MAX_G_MG` (3.5 g; the ±4 g range is
close to saturating) is a sudden impact: it latches a collision at once
(FR15, report 6.4), one report per impact, with |a| in mg as the detail.
Before, it counted only towards the 5-in-a-row invalid-data fault, so a
hard 1–4 sample crash could be missed while a softer bump was caught.
Failed reads and totals below 0.3 g keep D-42. In CALIBRATING a jolt (a
total that far above the range, or at least `CFG_IMU_IMPACT_MG` away from
1 g) means the car is not at rest: the average starts again without a
fault, since the motors are off; report 6.4's "any IMU state" is read as
applying to invalid data here (D-76), not to a bump while the car is
placed. Host test S4-U06; manual test S4-U37.

## E. Open questions

| ID | Question | Current behaviour |
|---|---|---|
| G1 | Demo-day network: which Wi-Fi and broker? Only the broker can change without reflashing (D-16): the Wi-Fi name and password, and the first broker, come from `config/wifi_secrets.h` at build time, and changing the Wi-Fi at run time would need a change to the template's radio task. | `config/wifi_secrets.h` values; broker changeable by command (D-16) |
| G2 | Should START also be possible over MQTT? | No: FR16 as written, START button only |
| G4 | After a failed barcode read, reverse and read again? | No: the speed hold is released and any held command is kept |
| G5 | Apply report 6.1's back-off to the Wi-Fi rejoin as well? It needs a change to the template's radio task. | MQTT only (D-70) |
| G11 | Fig 10 draws MONITORING as Obstacle Scan's initial state. Is RUN OVER (health pings only) until START the right reading? | RUN OVER until START (D-57) |
| G12 | May START begin a new run from COMPLETE? Report 6.5 says "until a new START"; the system statechart gives COMPLETE no exit. | No: COMPLETE until power-off (D-64) |
| G13 | Keep the STEADY motion class, which FR17 does not list? | Reported as a seventh class (D-58) |
| G14 | Use a barcode quiet zone (120 ms of white before a capture may start)? It is not in the report or the approved improvements. | Off: capture starts at the first bar (D-25) |
| G15 | When a line-sensor read fails, the Line task keeps checking in but leaves the last line record in place, and Navigation keeps steering on it while running (health is only checked in SELF-TEST). What should happen, after how long (fault, SAFE HOLD or line search), and should START check health again? | **Decided 16 Oct:** the record is marked invalid and 4 failed reads in a row latch a LINE_DATA fault (D-79). START does not re-check health; SELF-TEST does after RESET. Owner (S3) to confirm |
| G16 | Invalid scan readings leave a side's clearance at the 4 m maximum, and a coarse scan with no valid hit reports the path clear; invalid readings that are not three in a row never stop the car. What coverage is enough to plan a bypass or clear the path, and what happens without it? | **Decided 16 Oct:** invalid readings are repeated once; a side never seen has no room; path clear needs all five coarse readings valid (D-80). Owner (S5) to confirm |
| G17 | An accelerometer sample above 3.5 g (or below 0.3 g) is invalid data, not an impact: a short spike is only counted towards the 5-in-a-row fault. Calibration does not look for shocks. How should overrange and shocks while calibrating be treated? | **Decided 16 Oct:** above 3.5 g while monitoring is a collision at once; a jolt while calibrating restarts it without a fault; below 0.3 g and failed reads keep D-42 (D-81). Owner (S4) to confirm |
| G18 | FR19's commanded-distance and commanded-turn tests (S2-U35, S2-U36) cannot be run as written: nothing lets a tester start a single `moveForward(500)` or a 45° or 90° turn on demand; such moves only happen inside Navigation's own manoeuvres (junction, line search, obstacle bypass). Add a bench routine, or measure the moves Navigation already makes? | Not runnable (raised 14 Oct, not changed) |
| A11, A14 | Instructor agreement for the MQTT hooks and the pin override | implemented, switchable, marked pending |
| A12 | 1 ms tick: CPU load and jitter | to be measured |
| – | GitHub usernames for TEAM.md | to be filled in by each member |

FR5 (stop at a junction with no command) and FR16 (wait for START) are kept
exactly as written in the report.

## F. Consistency review, 13 Oct 2026

The code was checked against the W6 report (requirements, task table,
statecharts, SD1, SD2), the template rules, TEAM.md and the rules at the top
of this file. Each change is in the pull request of the code's owner.

| Found | Changed | Pull request | Entry | Tests |
|---|---|---|---|---|
| Following speed could fall below NFR17's 0.15 m/s (90 mm/s on a hump at full steering error) | speed clamped to 150–350 mm/s; compile-time checks of speeds, periods and the tick | shared | D-04, D-71 | NAV-U03, NAV-U14 |
| A barcode read started while turning could act after following resumed | the whole read is ignored | shared | D-53 | NAV-U10 |
| Hump freeze of the line levels (G7) was not a setting | `CFG_NAV_HUMP_FREEZE_LEVELS` | shared | D-23 | NAV-U14 |
| Several car- or course-dependent values were not tagged UNVALIDATED | tagged, with their basis | shared, S2–S5 | D-04 | – |
| On-target queue checks looked once and could fail at random | they poll for an empty queue | shared, S1, S2, S5 | – | NAV-U20, S1-U20, S2-U20, S5-U21 |
| FR10's position and clearances never reached MQTT | profile event on the event topic | S1, S5 | D-72 | S1-U11, S1-U08, S5-U33 |
| Encoder pull-ups were fixed in the driver | `CFG_ENC_PULL_UP` | S2 | D-04 | S2-U37 |
| Barcode capture waited for a 120 ms quiet zone, which the report does not have | capture starts at the first bar; quiet zone optional, off | S3 | D-25 | S3-U06, S3-U08 |
| An unplugged barcode sensor read dark | GP7 pulled to the white level | S3 | D-73 | S3-U35 |
| Bad IMU data in CALIBRATING did not latch a fault | latches after 5 invalid samples | S4 | D-76 | S4-U37 |
| Compass turn rate was also measured on a hump | flat ground only | S4 | D-77 | S4-U38 |
| A missed side echo while bypassing counted as clear | only valid readings count | S5 | D-74 | S5-U05 |
| A late scan release could ping less than 60 ms after the last ping | minimum gap enforced | S5 | D-75 | S5-U32 |
| Open space might read as a timed-out echo | manual test added | S5 | D-61 | S5-U34 |
| No requirement → design → code → test table (G6) | [`testing/TRACEABILITY.md`](../testing/TRACEABILITY.md) | shared | – | – |

Checked and unchanged: pins, task priorities, cores and periods (report
section 3); Safety's brake path and RESET handshake (SD2, A14); the barcode
→ junction → turn sequence (SD1); the other three approved improvements
(glitch filter, per-character wide/narrow, decoding at the 30th edge with
the 120 ms / 64-edge / 2 s failure path); the MQTT topics and back-off
(report 6.1, A11).

## G. Independent review, 14 Oct 2026

Another reviewer's list of remaining problems was checked against the W6
report, this code and its tests; each claim was reproduced on the host
where possible before anything was changed. Only the problems whose fix
needs no new team decision were changed. The others are open questions
G15–G18 in section E, unchanged in the code; G11–G14 are still open.

| Found | Changed | Pull request | Entry | Tests |
|---|---|---|---|---|
| A barcode letter could be decoded after an edge was lost inside the symbol | a loss within the first 30 edges fails the read; nothing after them spoils it; the capture is marked again after the drain that follows a queue overflow | S3 | D-78 | S3-U16, S3-U17, S3-U36 |
| A read still running when the car stopped following (line lost, junction, obstacle, fault), or started in READY, could act after following resumed | the read is ignored | shared | D-53 | NAV-U10 |
| `make` then `make SAN=1` ran the binary built without sanitizers again | host tests rebuild when `SAN`, `CC` or the flags change, and print the compiler and flags | shared | – | – |
| D-71 described NFR17 as a minimum speed | NFR17 is the range in which line following must work; the clamp limits the commanded speed only | shared | D-71 | SYS-17 now measures the real speed |
| D-73 and D-77 did not say what the pull and the held turn rate cannot show | notes added | shared | D-73, D-77 | S3-U35, S4-U38 |
| No requirement-to-evidence trail per owner | `testing/TRACEABILITY.md`: owner, evidence so far, open limits | shared | – | – |
| Tests missing for evidence the review asked for | S1-U35 (MQTT client ID), S3-U36 (chatter on GP7), INT-20 (reads around junction turns and humps); results to record (`testing/README.md`) | shared, S1, S3 | – | – |

## H. Second independent review, 16 Oct 2026

The changes of sections F and G (17 commits in six pull requests, prepared
in another team member's Claude Code session) were reviewed again in the
Claude Code session that wrote the first version, against the W6 report,
the rules at the top of this file and the code. Nothing was taken on
trust:

- The 17 patches were applied to `ee9601b` as the six branches; the merged
  tree is byte-identical to the package that was reviewed (git tree
  `9c3b41136a00`).
- Rebuilt here: the 7 firmware profiles build with 0 warnings; host tests
  pass 68/68 with gcc and with clang, each with and without ASan/UBSan.
- Reproduced here with a separate probe, before and after: a letter from a
  read that a junction interrupted was still held after the turn, so the
  car would have acted on it at the next junction (D-53). Confirmed in the
  old code: an invalid side echo was passed on as 4 m, that is clear
  (D-74); FR10's position and clearances never reached MQTT (D-72); a full
  steering error on a hump commanded 90 mm/s (D-71); bad IMU data in
  CALIBRATING restarted calibration without a fault (D-76); with
  `CFG_BC_DARK_LEVEL` 1 the GP7 pull-up made an unplugged sensor read dark
  (D-73).

| Item | Verdict | Note |
|---|---|---|
| All 17 commits | agreed, kept unchanged | Authors kept |
| D-78 (lost barcode edge) | agreed | In the case reproduced on 14 Oct (overflow flag at 29 stored edges, 30th edge still queued) the decoded letter was the true one: the dropped edge is newer than every queued edge, so it came after the symbol. The rule now gives up such a read, which is rare, to avoid decoding a symbol whose lost edges were replaced by later ones; "a wrong letter is far worse than no letter" (docs/notes/05) supports that trade. |
| S1-U20 (on-target) | agreed | Status records reach `mbf_tlm` at fixed phases, so a 10 ms poll can still miss a short empty moment. If it fails on the Pico, check that the free buffer space rises within the window instead. |
| Wording | corrected here | The rules at the top of this file were a team member's instructions to the session that wrote the code, not rules the whole team set. The last rule now says what was asked: a choice that changes a requirement or safety is asked, not made. Same in `app_config.h` and `bc_capture.h`. |
| G11–G18 | agreed that each needs a decision | G15, G16 and G17 were then put to the team member running this review, who chose the recommended option for each (D-79, D-80, D-81, implemented and host-tested here; the owners of S3, S4 and S5 still have to confirm them). G18 is the S2 owner's choice; G11–G14 are still open. |
````
