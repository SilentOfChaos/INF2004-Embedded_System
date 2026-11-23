# System — test log

Tests of the complete car against its requirements (docs/notes/04 section 1), run on the final hardware set-up. Run the full set before every demo and before hand-in.

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| SYS-01 | full course, start to finish, no operator input | FR1, NFR7 | manual | Run the course 3 times. | finished each time in under 5 min |
| SYS-02 | sample barcodes in both directions at 3 speeds | FR2 | manual | 20 passes each at 0.15, 0.25, 0.35 m/s, both orientations (design-note ST-B3-01). | no wrong letter; passes that gave no letter counted (telemetry `bc` unchanged: failed captures are not published) |
| SYS-03 | command acted on at the next junction, final heading | FR3, FR4 | manual | Barcodes A–D before junctions, 5 times each. | right action at the junction; heading within ±5° |
| SYS-04 | junction with no command | FR5 | manual | Junction with no barcode before it. | car stops; "no_command" event |
| SYS-05 | remap over MQTT without reflashing | FR6, NFR8 | manual | Remap E to right; run over barcode E. | turns right at the junction |
| SYS-06 | hump crossing and height | FR7, FR8 | manual | Run over each hump 3 times. | stays on the line; count right; height within ±5 mm |
| SYS-07 | obstacle: stop, profile, bypass, rejoin | FR9, FR10, FR11 | manual | Obstacle on the line, room on one side; then no room on either side. Measure the box and its position. | stops ≥ 10 cm before; `profile` event within ±2 cm of the measurements; passes 5–10 cm away; rejoins. No room: reverses 10 cm once, then stops |
| SYS-08 | telemetry content and rates | FR12, NFR9 | manual | As S1-U30 during a run. | all FR12 fields present; 5 Hz and 1 Hz |
| SYS-09 | route log and run summary on request | FR13, FR14 | manual | Summary command after a run. | every barcode, turn, hump and obstacle with distance and time |
| SYS-10 | collision and STOP: brake, latch, alarm until RESET | FR15, NFR6, NFR16 | manual | As S4-U33 and S4-U35 on the course. | drive removed within 50 ms; latched until RESET |
| SYS-11 | still after power-on until START | FR16 | manual | Power on and wait 60 s; then press START. | no movement before START |
| SYS-12 | motion classes and turn rate reported | FR17 | manual | Run the course; watch the event topic. | each class seen where expected |
| SYS-13 | line loss: search, stop, summary | FR18 | manual | Lift the car off the line while following. | search for 5 ± 0.5 s, then stop and publish the summary |
| SYS-14 | Wi-Fi or broker loss during a run | NFR10 | manual | Switch the broker off mid-run. | car carries on in the same mode |
| SYS-15 | power bank | NFR12 | manual | Full course 3 times on the 5 V / 2.1 A bank. | no reset |
| SYS-16 | CPU load | NFR13 | manual | Timing probes on each task (S3-U33 method); add up busy time. | ≤ 60% |
| SYS-17 | line following range | NFR17, D-71 | manual | Build with `CFG_NAV_CRUISE_MMPS` 150, then 350; at 200 and 1000 lux, across tile joins. Time the car over a marked metre of straight line at each setting: the commanded speed is not the evidence. | stays on the line; measured speeds recorded, covering 0.15 and 0.35 m/s |
| SYS-18 | RAM | NFR19 | manual | Map file (TEAM.md section 6) plus a run-time check of free memory. | below 75% of 264 KB |
| SYS-19 | no more than 3.3 V on any GPIO | NFR11 | manual | Inspect the wiring; meter each sensor output. | ≤ 3.3 V |
| SYS-20 | broker address changed without reflashing | NFR14 | manual | As S1-U33. | car on the new broker |

*Method:* **auto** = `TEST_CHECK` in the on-target test image (`make TEST=1`),
result printed on the console; **manual** = a person observes or measures
the result (scope, logic analyser, ruler, broker client, phone app ...).

## Results

Add a new row for every run. Never edit or delete old rows. No test has
been run on the car yet.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | SYS-01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
