# 06 — Subsystem behaviour (coverage D)

Tags and source keys: see `00_README.md`. This file says what the code at
REPO `58f7d61` **does**, read from its sources, file headers and
`docs/decisions.md` (D-numbers). [Verified: REPO] for every statement
unless tagged otherwise. Status of all of it: **implemented, host-tested
where a host test is named, never run on the car** (10). Every number
below is a setting in `app_config.h` (Appendix A), and most are
UNVALIDATED (04 §5).

Pure-logic modules (no kernel headers, host-tested): `nav_sm.c`,
`cmd_parse.c`, `tlm_json.c`, `tlm_format.c`, `motion_ctl.c`,
`bc_decode.c`, `bc_capture.c`, `line_track.c`, `steering.c`,
`letter_map.c`, `imu_calc.c`, `scan_logic.c`, `fixmath.c`. Task and driver
files (`*_task.c`, `drv_*.c`, `mqtt_bridge.c`) are checked only by the
on-target `*_test.c` checks (`TEST=1`), which have never been run.

## 1. Navigation (shared; `app_program/navigation/`)

- **Modes** (`nav_types.h`, report section 5, Fig 5): BOOT → SELF-TEST →
  READY → RUNNING { LINE FOLLOWING, HUMP CROSSING, LINE SEARCH, AT
  JUNCTION, AVOIDING } → SAFE HOLD / COMPLETE / FAULT. SAFE HOLD reasons:
  no command at a junction (FR5), line not found within 3 s after a turn,
  no safe route (FR11).
- **SELF-TEST**: waits up to 3 s (D-39) for the Line, IMU and Scan health
  levels (IF-17, D-57); otherwise FAULT (`FAULT_CAUSE_SELF_TEST`).
- **READY → RUNNING** only by the START button GP20, polled and debounced
  every 5 ms (D-65, FR16). No MQTT START (G2).
- **Following**: every 5 ms reads `line_err_t` (IF-02) and runs Buddy 3's
  PD steering (`steering.c`); left = speed − diff, right = speed + diff.
  Cruise 250 mm/s, hump 150 mm/s (D-32). The commanded following speed
  after the steering slow-down is clamped to 150–350 mm/s (D-71, teammate
  13 Oct; "This limits the commanded speed only"). While the line is not
  seen the last error is kept (D-52). A record with `b_valid` 0 counts as
  "no line" (`nav_sm.c:822`, D-79).
- **Barcodes and junctions** (SD1): a letter (IF-01) is held; at the
  junction (both sensors fully dark, 3 samples) the car moves the sensor-to-
  axle distance (80 mm), then turns by the held letter's action and looks
  for the new line with the search sweep (3 s limit). Default map A =
  straight, B = left, C = right, D = U-turn (left, D-34); other letters have
  no action until remapped over MQTT (FR6, D-54). A newer letter replaces
  the held one (D-35). No command at a junction → SAFE HOLD (FR5, kept as
  written). A failed read never clears a held command (D-53).
- **Reads that count** (D-53, approved improvement plus the teammate's
  13/14 Oct fixes): barcode and junction events count only in LINE
  FOLLOWING and HUMP CROSSING; a read counts only if the car was following
  from its start to its result. Residual window: Line posts a read's start
  up to one 5 ms cycle (plus the 1 ms glitch filter) after its first bar,
  so a read whose first bar came just before following resumed is counted;
  capture-start time stamps would close it and were not added. Host test
  NAV-U10.
- **Line search** (FR18): sweep 45° to the side last seen, 90° back, 45° to
  centre, repeated until 5 s (D-33); then RUN OVER → COMPLETE with a run
  summary.
- **Hump crossing**: IMU HUMP_START → hump speed, line levels frozen
  (`CFG_NAV_HUMP_FREEZE_LEVELS` 1, D-23, IF-11); HUMP_END → logged with the
  height (FR8); 5 s timeout (D-38). A hump that ends while not following is
  not logged (D-67).
- **Obstacle** (FR9–FR11, D-36): OBST_AHEAD → stop, wait 200 ms (D-37),
  ask Scan to scan (IF-06). PLAN → rectangular bypass: turn 90° to the
  chosen side, sideways to the obstacle edge + 75 mm gap + half the car,
  turn back, drive until Scan reports BEHIND (≤ 800 mm), 150 mm more, turn
  in, ≤ 300 mm to find the line, axle on the line, turn along it.
  NO_ROOM → reverse 100 mm once and scan again; NO SAFE ROUTE → SAFE HOLD.
  PATH_CLEAR → resume.
- **Heading**: odometry heading nudged 2% towards the compass at each IMU
  heading update, flat ground only (D-40).
- **Moves**: keepalive every 5 ms tick while a move runs (D-55); pivot
  turns counted by encoders (D-56).
- **COMPLETE has no exit** until power-off (D-64; open question G12).
  SAFE HOLD → RESET → READY; FAULT → RESET → SELF-TEST.
- **Route log**: 48 entries in RAM, cleared at START (D-51), replayed after
  a summary at 4 entries per 200 ms (D-60).

## 2. Subsystem 1 — Wi-Fi/MQTT, commands, telemetry (Buddy 1)

- `command_task` (priority 11, core 1): waits for command text (IF-14),
  parses flat JSON (`cmd_parse.c`, D-66). `remap`/`default_map` →
  Navigation, which acknowledges once the map changed (FR6, NFR8);
  `summary` → Telemetry publishes the run summary (FR14); `stop` → Safety
  latches `FAULT_CAUSE_STOP_CMD` (FR14, FR15); `broker` → acknowledged
  first, applied 300 ms later so the ack leaves on the old broker; new
  address in RAM only (NFR14, D-16); anything else → ack with `ok` 0. Link
  check every 100 ms.
- `telemetry_task` (priority 12, core 1): drains `mbf_tlm` (IF-12);
  telemetry at 5 Hz and heartbeat at 1 Hz (NFR9); route entries, obstacle
  profiles (D-72), acks, faults, motion-class and link changes published at
  once. Messages are queued to `mbf_publish` (IF-13) only while MQTT is up;
  a message that does not fit or finds the queue full is dropped and
  counted, never truncated (D-68, D-49).
- `mqtt_bridge.c`: the hooks the template's MQTT code calls inside
  `cyw43_utk_task` (A11, D-14); 10 ms publish pump, ≤ 4 messages per call
  (D-50); reconnect back-off 1, 2, 4 … 30 s (D-70).
