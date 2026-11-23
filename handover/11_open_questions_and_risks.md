# 11 — Open questions and risks (every item with a disposition)

Tags and source keys: see `00_README.md`. Dispositions: **OPEN** (needs a
decision), **DECIDED – to confirm** (the user decided; owner/team has not
confirmed), **DEFERRED** (needs hardware/measurement), **BLOCKED** (needs
something not available), **CLOSED** (resolved, evidence named),
**ACCEPTED RESIDUAL** (known limit, left as is by a recorded choice).
No new recommendations are added here; "what the sources say" quotes them.

## 1. Questions in `docs/decisions.md` §E (REPO `58f7d61`)

| ID | Question (short) | Current behaviour in code | Disposition | Who decides | Tag |
|---|---|---|---|---|---|
| G1 | Demo-day Wi-Fi and broker | `config/wifi_secrets.h` at build time; broker changeable by `broker` command (D-16), RAM only | OPEN | team / instructors (KB 06 question) | [Verified: §E] |
| G2 | START over MQTT? | No: FR16 as written | OPEN (user said "i will answer ltr") | team | [Verified: §E; TR] |
| G4 | Reverse and re-read after a failed barcode? | No; held command kept | OPEN | team | [Verified: §E] |
| G5 (§E sense) | Back-off for the Wi-Fi rejoin too? | MQTT only (D-70); radio task unchanged | OPEN | team (template change → instructors) | [Verified: §E] |
| G11 | Scan initial state RUN OVER vs Fig 10's MONITORING | RUN OVER until START (D-57) | OPEN | S5 owner / team; report revision | [Verified: §E; TPKG §5.6] |
| G12 | START from COMPLETE? | No; COMPLETE until power-off (D-64) | OPEN | team | [Verified: §E] |
| G13 | Keep the STEADY class (not in FR17)? | reported as a 7th class (D-58) | OPEN | S4 owner / team | [Verified: §E] |
| G14 | Barcode quiet zone? | off, first-bar start (D-25) | OPEN | S3 owner | [Verified: §E] |
| G15 | Failed line reads | `b_valid` 0; 4 in a row → LINE_DATA (D-79) | DECIDED – to confirm (S3) | user decided 16 Oct | [Verified: §E; TR L5713] |
| G16 | Scan coverage | retry once; unseen side no room; path clear needs 5 valid (D-80) | DECIDED – to confirm (S5) | user decided 16 Oct | same |
| G17 | Overrange and jolts | collision while monitoring; restart while calibrating (D-81) | DECIDED – to confirm (S4) | user decided 16 Oct | same |
| G18 | FR19 single-move tests S2-U35/S2-U36 not runnable | not changed | OPEN | S2 owner (Dominic); [Reported: TPKG §8.2] lists it among unresolved decisions | [Verified: §E] |
| A11, A14 | instructor agreement: MQTT hooks; pin override | implemented, switchable, pending | BLOCKED (instructor) | instructors | [Verified: TEAM.md §7] |
| A12 | 1 ms tick: CPU ≤ 60%, 5 ± 1 ms | not measured | DEFERRED | S3 (owner per RPT) | same |
| – | GitHub usernames in TEAM.md | `*to fill*` | OPEN | each member | [Verified: TEAM.md §1] |

## 2. KB 06 review findings G1–G10 (numbering differs, see 03 V2)

[Verified: `docs/notes/06_review_findings_and_open_questions.md`]

| KB 06 ID | Topic | Disposition |
|---|---|---|
| G1 | NFR14 broker / demo Wi-Fi | same as §E G1: OPEN |
| G2 | START vs "cannot be touched" | same as §E G2: OPEN |
| G3 | crossbar starts a capture while turning | CLOSED in code: D-53 (approved improvement, extended 13/14 Oct), host NAV-U10; on-car INT-20 not run |
| G4 | FR5 single misread costs the run | same as §E G4: OPEN |
| **G5 (KB 06)** | two more ISR hand-offs (barcode edge times, echo times) beyond the two justified shared variables | [Inferred] handled by message buffers from the ISRs (D-08); no report revision recorded — OPEN for the report text |
| G6 | interface IDs, traceability | CLOSED: IF-01…IF-18 (TEAM.md §5), `testing/TRACEABILITY.md` |
| G7 | sensor height change on a hump | implemented as setting `CFG_NAV_HUMP_FREEZE_LEVELS` 1 (D-23); DEFERRED (on-car) |
| G8 | Debug Probe unused | OPEN (not in code; `CRASH_REPORT=1` UART exists) |
| G9 | power bank limits / auto-off | soft start (D-30) implemented; DEFERRED (A6 test) |
| G10 | encoder interrupt rate | DEFERRED (A2/A8 measurement) |
| KB 06 "Questions to ask the instructors" | barcode/hump side, junction types, START after placing, demo SSID/broker, bar height and distance, hump height, obstacle size | OPEN, no answers in this session |

