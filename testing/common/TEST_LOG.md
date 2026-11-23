# Common code — test log

**Part:** shared helpers  ·  **Owner:** whole team  ·  **Code:** `app_program/common/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| COM-U01 | clamp, absolute value and angle wrapping | D-13 | host | `testing/host/test_common.c` | `[TEST] COM-U01 PASS` |
| COM-U02 | atan2 within 0.03° over the full circle, any input size | D-13 | host | `testing/host/test_common.c` | `[TEST] COM-U02 PASS` |
| COM-U03 | sine and cosine within 3/32768 | D-13 | host | `testing/host/test_common.c` | `[TEST] COM-U03 PASS` |
| COM-U04 | Q15 multiply and division round to nearest | D-13 | host | `testing/host/test_common.c` | `[TEST] COM-U04 PASS` |
| COM-U05 | integer square root rounds down | D-13 | host | `testing/host/test_common.c` | `[TEST] COM-U05 PASS` |
| COM-U06 | table interpolation and its ends | D-13 | host | `testing/host/test_common.c` | `[TEST] COM-U06 PASS` |
| COM-U07 | bit packing: unsigned fields and flags give the exact expected words; a value too large is kept at the field's maximum and reported | D-83 | host | `testing/host/test_common.c` | `[TEST] COM-U07 PASS` |
| COM-U08 | bit packing: signed fields are two's complement in their width, every 11-bit value round-trips, out-of-range values are kept at the nearest end and reported | D-83 | host | `testing/host/test_common.c` | `[TEST] COM-U08 PASS` |
| COM-U09 | bit packing: differences of wrapping counters and time stamps across the wrap | D-83 | host | `testing/host/test_common.c` | `[TEST] COM-U09 PASS` |
| COM-U10 | bit packing: bit arrays with fields across word boundaries give the exact expected words; changing one field leaves the others | D-83 | host | `testing/host/test_common.c` | `[TEST] COM-U10 PASS` |
| COM-U11 | bits needed for a range; message buffer bytes for a number of messages (kernel's 4-byte header, messages rounded up to 4) | D-83 | host | `testing/host/test_common.c` | `[TEST] COM-U11 PASS` |
| COM-U12 | bit packing: a whole word back to `int32_t` with arithmetic only (both ends and 200,000 random words; storing it again gives the same word) | D-83, D-88 | host | `testing/host/test_common.c` | `[TEST] COM-U12 PASS` |

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
| YYYY-MM-DD | COM-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