- Fault cause names in telemetry include `line_data` (added 16 Oct,
  `tlm_format.c`).
- Not published (TPKG NF-2, left as is): the barcode `failed` counter
  reaches Telemetry but `tlm_format.c` never formats it. [Reported: TPKG
  NF-2; Verified: decisions §G mentions SYS-02 uses `bc` instead]

## 3. Subsystem 2 — Motion (Buddy 2)

- `motion_task` (priority 9, core 0, 20 ms): reads encoder counts (ISR,
  rising edges of channel A only, D-28; `CFG_ENC_QUADRATURE_X4` selects all
  edges), works out wheel speeds and odometry, applies Navigation's
  commands (IF-07), runs the wheel PID with feed-forward and sets PWM
  (10 kHz, D-29), sends odometry and move-done (IF-08), checks in.
- States IDLE, DRIVING, HOLDING SPEED, TURNING, BRAKED. No update from
  Navigation for 100 ms while moving → stop (report 6.2).
- Soft start: duty rises ≤ 100‰ per 20 ms; slowing is immediate (D-30).
- Moves at 150 mm/s, pivot turns at 120 mm/s, half speed for the last
  20 mm (D-31).
- Safety's FAULT flag holds BRAKED; RESET request → clear queue, command
  and PID state → acknowledge (IF-09, D-46).
- Encoder pull-ups switchable `CFG_ENC_PULL_UP` (teammate `8a915ef`).
- No drift correction exists (TPKG I-09: "Measure NFR18 before adding
  drift correction"; agreed, nothing added). [Verified: REPO; Reported:
  TPKG I-09]
- FR19 single-move tests S2-U35/S2-U36 cannot be run as written: nothing
  starts a single move on demand (G18, open).

## 4. Subsystem 3 — Line, barcode, steering, letter map (Buddy 3)

- `line_task` (priority 6, core 0, 5 ms): reads both ADC channels
  (kernel `adca` driver, D-10), runs LINE TRACKING (`line_track.c`), writes
  `line_err_t` under `mtx_line_err` (IF-02), feeds barcode edges to the
  capture, posts events (IF-01), checks in, status to Telemetry every
  200 ms. RUN OVER: sensing idle, still checks in.
- **Tracking** (`line_track.h`): darkness d = (reading − white)/(black −
  white) in ‰; error = d_left − d_right; junction = both fully dark (≥ 800‰)
  for 3 samples, re-armed after 4 samples not both dark (D-22); lost = both
  white (≤ 150‰) for 300 ms; found when either is not white.
- **Level adaptation** (D-23): dark-band readings pull black 1/64 of the
  way, white-band readings pull white; limited to ±600 counts of the
  stored values, restored if the span falls below 400; frozen on a hump.
  Stored levels are constants, not flash (D-63).
- **Read failures** (D-79, G15 decided 16 Oct by the user; S3 owner to
  confirm): a failed ADC read sets `b_valid` 0 with a new `seq`; 4 in a row
  (`CFG_LINE_READ_FAIL_LIMIT`, 20 ms) report `FAULT_CAUSE_LINE_DATA` once
  with detail `-MERCD(ercd)`. A rail reading is not a failure. Host test
  S3-U18.
- **Barcode capture** (`bc_capture.c`, report 6.3 + approved
  improvements): WAITING ends at the first bar (`CFG_BC_QUIET_MS` 0, D-25;
  teammate 13 Oct). Glitch filter: edges closer than 1 ms cancel. Decode
  at once when 29 elements are complete (the 30th edge); else 120 ms with
  no edge, 64 edges or 2000 ms end the capture as failed (D-26). Symbol =
  first 30 edges; an edge lost inside it fails the read with
  `BC_FAIL_EDGE_LOST` (D-78, teammate 14 Oct). Residual window (D-78): an
  edge dropped while the Line task is emptying the queue is seen next
  cycle.
