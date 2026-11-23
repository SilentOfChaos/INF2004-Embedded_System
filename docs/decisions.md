# IS19 decisions log

Every choice the W6 report ([summary](notes/04_design_review_summary.md))
left open, every approved improvement, and every change to the template is
recorded here. The code refers to these entries as `D-nn`.

Rules for this code, given on 13 Oct 2026 to the Claude Code session that
wrote its first version (a team member's instructions; the whole team
has not confirmed them yet):

- Values that have not been measured on the car are typical or datasheet
  starting values. Each one is marked **UNVALIDATED** in
  [`app_program/app_config.h`](../app_program/app_config.h), kept there,
  and given its source. None of them is a measured value.
- Where the report leaves a choice open, the simplest behaviour consistent
  with the report and the template is used. Each such choice is a named
  setting where it can be adjusted, and is recorded below.
- The approved improvements (ignore barcode reads while turning at a
  junction, filter glitch edges, classify wide/narrow per character, decode
  as soon as the symbol is complete with a bounded timeout as well) are
  implemented and recorded below.
- Report-defined pins, task owners, periods, priorities, cores, safety
  behaviour and interfaces are not changed. Items that need the instructor
  (A11, A12, A14) are implemented as the report describes and stay marked
  pending.
- A choice that would materially change a requirement or safety, or
  contradict the report or the template, is asked, not made in the code.
  Until it is decided the code keeps the report's behaviour where the
  report gives one, says which reading it follows where the report can
  be read two ways, and the question is listed in section E.

The code was checked against the report and these rules on 13 Oct 2026;
what was found and changed is in section F. An independent review on
14 Oct, and what changed after it, is in section G. A second
independent review of those changes, on 16 Oct, is in section H. The
minimum-bit change of 22 Oct is in section I, an outside review of it
on 23 Oct, with what changed after it, in section J, and a second and a
third outside review the same day in sections K and L. A team member's
review of 24 Oct, and what changed after it, is in section M.

Tags used in `app_config.h`: `[REPORT]` from the W6 report,
`[DATASHEET]` from a datasheet, `[TEMPLATE]` from the course template,
`[DECISION D-nn]` this log, `[UNVALIDATED]` to be measured.

## A. Repository and process

**D-01 Repository layout.** This repository was restructured to the course
template's layout (`sirfonzie/INF2004_ProjectTemplate`); the earlier
planning notes moved to `docs/`. One pull request per buddy so each owner
reviews their own code; shared code goes in a base pull request first.

**D-02 Navigation folder.** The report makes Navigation a shared task, so it
lives in `app_program/navigation/`, not in a buddy's folder. Buddy 3's
steering and letter map stay in `subsystem3/` and Navigation calls them.
The template makefile already builds `app_program/*/*.c`.

**D-03 Common helpers.** Code every subsystem needs (fixed-point maths,
GPIO interrupt dispatcher, PWM set-up, time, register addresses) is in
`app_program/common/`, owned by the team.

**D-04 Central configuration.** Every tunable value is in
`app_program/app_config.h`, one section per subsystem, each tagged with
its source. Every value that describes the car, its sensors or the course,
or that has to be tuned on the car, is tagged `[UNVALIDATED]` until it is
measured, also when a datasheet or a decision gave the starting value.
Register values, bus addresses, protocol timings and the scale factors
that follow from a register setting are datasheet definitions, not
measurements, and keep `[DATASHEET]` alone.
Relations the report fixes are checked when compiling (`#error`): every
period is a whole number of ticks and `CFG_TICK_MS` equals the kernel's
`CNF_TIMER_PERIOD` (D-05), cruise and hump speeds lie inside NFR17's range
(D-71), and the scan period leaves NFR5's ping gap (D-75).

**D-17 Kernel object limits.** `CNF_MAX_MBFID` raised from 12 to 20 in
`config/config.h`; IS19 uses 14 message buffers (TEAM.md section 6).

**D-18 Coding standard.** IS19 files follow Barr-C: `g_` for module
globals, `p_` pointers, `b_` booleans, 4-space indentation, constants on the
left of `==`, `/*** end of file ***/`. Exceptions: template files and
template-style files keep their own style (`app_main.c`, `app.h`,
`test_report.h`, the library files A11 extends), and exported kernel-object
IDs keep the template's interface names (`mbf_xxx`, `flg_xxx`).

## B. Kernel and architecture

**D-05 1 ms tick.** `CNF_TIMER_PERIOD` 1 ms instead of the template's 10 ms,
so 5 ms periods are whole ticks (report section 3). Assumption A12 (CPU
≤ 60%, 5 ± 1 ms) must still be checked on the car. `CFG_TICK_MS` mirrors it.

**D-06 Priorities and cores.** The report's task table (priorities 5–12,
cores) is used as written instead of the template's default bands.

**D-07 Cyclic release.** Each periodic task waits on an event-flag bit set
by a cyclic handler, so its period does not drift with its run time.

**D-08 Interrupt hand-off.** Interrupt handlers only time-stamp or count:
barcode edges and ultrasonic echoes go into message buffers with `TMO_POL`;
encoder counts are one of the report's two justified shared variables. No
printing in interrupts (NFR15).

**D-09 One GPIO interrupt dispatcher.** All GPIO interrupts share IO_BANK0
(IRQ 13), so `common/gpio_irq.c` owns it and calls per-pin handlers. Its
NVIC line is enabled by the Navigation task, which runs on core 0, so every
GPIO interrupt is served on core 0 (report section 3). Each event is
acknowledged before its handler runs; passes per interrupt are bounded.

**D-10 Line ADC.** The kernel's `adca` device driver (already enabled in the
template) is used, owned by the Line task.

**D-11 IMU I2C.** A small polled I2C1 driver (`subsystem4/drv_i2c1.c`) with
every wait bounded, instead of switching on the kernel's IIC device, which
would change shared configuration.

**D-12 NeoPixel.** Driven through the SDK's PIO code in its own file
(`drv_neopixel.c`), which may not include the kernel headers (a `size_t`
clash). It uses the SDK include paths of the Wi-Fi build; without them it
compiles to a stub.

**D-13 Fixed point.** No floating point (Barr-C; the build has no maths
library): CORDIC `atan2`, Q15 sine table, integer square root. Units: centi-
degrees, mm, mm/s, mg. The CORDIC scales small inputs up first, so its
accuracy (about 0.01 degree) holds for any input size (host test COM-U02).

**D-14 MQTT hooks (A11).** The MQTT code calls hooks with weak defaults that
keep the template's behaviour; subsystem 1 defines them in
`mqtt_bridge.c`. They only use message buffers with `TMO_POL`: IF-13
publish, IF-14 commands, IF-15 broker. A 10 ms pump publishes queued
messages, keeps a message lwIP has no room for (`ERR_MEM`) and retries it,
and drops messages while the session is down. **Pending instructor
agreement.**

**D-15 Safety pin override (A14).** As the report describes: on a fault
Safety sets OUTOVER = low and OEOVER = enable on GP8–GP11 in one register
write each, and clears them only after Motion acknowledges it has zeroed
its commands. `CFG_SAFETY_PIN_OVERRIDE` 0 leaves braking to Motion alone.
**Pending instructor agreement.**

**D-16 Broker change (NFR14).** The simplest way with no reflash: an MQTT
command `{"cmd":"broker","ip":"…","port":…}`. The new address is kept in
RAM only, so a reset goes back to `config/wifi_secrets.h`. The command is
acknowledged first and applied `CFG_CMD_BROKER_DELAY_MS` (300 ms) later,
so the ack still leaves on the old broker. Limitation: it can only be sent
while the car is reachable on its current broker (see G1).

## C. Values chosen (all in `app_config.h`)

**D-19 Stacks.** 2048 bytes (template default) except Navigation and
Telemetry, 3072, for the larger state machine and the text formatting.

**D-20 Timing probe.** Optional pin the Line task toggles every cycle, for
checking A12 on a logic analyser. Off (−1); GP6 is the spare pin to claim.

**D-21 Button debounce.** 30 ms stable level (see D-65).

**D-22 Junction re-arm.** After a junction, 4 samples with at least one
sensor not fully dark before another junction can be reported: one event
per crossbar.

**D-23 Level adaptation.** A reading in the dark band (≥ 850‰) pulls that
sensor's black level 1/64 of the way towards it; one in the white band
(≤ 150‰) pulls the white level. Levels stay within ±600 counts of the
stored values; if they come within 400 counts of each other the stored
values are restored. Frozen while crossing a hump through IF-11 (review
finding G7; the report itself only says the levels adapt), switchable with
`CFG_NAV_HUMP_FREEZE_LEVELS`.

**D-24 ADC self-test range.** Readings outside 5–4090 at start-up mean a
floating or shorted input.

**D-25 Barcode capture start.** As report 6.3: WAITING ends at the first
bar (`CFG_BC_QUIET_MS` 0). The first version also required 120 ms of white
before a capture could start (a quiet zone). That was not one of the
approved improvements and could miss a barcode with less white before it,
so it was switched off on 13 Oct; a positive `CFG_BC_QUIET_MS` still
selects it, and whether to use it is open question G14. Edges closer than
1 ms cancel each other (approved improvement: glitch filter).

**D-26 Capture limits.** 64 edges or 2000 ms end a capture as failed, as
does 120 ms without an edge. With decoding at the 30th edge (approved
improvement), these keep a bounded failure path: a capture never waits for
an exact edge count.

**D-27 Wide/narrow classification (approved improvement).** Per character,
bars and spaces separately: `*` and every letter have exactly 2 wide bars of
5 and 1 wide space of 4 in either reading direction, so the 2 widest bars
and the widest space are wide, each at least 1.5 times the widest narrow
element of its kind. This tolerates a speed change along the barcode and a
comparator that widens bars and narrows spaces (±40%, design-note test
UT-B3-05, host test S3-U04). The first version (3 widest of all 9
elements) failed that test and was replaced. `CFG_BC_CLASSIFY_PER_CHAR` 0
selects the report's rule (wide if more than twice the narrowest element).

**D-28 Encoder decoding.** Rising edges of channel A only (lowest interrupt
load, A8); `CFG_ENC_QUADRATURE_X4` 1 counts all edges. Counts per turn must
be measured in the chosen mode.

**D-29 Motor PWM.** 10 kHz, half the driver's 20 kHz maximum.

**D-30 Soft start.** Duty may rise by at most 100‰ per 20 ms cycle, to
limit the current step on the power bank (review finding G9, A6); slowing
down is immediate. `CFG_MOTOR_RAMP_PM` 1000 turns it off.

**D-31 Moves and turns.** Encoder-counted moves at 150 mm/s, pivot turns at
120 mm/s, half speed for the last 20 mm.

**D-32 Line-following speeds.** Cruise 250 mm/s, hump 150 mm/s, within
NFR17's 150–350 mm/s.

**D-33 Search pattern.** Sweep 45° towards the side the line was last seen,
90° back across, 45° back to the centre, repeated until the 5 s limit. The
same sweep looks for the new line after a junction turn (3 s limit, report
section 5).

**D-34 U-turn direction.** Left.

**D-35 Newer letter.** A newer letter read before the junction replaces the
held one.

**D-36 Obstacle bypass.** Rectangular path: turn 90° to the chosen side;
drive sideways to the obstacle's edge plus a 75 mm gap plus half the car
(FR11: pass 5–10 cm away); turn back parallel; drive until the side scan
reports the obstacle passed (at most 800 mm); 150 mm more for the rear of
the car; turn in; drive up to 300 mm to find the line (report 6.5); put
the axle on the line and turn along it.

