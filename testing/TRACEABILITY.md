# Traceability: requirement → design → code → test

Lecture 6's matrix for the W6 report's requirements (FR1–FR19,
NFR1–NFR19). It answers two questions: is every requirement designed,
built and tested, and is anything built that no requirement asks for?

- **Design**: the report's section or figure (§3 task table, §6.1–6.5
  statecharts, Fig 5 system statechart, SD1, SD2), the tasks
  ([TEAM.md](../TEAM.md) section 4), the interfaces IF-nn (TEAM.md
  section 5) and the decisions D-nn ([docs/decisions.md](../docs/decisions.md)).
- **Code**: the files under `app_program/` that implement it.
- **Tests**: test IDs from the `TEST_LOG.md` files in this folder; their
  *Requirement / interface* column lists the same requirements.
- **How**: U = unit test (host or on-target), I = integration test,
  S = system test, Insp = inspection or analysis (Lecture 6, step 1).

No test has been run on the car yet, so this table says how each
requirement will be verified, not that it has been. Keep it up to date in
the same pull request whenever a requirement, design element, file or test
changes: it is the starting point for an impact analysis.

## Functional requirements

| Req | Requirement (short) | Design | Code | Tests | How |
|---|---|---|---|---|---|
| FR1 | follow the line from start to finish | §6.3 LINE TRACKING; Fig 5 LINE FOLLOWING; Line, Navigation, Motion; IF-02, IF-07, IF-17; D-52, D-79, D-94, D-96, D-98 | `subsystem3/line_track.c`, `steering.c`, `line_task.c` (failed reads), `line_keep.c` (health); `navigation/nav_sm.c` (`nav_sm_follow`; a record that stopped changing, `nav_sm_line_fresh`) | S3-U11, S3-U14, S3-U18, S3-U19, NAV-U03, NAV-U40, NAV-U41, NAV-U42, SYS-01 | U, S |
| FR2 | decode Code 39 A–Z in either direction | §6.3 BARCODE; Barcode ISR, Line; D-25–D-27 (approved improvements), D-78 | `subsystem3/drv_barcode.c`, `bc_capture.c`, `bc_decode.c`, `line_task.c` | S3-U01–S3-U05, S3-U09, S3-U16, S3-U17, S3-U36, SYS-02 | U, S |
| FR3 | act at the next junction, not at the barcode | SD1; Fig 5 AT JUNCTION; IF-01; D-35, D-53 | `navigation/nav_sm.c` (`nav_sm_barcode_event`, `nav_sm_enter_junction`); `subsystem3/line_track.c` | S3-U12, NAV-U04, NAV-U10, INT-20, SYS-03 | U, I, S |
| FR4 | straight, left, right or U-turn, within ±5° | SD1; §6.2 TURNING; IF-07, IF-08; D-34, D-56 | `navigation/nav_sm.c`; `subsystem2/motion_ctl.c`, `motion_task.c`; `subsystem4/imu_calc.c` (heading) | NAV-U04, NAV-U05, S4-U02, S4-U31, SYS-03 (turn accuracy: S2-U36) | U, S |
| FR5 | no command held at a junction: stop and report | SD1; Fig 5 AT JUNCTION → SAFE HOLD; D-54 | `navigation/nav_sm.c` (`nav_sm_enter_junction`) | NAV-U06, SYS-04 | U, S |
| FR6 | default map A–D; remap over MQTT | §6.1 commands; IF-14, IF-10; D-66 | `subsystem3/letter_map.c`; `subsystem1/cmd_parse.c`, `command_task.c`; `navigation/nav_sm.c` | S3-U15, S1-U01, NAV-U12, S1-U31, SYS-05 | U, S |
| FR7 | keep following while crossing a hump | Fig 5 HUMP CROSSING; §6.4 ON HUMP; IF-03, IF-11; D-23, D-38 | `navigation/nav_sm.c`; `subsystem4/imu_task.c`, `imu_calc.c` | NAV-U14, SYS-06 | U, S |
| FR8 | detect humps; height ±5 mm; count and highest | §6.4 ON HUMP and calibration table; A4 | `subsystem4/imu_calc.c` (`imu_calc_tilt`, `imu_hump_step`), `imu_task.c`; `subsystem1/tlm_format.c` | S4-U01, S4-U03, S4-U04, S4-U32, NAV-U14, SYS-06 | U, S |
| FR9 | detect an obstacle; stop at least 10 cm before it | §6.5 MONITORING; Fig 5 AVOIDING; IF-05; D-61, D-104 | `subsystem5/drv_ultrasonic.c`, `scan_task.c` (no-echo bit), `scan_logic.c` (`scan_range_mm`); `navigation/nav_sm.c` | S5-U31, S5-U34, S5-U36, NAV-U15, SYS-07 | U, S |
| FR10 | coarse then fine scan; report position, width, closest point, clearances (±2 cm) | §6.5 COARSE SCAN, FINE SCAN, PLANNING; IF-12; D-62, D-72, D-97 | `subsystem5/scan_task.c`, `scan_logic.c` (`scan_profile`), `drv_servo.c`; `subsystem1/tlm_format.c` (`tlm_format_obstacle`) | S5-U02, S5-U03, S1-U11, S5-U30, S5-U33, INT-19, SYS-07 | U, I, S |
| FR11 | pass on the side with more room, 5–10 cm away, rejoin; else reverse 10 cm and rescan once | §6.5 PLANNING, BYPASSING; IF-05, IF-06; D-36, D-48, D-74, D-80, D-97 | `subsystem5/scan_logic.c` (`scan_profile`, `scan_side_step`, `scan_all_valid`), `scan_task.c`; `navigation/nav_sm.c` (`nav_sm_plan`, `nav_sm_past_extra_mm`) | S5-U04, S5-U05, S5-U06, NAV-U15, NAV-U16, S5-U33, S5-U35, SYS-07 | U, S |
| FR12 | telemetry: state, speeds, counts, line, barcodes, humps, obstacles, distance | §6.1; IF-12, IF-13; D-49 | `subsystem1/telemetry_task.c`, `tlm_format.c`; status records from every task | S2-U03, INT-12, SYS-08 | U, I, S |
| FR13 | time-stamped route log | §6.1; IF-12; D-51, D-60 | `subsystem1/tlm_format.c`, `telemetry_task.c`; `navigation/nav_sm.c` (`nav_sm_route`) | S1-U06, S1-U09, S1-U32, SYS-09 | U, S |
| FR14 | MQTT commands: remap, summary, stop | §6.1 Command task; IF-14, IF-10, IF-04; D-66, D-101 | `subsystem1/cmd_parse.c` (`cmd_is_stop`), `command_task.c`, `mqtt_bridge.c` | S1-U01, S1-U02, S1-U07, S1-U12, S1-U32, SYS-09 | U, S |
| FR15 | collision or STOP: brake and hold a fault until reset | SD2; §6.4; Safety; IF-04, IF-09, IF-16; A14; D-15, D-46, D-47, D-76, D-79, D-81, D-94, D-95, D-96, D-98, D-101, D-102, D-105 | `subsystem4/safety_task.c`, `safety_latch.c` (latch, fault mask), `imu_msgs.c` (fault table), `imu_task.c`, `imu_calc.c` (`imu_calc_check_sample`); `subsystem3/line_task.c` (LINE_DATA); `subsystem2/motion_task.c`, `motion_ctl.c` (stall and encoder-fault watch); `navigation/nav_sm.c` (FAULT; LINE_DATA for a record that stopped changing) | S4-U06, S4-U07, S4-U08, S4-U20, S4-U33, S4-U37, S3-U18, S2-U07, S2-U38, NAV-U13, NAV-U40, NAV-U41, NAV-U42, INT-16, SYS-10 | U, I, S |
| FR16 | still after power-on until START | Fig 5 BOOT, SELF-TEST, READY; D-57, D-65, D-98 | `navigation/nav_sm.c`, `nav_task.c` (START button) | NAV-U01, NAV-U30, NAV-U42, SYS-11 | U, S |
| FR17 | motion classes and turn rate in telemetry | §6.4 MONITORING; D-58, D-77 | `subsystem4/imu_calc.c` (`imu_classifier_step`), `imu_task.c`; `subsystem1/tlm_format.c` | S4-U05, S4-U38, SYS-12 | U, S |
| FR18 | line lost: search 5 s, then stop, end the run, publish a summary | §6.3 LINE LOST; Fig 5 LINE SEARCH → COMPLETE; D-33, D-99 | `subsystem3/line_track.c`, `line_task.c` (measured time); `navigation/nav_sm.c` (`nav_sm_enter_search`, `nav_sm_enter_complete`); `subsystem1/tlm_format.c` | S3-U12, NAV-U07, NAV-U08, S1-U07, SYS-13 | U, S |
| FR19 | distance ±5%, turns ±5° from the encoders | §6.2 DRIVING, TURNING; IF-07, IF-08; D-28, D-31, D-56 | `subsystem2/drv_encoder.c`, `motion_ctl.c`, `motion_task.c` | S2-U01, S2-U02, S2-U35, S2-U36 | U |

