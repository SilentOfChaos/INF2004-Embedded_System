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

/* Measurement for the minimum-bit change (common/app_stats.h): 1 records
   task periods, work times, interrupt handler times, stack high-water
   marks and queue use, and prints them on the console every REPORT_MS.
   Off in normal builds.  Build the same setting before and after a change
   to compare them.  [DECISION D-82] */
#define CFG_STATS_ENABLE                0
#define CFG_STATS_REPORT_MS             5000

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

/* Optional timing probe for A12: the Line task toggles this pin at
   every 5 ms release (not on other wake-ups) so a logic analyser can
   check each period (S3-U33).  -1 = off.
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
#define CFG_LINE_LOST_MS                500  /* was 300: with both sensors briefly white on a straight, 300 ms started a search every half second */
/* Failed line-sensor reads in a row (4 = 20 ms) that latch a LINE_DATA
   fault in Safety; each failed read marks the line record invalid.
   [DECISION D-79, G15 decided 16 Oct 2026] */
#define CFG_LINE_READ_FAIL_LIMIT        4
/* Milliseconds in which the line record's sample count does not move
   while running: the Line task has stopped publishing, so Navigation
   stops using the record and reports a LINE_DATA fault, as for failed
   reads.  Elapsed time, not a count of ticks, so a late Navigation tick
   does not stretch it; it is seen at the first tick at least this long
   after the count last moved.  [DECISION D-94, G19 decided 23 Oct 2026] */
#define CFG_NAV_LINE_STALE_MS           20

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
#define CFG_STEER_KP_MMPS               90   /* was 150: jitter on the mat */
#define CFG_STEER_KD_X1000              100  /* was 300: D amplified sensor noise */
#define CFG_STEER_MAX_DIFF_MMPS         120  /* was 200 */
#define CFG_STEER_SLOWDOWN_PM           400

/* =====================================================================
 *  4. Buddy 2: motion
 * ===================================================================== */

/* Geometry.  [UNVALIDATED: typical 65 mm TT wheel (the kit information
   says 65 mm too) and a typical 2WD TT chassis; measure both (A2)] */
#define CFG_WHEEL_DIAMETER_MM           65
#define CFG_TRACK_WIDTH_MM              130

/* Encoder counts per wheel turn in the selected decoding mode.
   [UNVALIDATED: no datasheet for the kit's "encoder TT motor 6V 350RPM";
   placeholder from a common TT encoder motor (11-pulse Hall encoder,
   1:48 gearbox, rising edges of channel A only).  Measure (A2).] */
#define CFG_ENC_COUNTS_PER_REV         528  /* back from 624 (19 Nov): with 624 every turn and leg overshot (a "90 deg" turn carried the car back across the line) */
/* 0 = count rising edges of channel A only (lowest interrupt load, A8);
   1 = count every edge of A and B (4x resolution).  The value above must
   be measured in the selected mode.  [DECISION D-28] */
#define CFG_ENC_QUADRATURE_X4           0
/* Swap the count direction of one encoder.  [UNVALIDATED: depends on
   how the motors are mounted; set on the bench] */
#define CFG_ENC_L_INVERT                0
#define CFG_ENC_R_INVERT                1
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
#define CFG_MOTOR_R_INVERT              1
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
/* Stall and encoder-fault watch (Change Requirement 1): a wheel driven
   at this duty or more whose encoder gives no count for this long is
   reported (motion_ctl.h).  [DECISION D-105 / UNVALIDATED: the duty is
   above CFG_MOTOR_MIN_DUTY_PM, the start-up estimate, so a wheel that
   only needs time to start is not counted; the time covers the PID's
   whole climb from this duty to the most it can give, 1.0 s with the
   gains above, so a hump the PID can climb never trips it (S2-U07
   checks this for every target speed).  Buddy 2 to set both from a
   held wheel, an unplugged encoder (wheels off the ground) and a hump
   crossing, and to keep S2-U07 passing if the gains change] */
