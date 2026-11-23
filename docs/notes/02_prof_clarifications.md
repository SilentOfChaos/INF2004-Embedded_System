# Clarifications from the prof and instructors

Things said out loud that are not (or not clearly) in the written briefs.
`PT` = `project transcript.txt` (project briefing video), `L1` = `lecture 1 main
zoom transcript.txt`, `C` = `embedded prof consultation 1.txt` (timestamps).
Transcripts are auto-generated, so wording is garbled in places; the meaning
below is the most consistent reading.

## Course and mission

| Topic | What was said | Source |
|---|---|---|
| Barcode side | Barcode is on either the left or the right of the line; the side is fixed and would be confirmed when kits were issued | PT L16 |
| Hump side | Hump is on the opposite side to the barcode, and that is fixed too | PT L135–136 |
| Hump size | Only one wheel goes over the hump (no double hump, front sensors would be blocked) | PT L19 |
| Barcode meaning | The barcode tells you what to do at the next **cross junction**: straight, left, right or U-turn | PT L17–18, L148, L223 |
| When to act | The barcode always comes before the junction; act only when the junction is reached, not at the barcode | PT L139–141 |
| Barcode to junction distance | No guaranteed minimum, but always enough that you will not reach the junction before decoding | C 6:42–7:12 |
| Default mapping | A = forward, B = left, C = right, D = U-turn (this differs from the write-up's example table A = left, B = right, C = straight) | PT L146 |
| Demo-day mapping | Letters change on the day (for example A straight, Z U-turn, W left, F right). All of A–Z must be decodable, and the map must be changed over Wi-Fi with no recompiling | PT L142–147, L228–233 |
| Reading direction | Barcodes are directional; scanning the other way gives different data; the decoder must work in both directions ("it has been done before") | PT L97–100 |
| Obstacle placement | On the line, and only on a straight section, so you roughly know where to rejoin | PT L26 |
| Obstacle behaviour | Ultrasonic on the servo scans left and right, picks the bigger gap, keeps scanning to find the obstacle's depth, then turns back in and follows the line | PT L124–128 |
| Obstacle width | Measure it and send it over Wi-Fi; leave enough room for the car's own width | PT L132–133 |
| Course map | The final map is a surprise; there is definitely a line, and hump and barcode are on opposite sides | PT L134–136 |
| Run time | A well-built car should finish in under 5 minutes; 10 minutes means something is wrong | PT L207–210 |
| End of run | Tasks never "end"; a subsystem loops forever. The run itself can end when the line ends: search (for example about 5 s, or a full 180° sweep) and stop if there is still no line | C 1:44–4:14, 13:58–15:31 |
| Touching the car | Once the car is placed and switched on it cannot be touched, except to restart | C 5:52–6:02 |

## Subsystem-specific

| Subsystem | What was said | Source |
|---|---|---|
| WiFi (B1) | MQTT is required. The PC client should see everything about the car: movement, humps detected and heights, barcodes read and decoded, straight/left/right state | PT L59–62 |
| WiFi (B1) | Output need not be fancy, just clear to a human reading the subscriber | PT L64–66 |
| WiFi (B1) | Two-way: on request the car must send a snapshot/summary of its telemetry | PT L68–71 |
| WiFi (B1) | On demo day the car connects to the instructors' broker; a guide for running a broker on a laptop will be provided | PT L62 |
| WiFi (B1) | Telemetry should let you reconstruct the route: moved 20 cm, turned right, where obstacles and barcodes were | PT L224–226 |
| Motion (B2) | Two motors never match; PID must equalise them. Tune only after the wheels are fixed in place; gains are specific to your car | PT L74–85 |
| Motion (B2) | Must drive straight and turn precise angles (90°, 45°); on a hump one side is higher and the car must still follow the line | PT L87–90 |
| Motion (B2) | PID quality matters most for going around the obstacle | PT L134 |
| Line/barcode (B3) | This year: two IR sensors for line following plus a third for the barcode (last year was one each); placement is up to you | PT L92–96 |
| Line/barcode (B3) | Line-following strategies discussed: both sensors on the line, or both just outside it on white, correcting when one crosses an edge; detection must be fast enough to correct | C 17:46–18:15, 20:54–21:35 |
| Line/barcode (B3) | Calibration: make it as robust as possible; "you can just place [it] on the towel ... black and white" | C 7:15–7:56 |
| Line/barcode (B3) | Never decode only once; if a read fails, keep trying on the next barcode | C 3:11–3:58 |
| IMU (B4) | The GY-511 has an accelerometer and a **magnetometer (digital compass), not a gyroscope** | PT L104–107 |
| IMU (B4) | Combine encoder and IMU heading for turns; sensor fusion scores better than single-sensor use | PT L109–112 |
| IMU (B4) | Hump height from tilt: tilt the car to known heights, record readings, build a mapping, then tune | PT L113–118 |
| IMU (B4) | Ask Motion to slow down on a hump; move-and-measure scores better than stop-and-measure | PT L120–121 |
| Safety | Safety task data for stopping also needs to come from the ultrasonic | C 8:45–9:01 |

## Design-document feedback (consultation)

- Pin diagram: show the Grove groups from the Robo Pico document, so that when
  requirements change you can see which ports are free (C 0:13–0:43).
- Requirements need tolerances. A ± percentage is acceptable if you do not want
  absolute numbers; "as close to X as possible" is not acceptable. They will not
  fault you for the tolerance value itself, as long as it is achievable and
  testable (C 4:17–5:42).
- Task numbering in the interaction diagram can follow priority rather than
  sequence; Navigation looks like the main controller (C 8:05–11:46).
- Subsystem statecharts need a start and an end (C 12:52–12:58).
- Building the car before the W6 design was up to the team (C 20:22–20:28).

## RTOS, code and process

| Topic | What was said | Source |
|---|---|---|
| RTOS | µT-Kernel is enforced; it will not be taught (a FreeRTOS lab is given instead), mapping the concepts is part of the learning. The prof's port repo is shared | PT L31–44; L1 L409–414 |
| Why an RTOS | Helps testing, isolating problems and coordinating tasks | PT L55–56 |
| Development order | Write drivers per device first, then the subsystem, then integrate two or three buddies at a time | PT L236–239 |
| Pico capability | "The Pico is powerful enough to handle all of this", do not argue otherwise | PT L242–245 |
| Robustness | It must work every run; if it fails, know why. Test, test, test | PT L150–153; L1 L709–716 |
| Integer sizes | Choose widths and signedness to fit the data; floats for integer data are penalised | L1 L821–827 |
| Spaghetti code | Penalised; modular, commented code expected; rerun regression tests after any change | L1 L795–803 |
| Testing | He will look at the tests; incomplete coverage is treated as untested; put test data on GitHub | L1 L776–780 |
| PIO | Using PIO may be considered for better marks | L1 L884–887 |
| AI | Use it freely (local models, Claude, Codex, Gemini all mentioned) but own every line | L1 L337–343 |
| Repo | Prefer public; send the link when asked; follow the template structure | L1 L258–266 |
| Slackers | Email the prof early (can be anonymous); give evidence in peer evaluation | PT L180–183 |
| Help | Ask instructors in the lab first for default-project questions; hardware issues go to Mr Haja; discussion forum for content; email header `[INF2004]` with group | L1 L323–333, L381 |

Instructor names (W0 overview): module lead Muhamed Fauzi Bin Abbas; lab
manager Haja Mohideen; lab instructors Aw Kok Seng (also gives the W5/W6 SDLC
lectures), Deric Ng, Paul Fong, Kang Liat Chuan. IS-DCO labs (P5, P6) are on
Mondays 16:00–18:00.
