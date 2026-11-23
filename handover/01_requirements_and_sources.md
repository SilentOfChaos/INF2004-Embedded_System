# 01 — Requirements and sources (coverage A: project and assessment context)

Tags and source keys: see `00_README.md`.

## 1. The project in one table

| Item | Value | Tag |
|---|---|---|
| Module | INF2004 Embedded Systems (the planning notes also call it "ICT2212 Student Project") | [Verified: REPO `docs/planning_notes.md`, KB 01] |
| Team | IS19, 5 students | [Verified: RPT p.1, TEAM.md] |
| Project | Default project: autonomous line-following robot car | [Verified: KB 01, KB README] |
| Platform | Raspberry Pi Pico W on a Cytron Robo Pico; C with the Pico C SDK; the prof's µT-Kernel 3.0 port (course template) | [Verified: RPT NFR15, KB 01] |
| RTOS rule | µT-Kernel required; "FreeRTOS is allowed but caps the grade at A−" | [Verified: KB 01, citing write-up and lecture 1 L409–414, PT L31–39] |
| Coding standard | Barr-C Embedded C Coding Standard (2018); non-conformance costs marks | [Verified: KB 01] |
| AI use | AI-written code is fine, but each buddy must explain every line of their own subsystem or lose marks personally | [Verified: KB 01, citing PT L163–168, lecture 1 L189–194] |
| Repository | Public repo preferred; follow the template structure | [Verified: KB 02, citing L1 L258–266] |
| Commit history | "the prof also checks commit history, so commit and push regularly with meaningful messages" | [Verified: KB 01] |
| Session cost | "This session ran for over two hours and cost approximately SGD88." | [User-stated: TR L4275] |

### Team and ownership (student IDs intentionally omitted)

| Buddy | Name | Subsystem (RPT title table) | Code folder | Tag |
|---|---|---|---|---|
| 1 | Ang Hung Ling | Wi-Fi / MQTT communication and telemetry | `app_program/subsystem1/` | [Verified: RPT p.1, TEAM.md §1] |
| 2 | Dominic Ng Jun Jie | Motion control: motors, encoders and PID | `app_program/subsystem2/` | same |
| 3 | Nga Wei Yang | Line following and barcode decoding (also owns the steering calculation and the letter-to-action map that Navigation runs) | `app_program/subsystem3/` | same; RPT §3 "Ownership" |
| 4 | Yee Sheng Chua | IMU and terrain: hump measurement and collision detection (and the Safety task) | `app_program/subsystem4/` | same |
| 5 | Wong Qian Hui | Ultrasonic scanning and obstacle avoidance | `app_program/subsystem5/` | same |
| – | whole team | Navigation (the system statechart, "the briefing's Vehicle Controller") | `app_program/navigation/` | [Verified: RPT §3, TEAM.md §1] |

[Inferred] The user of this session is Buddy 3 (Nga Wei Yang): PREV's
`02_me_and_my_rules.md` lists Buddy 3's responsibility under "User and team",
the KB's Buddy 3 file is written to "you", `docs/planning_notes.md` is
signed "wy", and the G15 question put to the user on 16 Oct called Line
"your subsystem" without objection. The user never stated it directly.
[Reported: TPKG `IS19_Independent_Review_Actions.md` NF-6] The teammate's
review was prepared in Dominic Ng Jun Jie's (Buddy 2's) session; "you" in
that report means Buddy 2, not this session's user.

[Verified: TEAM.md §1] GitHub usernames are "*to fill*" for all five members.

## 2. Mission (write-up, as summarised in KB 01)

[Verified: KB 01 "The mission (write-up)"] The car runs a predefined course
autonomously and shall: 1 follow a line track; 2 detect and decode barcodes;
3 execute the barcode command (left, right, straight, U-turn) using
encoder-based motion control; 4 detect humps; 5 measure and report the
highest hump peak; 6 detect obstacles on or near the line; 7 profile obstacle
shape and position with the ultrasonic sensor; 8 go around obstacles without
colliding; 9 reacquire the original line; 10 report telemetry over Wi-Fi
(MQTT). Line following must be robust to minor lighting changes, slight
track imperfections and speed variations.