## Non-functional requirements

| Req | Requirement (short) | Design | Code | Tests | How |
|---|---|---|---|---|---|
| NFR1 | line sensors every 5 ± 1 ms | §3 Line 5 ms, priority 6, core 0; D-05, D-07; A12 | `subsystem3/line_task.c`, `drv_line_adc.c`; `config/config.h` (1 ms tick) | S3-U20, S3-U33 | U |
| NFR2 | barcode edges time-stamped to 1 µs | §6.3 Barcode ISR; D-08, D-93 | `subsystem3/drv_barcode.c`; `common/app_time.h` | S3-U32, S3-U34 | U |
| NFR3 | wheel-speed loop every 20 ± 2 ms | §3 Motion 20 ms; §6.2 | `subsystem2/motion_task.c`, `motion_ctl.c` | S2-U04, S2-U33 | U |
| NFR4 | accelerometer 100 Hz, magnetometer 75 Hz | §3 IMU 10 ms; §6.4 | `subsystem4/drv_lsm303.c`, `imu_task.c` | S4-U36 | U |
| NFR5 | pings at least 60 ms apart | §3 Scan 65 ms; D-75 | `subsystem5/scan_task.c` (`scan_step`) | S5-U32 | U, Insp (compile-time check) |
| NFR6 | drive removed within 50 ms of a fault or STOP | SD2; §3 Safety 10 ms, priority 5; A14; D-15 | `subsystem4/safety_task.c`; `subsystem1/command_task.c` (STOP) | S4-U33, SYS-10 | U, S |
| NFR7 | course in under 5 minutes | D-32 cruise speed | `app_config.h` section 5 | SYS-01 | S |
| NFR8 | remap takes effect within 1 s | IF-14, IF-10; D-66 | `subsystem1/command_task.c`; `navigation/nav_task.c`, `nav_sm.c` | NAV-U12, S1-U31, INT-10, SYS-05 | U, I, S |
| NFR9 | telemetry 5 Hz, heartbeat 1 Hz | §3 Telemetry 200 ms; §6.1; D-50 | `subsystem1/telemetry_task.c`; `lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c` | S1-U30, SYS-08 | U, S |
| NFR10 | Wi-Fi or MQTT loss does not stop the car | §6.1 BACKING OFF; A11; D-68, D-70, D-103 (Wi-Fi loss a status bit that never latches) | `subsystem1/telemetry_task.c`, `mqtt_bridge.c`, `command_task.c`; `subsystem4/safety_latch.c`; `mqtt_utk.c` | S1-U22, S1-U34, S1-U35, S4-U08, SYS-14 | U, S |
| NFR11 | no more than 3.3 V on any GPIO | §2 pin table; A5; D-73 | wiring; input pulls in `subsystem2/drv_encoder.c`, `subsystem3/drv_barcode.c` | S2-U37, S3-U35, SYS-19 | U, Insp, S |
| NFR12 | full course on the power bank without a reset | A6; D-30 soft start | `subsystem2/motion_ctl.c` | SYS-15 | S |
| NFR13 | CPU load at most 60% | §3; A12; D-05, D-20 | `config/config.h`; `subsystem3/line_task.c` (timing probe) | S3-U33, SYS-16 | U, S |
| NFR14 | broker address changeable without reflashing | §6.1; IF-15; D-16; G1 | `subsystem1/cmd_parse.c`, `command_task.c`, `mqtt_bridge.c`; `mqtt_utk.c` | S1-U01, S1-U04, S1-U33, INT-15, SYS-20 | U, I, S |
| NFR15 | C, Pico SDK, µT-Kernel template, TEAM.md rules, Barr-C, no printf in interrupts, no unjustified globals | TEAM.md; D-08, D-18 | all of `app_program/` | S1-U03, S1-U10, NAV-U19 | U, Insp (code review) |
| NFR16 | safety stop held until reset; commands zeroed before release | SD2; IF-09, IF-16; D-15, D-46, D-95 | `subsystem4/safety_task.c`, `safety_latch.c`; `subsystem2/motion_task.c` | S4-U07, S4-U35, INT-09, SYS-10 | U, I, S |
| NFR17 | following at 0.15–0.35 m/s, 200–1000 lux, across tile joins | §6.3; A9; D-32, D-71 | `navigation/nav_sm.c` (speed clamp); `subsystem3/line_track.c` | NAV-U03, NAV-U14 (commanded speed), SYS-17 (measured speed) | U, S, Insp (compile-time check) |
| NFR18 | straight driving: wheel speed ±10%, drift ≤ 2 cm/m | §6.2 DRIVING; D-99, D-100 | `subsystem2/motion_ctl.c`, `motion_task.c` | S2-U03, S2-U04, S2-U06, S2-U34 | U |
| NFR19 | RAM below 75% of 264 KB | TEAM.md section 6 | linker map; `config/config.h` limits | SYS-18 | Insp, S |

