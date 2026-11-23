# Minimum-bit data representation: evidence (change requirement 1)

Change requirement 1 (Tribunal 1): "Each data field representing
application information must use only the number of bits required to
represent its valid range, using C bit-fields or explicit masks and
shifts where appropriate." The reasons for every choice below are in
[`decisions.md`](decisions.md) section I (D-82 to D-92), with the fault
mask and the stall watch in D-102 to D-105; this page
collects the evidence for Week 13: the data representation table, memory
and message sizes before and after, and what the tests show.

"Before" is commit `c644b68` (16 Oct). All figures were taken from the
code and the builds on 22 Oct with `arm-none-eabi-gcc` 13.2.1, the
template's flags (`-O2 -mcpu=cortex-m0plus -mthumb`). Nothing in this
page has been measured on the car yet: run-time figures (stack use,
timing, CPU load) need the measurement build on the car, section 5.

## 1. How fields are packed

- `common/bitpack.h`: a record is one or more `uint32_t` words; a field
  is its lowest bit and its width. Masks and shifts on unsigned values
  only: no C bit-fields for shared or queued data (the compiler chooses
  their layout, Barr-C 5.5) and no bitwise operator on a signed value
  (Barr-C 5.3). A signed field is two's complement in its own width.
- A value that does not fit is never cut: the nearest value that fits
  is stored and the pack function returns false. Every layout is checked
  when building (`BITPACK_ASSERT`), so a setting that no longer fits
  stops the build.
- Each record has pack and read functions next to its type; nobody
  touches the words directly. Each has a host test with the exact
  expected words.
- A field nothing reads needs 0 bits: such fields were removed (listed
  in the tables as "removed").
- Message buffers are sized in messages (`common/app_mbf.h`), and each
  holds exactly as many messages as it really held before, so a burst
  overflows exactly when it did before (D-83).
- MQTT payloads stay JSON text until the forum question on binary
  payloads is answered (section 7); only records inside the firmware are
  packed.

## 2. Data representation table

Bits "before" are the C type's width; "after" are the bits the field
now uses. A range in **bold** is proved from the code that produces it
(each has a build-time check or a test, named in the decision).

### Subsystem 3, Line and barcode (Buddy 3)

**IF-01 line event** (Line → Navigation), 8 bytes → 2 bytes (D-84)

| Field | Valid range | Before | After |
|---|---|---|---|
| type | LINE_EVT 1..6 | 8 | 3 |
| letter | none, 'A'..'Z' | 8 | 5 |
| reversed | yes / no | 8 | 1 |
| failure reason | never read (Navigation does the same for every failure) | 8 | removed |
| time stamp | never read | 32 | removed |

**IF-02 line record** (shared, under a mutex), 24 bytes → 8 (D-84)

| Field | Valid range | Before | After |
|---|---|---|---|
| line error | **−1000..1000** | 32 | 11 |
| left, right readings | 0..4095 (12-bit ADC; read by the on-target test S3-U21) | 16 + 16 | 12 + 12 |
| line seen, valid | flags | 8 × 2 | 1 × 2 |
| lost | never read (Navigation learns it from the LOST and FOUND events) | 8 | removed |
| sample count | modulo 256 (at most 210 between two reads, S3-U20) | 32 | 8 |
| darkness left, right | never read outside Line | 16 + 16 | removed |
| time stamp | never read | 32 | removed |

**Barcode edge** (Barcode ISR → Line task queue), 8 bytes → 4 (D-85)

| Field | Valid range | Before | After |
|---|---|---|---|
| time | µs modulo 2^31; the Line task rebuilds the 32-bit time exactly for an edge under 35.8 min old | 32 | 31 |
| level | dark / white | 8 | 1 |
| padding | – | 24 | – |

**Barcode capture state** (Line task stack), 284 bytes → 192 (D-85)

| Field | Valid range | Before | After |
|---|---|---|---|
| state | WAITING, CAPTURING | 8 (enum) | 1 |
| current level, started sent, decode tried, edge lost, overflow | flags | 8 × 5 | 1 × 5 |
| failure reason | BC_FAIL 0..8 | 8 | 4 |
| edge count | 0..64 | 32 | 7 |
| quiet zone | 0..255 ms (`CFG_BC_QUIET_MS`) | 32 (µs) | 8 (ms) |
| white start, white before the capture | free-running µs clock | 32 × 2 | 32 × 2 |
| edge times, × 64 | edge 0 on the µs clock; edges 1..63 **within 4.19 s of edge 0** (2 s capture + 50 ms check-in + 200 ms watchdog, once the watchdog is armed: radio up, or 15 s after boot) | 32 × 64 | 32 + 22 × 63 |

