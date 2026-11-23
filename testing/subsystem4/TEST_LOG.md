# Subsystem 4 — test log

**Subsystem:** IMU and safety  ·  **Owner:** Yee Sheng Chua  ·  **Code:** `app_program/subsystem4/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S4-U01 | roll and pitch from gravity | FR8 | host | `testing/host/test_s4.c` | `[TEST] S4-U01 PASS` |
| S4-U02 | tilt-compensated heading on rotated test vectors (36 headings × 9 tilts) | FR4, D-40 | host | `testing/host/test_s4.c` | `[TEST] S4-U02 PASS` |
| S4-U03 | vector length; hump height from the table | FR8 | host | `testing/host/test_s4.c` | `[TEST] S4-U03 PASS` |
| S4-U04 | hump: on after 100 ms above, off when level, peak kept | FR8 | host | `testing/host/test_s4.c` | `[TEST] S4-U04 PASS` |
| S4-U05 | motion classes: still, steady, accelerating, turning, climbing, descending, impact | FR17, D-58 | host | `testing/host/test_s4.c` | `[TEST] S4-U05 PASS` |
| S4-U06 | sample check: failed read or below 0.3 g = bad data, 0.3–3.5 g = used, above 3.5 g = overrange; while calibrating, a jolt of 1 g or more from 1 g restarts the average | FR15, report 6.4, D-42, D-81 (G17) | host | `testing/host/test_s4.c` | `[TEST] S4-U06 PASS` |
| S4-U07 | Safety's fault latch and RESET handshake (`safety_latch.c`): every state (nothing latched, latched, RESET waiting for Motion) against every event (a fault, a status report, RESET with the cause cleared or still present, Motion's acknowledgement, no acknowledgement in time); a fault while RESET waits cancels the reset and becomes the latched cause, so the acknowledgement that follows releases nothing; RESET again then releases; a status report changes no state | FR15, NFR16, IF-04, IF-09, D-95 (G20), D-102 | host | `testing/host/test_s4.c` | `[TEST] S4-U07 PASS` |
| S4-U08 | fault mask (`safety_latch.c`, the fault table in `imu_msgs.h`): one bit per cause, 11 bits; Wi-Fi loss and no echo set and clear their bits without latching, counting as faults or cancelling a waiting RESET; every latching cause sets its bit, the counted ones too; the release clears only the latching bits; a cause outside the table latches; 100,000 random reports, RESET presses, acknowledgements and time-outs agree with the fault table | Change Requirement 1, NFR10, IF-04, D-102 | host | `testing/host/test_s4.c` | `[TEST] S4-U08 PASS` |
| S4-U40 | IF-03 IMU event packed into one word: the exact expected bits for a heading, a hump end, a hump start and a class; every heading from −17999 to 18000 and every peak roll from 0 to 18000 with the height the calibration table gives for it round-trip, and every table height fits the 7-bit field | IF-03, D-89 (minimum-bit change) | host | `testing/host/test_s4.c` | `[TEST] S4-U40 PASS` |
| S4-U41 | IF-04 fault report packed into one word: the exact expected bits; every cause (11, in 4 bits) and source with details up to 8191 round-trip; a status report carries its cleared flag; impossible values reported | IF-04, D-89, D-102 | host | `testing/host/test_s4.c` | `[TEST] S4-U41 PASS` |
| S4-U20 | no fault latched after start-up on a still car | FR15 | auto | `subsystem4_test.c` | `[TEST] S4-U20 PASS` |
| S4-U21 | IMU identified, calibrated and healthy | D-57 | auto | `subsystem4_test.c` | `[TEST] S4-U21 PASS` |
| S4-U22 | fault queue exists and Safety drains it | IF-04 | auto | `subsystem4_test.c` | `[TEST] S4-U22 PASS` |
| S4-U30 | IMU axes on the car |  | manual | Lift the left side: roll on the console must rise; turn the car left: heading must rise. | signs correct (else set `CFG_IMU_SWAP`/`INVERT`) |
| S4-U31 | magnetometer hard-iron offsets | FR4 | manual | Turn the car through 360° on the floor; record min and max of each axis. | offsets recorded; `CFG_IMU_MAG_OFF_*` updated |
| S4-U32 | hump height table | FR8, A4 | manual | Drive one wheel over blocks of known height (10, 20, 30, 40 mm), 3 times each. | reported height within ±5 mm; table updated |
| S4-U33 | motor drive removed after a collision or STOP | NFR6, FR15 | manual | Scope GP8 and a pin toggled at the fault; trigger by STOP command and by tapping the car. | pins low within 50 ms |
| S4-U34 | watchdog resets the Pico when a critical task stops checking in | report section 3, D-44 | manual | On a test branch only, remove the Line task's check-in. | reset within about 250 ms |
| S4-U35 | RESET releases only when the cause is cleared and Motion acknowledged | NFR16 | manual | Latch a fault; press RESET while the car is still being shaken, then when still. | no release while shaking; release after, motors stay at 0 |
| S4-U36 | IMU sample rates | NFR4 | manual | Count samples on the console for 10 s. | accelerometer 100 ± 5 Hz, magnetometer 75 ± 5 Hz |
| S4-U37 | a hard collision latches at once; bad data latches in every IMU state; a jolt while calibrating only restarts it | FR15, report 6.4, D-42, D-76, D-81 | manual | 1. Car READY (IMU monitoring): tap the bumper hard enough to saturate the ±4 g range; RESET; repeat 5 times. 2. Power on with the GY-511 SDA wire loose during the first second (CALIBRATING). 3. Power on and tap the car hard during the first second. | 1. FAULT latched at once each time, cause `collision`, `detail` = |a| in mg. 2. FAULT latched, cause `imu_data`; RESET clears it once the IMU calibrates again. 3. No fault; calibration ends about 1 s after the last tap; SELF-TEST passes |
| S4-U38 | turn rate only from the compass on flat ground | FR17, report 6.4, D-77 | manual | Turn the car by hand on the floor, then lift one side past the hump threshold and turn it again. | flat: `rate` follows the turn; tilted: `rate` holds its last value; level again: follows the turn |

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
| YYYY-MM-DD | S4-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