## 3. Report assumptions A1–A14 (RPT; full text in 01)

All are **DEFERRED** (need measurement or approval) unless stated; none
has evidence in this session. [Verified: test logs empty]

| A | Short | Code support | Test that would close it |
|---|---|---|---|
| A1 | radio/USB tasks do not delay Safety/control | watchdog armed after radio start (D-44) | timing with Wi-Fi build (TEAM.md §4 note) |
| A2 | encoder counts enough for ±5° | `CFG_ENC_COUNTS_PER_REV` 528 placeholder | S2 manual tests |
| A3 | clean barcode edges | glitch filter, D-78 | S3-U3x, logic analyser |
| A4 | hump height from tilt | table UNVALIDATED | S4 manual |
| A5 | HC-SR04P at 3.3 V | echo pull-down; D-80 relies on valid open-space reading | S5-U31, S5-U34 |
| A6 | power bank | soft start | A6 power test |
| A7 | lecturer's default mapping | `letter_map.c` defaults | confirm with lecturer (KB 02 records it as said) |
| A8 | encoder IRQs vs barcode timing | rising edges of A only (D-28) | measurement |
| A9 | line readings separate centred/junction/lost | levels 300/3000 UNVALIDATED | S3-U30 logs |
| A10 | 5 s search enough | `CFG_NAV_SEARCH_MS` 5000 | SYS tests |
| A11 | MQTT extension accepted | implemented | BLOCKED: instructor |
| A12 | 1 ms tick | `CNF_TIMER_PERIOD 1`; probe pin option (D-20) | logic analyser |
| A13 | barcode far enough before junction | — | SYS / INT-20 |
| A14 | Safety pin override accepted | implemented, switchable | BLOCKED: instructor |

## 4. Teammate review items still open (TPKG)

| Item | Disposition | Tag |
|---|---|---|
| NF-5 `CFG_LINE_SELFTEST_MAX` 4090 may reject a healthy sensor saturating at 4095 on black at power-on | BLOCKED on S3-U30 measured levels; no change | [Reported: TPKG NF-5] |
| NF-2 barcode `failed` counter not published | ACCEPTED RESIDUAL (optional S1 enhancement, not made) | [Reported] |
| I-09 no drift correction | DEFERRED (S2-U34 first) | [Reported] |
| NF-8 / D-78 window: edge dropped while the queue is emptied | ACCEPTED RESIDUAL (documented in D-78; a drop marker would close it) | [Verified: D-78] |
| NF-8 / D-53 window: read start attributed up to one Line cycle late | ACCEPTED RESIDUAL (documented in D-53; capture time stamps would close it) | [Verified: D-53] |
| S1-U20 may miss a short empty moment | ACCEPTED RESIDUAL; fallback named in §H | [Verified: §H] |
| W8-1 official Tribunal brief and rubric missing | BLOCKED | [Reported: TPKG] |
| C-09 meaning of unknown clearance in the profile event | CLOSED by D-80 (`cl`/`cr` −1) | [Verified: D-72] |
| NF-7 handover out of date | CLOSED by this version | — |

## 5. Risks found while writing this pack

| Risk | Evidence | Disposition |
|---|---|---|
| A line record that stops updating but stays `b_valid` 1 is still followed (I-02 probe on `main`: DRIVE 250 mm/s with a 10 s old record). D-79 covers reported read failures only. If the Line task stops, its 50 ms check-in lapses and the watchdog is no longer fed (reset, not a latched fault). | `evidence/probes/out_main.txt`; D-79; 07 §5 | OPEN — [Inferred]; not in §E. Raised here, not decided. |
| Decisions made before asking on 13 Oct (09 §3) | TR L1390, L4170 | OPEN items G1, G2, G4, G5, G11, G13 above |
| G15–G17 implemented on one member's decision | TR L5713 | DECIDED – to confirm |
| Nothing has run on hardware; 93 settings UNVALIDATED | 04 §5, 10 §1 | DEFERRED |
| Teammate's author email published in git history (his package asked to keep authors) | git log | ACCEPTED (user's merge); [Unknown] whether he wants it public |
| RPT revision status: whether the PDF used is the final submitted version | 03 V1 | [Unknown] |