- **Decoding** (`bc_decode.c`): Code 39 `*X*`, 29 element widths; wide/
  narrow per character, bars and spaces separately (approved improvement,
  D-27; `CFG_BC_CLASSIFY_PER_CHAR` 0 selects the report's rule). Direction
  from the end characters; reversed reads decoded once more.
- GP7 pull to the white level (D-73).
- **Steering** (`steering.c`): PD on the line error (KP 150 mm/s at full
  error, KD 300, diff ≤ 200 mm/s, slowdown 400‰ at full error); run by
  Navigation.
- **Letter map** (`letter_map.c`): owned by Navigation's instance; remaps
  through IF-10.

## 5. Subsystem 4 — IMU and Safety (Buddy 4)

- `imu_task` (priority 8, core 1, 10 ms): owns I2C1 (own polled driver,
  D-11); LSM303DLHC accel 100 Hz ±4 g high resolution, magnetometer 75 Hz.
- **CALIBRATING**: 100 samples (1 s) at rest (D-41). Invalid samples
  (failed read, |a| < 300 mg) are left out and counted; 5 in a row latch
  `FAULT_CAUSE_IMU_DATA` and the IMU restarts from identification (D-76,
  teammate). A jolt (|a| > 3500 mg, or |total − 1000| ≥ 1000 mg) restarts the
  average without a fault (D-81, G17 decided 16 Oct by the user; S4 owner
  to confirm).
- **MONITORING**: tilt-compensated heading to Navigation every 20 ms
  (D-43); motion class (FR17: stationary, accelerating, turning, climbing,
  descending, impact, plus STEADY — D-58, G13) over a 10-sample window;
  turn rate from the compass on flat ground only, held on a hump, smoothed
  over 4 updates (D-77); roll > 3° for ≥ 100 ms = ON HUMP, < 1.5° = level,
  then the peak roll → height from the table (FR8). One sample above
  3500 mg = collision at once via `imu_impact()`, detail |a| in mg (D-81);
  a horizontal jolt above 1000 mg = collision (FR15); 5 invalid in a row =
  `IMU_DATA` fault (D-42). Host tests S4-U06 and the imu_calc tests.
- `safety_task` (priority 5, core 0, 10 ms and on a fault event):
  latches the first fault (cause, source, detail), forces GP8–GP11 low with
  the IO_BANK0 output override (A14, `CFG_SAFETY_PIN_OVERRIDE`, D-15/D-45),
  sets FAULT (Motion BRAKED), alarm 2 kHz + red LED 250/250 ms (D-47);
  RESET (GP21) handling, watchdog and check-ins: 07.

## 6. Subsystem 5 — Ultrasonic and servo (Buddy 5)

- `scan_task` (priority 10, core 1, 65 ms; pings ≥ 60 ms apart even when
  a release is late, D-75, teammate): owns the HC-SR04P and the SG90.
- Readings (D-61): echo < 2 cm or none within 40 ms = invalid; beyond 4 m
  = valid "nothing in range". Open space must give a valid reading, not a
  timeout, or both MONITORING and D-80 stop the car (manual S5-U34).
- States (Fig 10): starts in **RUN OVER** with health pings only until
  START (D-57; Fig 10 draws MONITORING first — open question G11).
  MONITORING: ping ahead; two readings ≤ 30 cm → OBST_AHEAD. COARSE SCAN at
  30, 60, 90, 120, 150°; FINE SCAN in 5° steps to the obstacle edges;
  PLANNING: profile (FR10) published as an event (D-72) and the side with
  room (FR11, D-62: larger clearance wins, left on a tie).
- **Coverage** (D-80, G16 decided 16 Oct by the user; S5 owner to
  confirm): an invalid coarse/fine reading is repeated once at the same
  angle; a side with no valid reading outside the obstacle's run has
  clearance −1 and no room; PATH_CLEAR only if all five coarse readings are
  valid, otherwise handled as no room (reverse once, scan again, then NO
  SAFE ROUTE with reason INVALID). Host test S5-U06; manual S5-U35.
- **BYPASSING**: servo sideways (180° or 0°); obstacle behind only after
  it was seen beside the car and then 2 valid side readings > 400 mm
  (D-48, D-74, teammate); invalid readings skipped. REJOINING until
  Navigation finds the line. Three invalid readings in a row in a ranging
  state → NO SAFE ROUTE.

## 7. Common helpers (`app_program/common/`)

`fixmath.c` (Q15 fixed point, atan2, sin/cos, isqrt; D-13; host COM-U01…),
`gpio_irq.c` (one dispatcher, D-09), `app_gpio.c`, `app_pwm.c`,
`app_time.c`, `hw_regs.h` (register addresses; notes the template's
`GPIO_OUT` error).

---

## Appendix A — `app_program/app_config.h` verbatim (REPO `58f7d61`)

```c
/*
 *  app_config.h
 *  IS19 central configuration: every tunable value of the whole firmware.
 *
 *  Changed only by team agreement (like app.h).  Each value carries a tag
 *  that says where it comes from:
 *
 *    [REPORT x]      fixed by the Week 6 design review (requirement or
 *                    design target).  Still checked by measurement under
 *                    the report's assumptions A1-A14.
 *    [DATASHEET x]   taken from the named datasheet.
 *    [TEMPLATE x]    taken from the course template.
 *    [DECISION D-nn] a design choice listed in docs/decisions.md for the
 *                    team to approve.
 *    [UNVALIDATED]   a typical starting value.  NOT MEASURED ON OUR CAR.
 *                    The basis is given next to it.  Measure or tune it on
 *                    the physical car before relying on it, then change
 *                    the tag to [MEASURED <date> <who> <log file>].
 *
 *  Rule (13 Oct 2026, docs/decisions.md): every value that describes
 *  the car, its sensors or the course, or that has to be tuned on the
 *  car, carries [UNVALIDATED] until it is measured, also when a
 *  datasheet or a decision gave the starting value.  [REPORT] and [TEMPLATE] values are
 *  requirements or fixed settings, not measurements.  Register values,
 *  bus addresses, protocol timings and the scale factors that follow from
 *  a register setting are definitions copied from a datasheet, not
 *  measurements, so they keep [DATASHEET] alone.
 *
 *  No value in this file has been measured on the car yet.  Each
 *  subsystem's section belongs to its owner (TEAM.md section 1);
 *  sections 1, 2 and 5 are shared.
 *
 *  This header only holds macros, so the pure-logic modules and the host
 *  unit tests (testing/host/) can include it too.  It must not include
 *  any kernel header.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* =====================================================================
 *  1. Kernel timing, tasks and cores
 * ===================================================================== */

/* System tick.  Mirrors CNF_TIMER_PERIOD in config/config.h.
   [REPORT section 3, A12 open: 1 ms tick proposed so 5 ms periods are
   whole ticks] */
#define CFG_TICK_MS                     1

/* Task priorities, 1 = highest; 1-4 belong to the kernel.
   [REPORT section 3 task table] */
#define CFG_PRI_SAFETY                  5
#define CFG_PRI_LINE                    6
#define CFG_PRI_NAV                     7
#define CFG_PRI_IMU                     8
#define CFG_PRI_MOTION                  9
#define CFG_PRI_SCAN                    10
#define CFG_PRI_COMMAND                 11
#define CFG_PRI_TELEMETRY               12

/* Task periods in ms.  [REPORT section 3 task table, NFR1, NFR3, NFR4,
   NFR5, NFR9] */
#define CFG_PERIOD_SAFETY_MS            10
#define CFG_PERIOD_LINE_MS              5
#define CFG_PERIOD_NAV_MS               5
#define CFG_PERIOD_IMU_MS               10
#define CFG_PERIOD_MOTION_MS            20
#define CFG_PERIOD_SCAN_MS              65
#define CFG_PERIOD_LINKCHECK_MS         100
#define CFG_PERIOD_TELEMETRY_MS         200
#define CFG_HEARTBEAT_MS                1000

/* Every period is a whole number of kernel ticks (report section 3, A12).
   app_main.c also checks CFG_TICK_MS against CNF_TIMER_PERIOD. */
#if ((CFG_PERIOD_SAFETY_MS % CFG_TICK_MS) != 0) || \
    ((CFG_PERIOD_LINE_MS % CFG_TICK_MS) != 0) || \
    ((CFG_PERIOD_NAV_MS % CFG_TICK_MS) != 0) || \
    ((CFG_PERIOD_IMU_MS % CFG_TICK_MS) != 0) || \
    ((CFG_PERIOD_MOTION_MS % CFG_TICK_MS) != 0) || \
    ((CFG_PERIOD_SCAN_MS % CFG_TICK_MS) != 0) || \
    ((CFG_PERIOD_LINKCHECK_MS % CFG_TICK_MS) != 0) || \
    ((CFG_PERIOD_TELEMETRY_MS % CFG_TICK_MS) != 0)
#error "every task period must be a whole number of CFG_TICK_MS ticks"
#endif

/* Task stack sizes in bytes.  [TEMPLATE app.h TASK_STACK_SIZE 2048 is
   the default; DECISION D-19: more for the tasks that format text or
   run the large state machine] */
#define CFG_STACK_DEFAULT               2048
#define CFG_STACK_NAV                   3072
#define CFG_STACK_TELEMETRY             3072
#define CFG_STACK_COMMAND               2048

/* =====================================================================
 *  2. Pins  [REPORT section 2 pin table]
 * ===================================================================== */
#define CFG_PIN_ENC_L_A                 0   /* Grove 1 */
#define CFG_PIN_ENC_L_B                 1   /* Grove 1 */
#define CFG_PIN_IMU_SDA                 2   /* Grove 2, I2C1 SDA */
#define CFG_PIN_IMU_SCL                 3   /* Grove 2, I2C1 SCL */
#define CFG_PIN_US_TRIG                 4   /* Grove 3 */
#define CFG_PIN_US_ECHO                 5   /* Grove 3 */
#define CFG_PIN_BARCODE                 7   /* Grove 7, D0 */
#define CFG_PIN_MOTOR_L_A               8   /* M1A, PWM slice 4 A */
#define CFG_PIN_MOTOR_L_B               9   /* M1B, PWM slice 4 B */
#define CFG_PIN_MOTOR_R_A               10  /* M2A, PWM slice 5 A */
#define CFG_PIN_MOTOR_R_B               11  /* M2B, PWM slice 5 B */
#define CFG_PIN_SERVO                   12  /* Servo 1, PWM slice 6 A */
#define CFG_PIN_ENC_R_A                 16  /* Grove 4 */
#define CFG_PIN_ENC_R_B                 17  /* Grove 4 */
#define CFG_PIN_STATUS_LED              18  /* on-board NeoPixel, PIO */
#define CFG_PIN_START                   20  /* on-board button */
#define CFG_PIN_RESET                   21  /* on-board button */
#define CFG_PIN_BUZZER                  22  /* on-board piezo, PWM 3 A */
#define CFG_PIN_LINE_L                  26  /* Grove 6, ADC0 */
#define CFG_PIN_LINE_R                  27  /* Grove 6, ADC1 */
#define CFG_ADC_CH_LINE_L               0
#define CFG_ADC_CH_LINE_R               1

/* Optional timing probe for A12: the Line task toggles this pin every
   cycle so a logic analyser can check the 5 ms period.  -1 = off.
   GP6 (Grove 5) is the report's spare pin; claim it in TEAM.md before
   turning this on.  [DECISION D-20] */
#define CFG_PIN_TIMING_PROBE            (-1)

/* On-board buttons.  [UNVALIDATED: Robo_Pico.pdf does not show the
   button circuit; assumed to pull the pin to GND when pressed, with the
   RP2040 internal pull-up enabled.  Check by reading the pin.] */
#define CFG_BUTTON_ACTIVE_LOW           1
/* Stable level needed for a press.  [DECISION D-21; UNVALIDATED: tactile
   switches typically bounce for under 10 ms; check on the board] */
#define CFG_BUTTON_DEBOUNCE_MS          30

/* =====================================================================
 *  3. Buddy 3: line following and barcode
 * ===================================================================== */

/* Stored black and white levels, 12-bit ADC counts.  The report says the
   levels are measured in the lab and stored, then adapt while driving.
   [UNVALIDATED: typical TCRT5000 module A0 swing at 2-5 mm height (A0
   low on white, high on black).  Replace with lab-measured values, A9.] */
#define CFG_LINE_L_WHITE                300
#define CFG_LINE_L_BLACK                3000
#define CFG_LINE_R_WHITE                300
#define CFG_LINE_R_BLACK                3000

/* A reading at or above this darkness (per mille of the white-to-black
   span) counts as fully dark; at or below CFG_LINE_WHITE_PM as white.
   [UNVALIDATED: chosen to leave margin either side of the "partly dark"
   band seen over a line edge; check under A9.] */
#define CFG_LINE_DARK_PM                800
#define CFG_LINE_WHITE_PM               150

/* Junction: both fully dark for 3 samples in a row.  [REPORT 6.3] */
#define CFG_LINE_JUNCTION_SAMPLES       3
/* Samples with at least one sensor not fully dark before a new junction
   can be reported, so one crossbar gives one event.  [DECISION D-22;
   UNVALIDATED: 20 ms, enough for sensor noise at a crossbar's edge if
   the A9 logs agree] */
#define CFG_LINE_JUNCTION_REARM         4
/* Line lost: both white for 300 ms.  [REPORT 6.3] */
#define CFG_LINE_LOST_MS                300
/* Failed line-sensor reads in a row (4 = 20 ms) that latch a LINE_DATA
   fault in Safety; each failed read marks the line record invalid.
   [DECISION D-79, G15 decided 16 Oct 2026] */
#define CFG_LINE_READ_FAIL_LIMIT        4

/* Level adaptation while the sensors cross the line edges.
   [REPORT 6.3: levels adapt; DECISION D-23 for the method] */
#define CFG_LINE_ADAPT_ENABLE           1
/* Each adaptation step moves a level 1/2^shift of the way to the
   reading.  [UNVALIDATED: 1/64 per 5 ms sample, about 0.3 s time
   constant] */
#define CFG_LINE_ADAPT_SHIFT            6
/* Only readings within this band (per mille of the span) of a level
   adapt it.  [DECISION D-23; UNVALIDATED: check against the A9 logs] */
#define CFG_LINE_ADAPT_BAND_PM          150
/* An adapted level may not move further than this from its stored value
   (ADC counts).  [DECISION D-23; UNVALIDATED: about 20% of the typical
   swing above; set from the lab and demo-room readings (A9)] */
#define CFG_LINE_ADAPT_LIMIT            600
/* Smallest allowed black-white span (ADC counts); below this the levels
   are treated as broken and the stored ones are restored.
   [DECISION D-23; UNVALIDATED: about 15% of the typical swing] */
#define CFG_LINE_MIN_SPAN               400
/* ADC readings outside this range at start-up fail the self-test (a
   floating or shorted input).  [DECISION D-24; UNVALIDATED: a few counts
   inside the 12-bit rails; check the readings with a sensor unplugged] */
#define CFG_LINE_SELFTEST_MIN           5
#define CFG_LINE_SELFTEST_MAX           4090

/* Barcode sensor D0 level over a black bar.  [UNVALIDATED: typical
   MH-sensor module behaviour (D0 low on white, high on black); verify on
   our module, see docs/notes/05.]  The GP7 pull resistor is set to the
   other level, so an unplugged sensor reads white and never starts a
   capture. */
#define CFG_BC_DARK_LEVEL               1

/* Capture ends after 120 ms with no bar.  [REPORT 6.3] */
#define CFG_BC_END_MS                   120
/* A full symbol ('*', letter, '*') has 27 character elements plus 2
   gaps.  [REPORT 6.3] */
#define CFG_BC_ELEMENTS                 29
/* Edges closer together than this are a comparator glitch and are
   removed.  [UNVALIDATED: 1 ms is 0.35 mm of travel at 0.35 m/s, far
   below a printed bar; check under A3.] */
#define CFG_BC_GLITCH_US                1000
/* White time needed before a new capture may start.  0 = the report's
   behaviour (6.3: WAITING ends at the first bar).  A positive value is a
   proposal that is NOT approved yet (decision D-25): it would stop a
   capture from starting part-way through a pattern, but would also miss
   a barcode that has less white than this before it.  [REPORT 6.3] */
#define CFG_BC_QUIET_MS                 0
/* Hard limits that end a capture as failed, so a capture never waits
   for an exact edge count (approved improvement: decode at the 30th
   edge, keep a bounded failure path).  [DECISION D-26: 64 edges is twice
   a symbol; 2 s is twice a 141 mm barcode at 0.15 m/s] */
#define CFG_BC_MAX_EDGES                64
#define CFG_BC_MAX_CAPTURE_MS           2000

/* Wide/narrow classification.  1 = per character (approved
   improvement): in each character the 2 widest of the 5 bars and the
   widest of the 4 spaces are wide (true of '*' and every letter), each
   clearly wider than the next; bars and spaces are compared only with
   their own kind, so a comparator that widens bars and narrows spaces
   does not matter.  0 = the report's rule: wide if more than twice the
   narrowest element of the whole capture.  [DECISION D-27] */
#define CFG_BC_CLASSIFY_PER_CHAR        1
/* Report rule: wide if more than 2x the narrowest.  [REPORT 6.3] */
#define CFG_BC_WIDE_RATIO_X100          200
/* Per-character rule: the narrowest wide element of a kind must be at
   least this many times the widest narrow one.  [UNVALIDATED: printed
   ratio is 3:1 (sample measured, docs/notes/05); 1.5 leaves room for
   speed drift and print spread] */
#define CFG_BC_GAP_RATIO_X100           150

/* Steering (Buddy 3 code run by Navigation).  Line error is -1000..+1000
   per mille; positive = line under the left sensor, so steer left.
   KP: wheel speed difference (mm/s) at full error.  KD: wheel speed
   difference (mm/s) for a full-scale change of the error in one cycle.
   MAX_DIFF: limit on the difference.  SLOWDOWN: base speed cut (per
   mille) at full error; Navigation keeps the result inside NFR17 (D-71).
   [UNVALIDATED: starting gains; tune on the course] */
#define CFG_STEER_KP_MMPS               150
#define CFG_STEER_KD_X1000              300
#define CFG_STEER_MAX_DIFF_MMPS         200
#define CFG_STEER_SLOWDOWN_PM           400

/* =====================================================================
 *  4. Buddy 2: motion
 * ===================================================================== */

/* Geometry.  [UNVALIDATED: typical 65 mm TT wheel and a typical 2WD TT
   chassis; measure both (A2)] */
#define CFG_WHEEL_DIAMETER_MM           65
#define CFG_TRACK_WIDTH_MM              130

/* Encoder counts per wheel turn in the selected decoding mode.
   [UNVALIDATED: no datasheet for the kit's "encoder TT motor 6V 350RPM";
   placeholder from a common TT encoder motor (11-pulse Hall encoder,
   1:48 gearbox, rising edges of channel A only).  Measure (A2).] */
#define CFG_ENC_COUNTS_PER_REV          528
/* 0 = count rising edges of channel A only (lowest interrupt load, A8);
   1 = count every edge of A and B (4x resolution).  The value above must
   be measured in the selected mode.  [DECISION D-28] */
#define CFG_ENC_QUADRATURE_X4           0
/* Swap the count direction of one encoder.  [UNVALIDATED: depends on
   how the motors are mounted; set on the bench] */
#define CFG_ENC_L_INVERT                0
#define CFG_ENC_R_INVERT                0
/* 1 = enable the RP2040 internal pull-ups on the four encoder inputs.
   [UNVALIDATED: assumes open-collector Hall outputs, common on TT encoder
   motors; harmless if they are push-pull.  Check the edges on a scope
   (S2-U37).] */
#define CFG_ENC_PULL_UP                 1

/* Motor driver PWM.  [DATASHEET Robo_Pico.pdf: driver PWM 20 kHz max;
   DECISION D-29: 10 kHz, half the maximum] */
#define CFG_MOTOR_PWM_HZ                10000
/* Swap a motor's forward direction.  [UNVALIDATED: depends on wiring] */
#define CFG_MOTOR_L_INVERT              0
#define CFG_MOTOR_R_INVERT              0
/* Duty limits, per mille.  [UNVALIDATED: TT gear motors typically need
   about 20% duty to start turning] */
#define CFG_MOTOR_MIN_DUTY_PM           200
#define CFG_MOTOR_MAX_DUTY_PM           1000
/* Soft start: largest duty change per 20 ms cycle, per mille; 1000
   turns it off.  [DECISION D-30: limits the start-up current step on the
   power bank (A6); UNVALIDATED: starting value, 0 to full duty in 0.2 s;
   tune with the A6 power test] */
#define CFG_MOTOR_RAMP_PM               100

/* Wheel-speed PID, run every 20 ms.  Output is duty per mille.
   Feed-forward: duty per 100 mm/s of target speed.
   [UNVALIDATED: kit label 6 V 350 rpm scaled to the ~5 V supply is
   about 290 rpm, about 990 mm/s on a 65 mm wheel at full duty, so about
   100 per mille per 100 mm/s; gains are starting values to tune] */
#define CFG_PID_FF_PM_PER_100MMPS       100
#define CFG_PID_KP_X1000                1000 /* per mille duty per mm/s error */
#define CFG_PID_KI_X1000                2000 /* per mille duty per mm/s per s */
#define CFG_PID_KD_X1000                0
#define CFG_PID_I_LIMIT_PM              300

/* Navigation updates must arrive at least this often while moving,
   otherwise Motion stops.  [REPORT 6.2] */
#define CFG_MOTION_UPDATE_TIMEOUT_MS    100
/* Wheel speed for encoder-counted moves and pivot turns.
   [DECISION D-31 / UNVALIDATED: slow end of NFR17 for moves; turn speed
   to tune against FR19's +/-5 degrees] */
#define CFG_MOTION_MOVE_MMPS            150
#define CFG_MOTION_TURN_MMPS            120
/* Slow to half speed for the last part of a move or turn.
   [DECISION D-31 / UNVALIDATED] */
#define CFG_MOTION_SLOWDOWN_MM          20

/* =====================================================================
 *  5. Navigation (shared)
 * ===================================================================== */

/* Line-following speed range: NFR17's speeds at which line following
   must work (shown by SYS-17).  The speed Navigation commands while
   following, after the steering slow-down, is kept inside it, so the
   barcode timing (report 6.3: 120 ms is longer than a wide space at
   0.15 m/s) holds.  This limits the commanded speed, not the measured
   one.  [REPORT NFR17; DECISION D-71] */
#define CFG_NAV_SPEED_MIN_MMPS          150
#define CFG_NAV_SPEED_MAX_MMPS          350
/* Cruise and hump speeds.  [DECISION D-32: choices inside NFR17's
   range; UNVALIDATED: that the car follows the line at them is checked
   by SYS-17] */
#define CFG_NAV_CRUISE_MMPS             250
#define CFG_NAV_HUMP_MMPS               150

#if (CFG_NAV_CRUISE_MMPS < CFG_NAV_SPEED_MIN_MMPS) || \
    (CFG_NAV_CRUISE_MMPS > CFG_NAV_SPEED_MAX_MMPS) || \
    (CFG_NAV_HUMP_MMPS < CFG_NAV_SPEED_MIN_MMPS) || \
    (CFG_NAV_HUMP_MMPS > CFG_NAV_SPEED_MAX_MMPS)
#error "cruise and hump speeds must be inside NFR17's range (D-71)"
#endif

/* Lost-line search and end of run.  [REPORT FR18, 5 s] */
#define CFG_NAV_SEARCH_MS               5000
/* Line must be found within 3 s after a junction turn.  [REPORT 5] */
#define CFG_NAV_FIND_LINE_MS            3000
/* Sweep half-angle, centi-degrees, of the on-the-spot sweep used by
   LINE SEARCH and, after a junction turn, to find the new line.
   [DECISION D-33; UNVALIDATED: check that it finds the line again on
   straights, curves and a hump (A10)] */
#define CFG_NAV_SWEEP_CDEG              4500

/* Distance from the line sensors to the wheel axle, moved before turning
   at a junction.  [UNVALIDATED: measure on the car (A2)] */
#define CFG_NAV_SENSOR_TO_AXLE_MM       80
/* 1 = U-turn turns left, 0 = right.  [DECISION D-34] */
#define CFG_NAV_UTURN_LEFT              1
/* 1 = a newer letter replaces a held one before the junction.
   [DECISION D-35] */
#define CFG_NAV_NEW_LETTER_REPLACES     1

/* Obstacle bypass.  [REPORT FR11: reverse 10 cm; pass 5-10 cm from the
   obstacle (75 mm, the middle of that band); REPORT 6.5: rejoin within
   30 cm; DECISION D-36 for the rectangular path] */
#define CFG_NAV_REVERSE_MM              100
#define CFG_NAV_PASS_GAP_MM             75
#define CFG_NAV_REJOIN_MAX_MM           300
/* Longest leg driven beside the obstacle before giving up.
   [DECISION D-36; UNVALIDATED: obstacle size on the course unknown] */
#define CFG_NAV_BYPASS_MAX_MM           800
/* Extra forward travel after the side scan reports the obstacle behind,
   so the rear of the car clears it.  [UNVALIDATED: about the car's
   length behind the sensor; measure] */
#define CFG_NAV_PAST_EXTRA_MM           150
/* Car width, used for the bypass offset and by Obstacle Scan's room
   check.  [UNVALIDATED: measure the car] */
#define CFG_CAR_WIDTH_MM                150
/* Wait after stopping before asking for a scan.  [DECISION D-37;
   UNVALIDATED: time for the car to come to rest from cruise speed] */
#define CFG_NAV_STOP_SETTLE_MS          200

/* Leave HUMP CROSSING if the IMU never reports the hump passed.
   [DECISION D-38; UNVALIDATED: well over the time to cross a hump at
   0.15 m/s; check when driving over the humps (A4)] */
#define CFG_NAV_HUMP_MAX_MS             5000
/* 1 = freeze the line-level adaptation while crossing a hump, where the
   sensors' height changes (review finding G7); 0 = levels keep adapting,
   as report 6.3 alone describes.  [DECISION D-23] */
#define CFG_NAV_HUMP_FREEZE_LEVELS      1
/* Time allowed for every sensor to report healthy in SELF-TEST.
   [DECISION D-39] */
#define CFG_NAV_SELFTEST_MS             3000
/* Complementary filter: weight of the compass heading per IMU update,
   per mille, on flat ground only.  [DECISION D-40; UNVALIDATED: starting
   weight, about a 1 s time constant at 50 updates per second; tune] */
#define CFG_NAV_COMPASS_WEIGHT_PM       20

/* =====================================================================
 *  6. Buddy 4: IMU and safety
 * ===================================================================== */

/* GY-511 (LSM303DLHC) on I2C1.  [DATASHEET LSM303DLHC: 400 kHz fast
   mode, accel 0x19, mag 0x1E] */
#define CFG_IMU_I2C_HZ                  400000
#define CFG_IMU_ACC_ADDR                0x19u
#define CFG_IMU_MAG_ADDR                0x1Eu
/* CTRL_REG1_A: ODR 0101 = 100 Hz, normal mode, X/Y/Z on.
   [DATASHEET Table 20; REPORT NFR4] */
#define CFG_IMU_CTRL1_A                 0x57u
/* CTRL_REG4_A: BDU on, +/-4 g, high resolution.  [DATASHEET Table 27] */
#define CFG_IMU_CTRL4_A                 0x98u
/* Accelerometer sensitivity at +/-4 g: 2 mg per LSB of the 12-bit value.
   [DATASHEET Table 3] */
#define CFG_IMU_ACC_MG_PER_LSB          2
/* CRA_REG_M: DO 110 = 75 Hz.  [DATASHEET Table 72; REPORT NFR4] */
#define CFG_IMU_CRA_M                   0x18u
/* CRB_REG_M: GN 001 = +/-1.3 gauss.  [DATASHEET Table 75] */
#define CFG_IMU_CRB_M                   0x20u
/* MR_REG_M: continuous conversion.  [DATASHEET Table 78] */
#define CFG_IMU_MR_M                    0x00u

/* Mounting: car frame x forward, y left, z up.  [UNVALIDATED: assumes
   the GY-511 is flat with its X arrow forward; set after mounting] */
#define CFG_IMU_SWAP_XY                 0
#define CFG_IMU_INVERT_X                0
#define CFG_IMU_INVERT_Y                0
#define CFG_IMU_INVERT_Z                0
/* Magnetometer hard-iron offsets, raw counts.  [UNVALIDATED: zero until
   a hard-iron calibration is done with the motors and power bank
   fitted] */
#define CFG_IMU_MAG_OFF_X               0
#define CFG_IMU_MAG_OFF_Y               0
#define CFG_IMU_MAG_OFF_Z               0

/* CALIBRATING: samples averaged at rest.  [DECISION D-41: 1 s] */
#define CFG_IMU_CAL_SAMPLES             100

/* Hump: roll above ON for at least MIN_MS starts ON HUMP; below OFF ends
   it.  [UNVALIDATED: starting thresholds, A4] */
#define CFG_IMU_HUMP_ON_CDEG            300
#define CFG_IMU_HUMP_OFF_CDEG           150
#define CFG_IMU_HUMP_MIN_MS             100

/* Peak roll to hump height table (centi-degrees, mm), linear
   interpolation.  [UNVALIDATED: geometric estimate h = track width x
   sin(roll) with the 130 mm track above; replace with the A4 calibration
   table measured on humps of known height] */
#define CFG_IMU_HUMP_TABLE_LEN          7
#define CFG_IMU_HUMP_TABLE_CDEG         { 0, 200, 500, 1000, 1500, 2000, 3000 }
#define CFG_IMU_HUMP_TABLE_MM           { 0, 5, 11, 23, 34, 44, 65 }

/* Motion classes.  IMPACT_MG is also the calibration jolt: |a| that far
   from 1 g restarts CALIBRATING (D-81).  [UNVALIDATED: starting
   thresholds, tune with logs] */
#define CFG_IMU_IMPACT_MG               1000 /* horizontal jolt = collision */
#define CFG_IMU_ACCEL_MG                80   /* forward/back acceleration */
#define CFG_IMU_TURN_CDPS               2000 /* 20 deg/s turn rate */
#define CFG_IMU_STILL_MG                15   /* mean deviation when still */
#define CFG_IMU_CLASS_WINDOW            10   /* samples, 100 ms */

/* Invalid data: this many bad samples in a row latch a fault, in
   CALIBRATING as in MONITORING (report 6.4: "in any IMU state").
   [DECISION D-42, D-76] */
#define CFG_IMU_INVALID_LIMIT           5
/* Plausible total acceleration range.  Below MIN is invalid data (D-42);
   above MAX the +/-4 g range is close to saturating, so while monitoring
   one such sample is a collision, and while calibrating it is a jolt
   that restarts the calibration (D-81, G17 decided 16 Oct).
   [DECISION D-42, D-81; UNVALIDATED: 1 g at rest; jolts on the course
   not measured yet; check with a hard tap (S4-U37)] */
#define CFG_IMU_MIN_G_MG                300
#define CFG_IMU_MAX_G_MG                3500
/* Send a heading event every Nth IMU cycle.  [DECISION D-43] */
#define CFG_IMU_HEADING_EVERY           2
/* Compass turn-rate smoothing: each new rate moves the reported rate
   1/N of the way.  [UNVALIDATED: starting value; the rate comes from the
   compass on flat ground only, report 6.4, D-77] */
#define CFG_IMU_RATE_SMOOTH             4

/* Hardware watchdog, fed by Safety only while the critical tasks check
   in.  [REPORT section 3; DECISION D-44 for the times] */
#define CFG_WDT_ENABLE                  1
#define CFG_WDT_TIMEOUT_MS              200
/* Arm the watchdog only after the radio driver finished its start-up
   (or after this long), so cyw43 initialisation cannot trip it (A1).
   [DECISION D-44; UNVALIDATED: the radio's start-up time is not measured;
   read it on the console at boot] */
#define CFG_WDT_ARM_MAX_WAIT_MS         15000
/* Check-in deadlines per task, ms.  [DECISION D-44] */
#define CFG_CHECKIN_LINE_MS             50
#define CFG_CHECKIN_NAV_MS              50
#define CFG_CHECKIN_MOTION_MS           100
#define CFG_CHECKIN_IMU_MS              50
#define CFG_CHECKIN_SCAN_MS             325

/* Safety override of the motor pins.  1 = Safety forces GP8-GP11 LOW
   through the GPIO output override (A14, pending instructor agreement);
   0 = braking only through Motion's FAULT handling.  [REPORT 4, A14;
   DECISION D-45] */
#define CFG_SAFETY_PIN_OVERRIDE         1
/* Wait for Motion's zero acknowledgement after RESET.  [DECISION D-46] */
#define CFG_SAFETY_ACK_TIMEOUT_MS       500

/* Buzzer and status LED.  [UNVALIDATED: tone for a passive piezo;
   DECISION D-47 for the pattern and colours] */
#define CFG_BUZZER_HZ                   2000
#define CFG_ALARM_ON_MS                 250
#define CFG_ALARM_OFF_MS                250
#define CFG_LED_BRIGHTNESS              32  /* of 255, limits current */

/* =====================================================================
 *  7. Buddy 5: obstacle scan
 * ===================================================================== */

/* HC-SR04P trigger pulse.  [DATASHEET HCSR04.pdf: 10 us] */
#define CFG_US_TRIG_US                  10
/* Echo time per cm of range, and the usable range.  [DATASHEET
   HCSR04.pdf: distance cm = echo us / 58, range 2-400 cm; UNVALIDATED:
   the factor follows the speed of sound, which changes with the air
   temperature, and our 3.3 V module's range is assumption A5; check with
   S5-U31 and S5-U34] */
#define CFG_US_US_PER_CM                58
#define CFG_US_MIN_MM                   20
#define CFG_US_MAX_MM                   4000
/* Wait for the echo pulse to end.  [UNVALIDATED: HC-SR04 modules hold
   Echo high for tens of ms when nothing returns; 40 ms still fits the
   65 ms ping period.  It must be longer than our module's no-target
   pulse, or open space reads as invalid (S5-U34)] */
#define CFG_US_ECHO_TIMEOUT_MS          40
/* Pings are never closer together than this, even if the task's 65 ms
   release is late (D-75).  [REPORT NFR5; HCSR04.pdf: over 60 ms] */
#define CFG_US_MIN_PING_GAP_MS          60

/* Two readings at or below 30 cm stop the car; three invalid readings in
   a row give NO SAFE ROUTE.  [REPORT Fig 10, 6.5] */
#define CFG_SCAN_DETECT_MM              300
#define CFG_SCAN_DETECT_COUNT           2
#define CFG_SCAN_INVALID_LIMIT          3

/* Scan angles, degrees, 90 = straight ahead, larger = left.
   [REPORT 6.5: coarse 30-150 in 30 degree steps, fine 5 degree steps] */
#define CFG_SCAN_COARSE_FIRST_DEG       30
#define CFG_SCAN_COARSE_LAST_DEG        150
#define CFG_SCAN_COARSE_STEP_DEG        30
#define CFG_SCAN_FINE_STEP_DEG          5
#define CFG_SCAN_SERVO_MIN_DEG          0
#define CFG_SCAN_SERVO_MAX_DEG          180
/* A return closer than this during a scan belongs to the obstacle.
   [DECISION D-48; UNVALIDATED: obstacle sizes and spacing on the course
   are not known yet] */
#define CFG_SCAN_OBSTACLE_MM            500
/* Profile (D-62): neighbouring returns more than JUMP apart in range are
   different objects; returns up to DEPTH beyond the obstacle's front
   limit the clearance on their side.  [DECISION D-62; UNVALIDATED: check
   with boxes of known size (S5-U33)] */
#define CFG_SCAN_JUMP_MM                150
#define CFG_SCAN_DEPTH_MM               300
/* Side scan while bypassing: servo angle looking at the obstacle, the
   range above which the obstacle counts as passed, and how many such
   valid readings are needed after it was seen beside the car.  Invalid
   readings are skipped: they never confirm clearance (report 6.5,
   D-74).  [DECISION D-48; UNVALIDATED: range and count to check while
   passing a box (S5-U33)] */
#define CFG_SCAN_SIDE_LEFT_DEG          180
#define CFG_SCAN_SIDE_RIGHT_DEG         0
#define CFG_SCAN_SIDE_CLEAR_MM          400
#define CFG_SCAN_BEHIND_COUNT           2
/* Pings to skip after moving the servo, so it has settled.
   [UNVALIDATED: one 65 ms period; typical SG90 speed is about 0.1 s per
   60 degrees, not from a supplied datasheet] */
#define CFG_SCAN_SETTLE_PINGS           1

/* SG90 servo, 50 Hz.  [UNVALIDATED: typical SG90 pulse range for
   0-180 degrees; trim on the car] */
#define CFG_SERVO_US_AT_0               500
#define CFG_SERVO_US_AT_180             2400
#define CFG_SERVO_INVERT                0

/* =====================================================================
 *  8. Buddy 1: MQTT, telemetry and commands
 * ===================================================================== */

/* Topics.  [REPORT 6.1: under inf2004/IS19; publish telemetry, event,
   status; subscribe cmd; answer on ack] */
#define CFG_TOPIC_ROOT                  "inf2004/IS19"
#define CFG_TOPIC_TELEMETRY             CFG_TOPIC_ROOT "/telemetry"
#define CFG_TOPIC_EVENT                 CFG_TOPIC_ROOT "/event"
#define CFG_TOPIC_STATUS                CFG_TOPIC_ROOT "/status"
#define CFG_TOPIC_CMD                   CFG_TOPIC_ROOT "/cmd"
#define CFG_TOPIC_ACK                   CFG_TOPIC_ROOT "/ack"

/* Largest JSON payload.  lwIP's MQTT output buffer
   (MQTT_OUTPUT_RINGBUF_SIZE, raised to 512 in lwipopts.h) holds the
   whole PUBLISH: 3 bytes fixed header, 2 + 22 bytes for the longest
   topic, then the payload.  The longest IS19 message, telemetry with
   every field at its extreme, is 259 bytes (host test S1-U08).  A
   message that does not fit is dropped and counted, never sent
   truncated.  [DECISION D-49] */
#define CFG_MQTT_PAYLOAD_MAX            320
/* Largest incoming command.  [TEMPLATE mqtt_utk.c RX buffer, 128] */
#define CFG_MQTT_CMD_MAX                128
/* How often the MQTT code takes messages from mbf_publish is set in
   the MQTT code itself, which cannot include this file:
   MQTT_UTK_PUMP_MS and MQTT_UTK_PUMP_BURST in
   lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c.  [DECISION D-50] */
/* Route log entries kept for the run summary.  [DECISION D-51] */
#define CFG_TLM_ROUTE_MAX               48
/* Route log entries published again per 200 ms tick after a summary,
   so the publish queue is never flooded.  [DECISION D-60] */
#define CFG_TLM_REPLAY_PER_TICK         4u
/* 1 = also print every message on the console (bench tests without a
   broker).  [DECISION D-69] */
#define CFG_TLM_ECHO_CONSOLE            0
/* A broker command is acknowledged first and applied this long after,
   so the ack still leaves on the old broker.  The new address is kept
   in RAM only: a reset goes back to config/wifi_secrets.h.
   [DECISION D-16; UNVALIDATED: many 10 ms publish-pump cycles; S1-U33
   checks that the ack arrives on the old broker] */
#define CFG_CMD_BROKER_DELAY_MS         300u

#endif /* APP_CONFIG_H */

/*** end of file ***/
```
