# Integration — test log

Tests that two or more subsystems work together: one or more for every interface in TEAM.md section 5. Both owners agree on the test and sign off the result.

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| INT-01 | Line events reach Navigation: barcode letter held, junction acted on | IF-01 | manual | Push the car by hand over barcode B and a crossbar in a test run; watch the console and the route log. | letter B logged, then AT JUNCTION and a left turn |
| INT-02 | line-error record read by Navigation every cycle | IF-02 | manual | Move the car across the line by hand while READY; watch the 5 Hz telemetry `line` field. | error changes sign from side to side; never stale for more than 10 ms |
| INT-03 | IMU events reach Navigation: hump start/end, heading | IF-03 | manual | Tilt the car past the hump threshold and back while following. | HUMP CROSSING, then a hump route entry |
| INT-04 | faults from IMU, Command and a full queue latch Safety | IF-04 | manual | Tap the car; publish STOP; (test branch) fill a control queue. | FAULT latched with the right cause and source each time |
| INT-05 | obstacle events reach Navigation | IF-05 | manual | Put a box 25 cm ahead while following. | AVOIDING, plan event logged |
| INT-06 | scan requests reach Obstacle Scan | IF-06 | manual | Same run as INT-05; watch the servo. | coarse then fine scan after the car stops |
| INT-07 | Motion carries out Navigation's commands | IF-07 | manual | Run NAV junction sequence with the wheels off the ground. | forward, turn, stop as commanded |
| INT-08 | odometry and move-done reach Navigation | IF-08 | manual | Same run as INT-07. | each move finishes the right step; distance in telemetry |
| INT-09 | RESET handshake: Motion zeroes before Safety releases | IF-09, NFR16 | manual | Latch a fault while driving; press RESET. | zero ack, then release; motors stay still until START |
| INT-10 | remap command reaches Navigation and is acknowledged | IF-10, NFR8 | manual | As S1-U31. | ack within 1 s |
| INT-11 | level adaptation frozen on a hump; RUN OVER after COMPLETE; sensing again in SELF-TEST | IF-11, D-98 | manual | Cross a hump; then let the line run out; then publish STOP and press RESET. | levels unchanged on the hump; Line idle after COMPLETE; after RESET the Line task reads again and READY comes only after its health is back |
| INT-12 | status records from every task reach Telemetry | IF-12, FR12 | manual | Watch the 5 Hz telemetry for 30 s. | line, motion, IMU, scan, mode and safety fields all change |
| INT-13 | Telemetry messages reach the broker | IF-13 | manual | As S1-U30. | all four topics seen |
| INT-14 | commands from the broker reach the Command task | IF-14 | manual | Publish each command once. | one ack per command |
| INT-15 | broker change reaches the MQTT code | IF-15, NFR14 | manual | As S1-U33. | car moves to the new broker |
| INT-16 | FAULT level stops Motion and Navigation; restart after release | IF-16, FR15 | manual | Latch a fault while driving; then RESET. | Motion BRAKED, Navigation FAULT; then SELF-TEST, READY |
| INT-17 | health levels let SELF-TEST pass; a missing sensor fails it; a working ultrasonic sensor facing open space passes (it answers without a valid range) | IF-17, D-57, D-104 | manual | 1. Boot normally. 2. Boot with the ultrasonic sensor unplugged. 3. Boot with it plugged in and facing open space (nothing within 4 m). | 1. READY. 2. FAULT (self-test) after 3 s. 3. READY |
| INT-18 | watchdog check-ins from every critical task | IF-18, D-44 | manual | As S4-U34, once for each task. | reset each time |
| INT-19 | obstacle profile goes from Obstacle Scan through Telemetry to the broker | IF-12, IF-13, FR10, D-72 | manual | Same run as INT-05 with `NET=lwip MQTT=1`; `mosquitto_sub -v -t 'inf2004/IS19/event'`. | one `profile` event for each scan that finds the box (`rev` 1 after reversing), with the side Navigation then takes |
| INT-20 | barcode reads around junction turns and humps: only a read that starts and ends while following is used | IF-01, FR2, FR3, D-53 | manual | Sample barcodes: one before a junction; one just after it, so the barcode sensor crosses it while the car turns; one beside a hump. Run 5 times; watch the barcode and route events (`inf2004/IS19/event`) or the console. | the barcode before the junction decides the turn; the one crossed while turning is never acted on; the one beside the hump gives its letter or "capture failed", never a wrong letter |

*Method:* **auto** = `TEST_CHECK` in the on-target test image (`make TEST=1`),
result printed on the console; **manual** = a person observes or measures
the result (scope, logic analyser, ruler, broker client, phone app ...).

## Results

Add a new row for every run. Never edit or delete old rows. No test has
been run on the car yet.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | INT-01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