## 3. Assessment and dates

All rows [Verified: KB 01 "Grading" and "Dated timeline"], which cite the
write-up, briefing, `W0_Module Overview.pdf` and the transcripts.

| Component | Weight | When | Form |
|---|---|---|---|
| Lab quizzes | 10% (2% × 5) | Due 2 weeks after each lab, W2–W6 | xSITe MCQ, unlimited attempts, last attempt counts |
| Design package | Hurdle | W6, Sat 10 Oct 23:59 | template docx |
| Tribunal 1 | 15% | W8, **Mon 19 Oct**, lecture slot (KB README: 11:00–13:00, Zoom) | Zoom breakout discussion; a requirements change is introduced and must be incorporated |
| Subsystem check-in | Hurdle | W10 lab (IS-DCO: **Mon 2 Nov, 16:00–18:00**) | Show working progress |
| Lecture quiz | 30% | W12, Mon 16 Nov, 11:00, in person | 50 questions in 30 min, no AI, closed book |
| Practical skills assessment | 15% | W12, back to back with the lecture quiz | Debug code snippets, no AI |
| Tribunal 2 | 15% | W13, Mon 23 Nov, lecture slot | Requirements change discussed in theory only |
| Final run | 15% | W13 lab session | Demo + final code + testing evidence in GitHub; a short video is also submitted |

- Hurdle rule: missing or late = 5% off the total group project mark per day [Verified: KB 01].
- Group project (45%) graded holistically, then moderated by peer evaluation [Verified: KB 01].
- Bonus up to 30 marks (individual) for a genuine bug in the prof's µT-Kernel port [Verified: KB 01, lecture 1 L415–483].
- Week 1 started Mon 31 Aug 2026; W7 (12–18 Oct) is the break week; the pack was written in W7 [Verified: KB 01 timeline; system date 2026-10-14].
- Page limit: "Prof's email, 2026-10-08: page limit removed (was 5). Report not graded, but submission is a hurdle." [Reported: PREV `01_goal_and_scope.md`, quoting an earlier Claude handoff].

### Tribunal mechanics [Verified: KB 01, citing lecture 1 L196–254, PT L155–162]

- Each team records its own Zoom breakout session; no recording means zero. All five should record locally.
- The prof introduces a requirements change; each buddy must say how it affects their subsystem, or why it does not. Open AI use on screen is encouraged.
- Tribunal 1 builds on the W6 report; "the prof can ask anyone about anything"; each buddy will be asked about their own §8.1 row.

### Final demo rules [Verified: KB 01, citing PT L205–233, lecture 1 L255–267, consultation 5:52–6:02]

- Three attempts; **the most recent attempt's result counts**.
- Under about 5 minutes.
- Letter-to-action mapping announced on the day; A–Z must be decodable; mapping changed over Wi-Fi **without recompiling**.
- Held on the instructors' own MQTT broker.
- Once placed and switched on the car must not be touched, except to restart the attempt.

## 4. Functional requirements (verbatim, RPT §1.1)

[Verified: RPT text extract, lines 24–84] Wording copied exactly; line
breaks from the PDF table were joined.

