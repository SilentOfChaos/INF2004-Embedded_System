# W6 design review (as submitted), condensed

Source: `INF2004_Design Review_IS19.pdf` and Fig 1 to Fig 10. This is a
summary for quick recall before Tribunal 1; the PDF is the reference.

## 1. Requirements

### Functional

| ID | Short form |
|---|---|
| FR1 | Follow the 18 mm black line start to finish, no operator input |
| FR2 | Decode Code 39 letters A–Z in either direction of travel |
| FR3 | Act on a decoded command at the next cross junction, not at the barcode |
| FR4 | At a junction: straight / left / right / U-turn, final heading within ±5° |
| FR5 | No command held at a junction: stop and report "no command read" |
| FR6 | Default map A straight, B left, C right, D U-turn; remappable over MQTT without reflashing |
| FR7 | Keep following the line while one wheel crosses a hump |
| FR8 | Detect each hump, estimate peak height ±5 mm, report hump count and highest peak |
| FR9 | Detect an obstacle on or near the line and stop at least 10 cm before it |
| FR10 | Coarse then fine scan; report position, width, closest point, left and right clearance, each ±2 cm |
| FR11 | Bypass on the side with more clearance, passing 5–10 cm away, rejoin beyond it; if no room, reverse 10 cm, re-scan once, then stop |
| FR12 | Telemetry: state, wheel speeds, encoder counts, line sensors, barcodes, hump data, obstacle data, distance |
| FR13 | Timestamped route log (distances, turns, where each barcode, hump and obstacle was met) |
| FR14 | MQTT commands: remap letters, request run summary, stop |
| FR15 | Collision or STOP: brake and hold a fault state until reset |
| FR16 | Stay still after power-on until START is pressed |
| FR17 | Classify motion (stationary, accelerating, turning, climbing, descending, sudden impact); report events and turn rate |
| FR18 | Unexpected line loss while following or on a hump: search 5 ± 0.5 s, then stop, end the run, publish a summary |
| FR19 | Commanded distance ±5%, commanded angle (45°, 90°) ±5°, via encoders |

### Non-functional

| ID | Short form |
|---|---|
| NFR1 | Line sensors sampled every 5 ± 1 ms |
| NFR2 | Barcode edges time-stamped at 1 µs resolution or better |
| NFR3 | Wheel-speed loop every 20 ± 2 ms |
| NFR4 | Accelerometer 100 ± 5 Hz, magnetometer 75 ± 5 Hz |
| NFR5 | Ultrasonic pings at least 60 ms apart |
| NFR6 | Motor drive removed within 50 ms of a confirmed fault or STOP |
| NFR7 | Course completed in under 5 minutes |
| NFR8 | Barcode remap effective within 1 s |
| NFR9 | Telemetry 5 ± 0.5 Hz, heartbeat every 1 ± 0.1 s |
| NFR10 | Wi-Fi/MQTT loss must not stop the robot or change its mode |
| NFR11 | Nothing above 3.3 V on any GPIO |
| NFR12 | Full course on the 5 V / 2.1 A power bank without resetting |
| NFR13 | Steady-state CPU load ≤ 60% |
| NFR14 | Broker address changeable without reflashing |
| NFR15 | C + Pico SDK + µT-Kernel + template repo and TEAM.md rules (exceptions recorded), Barr-C, no printf in ISRs, no unjustified globals |
| NFR16 | Safety stop latched until reset, not undone by motor-control updates; motor commands zeroed before release |
| NFR17 | Line following from 0.15 to 0.35 m/s, 200 to 1000 lux, across tile joins |
| NFR18 | Straight driving: each wheel within ±10% of target, drift ≤ 2 cm per metre |
| NFR19 | Total RAM (stacks + Wi-Fi stack) below 75% of 264 KB |

## 2. Pins

