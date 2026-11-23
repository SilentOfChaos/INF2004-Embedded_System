# Buddy 3: line following and barcode decoding

Owner: Nga Wei Yang. Sources: write-up (Buddy 3 tasks), `Barcode & Line
Specification.pdf`, `Barcode Sample.pdf`, design review §2, §3, §6.3, §7, §8,
Fig 3 and Fig 8, TCRT5000 and LM393 datasheets, `project transcript.txt`,
consultation transcript. The barcode numbers in §3 were measured from the
sample PDF for these notes.

## 1. What Buddy 3 owns

- Write-up tasks: IR sensor integration, calibration, line position estimate,
  line-following algorithm, junction detection, barcode detection and decoding,
  navigation command generation. Deliverables: line-following module, barcode
  decoder, navigation command interface. Performance goals: reliable tracking,
  accurate recognition, robust at varying speeds.
- In our design: the **Line Task** (priority 6, 5 ms, core 0), the **Barcode
  ISR** (GP7), the **steering calculation** that runs inside Navigation, and
  the **letter-to-action map** (default A straight, B left, C right, D U-turn,
  remappable over MQTT).
- Assumptions to close: A3, A9, A12, A13 (alone), A8 (with B2), A10 (with B2).
- At Tribunal 1 you must be able to explain your §8.1 row and every line of
  this subsystem's code.

## 2. Track geometry

| Item | Value | Source |
|---|---|---|
| Line width | 18 mm, black on light grey/white | Spec photo |
| Gap from line edge to barcode | 20 mm, so the barcode starts **29 mm from the line centre** | Spec photo |
| Barcode length along the line | ≈ 141 mm | Spec photo |
| Bar height (perpendicular to the line) | Not given; the photo crops it. **Measure on the real course strips** | — |
| Side | Fixed left or right; hump is on the other side. Confirm which side | PT L16, L135 |
| Barcode before junction | Always, with "sufficient" distance, no stated minimum | PT L139; C 6:42 |
| Junction type | Cross junction ("+") | PT L17, L148 |
| Obstacles | Only on straight track | PT L26 |

## 3. Code 39 as used on the course

Verified on `Barcode Sample.pdf` by measuring every bar at 300 dpi:

- 4 pages: `*A*` full width, `*A*` half width, `*Z*` full width, `*Z*` half width.
- Each barcode is 3 characters (`*`, letter, `*`) = 3 × 9 elements + 2 narrow
  inter-character gaps = **29 elements** (15 bars, 14 spaces).
- Every character has exactly 3 wide elements out of 9 (5 bars, 4 spaces,
  starting and ending with a bar).
- Wide:narrow ratio measured **3.02 to 3.05**, so 3:1.
- Printed sizes in the sample PDF: full width 328.5 mm (narrow 6.94 mm), half
  width 164.9 mm (narrow 3.47 mm). The course barcode is ≈ 141 mm, which gives
  **narrow ≈ 3.0 mm, wide ≈ 9.0 mm** (141 / 47 modules). The decoder must be
  ratio-based because the print scale varies.

Patterns (b s b s b s b s b, w = wide, n = narrow):

| Char | Pattern | Char | Pattern | Char | Pattern |
|---|---|---|---|---|---|
| A | wnnnnwnnw | J | nnnnwwwnn | S | nnwnnnwwn |
| B | nnwnnwnnw | K | wnnnnnnww | T | nnnnwnwwn |
| C | wnwnnwnnn | L | nnwnnnnww | U | wwnnnnnnw |
| D | nnnnwwnnw | M | wnwnnnnwn | V | nwwnnnnnw |
| E | wnnnwwnnn | N | nnnnwnnww | W | wwwnnnnnn |
| F | nnwnwwnnn | O | wnnnwnnwn | X | nwnnwnnnw |
| G | nnnnnwwnw | P | nnwnwnnwn | Y | wwnnwnnnn |
| H | wnnnnwwnn | Q | nnnnnnwww | Z | nwwnwnnnn |
| I | nnwnnwwnn | R | wnnnnnwwn | * | nwnnwnwnn |

### Reading direction (important)

Scanning a barcode backwards reverses the element order. Checked for all 26
letters:

- `*` read backwards is **P**. So a reversed scan always reads `P?P` and a
  forward scan `*?*`. `*A*` backwards reads `P1P`; `*Z*` backwards reads `PTP`.
- **Every letter read backwards is another valid symbol**: A→`1`, B→`8`,
  C→`7`, D→`5`, E→`4`, F→`0`, G→`3`, H→`2`, I→`9`, J→`6`, K→`U`, L→`.`,
  M→`-`, N→`Y`, O→`X`, P→`*`, Q→`W`, R→`V`, S→space, T→`Z`, U→`K`, V→`R`,
  W→`Q`, X→`O`, Y→`N`, Z→`T`.