| ID | Requirement |
|---|---|
| FR1 | The robot shall follow the 18 mm black line from start to finish without operator input. |
| FR2 | The robot shall decode Code 39 barcodes (letters A to Z) read in either direction of travel. |
| FR3 | The robot shall act on a decoded barcode command at the next cross junction, not at the barcode. |
| FR4 | At a junction, the robot shall go straight, turn left, turn right or U-turn as commanded, finishing within ±5° of the commanded heading. |
| FR5 | If no barcode command is held when a junction is reached, the robot shall stop and report that no command was read. |
| FR6 | The robot shall use the default mapping A = straight, B = left, C = right, D = U-turn, and shall allow the mapping to be changed over MQTT without reflashing. |
| FR7 | The robot shall keep following the line while one wheel crosses a hump. |
| FR8 | The robot shall detect each hump, estimate its peak height to within ±5 mm, and report the number of humps and the highest peak. |
| FR9 | The robot shall detect an obstacle on or near the line and stop at least 10 cm before it. |
| FR10 | The robot shall scan the obstacle with the servo-mounted ultrasonic sensor, coarse then fine, and report its position, width, closest point and left and right clearance, each to within ±2 cm. |
| FR11 | The robot shall go around the obstacle on the side with more clearance, passing 5 to 10 cm from it, and rejoin the line beyond it; if neither side has room, it shall reverse 10 cm and scan once more before stopping. |
| FR12 | The robot shall publish telemetry over MQTT covering robot state, wheel speeds, encoder counts, line sensor readings, barcodes read, hump data, obstacle data and distance travelled. |
| FR13 | The robot shall publish a timestamped route log of distances, turns, and where each barcode, hump and obstacle was met. |
| FR14 | The robot shall accept MQTT commands to remap barcode letters, request a run summary and stop the robot. |
| FR15 | The robot shall brake its motors and hold a fault state on a collision or a STOP command, until it is reset. |
| FR16 | The robot shall stay still after power-on until the START button is pressed. |
| FR17 | The robot shall classify its motion as stationary, accelerating, turning, climbing a hump, descending a hump or sudden impact, and report motion events and turn rate in telemetry. |
| FR18 | When the line is lost unexpectedly during line following or hump crossing, the robot shall search for it for 5 s ± 0.5 s and, if it is not found, stop, end the run and publish a run summary. |
| FR19 | The robot shall drive a commanded distance to within ±5% and turn a commanded angle, such as 45° or 90°, to within ±5°, using the wheel encoders. |

## 5. Non-functional requirements (verbatim, RPT §1.2)

[Verified: RPT text extract, lines 85–133]

| ID | Requirement |
|---|---|
| NFR1 | The line sensors shall be sampled every 5 ms ± 1 ms. |
| NFR2 | Barcode bar edges shall be time-stamped to 1 µs resolution or better. |
| NFR3 | The wheel-speed control loop shall run every 20 ms ± 2 ms. |
| NFR4 | The accelerometer shall be sampled at 100 Hz ± 5 Hz and the magnetometer at 75 Hz ± 5 Hz. |
| NFR5 | Ultrasonic pings shall be at least 60 ms apart. |
| NFR6 | The robot shall remove motor drive within 50 ms of a fault being confirmed or a STOP command being received on board. |
| NFR7 | The robot shall complete the course in under 5 minutes. |
| NFR8 | A barcode remap shall take effect within 1 s of being received. |
| NFR9 | Telemetry shall be published at 5 Hz ± 0.5 Hz, with a heartbeat every 1 s ± 0.1 s. |
| NFR10 | Loss of Wi-Fi or MQTT shall not stop the robot or change its mode. |
| NFR11 | No voltage above 3.3 V shall be applied to any Pico W GPIO pin. |
| NFR12 | The robot shall run a full course on the 5 V / 2.1 A power bank without resetting. |
| NFR13 | Steady-state CPU load shall not exceed 60%. |
| NFR14 | The MQTT broker address shall be changeable without reflashing. |
| NFR15 | The firmware shall be written in C with the Pico C SDK, run on µT-Kernel using the course template repository and its TEAM.md rules for pins, priorities and interfaces (recording any agreed exception there), and follow the Barr C Coding Standard, with no printf inside interrupt handlers and no unjustified global variables. |
| NFR16 | The safety stop shall stay active until reset and shall not be undone by normal motor-control updates; motor commands shall be zeroed before it is released. |
| NFR17 | Line following shall work at speeds from 0.15 to 0.35 m/s, under room lighting from 200 to 1000 lux, and across the joins between course tiles. |
| NFR18 | When driving straight, each wheel's speed shall stay within ±10% of its target and the car shall drift no more than 2 cm sideways per metre. |
| NFR19 | Total RAM use, including task stacks and the Wi-Fi stack, shall stay below 75% of the Pico W's 264 KB. |

