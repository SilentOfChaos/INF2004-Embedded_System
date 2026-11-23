# Project, rules and assessment

Sources: `Project Write-up.pdf`, `Project Briefing.pdf`, `W0_Module Overview.pdf`,
`project transcript.txt`, `lecture 1 main zoom transcript.txt`.

## The mission (write-up)

The car runs a predefined course autonomously (no remote control) and shall:

1. Follow a line track.
2. Detect and decode barcodes along the track.
3. Execute the barcode command (turn left, turn right, go straight, U-turn), using encoder-based motion control.
4. Detect humps.
5. Measure and report the highest hump peak of the run.
6. Detect obstacles on or near the line.
7. Profile obstacle shape and position with the ultrasonic sensor.
8. Go around obstacles without colliding.
9. Reacquire the original line after the bypass.
10. Report telemetry over Wi-Fi (MQTT).

Line following must be robust to minor lighting changes, slight track
imperfections and speed variations. Obstacle handling order in the write-up:
stop or slow, profile, choose a bypass, leave the line, go around, search for
and reacquire the line, continue; deviation from the route should be minimal.

Suggested layers: sensor (IR, encoders, IMU, ultrasonic, barcode sensor),
control (motion, line following, obstacle avoidance, navigation logic),
communication (Wi-Fi, MQTT, telemetry).

## Mandatory constraints

