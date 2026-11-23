# Cross-check of the W6 design against the sources

What was checked: every requirement against the write-up, briefing and
transcripts; the pin map against the Robo Pico port table and PWM slices; IMU,
ultrasonic and IR claims against their datasheets; the barcode claims against
the measured sample; the method against Lectures 5 and 6.

## Confirmed (no action)

- Default mapping A straight, B left, C right, D U-turn matches what the prof
  said (PT L146); the write-up's example table differs, so A7 and MQTT remap
  are the right hedge.
- Act at the junction, decode A–Z both ways, MQTT remap without recompiling,
  summary on request, route log, 5-minute run, end the run when the line ends:
  all match the prof's statements.
- Pin map: every device sits on a port that has the needed function (ADC on
  GP26/27, I2C1 on GP2/3, motors on slices 4/5, servo on 6A). Grove 5/6 GP26
  sharing and the Maker-port sharing are handled.
- LSM303DLHC claims: no accelerometer WHO_AM_I (correct), magnetometer
  identity registers, 100 Hz accelerometer and 75 Hz magnetometer are valid
  rates.
- Brake = both driver inputs low (Robo Pico truth table); both high is coast.
- Power bank label really says 5 V 2.1 A per output; HC-SR04P is the 3.3 V
  capable variant (printed on the board).
- Code 39 structure, 3:1 ratio and ~3 mm / 9 mm bars at 141 mm: consistent
  with the measured sample.

## Gaps and inconsistencies

| # | Finding | Why it matters | Suggested fix | Owner |
|---|---|---|---|---|
| G1 | NFR14 says the broker address can change without reflashing, but if the car cannot reach a broker it cannot receive an MQTT command to change it. Demo-day Wi-Fi SSID/password may also differ, and recompiling is not allowed on the day | The car could be unable to send any telemetry at the demo | Out-of-band path: USB console command that stores Wi-Fi and broker settings in flash, or a list of candidate networks/brokers tried in turn | B1 |
| G2 | FR16 requires pressing START after power-on; the consultation said the car cannot be touched once placed and switched on | Possibly fine (placing and starting are one moment), but unconfirmed | Ask; optionally also accept START over MQTT | B1, Nav |
| G3 | At every cross junction the crossbar passes under the barcode sensor and starts a capture; Navigation would request `holdSpeed()` mid-manoeuvre and later get "capture failed" | Interferes with the junction turn; a careless implementation could clear the held letter | Navigation ignores barcode events in AT JUNCTION; "capture failed" never clears a held letter | B3, Nav |
| G4 | FR5: one failed read means no command at the junction, so SAFE HOLD and the attempt is over | A single misread costs the run; only the last of 3 attempts counts | Decide: keep, or reverse slowly and re-read (decoding works both ways), or another fallback. Be ready to defend it | Team |
| G5 | Only two shared variables are justified (encoder counters, line-error record), but there are two more ISR hand-offs: barcode edge times (ISR and Line task both on core 0) and echo times (Echo ISR on core 0, Obstacle Scan on **core 1**) | The design's own rule says cross-core data goes through kernel objects; NFR15 says no unjustified globals | Check which µT-Kernel calls the port allows from interrupt context; use them, or justify a single-producer ring buffer per hand-off | B3, B5 |
| G6 | Interface arrows in Fig 2 have names but no IDs; there is no traceability matrix yet | Lecture 5/6: every box and arrow gets an ID; integration tests exercise exactly those interfaces; the final run marks testing | Number tasks D1–D8 (already done by priority) and interfaces I1…; build Req → design → test table | Team |
| G7 | During HUMP CROSSING the chassis rolls; the front line sensors change height and the TCRT5000 output drops off sharply with distance (peak 2.5 mm, usable to 15 mm) | Line readings can look like "lost" or bias the error on a hump; adaptive levels could learn wrong values | Freeze level adaptation while Navigation is in HUMP CROSSING; test line tracking over a hump (already in A10's method) | B3, B4 |
| G8 | The kit's Raspberry Pi Debug Probe is not used anywhere in the design | SWD debugging and a UART console help with Heisenbugs and the tests the prof wants | Plan SWD debugging and UART logging; keep the GP0/GP1 crash-report conflict in mind | Team |
| G9 | Power bank: besides the 2.1 A limit (A6), many power banks switch off at light load | The car may lose power while idle in READY or COMPLETE, or reset on motor start | Add to A6 tests: idle for several minutes; both motors starting together; soft-start ramp in Motion | B2, Team |
| G10 | Encoder interrupt rate depends on counts per turn (A2), still unknown | Feeds A8 and CPU load (NFR13) | After measuring A2, compute edges per second at top speed; consider the PIO quadrature decoder from pico-examples (also earns PIO credit) | B2 |

## Top risks

1. One barcode misread ends an attempt (G4), and the last attempt counts.
2. No telemetry on demo day because of network configuration (G1).
3. Power: resets on motor start or power-bank auto-off (A6, G9).
4. Template exceptions not approved in time: priorities above comms, Safety
   pin override, MQTT message buffers, 1 ms tick (A11, A12, A14).
5. Integration time: the W10 check-in is a hurdle (5% per day if missed) and
   needs something working on the car.

## Questions to ask the instructors (ideally before Mon 19 Oct)

1. Which side are the barcodes on, and the hump? (Promised at kit issue.)
2. Are all junctions cross junctions, and does every junction have a barcode
   before it? What should happen at a junction without a valid read?
3. Is pressing the on-board START button after placing the car allowed?
4. Demo-day network: which SSID and broker, and may we set them over USB
   before the run (no recompile)?
5. Height of the printed bars and the typical barcode-to-junction distance on
   the real course (A13).
6. Hump height range and shape; is the hump ever near a junction or barcode?
7. Approval for the TEAM.md exceptions (A11, A14), the team's priority order
   and the 1 ms tick (A12).
8. Which µT-Kernel services the port allows from interrupt handlers.
9. Is the course a loop or does the line end (the run-end design depends on it)?
10. Obstacle size range and the free space on each side.

## Tribunal 1 checklist (Mon 19 Oct, 11:00)

- [ ] Everyone reads the whole design, not just their own part.
- [ ] Each buddy can explain their §8.1 row, their assumptions, their
      statechart, and their task's period and priority.
- [ ] All five set up local recording and test it before Monday (no video =
      zero).
- [ ] Each buddy has an AI tool ready to screen-share; a short shared prompt for
      impact analysis is agreed in advance.
- [ ] Impact-analysis template ready, per buddy: affected requirement IDs,
      affected tasks / statecharts / pins / interfaces, new or changed
      assumptions, tests to add or rerun; or "not affected, because …".
- [ ] Spare hardware list at hand for hardware changes: Grove 5 GP6, GP28 on
      Grove 7, servo ports 2–4 (GP13–GP15).
- [ ] Watch xSITe for the Tribunal 1 template (the prof said it comes 1–2
      weeks before).
- [ ] Decide G4 (failed read) and G1 (demo-day network) as a team, so the
      answer is the same whoever is asked.
