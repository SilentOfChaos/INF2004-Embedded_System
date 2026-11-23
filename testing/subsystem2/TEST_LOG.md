# Subsystem 2 — test log

**Subsystem:** motion (motors, encoders, PID)  ·  **Owner:** Dominic Ng Jun Jie  ·  **Code:** `app_program/subsystem2/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S2-U01 | encoder counts ↔ mm (uses the UNVALIDATED geometry) | FR19 | host | `testing/host/test_s2.c` | `[TEST] S2-U01 PASS` |
| S2-U02 | pivot-turn counts | FR19, D-56 | host | `testing/host/test_s2.c` | `[TEST] S2-U02 PASS` |
| S2-U03 | wheel speed from counts over the real time between reads (40 ms of counts read at once give the same speed as 20 ms of half as many), and odometry heading, with signs | FR12, NFR18, D-99 | host | `testing/host/test_s2.c` | `[TEST] S2-U03 PASS` |
| S2-U04 | PID: feed-forward, duty limit, anti-windup, zero target brakes | NFR3, NFR18 | host | `testing/host/test_s2.c` | `[TEST] S2-U04 PASS` |
| S2-U05 | soft start; slowing down at once; stop before reversing | D-30 | host | `testing/host/test_s2.c` | `[TEST] S2-U05 PASS` |
| S2-U06 | drive from the PID: never against the target's direction (slowing down is less drive, never reverse drive), the start-up duty only while the wheel stands still, zero for a zero target | NFR18, FR19, D-100 | host | `testing/host/test_s2.c` | `[TEST] S2-U06 PASS` |
| S2-U07 | stall and encoder-fault watch (`motion_stall_step()`): a wheel driven at `CFG_MOTION_STALL_DUTY_PM` or more with no count for `CFG_MOTION_STALL_MS` is reported once, exactly when that time is reached (both wheels: stall; one while the other counts: encoder fault, on either side and in reverse; one while the other neither drives nor counts: stall); less drive or any count starts it again, and counting again re-arms it; a very late cycle saturates; with today's PID gains a wheel held still at any target from 10 to 350 mm/s is reported only once the PID's output is within 20 ‰ of the most it gives, so a hump the PID can climb never trips it | Change Requirement 1, FR7, FR15, D-105 | host | `testing/host/test_s2.c` | `[TEST] S2-U07 PASS` |
| S2-U40 | IF-07 command packed into one word: the exact expected bits for a drive, a move, a turn, hold and stop; every speed and difference from −1000 to 1000, every move and turn size from 0 to 32767 (15 bits) with every move ID round-trip, the bits above stay 0; fields a kind does not carry read 0; a negative move is its size, as Motion always took it; a size above 32767 is reported (Motion's API then refuses the command, D-88) and kept at the nearest end; other impossible values reported, a type that is not a command kept as KEEPALIVE | IF-07, D-88 (minimum-bit change) | host | `testing/host/test_s2.c` | `[TEST] S2-U40 PASS` |
| S2-U41 | IF-08 status packed into two words: the exact expected bits; every heading from −17999 to 18000, the move ID and 100,000 random 31-bit distances (both ends too) round-trip; the farthest distances the counters can give fit; a distance outside 31 bits is reported and kept at the nearest end | IF-08, D-88 | host | `testing/host/test_s2.c` | `[TEST] S2-U41 PASS` |
| S2-U42 | encoder pair in one word: a step on one wheel never changes the other half, also across its wrap; the driver's own add (`encoder_add_pair()`, which `encoder_add_counts()` calls) puts each wheel in its own total and adds nothing on a second read with no counts; over 20,000 reads with up to 3000 counts per wheel between reads, its totals equal plain 32-bit counters; 32767 counts between reads are exact | NFR3, D-88 | host | `testing/host/test_s2.c` | `[TEST] S2-U42 PASS` |
| S2-U20 | command queue exists and Motion drains it (seen empty within 100 ms) | IF-07 | auto | `subsystem2_test.c` | `[TEST] S2-U20 PASS` |
| S2-U21 | Motion's event flag exists (cyclic release, reset) | IF-09 | auto | `subsystem2_test.c` | `[TEST] S2-U21 PASS` |
| S2-U30 | encoder counts per wheel turn, in the chosen decoding mode | A2, D-28 | manual | Turn each wheel 10 times by hand; read the counts on the console. | value recorded; `CFG_ENC_COUNTS_PER_REV` updated |
| S2-U31 | wheel diameter, track width, sensor-to-axle distance, car width | A2 | manual | Measure on the car. | values recorded; `app_config.h` updated |
| S2-U32 | motor and encoder directions |  | manual | Command a slow forward drive with the wheels off the ground. | both wheels forward; both counts increase (else set the `INVERT` settings) |
| S2-U33 | speed loop period | NFR3 | manual | Toggle a spare pin each Motion cycle; logic analyser for 10 s. | 20 ± 2 ms |
| S2-U34 | straight driving | NFR18 | manual | Drive 1 m straight at 250 mm/s, 5 times. | each wheel within ±10% of target; drift ≤ 2 cm per metre |
| S2-U35 | commanded distance | FR19 | manual | `moveForward(500)` 5 times; measure. | 500 mm ± 5% |
| S2-U36 | commanded turns | FR19 | manual | `turnLeft`/`turnRight` 45° and 90°, 5 times each; measure. | within ±5° |
| S2-U37 | encoder signal levels and edges | NFR11, A2, D-28 | manual | Scope GP0 and GP16 while turning each wheel by hand, with `CFG_ENC_PULL_UP` 1 and then 0. | clean 0–3.3 V edges, never above 3.3 V; keep the setting that gives clean edges |
| S2-U38 | stall and encoder-fault watch on the car: thresholds `CFG_MOTION_STALL_DUTY_PM` and `CFG_MOTION_STALL_MS` | Change Requirement 1, FR7, FR15, risk R5, D-105 | manual | 1. Wheels off the ground: start a 90° turn and unplug the left encoder (GP0/GP1) during it; repeat with both encoders unplugged. 2. On the floor, drive and hold one wheel by hand; then both. 3. Cross the hump 5 times at `CFG_NAV_HUMP_MMPS`, and start from rest 5 times. Watch the console and the heartbeat `mask`. | 1. FAULT within about `CFG_MOTION_STALL_MS` + 0.3 s of the wheel's duty passing `CFG_MOTION_STALL_DUTY_PM`: `encoder_fault` (detail 1) with one encoder out, `motor_stall` (detail 3) with both. 2. FAULT: `encoder_fault` with one wheel held (it looks like a dead encoder, R5), `motor_stall` with both. 3. No FAULT on the hump or at the start (else raise the time or the duty, and keep S2-U07 passing) |

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
| YYYY-MM-DD | S2-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
