# Subsystem 1 — test log

**Subsystem:** Wi-Fi / MQTT commands and telemetry  ·  **Owner:** Ang Hung Ling  ·  **Code:** `app_program/subsystem1/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S1-U01 | each command form parses with its fields | FR6, FR14, NFR14, D-66 | host | `testing/host/test_s1.c` | `[TEST] S1-U01 PASS` |
| S1-U02 | bad commands are rejected; `seq` kept for the ack | FR14, D-66 | host | `testing/host/test_s1.c` | `[TEST] S1-U02 PASS` |
| S1-U03 | JSON field readers stay inside their buffers | NFR15 | host | `testing/host/test_s1.c` | `[TEST] S1-U03 PASS` |
| S1-U04 | IPv4 address check for the broker command | NFR14 | host | `testing/host/test_s1.c` | `[TEST] S1-U04 PASS` |
| S1-U05 | JSON writer: exact output; overflow gives 0, never a cut message | D-49 | host | `testing/host/test_s1.c` | `[TEST] S1-U05 PASS` |
| S1-U06 | route log: totals, indexes, full log, new run clears it | FR13 | host | `testing/host/test_s1.c` | `[TEST] S1-U06 PASS` |
| S1-U07 | run summary: time, distance, how the run ended | FR14, FR18 | host | `testing/host/test_s1.c` | `[TEST] S1-U07 PASS` |
| S1-U08 | every message (obstacle profile included), with every field at the extreme its packed record can hold, fits `CFG_MQTT_PAYLOAD_MAX`; the longest, telemetry, is 249 bytes | D-49, D-92 | host | `testing/host/test_s1.c` | `[TEST] S1-U08 PASS` |
| S1-U09 | route entries carry the fields of their kind | FR13 | host | `testing/host/test_s1.c` | `[TEST] S1-U09 PASS` |
| S1-U10 | unknown codes print as "?", never read out of range; the four new fault causes print their names | NFR15, D-102 | host | `testing/host/test_s1.c` | `[TEST] S1-U10 PASS` |
| S1-U11 | obstacle profile event: position, width, closest point, both clearances, chosen side; too small a buffer gives 0 | FR10, D-72 | host | `testing/host/test_s1.c` | `[TEST] S1-U11 PASS` |
| S1-U12 | a STOP is recognised from the raw payload, with or without `seq`, so a full command queue cannot lose it; other and cut-off commands are not | FR14, FR15, D-68, D-101 | host | `testing/host/test_s1.c` | `[TEST] S1-U12 PASS` |
| S1-U40 | IF-15 broker record packed into two words, each address part's spelling in 9 bits: the exact expected bits; all 366 spellings of a part in every position, and 10,000 random broker commands (leading zeros included), come back character for character with their port; a bad or too long address is reported and kept as 0.0.0.0 | IF-15, NFR14, D-91 (minimum-bit change) | host | `testing/host/test_s1.c` | `[TEST] S1-U40 PASS` |
| S1-U41 | IF-10 Navigation command packed into two words: the exact expected bits; 26,000 parsed remap and default-map commands (every letter and action, `seq` up to 999,999,999) read back exactly as the old record carried them; an invalid type, letter or action is stored as a remap of no letter and reported | IF-10, FR6, D-91 | host | `testing/host/test_s1.c` | `[TEST] S1-U41 PASS` |
| S1-U42 | IF-12 records packed: the exact expected bits of every record type (line, Motion, IMU, Scan, Navigation, Safety with its fault mask, route entries of each kind, ack, fault with its 4-bit cause, link, obstacle profile); the message is 20 bytes; values outside a field are reported and kept at the nearest end | IF-12, D-92 (minimum-bit change), D-102 | host | `testing/host/test_s1.c` | `[TEST] S1-U42 PASS` |
| S1-U43 | the packed records give byte-identical MQTT messages to the old records and formatter (a frozen copy in `testing/host/ref/`): 20,000 random states (telemetry and heartbeat), 20,000 random events of each kind (route entries with the run totals and summary, acks, faults, links, obstacle profiles) and the route-log replay; random values in the dropped fields change nothing; the one addition is the heartbeat's `"mask"` after `"cause"` (D-102) | IF-12, FR12, FR13, D-92, D-102 | host | `testing/host/test_s1.c` | `[TEST] S1-U43 PASS` |
| S1-U44 | every obstacle profile the scan code makes from 20,000 random scans (readings every 6° from 0 to 180°) fits the packed profile record and reads back exactly | IF-12, FR10, D-92 | host | `testing/host/test_s1.c` | `[TEST] S1-U44 PASS` |
| S1-U20 | Telemetry drains its input queue (seen empty within 500 ms) | IF-12 | auto | `subsystem1_test.c` | `[TEST] S1-U20 PASS` |
| S1-U21 | publish, command and broker queues and the flag exist | IF-13, IF-14, IF-15 | auto | `subsystem1_test.c` | `[TEST] S1-U21 PASS` |
| S1-U22 | nothing is queued for MQTT while the session is down | NFR10 | auto | `subsystem1_test.c` | `[TEST] S1-U22 PASS` |
| S1-U30 | telemetry and heartbeat rates on the broker | NFR9 | manual | 1. Build `NET=lwip MQTT=1`, flash. 2. `mosquitto_sub -v -t 'inf2004/IS19/#'` with time stamps for 60 s. 3. Count telemetry and status messages. | 270–330 telemetry, 54–66 status messages; `seq` without gaps |
| S1-U31 | remap acknowledged within 1 s and used at the next barcode | FR6, NFR8, IF-10 | manual | 1. Publish `{"seq":1,"cmd":"remap","letter":"E","action":"left"}` on `inf2004/IS19/cmd`. 2. Time the ack. 3. Pass barcode E. | ack `ok` 1 within 1 s; route event `act` "left" |
| S1-U32 | summary command: summary event, then the route log replayed | FR13, FR14 | manual | 1. Run part of the course. 2. Publish `{"seq":2,"cmd":"summary"}`. | ack, one summary event, then every logged entry with `re` 1 |
| S1-U33 | broker command moves the car to a second broker; reset goes back | NFR14, D-16 | manual | 1. Start a second broker. 2. Publish the broker command. 3. Watch both brokers. 4. Reset the Pico. | ack on the old broker, telemetry on the new one within a few s; after reset on the old one again |
| S1-U34 | broker unreachable: the car keeps running; retries back off; Wi-Fi loss is a status bit only | NFR10, report 6.1, D-70, D-103 | manual | 1. Stop the broker during a run. 2. Watch the console for 2 min. 3. Start it again. | mode unchanged and the car never brakes; retries after 1, 2, 4 … 30 s; telemetry resumes with `fault` 0, and `mask` loses 1024 (Wi-Fi loss) within a second of the link coming back |
| S1-U35 | the car's MQTT client ID is unique on the broker | FR12, NFR10, README | manual | Check that `MQTT_CLIENT_ID` in `config/wifi_secrets.h` is not the template's `mtk3smp-pico`. While the car publishes, connect `mosquitto_sub -i mtk3smp-pico -v -t 'inf2004/IS19/#'` for 1 min. | the car's telemetry keeps arriving; no MQTT reconnect on the car's console |

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
| YYYY-MM-DD | S1-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