#define CFG_MOTION_STALL_DUTY_PM        300
#define CFG_MOTION_STALL_MS             1000

#if (CFG_MOTION_STALL_DUTY_PM <= CFG_MOTOR_MIN_DUTY_PM)
#error "CFG_MOTION_STALL_DUTY_PM must be above CFG_MOTOR_MIN_DUTY_PM"
#endif

/* =====================================================================
 *  5. Navigation (shared)
 * ===================================================================== */

/* Line-following speed range: NFR17's speeds at which line following
   must work (shown by SYS-17).  The speed Navigation commands while
   following, after the steering slow-down, is kept inside it, so the
   barcode timing (report 6.3: 120 ms is longer than a wide space at
   0.15 m/s) holds.  This limits the commanded speed, not the measured
   one.  [REPORT NFR17; DECISION D-71] */
#define CFG_NAV_SPEED_MIN_MMPS          100  /* below NFR17 (150) for tuning */
#define CFG_NAV_SPEED_MAX_MMPS          350
/* Cruise and hump speeds.  [DECISION D-32: choices inside NFR17's
   range; UNVALIDATED: that the car follows the line at them is checked
   by SYS-17]
   Lowered to 120 for tuning (jitter on the mat): below NFR17's 150 and
   the barcode timing assumption (report 6.3); go back to 150 if barcodes
   misread. */
#define CFG_NAV_CRUISE_MMPS             120
#define CFG_NAV_HUMP_MMPS               120

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
#define CFG_NAV_PASS_GAP_MM             120  /* was 75: the car passed too close to the box */
/* Rejoining after the bypass: with the axle on the line the car turns
   back along it until a line sensor sees it, at least ALIGN_MIN and at
   most ALIGN_MAX, so a line that bends near the box is still followed
   (a fixed 90-degree turn lost it).  [Tuning, 19 Nov 2026] */
#define CFG_NAV_ALIGN_MIN_CDEG          4500
/* Rejoin blocked (the obstacle is still ahead): turn back parallel, go
   this much further past it, and try again, at most RETRIES times, then
   SAFE HOLD.  [Tuning, 19 Nov 2026] */
#define CFG_NAV_REJOIN_MORE_MM          150
#define CFG_NAV_REJOIN_RETRIES          3
/* While following with neither sensor on the line, the held error
   shrinks by this factor each 5 ms tick (half in about 170 ms), so a
   line between the sensors no longer keeps the car turning one way
   (the cause of the zigzag on straights).  1000 = held as before (D-52).
   [Tuning, 19 Nov 2026] */
#define CFG_NAV_LOST_DECAY_PM           980
#define CFG_NAV_ALIGN_MAX_CDEG          13500
/* Ultrasonic beam spread, per mille of range (about tan 15 deg).  The
   scan places each edge where the beam pointed, but the beam's side
   still hits the obstacle beyond its real edge, so the measured width
   comes out too wide (a ~15 cm box read as 36-40 cm at 28 cm).  The
   sideways leg takes closest x this off the edge.  [Tuning, 19 Nov
   2026; check with boxes of known size] */
#define CFG_NAV_BEAM_SPREAD_PM          270
#define CFG_NAV_REJOIN_MAX_MM           400  /* was 300 */
/* Rejoin drove CFG_NAV_REJOIN_MAX_MM without crossing the line (the line
   bends near the obstacle): 1 = sweep for it (LINE SEARCH, 5 s) instead
   of SAFE HOLD.  [Tuning, 19 Nov 2026] */
#define CFG_NAV_REJOIN_MISS_SEARCH      1
/* Neither side looked clear, even after reversing (clutter or no echo
   at the sweep's ends): 1 = go round anyway on FALLBACK_SIDE (1 = left,
   0 = right) with this sideways leg and depth, still watching ahead on
   the way back; 0 = SAFE HOLD as FR11 describes.  [Tuning, 19 Nov 2026] */