See [03_hardware.md](03_hardware.md#our-pin-map-design-review-2-checked-against-the-board).

## 3. RTOS tasks

µT-Kernel priority 1 is highest; 1–4 are kernel services (USB console 3,
Wi-Fi `cyw43_utk_task` 4). The team proposes a 1 ms kernel tick (template
default 10 ms, `CNF_TIMER_PERIOD`), periodic tasks released by cyclic handlers,
dual-core build (`SMP=1`).

| # | Task | Period | Prio | Core | Owner | Job |
|---|---|---|---|---|---|---|
| 1 | Safety | 10 ms + fault events | 5 | 0 | B4 | Latch faults, hold motor pins low via output override, watchdog fed only when Line, Nav, Motion, IMU and Scan have checked in; RESET button GP21; buzzer, LED |
| 2 | Line | 5 ms | 6 | 0 | B3 | Sample line ADCs, line error, junctions, barcode decode from ISR edge times |
| 3 | Navigation | 5 ms + events | 7 | 0 | Shared (B3 owns steering and letter map) | Mission states, steering, holds barcode command until a junction, line search, the only task commanding Motion; START button GP20 |
| 4 | IMU | 10 ms | 8 | 1 | B4 | Humps and heights, motion class, turn rate, collision |
| 5 | Motion | 20 ms | 9 | 0 | B2 | Wheel PID, moves and turns, odometry every 20 ms, move-done by ID |
| 6 | Obstacle Scan | 65 ms | 10 | 1 | B5 | Ping, coarse/fine scan, profile |
| 7 | Command | on message, link check 100 ms | 11 | 1 | B1 | Route MQTT commands (remap, summary, STOP) |
| 8 | Telemetry | 200 ms | 12 | 1 | B1 | Telemetry, heartbeat, route log into `mbf_publish` |

Core 0: control path (Safety, Line, Navigation, Motion) and all GPIO
interrupts. Core 1: IMU, Scan, Command, Telemetry. Cross-core data only through
kernel objects. Code lives in `app_program/subsystemN/` with interfaces in
`subsystemN.h` and `TEAM.md`.

## 4. Interactions (Fig 2) and sequence diagrams

- Line, IMU and Scan send events to Navigation through fixed-size queues
  (`line_evt_t`, `imu_evt_t`, `obstacle_t`). A full queue is counted in
  telemetry and treated as a fault.
- Line error is not queued: a latest-value record under a mutex, read by
  Navigation every cycle.
- Navigation sends `motion_cmd_t` to Motion; Motion returns odometry every
  20 ms and move-done events; Navigation fuses odometry with the
  tilt-compensated compass heading.
- Collision (`fault_evt`) and STOP go straight to Safety. Safety forces the motor
  pins low with a GPIO output override (Motion owns PWM slices 4 and 5 and the
  pins; this is a proposed TEAM.md exception, A14). On RESET, Safety asks
  Motion to clear queue, command and PID state and releases the override only
  after Motion acknowledges.
- No application task calls cyw43, lwIP or MQTT functions. Telemetry fills
  `mbf_publish`; the MQTT code in `cyw43_utk_task` polls it (`TMO_POL`) and
  publishes; incoming commands go to `mbf_cmd_in` for the Command task (needs
  instructor agreement, A11, and the `NET=lwip MQTT=1` build).
- ISRs only record times or counts. Justified shared variables: encoder
  counters and the latest line-error record.
- SD1 (Fig 3): bar edges → Barcode ISR → Line ("capture started") →
  Navigation `holdSpeed()` → decode (reverse if the ends are not `*`) →
  letter 'B' → Navigation maps B = left, holds it, releases speed → line
  sensors both fully dark → "junction reached" → `moveForward(sensor-to-axle
  offset)` → `turnLeft(90)` → clear held command. Postcondition: heading
  changed by 90° ± 5°.
- SD2 (Fig 4): IMU sudden impact → Safety overrides pins low (< 50 ms),
  latches FAULT, buzzer/LED, FAULT flag puts Motion in BRAKED → RESET (GP21)
  → check cause cleared → zero commands and PID → ack → release override →
  SELF-TEST → READY.

## 5. System statechart (Fig 5)

BOOT (motor pins safe) → SELF-TEST (sensors only, motors off) → READY (wait for
START) → RUNNING { LINE FOLLOWING ⇄ HUMP CROSSING, ⇄ LINE SEARCH (≤ 5 s),
→ AT JUNCTION (move to axle, turn, find line) →, → AVOIDING OBSTACLE (scan,
go around or reverse, rejoin) → }. Exits: LINE SEARCH timeout → COMPLETE
(summary sent); no held command at a junction, no line within 3 s of a turn,
or no safe route → SAFE HOLD; collision or STOP from any running state →
FAULT; RESET → SELF-TEST (from FAULT) or READY (from SAFE HOLD).

## 6. Subsystem statecharts

| Chart | States |
|---|---|
| 6.1 Connectivity (B1) | CONNECTING → ONLINE ⇄ HANDLING COMMAND; failures → BACKING OFF (1, 2, 4 … 30 s) → CONNECTING. Topics under `inf2004/IS19`: publishes telemetry, event, status; subscribes cmd; answers on ack; short JSON with sequence number and timestamp |
| 6.2 Motion (B2) | IDLE ⇄ MOVING { DRIVING ⇄ HOLDING SPEED, DRIVING ⇄ TURNING }, turn also from IDLE; 100 ms without a Navigation update → IDLE; FAULT flag → BRAKED → (RESET handshake) → IDLE |
| 6.3 Line and barcode (B3) | LOAD LEVELS → ACTIVE with two parallel regions: LINE TRACKING (TRACKING ⇄ LINE LOST) and BARCODE (WAITING → CAPTURING → DECODING → WAITING); → RUN OVER. Details in [05](05_buddy3_line_and_barcode.md) |
| 6.4 IMU and safety (B4) | CALIBRATING → MONITORING ⇄ ON HUMP (roll above threshold; report on level); impact or bad data → FAULT LATCHED → (RESET, cause cleared) → MONITORING |
| 6.5 Obstacle (B5) | MONITORING (ping every 65 ms) → two readings ≤ 30 cm → STOPPING → COARSE SCAN (30–150°) → FINE SCAN (5° steps) → PLANNING → BYPASSING → REJOINING (≤ 30 cm) → MONITORING; no room → REVERSING once → re-scan; 3 invalid readings or still no room or no line → NO SAFE ROUTE → RUN OVER |

## 7. Assumptions (all Open)

| ID | Assumption | Owner |
|---|---|---|
| A1 | Kernel Wi-Fi (prio 4) and USB (prio 3) do not push Safety past 50 ms or loops past their periods | B1 |
| A2 | Motor encoders give enough counts per wheel turn for ±5° turns | B2 |
| A3 | Barcode sensor gives one clean edge per bar at cruise speed | B3 |
| A4 | Hump height from tilt via a calibration table | B4 |
| A5 | HC-SR04P works reliably at 3.3 V | B5 |
| A6 | Power bank supplies both motors and servo without resets (1 A / 1.5 A driver limits) | Team |
| A7 | The lecturer's default mapping applies, not the write-up's table | Team |
| A8 | Encoder interrupts do not delay barcode edge timing (else move encoders to PIO) | B2, B3 |
| A9 | With line sensors over the two edges, readings separate centred, junction and lost | B3 |
| A10 | A 5 s search recovers typical line losses; an unsuccessful search ends the run | B3, B2 |
| A11 | Instructor agrees to extending the template MQTT code with `mbf_publish` / `mbf_cmd_in` | B1 |
| A12 | 1 ms tick keeps CPU under 60% and 5 ms loops within ±1 ms | B3 |
| A13 | Each barcode is far enough before its junction for decoding to finish | B3 |
| A14 | Safety override on Motion's pins accepted as a TEAM.md exception | B4, B2 |

## 8. AI evidence highlights

8.2 architecture decisions: brake by forcing both pins low (not disabling PWM);
stop rather than go straight when no barcode was read; Wi-Fi loss never stops
the car; act at the junction, not at the barcode; lecturer's default mapping,
remappable; A0 for line sensors, D0 for the barcode sensor; network calls only
in `cyw43_utk_task`; team's own priority order 5–12; sensors over the two line
edges; end the run on a 5 s failed search instead of an MQTT "finish" command;
Safety keeps only the pin override.

8.3 gap check: items still "Partially fixed" or "Needs validation" are the
10 cm stopping distance at speed, both-white ambiguity (sensor placement),
the power bank's real 5 V / 2.1 A rating, encoder interrupt load, the 1 ms
tick, and stored line levels instead of demo-day calibration.

## 9. Design consistency answers

Device ownership: Line (IR sensors), IMU (GY-511), Scan (HC-SR04P, servo),
Motion (motors, encoders), Safety (buzzer, LED, pin override), kernel task
(radio, TCP/IP, MQTT). Highest priority is Safety (5). Statechart mapping: 6.1
Command/Telemetry, 6.2 Motion, 6.3 Line, 6.4 IMU/Safety, 6.5 Scan with
Navigation. Expected changes after W6: periods, priorities and tolerances are
targets to be measured; A1–A14 open; barcode mapping changes on demo day; a
requirements change at Tribunal 1.