[Verified: `docs/decisions.md` §E] FR5 and FR16 are "kept exactly as written
in the report" in the code. No requirement text was changed by the code work.
How each requirement is met (or not yet shown) is in `02_current_state.md`.

## 6. Assumptions A1–A14 (RPT §7, status as in RPT)

[Verified: RPT text extract, lines 548–622] The PDF table is laid out in
columns; the cell text below was re-joined from the column-wrapped extract.
Words are unchanged; check line breaks against the PDF if exact layout
matters. All fourteen are **Open** in RPT.

| ID | Assumption | Risk if wrong | Validation method | Owner |
|---|---|---|---|---|
| A1 | The kernel's Wi-Fi (priority 4) and USB (priority 3) service tasks, which run above all our tasks, will not push Safety past 50 ms or the control loops past their periods. | Stops or steering updates arrive late while the radio is busy. | Measure the Safety response and loop periods with MQTT publishing at full rate and during reconnects. | Buddy 1 |
| A2 | The motors' built-in encoders give enough counts per wheel turn for ±5° turns. | Turns overshoot or undershoot. | Count encoder edges per wheel turn; test 10 turns of each type, and measure the sensor-to-axle offset used before turning at junctions. | Buddy 2 |
| A3 | The barcode sensor gives one clean edge per bar at cruise speed. | Extra edges corrupt the decode. | Measure the printed narrow and wide bars, then pass the sample barcodes at 3 speeds and check the edges on a logic analyser. | Buddy 3 |
| A4 | Hump height can be estimated from the car's tilt using a calibration table. | Wrong heights, or turns mistaken for humps. | Drive over humps of known height at 3 speeds. | Buddy 4 |
| A5 | The HC-SR04P works reliably at 3.3 V. | Short range or missed echoes. | Check the Echo HIGH level is at most 3.3 V before connecting GP5, then measure range at 10 to 100 cm. | Buddy 5 |
| A6 | The power bank can supply both motors and the servo without the Pico resetting. | The car resets mid-run, or a motor channel exceeds 1 A continuous / 1.5 A peak (under 5 s). | Measure supply voltage and current during motor starts with the servo loaded; check motor stall current against the driver rating. | Team |
| A7 | The lecturer's default mapping (A = straight, B = left, C = right, D = U-turn) applies, not the write-up's example table. | Wrong turns at junctions. | Confirm with the lecturer; remap over MQTT on demo day if needed. | Team |
| A8 | Encoder interrupts will not delay barcode edge timing. | Barcode decode errors at speed. | Measure the encoder interrupt rate; move encoder counting to the Pico's PIO if it is high. | Buddy 2, Buddy 3 |
| A9 | With the line sensors over the two line edges, the analogue readings clearly separate centred, junction and lost. | Junctions missed or taken for a centred line. | Using levels stored in the lab, log both sensors over the line, a junction and white floor, under the lab's and the demo room's lighting. | Buddy 3 |
| A10 | The team assumes a 5 s search is enough to recover typical temporary line losses. An unsuccessful search ends the run; it does not by itself confirm that the course was completed. | The car ends the run early mid-course, or keeps searching after the end. | Lift the car off the line briefly on straights, curves and a hump and check it recovers; at the course end, check it stops in COMPLETE after the search. | Buddy 3, Buddy 2 |
| A11 | The instructor agrees to extending the template's MQTT code with the mbf_publish and mbf_cmd_in message buffers. | Telemetry and remote commands would need another route. | Ask the instructor at the next consultation; record the interface in TEAM.md section 5. | Buddy 1 |
| A12 | A 1 ms kernel tick keeps CPU load under 60% and the 5 ms loops within ±1 ms. | Line control is late or uneven. | Toggle a spare pin each cycle and check it on a logic analyser; read the RTOS CPU statistics. | Buddy 3 |
| A13 | Each barcode is far enough before its junction for decoding to finish first (the lecturer gave no minimum distance, only that it will be sufficient). | The letter arrives after the junction, so the car stops in SAFE HOLD. | Measure the time from the last bar to the decoded letter (120 ms end of capture plus decoding) at top speed, convert it to distance and compare with the sample course. | Buddy 3 |
| A14 | The proposed Safety override on Motion's motor pins is accepted as an exception to TEAM.md's one-owning-task rule. | The stop would have to go through Motion, so braking would depend on Motion still running. | Agree it as a team, record it in TEAM.md, and confirm with the instructor at the next consultation. | Buddy 4, Buddy 2 |