## Interfaces → tests

Each interface in TEAM.md section 5 has an integration test, and most
also have an on-target check that the queue or flag exists and is served.
The last column lists the host tests of each packed message format
(minimum-bit change, 22 Oct; flags and the MQTT text are not packed).

| Interface | Integration test | On-target check | Packed format (host) |
|---|---|---|---|
| IF-01 Line → Navigation events | INT-01, INT-20 | NAV-U20 | S3-U41 |
| IF-02 line-error record | INT-02 | S3-U20, S3-U22 | S3-U40 |
| IF-03 IMU → Navigation | INT-03 | NAV-U20 | S4-U40 |
| IF-04 faults → Safety | INT-04 | S4-U22 | S4-U41, S4-U08 (fault mask) |
| IF-05 Scan → Navigation | INT-05 | NAV-U20 | S5-U40 |
| IF-06 Navigation → Scan | INT-06 | S5-U21 | S5-U41 |
| IF-07 Navigation → Motion | INT-07 | S2-U20 | S2-U40 |
| IF-08 Motion → Navigation | INT-08 | NAV-U20 | S2-U41 |
| IF-09 RESET handshake | INT-09 | S2-U21 | – |
| IF-10 Command → Navigation | INT-10 | NAV-U20 | S1-U41 |
| IF-11 Navigation → Line | INT-11 | – | – |
| IF-12 every task → Telemetry | INT-12, INT-19 | S1-U20 | S1-U42–S1-U44 |
| IF-13 Telemetry → MQTT | INT-13, INT-19 | S1-U21 | – |
| IF-14 MQTT → Command | INT-14 | S1-U21 | – |
| IF-15 Command → MQTT broker setting | INT-15 | S1-U21 | S1-U40 |
| IF-16 FAULT level, restart, reset | INT-16 | NAV-U21 | – |
| IF-17 sensor health | INT-17 | NAV-U21 | – |
| IF-18 watchdog check-ins | INT-18 | – | – |