- So "the middle character decoded to a valid letter" proves nothing about
  direction. If the end characters were not checked, a backwards `*Z*` would be
  accepted as **T**, and a backwards `*K*` as **U**: a confident wrong turn.
  Direction must come from the ends: `*` both ends = forward; `P` both ends =
  reversed, so reverse the element list and decode again; anything else =
  failed read.

This matches the prof's "if you scan one way and scan another way, you have
different data" (PT L97) and is a good Tribunal answer.

## 4. Timing budget

| Speed | Narrow bar 3 mm | Wide bar 9 mm | Whole barcode 141 mm | 5 ms of travel | 120 ms of travel |
|---|---|---|---|---|---|
| 0.15 m/s | 20.0 ms | 60.0 ms | 0.94 s | 0.75 mm | 18 mm |
| 0.25 m/s | 12.0 ms | 36.0 ms | 0.56 s | 1.25 mm | 30 mm |
| 0.35 m/s | 8.6 ms | 25.7 ms | 0.40 s | 1.75 mm | 42 mm |

- At 0.35 m/s a narrow bar lasts under two 5 ms polling periods, so polling
  would quantise widths by up to ±5 ms (±58%). That, not "too short to poll"
  (the AI claim the team rejected: a 12 ms bar does get 2–3 polls), is the
  real reason for ISR timestamps with `time_us_64()` (1 µs, NFR2).
- The design's 120 ms end-of-capture timeout is 2× the widest space at the
  slowest speed. It also adds up to 42 mm of travel before the letter is
  known (relevant to A13). See §6 for a way to remove that delay.

## 5. Sensors and mounting

- TCRT5000 peak response at 2.5 mm from the surface, usable 0.2–15 mm.
  Its sensing spot is a few mm wide, about the same as a 3 mm narrow bar, so
  the analogue signal over narrow bars and spaces will not swing fully between
  black and white levels.
- D0 comes from an LM393 comparator against the trim-pot threshold, with no
  hysteresis on these modules. Consequences: (1) bars come out systematically
  wider or narrower than spaces depending on the pot setting; (2) slow edges
  can produce a burst of extra edges. Both directly threaten A3.
- Typical MH-module polarity is D0 low on white (indicator LED on) and high
  on black, and A0 rises on black. **Verify on our modules** before writing the
  ISR edge logic.
- Practical steps: mount the barcode sensor 2–4 mm above the surface; set the
  pot midway between the A0 black and white levels; wire the barcode module's
  A0 to **GP28 (ADC2)** as well, since GP28 is on the same Grove 7 cable as GP7
  and is unused (keeps a diagnostic and fallback channel, and lets A3 be
  measured without a logic analyser).
- Line sensors: two modules, centres about 18 mm apart so each sits over one
  edge of the line (see §7). Measure the sensor-to-axle distance used by
  `moveForward(offset)` before a turn (A2).
- Barcode sensor lateral position: past 29 mm from the line centre plus the
  expected lateral wander, inside the bar height. Decide after measuring the
  bar height.

## 6. Barcode decoder

Current design (§6.3, Fig 8): WAITING → first bar → CAPTURING (ISR stamps
every edge; Navigation calls `holdSpeed()`) → 120 ms without a bar → DECODING
(9 elements per character, skip the inter-character gap; wide = more than 2×
the narrowest; check `*` at both ends; if not, reverse and try once) → valid
letter or "capture failed" to Navigation (both release the speed hold) →
WAITING.

Suggested hardening, all compatible with the submitted statechart:

1. **Glitch filter in the ISR.** Ignore an edge less than ~1 ms after the
   previous one (1 ms is 0.35 mm at top speed, far below a 3 mm bar).
2. **Classify per character with the 3-of-9 rule** instead of one global
   "2× narrowest" threshold: sort the 9 widths of a character, the 3 largest
   are wide, and require a clear gap between the 3rd and 4th largest. This
   tolerates speed drift across the barcode and the bar/space bias from the
   comparator. (A single short glitch also no longer shrinks the "narrowest"
   reference for the whole barcode.) An alternative is separate thresholds for
   bars and for spaces.
3. **Validate structure:** exactly 29 elements; both inter-character gaps
   narrow; ends `*` (forward) or `P` (reversed); middle character in A–Z only.
   Anything else is "capture failed". A wrong letter is far worse than no
   letter.
4. **Decode as soon as the 30th edge arrives** (trailing edge of the 15th bar)
   instead of waiting 120 ms; keep the timeout only for incomplete captures.
   This removes up to 42 mm of travel from the A13 budget.
5. **Require a quiet zone before WAITING → CAPTURING** (white for some
   minimum time) so noise or a line crossing does not start a capture
   mid-pattern.
