# 02 — Current state (as of 2026-10-16, W7 break week)

Tags and source keys: see `00_README.md`.

## 1. Snapshot

| Item | State | Tag |
|---|---|---|
| W6 design review | Submitted before this session (hurdle, Sat 10 Oct 23:59). RPT is dated 2026-10-10 09:16 and contains the four later fixes PREV lists | [Reported: PREV `03_status.md` "submitted once"]; [Verified: RPT file date; TR L1073] |
| Four-fix version resubmitted? | Not known | [Unknown] (PREV also left it unknown) |
| Knowledge base `docs/notes/` | Written by Claude; merged (PR #1, `424447f`, 2026-10-13T09:13:25Z) | [Verified: git, GitHub] |
| Firmware v1 (all buddies + Navigation) | Written by Claude 13 Oct; merged to `main` (`ee9601b`) via PRs #2–#7 with no GitHub review | [Verified: git; GitHub `get_reviews` = `[]` for #2–#7] |
| Teammate review (TPKG) | 17 commits by Buddy 2's Claude Code session (13 Oct consistency check + 14 Oct independent review), delivered as patches | [Verified: TPKG contents; git authors] |
| This session's review of TPKG (16 Oct) | All 17 commits agreed; rebuilt, retested and reproduced here; merged to `main` with merge commits, no PRs, on the user's choice | [Verified: git `fa142f9`…`698e39d`; TR L5713] |
| User's decisions G15, G16, G17 (16 Oct) | Implemented as D-79, D-80, D-81 with host tests; on `main` | [Verified: git `fb7a67b`, `c0e46a9`, `ea95a01`, `b7bf737`, `58f7d61`; TR L5713] |
| `main` | `58f7d61302e686ff2e8c84df1a5016c7807ac99c` | [Verified: `git ls-remote`] |
| Builds | 7 profiles, exit 0, 0 warnings | [Verified: EV `final_main/build_summary.txt`] |
| Host unit tests | 71 passed, 0 failed (gcc, gcc+ASan/UBSan, clang, clang+ASan/UBSan) | [Verified: EV `final_main/host_*.log`] |
| On-target tests (`TEST=1` image) | Built, never run | [Verified: EV; TEST_LOG Results tables hold only the template's example row] |
| Manual, integration, system tests | Defined, none run | [Verified: `testing/*/TEST_LOG.md`] |
| Hardware values | All starting values; 52 lines of `app_config.h` carry `UNVALIDATED` | [Verified: `grep -c`] |
| Template exceptions A11, A14 | Implemented, switchable; no instructor approval and no team agreement recorded | [Verified: TEAM.md §7]; [Unknown] approvals |
| 1 ms tick (A12) | Implemented; CPU load and jitter not measured | [Verified: `config/config.h`] |
| Decisions G15–G17 | Made by the user (one member); the owners of S3, S4, S5 have not confirmed them | [Verified: TR L5713; `docs/decisions.md` §E] |
| Tribunal 1 (Mon 19 Oct) | Not yet held | [Verified: KB README dates; system date] |

## 2. Lifecycle of the main work items

"Host" = host unit tests on a PC. "—" = step does not apply.

| Work item | Proposed | Accepted | Implemented | Inspected / verified | Tested | Submitted / approved |
|---|---|---|---|---|---|---|
| W6 design (RPT) | team | team (submitted) [Reported: PREV] | — | — | — | hurdle submitted [Reported: PREV]; not graded [Reported: PREV quoting the prof's email] |
| Repo restructured to the template | Claude (Q2) | user "2) a" [Verified: TR L1081] | `56ec731`, `e870827` (template byte-identical to the uploaded zip, 643/643 files) [Verified] | builds [Verified: EV] | — | — |
| One PR per buddy (13 Oct) | Claude (Q3) | user "3) **b" [Verified: TR L1081] | PRs #2–#7 [Verified] | no human review [Verified: GitHub] | — | merged 2026-10-13T11:38Z on "just push all into main first" [Verified: TR L4205] |
| UNVALIDATED values in central config | Claude (Q4b) | user, with extra rules [Verified: TR L1081] | `app_config.h` [Verified] | widened by the teammate (more tags, 13 Oct) and checked here | not measured | — |
| Four improvements (junction read-ignore, glitch filter, per-character wide/narrow, decode at 30th edge with bounded timeout) | Claude, KB 05 §6, KB 06 G3 | user "6) b" [Verified: TR L1081] | D-25, D-26, D-27, D-53 [Verified] | reviewed twice (TPKG; this session) | host S3-U04, S3-U06…S3-U10, S3-U16, S3-U17, NAV-U10 PASS [Verified: EV] | team / instructor [Unknown] |
| Barcode quiet zone (120 ms white before a capture) | Claude, KB 05 §6 item 5 | **not** approved (not in the Q6 list) | was on in v1; switched **off** by TPKG (D-25) | agreed here | S3-U08 covers both modes | open question G14 |
| Teammate fixes (17 commits) | TPKG | user: "if u agree, fix it" [Verified: TR L5187]; this session agreed with all | merged `fa142f9`…`698e39d` [Verified] | reviewed here; I-01, I-05 reproduced with a separate probe | 68/68 host on the reviewed merge [Verified: EV] | owners / team [Unknown] |
| G15 line read failures → LINE_DATA fault | TPKG option A | user, 16 Oct [Verified: TR L5713] | D-79, `fb7a67b` | self-review | host S3-U18 [Verified: EV] | owner S3 to confirm |
| G16 unseen scan side = no room | TPKG option A | user, 16 Oct | D-80, `ea95a01` | self-review | host S5-U06 [Verified: EV] | owner S5 to confirm |
| G17 overrange = collision; jolt restarts calibration | TPKG option A + (ii) | user, 16 Oct | D-81, `c0e46a9` | self-review | host S4-U06 [Verified: EV] | owner S4 to confirm |
| A11 MQTT hooks | RPT A11 | implemented "pending" by Claude's choice, not the user's answer (08 §2) [Verified: TR L4170] | `mqtt_utk.c/.h`, `lwipopts.h`, `mqtt_bridge.c` | reviewed | builds with `NET=lwip MQTT=1`; never run | instructor [Unknown]; marked pending |
| A14 Safety pin override | RPT A14 | same as A11 | `safety_task.c`, `CFG_SAFETY_PIN_OVERRIDE` | reviewed | never run (register code, no host test) | instructor [Unknown]; marked pending |
| A12 1 ms tick | RPT §3 | report design | `CNF_TIMER_PERIOD 1`; compile-time check `CFG_TICK_MS == CNF_TIMER_PERIOD` (TPKG) | — | not measured | to be verified |
| NFR14 broker change | RPT NFR14 | Claude's choice (MQTT `broker` command, RAM only), not the user's answer [Verified: TR L4170] | D-16 | — | host S1-U01, S1-U04 | — |
| Handover | user | — | this folder | cross-checked (12, 10) | — | pushed to `main` |

## 3. Implementation-status table by requirement

Sources: "Implemented in" from file header comments and `docs/decisions.md`
[Verified: REPO]; test IDs and their requirement links from the
*Requirement / interface* column of the current `testing/*/TEST_LOG.md`
[Verified: REPO]; host results from EV (all 71 PASS on `b7bf737`, code
identical to `58f7d61`). **No row has any on-target, hardware or timing
result.** "Defined" = the test exists with steps and expected result and
has not been run.

### Functional requirements

| ID | Implemented in | Host tests (PASS) | Defined, not run | Status |
|---|---|---|---|---|
| FR1 line following | `subsystem3/line_task.c`, `line_track.c`, `steering.c`; Navigation LINE FOLLOWING (`nav_sm.c`) | NAV-U03, S3-U11, S3-U14, S3-U18 | SYS-01 | implemented; commanded speed kept in 150–350 mm/s (D-71); read failures latch a fault (D-79) |
| FR2 Code 39 both directions | `bc_capture.c`, `bc_decode.c`, `drv_barcode.c` | S3-U01…S3-U05, S3-U09, S3-U16, S3-U17 | INT-20, S3-U36, SYS-02 | implemented; capture starts at the first bar (D-25); a lost edge inside the symbol fails the read (D-78); sensor edges (A3) unmeasured |
| FR3 act at next junction | `nav_sm.c` (held command); `line_track.c` (junction) | NAV-U04, S3-U12 | INT-20, SYS-03 | implemented; a read counts only if the car followed the line from its start to its result (D-53) |
| FR4 turns within ±5° | `nav_sm.c` AT JUNCTION; `motion_task.c`, `motion_ctl.c` | NAV-U04, NAV-U05, S4-U02 | S4-U31, SYS-03 | implemented; accuracy depends on A2 |
| FR5 no command → stop, report | `nav_sm.c` → SAFE HOLD `no_command` | NAV-U06 | SYS-04 | implemented exactly as written |
| FR6 default map + MQTT remap | `letter_map.c`, `cmd_parse.c`, `command_task.c`, Navigation ack | NAV-U12, S1-U01, S3-U15 | S1-U31, SYS-05 | implemented |
| FR7 follow on a hump | HUMP CROSSING (150 mm/s; level freeze `CFG_NAV_HUMP_FREEZE_LEVELS` 1) | NAV-U14 | SYS-06 | implemented |
| FR8 hump count and height ±5 mm | `imu_calc.c`, `imu_task.c`, summary | NAV-U14, S4-U01, S4-U03, S4-U04 | S4-U32, SYS-06 | height table UNVALIDATED (A4) |
| FR9 stop ≥ 10 cm before an obstacle | `scan_task.c` (two readings ≤ 300 mm), `nav_sm.c` | NAV-U15 | S5-U31, SYS-07 | stopping distance at speed not shown |
| FR10 profile ±2 cm, reported | `scan_logic.c`, `scan_task.c`; profile event on MQTT (D-72) | S1-U11, S5-U02, S5-U03, S5-U06 | INT-19, S5-U33, SYS-07 | implemented; position and clearances now published (TPKG); an unseen side reports −1 (D-80) |
| FR11 bypass / reverse-and-rescan | `scan_logic.c` (side choice, `scan_side_step`), `nav_sm.c` (D-36) | NAV-U15, NAV-U16, S5-U04, S5-U05, S5-U06 | S5-U33, S5-U35, SYS-07 | implemented; side scan uses valid readings only (D-74); unseen side has no room (D-80) |
| FR12 telemetry content | `telemetry_task.c`, `tlm_format.c` | S2-U03 (as linked) | INT-12, S1-U35, SYS-08 | never published to a broker |
| FR13 route log | `tlm_format.c`, D-51, D-60 | S1-U06, S1-U09 | S1-U32, SYS-09 | implemented |
| FR14 remap / summary / stop | `cmd_parse.c`, `command_task.c` | S1-U01, S1-U02, S1-U07 | S1-U32, SYS-09 | implemented |
| FR15 brake + latch until reset | `safety_task.c`; collision from `imu_task.c`/`imu_calc.c`; LINE_DATA from `line_task.c` | NAV-U13, S4-U06 | INT-16, S4-U20, S4-U33, S4-U37, SYS-10 | implemented; overrange = collision at once (D-81); Safety task itself has no host test |
| FR16 still until START | `nav_sm.c` READY (GP20 polled, D-65) | NAV-U01 | NAV-U30, SYS-11 | implemented exactly as written |
| FR17 motion classes + turn rate | `imu_calc.c` (+ STEADY, D-58); rate from flat ground only (D-77) | S4-U05 | S4-U38, SYS-12 | STEADY is open question G13 |
| FR18 line lost → 5 s search → stop, summary | `nav_sm.c` LINE SEARCH → COMPLETE | NAV-U07, NAV-U08, S1-U07, S3-U12 | SYS-13 | implemented |
| FR19 distance ±5 %, angle ±5° | `motion_ctl.c`, `motion_task.c` | S2-U01, S2-U02 | S2-U35, S2-U36 | S2-U35/U36 cannot be run as written (open question G18) |

### Non-functional requirements

| ID | How the code addresses it | Host (PASS) | Defined, not run | Status |
|---|---|---|---|---|
| NFR1 5 ± 1 ms | 5 ms cyclic release on a 1 ms tick; compile-time check periods are whole ticks | — | S3-U20, S3-U33 | not measured (A12) |
| NFR2 1 µs edge stamps | Barcode ISR time-stamps via the dispatcher | — | S3-U32, S3-U34 | not measured (A8) |
| NFR3 20 ± 2 ms | Motion 20 ms cyclic | S2-U04 | S2-U33 | not measured |
| NFR4 100 Hz / 75 Hz | `CTRL1_A 0x57`, `CRA_M 0x18`, IMU every 10 ms | — | S4-U36 | not measured |
| NFR5 pings ≥ 60 ms apart | 65 ms period + enforced 60 ms gap (D-75) + compile-time check | — | S5-U32 | not measured |
| NFR6 drive off within 50 ms | Safety 10 ms + wake on fault; pin override | — | S4-U33, SYS-10 | not measured |
| NFR7 course < 5 min | whole system | — | SYS-01 | untested |
| NFR8 remap within 1 s | ack once the map changed (D-66) | NAV-U12 | INT-10, S1-U31, SYS-05 | latency not measured |
| NFR9 5 Hz / 1 Hz | 200 ms telemetry; heartbeat every 1000 ms (compile-time check) | — | S1-U30, SYS-08 | not measured |
| NFR10 comms loss never stops the car | nothing in the control path waits on comms; telemetry drops and counts (D-68) | — | S1-U22, S1-U34, S1-U35, SYS-14 | untested on target |
| NFR11 ≤ 3.3 V on GPIO | wiring; input pulls (GP7 to white, D-73; encoder pull-up setting) | — | S2-U37, S3-U35, SYS-19 | not checked (A5) |
| NFR12 power bank | soft start (D-30) | — | SYS-15 | not checked (A6) |
| NFR13 CPU ≤ 60 % | optional timing probe (D-20, off) | — | S3-U33, SYS-16 | not measured (A12) |
| NFR14 broker without reflash | MQTT `broker` command, RAM only (D-16); Wi-Fi stays build-time (G1) | S1-U01, S1-U04 | INT-15, S1-U33, SYS-20 | implemented with that limit |
| NFR15 C/SDK/µT-Kernel/template/Barr-C | conventions D-18; exceptions TEAM.md §7 | NAV-U19, S1-U03, S1-U10 | — | no Barr-C checker run; conformance [Unknown] |
| NFR16 latched stop, zeroed before release | IF-09 handshake, override, FAULT level | — | INT-09, S4-U35, SYS-10 | untested on target |
| NFR17 0.15–0.35 m/s, lighting, joins | commanded following speed clamped to 150–350 mm/s (D-71); level adaptation (D-23) | NAV-U03, NAV-U14 | SYS-17 | commanded speed only; real speed unmeasured |
| NFR18 wheel ±10 %, drift ≤ 2 cm/m | PID with feed-forward | S2-U03, S2-U04 | S2-U34 | no drift correction exists; measure first (TPKG I-09) |
| NFR19 RAM < 75 % | ≈ 25 % default, ≈ 47 % MQTT build (TEAM.md §6, linker map) | — | SYS-18 | estimate only |

## 4. Repository and branches

| Item | Value | Tag |
|---|---|---|
| `main` | `58f7d61` (first-parent: G15–G17 commits on top of six merge commits `fa142f9`…`698e39d`, on top of PR #8 `cc97bbf`) | [Verified: git] |
| Earlier merges | PR #1 `424447f`; PR #2 `ee9601b` (merging #3 `1d849ad`, #4 `5268acc`, #5 `97538b6`, #6 `367b01f`, #7 `7d4ac11`); PR #8 `cc97bbf` (handover drafts, by the user) | [Verified: git, GitHub] |
| Commit authors since `cc97bbf` | 17 by Dominic Ng Jun Jie (TPKG), 13 by Claude (2 review commits, 6 merges, 3 decision commits, 2 docs) | [Verified: `git log`] |
| Remote branches | `main`, `claude/upbeat-franklin-pkkhdt` (= `main`), `handover` (user's, PR #8), and the 13 Oct `-s1-comms` … `-s5-scan` branches | [Verified: `git ls-remote`] |
| Open PRs | none | [Verified: GitHub] |
| CI | none configured | [Verified: TR L4201] |

## 5. What nobody has done yet (facts, no new advice)

- [Verified: GitHub; TPKG] No GitHub review by any teammate; the S1–S5
  owners other than the user and Buddy 2 have left no trace in the repo.
- [Verified: TEAM.md §1] No GitHub usernames recorded.
- [Verified: `testing/*/TEST_LOG.md`] No test result recorded, including the
  host-test results in EV.
- [Verified: `app_config.h`] No measured hardware value.
- [Unknown] Whether any member has flashed the firmware.
- [Unknown] Whether the instructors were asked about A11, A12, A14 or the
  KB 06 questions.
