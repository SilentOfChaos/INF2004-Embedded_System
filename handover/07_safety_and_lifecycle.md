# 07 — Safety and lifecycle (coverage E)

Tags and source keys: see `00_README.md`. Read from REPO `58f7d61`
(`subsystem4/safety_task.c`, `navigation/nav_sm.c`, `nav_types.h`,
`docs/decisions.md`). [Verified: REPO] unless tagged otherwise. **None of
this has been run on hardware**; the watchdog, override and RESET paths have
no test result (10).

## 1. System modes (report section 5, Fig 5)

| Mode | Entered when | Leaves when | Motors |
|---|---|---|---|
| BOOT | power-on | tasks created → SELF-TEST | off |
| SELF-TEST | after BOOT; after a FAULT is released | Line, IMU, Scan healthy within 3 s → READY; else FAULT `SELF_TEST` (D-39, D-57) | off |
| READY | self-test passed; RESET in SAFE HOLD | START button GP20 (FR16; no MQTT START, G2) | off |
| LINE FOLLOWING / HUMP CROSSING / LINE SEARCH / AT JUNCTION / AVOIDING | START; then by events (06 §1) | see 06 §1 | commanded by Navigation |
| SAFE HOLD | junction with no command (FR5); line not found 3 s after a turn; NO SAFE ROUTE (FR11) | RESET (GP21) with no fault latched → READY (D-64) | stopped |
| COMPLETE | line search ran out (5 s, FR18) → run summary | **no exit until power-off** (D-64; G12 open) | stopped |
| FAULT | Safety latched a fault (IF-16) | RESET with cause cleared and Motion's acknowledgement → SELF-TEST | braked, pins forced low |

## 2. Fault causes and sources

[Verified: `subsystem4/imu_msgs.h`]

| Cause | Code | Raised by | Requirement / decision |
|---|---|---|---|
| `FAULT_CAUSE_COLLISION` | 1 | IMU: horizontal jolt above `CFG_IMU_IMPACT_MG` 1000 mg, or (since 16 Oct) one sample above `CFG_IMU_MAX_G_MG` 3500 mg while MONITORING, detail = abs(a) in mg | FR15; D-81 |
| `FAULT_CAUSE_STOP_CMD` | 2 | Command: MQTT `stop` | FR14, FR15 |
| `FAULT_CAUSE_IMU_DATA` | 3 | IMU: 5 invalid samples in a row (failed read or abs(a) < 300 mg), in MONITORING and CALIBRATING | report 6.4; D-42, D-76 |
| `FAULT_CAUSE_SELF_TEST` | 4 | Navigation: SELF-TEST timed out | report 5; D-39 |
| `FAULT_CAUSE_QUEUE_FULL` | 5 | any sender of a full control queue | report 4; D-68 |
| `FAULT_CAUSE_LOST_RECORD` | 6 | Safety itself, when a fault record could not be queued (`SAFETY_FLG_FAULT_LOST`) | — |
| `FAULT_CAUSE_LINE_DATA` | 7 | Line: 4 failed ADC reads in a row, detail = `-MERCD(ercd)` (added 16 Oct) | D-79 (G15) |

Sources: `FAULT_SRC_LINE` 1, `NAV` 2, `IMU` 3, `MOTION` 4, `SCAN` 5,
`COMMAND` 6, `TELEMETRY` 7, `SAFETY` 8.

## 3. Latch (FR15, NFR6, NFR16)

`safety_task` (priority 5, core 0) wakes on its 10 ms tick and at once on a
fault record (`mbf_fault` + `SAFETY_FLG_WAKE`). Comment in the code:
"Faults first: the override goes on within this wake-up (NFR6)." On the
**first** fault (`safety_latch()`):

1. `safety_pins_low(true)`: GP8–GP11 forced low through the IO_BANK0
   `GPIO_CTRL` output override (OUTOVER = drive low, OEOVER = enable). By
   the board's truth table, both inputs low = **brake** (04 §1). Switch:
   `CFG_SAFETY_PIN_OVERRIDE` 1 (D-15, D-45). This is template exception
   **A14, pending instructor agreement** [Verified: TEAM.md §7].