#define CFG_NAV_NO_ROOM_FALLBACK        1
#define CFG_NAV_FALLBACK_LEFT           1
#define CFG_NAV_FALLBACK_LATERAL_MM     250
#define CFG_NAV_FALLBACK_DEPTH_MM       200
/* Longest leg driven beside the obstacle before giving up.
   [DECISION D-36; UNVALIDATED: obstacle size on the course unknown] */
#define CFG_NAV_BYPASS_MAX_MM           900  /* cap on the passing leg below */
/* Passing leg by dead reckoning: the car drives the obstacle's range +
   its depth (taken as its measured width, kept within MIN..MAX) + the
   car's length + a margin, so its rear is clear even if the side scan
   never reports "behind".  The side scan still ends the leg early when
   it does.  Then it turns back in, watching ahead (SCAN_REQ_REJOIN).
   [Tuning, 19 Nov 2026; CAR_LENGTH UNVALIDATED: measure] */
#define CFG_CAR_LENGTH_MM               200
#define CFG_NAV_DEPTH_MIN_MM            100
#define CFG_NAV_DEPTH_MAX_MM            300
#define CFG_NAV_PASS_MARGIN_MM          100
/* Extra forward travel after the side scan reports the obstacle behind,
   so the rear of the car clears it; Navigation adds the side scan's
   look-ahead to it (D-97).  Doubled from 150 to 300: on the car the
   forward leg past the obstacle was too short (it turned back toward the
   line about as soon as it cleared the obstacle), so the car now drives
   further along before turning in to rejoin (measured on the car).
   [UNVALIDATED: about the car's length behind the sensor; measure] */
#define CFG_NAV_PAST_EXTRA_MM           300
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
#define CFG_IMU_SWAP_XY                 1 /* was 0 */
#define CFG_IMU_INVERT_X                1 /* was 0 */
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
#define CFG_IMU_IMPACT_MG               2800 /* calibration jolt and the
                                                   "impact" label only: no
                                                   longer a collision (lift
                                                   detection below).  Motor
                                                   start and pivot jolts reach
                                                   1500-2450 mg. */
/* Lift / overturn detection (replaces IMU collision detection,
   requirement change 19 Nov 2026).  While a run is active, the IMU
   compares each sample with the gravity vector stored at calibration:
     LIFTED      acceleration along "up" at least LIFT_MG above 1 g for
                 LIFT_SAMPLES samples in a row (the car being picked up);
     OVERTURNED  the angle to the stored "up" is larger than the angle
                 whose cosine is OVERTURN_COS_PM for OVERTURN_SAMPLES in
                 a row (500 per mille = 60 degrees, well above a hump).
   Either latches a LIFTED fault: Safety brakes the motors at once.
   Lifting the car is how a demo run is ended.
   [UNVALIDATED: tune with logs: lift gently and quickly, cross the hump,
   pivot turns] */
#define CFG_IMU_LIFT_MG                 250
#define CFG_IMU_LIFT_SAMPLES            5    /* 50 ms */
#define CFG_IMU_OVERTURN_COS_PM         500  /* 60 degrees */
#define CFG_IMU_OVERTURN_SAMPLES        20   /* 200 ms */
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
   Echo high for about 38 ms when nothing returns, which left only 1-2 ms
   in a 40 ms wait (24 Oct review, R-07).  Raised from 50 to 60 ms: our
   module's no-target pulse runs past 50 ms, so open space timed out as
   invalid and tripped three-in-a-row NO SAFE ROUTE on START (measured on
   the car, S5-U34).  60 ms still fits under the 65 ms ping period and
   lets the full no-target pulse complete, so open space now reads as a
   valid maximum range instead of an invalid ping.  Must stay longer than
   the module's no-target pulse and shorter than CFG_PERIOD_SCAN_MS. */
#define CFG_US_ECHO_TIMEOUT_MS          60
/* Pings are never closer together than this, even if the task's 65 ms
   release is late (D-75).  [REPORT NFR5; HCSR04.pdf: over 60 ms] */
