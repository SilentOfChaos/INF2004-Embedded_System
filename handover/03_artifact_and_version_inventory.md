# 03 — Artifact and version inventory

Tags and source keys: see `00_README.md`. State described: REPO `main` at
`58f7d61302e686ff2e8c84df1a5016c7807ac99c` (16 Oct 2026).

## 1. Authoritative file for each purpose

| Purpose | Authoritative file (latest) | Older or competing versions | Tag |
|---|---|---|---|
| Requirements, assumptions, W6 design | RPT `INF2004_Design Review_IS19.pdf` (30 pages, 2026-10-10 09:16, sha256 `98bda125…c08c14f`) | PREV inspected a DOCX, `INF2004_Week6_Design_Review_IS19_final (10).docx`, which was never available here. Claude checked at 09:17Z on 13 Oct that the PDF "already contains all four fixes listed in `10_final_content.md`" and used the PDF. | [Verified: TR L1073; RPT text contains all four fixed passages, see 13] |
| Design figures | `Fig1 - Hardware wiring.png` … `Fig10 - Subsystem statechart 5 (obstacle).png` in the RPT zip (dated 2026-10-10 08:31) | KB 04 says "the PDF figures match the PNGs" | [Verified: zip listing]; [Reported: KB 04 claim, written by Claude in this session] |
| Condensed design | `docs/notes/04_design_review_summary.md` | RPT wins wherever wording matters | [Verified: REPO] |
| Ownership, pins, tasks, interfaces, kernel-object budget, template exceptions | `TEAM.md` | template version at `e870827` | [Verified: git diff] |
| Design decisions, changes, reviews | `docs/decisions.md` (D-01 … D-81; §E open questions; §F 13 Oct consistency review; §G 14 Oct independent review; §H 16 Oct second review) | the 13 Oct version had D-01 … D-70 (commit `ee9601b`) | [Verified: REPO, git log] |
| Requirement → design → code → test | `testing/TRACEABILITY.md` (added by the teammate's commit `e7adb6f` and earlier shared commits, updated by Claude in `b7bf737`) | none | [Verified: git log] |
| Tunable values and their basis | `app_program/app_config.h` (621 lines) | `scratchpad/app_config.h.bak` is a 13 Oct copy and is now **out of date** (superseded) | [Verified: `wc -l`; `cmp` differs] |
| Kernel configuration | `config/config.h` (`CNF_TIMER_PERIOD 1`, `CNF_MAX_MBFID 20`) | template values 10 and 12 at `e870827` | [Verified: git diff] |
| MQTT/lwIP extension (A11) | `lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c`, `mqtt_utk.h`, `lwipopts.h` | template versions at `e870827` | [Verified: git diff --stat] |
| Test definitions and (empty) results | `testing/*/TEST_LOG.md`, `testing/README.md` | template example rows replaced | [Verified: REPO] |
| Host tests | `testing/host/` (Makefile + 10 C/H files), 71 tests | 64 on 14 Oct (`ee9601b`), 68 in TPKG v2 | [Verified: EV `host_test_list.txt`] |
| Build/test evidence for `main` | `handoff_artifacts.zip` → `evidence/final_main/` (commit `b7bf737`, 2026-10-16) | `evidence/reviewed_merge/` (`698e39d`), `evidence/teammate_v2/`, `evidence/ee9601b/` (14 Oct); figures quoted in compaction summaries are not evidence (09 §1) | [Verified: EV] |
| Knowledge base | `docs/notes/README.md`, `01`–`07` | first committed as `notes/` in PR #1, moved to `docs/notes/` in `56ec731` (content unchanged) | [Verified: git log] |
| Original planning notes (user's) | `docs/planning_notes.md` | was the repo's `README.md` (commits `bbb8f09`, `ecd7195`); moved unchanged in `56ec731` | [Verified: git log] |
| Repo README | `README.md` = IS19 header + the template guide | template README at `e870827` | [Verified: git diff] |
| Teammate's review and its reasoning | TPKG `IS19_Independent_Review_Actions.md` (588 lines) | the 13 Oct PR texts it replaced (`pr/*.md` inside TPKG) | [Verified: upload; `wc -l`] |
| Handover | this folder `handover/` | the 14 Oct drafts of 00–04 (PR #8, `cc97bbf`) are superseded by this version; PREV is superseded where it says "The team has not started any code yet." | [Verified: git log; PREV `00_README.md`] |

## 2. Version conflicts and how they stand

| # | Conflict | Resolution in this pack | Status |
|---|---|---|---|
| V1 | DOCX "(10)" (PREV) vs PDF (RPT) as the report source | PDF used; it contains all four fixes. A newer report may exist outside this session. | [Unknown] whether newer; [Unknown] whether resubmitted |
| V2 | KB 06 numbers review findings **G1–G10**; `docs/decisions.md` §E reuses **G1, G2, G4 with the same meaning** but uses **G5 for a different question** ("Apply report 6.1's back-off to the Wi-Fi rejoin as well?") than KB 06's G5 ("two more ISR hand-offs … barcode edge times … echo times"). D-70 says "(see G5)" in the §E sense. D-23 "(improvement G7)" and D-30 "(improvement G9)" match KB 06. §E also adds G11–G18, which KB 06 does not have. | Both G5 meanings are listed in 11 as "G5 (KB 06)" and "G5 (decisions §E)". The files were not changed. | [Verified: both files at `58f7d61`]; open, documentation only |
| V3 | `bc_decode.c` header said the bias tolerance was "host test S3-U05"; D-27 and the S3 TEST_LOG say S3-U04 | **Fixed** by the teammate's commit `31eda84` (S3), which corrected the comment ("V3 comment" in TPKG §9 row 8) | [Verified: `grep S3-U0 app_program/subsystem3/bc_decode.c` at `58f7d61`] |
| V4 | Image sizes differ between versions and summaries | Each figure belongs to one tree: compaction summary 314668 / 369672 (intermediate 13 Oct 11:00Z build, not evidence); `ee9601b` 314828 / 369832; TPKG v2 and `698e39d` 315492 / 370496; `b7bf737` (= `main` code) 316096 / 371100. Use `evidence/final_main/`. | [Verified: TR L3147; EV] |
| V5 | `Robo_Pico.pdf` page numbering: KB 03 cites the Grove table as "p.3"; RPT §8.3 cites "Robo_Pico.pdf, p. 4" | Same page: PDF page 4 of 11 carries the printed page number 3 | [Verified: page-split text extract of `Robo_Pico.pdf`] |
| V6 | Template source: uploaded zip vs the GitHub URL the user gave | REPO commit `e870827` is byte-identical to the uploaded zip (643 of 643 files, git blob hashes). The GitHub template repository was not fetched from this session, so equality with its current head is unknown. | [Verified: `evidence/template_import_check.txt`, re-run 2026-10-16 01:57Z]; [Unknown] GitHub head |
| V7 | PREV says "no firmware code or config was created" and "The team has not started any code yet" | Superseded by REPO | [Verified: REPO] |
| V8 | `scratchpad/sizes.c` sized `mbf_publish` at 225 bytes (an earlier payload limit) | Superseded by `helpers/sizes_now.c` (321 = 1 + `CFG_MQTT_PAYLOAD_MAX` 320) | [Verified: `evidence/ee9601b/mbf_sizes_now.txt`] |
| V9 | Host test count: 64 (14 Oct, handover drafts, NF-7), 66 (TPKG v1), 68 (TPKG v2), 71 (`main`) | 71 is current: TPKG added host tests S1-U11, S3-U16, S3-U17, S5-U05 (68); Claude added S3-U18, S4-U06, S5-U06 for D-79…D-81 (71) | [Verified: EV host logs] |
| V10 | Who the user of this session is. TPKG NF-6: "This session's user is Dominic Ng Jun Jie (Buddy 2)" — about the teammate's own session. The 14 Oct draft of 01 inferred that this session's user is Buddy 3. | Two different sessions; 01 §identity keeps both statements apart. | [Reported: TPKG NF-6]; see 01 |
| V11 | TPKG §2 table and the TPKG summary speak of "12 patches" (the 13 Oct set) and of 17 (v2) | v2 = the 12 of 13 Oct plus 5 of 14–15 Oct; all 17 are in REPO | [Verified: `git log cc97bbf..58f7d61`] |

## 3. Repository contents relevant to IS19 (`main`)

[Verified: `git diff --name-status e870827..58f7d61`] 102 files added
outside `handover/`, 32 template files modified, 4 template files deleted.

### Files added for IS19

| Folder | Files | Notes |
|---|---|---|
| `app_program/app_config.h` | 1 (621 lines) | all tunable values |
| `app_program/common/` | `app_gpio.c/.h`, `app_pwm.c/.h`, `app_time.c/.h`, `fixmath.c/.h`, `gpio_irq.c/.h`, `hw_regs.h` (11) | team-shared helpers |
| `app_program/navigation/` | `nav_sm.c/.h` (statechart, `nav_sm.c` 1135 lines), `nav_task.c/.h`, `navigation.c/.h`, `nav_types.h`, `navigation_test.c` (8) | team-shared Navigation |
| `app_program/subsystem1/` | `cmd_parse.c/.h`, `tlm_json.c/.h`, `tlm_format.c/.h`, `tlm_msgs.h`, `comms.h`, `mqtt_bridge.c`, `command_task.c/.h`, `telemetry_task.c/.h` (13) | Buddy 1 |
| `app_program/subsystem2/` | `motion_ctl.c/.h`, `drv_motor.c/.h`, `drv_encoder.c/.h`, `motion_task.c/.h`, `motion_msgs.h` (9) | Buddy 2 |
| `app_program/subsystem3/` | `bc_decode.c/.h`, `bc_capture.c/.h`, `line_track.c/.h`, `steering.c/.h`, `letter_map.c/.h`, `drv_barcode.c/.h`, `drv_line_adc.c/.h`, `line_task.c/.h`, `line_msgs.h` (17) | Buddy 3 |
| `app_program/subsystem4/` | `drv_i2c1.c/.h`, `drv_lsm303.c/.h`, `drv_watchdog.c/.h`, `drv_buzzer.c/.h`, `drv_neopixel.c/.h`, `imu_calc.c/.h`, `imu_task.c/.h`, `safety_task.c/.h`, `imu_msgs.h` (17) | Buddy 4 |
| `app_program/subsystem5/` | `drv_servo.c/.h`, `drv_ultrasonic.c/.h`, `scan_logic.c/.h`, `scan_task.c/.h`, `scan_msgs.h` (9) | Buddy 5 |
| `docs/decisions.md` | 1 (595 lines) | D-01 … D-81 |
| `testing/TRACEABILITY.md` | 1 (140 lines) | requirement → evidence |
| `testing/host/` | `Makefile`, `unit.c/.h`, `test_main.c`, `test_common.c`, `test_s1.c` … `test_s5.c`, `test_nav.c` (11) | host tests |
| `testing/common/`, `testing/navigation/` | `TEST_LOG.md` + `logs/.gitkeep` each (4) | new test areas |

### Template files modified or deleted

`.gitignore`, `README.md`, `TEAM.md`, `app_program/app.h`,
`app_program/app_main.c`, `subsystemN.c/.h/_test.c` (N = 1…5),
`config/config.h`, `lib/liblwip/sysdepend/pico_rp2040/lwipopts.h`,
`mqtt_utk.c`, `mqtt_utk.h`, `testing/README.md`, the seven template
`TEST_LOG.md` files (integration, system, subsystem1–5). Deleted template
examples: `subsystem1/task_led.c`, `subsystem2/task_producer.c`,
`subsystem3/task_consumer.c`, `subsystem4/task_monitor.c`.
[Verified: git diff --name-status]

Not changed (reported only): `include/sys/sysdepend/cpu/rp2040/sysdef.h`
defines `GPIO_OUT` as `(SIO_BASE+0x008)`. [Verified: TEAM.md §7]

### Git history on `main`

[Verified: `git log --first-parent`]

| Commit | What |
|---|---|
| `bbb8f09`, `ecd7195` | user's first uploads (planning notes as README) |
| `ea4c1bb` / `424447f` | KB (PR #1) |
| `56ec731` | notes moved into `docs/` |
| `e870827` | course template imported unchanged |
| … → `ee9601b` | first IS19 code, written by Claude 13 Oct (PRs #2–#7, merged by the user) |
| `180fc7b` / `cc97bbf` | handover drafts 00–04 (PR #8, "Incomplete handover (fixing code first)") |
| `fa142f9`, `c719804`, `e46932a`, `0578f89`, `f49ca00`, `698e39d` | merges of the teammate's six branches (shared, S1…S5), 16 Oct, with Claude's two corrections `c0ac7af`, `a458d8e` |
| `fb7a67b`, `c0e46a9`, `ea95a01` | G15 (D-79), G17 (D-81), G16 (D-80), Claude, 16 Oct |
| `b7bf737`, `58f7d61` | decisions log D-79…D-81, D-80 note |

Teammate commits as applied here (author kept): shared `9378c9c`
`8253c01` `4e4ce12` `5f7c714` `9fb245c` `b0b45b9` `e7adb6f`; S1 `d94a4ed`
`0eaa975` `6d2ffba`; S2 `8a915ef`; S3 `a7beb54` `3a8f6d8` `31eda84`; S4
`cff36f5`; S5 `84a51b5` `b30ee40`. Authors since `cc97bbf`: 17 commits by
the teammate (Dominic Ng Jun Jie), 13 by Claude. [Verified: `git shortlog`]

Branches: `main` and `claude/upbeat-franklin-pkkhdt` both at `58f7d61`.
No pull request was opened for the 16 Oct work, on the user's choice
"Merge branch into main, no PRs". [Verified: TR L5713; git]

## 4. Sources that are NOT in the public repository

[Verified: `sha256sum`, `stat`, `unzip -l` in this container]

| File (upload name) | Bytes | sha256 | Entries | Why excluded |
|---|---|---|---|---|
| `2fe3821d-INF2004_final_merged_handoff_1.zip` (PREV) | 16159 | `cdf2a74589d40b30080b9e6ec81f5e02beb2a9b9259142b88cccd815cf25bd75` | 11 files | lists student IDs |
| `30357074-INF2004_Design_Review_IS19.zip` (RPT + figures) | 5332943 | `87dce642f1777084ce0b15230a2ccca1714071a1def9cb63371ac4efd6623cbb` | 11 | RPT lists student IDs |
| `b5a363a3-Transcript.zip` | 62628 | `a928cf298b25d89f8922b61c2502b8d6cf52392f1a9065227a9d7611ec58750f` | 5 | lecture/consultation transcripts |
| `d18b81e9-Materials_for_project.zip` | 30853547 | `819a2e5f4156fdf6928e129aa8eaa59753a81d79e1a42a1d1da470fedf915bd1` | 25 | course materials and datasheets |
| `f7f8f9ac-hardware_images_not_all_are_relevant.zip` | 8767960 | `a8104248afbf5be9a6ad7886c71af8394334275144fdf121410352d69ab73e4d` | 30 | photos and slides |
| `fd45f295-INF2004_ProjectTemplate-main.zip` (TPL) | 793020 | `918beecbede551f0bf531c8c584a2f3aab8078c351bad15b7743b1a795c5651f` | 924 | already in REPO as `e870827` |
| `fed21e50-lectures.zip` | 15541283 | `500df5bfe5b528fb2e2c80b7a0818baf6eb3d0b1a5d49fdb92dee64d1a96667b` | 20 | course materials |
| `INF2004_Design Review_IS19.pdf` (inside the RPT zip) | 2046709 | `98bda1250182eb7ff9253dd342d94419bbc1e198b52dc106c021d2763c08c14f` | — | lists student IDs |
| `815149fa-IS19_Independent_Review_Actions.md` (TPKG) | — | `57edc0a20abac53f5c9fba9c96a8fe60e032c60d27a3372e1777386339152088` | — | teammate's private review report; its conclusions are summarised in `docs/decisions.md` §F–H and in 08/09 |
| `5f0dc9a1-IS19_consistency_PRs_v2.zip` (TPKG) | 321624 | `2666649d17dd53a396c0a7d5640945142e329690c60f711aafa0efe2e0a25602` | — | teammate's package; its 17 commits are in REPO |
| `33f9916c-INF2004_IS19_corrected_v2.zip` (TPKG) | 1041503 | `20cf2b75e9b465d0d6bf5fdb4e600f67a25dcd0cec5abbb241ad9ecc63843852` | — | equals the six teammate branches merged on `ee9601b` (git tree `9c3b4113`); REPO `698e39d` adds Claude's corrections `c0ac7af` and `a458d8e` on top |
| PREV `00_README.md` … `10_final_content.md` | 33547 total | per file in `handoff_artifacts.zip` → `MANIFEST.md` | 11 | list student IDs |
| Session transcript JSONL | growing (tens of MB) | changes while the session runs | — | private conversation; holds course-material extracts |

PREV's own file list: `00_README.md`, `01_goal_and_scope.md`,
`02_me_and_my_rules.md`, `03_status.md`, `04_facts_and_evidence.md`,
`05_decisions.md`, `06_corrections_and_mistakes.md`, `07_dead_ends.md`,
`08_open_questions.md`, `09_next_steps.md`, `10_final_content.md`.
[Verified: zip listing]

## 5. Container-only working files (scratchpad) and what happened to them

Scratchpad:
`/tmp/claude-0/-home-user-INF2004-IS19/5b9ec659-e66f-563e-800c-986e0ee067a2/scratchpad`.
The container is deleted when the session ends; anything not in the repo or
in a zip is lost. [Verified: directory listing]

| Item | Content | In `handoff_artifacts.zip`? |
|---|---|---|
| `rv/ev/final2/` | 7 builds, warncheck, host 4 configs, test list, for `b7bf737` | yes, `evidence/final_main/` (**master**) |
| `rv/ev/final/` | same for `698e39d` (teammate merge, before G15–G17) | yes, `evidence/reviewed_merge/` |
| `rv/ev/v2/` | same for the TPKG v2 tree | yes, `evidence/teammate_v2/` |
| `rv/ev/per_branch.txt`, `pb_*.log` | each teammate branch alone: host tests and 2 builds | yes, `evidence/per_branch/` |
| `rv/myprobe/`, `rv/theirprobe/` | probes of I-05/I-01 (Claude) and the teammate's probes, rerun on both trees | yes, `evidence/probes/` (sources and outputs; no binaries) |
| `evidence/` (14 Oct) | `ee9601b` builds and host tests, JSON worst case, mbf sizes | yes, `evidence/ee9601b/` |
| `tplchk/template_import_check.txt` | blob comparison template zip vs `e870827` | yes, `evidence/template_import_check.txt` |
| `unvalidated_settings.txt` | list of settings under an UNVALIDATED comment at `58f7d61` | yes, `evidence/final_main/` |
| `baseline_*.log` | template-only builds, 13 Oct 09:33Z | yes, `evidence/template_baseline/` |
| `rv/verify_tree.sh`, `rv/per_branch.sh`, `collect_evidence.sh`, `verify_branch.sh`, `scripts/warncheck.sh`, `cc_cmd*.txt` | scripts that produced the evidence | yes, `scripts/` |
| `gen_testlogs.py`, `gen_testlogs2.py`, `scripts/bars.py`, `contact.py`, `sheet2.py` | generators and image helpers | yes, `scripts/` |
| `worst.c`, `bound.c`, `hdrtest.c`, `cfg_table.py` | helper programs | yes, `helpers/`; `sizes.c` in `superseded/` |
| `stubs/subsystem1…5/` | placeholder files of the 13 Oct base branch | yes, `superseded/base_branch_placeholders/` |
| `b.log`, `b0.log`, `bn.log`, `base.log`, `vb.log`, `build_*.log`, `vw.log` | intermediate 13 Oct builds; exact tree not recorded | yes, `superseded/intermediate_build_logs/` (not evidence) |
| `summary_0938.txt` | first compaction summary (09:38Z 13 Oct); holds the never-sent second-round questions | yes, `history/` |
| `handover_local_backup/` | 14 Oct drafts of 00–04 | no (same text as commit `cc97bbf`) |
| `app_config.h.bak`, `nav_sm_v2.c` | 13 Oct copies | no (in git history) |
| `rv/build_*`, `rv/src_*`, `rv/wt`, `rv/pb_*` trees, `baseline/` | build and source trees | no (build caches / reproducible from git) |
| `*.o`, `bound`, `worst`, `sizes`, `calc/` binaries | build outputs | no |
| `l5v2.txt`, `l5v4.txt`, `txt/`, `img/`, `ex/`, `private/`, `rv/prs/`, `rv/corrected/`, `rv/arch/` | course-material extracts, RPT/PREV text, teammate zips unpacked | no (private or not ours to publish) |
