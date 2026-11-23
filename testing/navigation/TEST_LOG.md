# Navigation (shared) — test log

**Part:** Navigation, the system statechart  ·  **Owner:** whole team  ·  **Code:** `app_program/navigation/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| NAV-U01 | BOOT → SELF-TEST → READY → LINE FOLLOWING on START only | FR16 | host | `testing/host/test_nav.c` | `[TEST] NAV-U01 PASS` |
| NAV-U02 | SELF-TEST failure reported once after 3 s | report 5, D-39 | host | `testing/host/test_nav.c` | `[TEST] NAV-U02 PASS` |
| NAV-U03 | steers towards the line; keeps the last error when the line is briefly gone; the commanded speed stays inside NFR17's 0.15–0.35 m/s | FR1, NFR17, D-52, D-71 | host | `testing/host/test_nav.c` | `[TEST] NAV-U03 PASS` |
| NAV-U04 | SD1: letter B held, junction, axle onto it, turn left 90°, find the line | FR3, FR4 | host | `testing/host/test_nav.c` | `[TEST] NAV-U04 PASS` |
| NAV-U05 | right, U-turn and straight at a junction | FR4, D-34 | host | `testing/host/test_nav.c` | `[TEST] NAV-U05 PASS` |
| NAV-U06 | junction with no command: SAFE HOLD and "no command" logged; RESET → READY | FR5, D-54 | host | `testing/host/test_nav.c` | `[TEST] NAV-U06 PASS` |
| NAV-U07 | line lost: sweep search for 5 s, then COMPLETE with a summary | FR18, D-33 | host | `testing/host/test_nav.c` | `[TEST] NAV-U07 PASS` |
| NAV-U08 | line found during the search: following again | FR18 | host | `testing/host/test_nav.c` | `[TEST] NAV-U08 PASS` |
| NAV-U09 | no line within 3 s of a turn: SAFE HOLD | report 5 | host | `testing/host/test_nav.c` | `[TEST] NAV-U09 PASS` |
| NAV-U10 | barcode and junction events ignored while turning, including the letter or failure of a read that started while turning; a read counts only if the car was following (LINE FOLLOWING or HUMP CROSSING) from its start to its result: ignored if the line is lost, a junction is reached or a fault latches during it, or if it started in READY; a hump during a read does not stop it counting | D-53 | host | `testing/host/test_nav.c` | `[TEST] NAV-U10 PASS` |
| NAV-U11 | a newer letter replaces the held one; a failed read keeps it | D-35 | host | `testing/host/test_nav.c` | `[TEST] NAV-U11 PASS` |
| NAV-U12 | remap and default map, each acknowledged | FR6, NFR8 | host | `testing/host/test_nav.c` | `[TEST] NAV-U12 PASS` |
| NAV-U13 | fault from a running mode; restart goes back through SELF-TEST | FR15, D-64 | host | `testing/host/test_nav.c` | `[TEST] NAV-U13 PASS` |
| NAV-U14 | hump: slower but never commanded below 0.15 m/s, levels frozen (`CFG_NAV_HUMP_FREEZE_LEVELS` 1), height logged | FR7, FR8, NFR17, D-23, D-71 | host | `testing/host/test_nav.c` | `[TEST] NAV-U14 PASS` |
| NAV-U15 | obstacle: stop, scan, go round on the left, rejoin the line; after the side scan's `behind`, the extra travel includes the scan's look-ahead (passing on either side) | FR9, FR11, D-36, D-97 | host | `testing/host/test_nav.c` | `[TEST] NAV-U15 PASS` |
| NAV-U16 | no room: reverse 10 cm once, scan again, then SAFE HOLD; path clear: carry on | FR11 | host | `testing/host/test_nav.c` | `[TEST] NAV-U16 PASS` |
| NAV-U17 | a running move is kept alive every tick | report 6.2, D-55 | host | `testing/host/test_nav.c` | `[TEST] NAV-U17 PASS` |
| NAV-U18 | fused heading starts from odometry and is nudged by the compass | D-40 | host | `testing/host/test_nav.c` | `[TEST] NAV-U18 PASS` |
| NAV-U19 | the action list never overflows in one step | NFR15 | host | `testing/host/test_nav.c` | `[TEST] NAV-U19 PASS` |
| NAV-U20 | every event queue exists and Navigation drains it (seen empty within 20 ms) | IF-01, 03, 05, 08, 10 | auto | `navigation_test.c` | `[TEST] NAV-U20 PASS` |
| NAV-U21 | Navigation's event flag exists | IF-16, IF-17 | auto | `navigation_test.c` | `[TEST] NAV-U21 PASS` |
| NAV-U30 | one START press starts one run | FR16, D-65 | manual | Press START briefly, then hold it for 2 s. | one run each time; nothing while held |
| NAV-U40 | a line record whose sample count has not moved for 20 ms while running: not used as line seen (a line search goes on), one LINE_DATA fault reported with source Navigation and the time as its detail, at the first tick 20 ms on, also when a tick comes late; a moving count, failed reads included, or ticks close together never trip it; a new sample is used at once; not checked outside a run | FR1, FR15, IF-02, D-94 (G19) | host | `testing/host/test_nav.c` | `[TEST] NAV-U40 PASS` |
| NAV-U41 | line reads failing across RESET, with the Line task's health and failed-read count built from its own pure parts: SELF-TEST never passes while the reads fail, however soon RESET comes, and fails at its deadline; after the reads recover, READY only after a whole good window; a new run of failures in READY is reported again and START is refused | FR1, FR15, IF-17, D-79, D-96 | host | `testing/host/test_nav.c` | `[TEST] NAV-U41 PASS` |
| NAV-U42 | after COMPLETE (the Line task idle), a fault and RESET, with the Line task's health, failed-read count and RUN OVER built from its own pure parts and Navigation's actions: SELF-TEST wakes the Line task and passes only after a whole window of new good reads, never on health from idle cycles; with the reads failing it does not pass and the failures are reported | FR15, FR16, IF-11, IF-17, D-96, D-98 | host | `testing/host/test_nav.c` | `[TEST] NAV-U42 PASS` |

*Method:* **host** = host unit test, built and run with `make` in
`testing/host/` (no hardware; see `testing/README.md`);
**auto** = `TEST_CHECK` in the on-target test image (`make TEST=1`), result
printed on the console; **manual** = a person observes or measures the
result (scope, logic analyser, ruler, broker client, phone app ...).

## Results

Add a new row for every run. Never edit or delete old rows. No test has
been run on the car yet.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | NAV-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