#define CFG_US_MIN_PING_GAP_MS          60

/* Two readings at or below 30 cm stop the car; three invalid readings in
   a row give NO SAFE ROUTE.  [REPORT Fig 10, 6.5] */
#define CFG_SCAN_DETECT_MM              300
#define CFG_SCAN_DETECT_COUNT           2
#define CFG_SCAN_INVALID_LIMIT          3

/* Reported-range glitch filter: this many open-space pings in a row
   before the telemetry range jumps to the maximum.  A single stray
   "nothing there" ping (a weak or off-angle reflection off a real
   object) is then ignored, while a genuinely removed object still reads
   as clear within a few pings.  This filters only the REPORTED range,
   not obstacle detection (which uses the raw per-ping reading).
   [UNVALIDATED: starting value, tune with logs] */
#define CFG_SCAN_FAR_CONFIRM            3

/* Scan angles, degrees, 90 = straight ahead, larger = left.
   [REPORT 6.5: coarse 30-150 in 30 degree steps, fine 5 degree steps] */
#define CFG_SCAN_COARSE_FIRST_DEG       45     /* was 30 */
#define CFG_SCAN_COARSE_LAST_DEG        135    /* was 150 */
#define CFG_SCAN_COARSE_STEP_DEG        22     /* was 30: 45, 67, 89, 111, 135 */
#define CFG_SCAN_FINE_STEP_DEG          5
/* The servo's travel: no angle outside it is ever sent (drv_servo.c),
   and the fine and side scans stop at it.  [DECISION D-97; UNVALIDATED:
   the kit information says at most 140 degrees, and another group with
   the same kit tested 30-150; measure ours (S5-U30) before widening] */
#define CFG_SCAN_SERVO_MIN_DEG          45     /* was 30 */
#define CFG_SCAN_SERVO_MAX_DEG          135    /* was 150 */
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
/* Side scan while bypassing: servo angle looking at the obstacle, as
   far sideways as the servo goes (D-97: short of square, so Navigation
   adds the look-ahead to its extra travel), the range above which the
   obstacle counts as passed, and how many such valid readings are
   needed after it was seen beside the car.  Invalid readings are
   skipped: they never confirm clearance (report 6.5, D-74).
   [DECISION D-48, D-97; UNVALIDATED: range and count to check while
   passing a box (S5-U33)] */
#define CFG_SCAN_SIDE_LEFT_DEG          CFG_SCAN_SERVO_MAX_DEG
#define CFG_SCAN_SIDE_RIGHT_DEG         CFG_SCAN_SERVO_MIN_DEG
#define CFG_SCAN_SIDE_CLEAR_MM          500  /* was 400: box at 45 deg read just over 400 */
#define CFG_SCAN_BEHIND_COUNT           3    /* was 2: a round obstacle gave gaps that read as "passed" too early */
/* While driving back to the line after a bypass the sensor looks
   straight ahead: this many readings (CFG_SCAN_DETECT_COUNT) at or
   below this range mean the obstacle is still in the way.  [Tuning,
   19 Nov 2026] */
#define CFG_SCAN_REJOIN_BLOCK_MM        250
/* Pings to skip after moving the servo, so it has settled.
   [UNVALIDATED: one 65 ms period; typical SG90 speed is about 0.1 s per
   60 degrees, not from a supplied datasheet] */
#define CFG_SCAN_SETTLE_PINGS           1

/* SG90 servo, 50 Hz: the pulse widths for 0 and 180 degrees, so the
   line through them gives every angle between; only angles within
   CFG_SCAN_SERVO_MIN/MAX_DEG are sent.  [UNVALIDATED: typical SG90
   values; check 30, 90 and 150 degrees with a protractor (S5-U30)] */
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
   every field at the extreme its packed record can hold, is 249 bytes
   (host test S1-U08; 259 before the records were packed, D-92).  A
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