## Change requirement 1: minimum-bit data representation (22 Oct)

Introduced at Tribunal 1. Evidence page: [`docs/minimum_bit.md`](../docs/minimum_bit.md).

| Req | Requirement (short) | Design | Code | Tests | How |
|---|---|---|---|---|---|
| CR1 | every application data field uses only the bits its valid range needs (bit-fields or masks and shifts); fault conditions as bits of one fault mask | D-82–D-92; masks and shifts, no bit-fields (D-83); the fault mask and its single writer (D-102 to D-105); every interface's format in TEAM.md section 5 | `common/bitpack.h`, `bitpack.c`, `app_mbf.h`, `app_stats.c` (measurement); `subsystem1/tlm_msgs.c`, `cmd_parse.c`; `subsystem2/motion_msgs.c`, `drv_encoder.c`, `motion_ctl.c` (stall watch); `subsystem3/line_msgs.c`, `bc_capture.c`, `line_track.c`, `steering.c`, `letter_map.c`, `line_keep.c`; `subsystem4/imu_msgs.c` (fault table), `safety_latch.c` (fault mask); `subsystem5/scan_msgs.c`, `drv_ultrasonic.c` | COM-U07–COM-U12, S1-U40–S1-U44, S2-U07, S2-U40–S2-U42, S3-U40–S3-U49, S4-U08, S4-U40, S4-U41, S5-U40–S5-U42; all NAV tests on the packed messages | U, Insp (the table in `docs/minimum_bit.md`); timing and stack use on the car still to measure (D-82) |

## Built but not asked for (gold-plating check)

When this table was made (13 Oct 2026), everything else in the code traced
to a requirement above, an interface or the report's design. These items
do not, so each is recorded with its reason and can be switched off or
questioned:

| Item | Why it is there | Entry |
|---|---|---|
| STEADY motion class | FR17 has no class for driving at constant speed | D-58, open question G13 |
| Line-level freeze on a hump | risk to FR7 found in review (G7); `CFG_NAV_HUMP_FREEZE_LEVELS` | D-23 |
| Motor soft start | power-bank risk A6 (NFR12); `CFG_MOTOR_RAMP_PM` 1000 turns it off | D-30 |
| Barcode quiet zone | proposal only, off (`CFG_BC_QUIET_MS` 0) | D-25, open question G14 |
| Telemetry echo on the console | bench tests without a broker; off by default | D-69 |
| Timing probe pin | measuring A12; off by default | D-20 |