2. Cause, source and detail stored; the `SAFETY_FLG_FAULTED` level set, so
   Motion holds BRAKED and Navigation stops commanding (IF-16).
3. Alarm on: buzzer 2 kHz, red LED, 250 ms on / 250 ms off (D-47).
4. Fault record to Telemetry (`"ev":"fault"`, `"on":1`) and a console line.

Later faults while latched only increase counters (`faults`, `queue_full`);
the first cause stays. The override is re-applied every 10 ms tick while
latched.

## 4. RESET (SD2; IF-09; report 6.4)

RESET is GP21, polled every 10 ms, debounced 30 ms, acts once per press
(D-21, D-65).

- **Fault latched**: `safety_cause_cleared()` — for COLLISION and IMU_DATA
  the IMU must report healthy data (`SAFETY_FLG_IMU_OK`, set by
  `safety_set_imu_ok()`); every other cause, LINE_DATA included, counts as
  cleared, and SELF-TEST re-checks the sensors afterwards (D-79). If not
  cleared: "safety: RESET refused, cause still present". If cleared:
  `motion_request_reset()`; Motion clears its queue, command and PID state
  and acknowledges (`safety_ack_zero()`); on the acknowledgement
  `safety_release()` removes the override, clears FAULTED, LED green, sends
  `"on":0`, and sets `NAV_FLG_RESTART` → SELF-TEST. No acknowledgement within
  500 ms (`CFG_SAFETY_ACK_TIMEOUT_MS`, D-46): the request is dropped, the
  fault stays latched ("safety: no acknowledgement from Motion").
- **No fault latched**: Navigation is told (`NAV_FLG_RESET`): SAFE HOLD →
  READY.

## 5. Watchdog and check-ins (report section 3; D-44)

- Line, Navigation, Motion, IMU and Scan set a check-in bit once per cycle
  (IF-18). Every 10 ms Safety reads and clears the bits in one atomic call
  and compares each task's last check-in with its deadline: Line 50 ms,
  Navigation 50 ms, Motion 100 ms, IMU 50 ms, Scan 325 ms.
- The hardware watchdog (timeout 200 ms) is armed only after the radio
  driver reports its start-up complete, or after 15 s (A1;
  `CFG_WDT_ARM_MAX_WAIT_MS`, UNVALIDATED), and is fed only while every
  task is within its deadline. A missed deadline is counted (`miss` in the
  heartbeat).
- Command and Telemetry do not check in (the car never waits for them,
  NFR10). [Verified: `g_checkin_bits` has 5 entries]

## 6. Degraded-data rules that feed the latch (all decided; owners to confirm the 16 Oct ones)

| Data | Rule | Decided by | Status |
|---|---|---|---|
| Line ADC read fails | record invalid at once; 4 in a row → LINE_DATA | user, 16 Oct (G15 → D-79) | implemented, host-tested S3-U18; S3 owner to confirm |
| Accelerometer overrange | > 3.5 g while monitoring → collision at once; while calibrating → restart, no fault | user, 16 Oct (G17 → D-81) | implemented, host-tested S4-U06; S4 owner to confirm |
| Invalid IMU data | 5 in a row → IMU_DATA, in every IMU state | report 6.4; teammate fix 13 Oct (D-76) | implemented; manual S4-U37 not run |
| Scan readings invalid | repeated once; unseen side = no room; path clear needs 5 valid coarse readings; 3 invalid in a row → NO SAFE ROUTE (SAFE HOLD, not FAULT) | user, 16 Oct (G16 → D-80); report 6.5 | implemented, host-tested S5-U06; S5 owner to confirm |
| Full control queue | fault QUEUE_FULL | report 4 (D-68) | implemented |

"Decided by the user" means the one team member running this session
chose the recommended option in an AskUserQuestion on 16 Oct [Verified: TR
L5712–L5713]. Team agreement: [Unknown]. Instructor approval: not sought.

## 7. What is still unproven

[Verified: test logs empty] NFR6 (brake within the stated time), the
override actually braking the Robo Pico driver, the watchdog arming and
feeding under load with Wi-Fi, the RESET handshake, and the alarm are
defined as tests (S4-U30…S4-U38, INT-xx, SYS-xx) with no result. A14 has
no recorded instructor approval.