6. Options worth knowing for Tribunal questions:
   - Stamp edges with encoder position instead of time: speed-independent,
     but couples Line to Motion's counters (cross-subsystem interface).
   - Measure pulse widths with a PIO state machine: cycle-accurate, immune to
     interrupt latency from the encoders (A8), and the prof said PIO use may
     earn credit.

## 7. Line following

- Sensors straddle the two edges. Centred: both partly dark. Junction
  crossbar: both fully dark. Off line: both white. With both sensors on the
  line, a centred car and a crossbar would both read fully dark, which is why
  the team chose the edges (§8.2).
- Line error: normalise each sensor to darkness d = (A0 − white) / (black −
  white), clamp to 0..1, error = d_left − d_right in −1..+1. Agree the sign
  convention with the steering code. In firmware prefer fixed point (for
  example int16 in −1000..+1000) over float: Barr-C 5.4 and the prof both push
  against floats where integers will do.
- The error is only proportional within about ± the sensor spot radius (a
  couple of mm) of centre. Beyond that it saturates at ±1 and stays there
  until the car is almost a full line width off; then it falls back towards 0
  as the sensor still over the line crosses the far edge, until both read
  white at roughly 18 mm plus the spot radius. So it is **not monotonic**, and
  in practice it behaves like "left / centred / right" plus a small linear
  zone: tune the steering with that in mind. Keep the last non-zero error sign
  and start the line search towards that side; the design's LINE SEARCH does
  not say which side it sweeps first.
- Junction: both fully dark for 3 samples (15 ms = 2.3–5.3 mm). An 18 mm
  crossbar gives 10 samples at 0.35 m/s and 24 at 0.15 m/s, so 3 leaves margin.
- Line lost: both white for 300 ms (45–105 mm of travel), long enough to
  bridge tile joins (NFR17).
- Levels: black and white stored from the lab, then adapted while driving as
  the sensors cross the edges. The consultation only asked that calibration
  be "as robust as possible".
- Steering (B3-owned code that runs in Navigation every 5 ms): PD on the line
  error giving a left/right speed difference to Motion; slowing down on large
  error helps curves. The Line task (priority 6) runs before Navigation
  (priority 7) in the same 5 ms tick, so Navigation always uses this tick's
  error.

## 8. Edge cases to settle

| Case | What happens now | Suggested handling |
|---|---|---|
| Junction crossbar passes under the barcode sensor (the crossbar extends to the barcode side) | Starts a capture of one 18 mm "bar" → capture failed, and `holdSpeed()` is requested in the middle of the junction manoeuvre | Navigation ignores barcode events in AT JUNCTION; "capture failed" must never clear an already held letter |
| Line passes under the barcode sensor on a tight curve | Same as above | Same; quiet-zone check helps |
| Car stops mid-barcode (obstacle, fault) | Timeout → capture failed | Fine |
| Read fails before a junction | FR5: no command at the junction → SAFE HOLD, run over | Team decision: keep, or reverse slowly over the barcode and re-read (bidirectional decoding makes this possible) |
| Two barcodes before one junction | Not specified | Latest valid letter wins, or treat as an error: decide |
| Lateral drift takes the sensor off the bars | Missing elements → capture failed | Size the sensor offset after measuring bar height |
| Demo-room lighting or sunlight | Stored levels + adaptation | Test under two lighting conditions (A9, NFR17) |
| One wheel on a hump tilts the chassis, so the line sensors' height changes (TCRT5000 output falls off quickly beyond a few mm) | Readings may look lost or biased; adaptive levels could learn wrong values | Freeze level adaptation during HUMP CROSSING; include a hump in the A10 tests |

## 9. Interfaces to pin down in `subsystem3.h` / TEAM.md

- `line_err` record (latest value, mutex): error, timestamp, "valid" flag (for
  example false while lost).
- `line_evt_t` queue to Navigation: JUNCTION, LINE_LOST, LINE_FOUND,
  CAPTURE_STARTED, LETTER(letter, direction), CAPTURE_FAILED(reason).
- Barcode ISR → Line task hand-off of edge timestamps. The design justifies
  only two shared variables (encoder counters, line-error record); this ring
  buffer is a third. Either justify it (single producer and consumer, both on
  core 0) or use a kernel object, after checking which µT-Kernel calls the
  port allows from interrupt context.
- Letter map: 26-entry table, letter → action, updated atomically by the
  Command path (NFR8: within 1 s).

## 10. Assumption validation plan