**D-37 Stop settle.** 200 ms after stopping before the scan starts.

**D-38 Hump timeout.** Leave HUMP CROSSING after 5 s if the IMU never
reports the hump passed.

**D-39 Self-test time.** 3 s for every sensor to report healthy; then
FAULT (report section 5).

**D-40 Heading fusion.** Odometry heading, nudged 2% towards the compass at
each IMU heading update on flat ground only (the tilt distorts the compass
on a hump).

**D-41 IMU calibration.** 100 samples (1 s) at rest.

**D-42 Bad IMU data.** 5 samples in a row of invalid data (a failed read,
or a total acceleration below 300 mg) latch a fault (report 6.4), in
CALIBRATING as well as MONITORING (D-76). A total above 3500 mg is not
invalid data but a jolt (D-81, decided 16 Oct).

**D-43 Heading events.** Every second IMU cycle (20 ms).

**D-44 Watchdog.** 200 ms timeout. Armed only after the radio driver
reports its start-up complete, or after 15 s (A1). Check-in deadlines:
Line 50 ms, Navigation 50 ms (10 periods), Motion 100 ms, IMU 50 ms, Scan
325 ms (5 periods).

**D-45 Pin override switch.** See D-15.

**D-46 Acknowledgement wait.** Safety waits up to 500 ms for Motion's zero
acknowledgement after RESET, then stays latched.

**D-47 Alarm.** Buzzer at 2 kHz and red LED, 250 ms on and 250 ms off,
while a fault is latched; LED green when released; brightness 32 of 255.

**D-48 Scan thresholds.** During a scan, returns within 500 mm belong to the
obstacle. While bypassing, the servo looks as far sideways as it goes
(150° or 30°, D-97; at first 180° or 0°); the obstacle counts as passed once it has been seen beside the car and then 2
valid side readings (`CFG_SCAN_BEHIND_COUNT`) are above 400 mm (D-74). One
ping is skipped after every servo move.

**D-49 Message size.** JSON payloads up to 320 bytes. lwIP builds each
PUBLISH in its MQTT output buffer; the default 256 bytes is smaller than a
worst-case telemetry record (259 bytes of JSON plus topic and header), so
`MQTT_OUTPUT_RINGBUF_SIZE` is 512 in `lwipopts.h` (A11). A message that does
not fit is dropped and counted, never sent cut short (host test S1-U08).

**D-50 Publish pump.** Every 10 ms, at most 4 messages per call
(`MQTT_UTK_PUMP_MS`, `MQTT_UTK_PUMP_BURST` in `mqtt_utk.c`); one more lwIP
timeout slot for the pump's timer (`MEMP_NUM_SYS_TIMEOUT`).

**D-51 Route log.** 48 entries in RAM, cleared at each START.

## D. Behaviour

**D-52 Brief line loss.** While the line is not seen, steering keeps the
last error, so the car keeps turning towards where the line was.

**D-53 Events while turning (approved improvement).** Barcode and junction
events count only in LINE FOLLOWING and HUMP CROSSING: the junction
crossbar also passes under the barcode sensor while turning (review
finding G3). A read that starts while the car is not following stays
ignored to its end, even if its letter or failure arrives after following
has resumed (added 13 Oct, host test NAV-U10). A read counts only if the
car was following from its start to its result (14 Oct): if the car stops
following during a read (line lost, a junction, an obstacle, a fault), the
read is ignored even if its result arrives after following has resumed,
and a read that started in READY stays ignored after START. LINE
FOLLOWING ↔ HUMP CROSSING is still following. The state that counts is
the one Navigation is in when it handles the event; Line posts the start
of a read at most one 5 ms cycle (plus the 1 ms glitch filter) after its
first bar, so a read whose first bar came in that last moment before
following resumed is counted. Capture-start time stamps would remove that
window; they are not added (host test NAV-U10). A failed read never clears
a held command.

**D-54 Letters without an action.** Reported and logged, but not held: a
junction after it still finds no command (FR5 → SAFE HOLD), or keeps a
command held earlier.

**D-55 Keepalive.** While a move runs, Navigation sends a keepalive every
5 ms tick it sends nothing else, so Motion's 100 ms update rule (report
6.2) never stops a long move.

**D-56 Turns.** Pivot turns (wheels in opposite directions), counted by the
encoders (FR19).

**D-57 Health and SELF-TEST.** Line, IMU and Scan each set a health level
(IF-17) that SELF-TEST waits for. Obstacle Scan starts in RUN OVER and
only pings for health until START, so SELF-TEST can see a working sensor
and nothing in front of a car waiting in READY starts a scan. Fig 10 draws
MONITORING as the initial state; this is how the code reads it (open
question G11).

**D-58 STEADY class.** FR17's classes have none for driving at constant
speed, so a seventh class, STEADY, is reported instead of forcing one of
the others. This adds to FR17's list (open question G13).

**D-59 Queue depths.** Each receiver sizes its queues for bursts at its own
rate: Navigation's queues 8–16, Motion's 16, Safety's 16, Scan's 8,
ultrasonic echoes 8, barcode edges 64, Telemetry's 24, publish 8,
commands 4, broker 2. (22 Oct: the buffers really held more, because
each message was given 8 bytes of margin; a buffer whose message is
packed now holds exactly that real number, D-83. Navigation's: line 21,
IMU 17, obstacle 9, Motion status 18, commands 10; barcode edges 85;
Motion's commands 20; Safety's faults 21; Scan's requests 10, ultrasonic
echoes 10; Telemetry's 27; the broker queue 2. The publish and command
queues carry MQTT text and keep their bytes.)

**D-60 Route log replay.** After a summary, the log is published again at 4
entries per 200 ms tick, so the publish queue is never flooded.

**D-61 Ultrasonic readings.** An echo shorter than 2 cm, or none within
50 ms, is invalid; an echo beyond 4 m is a valid "nothing in range". The
wait must be longer than our module's echo pulse when nothing returns,
or open space reads as invalid (UNVALIDATED; manual test S5-U34). It was
40 ms until the team member's review of 24 Oct (R-07): common HC-SR04
boards hold Echo for about 38 ms when nothing returns, which left 1 to
2 ms; 50 ms still fits the 65 ms ping period. Changed in `2fee4b3`.

**D-62 Obstacle profile.** The obstacle is the run of neighbouring returns
around the closest one while each is within 500 mm and within 150 mm of its
neighbour. Returns outside the run up to 300 mm beyond its front set the
clearance on their side. The side passed on must have room for the car and
the gap; if both have room, the larger clearance wins (left on a tie).
Settings: `CFG_SCAN_OBSTACLE_MM`, `CFG_SCAN_JUMP_MM`, `CFG_SCAN_DEPTH_MM`.

**D-63 Stored line levels.** The report's "levels stored in the lab" are
constants in `app_config.h`, not flash; until measured they are UNVALIDATED
placeholders (300 white, 3000 black).

**D-64 Mode exits.** As the report: SAFE HOLD → RESET → READY; FAULT →
RESET (cause cleared, Motion acknowledged) → SELF-TEST. The system
statechart gives COMPLETE no exit, so it stays until power-off. Report 6.5
says Obstacle Scan stays in RUN OVER "until a new START", which suggests a
new run can follow COMPLETE; the code follows the system statechart (open
question G12).

**D-65 Buttons.** START (Navigation, every 5 ms) and RESET (Safety, every
10 ms) are polled and debounced, not interrupt-driven.

**D-66 Command format.** Flat JSON on `inf2004/IS19/cmd`:
`remap` (letter `A`–`Z`, action `none`/`straight`/`left`/`right`/`uturn`),
`default_map`, `summary`, `stop`, `broker`. The sender's `seq` is echoed in
the ack on `inf2004/IS19/ack`; anything not understood gets `ok` 0.
Navigation acknowledges remaps once the map has changed (NFR8).

**D-67 Hump reports while turning.** A hump that ends while the car is not
following the line is not logged (report 6.4).

**D-68 Full queues.** A full control queue is a fault (report section 4):
the sender reports `QUEUE_FULL` to Safety. A full telemetry queue only drops
and counts (NFR10); a full command queue drops the command, which then gets
no ack, so the sender can retry. A STOP is the exception: it reaches Safety
even when the command queue is full (D-101).

**D-69 Console echo.** `CFG_TLM_ECHO_CONSOLE` 1 prints every message on the
console, for bench tests without a broker. Off by default.

**D-70 Reconnect back-off.** The broker connection retries after 1, 2, 4 …
30 s (report 6.1, BACKING OFF), back to 1 s once connected, replacing the
template's fixed 3 s. The Wi-Fi rejoin in the template's radio task
(`cyw43_utk.c`, fixed 5 s) is not changed (see G5).

**D-71 Following speed range (NFR17).** NFR17 says line following "shall
work at speeds from 0.15 to 0.35 m/s": the range in which the car must be
shown to follow the line (SYS-17), not a rule that its speed may never be
lower. While following, the speed Navigation sends to Motion after the
steering slow-down is kept inside that range (a design choice, 13 Oct), so
the car is only commanded at speeds that NFR17 and report 6.3's barcode
timing (120 ms is longer than a wide space at 0.15 m/s) cover. Without it,
a full steering error on a hump (150 mm/s less 40%) commanded 90 mm/s.
This limits the commanded speed only: it says nothing about the real speed
(wheel slip, a hump), which SYS-17 and S2-U34 measure. Wording corrected
14 Oct. Host tests NAV-U03, NAV-U14.

**D-72 Obstacle profile event (FR10).** FR10 asks the car to report the
obstacle's position, width, closest point and both clearances; before
13 Oct only the width and closest point reached MQTT (in the route log).
After PLANNING, Obstacle Scan posts the whole profile to Telemetry
(IF-12, `TLM_MSG_OBSTACLE`), which publishes it at once on
`inf2004/IS19/event` as `{"ev":"profile","x":…,"y":…,"w":…,"close":…,
"cl":…,"cr":…,"side":…,"rev":…}` (mm; `rev` 1 for the scan after
reversing; `cl` or `cr` −1 when that side was never seen, D-80). Host
test S1-U11; manual test S5-U33.

**D-73 Barcode input pull.** GP7's internal pull goes to the white level
(pull-down when `CFG_BC_DARK_LEVEL` is 1), so an unplugged sensor reads
white and never starts a capture. The first version used a pull-up, which
reads dark with that setting. This is not unplug detection: an unplugged
sensor, or one that always reads white, is not reported, and the pull says
nothing about the sensor's health (14 Oct). The module's real polarity and
levels are checked by hand (S3-U31, S3-U35).

**D-74 Side scan uses valid readings only (report 6.5).** While bypassing,
an invalid side reading (missing, malformed or timed out) used to count as
"nothing in range", that is, clear, so missed echoes could end the bypass
early. Now the obstacle counts as behind only after it was seen beside the
car and then `CFG_SCAN_BEHIND_COUNT` valid readings were clear; invalid
readings are skipped, and three in a row still give NO SAFE ROUTE. Host
test S5-U05.

**D-75 Ping gap (NFR5).** A ping is never sent less than
`CFG_US_MIN_PING_GAP_MS` (60 ms) after the previous one. Releases are
65 ms apart, but one that runs late leaves the next closer than 60 ms; that
ping waits one period. Manual test S5-U32.