## Evidence by owner (14 Oct 2026, updated 16 and 22 Oct)

The trail each owner must be able to explain: requirement → decision →
code → test → result → what is still open. "Host" means host unit tests
of the logic on a PC; they pass on `main` as of 16 Oct (71 tests), on
`main` with the minimum-bit change and the team's review fixes, 22
Oct (100 tests), with G19 and G20 decided, 23 Oct (102 tests),
after the second outside review, 23 Oct (104 tests), after the
third, 23 Oct (105 tests), and after a team member's review, 25 Oct
(109 tests).
They say nothing about timing, sensors or the car.
Nothing has been run on the car, so the *On the car* column is empty
for everyone. Update this table
with each new result.

| Owner | Leads on | Host evidence | On the car | Still open |
|---|---|---|---|---|
| Ang Hung Ling (S1) | FR12–FR14, NFR9, NFR10, NFR14; FR6 remap commands; FR10 profile event | S1-U01–S1-U12, S1-U40–S1-U44 | none | never run against a broker; A11 (instructor agreement) pending; Wi-Fi is build-time only (G1); MQTT client ID (S1-U35); a STOP never lost (D-101) and Wi-Fi loss as a status bit (D-103), owner to confirm |
| Dominic Ng Jun Jie (S2) | FR19, NFR3, NFR18; moves and turns for FR4; stall and encoder-fault detection | S2-U01–S2-U07, S2-U40–S2-U42 | none | geometry, counts per turn and gains UNVALIDATED (A2); FR19's S2-U35/S2-U36 cannot be run as written (G18); the start-up duty and the feed-forward to measure together (D-100); stall thresholds UNVALIDATED (D-105, S2-U38), owner to set |
| Nga Wei Yang (S3) | FR1, FR2, NFR1, NFR2; junction and line-lost detection for FR3, FR18; letter map (FR6); steering | S3-U01–S3-U19, S3-U40–S3-U49 | none | line levels and barcode polarity UNVALIDATED (A3, A9, D-63); quiet zone (G14); one rare edge-loss case left (D-78); read failures now latch a fault (D-79, decided 16 Oct); line health drops at the first bad sample and while sensing is idle (D-96, D-98, from the outside reviews) |
| Yee Sheng Chua (S4) | FR7, FR8, FR15, FR17, NFR4, NFR6, NFR16; Safety and the fault mask | S4-U01–S4-U08, S4-U40, S4-U41 | none | height table and collision thresholds UNVALIDATED (A4); overrange = collision and calibration jolts (D-81, decided 16 Oct, owner to confirm); a fault while RESET waits cancels the reset (D-95, decided 23 Oct, owner to confirm); the fault mask as agreed after Tribunal 1, with stall and encoder fault latching (D-102, D-105, owner to confirm); STEADY class (G13); pin override (A14) pending |
| Wong Qian Hui (S5) | FR9–FR11, NFR5 | S5-U01–S5-U06, S5-U40–S5-U42 | none | ranges and servo angles UNVALIDATED (A5); servo limited to 30°–150° until measured, side scan 30° short of square (D-97, owner to confirm); unseen space now has no room (D-80, decided 16 Oct, owner to confirm); initial state (G11); echo wait 50 ms and SELF-TEST asking only for an answer (D-61, D-104), the no-target pulse to measure (S5-U34); packing in the Echo ISR (G25) |
| Whole team | FR3–FR5, FR16, FR18 (Navigation); NFR7, NFR13, NFR15, NFR17, NFR19; CR1 | NAV-U01–NAV-U19, NAV-U40–NAV-U42, COM-U01–COM-U12 | none | COMPLETE has no exit (G12); 1 ms tick and CPU load (A12); RAM is an estimate (NFR19); a line record that stops changing is a fault (D-94, decided 23 Oct, team to confirm) |

## Weak spots

- Known gaps waiting for a team decision, not changed in the code: open
  questions G11–G14, G18 and G22–G25 in `docs/decisions.md` section E
  (G15–G17 were decided on 16 Oct: D-79–D-81; G19 and G20 on 23 Oct:
  D-94, D-95; G21 by the team after Tribunal 1, built 25 Oct: D-102).
- Verified only on the car (no host or on-target evidence yet): NFR4,
  NFR5, NFR7, NFR12, NFR19, and FR12's field list (SYS-08).
- Depends on values still marked UNVALIDATED in `app_config.h`: every
  accuracy requirement (FR4, FR8, FR10, FR11, FR19, NFR18) and the line
  levels (FR1, NFR17). Their manual tests are the measurements.
- NFR15 has no automatic check for Barr-C; it is verified by review.