**Line tracking** (Line task stack), 48 bytes → 12 (D-86)

| Field | Valid range | Before | After |
|---|---|---|---|
| four levels | **0..4095** (move only towards a 12-bit reading) | 32 × 4 | 12 × 4 |
| line error | −1000..1000 | 32 | 11 |
| dark run | stops at its limit, 3 | 32 | 2 |
| rearm run | 0..4 | 32 | 3 |
| white time | stops at its limit, 300 ms | 32 | 9 |
| failed reads in a row | 0..4 | 32 | 3 |
| junction armed, lost, line seen | flags | 8 × 3 | 1 × 3 |
| darkness left, right | never read | 32 × 2 | removed |

**Steering** (in Navigation's state), 8 bytes → 2; **letter map**, 26 bytes
→ 12 (D-86)

| Field | Valid range | Before | After |
|---|---|---|---|
| previous error | −1000..1000 (clamped first) | 32 | 11 |
| have previous | flag | 8 | 1 |
| action per letter, × 26 | letter_action 0..4 | 8 × 26 | 3 × 26 |

**What the Line task keeps between cycles** (task stack), about 24
bytes → 8 (D-87, D-92)

| Field | Valid range | Before | After |
|---|---|---|---|
| last readings | 0..4095 | 32 × 2 | 12 × 2 |
| cycle | modulo 40 (when a status is due) | 32 | 6 |
| letters read | reported modulo 256 | 32 | 8 |
| failed captures | never printed (D-92) | 32 | removed |
| last letter | none, 'A'..'Z' | 8 | 5 |
| readings in range, edge dropped | flags | 8 × 2 | 1 × 2 |

### Subsystem 2, Motion

**IF-07 command** (Navigation → Motion), 12 bytes → 4 (D-88). Fields by
type; a field a command does not carry reads 0.

| Field | Valid range | Before | After |
|---|---|---|---|
| type | MOTION_CMD 1..8 | 8 | 3 |
| drive speed, difference | **−1000..1000** (`motion_send()` limits them) | 16 + 16 | 11 + 11 |
| move ID | 0..255 | 8 | 8 |
| distance or angle, its size (the type gives the direction) | **0..18000** (Navigation's largest: a U-turn; every value checked when building; Motion's API refuses a larger one) | 32 | 15 |
| hold flag | yes / no | 32 | 1 |

**IF-08 status** (Motion → Navigation), 28 bytes → 8 (D-88)

| Field | Valid range | Before | After |
|---|---|---|---|
| type | 1..2 | 8 | 1 |
| heading | (−18000, 18000] cdeg | 32 | 16 |
| finished move ID | 0..255 | 8 | 8 |
| distance since boot | **below 2^30 mm in size** (two 32-bit counters, π × 65 / 528 mm per count) | 32 | 31 |
| counts, speeds, state, move active, time | never read here | 32 × 2, 16 × 2, 8, 8, 32 | removed |

**Encoder counters** (Encoder ISRs ↔ Motion), 8 bytes → 4 (D-88,
Tribunal 1 "Version B"): two 16-bit counters in one word, each written by
its own ISR with a 16-bit store, read by Motion in one 32-bit load;
Motion adds the signed 16-bit change to its 32-bit totals. Exact below
32768 counts between reads: Motion reads every 20 ms (about 100 counts
at 2 m/s); if it stops, its 100 ms check-in and the 200 ms watchdog
reset the chip within about 0.3 s (about 1,600 counts), once the
watchdog is armed (radio up, or 15 s after boot).

**Stall and encoder-fault watch** (Motion's own state, new with D-105):
one word, each wheel's silent time 0..1000 ms (`CFG_MOTION_STALL_MS`,
checked when building) in 10 bits and a reported flag, 21 bits.

### Subsystem 4, IMU and Safety

**IF-03 IMU event** (IMU → Navigation), 32 bytes → 4 (D-89)

| Field | Valid range | Before | After |
|---|---|---|---|
| type | 1..4 | 8 | 2 |
| heading, level ground | (−18000, 18000]; flag | 32; 8 | 16; 1 |
| hump peak roll, height | 0..18000; **0..65 mm** (table) | 32; 32 | 15; 7 |
| motion class | 0..6 | 8 | 3 |
| turn rate, roll, pitch, time | never read here | 32 × 4 | removed |

**IF-04 fault report** (any task → Safety), 8 bytes → 4 (D-89)

| Field | Valid range | Before | After |
|---|---|---|---|
| cause | 1..11 (7 until the fault mask, D-102) | 8 | 4 (3 until D-102) |
| source | 1..8 | 8 | 3 |
| detail | **0..8191** (largest: a collision's \|a\|, under 6929 mg) | 16 | 13 |
| cleared (status causes only, D-102) | flag | – | 1 |
| time stamp | never read | 32 | removed |

**Fault mask** (Safety's state, sent in its status record; D-102): one
bit per cause, 11 bits, written only by Safety from the reports in its
queue. Before: one cause code (8 bits), kept for the first fault only.

### Subsystem 5, Obstacle scan

**IF-05 obstacle event** (Scan → Navigation), 24 bytes → 8 (D-90)

| Field | Valid range | Before | After |
|---|---|---|---|
| type | 1..6 | 8 | 3 |
| side (plan) | 0..2 | 8 | 2 |
| give-up reason | never read (Navigation holds the car the same way for every reason) | 8 | removed |
| closest range | **0..500** (hits only) | 16 | 9 |
| width | −1000..1000 | 16 | 11 |
| left, right edge | −500..500 | 16 × 2 | 10 × 2 |
| distance, centre, clearances, time | never read here | 16 × 4, 32 | removed |

**IF-06 scan request** (Navigation → Scan), 8 bytes → 1: type 8 → 4 bits,
side 8 → 2 bits (bypass only), time removed. **Ultrasonic echo** (ISR →
Scan): width 32 → 15 bits (every width from 23.2 ms up reads as 4 m, so
a longer one is kept at 32767 µs); rise time keeps 32 (compared with the
trigger time); the message stays 8 bytes (D-90).

### Subsystem 1, Command and Telemetry

**IF-10 Navigation command** (Command → Navigation), 8 bytes → 8, 39 of
64 bits used (D-91): type 8 → 1, letter 8 → 5, action 8 → 3, seq 32 → 30
(**0..999,999,999**: the parser reads at most 9 digits).

**IF-15 broker** (Command → MQTT code), 18 bytes → 8 (D-91): the address
text, 16 characters of 8 bits → each of its four parts in 9 bits, as the
code of its spelling (a part of 1 to 3 digits up to 255 has 366
spellings; the spelling is kept because lwIP reads "010" as octal), the
port 16 bits.

**IF-12 telemetry records** (every task → Telemetry), message 28 bytes →
20 (D-92). The 32-bit time travels only with the records whose time is
printed.

| Record | Fields after (bits) | Removed (never printed) | Bytes before → after |
|---|---|---|---|
| line status | readings 12 + 12, error 11, letters 8, last letter 5 | lost flag, failed count, time | 10 → 8 |
| Motion status | speeds 16 + 16, counts 32 + 32, distance 31 | state, time | 20 → 16 |
| IMU status | rate 16, class 3, humps 8, highest hump 7 (+ time 32) | heading, roll | 10 → 8 |
| Scan status | range 12, obstacles 16 | state, invalid count, servo angle, time | 8 → 4 |
| Navigation status | mode 4, held letter 5, hold reason 2 | held action, heading, run time, time | 12 → 4 |
| Safety status | 2 flags, cause 4, queue-full 8, faults 16, misses 16, fault mask 11 (D-102) | time | 8 → 8 |
| route entry | distance 31, kind 4, by kind up to 35 (+ time 32) | reason | 16 → 12 |
| ack | seq 30, code 3, ok 1 (+ time) | detail | 8 → 8 |
| fault | cause 4, source 3, detail 13, latched 1 (+ time) | – | 6 → 4 |
| link | online 1, Wi-Fi state 3 (−3..3) (+ time) | – | 8 → 4 |
| obstacle profile | side 2, reversed 1, x 9, y 10, closest 9, width 11, clearances 12 + 12 (+ time) | – | 14 → 12 |

Telemetry keeps the status records and the route log packed: its state
is 868 bytes instead of 1088 (route log 48 × 20 → 48 × 16 bytes).

## 3. Memory before and after

### Static RAM and code (linker, `arm-none-eabi-size`, default build)

| Step | Commit | .text | .data + .bss |
|---|---|---|---|
| before | `c644b68` | 316096 | 28120 |
| measurement build added (off) | `4eca7d3` | 316096 | 28120 |
| packing helpers | `13e778d` | 316096 | 28120 |
| line event and record | `5d5f337` | 316608 | 28104 |
| edge record and capture | `4ea0bcd` | 317104 | 28104 |
| tracking, steering, letter map | `5ef6be9` | 317544 | 28088 |
| Line task's kept values | `9734da0` | 317932 | 28088 |
| Motion messages, encoder | `3883945` | 318552 | 28088 |
| IMU events, faults | `64fb037` | 318872 | 28088 |
| obstacle events, requests, echoes | `4d2fd79` | 319652 | 28088 |
| Navigation command, broker | `f8e8dc3` | 320008 | 28080 |
| distance 31 bits | `6c89bb3` | 320044 | 28080 |
| telemetry records | `cf587f3` | 324040 | 27860 |
| team review fixes: Line | `488a32d` | 323932 | 27860 |
| team review fixes: Obstacle scan | `2c3937b` | 323932 | 27860 |
| team review fixes: Motion | `9ebeee1` | 323912 | 27860 |
| team review fixes: Command, Telemetry | `d0e0bcb` | 324016 | 27852 |
| external review: Motion refuses an oversize move | `5cddd0a` | 324000 | 27852 |
| external review: GPIO time stamp per status read | `7bcbf91` | 324004 | 27852 |
| external review: S3-U20 text, timing probe | `b1d23ec` | 324004 | 27852 |
| decided G19: a line record that stops changing | `551329e` | 323980 | 27860 |
| decided G20: a fault cancels a waiting RESET | `dd3a336` | 324276 | 27860 |
| second review: line health drops at once | `6c5011b` | 324324 | 27860 |
| second review: stale record timed, not ticked | `4e0a9e7` | 324536 | 27860 |
| second review: failed-read count restarts while idle | `049504b` | 324560 | 27860 |
| servo travel 30–150°, side-scan look-ahead | `35ae793` | 324612 | 27860 |
| third review: SELF-TEST after COMPLETE senses again | `ae83300` | 324680 | 27860 |
| team member's review: measured time, safe drive | `0e5ecec` | 324648 | 27860 |
| team member's review: line-lost time measured | `87476ab` | 324700 | 27860 |
| team member's review: STOP never lost (MQTT build) | `0bb3580` | 324700 | 27860 |
| team member's review: count cap, start-up failure | `06ad014` | 324820 | 27860 |
| team member's review: echo wait 50 ms | `2fee4b3` | 324820 | 27860 |
| fault mask | `4d43ac3` | 325148 | 27860 |
| Wi-Fi loss bit | `66be08e` | 325332 | 27860 |
| no-echo bit; SELF-TEST needs an answer | `4391724` | 325548 | 27864 |
| stall and encoder-fault watch | `50f7f39` | 325888 | 27864 |
| Navigation starts last | `b7e9553` | 326004 | 27864 |

Up to `b1d23ec`: static RAM −268 bytes (the MQTT build too: 86840 →
86572); code +7908 bytes (+2.5%) for the pack and read functions. With
the measurement build off, `4eca7d3` gives images byte-identical to
`c644b68` in all seven profiles. The behaviour changes made after the
outside reviews (G19, G20, D-96; section 10) and the servo's travel
(D-97) are not part of the representation change: together they add
8 bytes of static RAM (Navigation's state 432 → 440 bytes) and 676
bytes of code. The changes after the team member's review (section 11)
add 4 bytes of static RAM (the Scan task's context) and 1324 bytes of
code, of which the fault mask with its two status bits takes 728 and
the stall watch 340.

Records on task stacks also shrank (capture 284 → 192 bytes, line
tracking 48 → 12, the Line task's kept values about 24 → 8, each
`tlm_msg_t` 28 → 20), but the configured stack sizes are unchanged:
lowering them needs the high-water marks measured on the car first
(section 5).

### Kernel heap: message buffers

Bytes the code asks the kernel for. Each buffer holds the same number
of messages as before; the kernel stores each as a 4-byte header plus
the message rounded up to 4 bytes.

| Queue | Message bytes, before → after | Messages | Buffer bytes, before → after |
|---|---|---|---|
| `mbf_line_evt` (IF-01) | 8 → 2 | 21 | 256 → 168 |
| `mbf_imu_evt` (IF-03) | 32 → 4 | 17 | 640 → 136 |
| `mbf_obstacle` (IF-05) | 24 → 8 | 9 | 256 → 108 |
| `mbf_motion_sts` (IF-08) | 28 → 8 | 18 | 576 → 216 |
| `mbf_nav_cmd` (IF-10) | 8 → 8 | 10 | 128 → 120 |
| `mbf_tlm` (IF-12) | 28 → 20 | 27 | 864 → 648 |
| `mbf_mqtt_cfg` (IF-15) | 18 → 8 | 2 | 52 → 24 |
| `mbf_motion_cmd` (IF-07) | 12 → 4 | 20 | 320 → 160 |
| `mbf_fault` (IF-04) | 8 → 4 | 21 | 256 → 168 |
| `mbf_scan_req` (IF-06) | 8 → 1 | 10 | 128 → 80 |
| echoes | 8 → 8 | 10 | 128 → 120 |
| barcode edges | 8 → 4 | 85 | 1024 → 680 |
| `mbf_publish`, `mbf_cmd_in` (MQTT text) | unchanged | 8, 4 | 2632 + 544, unchanged |
| **Total** | | | **7804 → 5804** |

## 4. MQTT payloads

The format is unchanged (JSON text): S1-U43 shows every message is
byte-identical to before for the same values; the one addition since is
the heartbeat's fault mask (D-102), which S1-U43 allows for. The longest
possible messages are shorter, because no field can hold a value outside
its range any more (bytes, every field at its extreme):

| Message | Before | After |
|---|---|---|
| telemetry | 259 | 249 |
| heartbeat | 213 | 204 (218 with the fault mask, D-102) |
| route entry (longest) | 132 | 118 |
| summary | 208 | 208 |
| ack | 77 | 77 |
| fault | 108 | 107 (109 with the new cause names, D-102) |
| motion event | 89 | 84 |
| link | 73 | 64 |
| obstacle profile | 143 | 126 |

The fault figures were first worked out with the source "safety" (105
and 104); "telemetry", 3 characters longer, gives the longest message,
and is used here since 25 Oct.

## 5. Correctness and performance evidence

### Host tests (`testing/host/`, 109 tests)

All pass with gcc and clang, with and without AddressSanitizer and
UBSan (LeakSanitizer on as well). Each packed record has a test with
the exact expected words, and the boundaries of every field. The
side-by-side tests run a frozen copy of the old code
(`testing/host/ref/`) next to the packed code:

| Test | What it shows |
|---|---|
| COM-U07 to COM-U12 | the packing helpers: unsigned and signed fields, values too large kept at the end, wrapping differences, bit arrays across words, bits needed for a range, buffer bytes per message, a whole word back to `int32_t` |
| S3-U40, S3-U41 | IF-02 and IF-01: exact bits, every value round-trips |
| S3-U42 | edge record: the exact 32-bit time rebuilt for 200,000 random edges, across the clock's wrap |
| S3-U43 | old and packed capture side by side, 4000 random runs: the same output at every poll |
| S3-U44 | capture layout, the 4.19 s limit, and a slow letter whose last edges are stored past 2^21 µs (Line task held up 0.26 s): read back exactly, as by the old code |
| S3-U45 to S3-U47 | old and packed line tracking (1.2 million samples), steering (200,000 steps), letter map (100,000 operations): identical |
| S3-U48 | the Line task's kept values against the old arithmetic, 20,000 cycles |
| S3-U49 | old and packed capture side by side with the Line task held up for up to 0.27 s near the 2 s limit: edges stored up to 2.2 s after the first bar, the same output at every poll |
| S2-U40 to S2-U42 | Motion commands (every move size 0..32767), status (every 31-bit distance), encoder pair through the driver's own add (each wheel changes only its own half; totals equal 32-bit counters through many wraps) |
| S4-U40, S4-U41 | IMU events (every height the table can give), fault reports (11 causes in 4 bits, the cleared flag) |
| S4-U08 | the fault mask: one bit per cause; status bits that never latch, count or cancel a RESET; the release clears only the latching bits; 100,000 random steps against the fault table |
| S5-U40 to S5-U42 | every plan the profile code makes from 20,000 random scans (all 31 angles) fits; every event type; requests; echo record through the ISR's own packing, every width up to 200 ms |
| S1-U40, S1-U41 | broker record (every spelling of a part, and 10,000 random addresses, back character for character); Navigation commands (26,000 parsed commands) |
| S1-U42 | exact bits of every telemetry record |
| S1-U43 | old and packed telemetry side by side: 20,000 random states, 20,000 random events of each kind, the route-log replay: every MQTT message identical |
| S1-U44 | every obstacle profile from 20,000 random scans fits |
| NAV-U01 to NAV-U19 | the whole Navigation statechart, now on packed messages |
| NAV-U40 to NAV-U42, S3-U19, S4-U07 | not packing: the behaviour changes made after the outside reviews, a line record that stops changing (G19), a fault while RESET waits (G20), line health after failed reads (D-96) and SELF-TEST after COMPLETE (D-98) |
| S2-U03, S2-U06, S2-U07, S1-U12 | not packing: the team member's review, measured time (D-99), the safe drive (D-100), the stall and encoder-fault watch (D-105, its state in one word), a STOP never lost (D-101) |

### Code

- Encoder ISR: each ISR updates its own 16-bit half with `ldrh`,
  `adds`, `uxth`, `strh` (checked in the compiled code, `arm-none-eabi-gcc`
  13.2.1, `-O2 -mcpu=cortex-m0plus -mthumb`; a reviewer found the same
  with gcc 13.3.1 and clang 18); Motion reads both halves with one load,
  so interrupts are no longer masked to read them.
- Seven build profiles, 0 warnings; the measurement build too.

### To measure on the car (not done yet)

Timing, stack use and queue use before and after, with the measurement
build (D-82; how: `testing/README.md`): set `CFG_STATS_ENABLE` to 1,
build and run commit `4eca7d3` (old representation), then the newest
commit, the same way, and compare the console reports: task periods and
work times, handler times, deepest stack use per task, least free space
per queue. Only then can the stack sizes be lowered.

## 6. Not packed yet

- Each subsystem's own stored state other than Line's (Buddy 3's is
  done): Motion's, the IMU's, Safety's and Scan's task contexts,
  Navigation's statechart state (`nav_sm_t`), Telemetry's run totals,
  Command's context, the encoder ISRs' 2-bit quadrature states, the GPIO
  dispatcher's interrupt counter. Each owner can do theirs the same way.
- MQTT payloads (JSON text), until the forum question is answered.

## 7. Open, needing a team or instructor decision

- Telemetry's wheel speeds and turn rate keep the old 16-bit clamp
  (D-92): 12 bits would hold the motor's rated top speed, but a clamp
  there changes the MQTT text for a faster reading.
- Showing a removed field anywhere, such as a failed capture's reason
  or why the Scan task gave up (D-84, D-90), would be new behaviour.
- MQTT: the requirement says compact binary fields; the instructor said
  MQTT messages must stay readable in an ordinary MQTT client. Asked on
  the course forum; JSON kept until then (G22 in `decisions.md`).
- Not done because they change behaviour (`decisions.md` section E): a
  PWM cap, rejecting oversized MQTT messages. The fault mask (G21,
  decided by the team after Tribunal 1) and stall and encoder-fault
  detection are done (section 11); the RESET handshake fix (G20) and
  the age check on the line record (G19) were decided on 23 Oct and are
  done (section 10).
- The Barcode and Echo ISRs pack their records (D-85, D-90); the team
  ruled packing in interrupts out after Tribunal 1 and has not decided
  whether to keep it once the interrupt times are measured or move it
  into the tasks (G25).

## 8. Findings while packing

- `bc_decode()` never reports `BC_FAIL_PATTERN` while characters are
  classified one by one (D-85); harmless, not changed.
- The encoder pair no longer needs interrupts masked (D-88).
- Telemetry fields nobody printed were removed (D-92); showing any of
  them on MQTT would change the JSON and needs the team's agreement.

## 9. Team review (22 Oct)

Four team members reviewed their parts of this change (Line and
barcode, Obstacle scan, Motion, Command and Telemetry) against the
rules on this page. None found a blocker. What changed, with no
behaviour change (`488a32d`, `2c3937b`, `9ebeee1`, `d0e0bcb`; D-84 to
D-92 updated):

- Fields nothing read are removed: the line event's failure reason and
  the line record's lost flag (D-84), the obstacle event's give-up
  reason (D-90), a fault counter in Telemetry's state (D-92).
- Narrower fields: a Motion move's value is its size in 15 bits, with a
  build check on every value Navigation sends (D-88); the echo width is
  15 bits (D-90); the broker address is 9 bits per part, two words
  instead of three (D-91).
- Build checks completed: every coarse scan angle within the servo's
  range (D-90).
- Tests run the code the car runs: the encoder add (S2-U42) and the
  echo clamp (S5-U42). Edge offsets past 2^21 µs are read back (S3-U44,
  S3-U49). Random scans use every angle (S5-U40, S1-U44).
- Wording: the bounds that rely on the watchdog hold once it is armed,
  radio up or 15 s after boot (D-85, D-88); the counter removed from
  the Line status counted failed barcode captures, not failed reads
  (D-87, D-92).

The changes the reviewers made to show a gap (a change that would
break the car but passed the tests) now fail a test or the build: an
offset's bit 21 dropped, a coarse angle past 180°, a 360° search sweep,
an encoder add that loses its last pair or adds one wheel twice, and an
echo width cut instead of kept at its largest (the clamp is now the
ISR's shared packing, which S5-U42 runs). One gap stays, as the
reviewer advised: whether each task fills its telemetry record with the
right values is checked only by reading the code (the tasks are not in
the host build); an on-target check of one message per task would close
it.

## 10. External review (23 Oct)

An outside review of `1d75e59` was checked against the code
(`decisions.md` section J). It found no packing fault on valid input.
Changed (`5cddd0a`, `7bcbf91`, `b1d23ec`): Motion's API refuses a
distance or angle too large for its field instead of sending it
shortened (D-88); each GPIO status read has its own time stamp (D-93);
S3-U20 says what a sample count shows; the timing probe marks only the
Line task's 5 ms releases; what each measurement figure includes is
written down (`testing/README.md`). G19 and G20 were decided the same
day and are done (`551329e`, `dd3a336`): while running, Navigation
stops using a line record whose sample count has not moved for 20 ms
and reports a LINE_DATA fault (D-94, NAV-U40); a fault while RESET
waits for Motion cancels the reset and becomes the latched cause
(D-95, S4-U07). Left for a decision: G21, G22.

A second outside review, of `207f498` (`decisions.md` section K),
confirms those fixes and found two more points, both fixed. Line
reads failing for good could survive a RESET made within 200 ms of the
failures, because line health was published only at the end of each
200 ms window: it now drops at the first bad sample and comes back only
after a whole good window, and the failed-read count starts again
while the Line task is idle after COMPLETE (D-96; S3-U19, NAV-U41).
G19's 20 ms is now
measured as elapsed time, so a late Navigation tick does not stretch it
(D-94, NAV-U40).

A third outside review, of `342f4a5` (`decisions.md` section L),
confirms those fixes and found one more gap: after COMPLETE, a fault
and RESET, SELF-TEST could pass while the Line task was still idle and
had read nothing. Idle cycles now drop the line health, and the
restart into SELF-TEST wakes the Line task, so SELF-TEST waits for a
whole window of new good reads (D-98, NAV-U42).

## 11. Team member's review (24 Oct)

A team member reviewed `0ff78e1` (`decisions.md` section M). For this
page: the fault mask that Change Requirement 1 names is built as the
team agreed after Tribunal 1 (Version C): 11 causes, each one bit with
a latch flag in one table (`imu_msgs.h`); the fault report's cause
field is 4 bits; Wi-Fi loss and no echo are status bits that never
latch; Safety is the only writer, fed by the fault reports, so no
read-modify-write on the mask can race; a copy goes to Telemetry in
Safety's status record and the heartbeat (D-102 to D-104; S4-U08,
S4-U41, S1-U42, S1-U43). Motion now reports a motor stall or an encoder
fault, the two bits the code could not raise before (D-105; S2-U07, and
S2-U38 on the car). The SELF-TEST check of the ultrasonic sensor asks
only that it answers (D-104). Also fixed: speeds and the line-lost
timer use measured time (D-99), the PID never drives against its
target (D-100), a STOP is never lost (D-101), start-up failures leave
the car still (D-106), and Safety's queue-full count stops at 255.
Left open: G22 to G25, and the stored state of most subsystems
(section 6).