**D-76 Bad IMU data while calibrating (report 6.4).** "Invalid sensor data
in any IMU state latches a fault": in CALIBRATING an invalid sample used to
restart calibration without a fault. Now invalid samples are left out of
the average and counted as in MONITORING; `CFG_IMU_INVALID_LIMIT` (5) in a
row latch `FAULT_CAUSE_IMU_DATA`, and the IMU starts again from
identification so the cause can clear before RESET. A jolt while
calibrating is not invalid data: it restarts the average (D-81). Manual
test S4-U37.

**D-77 Turn rate on flat ground (report 6.4, FR17).** The turn rate is
"taken from the compass on flat ground": on a hump it keeps its last value
and the next one is measured from the first heading after the hump,
smoothed over `CFG_IMU_RATE_SMOOTH` (4) updates. So while the car is on a
hump (motion class climbing or descending) the reported turn rate, in
telemetry and in the IMU's heading events, is the last flat-ground value
held, not a new measurement (noted 14 Oct). Manual test S4-U38.

**D-78 Barcode symbol boundary (14 Oct).** A capture's symbol is its
first 30 edges, the same boundary as the decode at the 30th edge (D-26).
An edge lost inside it (levels that do not alternate, or the Barcode
ISR's full-queue flag while fewer than 30 edges are stored) fails the
capture with reason "edge lost", even if the stored widths would decode;
before, a decode that succeeded won over a loss already recorded. Edges
and losses after the 30th edge belong to whatever follows the barcode
and never stop a complete symbol from decoding. The ISR's flag only says
that the dropped edge is newer than every edge still queued, so the Line
task marks the capture when the flag arrives (a loss among queued edges
then counts as inside, the safe side) and again after it has taken the
queued edges (for a capture they started). Left open: an edge dropped
while the Line task is emptying the queue is seen at its next cycle, so a
symbol completed in that same short window still decodes; that needs
chatter fast enough to keep the queue full while it is being emptied. A
drop marker in each queued edge would close it; not added. Host tests
S3-U16, S3-U17; manual test S3-U36.

**D-79 Line-sensor read failures (G15, decided 16 Oct).** When the ADC
read fails, the Line task marks the line record invalid (`b_valid` 0, new
`seq` and time), so Navigation treats it as no line: it keeps the last
error for the moment (D-52) and an invalid record never ends a line
search. `CFG_LINE_READ_FAIL_LIMIT` (4, that is 20 ms) failed reads in a row
report `FAULT_CAUSE_LINE_DATA` to Safety once (detail: the kernel's main
error code); Safety brakes and latches as for any fault (FR15's latch,
the same pattern report 6.4 uses for bad IMU data). RESET needs no extra
check for this cause: it re-runs SELF-TEST, which waits for healthy line
readings (D-96 keeps that health from being older than a failed
read). Only failed reads count; a reading at the ADC rail (a sensor
fully on black) is not a failure. Host test S3-U18.

**D-80 Scan coverage (G16, decided 16 Oct).** Report 6.5 treats missing,
malformed and timed-out readings as invalid "because clearance cannot be
confirmed". An invalid reading in the coarse or fine scan is repeated
once at the same angle (the three-in-a-row rule still counts both). A
side with no valid reading outside the obstacle's run was never seen:
its clearance is −1 (`SCAN_CLEAR_UNKNOWN`) and it has no room, so FR11's
"reverse 10 cm and scan once more" follows if neither side has room, then
NO SAFE ROUTE. A coarse scan with no hit reports the path clear only if
all five readings are valid; otherwise the path is not confirmed and is
handled like no room (reverse once, scan again, then NO SAFE ROUTE with
reason "invalid"). A valid "nothing in range" (D-61) counts as seen, so
this rule relies on open space giving a valid reading, not a timeout
(manual test S5-U34); MONITORING already relies on it, since three
invalid readings in a row stop the car there too. Host test S5-U06;
manual test S5-U35.

**D-81 Accelerometer overrange and jolts (G17, decided 16 Oct).** In
MONITORING one sample above `CFG_IMU_MAX_G_MG` (3.5 g; the ±4 g range is
close to saturating) is a sudden impact: it latches a collision at once
(FR15, report 6.4), one report per impact, with |a| in mg as the detail.
Before, it counted only towards the 5-in-a-row invalid-data fault, so a
hard 1–4 sample crash could be missed while a softer bump was caught.
Failed reads and totals below 0.3 g keep D-42. In CALIBRATING a jolt (a
total that far above the range, or at least `CFG_IMU_IMPACT_MG` away from
1 g) means the car is not at rest: the average starts again without a
fault, since the motors are off; report 6.4's "any IMU state" is read as
applying to invalid data here (D-76), not to a bump while the car is
placed. Host test S4-U06; manual test S4-U37.

**D-93 GPIO edge time stamps (external review, 23 Oct).** The shared
dispatcher (D-09) took one time stamp when the interrupt was entered
and passed it to every handler it called in that interrupt. An edge
found on a later pass, or in a status register read after other
handlers had run, happened after those earlier reads, so it was dated
too early by however long the handlers took: normally microseconds,
but enough to make a pulse that arrived during a slow handler read as
zero width. Each status read now has its own time stamp, read just
after the status, so no edge is dated before it happened. In the usual
case (events in one register, one pass) that is still one timer read
per interrupt. The time stamp is still taken in software after the
interrupt is entered, so interrupt latency remains: S3-U34 (on the car,
a known square wave on GP7 with both wheels at full speed) measures the
result. The measurement build still times the dispatcher from entry.

**D-94 A line record that stops changing (G19, decided 23 Oct).** The
Line task publishes a line record every 5 ms, and each one, a failed
read included (D-79), moves its 8-bit sample count (IF-02). While a run
is on (line following, hump crossing, line search, junction, avoiding),
Navigation keeps the count it last saw and the time it first saw it.
If, at one of its ticks, the count has not moved for
`CFG_NAV_LINE_STALE_MS` (20 ms) or more, the Line task has stopped
publishing. The record is then not used: it does not count as line
seen, so an old "line seen" can no longer end a line search or a
rejoin, and steering keeps the last error, as for an invalid record
(D-52). Navigation reports `FAULT_CAUSE_LINE_DATA` once, with source
Navigation and that time in ms as the detail (at most 8191), and Safety
brakes and latches as for any fault (D-79). The check starts again at
each START and is not made outside a run, where nothing steers on the
record. It measures time, not ticks (changed after the second outside
review, section K): a late Navigation tick reports at once instead of
stretching the limit, and ticks that come close together are not taken
for 20 ms. The fault is seen at Navigation's first tick 20 ms or more
after the count last moved, so it also depends on Navigation running
(its check-in deadline is 50 ms); how soon the motors stop after that
is Safety's path, to be measured on the car (NFR6). 20 ms, not 5: with
both tasks on time, Navigation sees the count move at least every
10 ms (a Line period plus a Navigation period), and a build check keeps
the limit at least twice that. Before this, a stalled Line task was
caught only by Safety's check-in and the watchdog, about 0.25 s later
and only once the watchdog was armed (radio up, or 15 s after boot).
RESET needs no extra check: if the Line task is still stalled, the next
run reports it again 20 ms after it starts. Navigation's state grows by
8 bytes (`nav_sm_t`, 432 to 440). Host test NAV-U40.

**D-95 A fault while RESET waits for Motion (G20, decided 23 Oct).**
RESET with the cause cleared asks Motion to clear its queue, command
and PID state, and the latch is released only on Motion's
acknowledgement (IF-09, SD2). Before, a fault that arrived in between
was only counted, and the acknowledgement then released the latch, so
that fault was never latched, alarmed or reported as the cause. Now
such a fault cancels the reset and becomes the latched cause (cause,
source and detail): the brake override, the FAULT level and the alarm
stay on, the new cause is sent as a latched fault report, and an
acknowledgement that arrives later is ignored. RESET must be pressed
again, and its "cause cleared" check then applies to the new cause.
Safety handles the faults of a wake-up before the acknowledgement, so
a fault and an acknowledgement in the same wake-up also cancel the
reset. Nothing else changes: while a fault is latched and no reset
waits, a later fault is counted and the first cause stays; an
acknowledgement when no reset waits is ignored; with no
acknowledgement within 500 ms the reset is abandoned and the latch
stays. The latch and its handshake are now plain logic
(`safety_latch.c`) that `safety_task.c` acts on, so the host tests run
every state against every event. Owner (S4) to confirm. Host test
S4-U07.

**D-96 Line health is never older than a bad sample (second outside
review, 23 Oct).** The Line task publishes line health
(`NAV_FLG_LINE_OK`, IF-17) for each 200 ms status window: healthy only
if every sample in the window was read without error and in range.
SELF-TEST waits for it, which is why D-79 needs no extra RESET check.
But it changed only at a window's end, so for up to 200 ms after reads
began to fail it still said healthy. A RESET within that time could
pass SELF-TEST on the old window and allow START while every read
failed: D-79 had already reported its one fault and does not report
again before a good read, and G19's check stays quiet because failed
reads still move the count (D-94). Now the first bad sample of a
window clears the health at once; it is set again only at the end of a
whole window of good samples, and those good reads also clear D-79's
count, so a run started after a recovery is protected by D-79 again.
A similar gap after COMPLETE was found while checking this: there the
Line task reads nothing (RUN OVER) and its health stays up, so a run
started from there (only through a fault and RESET, G12) could begin
with D-79's count already at its limit if the fourth failure came just
as the run ended. A run of failed reads therefore does not span the
idle time: the count starts again while sensing is idle, so D-79 can
report within four reads after START. D-98 then closes the rest of
that gap: SELF-TEST after COMPLETE senses again and waits for new good
reads. Unchanged: START does not re-check health (G15), and Safety's "cause cleared" check for
LINE_DATA stays as it is, since SELF-TEST is the gate. Host tests
S3-U19, NAV-U41; the idle-time line in `line_task.c` is checked by
reading (the task is not in the host build).

**D-98 SELF-TEST after COMPLETE senses the line again (third outside
review, 23 Oct).** After COMPLETE the Line task is idle (RUN OVER) and
reads nothing, yet its idle cycles still counted as good samples, so it
kept publishing line health as good; and neither FAULT nor the restart
into SELF-TEST woke it, only START did. After COMPLETE, a fault and
RESET, SELF-TEST could therefore pass with no line reading at all. With
D-96's count restart a failing sensor was then reported within four
reads of START, but movement had already been allowed, against the
report's SELF-TEST before READY (report 5, SD2). Now an idle cycle is
not a good sample, so line health drops as soon as the Line task goes
idle and stays down, and Navigation's restart into SELF-TEST makes the
Line task sense again (`NAV_ACT_LINE_RUN_OVER` 0). SELF-TEST then passes
only after a whole status window of new good reads, 200 to 400 ms, as at
power-up; with the reads failing it does not pass, and D-79 reports the
failures. A run after COMPLETE is still only possible through a fault
and RESET (G12, D-64). Host test NAV-U42; the two lines in
`line_task.c` are checked by reading, and NAV-U42 runs the same rules
in its model of the Line task.

**D-97 Servo travel 30°–150° (23 Oct).** The code assumed the scan
servo turns 0°–180°, a typical SG90. The kit information says it turns
at most 140°, and another group with the same kit tested 30°–150°;
another tested 100° (40°–140°), which cannot reach the report's coarse
scan (6.5: 30°–150°). So until ours is measured the servo is limited to
30°–150° (`CFG_SCAN_SERVO_MIN_DEG`, `CFG_SCAN_SERVO_MAX_DEG`; chosen
23 Oct): the report's coarse scan is unchanged, the servo driver never
sends an angle outside the range, and the fine scan stops at 30° and
150°. While passing an obstacle the side scan looks as far sideways as
the servo goes, 60° from straight ahead, so 30° short of square (D-48
had 90°). It therefore loses the obstacle early, by about the passing
distance (gap plus half the car, 150 mm) × tan 30°, 87 mm, and
Navigation adds that to the extra travel after "behind": 237 mm instead
of 150 mm, worked out from the settings, so it follows any later change
of the range (a build check keeps the look-ahead at most 45°). Whether
the sensor gets an echo from a flat box side 30° off square is to be
checked with a box (S5-U33). The pulse widths are still the typical
SG90 values; S5-U30 checks 30°, 90° and 150° with a protractor and
finds the real end stops, after which the range may be widened. The
wheel diameter, 65 mm, already matched the kit information. Owner (S5)
to confirm. Host test NAV-U15; changed in `35ae793`.

**D-99 Measured time, not the nominal period (24 Oct review, R-05).**
Motion turned each read of the encoder counts into a speed by dividing
by the nominal 20 ms, and the Line task added a nominal 5 ms per sample
to its line-lost timer. A late release, or two merged into one (a
task's tick bit is cleared when it is read, so a second release before
the read is lost), then doubled the measured speed for one cycle or
stretched the 300 ms line-lost time. Both now use the time measured
since their previous read: Motion the µs clock between encoder reads,
for the speed and for the PID's time step (in whole ms); the Line task
the ms since its previous release, at most `CFG_LINE_LOST_MS`. Junction
detection stays sample-based. Host test S2-U03; `line_task.c` checked
by reading. Changed in `0e5ecec` and `87476ab`.

**D-100 The PID's drive, made safe (24 Oct review, R-06).** The start-up
duty (`CFG_MOTOR_MIN_DUTY_PM`, 200 ‰) replaced any PID output below
200 ‰ with ±200 ‰ in the target's direction, even when the PID asked for
less because the wheel was too fast; and a negative output with a
forward target ramped through zero into reverse drive. Now
(`motion_output_duty()`) the drive is never against the target's
direction (slowing down is less drive, down to zero, never reverse
drive), and the start-up duty is used only while the wheel stands
still, to break static friction; a zero target gives zero. The start-up
duty and the feed-forward still describe different motors: they must be
measured together (A2: duty against speed for each wheel). Host test
S2-U06. Changed in `0e5ecec`.

**D-101 A STOP is never lost (24 Oct review, R-04).** With the command
input queue full, the MQTT hook dropped every command, a STOP too, and
only counted it (D-68). It now recognises a STOP in the raw payload
(`cmd_is_stop()`, the parser's own rules, with or without `seq`) and
reports it straight to Safety, which latches it even when its own queue
is full (its FAULT_LOST bit). Such a STOP is still counted as a dropped
command and gets no ack; other commands are dropped as before. Host test
S1-U12. Changed in `0bb3580`.

**D-102 The fault mask (24 Oct review, R-02; G21).** Change Requirement
1 names motor stall, encoder fault, IMU fault, no echo and Wi-Fi loss
as bits of a shared fault mask. Built as the team agreed after
Tribunal 1 (Version C): each cause is one bit, bit (cause − 1), with a
latch flag, in one fault table (`imu_msgs.h`). There are 11 causes: the
7 before, MOTOR_STALL and ENCODER (D-105), which latch, and NO_ECHO
(D-104) and WIFI_LOSS (D-103), status causes that never latch. The
fault report's cause field is 4 bits instead of 3, and a status report
carries a cleared flag (bit 20). Safety stays the only writer of the
mask: every report reaches it through its queue (IF-04), so no two
tasks or cores ever write the mask and no read-modify-write can race
(the Cortex-M0+ has no atomic one, and the RP2040's set and clear
aliases exist only for peripheral registers). A status report only
sets or clears its bit: nothing is braked, it is not counted as a fault
and it does not cancel a waiting RESET; `safety_report_status()` never
raises FAULT_LOST on a full queue, the reporter sends it again later.
Every latching cause sets its bit, the first and the counted ones, so
the mask shows every cause since the latch; the release clears those
bits and keeps the status bits. RESET's "cause cleared" check now
looks at every IMU cause in the mask, not only the first cause latched.
Safety's status record carries the mask (word 1, bits 16–26), and the
heartbeat prints it as `"mask"`, a number, bit (cause − 1), right after
`"cause"`: the one change to the MQTT text (S1-U43). Host tests S4-U07,
S4-U08, S4-U41, S1-U42, S1-U43. Changed in `4d43ac3`.

**D-103 Wi-Fi loss (24 Oct review, R-02).** The Command task, which
checks the link every 100 ms, tells Safety when the MQTT link goes
(Wi-Fi down or the broker unreachable) and when it comes back: the
WIFI_LOSS bit of the fault mask, a status bit that never stops the car
(NFR10). From boot it is set until the first connection; in a build
without Wi-Fi it stays set. While the link is down nothing reaches the
broker, so on MQTT the bit is seen mostly as it clears; the console
shows every link change. If Safety's queue is full the change is sent
again at the next check. Manual test S1-U34. Changed in `66be08e`.

**D-104 No echo, and SELF-TEST needs the sensor to answer (24 Oct
review, R-02, R-07).** Two changes the team agreed after Tribunal 1.
Three invalid readings in a row (`CFG_SCAN_INVALID_LIMIT`) set the
NO_ECHO bit of the fault mask, and the next valid reading clears it. It
is a status bit: in a run the same readings still give NO SAFE ROUTE
and Navigation's SAFE HOLD (6.5), and a sensor that does not answer at
all fails SELF-TEST, which latches. SELF-TEST's ultrasonic health
(IF-17) is now "the sensor answers its pings" instead of "valid
readings": a ping is answered when an echo rose after its trigger, or
when Echo was low before the trigger and is still high when the wait
ends (a pulse longer than the wait, which open space can give: risk
R4). Health is lost after three pings in a row without an answer. A
working sensor facing open space passes; an unplugged one (Echo pulled
low) fails. Ranging itself is unchanged: R4's other half, reading a
pulse longer than the wait as "nothing in range", waits for the
no-target pulse to be measured (S5-U34). Manual tests S5-U36 and
INT-17; on the car S5-U20. Changed in `4391724`.

**D-105 Motor stall and encoder fault (24 Oct review, R-01).** Nothing
noticed a driven wheel whose encoder gives no counts: the PID drove it
harder and Navigation's keepalives kept the move alive, so with one
encoder dead a 90° turn ended at about 260° (the review's model), and
with both dead the car spun on. Motion now watches each wheel
(`motion_stall_step()`, pure, its state one word). A wheel driven at
`CFG_MOTION_STALL_DUTY_PM` (300 ‰) or more whose encoder gives no
count for `CFG_MOTION_STALL_MS` (1 s) is reported once to Safety, which
latches it: MOTOR_STALL when both wheels are silent (a stall or jam, or
both encoders dead) or the other wheel is not counting either, ENCODER
when the other wheel counts (a dead encoder, or that wheel held: the
two look the same, risk R5). The detail is the silent wheels (1 left,
2 right). Both values are UNVALIDATED, for Buddy 2 to set from a held
wheel, an unplugged encoder and the hump (S2-U38). They were chosen so
that the watch does not trip at start-up or on a hump, the post-Tribunal
report's concern: the duty is above the 200 ‰ start-up duty, so a wheel
that only needs time to start is not counted, and the time covers the
PID's whole climb from that duty to the most it can give (1.0 s with
today's gains, at every target speed), so a wheel the PID can still get
moving is never reported. S2-U07 checks this with the real PID for every
target from 10 to 350 mm/s, and fails if the gains change without the
time. A model with the real PID and a simple motor (it starts at a set
duty, then 1 mm/s per ‰ above 50 ‰, 80 ms lag) gave: no trip on any
hump the PID can climb, at 100 to 350 mm/s; no trip at start-up for
start-up duties up to 400 ‰; a jam, or both encoders dead, reported
after about 1.0 to 1.2 s; one dead encoder in a 90° turn reported after
about 1.2 s, by when the turn has gone much too far (about 250° in the
model). Detecting sooner would trip on humps; the IMU could tell a dead
encoder from a held wheel sooner (R5), not done. Navigation's counted
steps still have no deadline (G24). Host test S2-U07; manual S2-U38.
Changed in `50f7f39`.

**D-106 Start-up failures (24 Oct review, R-23).** `usermain` printed a
failed `_init()` or `_start()` and carried on, so tasks could use
objects that do not exist, and Navigation could run without Safety. Now
a kernel object that cannot be created starts no task at all, and a
task that cannot start keeps Navigation, the only task that commands
the motors, from starting; either way the console says so and the car
stays still. Checked by reading (`app_main.c`). Changed in `06ad014`
and `b7e9553`.

## E. Open questions

| ID | Question | Current behaviour |
|---|---|---|
| G1 | Demo-day network: which Wi-Fi and broker? Only the broker can change without reflashing (D-16): the Wi-Fi name and password, and the first broker, come from `config/wifi_secrets.h` at build time, and changing the Wi-Fi at run time would need a change to the template's radio task. | `config/wifi_secrets.h` values; broker changeable by command (D-16) |
| G2 | Should START also be possible over MQTT? | No: FR16 as written, START button only |
| G4 | After a failed barcode read, reverse and read again? | No: the speed hold is released and any held command is kept |
| G5 | Apply report 6.1's back-off to the Wi-Fi rejoin as well? It needs a change to the template's radio task. | MQTT only (D-70) |
| G11 | Fig 10 draws MONITORING as Obstacle Scan's initial state. Is RUN OVER (health pings only) until START the right reading? | RUN OVER until START (D-57) |
| G12 | May START begin a new run from COMPLETE? Report 6.5 says "until a new START"; the system statechart gives COMPLETE no exit. | No: COMPLETE until power-off (D-64) |
| G13 | Keep the STEADY motion class, which FR17 does not list? | Reported as a seventh class (D-58) |
| G14 | Use a barcode quiet zone (120 ms of white before a capture may start)? It is not in the report or the approved improvements. | Off: capture starts at the first bar (D-25) |
| G15 | When a line-sensor read fails, the Line task keeps checking in but leaves the last line record in place, and Navigation keeps steering on it while running (health is only checked in SELF-TEST). What should happen, after how long (fault, SAFE HOLD or line search), and should START check health again? | **Decided 16 Oct:** the record is marked invalid and 4 failed reads in a row latch a LINE_DATA fault (D-79). START does not re-check health; SELF-TEST does after RESET. Owner (S3) to confirm |
| G16 | Invalid scan readings leave a side's clearance at the 4 m maximum, and a coarse scan with no valid hit reports the path clear; invalid readings that are not three in a row never stop the car. What coverage is enough to plan a bypass or clear the path, and what happens without it? | **Decided 16 Oct:** invalid readings are repeated once; a side never seen has no room; path clear needs all five coarse readings valid (D-80). Owner (S5) to confirm |
| G17 | An accelerometer sample above 3.5 g (or below 0.3 g) is invalid data, not an impact: a short spike is only counted towards the 5-in-a-row fault. Calibration does not look for shocks. How should overrange and shocks while calibrating be treated? | **Decided 16 Oct:** above 3.5 g while monitoring is a collision at once; a jolt while calibrating restarts it without a fault; below 0.3 g and failed reads keep D-42 (D-81). Owner (S4) to confirm |
| G18 | FR19's commanded-distance and commanded-turn tests (S2-U35, S2-U36) cannot be run as written: nothing lets a tester start a single `moveForward(500)` or a 45° or 90° turn on demand; such moves only happen inside Navigation's own manoeuvres (junction, line search, obstacle bypass). Add a bench routine, or measure the moves Navigation already makes? | Not runnable (raised 14 Oct, not changed) |
| G19 | A line record that stops changing (the Line task stalled or held up) is still used while it is valid: Navigation keeps steering on it, and an old "line seen" can end a line search or a rejoin. Safety's check-in and the armed watchdog reset the chip within about 0.25 s, but only once the watchdog is armed (radio up, or 15 s after boot). Should Navigation treat a record whose sample count has not moved for a set time as a fault? Proposed: 4 of its ticks (20 ms), a LINE_DATA fault, as D-79 does for failed reads. | **Decided 23 Oct:** as proposed, 20 ms without a new sample while running, then the record is not used and a LINE_DATA fault is reported once (D-94). Counted as 4 ticks at first; measured as elapsed time since the second outside review (section K), so a late tick does not stretch it. Team to confirm (Navigation is shared) |
| G20 | A fault that arrives while a RESET waits for Motion's acknowledgement is counted but does not stop the reset, so the acknowledgement releases the latch and the new cause is never latched. Proposed: such a fault cancels the reset and becomes the latched cause; RESET must be pressed again, and its "cause cleared" check then applies to that fault. | **Decided 23 Oct:** as proposed: the fault cancels the reset and becomes the latched cause; RESET must be pressed again (D-95). Owner (S4) to confirm |
| G21 | Keep a mask of every fault cause seen (one bit per cause, 8 bits) and publish it? Safety keeps only the first cause and the counts. Which bits, when cleared, and where shown? | **Decided** by the team after Tribunal 1 (Version C), built 25 Oct: 11 causes in 4 bits, a latch flag each, Safety the only writer, Wi-Fi loss and no echo as status bits that never latch; the release clears the latching bits; shown in Safety's status record and the heartbeat (D-102 to D-104). Owner (S4) to confirm |
| G22 | MQTT payloads: the change requirement asks for compact binary fields; the instructor asked for messages readable in an ordinary MQTT client (22 Oct). Asked on the course forum. | JSON text (`docs/minimum_bit.md` section 7) |
| G23 | An obstacle ahead is acted on only while following the line, on a hump too: while turning at a junction or searching for the line, Scan is told to carry on, and during an avoidance a second one is not acted on (the lecturer asked in W6 about an obstacle during a hump or a junction; 24 Oct review, R-22). Stop for a very close obstacle in every driving mode, or record the assumption? | Following only (report Fig 5 and 6.5; raised 25 Oct, not changed) |
| G24 | Give each counted Navigation step (junction, avoidance) a deadline of a few times its expected time, and on time-out stop with a fault or SAFE HOLD (24 Oct review, R-01)? Motion's stall watch (D-105) already ends a move whose wheels give no counts; a step that ends too late, as with one dead encoder, would not trip a deadline either. | No deadline (raised 25 Oct, not changed) |
| G25 | The Barcode and Echo ISRs pack their records (D-85, D-90), which the team ruled out after Tribunal 1 ("no packing inside the echo interrupt"; still open in Version C). Keep it, with the interrupt times measured on the car (the measurement build, D-82), or move the packing into the tasks? (24 Oct review, R-15) | Packed in the ISRs (raised 25 Oct, not changed) |
| A11, A14 | Instructor agreement for the MQTT hooks and the pin override | implemented, switchable, marked pending |
| A12 | 1 ms tick: CPU load and jitter | to be measured |
| – | GitHub usernames for TEAM.md | to be filled in by each member |

FR5 (stop at a junction with no command) and FR16 (wait for START) are kept
exactly as written in the report.

## F. Consistency review, 13 Oct 2026

The code was checked against the W6 report (requirements, task table,
statecharts, SD1, SD2), the template rules, TEAM.md and the rules at the top
of this file. Each change is in the pull request of the code's owner.

| Found | Changed | Pull request | Entry | Tests |
|---|---|---|---|---|
| Following speed could fall below NFR17's 0.15 m/s (90 mm/s on a hump at full steering error) | speed clamped to 150–350 mm/s; compile-time checks of speeds, periods and the tick | shared | D-04, D-71 | NAV-U03, NAV-U14 |
| A barcode read started while turning could act after following resumed | the whole read is ignored | shared | D-53 | NAV-U10 |
| Hump freeze of the line levels (G7) was not a setting | `CFG_NAV_HUMP_FREEZE_LEVELS` | shared | D-23 | NAV-U14 |
| Several car- or course-dependent values were not tagged UNVALIDATED | tagged, with their basis | shared, S2–S5 | D-04 | – |
| On-target queue checks looked once and could fail at random | they poll for an empty queue | shared, S1, S2, S5 | – | NAV-U20, S1-U20, S2-U20, S5-U21 |
| FR10's position and clearances never reached MQTT | profile event on the event topic | S1, S5 | D-72 | S1-U11, S1-U08, S5-U33 |
| Encoder pull-ups were fixed in the driver | `CFG_ENC_PULL_UP` | S2 | D-04 | S2-U37 |
| Barcode capture waited for a 120 ms quiet zone, which the report does not have | capture starts at the first bar; quiet zone optional, off | S3 | D-25 | S3-U06, S3-U08 |
| An unplugged barcode sensor read dark | GP7 pulled to the white level | S3 | D-73 | S3-U35 |
| Bad IMU data in CALIBRATING did not latch a fault | latches after 5 invalid samples | S4 | D-76 | S4-U37 |
| Compass turn rate was also measured on a hump | flat ground only | S4 | D-77 | S4-U38 |
| A missed side echo while bypassing counted as clear | only valid readings count | S5 | D-74 | S5-U05 |
| A late scan release could ping less than 60 ms after the last ping | minimum gap enforced | S5 | D-75 | S5-U32 |
| Open space might read as a timed-out echo | manual test added | S5 | D-61 | S5-U34 |
| No requirement → design → code → test table (G6) | [`testing/TRACEABILITY.md`](../testing/TRACEABILITY.md) | shared | – | – |

Checked and unchanged: pins, task priorities, cores and periods (report
section 3); Safety's brake path and RESET handshake (SD2, A14); the barcode
→ junction → turn sequence (SD1); the other three approved improvements
(glitch filter, per-character wide/narrow, decoding at the 30th edge with
the 120 ms / 64-edge / 2 s failure path); the MQTT topics and back-off
(report 6.1, A11).

## G. Independent review, 14 Oct 2026

Another reviewer's list of remaining problems was checked against the W6
report, this code and its tests; each claim was reproduced on the host
where possible before anything was changed. Only the problems whose fix
needs no new team decision were changed. The others are open questions
G15–G18 in section E, unchanged in the code; G11–G14 are still open.

| Found | Changed | Pull request | Entry | Tests |
|---|---|---|---|---|
| A barcode letter could be decoded after an edge was lost inside the symbol | a loss within the first 30 edges fails the read; nothing after them spoils it; the capture is marked again after the drain that follows a queue overflow | S3 | D-78 | S3-U16, S3-U17, S3-U36 |
| A read still running when the car stopped following (line lost, junction, obstacle, fault), or started in READY, could act after following resumed | the read is ignored | shared | D-53 | NAV-U10 |
| `make` then `make SAN=1` ran the binary built without sanitizers again | host tests rebuild when `SAN`, `CC` or the flags change, and print the compiler and flags | shared | – | – |
| D-71 described NFR17 as a minimum speed | NFR17 is the range in which line following must work; the clamp limits the commanded speed only | shared | D-71 | SYS-17 now measures the real speed |
| D-73 and D-77 did not say what the pull and the held turn rate cannot show | notes added | shared | D-73, D-77 | S3-U35, S4-U38 |
| No requirement-to-evidence trail per owner | `testing/TRACEABILITY.md`: owner, evidence so far, open limits | shared | – | – |
| Tests missing for evidence the review asked for | S1-U35 (MQTT client ID), S3-U36 (chatter on GP7), INT-20 (reads around junction turns and humps); results to record (`testing/README.md`) | shared, S1, S3 | – | – |

## H. Second independent review, 16 Oct 2026

The changes of sections F and G (17 commits in six pull requests, prepared
in another team member's Claude Code session) were reviewed again in the
Claude Code session that wrote the first version, against the W6 report,
the rules at the top of this file and the code. Nothing was taken on
trust:

- The 17 patches were applied to `ee9601b` as the six branches; the merged
  tree is byte-identical to the package that was reviewed (git tree
  `9c3b41136a00`).
- Rebuilt here: the 7 firmware profiles build with 0 warnings; host tests
  pass 68/68 with gcc and with clang, each with and without ASan/UBSan.
- Reproduced here with a separate probe, before and after: a letter from a
  read that a junction interrupted was still held after the turn, so the
  car would have acted on it at the next junction (D-53). Confirmed in the
  old code: an invalid side echo was passed on as 4 m, that is clear
  (D-74); FR10's position and clearances never reached MQTT (D-72); a full
  steering error on a hump commanded 90 mm/s (D-71); bad IMU data in
  CALIBRATING restarted calibration without a fault (D-76); with
  `CFG_BC_DARK_LEVEL` 1 the GP7 pull-up made an unplugged sensor read dark
  (D-73).

| Item | Verdict | Note |
|---|---|---|
| All 17 commits | agreed, kept unchanged | Authors kept |
| D-78 (lost barcode edge) | agreed | In the case reproduced on 14 Oct (overflow flag at 29 stored edges, 30th edge still queued) the decoded letter was the true one: the dropped edge is newer than every queued edge, so it came after the symbol. The rule now gives up such a read, which is rare, to avoid decoding a symbol whose lost edges were replaced by later ones; "a wrong letter is far worse than no letter" (docs/notes/05) supports that trade. |
| S1-U20 (on-target) | agreed | Status records reach `mbf_tlm` at fixed phases, so a 10 ms poll can still miss a short empty moment. If it fails on the Pico, check that the free buffer space rises within the window instead. |
| Wording | corrected here | The rules at the top of this file were a team member's instructions to the session that wrote the code, not rules the whole team set. The last rule now says what was asked: a choice that changes a requirement or safety is asked, not made. Same in `app_config.h` and `bc_capture.h`. |
| G11–G18 | agreed that each needs a decision | G15, G16 and G17 were then put to the team member running this review, who chose the recommended option for each (D-79, D-80, D-81, implemented and host-tested here; the owners of S3, S4 and S5 still have to confirm them). G18 is the S2 owner's choice; G11–G14 are still open. |

## I. Minimum-bit data representation, from 22 Oct 2026

Change requirement 1 (Tribunal 1): "Each data field representing
application information must use only the number of bits required to
represent its valid range, using C bit-fields or explicit masks and shifts
where appropriate." It covers motion and encoder data, line and barcode
sensing, obstacle detection, hump estimation, vehicle and controller
states, data shared between tasks or between an interrupt handler and a
task, queue messages, and MQTT telemetry and commands.

The instructor's answers at the 22 Oct consultation, as the team noted
them: pack every stored field to the bits its range needs, and put spare
bits to use; provide pack and unpack functions; a yes/no status is one
bit; loop counters and other short-lived variables are out of scope,
stored application data is in scope; there are no exceptions. MQTT
messages must stay readable in an ordinary MQTT client, which does not
match the requirement's "compact binary fields rather than text"; the team
has asked on the course forum. Until that is answered, MQTT payloads stay
the JSON text they are today and only the records inside the firmware are
packed.

This change is about representation only: what the car does must not
change. Anything that would change behaviour is asked first (rules at the
top of this file). The evidence (the data representation table, memory
and message sizes before and after, the tests) is collected in
[`minimum_bit.md`](minimum_bit.md).

**D-82 Measurement before and after the change (22 Oct).** Week 13 needs
measured RAM, message sizes and timing before and after. Setting
`CFG_STATS_ENABLE` in `app_config.h` (default 0) builds
`common/app_stats.c`, which records task periods and work times, the
longest time in each GPIO handler and in the shared dispatcher, the
deepest stack use of each task and the least free space seen in each
message buffer, and prints them on the console every
`CFG_STATS_REPORT_MS` (5 s). With 0 nothing is compiled: all seven build
profiles give images byte-identical to `c644b68` (`objcopy -O binary`,
`cmp`). Stack figures: `app_start_task_stack()` reads where the kernel put
the stack with `tk_get_reg()` while the task is still DORMANT (the
kernel's debug calls are off in `config/config.h` and are not turned on),
and each task fills the unused part of its stack with a pattern when it
starts. Limits: queue space is sampled every 200 ms, so a shorter peak can
be missed; the 128 bytes just below the stack pointer at the start are
not filled, so stack use can read up to 128 bytes high; the timer has
1 µs resolution; a measurement build spends two extra timer reads per
GPIO handler, so compare a measurement build only with a measurement
build. How to record it: `testing/README.md`, which also lists what each
figure includes and leaves out (wall-clock times, not CPU time;
Navigation's work leaves out its event handling).

**D-83 How fields are packed (22 Oct).** `common/bitpack.h` packs
records into `uint32_t` words with masks and shifts; bit 0 is the least
significant bit. C bit-fields are not used for shared or queued data,
because the compiler chooses their layout (Barr-C 5.5), and no bitwise
operator touches a signed value (Barr-C 5.3): a signed field is two's
complement in its own width, converted with arithmetic only. A value
outside its field is never cut: the nearest value that fits is stored and
the pack function reports it. Each record has a pack and an unpack
function next to its type, with fixed expected words in its host test.
Counters and time stamps that only ever need differences wrap in their
width (`bitpack_sub`), which is exact while the true difference stays
below 2^width. Message buffers are sized in messages with
`common/app_mbf.h` (the kernel stores a 4-byte header plus the message
rounded up to 4 bytes), and every buffer keeps the number of messages it
held before the change, so a burst overflows exactly when it did before:
for example the line events buffer held 21 events (16 was asked for, with
8 bytes of margin per message).

**D-84 Line events and line record (IF-01, IF-02), 22 Oct.** A line
event (`line_evt_t`) is 16 bits instead of 8 bytes: type 3 bits (1..6),
letter 5 bits (0 none, 1..26 for A..Z), reversed 1 bit, 7 bits spare.
The line record (`line_err_t`) is two words instead of 24 bytes: error
11 bits (−1000..1000), line seen and valid 1 bit each, sample count 8
bits; left and right readings 12 bits each (0..4095). Fields removed,
because nothing reads them: the time stamp of both; the event's failure
reason (Navigation does the same for every failed capture: it releases
the speed hold and keeps any held command); the record's lost flag
(Navigation learns of a lost line from the LOST and FOUND events); and
the record's two darkness values (never read outside the Line task,
which keeps them). The readings and the sample count stay although
Navigation does not read them, because the on-target tests do (S3-U21
the readings, S3-U20 the count). The sample count wraps at 256: it is
only used to count the samples between two reads at most 1 s apart
(S3-U20: 190–210). Navigation reads both through the functions in
`line_msgs.h`, and the record is still copied whole under the mutex.
The line events buffer keeps its 21 events (D-83). Static RAM −16 bytes,
line events buffer 256 → 168 bytes. Tests S3-U40, S3-U41; all Navigation
host tests now run on the packed formats. The failure reason and the
lost flag were removed after the team's review of 22 Oct; showing the
reason anywhere, for example on MQTT, would be new behaviour and needs a
team decision.

**D-85 Barcode edges and the capture, 22 Oct.** The Barcode ISR's edge
record is one word instead of 8 bytes: the time in 31 bits (µs modulo
2^31, about 35.8 minutes) and the level in 1 bit. The Line task rebuilds
the exact 32-bit time from its own clock, read after it takes the edge
from the queue. An edge normally waits less than one 5 ms cycle (the
Line task can be held up longer by higher-priority tasks and interrupt
handlers); the time is rebuilt exactly for any wait under 35.8 minutes.
The ISR packs it in six instructions and copies 4 bytes into the queue
instead of 8. The edge buffer keeps its 85 edges (D-83): 1024 → 680
bytes.

The capture state (`bc_capture_t`, on the Line task's stack) is 192
bytes instead of 284: one word of flags and counts (state, level, four
flags, failure reason 4 bits, edge count 7 bits, quiet zone 8 bits in
ms), three 32-bit times (the first bar, the start of the current white,
the white before the capture), and the other 63 edges as 22-bit offsets
from the first bar. The three times keep 32 bits because they are
compared with the free-running 32-bit clock: a narrower copy would wrap
and could make a long white look short. 22 bits: a capture ends at the
first poll after 2 s, and once the watchdog is armed (radio up, or 15 s
after boot) the Line task can be late by at most its 50 ms check-in
deadline plus the 200 ms watchdog time before the chip is reset, 2.25 s
in all, which needs 22 bits (up to 4.19 s). The only difference from
before: an edge more than 4.19 s after the first bar cannot be stored
and ends the capture as too long, where the old code stored it. That
needs the Line task held up for over 2 s, which the armed watchdog does
not allow. Before the watchdog is armed (or with `CFG_WDT_ENABLE` 0)
nothing bounds a hold-up: if the Line task were held up for more than
about 2.2 s during a barcode, the new code would end the capture as too
long; the old code could only have read a letter from that capture if
the barcode's first 30 edges took more than 4.19 s to cross.

Tests: S3-U42 (edge record), S3-U43 (the old capture, kept as a
test-only copy in `testing/host/ref/`, and the packed one side by side
over 4000 random runs: the same output at every poll), S3-U44 (layout,
the 4.19 s limit, and a slow letter whose last edges are stored past
2^21 µs, read back exactly), S3-U49 (side by side again, with the Line
task held up for up to 0.27 s near the 2 s limit, so edges are stored
up to 2.2 s after the first bar: bit 21 of the offset is written and
read back). S3-U43 also showed that `bc_decode()` never reports
`BC_FAIL_PATTERN` while characters are classified one by one
(`CFG_BC_CLASSIFY_PER_CHAR` 1): a character only classifies with 2 wide
bars and 1 wide space, and all 40 such patterns are Code 39 characters.
That is harmless and is not changed here.

**D-86 Line tracking, steering and letter map state, 22 Oct.** Line
tracking (`line_track_t`, on the Line task's stack) is three words
instead of 48 bytes: the four levels 12 bits each (a level only moves
towards a 12-bit reading or back to its stored value, so it stays within
0..4095), the error 11 bits, the rearm run 3 bits (0..4), the failed-read
count 3 bits (0..4) and three flags. The dark run (2 bits) and the white
time (9 bits) stop at the limit they are compared with (3 samples,
300 ms) instead of counting on, which cannot change a result: before,
they only ever met `>= limit`. The two darkness values were written and
never read, and are gone. Readings passed in must be 12-bit, as the ADC
gives them. Steering (`steering_t`, in Navigation's state) is 16 bits
instead of 8 bytes: the previous error 11 bits (it is clamped to
−1000..1000 before it is kept) and a flag. The letter map
(`letter_map_t`, in Navigation's state) is 26 actions of 3 bits in three
words instead of 26 bytes. Navigation's state goes from 448 to 432 bytes
of static RAM. Tests S3-U45 to S3-U47 run each packed module against the
old code (kept in `testing/host/ref/`) on random input: 1.2 million line
samples, 200,000 steering steps, 100,000 remaps and reads, all
identical.

**D-87 What the Line task keeps between cycles, 22 Oct.** The Line task
kept eight values for its whole life (about 24 bytes on its stack):
the last two readings, a cycle count, the counts of letters read and of
failed barcode captures, the last letter, and two flags (all readings in
range since the last status; the ISR dropped an edge since the queue was
emptied). They are stored data, not loop temporaries, so they are
packed into two words (`line_keep.h`): readings 12 bits each, the cycle
count 6 bits modulo 40 (it only decides when the 200 ms status is due),
the letter count 8 bits (it was only ever reported modulo 256), the
last letter 5 bits and two flags. That is seven values: the
failed-capture count is gone (D-92), because it only fed a telemetry
field that was never printed. Test S3-U48 compares them with the old
arithmetic over 20,000 random cycles.

**D-88 Motion's messages and the encoder counters, 22 Oct.** A Motion
command (IF-07) is one word instead of 12 bytes. Its fields depend on
its type, since no command uses all of them: the type in 3 bits; a
drive's speed and difference 11 bits each (−1000..1000, the limit
`motion_send()` already applies); a move's or turn's ID 8 bits and the
size of its distance or angle 15 bits (0..32767: the type gives the
direction, and Motion has only ever used the size); hold's flag 1 bit.
Navigation's largest value is a U-turn, 18000 cdeg (its longest move is
the sideways leg round an obstacle, at most 950 mm), which needs 15
bits, and a build-time check in `nav_sm.c` stops the build if any
distance or angle Navigation sends would not fit, for example a sweep
setting above 16383 cdeg, since the way back is twice the sweep.
Motion's API refuses a distance or angle whose size does not fit
(`E_PAR`, nothing queued) instead of sending it shortened, and a
negative value is its size, as Motion always took it (external review
of 23 Oct, section J). A field a command does not carry reads 0, as
before.
Motion's command queue keeps the 20 commands it really held (D-83):
320 → 160 bytes. A Motion status (IF-08) is two words instead of
28 bytes, with only what Navigation reads: the type, the heading
(16 bits, (−18000, 18000]) or the finished move's ID, and the distance
since boot. A run has no set length (the track runs endlessly), so the
distance's range is what the 32-bit counters can give: half of each,
added, times π × 65 / 528 mm per count, which stays below 2^30 mm in
size, so 31 bits (`MOTION_DISTANCE_BITS`; a build-time check fails if
new wheel or encoder settings make one count worth half a millimetre or
more). The first commit of this change kept 32 bits; corrected the same
day. The counts, speeds, state and time stamp were never read from IF-08
(Telemetry gets them in Motion's status record) and are gone.
Navigation's buffer keeps its 18 messages: 576 → 216 bytes.

The encoder counters follow the team's Tribunal 1 design ("Version B"):
the shared pair is one word holding two 16-bit counters that wrap. Each
Encoder ISR changes only its own half with a 16-bit load and store
(checked in the compiled code, `arm-none-eabi-gcc` 13.2.1 with the
firmware's flags `-O2 -mcpu=cortex-m0plus -mthumb`: `ldrh`, `adds`,
`uxth`, `strh`; a reviewer found the same with gcc 13.3.1 and clang 18,
in both decode modes); Motion reads both halves with one 32-bit load,
so the pair is consistent without masking interrupts (the old code
masked them), and adds each wheel's signed 16-bit change to its own
32-bit totals (`encoder_add_counts()`, through `encoder_add_pair()`,
inline in `drv_encoder.h` so that S2-U42 runs the same code). The
change is exact below 32768 counts between reads. Motion reads every
20 ms (about 100 counts at 2 m/s); if it stops, its 100 ms check-in and
the 200 ms watchdog reset the chip within about 0.3 s (about 1,600
counts at 2 m/s), once the watchdog is armed (radio up, or 15 s after
boot). Before that, 32768 counts would need Motion stopped for 6.3 s at
2 m/s. The shared data goes from 8 to 4 bytes; static RAM is unchanged,
because Motion keeps a copy of the last pair. Tests S2-U40 to S2-U42
and COM-U12. After the team's review of 22 Oct the move value went from
16 signed bits to the 15-bit size with the build-time check, and S2-U42
runs the driver's own add instead of a copy of it.

**D-89 IMU events and fault reports, 22 Oct.** An IMU event (IF-03) is
one word instead of 32 bytes, with what Navigation reads, chosen by the
type: the type in 2 bits (1..4, kept as type − 1); a heading 16 bits
((−18000, 18000]) and the level-ground flag; a hump's peak |roll|
15 bits (0..18000) and height 7 bits (the calibration table's largest
entry is 65 mm; S4-U40 checks every height the table can give); a motion
class 3 bits. The turn rate, roll, pitch and time stamp were never read
from IF-03 (Telemetry gets them in the IMU status record) and are gone.
Navigation's buffer keeps its 17 events: 640 → 136 bytes. A fault report
(IF-04) is one word instead of 8 bytes: cause 3 bits, source 3 bits
(1..8, kept as source − 1), detail 13 bits (the largest is a collision's
|a|, under 6929 mg); the time stamp was never read. Safety's queue keeps
its 21 reports: 256 → 168 bytes. Tests S4-U40, S4-U41.

**D-90 Obstacle events, scan requests and echoes, 22 Oct.** An obstacle
event (IF-05) is two words instead of 24 bytes: type 3 bits (1..6), and
only a plan carries the profile, with what Navigation reads: side 2
bits, closest range 9 bits, width 11 bits, each edge's lateral position
10 bits. A plan's points are all hits, within `CFG_SCAN_OBSTACLE_MM`
(500 mm) of the sensor, so the widths follow from that setting; S5-U40
runs the profile code on 20,000 random scans and checks that every plan
it makes fits. The forward distance, centre and clearances (sent to
Telemetry in their own record, D-72), the time stamp and a give-up's
reason were never read from IF-05 and are not sent: on NO SAFE ROUTE
Navigation holds the car the same way for every reason (the Scan task
only ever gave two: three invalid readings in a row, and still no room
after reversing). Navigation's buffer keeps its 9 events: 256 → 108
bytes. A scan request (IF-06) is one byte instead of 8: type 4 bits, side
2 bits (kept only for a bypass, the only request that has one); its
queue keeps its 10 requests: 128 → 80 bytes. The ultrasonic echo's
width is 15 bits (`ultrasonic_echo.h`, packed by the Echo ISR): every
width from 23200 µs (`CFG_US_MAX_MM`) up reads as 4 m, so a longer
width is kept at 32767 µs and no range changes (S5-U42 checks every
width up to 200 ms through the ISR's own packing). Its rise time keeps
32 bits: Scan compares it with the trigger time to drop echoes that rose
before the ping, and a sensor stuck high can make such an echo
arbitrarily old. The echo message stays 8 bytes (two words: the rise
time needs one of its own); its queue keeps its 10 echoes. A reading is
kept at the servo angle asked for, so a build check makes sure every
coarse angle is one the servo can reach (the telemetry profile stores
its forward distance unsigned, D-92). The give-up reason was removed,
the echo width narrowed from 16 bits and the coarse-angle check
completed after the team's review of 22 Oct; showing the give-up reason
anywhere, for example in the SAFE HOLD route entry, would be new
behaviour and needs a team decision.

**D-91 Commands to Navigation and the broker record (IF-10, IF-15),
22 Oct.** A Navigation command (IF-10) uses 39 bits: the type 1 bit
(kept as type − 1), the letter 5 bits (0 none, 1..26 for A..Z), the
action 3 bits (0..4) and the command's `seq` 30 bits. The `seq` is
whatever number the sender wrote, echoed in the ack; `cmd_parse()` reads
at most 9 digits and ignores a negative one, so 0..999,999,999
(`CMD_SEQ_MAX`). 39 bits need two words, so the record stays 8 bytes,
with 25 bits spare. A default-map command carries no letter or action
(Navigation never read them). The Command task only sends what
`cmd_parse()` accepted, so its commands always pack exactly; anything
else is stored as a remap of no letter, which Navigation refuses as a
remap without touching the map (S1-U41, NAV-U12). Before, a command of
an invalid type was answered "unknown"; the Command task never sends
one, so Navigation's branch for it can no longer be reached (kept).

The broker record (IF-15) is two words instead of 18 bytes: each of
the address's four parts in 9 bits, as the code of its spelling, and the
16-bit port. `cmd_ip_valid()` accepts a part of 1 to 3 digits up to 255,
which has 366 spellings (10 of one digit, 100 of two, 256 of three), so
9 bits keep the exact text, leading zeros included. The spelling is
kept, not only the 32-bit address, because the MQTT code hands the text
to lwIP's `ip4addr_aton()`, which reads a part with a leading zero as
octal ("010" is 8): a plain address would change which broker such text
means. The broker queue holds its 2 records exactly: 52 → 24 bytes. The
Command task's static state is 16 bytes smaller (40 → 24). The publish
and command-text queues carry MQTT text, which this change does not
touch, and keep their bytes; Telemetry's queue changes with its
records (D-92). Tests S1-U40 (every spelling of a part in every
position, and 10,000 random broker commands, come back character for
character) and S1-U41 (26,000 parsed commands read back exactly as the
old record carried them). The first version of this change kept the
text as 4-bit characters in three words; it became 9 bits per part after
the team's review of 22 Oct.

**D-92 Telemetry records (IF-12), 22 Oct.** Every task sends Telemetry
records: status every 200 ms; route-log entries, acks, faults, link
changes, obstacle profiles and summary requests when they happen. The
message is now five words instead of 28 bytes: one word of type (4 bits
used), then the record, which keeps only what Telemetry prints:

- Line status, 48 bits: the readings 12 bits each, the error 11, the
  letter count 8 (modulo 256, as before), the last letter 5.
- Motion status, 127 bits: the wheel speeds 16 bits each, the encoder
  counts since boot 32 each and the distance 31 (D-88). This is the
  largest record, which is why the message needs five words. The speeds'
  range is set by Motion's clamp to the old 16-bit field, kept so that
  any measured value, encoder noise included, is reported as before; the
  motor's rated top speed (about 1.2 m/s) would fit 12 bits, but a
  clamp at ±2047 mm/s changes the MQTT text for a faster reading, so it
  needs a team decision (the turn rate below is the same case).
- IMU status, 34 bits: the turn rate 16 (cut to 16 bits as before), the
  class 3, the hump count 8, the highest hump 7 (the height table's
  largest entry is 65 mm).
- Scan status, 28 bits: the last range 12 (0..4000), the obstacle
  count 16. Navigation status, 11 bits: the mode 4, the held letter 5,
  the SAFE HOLD reason 2. Safety status, 45 bits: two flags, the cause
  3, the queue-full count 8, the fault and miss counts 16 each.
- A route-log entry, three words: the distance 31 bits, the kind 4,
  then what that kind's message prints: a barcode's letter, action and
  direction (9 bits), a turn's action and two angles (35), a hump's
  height and peak roll (22), an obstacle's side, width and closest
  point (22), a search's start or end (1), a SAFE HOLD reason (2), the
  run time (32).
- An ack, 34 bits: seq 30, code 3, ok 1. A fault, 20 bits: cause 3,
  source 3, detail 13, latched 1. A link change, 4 bits: online, and
  the CYW43 link state (−3..3) in 3 bits. An obstacle profile, 66 bits:
  side 2, reversed 1, forward distance 9 (0..500: every point is a hit
  within 500 mm at a servo angle of 0..180°, every scan angle checked
  when building, D-90), the lateral centre 10, the closest point 9, the
  width 11, each clearance 12 (stored plus one, so 0 is "never seen",
  −1).

The time stamp keeps 32 bits, since it is printed as it is, and only
the records whose time Telemetry prints carry it. Fields nothing ever
printed are gone: the Line status's lost flag and failed-capture count
(so the Line task no longer counts failed barcode captures, D-87; failed
line-sensor reads are still counted, D-79), Motion's state, the
IMU's heading and roll, Scan's state, invalid-reading count and servo
angle, Navigation's held action, heading and run time, an ack's detail,
a route entry's reason (and Telemetry's copy of it), and the time stamp
of the five status records.

Telemetry keeps the status records and the route log as they arrive,
packed: its state is 868 bytes instead of 1088 (the route log
960 → 768). Its state also kept a fault counter that nothing wrote or
read (the summary's faults come from the Safety record); it is gone,
which the struct's alignment leaves at 868 bytes. Its queue keeps the
27 records it really held (24 were asked for, D-83): 864 → 648 bytes.
Static RAM −220 bytes in the default build; the pack and read functions
add about 4 KB of code.

The MQTT messages do not change. S1-U43 runs the old records and
formatter (a frozen copy in `testing/host/ref/`) beside the new ones on
20,000 random states and 20,000 random events of every kind, with
random values in the dropped fields, and every message is
byte-identical. S1-U42 checks the exact bits of every record and S1-U44
that every obstacle profile the scan code makes fits. A value too big
for its field is stored as the largest value that fits and reported,
as everywhere in this change; no task sends one, because every value
comes from a field at most as wide or a range checked above. S1-U08's
longest message is now 249 bytes instead of 259, because no field can
hold a value outside its range any more. Telemetry's run totals (for
the summary) are its own stored values, not a record, and are left to
a later step.

## J. External review of the minimum-bit change, 23 Oct 2026

An outside review of `main` at `1d75e59` was checked against this code
before anything was changed; nothing was taken on trust. Every fact it
states that was checked here holds. It raises no packing fault on valid
input. Where it asks for new behaviour, the change waited for a decision
(section E). The changes are in `5cddd0a`, `7bcbf91` and `b1d23ec`; G19
and G20 were decided the same day and changed in `551329e` and `dd3a336`.

| Found | Verdict | Changed | Entry | Tests |
|---|---|---|---|---|
| A valid line record is used even when it stopped changing (B3-01) | agreed, confirmed in the code; it was so before the change | decided 23 Oct (G19): after 20 ms without a new sample while running, the record is not used and a LINE_DATA fault is reported | G19, D-94 | NAV-U40 |
| Edges found later in an interrupt got the entry time stamp (B3-02) | agreed | each status read has its own time stamp | D-93 | S3-U34 (car) |
| S3-U20 printed "every 5 ms +/- 1 ms" but only counts samples (B3-03) | agreed | S3-U20 counts over a measured window, prints it, and says it shows the average rate; each interval is S3-U33, whose probe now toggles only at the 5 ms releases | – | S3-U20, S3-U33 |
| A fault during a pending RESET does not stop the reset (S4-01) | agreed, confirmed in the code; it was so before the change | decided 23 Oct (G20): the fault cancels the reset and becomes the latched cause | G20, D-95 | S4-U07 |
| Motion's API sent an oversize distance shortened and returned `E_OK` (S2-01) | agreed: a result of D-88 | refused with `E_PAR`, nothing queued; a negative value is still its size | D-88 | S2-U40 |
| MQTT is still JSON (REQ-01) | agreed that it is open | not changed: waiting for the forum answer | G22 | – |
| No accumulated fault mask (REQ-02) | agreed that it is not done | not changed: it changes Safety's reports and the MQTT text | G21 | – |
| Most subsystems' own stored state is not packed (REQ-03) | agreed | not changed here: each owner's (`docs/minimum_bit.md` section 6); Buddy 3's is done | – | – |
| No on-car evidence; what the measurement figures include was not written down (VAL-01) | agreed; the car is needed | `testing/README.md` lists what each figure includes and leaves out; the statistics' cross-core read is recorded in TEAM.md section 7. Navigation's work window is not moved: the before and after builds must measure the same way | D-82 | – |

Checked here, where the review could not: the host tests pass 100/100
with LeakSanitizer on as well as ASan and UBSan (a deliberate leak was
caught, so leak checking works here), and the firmware builds it could
not run were built: seven profiles, 0 warnings.

## K. Second outside review, 23 Oct 2026

An outside review of `main` at `207f498` was checked against this code
the same way, nothing taken on trust. It confirms the fixes of section
J (D-88, D-93, D-95, S3-U20's wording, G19's check) and finds two more
points. Both were confirmed here and are fixed in `6c5011b`, `4e0a9e7`
and `049504b`.

| Found | Verdict | Changed | Entry | Tests |
|---|---|---|---|---|
| Line reads failing for good can survive a RESET: for up to 200 ms the published line health still says healthy, so SELF-TEST can pass and START is allowed with no good read; D-79 has reported its one fault already and G19's check sees the count still moving (R2-B3-01) | agreed, reproduced here: with the old health rule NAV-U41 reaches READY while every read fails. It was so since D-79, and needs RESET within the same 200 ms status window as the failures | line health drops at the first bad sample and comes back only after a whole good window; also found here: the failed-read count starts again while the Line task is idle after COMPLETE | D-96 | S3-U19, NAV-U41 |
| G19's check counted Navigation ticks, so its 20 ms held only while Navigation ran on time: a late tick counted as one, and the fault could come 40 ms after the count stopped (R2-B3-02) | agreed | measured as elapsed time: reported at the first tick 20 ms or more after the count last moved | D-94 | NAV-U40 |
| MQTT still JSON; no accumulated fault mask; most subsystems' stored state not packed; no evidence from the car | agreed that they are open | not changed here | G21, G22; `docs/minimum_bit.md` section 6 | – |

Checked here, where the review could not: LeakSanitizer runs (the host
tests pass 104/104 with it, ASan and UBSan, with gcc and clang), and
the firmware builds: nine builds, 0 warnings.

## L. Third outside review, 23 Oct 2026

An outside review of `main` at `342f4a5` (before the servo change,
D-97) was checked against this code the same way. It confirms the fixes
of section K and the earlier ones, finds no new high-severity problem,
and finds one more gap, confirmed here and fixed in `ae83300`.

| Found | Verdict | Changed | Entry | Tests |
|---|---|---|---|---|
| After COMPLETE, a fault and RESET, SELF-TEST can reach READY with the Line task still idle and no new reading: its idle cycles still count as good, and nothing wakes it before START; D-96's count restart then stops a failing sensor only after movement was allowed (R3-B3-01) | agreed, confirmed in the code; D-96 had found the idle case and fixed only the count | idle cycles drop the line health; the restart into SELF-TEST wakes the Line task, so SELF-TEST waits for a whole window of new good reads | D-98 | NAV-U42 |
| Binary MQTT, the fault mask, the stored state of most subsystems, evidence from the car | agreed that they are open | not changed here | G21, G22; `docs/minimum_bit.md` section 6 | – |

Checked here, where the review could not: LeakSanitizer runs (105/105
with it, ASan and UBSan, gcc and clang), and the nine firmware builds,
0 warnings.

## M. A team member's review, 24 Oct 2026

A team member reviewed `main` at `0ff78e1` (24 findings, R-01 to R-24,
with models of some faults). Each finding was checked against the code
here, nothing taken on trust; where the review's model was the evidence,
it was rebuilt on the real code. Fixed in `0e5ecec`, `87476ab`,
`0bb3580`, `06ad014`, `2fee4b3`, `4d43ac3`, `66be08e`, `4391724`,
`50f7f39` and `b7e9553`. The fault mask (R-02) and the SELF-TEST change
(R-07) follow what the team agreed after Tribunal 1 (Version C).

| Found | Verdict | Changed | Entry | Tests |
|---|---|---|---|---|
| R-01 Nothing detects a stall, a lack of progress or an encoder fault, and Navigation's keepalives keep a counted move or turn going for ever: one dead encoder makes a 90° turn about 260° in the review's model, two make the car spin | agreed, confirmed in the code | Motion's stall and encoder-fault watch reports MOTOR_STALL or ENCODER, which Safety latches; 300 ‰ for 1 s, UNVALIDATED, chosen so that a hump the PID can climb never trips it. Navigation deadlines not added: a move whose wheels give no counts now ends in a latched fault, and the one-dead-encoder turn does end, too late, so a deadline would not trip; raised as G24 | D-105, G24 | S2-U07, S2-U38 |
| R-02 The change requirement's fault mask is not built: one 3-bit cause, only the first kept; no stall, encoder, no-echo or Wi-Fi-loss bits | agreed | built as the team agreed: 11 causes in 4 bits, a latch flag each, Safety the only writer, Wi-Fi loss and no echo as status bits that never brake, the mask in Safety's status record and the heartbeat | D-102 to D-104, G21 | S4-U07, S4-U08, S4-U41, S1-U42, S1-U43, S1-U34, S5-U36 |
| R-03 No test evidence from the car in the repository: every `logs/` folder is empty | agreed that it is missing | nothing to change in the code: the logs come from the car, and each TEST_LOG says where they go | – | – |
| R-04 A STOP is dropped when the command input queue is full | agreed | the MQTT hook recognises a STOP and reports it to Safety directly | D-101 | S1-U12 |
| R-05 Speeds and the line-lost timer use the nominal periods; a missed release doubles the measured speed | agreed | the measured time since the previous read in both | D-99 | S2-U03 |
| R-06 The start-up duty against the feed-forward: forced overspeed below 200 mm/s and reverse-drive pulses | agreed for the code; the two settings still need measuring | the drive is never against the target, the start-up duty only from standstill | D-100 | S2-U06 |
| R-07 The 40 ms echo wait leaves 1–2 ms over a 38 ms no-target pulse; the agreed SELF-TEST change is not built | agreed | wait 50 ms; SELF-TEST needs only that the sensor answers. A module whose no-target pulse is longer than 50 ms still reads open space as invalid in a run, until measured (S5-U34) | D-61, D-104 | S5-U20, S5-U34, INT-17 |
| R-08 The 120 ms end of capture sets a lowest crossing speed: about 80 mm/s for the 141 mm print; a larger print fails at 150 mm/s | agreed as a limit; not changed | Navigation never commands below 150 mm/s, so the course's print decodes at every commanded speed, and a capture that ends early fails safe (no wrong letter). Ending relative to the widest element changes decoding, which S3-U43 and S3-U49 hold equal to the old code; it needs the print size used on the course and SYS-02 and INT-20 beside a hump first | – | SYS-02, INT-20 |
| R-09 The obstacle width comes out too wide by the beam's spread; servo angles unmeasured | agreed as a limit; not changed | a correction needs the beam measured with boxes of known width at two or three distances; a guessed one could make the width worse; the error gives more clearance | D-97 | S5-U30, S5-U33, SYS-07 |
| R-10 The hump height uses roll only on a 130 mm lever; curves can raise false humps | agreed as a limit; not changed | the table is a placeholder until calibrated on the car (A4); roll on the course's curves must be logged before choosing a filter | – | S4-U32, SYS-06 |
| R-11 The 20 ms stale-record fault is not measured under Wi-Fi load | agreed that it is unmeasured; not changed | the Line release jitter with MQTT running and a forced Wi-Fi drop sets `CFG_NAV_LINE_STALE_MS` | D-94 | S3-U33 |
| R-12 MQTT is JSON, not binary; the template's receive path turns every byte outside 0x20–0x7E into "." | agreed, the template point confirmed in `mqtt_utk.c` | waiting for the course forum (G22); binary commands would also need that template change (A11) | G22 | – |
| R-13 Most stored state is not packed | agreed that it is open | each owner packs theirs or records why it is exempt (`docs/minimum_bit.md` section 6); the state added here is packed (the stall watch, one word; the mask, 11 bits) | – | – |
| R-14 Motion's `ctx.state` is read before it is set (Barr-C 7.2.a) | agreed | the context starts zeroed | – | – |
| R-15 The Barcode and Echo ISRs pack their records, against the team's own decision | agreed that code and documents disagree; not changed | the team has not decided between keeping it, with the interrupt time measured, and moving it into the tasks; raised as G25 | G25 | – |
| R-16 Safety's queue-full count wraps after 255 | agreed | it stops at 255, like the other counts | – | – |
| R-17 X1 decoding counts a wheel rocking on an A edge | agreed as a limit; not changed | X1 was chosen for the interrupt rate (D-28); X4 is one setting once the encoder interrupt load is measured (A8) | D-28 | S2-U30, S2-U37 |
| R-18 One count per 20 ms is 19.3 mm/s | agreed as a limit; not changed | set by the counts per turn, still to measure (A2); a longer window or edge timing changes the speed loop and is Buddy 2's once the PID is tuned | – | S2-U30 |
| R-19 The rejoin drive is fixed at 300 mm while the sideways leg can be 950 mm | not a defect | 300 mm is report 6.5's "rejoin within 30 cm"; a car further out goes to SAFE HOLD as the report says. Deriving it from the leg would change the report's behaviour | – | NAV-U15 |
| R-20 Letters E–Z have no action by default, so a junction after one is SAFE HOLD | not a defect | the report's default map (A–D) and FR5; the demo-day remap must cover every letter on the course | D-54 | NAV-U06 |
| R-21 The compass affects only the logged turn angle | agreed; as designed | turns are counted on the encoders; the logged angle is an estimate until the compass is calibrated on the car | – | S4-U31 |
| R-22 No obstacle reaction while turning at a junction or searching | agreed that it is so; not changed | acting in more modes changes the report's statecharts; raised as G23 | G23 | – |
| R-23 A failed `_init()` or `_start()` is only printed | agreed | nothing starts if an object is missing, and Navigation does not start if a task is missing | D-106 | – |
| R-24 Dropped commands and dropped telemetry share one counter | agreed; not changed | a STOP no longer depends on it (D-101); a second counter would change the heartbeat for little gain | – | – |

The review's own checks (section 6 of its report: registers, the
LSM303DLHC driver, the watchdog, the PWM plan and others) agree with
this code. Checked here: the host tests pass 109/109 with gcc and
clang, with ASan, UBSan and LeakSanitizer; every new commit builds
with 0 warnings and passes the host tests; the nine firmware builds of
the last commit, 0 warnings.