| ID | How to close it | Pass criterion (proposal) |
|---|---|---|
| A3 | Log A0 (GP28) and D0 (GP7) while pushing the car over the sample barcode at 3 speeds; count edges per pass | 30 edges per pass, no extra edges after the glitch filter |
| A8 | Feed a known square wave into GP7 (spare GPIO or PIO) while both wheels run at full speed; compare measured and true widths | Error under 1% of a narrow bar at 0.35 m/s |
| A9 | Log both line ADCs over: centred line, crossbar, white floor, in the lab and in the demo room | Clear gaps between "partly dark", "fully dark" and "white" bands |
| A10 | Lift the car off the line on a straight, a curve and a hump | Recovers within 5 s; stops in COMPLETE at the real end of the line |
| A12 | Toggle a spare pin at each Line task release; check on a logic analyser; read RTOS CPU stats | 5 ± 1 ms; CPU ≤ 60% |
| A13 | Time from last bar to letter delivered at top speed; convert to distance; compare with the sample course | Letter known well before the junction (state the margin) |

## 11. Test plan sketch (Lecture 6 style IDs)

Unit tests (host, Unity framework, decoder fed with synthetic widths):

| ID | Case |
|---|---|
| UT-B3-01 | All 26 letters forward, ideal widths |
| UT-B3-02 | All 26 letters reversed → same letter, direction = reversed |
| UT-B3-03 | Reversed `*Z*`, `*K*`, `*O*` must never produce T, U, X |
| UT-B3-04 | Speed ramp ±25% across one barcode |
| UT-B3-05 | Bars +40%, spaces −40% (comparator bias) |
| UT-B3-06 | Glitch edges injected; missing element (28); extra element (30) → no letter |
| UT-B3-07 | Middle character not A–Z (digit, `-`, `.`, space, `*`) → rejected |
| UT-B3-08 | Single 18 mm dark region (junction crossbar) → capture failed |
| UT-B3-09 | Line-error boundaries: both white, both black, one black, values outside the stored levels (clamping) |

Integration tests: line-error period 5 ± 1 ms over 1000 updates (IT-B3-01);
event queue full is counted and raised (IT-B3-02); edge timing under encoder
interrupt load (IT-B3-03, A8); Navigation holds a letter until the junction
and a later "capture failed" does not clear it (IT-B3-04).

System tests: sample barcodes at 0.15/0.25/0.35 m/s in both orientations,
20 passes each, zero wrong letters (ST-B3-01); test loop with straights,
curves, tile joins, two lighting levels (ST-B3-02); line-loss recovery (ST-B3-03);
barcode-to-junction latency (ST-B3-04). Inspection: sensor heights and spacing
(INS-B3-01).

## 12. Tribunal 1 preparation (Buddy 3)

Your §8.1 row, in short:

- Prompts: (1) design line following and Code 39 decoding in both directions for
  2 line sensors and 1 barcode sensor from the spec, sample, briefing transcript
  and PicoCar slides, with statechart and sequence diagrams; (2) check that
  design and another AI's design against the TCRT5000 datasheet and barcode
  sheet and list every wrong claim.
- Useful AI ideas: the barcode sits beside the line (20 mm gap); a reversed
  barcode decodes by reversing the bars; interrupt timestamps beat polling;
  line tracking keeps running during capture.
- Weak AI ideas: assumed ST188 modules (kit is TCRT5000); treated both-white as
  line loss without saying where the sensors sit; drew tracking and capture as
  one-at-a-time states; claimed a 12 ms bar was too short to poll.
- Decisions: A0 for line sensors, D0 for the barcode sensor; sensors over the
  line edges (A9); two parallel regions; failed captures reported so the speed
  hold is released; levels stored in the lab.

Questions to be ready for: why the edges and not on/outside the line; how the
direction is detected (§3); why ISR and not polling (§4 numbers); what happens
after a failed read (FR5, SAFE HOLD); lighting robustness without demo-day
calibration; how a junction differs from a curve; where your shared data lives
and how it is protected; why Line is priority 6 and Navigation 7.

Plausible requirement changes and their effect on this subsystem:

| Change | Impact |
|---|---|
| Barcodes on both sides, or the side swaps | Second barcode sensor: GP6 on Grove 5 is free (GP26 there is taken); ISR, Line task and capture state per side |
| Multi-letter barcodes or another symbology | Element count no longer fixed at 29; variable-length decode up to the stop character; Code 128 would need a new decoder |
| New commands (stop, 45° turn, speed change) | Letter map entries and Navigation actions; decoder unchanged |
| Thinner or inverted line (white on black) | Sensor spacing (must match line width) and normalisation polarity |
| Higher speed (for example 0.5 m/s) | Narrow bar 6 ms, still fine for ISR timing; only 7 junction samples per crossbar; the 300 ms loss window becomes 150 mm of travel; steering gains |
| Junctions of other shapes (T, Y) | Junction classification from which sensors go dark and in which order |
| One line sensor removed | Single-sensor edge following; no centred/junction distinction from two sensors |
