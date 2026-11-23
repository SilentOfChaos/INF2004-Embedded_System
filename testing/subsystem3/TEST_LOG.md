# Subsystem 3 — test log

**Subsystem:** line following, junctions, barcodes  ·  **Owner:** Nga Wei Yang  ·  **Code:** `app_program/subsystem3/`

See `testing/README.md` for how to run tests and record results.

Design-note test IDs (`UT-B3-xx`, docs/notes/05 section 11) are given next to the matching S3 tests.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S3-U01 | all 26 letters forwards (design note UT-B3-01) | FR2 | host | `testing/host/test_s3.c` | `[TEST] S3-U01 PASS` |
| S3-U02 | all 26 letters backwards: same letter, marked reversed; Z, K, O never read as T, U, X (UT-B3-02, UT-B3-03) | FR2 | host | `testing/host/test_s3.c` | `[TEST] S3-U02 PASS` |
| S3-U03 | speed change of ±25% across one barcode (UT-B3-04) | FR2 | host | `testing/host/test_s3.c` | `[TEST] S3-U03 PASS` |
| S3-U04 | comparator bias: bars +40%, spaces −40% and the opposite (UT-B3-05) | FR2, D-27 | host | `testing/host/test_s3.c` | `[TEST] S3-U04 PASS` |
| S3-U05 | no letter from wrong counts, non-letters, flat widths, wide gaps, wrong ends (UT-B3-06, UT-B3-07) | FR2 | host | `testing/host/test_s3.c` | `[TEST] S3-U05 PASS` |
| S3-U06 | capture: letter just after the 30th edge, not 120 ms later; the next bar starts a new capture | D-26, report 6.3 | host | `testing/host/test_s3.c` | `[TEST] S3-U06 PASS` |
| S3-U07 | capture: glitches under 1 ms removed, also on the first bar | D-25 (approved glitch filter) | host | `testing/host/test_s3.c` | `[TEST] S3-U07 PASS` |
| S3-U08 | capture starts at the first bar (`CFG_BC_QUIET_MS` 0); with the optional quiet zone set, only after that much white | report 6.3, D-25 | host | `testing/host/test_s3.c` | `[TEST] S3-U08 PASS` |
| S3-U09 | capture: crossbar, too few, extra and lost edges give no letter (UT-B3-06, UT-B3-08) | FR2, D-26 | host | `testing/host/test_s3.c` | `[TEST] S3-U09 PASS` |
| S3-U10 | capture: edge and time limits always end it | D-26 | host | `testing/host/test_s3.c` | `[TEST] S3-U10 PASS` |
| S3-U11 | line error sign and limits (UT-B3-09) | FR1, IF-02 | host | `testing/host/test_s3.c` | `[TEST] S3-U11 PASS` |
| S3-U12 | one junction per crossbar; line lost after exactly 300 ms; found again | FR3, FR18 | host | `testing/host/test_s3.c` | `[TEST] S3-U12 PASS` |
| S3-U13 | level adaptation follows slowly, stays within its limits, frozen when off | D-23 | host | `testing/host/test_s3.c` | `[TEST] S3-U13 PASS` |
| S3-U14 | steering: sign, gains, limit, slow-down | FR1 | host | `testing/host/test_s3.c` | `[TEST] S3-U14 PASS` |
| S3-U15 | letter map: defaults, remap, bad input | FR6 | host | `testing/host/test_s3.c` | `[TEST] S3-U15 PASS` |
| S3-U16 | capture: an edge lost inside the symbol (queue overflow before the 30th edge is taken, levels that do not alternate, overflow while the first bar is still queued) fails the read even if the widths decode; 5 ms Line cycles as in `line_task.c` | FR2, D-78 | host | `testing/host/test_s3.c` | `[TEST] S3-U16 PASS` |
| S3-U17 | capture: edges and losses after the 30th edge (loss reported after all 30 are stored, a same-level edge, trailing edges, a full buffer) do not stop a complete symbol from decoding | FR2, D-78 | host | `testing/host/test_s3.c` | `[TEST] S3-U17 PASS` |
| S3-U18 | failed line-sensor reads: one report on the 4th failure in a row, none for a longer run, a good read starts the count again | FR1, report 6.3, D-79 (G15) | host | `testing/host/test_s3.c` | `[TEST] S3-U18 PASS` |
| S3-U19 | line health (`line_keep_note_sample`): the first bad sample of a 200 ms status window (a failed or out-of-range read) drops the published health at once, once per window; it comes back only at the end of a whole window of good samples | FR1, IF-17, report 5 (SELF-TEST), D-96 | host | `testing/host/test_s3.c` | `[TEST] S3-U19 PASS` |
| S3-U40 | IF-02 line record packed into two words: the exact expected bits; every error from −1000 to 1000 and the reading ends round-trip, the spare bits stay 0; a failed read keeps the fields; 200 samples counted across the 8-bit wrap; impossible values kept at the nearest end and reported (no lost flag: nothing read it, D-84) | IF-02, D-84 (minimum-bit change) | host | `testing/host/test_s3.c` | `[TEST] S3-U40 PASS` |
| S3-U41 | IF-01 line event packed into 16 bits: the exact expected bits; every type, letter and direction round-trips, the 7 spare bits stay 0; impossible values reported (no failure reason: nothing read it, D-84) | IF-01, D-84 | host | `testing/host/test_s3.c` | `[TEST] S3-U41 PASS` |
| S3-U42 | barcode edge record packed into one word: the exact expected bits; the Line task rebuilds the exact 32-bit time for 200,000 random edges up to 2^31 µs old, also across the clock's wrap | NFR2, D-85 | host | `testing/host/test_s3.c` | `[TEST] S3-U42 PASS` |
| S3-U43 | barcode capture before and after packing, side by side (`testing/host/ref/` holds the old code): 4000 random runs of letters, glitches, lost and extra edges, chatter, queue losses and long gaps, every edge passed through the packed edge record, 5 ms Line cycles with 1 ms jitter: the same output at every poll and the same state after every call; every outcome and every failure reason the decoder can give is reached | FR2, D-78, D-85 | host | `testing/host/test_s3.c` | `[TEST] S3-U43 PASS` |
| S3-U44 | barcode capture state packed: the exact expected words; the quiet zone kept in ms; an edge 2^22 − 1 µs after the first bar is stored, one 2^22 µs after it ends the capture as too long; a slow letter (2.2 s) with the Line task held up 0.26 s from 1.95 s: its last two edges are stored 2.146 and 2.204 s after the first bar (bit 21 of the offset set), a flicker between them is dropped and the letter is read, as by the old version (on time: too long at 2 s); also across the clock's wrap | D-85 | host | `testing/host/test_s3.c` | `[TEST] S3-U44 PASS` |
| S3-U45 | line tracking state packed into three words: the exact expected words; the packed and the old version (`testing/host/ref/`) agree on the event, error, flags, all four levels and the failed-read count after every sample of 600 random runs of 2000 samples (on the line, crossbars, white, one side, drifting light, any reading, scale ends, any period, runs of failed reads) | FR1, FR3, FR18, D-79, D-86 | host | `testing/host/test_s3.c` | `[TEST] S3-U45 PASS` |
| S3-U46 | steering state packed into 16 bits: the exact expected bits; the packed and the old version give the same speed and difference for 200,000 random errors (beyond ±1000 too), speeds and resets | FR1, D-86 | host | `testing/host/test_s3.c` | `[TEST] S3-U46 PASS` |
| S3-U47 | letter map packed as 26 × 3 bits: the exact expected words, including a letter split across two words; the packed and the old version agree on 100,000 random remaps (bad letters and actions too) and reads | FR6, D-86 | host | `testing/host/test_s3.c` | `[TEST] S3-U47 PASS` |
| S3-U48 | what the Line task keeps between cycles, packed into two words: the exact expected bits; over 20,000 random cycles the status falls due on the same cycles as `cycle % 40 == 0` and the letter count equals the old 32-bit counter taken modulo 256, through several wraps (failed barcode captures are no longer counted: nothing read that count, D-92) | FR12, D-87, D-92 | host | `testing/host/test_s3.c` | `[TEST] S3-U48 PASS` |
| S3-U49 | barcode capture before and after packing, side by side, with the Line task held up: 1500 random runs of 4 slow symbols (1.9–2.2 s) each, the Line task held up for up to 0.27 s (the most the armed watchdog allows) from 1.9 to 2.0 s after the first bar while edges keep arriving; edges are stored up to 2.2 s after the first bar (bit 21 of the offset), letters are read from them and captures end at the limit: the same output at every poll. Fails if bit 21 is dropped when an offset is stored or read | D-85 | host | `testing/host/test_s3.c` | `[TEST] S3-U49 PASS` |
| S3-U20 | line-error record sampled at a 5 ms period on average: the samples counted over a measured window of about 1 s (printed) are within ±5% of the window over 5 ms. A count shows the average rate only; each 5 ± 1 ms interval is S3-U33 | NFR1, IF-02 | auto | `subsystem3_test.c` | `[TEST] S3-U20 PASS` |
| S3-U21 | both ADC readings in the plausible range | D-24 | auto | `subsystem3_test.c` | `[TEST] S3-U21 PASS` |
| S3-U22 | line error stays within −1000..1000 | IF-02 | auto | `subsystem3_test.c` | `[TEST] S3-U22 PASS` |
| S3-U30 | black and white ADC levels of both sensors | A3, D-63 | manual | Read the raw values on the console over the line and over the floor, at both lighting levels. | levels recorded; `CFG_LINE_*` updated |
| S3-U31 | barcode sensor D0 level over a bar | A3 | manual | Read GP7 with a bar under the sensor. | `CFG_BC_DARK_LEVEL` confirmed or changed |
| S3-U32 | 30 edges per pass of the sample barcode at 3 speeds | A3, NFR2 | manual | Log GP7 on a logic analyser while pushing the car over the barcode at 0.15, 0.25, 0.35 m/s. | 30 edges per pass; no extra edges after the glitch filter |
| S3-U33 | Line period and CPU load with the 1 ms tick | NFR1, NFR13, A12 | manual | Claim GP6, set `CFG_PIN_TIMING_PROBE` 6 (it toggles at each 5 ms release only); logic analyser for 10 s, with and without the Wi-Fi build. | 5 ± 1 ms |
| S3-U34 | barcode edge timing under encoder interrupt load | A8, NFR2 | manual | Feed a known square wave into GP7 while both wheels run at full speed. | width error under 1% of a narrow bar at 0.35 m/s |
| S3-U35 | GP7 with the barcode sensor unplugged, then plugged in | D-73, NFR11 | manual | Unplug the Grove 7 cable and read GP7 on the console; plug it in and scope D0 over a bar and over white. | unplugged: reads white (no capture starts); plugged: clean 0–3.3 V levels |
| S3-U36 | chatter on GP7 never gives a letter | FR2, A3, D-78 | manual | Sensor unplugged; drive GP7 with 3.3 V logic only (NFR11) from a signal generator or another microcontroller: the 30 edges of the sample `*B*` at 0.25 m/s timing, once clean and once with a 3 ms burst of 50 kHz edges inside the last bar. Watch `bc` and `last` in the telemetry (or its console echo, D-69): failed reads are not published, so a failed pass shows as no new letter. | clean pass: `bc` up by 1, `last` B; pass with the burst: `bc` unchanged, never a wrong letter |

*Method:* **host** = host unit test, built and run with `make` in
`testing/host/` (no hardware; see `testing/README.md`);
**auto** = `TEST_CHECK` in the on-target test image (`make TEST=1`), result
printed on the console; **manual** = a person observes or measures the
result (scope, logic analyser, ruler, broker client, phone app ...).

## Results

Add a new row for every run. Never edit or delete old rows. No test has
been run on the car yet.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | S3-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
