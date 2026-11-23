# Subsystem 5 — test log

**Subsystem:** ultrasonic scanning and obstacle profile  ·  **Owner:** Wong Qian Hui  ·  **Code:** `app_program/subsystem5/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S5-U01 | echo width to range; invalid and out-of-range echoes | D-61 | host | `testing/host/test_s5.c` | `[TEST] S5-U01 PASS` |
| S5-U02 | coarse hits give the fine-scan angles | FR10 | host | `testing/host/test_s5.c` | `[TEST] S5-U02 PASS` |
| S5-U03 | profile of an obstacle ahead: distance, edges, width, clearances | FR10, D-62 | host | `testing/host/test_s5.c` | `[TEST] S5-U03 PASS` |
| S5-U04 | side with room for the car, or none | FR11, D-62 | host | `testing/host/test_s5.c` | `[TEST] S5-U04 PASS` |
| S5-U05 | side scan: obstacle behind only after it was seen beside the car, then clear on valid readings; invalid readings never confirm it | FR11, report 6.5, D-74 | host | `testing/host/test_s5.c` | `[TEST] S5-U05 PASS` |
| S5-U06 | a side with no valid reading beyond the obstacle has no room (clearance −1); with no hit, path clear only if all five coarse readings are valid | FR10, FR11, report 6.5, D-80 (G16) | host | `testing/host/test_s5.c` | `[TEST] S5-U06 PASS` |
| S5-U40 | IF-05 obstacle event packed into two words: the exact expected bits for a plan, a give-up and a simple event; every type round-trips in 3 bits, a type that does not fit is reported; for 20,000 random scans (near and far obstacles, gaps, invalid readings; readings every 6° from 0 to 180°), every plan the profile code makes fits the packed fields exactly | IF-05, D-90 (minimum-bit change) | host | `testing/host/test_s5.c` | `[TEST] S5-U40 PASS` |
| S5-U41 | IF-06 scan request packed into one byte: the exact expected bits; every type and side round-trips (side kept only for a bypass) | IF-06, D-90 | host | `testing/host/test_s5.c` | `[TEST] S5-U41 PASS` |
| S5-U42 | echo record packed into two words by the ISR's own function (`ultrasonic_echo.h`): the exact expected bits; the width in 15 bits, a longer one kept at 32767 µs (not cut): every width from 0 to 200 ms gives the same range and validity as the full 32-bit width | D-61, D-90 | host | `testing/host/test_s5.c` | `[TEST] S5-U42 PASS` |
| S5-U20 | ultrasonic sensor answers its pings (an echo pulse starts), with a valid range or not | A5, D-57, D-104 | auto | `subsystem5_test.c` | `[TEST] S5-U20 PASS` |
| S5-U21 | request queue exists and Scan drains it (seen empty within 200 ms) | IF-06 | auto | `subsystem5_test.c` | `[TEST] S5-U21 PASS` |
| S5-U30 | servo positions and travel | A5, D-97 | manual | 1. Command 30°, 90° and 150° (`servo_set_angle()`, e.g. from a test build) and check each with a protractor. 2. With the power off, turn the horn gently by hand to each end and note the angles. | 30°, 90°, 150° within ±5° (else adjust `CFG_SERVO_US_AT_*`); the measured ends recorded, and `CFG_SCAN_SERVO_MIN_DEG`/`MAX_DEG` kept inside them (D-97) |
| S5-U31 | range accuracy | A5, FR9 | manual | Flat target at 10, 20, 30, 50, 100 cm, 10 readings each. | within ±1 cm |
| S5-U32 | ping spacing | NFR5, D-75 | manual | Logic analyser on GP4 for 10 s, while monitoring and during a scan. | pings at least 60 ms apart (`CFG_US_MIN_PING_GAP_MS`), normally 65 ms |
| S5-U33 | obstacle profile accuracy; passing it | FR10, FR11, D-72 | manual | 1. Box of known size at known positions; read the `profile` event on `inf2004/IS19/event`. 2. Let the car pass it and watch the scan status. | position, width, closest point, clearances within ±2 cm; `behind` only once the box is behind the car, with the side scan 30° short of square (D-97), and the car's rear clear of the box after the extra travel (else adjust `CFG_SCAN_SIDE_CLEAR_MM`, `CFG_SCAN_BEHIND_COUNT`, `CFG_SCAN_JUMP_MM`, `CFG_SCAN_DEPTH_MM`, `CFG_NAV_PAST_EXTRA_MM`) |
| S5-U34 | open space reads as a valid “nothing in range”, not a timeout | A5, D-61 | manual | 1. Point the sensor at open space (nothing within 4 m). 2. Scope the Echo pulse width with no target. 3. Read the scan status `range` and `invalid_run`. | `invalid_run` stays 0; `CFG_US_ECHO_TIMEOUT_MS` is longer than the measured no-target pulse (else raise it; it must stay below the 65 ms scan period, and if it cannot, record that against A5) |
| S5-U35 | scan with missing echoes: an invalid reading is repeated once at the same angle; a side never seen is not chosen | FR11, report 6.5, D-80 | manual | 1. Box on the line; let the car stop for it. During the coarse scan cover the sensor with a cloth at one angle for one ping, then (next run) for two pings. 2. Repeat with the right side hidden: cover the sensor whenever it points right of the box. | 1. One ping: one extra ping at that angle and the scan carries on. Two pings: that angle counts as invalid. 2. `cr` −1 in the `profile` event and the car passes on the left; with neither side seen it reverses 10 cm, scans again, then stops (NO SAFE ROUTE) |
| S5-U36 | no echo in a run is a status bit only | Change Requirement 1, report 6.5, D-102, D-104 | manual | 1. Start a run with the broker up. 2. On a straight, unplug the sensor's Echo lead (or cover the sensor) for about 1 s, then put it back. 3. Watch the heartbeat on `inf2004/IS19/status`. | NO SAFE ROUTE: SAFE HOLD (`hold` `no_safe_route`), `fault` stays 0, `mask` gains 512 (no echo) within 3 pings (about 200 ms) and loses it at the next valid reading |

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
| YYYY-MM-DD | S5-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
