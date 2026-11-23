# IS19 Firmware Changes (dashboard package)

These are the 16 files in `firmware_changes/`. They add four things. `app_config.h`, the tuning values and the sensor drivers are unchanged.

The full diff is in `dashboard_firmware.patch`, in the same zip.

---

## 1. START and RESET buttons on the dashboard (Telemetry / commands, Buddy 1)

| File | Change |
|---|---|
| `app_program/subsystem1/tlm_msgs.h` | Added command codes `CMD_CODE_START` (6) and `CMD_CODE_RESET` (7), and a new route event kind `ROUTE_BC_FAIL` (11) |
| `app_program/subsystem1/tlm_msgs.c` | Updated the bit-size checks so the new codes fit the packed messages |
| `app_program/subsystem1/cmd_parse.c` / `cmd_parse.h` | The parser now accepts `"start"` and `"reset"` from MQTT `inf2004/IS19/cmd` |
| `app_program/subsystem1/command_task.c` | `start` calls `nav_request_start()` and `reset` calls `safety_request_reset()`; both send an ack back |
| `app_program/subsystem1/tlm_format.c` | Adds the names `"start"` and `"reset"` (for acks) and `"bc_fail"` (for events) to the JSON output |

## 2. Remote START and RESET act like the physical buttons (Navigation and Safety)

| File | Change |
|---|---|
| `app_program/navigation/navigation.h` / `navigation.c` | New flag `NAV_FLG_REMOTE_START` and function `nav_request_start()` |
| `app_program/navigation/nav_task.c` | Each tick, if that flag is set, it's treated the same as pressing the START button (GP20) |
| `app_program/subsystem4/subsystem4.h` / `safety_task.c` | New flag `SAFETY_FLG_REMOTE_RESET` and function `safety_request_reset()`; it's treated the same as pressing RESET (GP21) |

## 3. Fixes for problems seen on the track

| File | Change | Why |
|---|---|---|
| `app_program/navigation/nav_sm.c` | New `nav_sm_fallback_bypass()`. When the scan reports **NO_SAFE_ROUTE** after reversing, the car now goes round on the default side instead of entering SAFE HOLD | The car measured the obstacle width and then stopped in SAFE HOLD |
| `app_program/navigation/nav_sm.c` | When a barcode capture fails, it now logs a `bc_fail` event | So the dashboard can show "barcode seen but not decoded" |
| `app_program/navigation/nav_task.c` | New `nav_lift_watch()`, which is true while running, in **SAFE HOLD** and in **COMPLETE** | Picking the car up wasn't detected once it had already stopped |
| `app_program/subsystem4/imu_task.c` | The lift report is now gated by `nav_lift_watch()` instead of `nav_is_running()` | Same lift fix as the row above |

## 4. Host tests updated to match

| File | Change |
|---|---|
| `testing/host/test_s1.c` | Tests for parsing `start`/`reset` and for the new names |
| `testing/host/ref/ref_tlm_format.c` | The reference formatter gets the same new names |
| `testing/host/test_nav.c` | NAV-U16 extended: NO_SAFE_ROUTE after reversing now goes round, while NO_SAFE_ROUTE during line following still goes to SAFE HOLD |

---

## Note

These files are in other buddies' subsystems (Telemetry, Navigation, Safety/IMU), so tell those owners about the changes before merging.