Current status of each assumption after the code work: `11_open_questions_and_risks.md` §2.

## 7. Source material that exists (outside the public repo)

All seven uploads are in this container only; hashes and sizes are in
`03_artifact_and_version_inventory.md` §4. Content descriptions:
[Verified: KB README "Source inventory", 03 §4 listing].

| Upload (as uploaded, with upload-ID prefix) | Contents | Used for |
|---|---|---|
| `30357074-INF2004_Design_Review_IS19.zip` | RPT PDF + `Fig1`–`Fig10` PNGs | Requirements, design, assumptions |
| `b5a363a3-Transcript.zip` | `project transcript.txt` (briefing), `lecture 1 main zoom transcript.txt`, `lecture 1 part 2 transcript.txt`, `embedded prof consultation 1.txt` | Instructor statements (KB 02 cites them as PT, L1, C) |
| `d18b81e9-Materials_for_project.zip` | Project Briefing, Project Write-up, `Barcode & Line Specification.pdf`, `Barcode Sample.pdf`, Barr-C 2018, datasheets (Pico W, `Robo_Pico.pdf`, `Robo Pico Board Datasheet.pdf`, TCRT5000, IR sensor manual, LM393, HC-SR04, LSM303DLHC, OKY3231/GY-511), `Project Mapping.xlsx`, alternative-project briefs | Hardware facts, barcode facts |
| `fed21e50-lectures.zip` | W0–W6 lecture decks, Lecture 5 (v2, v4), Lecture 6, lab simulator pages | Method; quiz content (KB 07) |
| `f7f8f9ac-hardware_images_not_all_are_relevant.zip` | 28 photos + `PicoCar.pptx` | Kit identification (KB 03) |
| `2fe3821d-INF2004_final_merged_handoff_1.zip` | PREV (11 Markdown files) | Starting state before coding |
| `fd45f295-INF2004_ProjectTemplate-main.zip` | TPL, the course template | Imported into REPO (commit `e870827`) |
| `815149fa-IS19_Independent_Review_Actions.md` (16 Oct) | TPKG: the teammate's review report (52 feedback items, 8 new findings, dispositions) | Reviewed in this session (08, 09) |
| `5f0dc9a1-IS19_consistency_PRs_v2.zip` (16 Oct) | TPKG: 17 patches in six series, PR texts, `open_prs.sh` and its tests, evidence logs | Applied to REPO (merged 16 Oct) |
| `33f9916c-INF2004_IS19_corrected_v2.zip` (16 Oct) | TPKG: the tree with all six series merged (750 files) | Compared byte for byte with the applied patches |

Not available in this session [Verified: KB README; PREV 00_README.md]:
`Week_6_Design_Review_Template.docx`, the report DOCX
`INF2004_Week6_Design_Review_IS19_final (10).docx`, the Tribunal 1 template,
lecture videos, and any measurement from the real car.