| Constraint | Source |
|---|---|
| µT-Kernel RTOS (the prof's own Pico port). FreeRTOS is allowed but caps the grade at A−. | Write-up; lecture 1 L409–414; project transcript L31–39 |
| Raspberry Pi Pico W on the Cytron Robo Pico | Write-up |
| C language with the Pico C SDK (no Python, no Rust) | Write-up; lecture 1 L847–856 |
| Follow the template GitHub repo structure | Write-up; lecture 1 L258–266 |
| Barr-C Embedded C Coding Standard (2018); non-conformance costs marks | Write-up; lecture 1 L288–291 |
| AI-written code is fine, but each buddy must explain every line of their own subsystem or lose marks personally | Project transcript L163–168; lecture 1 L189–194 |
| No recompiling on demo day after the barcode mapping is announced | Project transcript L232–233 |

## Buddy split (fixed 5-person structure)

| Buddy | Owner (IS19) | Subsystem | Write-up deliverables |
|---|---|---|---|
| 1 | Ang Hung Ling | Wi-Fi, MQTT command and telemetry | Communication API, telemetry framework, MQTT documentation, telemetry demo |
| 2 | Dominic Ng Jun Jie | Motors, encoders, PID | Motion-control library, PID tuning report, motion accuracy evaluation |
| 3 | Nga Wei Yang | 3 IR sensors: line following, junctions, barcode decoding | Line-following module, barcode decoder, navigation command interface |
| 4 | Yee Sheng Chua | IMU: tilt, humps, peak height, motion class, collision | IMU processing module, hump-detection algorithm, terrain analysis report |
| 5 | Wong Qian Hui | Ultrasonic + servo: coarse/fine scan, profiling, avoidance, line recovery | Scanning subsystem, obstacle profile generator, avoidance and recovery algorithms |

Every buddy also participates in integration, system testing, debugging and the
final demo, and must understand all subsystem interfaces. Write-up API examples
for Buddy 2: `moveForward(distance)`, `moveBackward(distance)`,
`turnLeft(angle)`, `turnRight(angle)`, `stop()`.

## Grading

| Component | Weight | When | Form |
|---|---|---|---|
| Lab quizzes | 10% (2% × 5) | Due 2 weeks after each lab, W2 to W6 labs | xSITe MCQ, unlimited attempts, **last attempt counts** |
| Design package | Hurdle | W6, Sat 10 Oct 23:59 | "3 to 5 pages" per the overview; template docx |
| Tribunal 1 | 15% | W8, Mon 19 Oct, lecture slot | Zoom breakout discussion |
| Subsystem check-in | Hurdle | W10 lab (IS-DCO: Mon 2 Nov, 16:00–18:00) | Show working progress |
| Lecture quiz | 30% | W12, Mon 16 Nov, 11:00, **in person** | 50 questions in 30 min, one attempt, no AI, closed book |
| Practical skills assessment | 15% | W12, back to back with the lecture quiz, in person | Debug code snippets (logic errors, Heisenbugs), no AI |
| Tribunal 2 | 15% | W13, Mon 23 Nov, lecture slot | Zoom; requirements change discussed in theory only |
| Final run | 15% | W13 lab session | Demo + final code + testing evidence in GitHub |

Hurdle rule: missing or late = 5% off the total group project mark per day.
The group project (45%) is graded holistically, then individual marks are
moderated by peer evaluation; the prof also checks commit history, so commit
and push regularly with meaningful messages.

Bonus (individual, up to 30 marks): find, reproduce, document and ideally fix a
genuine bug in the prof's µT-Kernel port, or propose a feature first and get it
approved (lecture 1 L415–483). It is debugging against the OS's own
specification, not general pen testing.

## Dated timeline

Week 1 started Mon 31 Aug 2026 (consistent with the README's W4 = 21–27 Sep).

| Week | Dates | Project event |
|---|---|---|
| W6 | 5–11 Oct | Design review submitted (hurdle) |
| W7 | 12–18 Oct | Break week (now) |
| W8 | 19–25 Oct | **Tribunal 1** Mon 19 Oct; labs become own time, own target |
| W9 | 26 Oct–1 Nov | Sensor fusion lecture |
| W10 | 2–8 Nov | Subsystem check-in (hurdle) in the lab |
| W11 | 9–15 Nov | No lecture (public holiday on Mon 9 Nov; pre-recorded edge computing video). Monday labs get a make-up |
| W12 | 16–22 Nov | Lecture quiz + practical quiz, in person, Mon 11:00–13:00 |
| W13 | 23–29 Nov | Tribunal 2 + final demonstration |

## How the tribunals work (lecture 1 L196–254, project transcript L155–162)

- Everyone attends the lecture slot; teams go into Zoom breakout rooms and
  **record their own session**. No recording means zero (assumed cheating). The
  prof recommends all five members record locally in case one PC fails.
- The prof introduces a requirements change ("monkey wrench"). In W8 it must be
  incorporated into the project; in W13 it is discussed in theory only.
- Each buddy must say how the change affects their subsystem, and if it does
  not, why not. Using AI openly on screen during the session is encouraged
  (show the prompts).
- Roughly 1.5 hours; there are three reports per tribunal (before, during,
  after), and after the tribunal the team has one week to refine the analysis
  with AI and lock it down.
- Tribunal 1 builds on the W6 report. Templates come one or two weeks before;
  the prof explains the format around W7.
- What is graded: how the requirements change affects the design and how the
  team discusses it. The prof can ask anyone about anything. Per the team
  README, each buddy will be asked to explain their own row in design section
  8.1, so nobody can answer for you.

## Final demo rules (project transcript L205–233, lecture 1 L255–267, L284)

- Three attempts are allowed and **the most recent attempt's result counts**,
  not the best. Decide carefully whether to try again.
- The whole course should take under about 5 minutes; very slow runs are noted
  and may be penalised.
- Mapping of letters to actions is announced on the day; A–Z must already be
  decodable and the mapping must be changed over Wi-Fi without recompiling.
- The demo is held on the instructors' own MQTT broker.
- A perfect run does not guarantee full marks: final code quality (standard,
  structure) and testing evidence in the GitHub repo are part of the mark. A
  short video of the project working must also be submitted.
- Once the car is placed and switched on it must not be touched, except to
  restart the attempt (consultation 5:52–6:02).

## What the prof said he will check or mark down

From lecture 1 and the embedded C video (part 2). The last item was a general
warning rather than an explicit marking rule.

- Spaghetti code; functions should be small and modular with clear interfaces.
- Wrong variable widths or types (for example a float where the data is an
  integer, or a 32-bit type for 0 to 100); missing `volatile` on ISR-shared or
  hardware-changed data; missing `const`.
- Global variables, unless justified (kernel objects such as semaphores and
  message buffers are fine to declare globally).
- Long interrupt handlers; never `printf` or loops inside an ISR.
- Untested or happy-path-only testing. He reviews test coverage, expects test
  data in GitHub, and expects regression tests to be re-run after every change.
- Inconsistent behaviour between runs ("works most of the time" is not
  acceptable; failures must be explained and fixed).
- `malloc` in embedded code.

Positive signals: sensor fusion (encoder plus IMU heading, rather than single
sensors), measuring the hump while moving rather than stop-and-measure, and use
of the Pico's PIO ("I might even be considering certain better marks").
