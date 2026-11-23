# Differences: `INF2004_IS19-main(dom)` vs `INF2004_IS19-main(mine)`

Compared on **19 Nov 2026** with a full recursive diff (`.git` and build outputs excluded). Where a setting or piece of code was changed more than once, only the final version is described. Anything identical in both versions is not mentioned.

**Summary:** 23 source/test files differ, plus 2 docs that exist only in (mine). Every change is in **(mine)**; (dom) is the baseline.

| # | File | Area | What (mine) changes |
|---|------|------|---------------------|
| 1 | `app_program/app_config.h` | Config | Retuned values and new settings (§1) |
| 2 | `app_program/app_main.c` | Startup | Calls `app_time_init()` first in `usermain()` (§2–5) |
| 3 | `app_program/common/app_time.c` | Timer | New `app_time_init()` that starts the 1 µs tick (§2–5) |
| 4 | `app_program/common/app_time.h` | Timer | Declares `app_time_init()` (§2–5) |
| 5 | `app_program/common/hw_regs.h` | Registers | Adds watchdog TICK register defines (§2–5) |
| 6 | `app_program/subsystem4/drv_i2c1.c` | IMU / I2C | I2C bus recovery (§6–7) |
| 7 | `app_program/subsystem4/imu_task.c` | IMU | Re-inits I2C1 if the ID check fails (§6–7); collision detection replaced by lift / overturn detection (§11) |
| 8 | `app_program/subsystem4/imu_calc.c` / `.h` | IMU | New lift / overturn detector (§11) |
| 9 | `app_program/subsystem4/imu_msgs.c` / `.h` | Faults | Fault cause 1 renamed COLLISION → LIFTED (§11) |
| 10 | `app_program/subsystem4/safety_task.c` | Safety | Uses `FAULT_CAUSE_LIFTED` (§11) |
| 11 | `app_program/subsystem1/tlm_format.c` | Telemetry | Cause text `"collision"` → `"lifted"` (§11) |
| 12 | `app_program/navigation/navigation.h`, `nav_task.c` | Navigation | New `nav_is_running()` (§11) |
| 13 | `app_program/navigation/nav_sm.c` / `.h` | Navigation | Obstacle bypass rework, line-loss smoothing (§12–15) |
| 14 | `app_program/subsystem5/drv_ultrasonic.c` / `.h` | Ultrasonic | Echo ISR state at file scope + `ultrasonic_ping_reset()` (§8–10) |
| 15 | `app_program/subsystem5/scan_task.c` | Obstacle scan | Reset before each ping, range glitch filter (§8–10); watches ahead while rejoining (§13) |
| 16 | `testing/host/test_nav.c`, `test_s4.c`, `ref/ref_tlm_format.c` | Host tests | Tests for the new behaviour (§16) |
| 17 | `Changes.md`, `MQTT_SETUP_GUIDE.md` | Docs | Only in (mine) |

---

## 1. `app_config.h` — value changes

### Line following and speed

| Constant | (dom) | (mine) | Why |
|---|---|---|---|
| `CFG_LINE_LOST_MS` | 300 | **500** | On straights both sensors are often white with the line between them; 300 ms started a search about every 0.5 s |
| `CFG_STEER_KP_MMPS` | 150 | **90** | Jitter / zigzag on the mat |
| `CFG_STEER_KD_X1000` | 300 | **100** | The D term amplified the on/off sensor jumps |
| `CFG_STEER_MAX_DIFF_MMPS` | 200 | **120** | Smaller wheel-speed difference |
| `CFG_NAV_CRUISE_MMPS` | 250 | **120** | Slower line following |
| `CFG_NAV_HUMP_MMPS` | 150 | **120** | Slower over the hump |
| `CFG_NAV_SPEED_MIN_MMPS` | 150 | **100** | Needed for cruise 120. **Below NFR17 (0.15–0.35 m/s)** and the barcode timing assumption; set back to 150 for the demo if barcodes misread |
| `CFG_NAV_LOST_DECAY_PM` | — | **980** (new) | With no sensor on the line, the held steering error fades (about half in 170 ms) instead of staying at full strength (§15) |

### Motion geometry

| Constant | (dom) | (mine) | Why |
|---|---|---|---|
| `CFG_ENC_COUNTS_PER_REV` | 624 | **528** | Back to the documented value (11-pulse encoder × 1:48 gearbox). With 624 every turn and distance overshot; a "90°" turn carried the car back across the line. Not measured on the car; tune by eye (528 × 90 ÷ observed angle of a commanded 90° turn) |

### Obstacle bypass

| Constant | (dom) | (mine) | Why |
|---|---|---|---|
| `CFG_NAV_PASS_GAP_MM` | 75 | **120** | The car passed too close to the box |
| `CFG_NAV_BEAM_SPREAD_PM` | — | **270** (new) | Corrects the scan's too-wide obstacle edges (§12) |
| `CFG_NAV_BYPASS_MAX_MM` | 800 | **900** | Now only a cap on the computed passing leg (§13) |
| `CFG_CAR_LENGTH_MM` | — | **200** (new) | Used for the passing leg. **Estimate: measure the car** |
| `CFG_NAV_DEPTH_MIN_MM` / `CFG_NAV_DEPTH_MAX_MM` | — | **100 / 300** (new) | Limits on the obstacle depth guess (= measured width) |
| `CFG_NAV_PASS_MARGIN_MM` | — | **100** (new) | Extra clearance behind the car before turning in |
| `CFG_NAV_PAST_EXTRA_MM` | 150 | **300** | The forward leg past the obstacle was too short; the car turned back as soon as it cleared it |
| `CFG_NAV_REJOIN_MAX_MM` | 300 | **400** | Longer drive back to the line |
| `CFG_NAV_ALIGN_MIN_CDEG` / `CFG_NAV_ALIGN_MAX_CDEG` | — | **4500 / 13500** (new) | Turn back along the line until a sensor sees it, between 45° and 135° (§13) |
| `CFG_NAV_REJOIN_MORE_MM` | — | **150** (new) | Rejoin blocked: go this much further past the obstacle |
| `CFG_NAV_REJOIN_RETRIES` | — | **3** (new) | Blocked rejoins before SAFE HOLD |
| `CFG_NAV_REJOIN_MISS_SEARCH` | — | **1** (new) | Rejoin missed the line: line search (5 s) instead of SAFE HOLD |
| `CFG_NAV_NO_ROOM_FALLBACK` | — | **1** (new) | No side clear twice: go round on the default side instead of SAFE HOLD. **Departs from FR11**; set 0 for the strict behaviour (§14) |
| `CFG_NAV_FALLBACK_LEFT` | — | **1** (new) | Default side for the fallback (1 = left, 0 = right) |
| `CFG_NAV_FALLBACK_LATERAL_MM` / `CFG_NAV_FALLBACK_DEPTH_MM` | — | **250 / 200** (new) | Sideways leg and assumed depth for the fallback |

### Obstacle scan

| Constant | (dom) | (mine) | Why |
|---|---|---|---|
| `CFG_US_ECHO_TIMEOUT_MS` | 50 | **60** | The HC-SR04's no-target pulse runs past 50 ms, so open space read as invalid (NO SAFE ROUTE on START); 60 ms still fits the 65 ms ping period |
| `CFG_SCAN_FAR_CONFIRM` | — | **3** (new) | Open-space pings in a row before the reported range jumps to max (§8–10) |
| `CFG_SCAN_COARSE_FIRST_DEG` / `LAST_DEG` / `STEP_DEG` | 30 / 150 / 30 | **45 / 135 / 22** | Narrower sweep. Angles are 45, 67, 89, 111, 133 (135 is not reached, though the code comment says it is) |
| `CFG_SCAN_SERVO_MIN_DEG` / `CFG_SCAN_SERVO_MAX_DEG` | 30 / 150 | **45 / 135** | Servo travel limits to match the sweep |
| `CFG_SCAN_SIDE_CLEAR_MM` | 400 | **500** | Box at the 45° side angle read just over 400 |
| `CFG_SCAN_BEHIND_COUNT` | 2 | **3** | A round obstacle gave gaps that read as "passed" too early |
| `CFG_SCAN_REJOIN_BLOCK_MM` | — | **250** (new) | Range ahead that means "still in the way" while rejoining (§13) |

### IMU

| Constant | (dom) | (mine) | Why |
|---|---|---|---|
| `CFG_IMU_IMPACT_MG` | 1000 | **2800** | Collision detection is gone (§11), so this is now only the calibration-jolt threshold and the "impact" motion label; it never stops the car. 2800 keeps motor-start and pivot jolts (1500–2450 mg) from restarting calibration |
| `CFG_IMU_LIFT_MG` | — | **250** (new) | Upward acceleration above 1 g that counts as a lift |
| `CFG_IMU_LIFT_SAMPLES` | — | **5** (new) | 50 ms in a row |
| `CFG_IMU_OVERTURN_COS_PM` | — | **500** (new) | Tilt over 60° = overturned |
| `CFG_IMU_OVERTURN_SAMPLES` | — | **20** (new) | 200 ms in a row |

---

## 2–5. 1 µs timer tick fix (`app_main.c`, `app_time.c/.h`, `hw_regs.h`)

**Problem:** The RP2040 TIMER (behind `app_time_us()`) and the watchdog both count the watchdog tick generator. The Pico SDK starts it, but the micro T-Kernel BSP does not. It only ran because the bootrom left it on after a UF2 flash. After pressing **RST**, the timer ran about 43× too slow (1 s measured as 23449 µs). That made every ultrasonic echo width too short, so the sensor read invalid and reported 4000 mm.

**Changes in (mine):**
- `hw_regs.h`: adds `HW_WATCHDOG_TICK` (0x4005802C), `HW_WATCHDOG_TICK_ENABLE` (bit 9), `HW_WATCHDOG_TICK_RUNNING` (bit 10) and `HW_XOSC_MHZ` (12).
- `app_time.c`: new `app_time_init()` writes `ENABLE | 12` to the TICK register and waits for RUNNING.
- `app_time.h`: prototype for `app_time_init()`.
- `app_main.c`: `#include "common/app_time.h"` and calls `app_time_init()` as the first statement in `usermain()`.

## 6–7. I2C bus recovery for the GY-511 IMU (`drv_i2c1.c`, `imu_task.c`)

**Problem:** RST resets only the RP2040. The GY-511 stays powered, so if RST lands mid-byte, the sensor holds SDA low. I2C1 can then never START, the ID check fails, SELF-TEST fails and Safety latches FAULT (the beeping). RESET can't clear it.

**Changes in (mine):**
- `drv_i2c1.c`:
  - New static functions `i2c1_bus_recover()`, `i2c1_line()` (open-drain release/pull-low) and `i2c1_bit_delay()` (5 µs busy-wait, so a ~100 kHz clock).
  - New constants `I2C1_RECOVER_CLOCKS = 9` and `I2C1_RECOVER_HALF_US = 5`.
  - `i2c1_init()` now calls `i2c1_bus_recover()` before taking I2C1 out of reset. Recovery switches SDA/SCL to SIO with pull-ups, clocks SCL up to 9 times until SDA is released, then sends a STOP.
  - Lines are only ever pulled low or released, never driven high.
- `imu_task.c` (`imu_try_init`):
  - (dom): `if (lsm303_check_id() && lsm303_init()) { ... }`
  - (mine): if `lsm303_check_id()` fails, it calls `i2c1_init(CFG_IMU_I2C_HZ)` (which recovers the bus) so the next retry can succeed. Otherwise it continues with `lsm303_init()`.

## 8–10. Ultrasonic echo resync + reported-range filter (`drv_ultrasonic.c/.h`, `scan_task.c`)

**Problem:** One lost or merged falling edge (from a jolt, or Echo held high through the ~38 ms timeout) left the ISR's `b_echo_high` stuck true with an old rise time. Later edges paired against that stale time, and the range froze (stuck at 4000 or tiny) until a reflash.

**Changes in (mine):**
- `drv_ultrasonic.c`:
  - `saved_rise_us` / `b_echo_high` changed from `static` locals inside the ISR to file-scope `static volatile` variables `g_saved_rise_us` / `g_echo_high`.
  - New public function `ultrasonic_ping_reset()` sets `g_echo_high = false` and `g_saved_rise_us = 0`. It deliberately doesn't use the live pin level.
  - The ISR body now uses the `g_` variables (same logic otherwise).
- `drv_ultrasonic.h`: adds the `void ultrasonic_ping_reset(void);` prototype.
- `scan_task.c`:
  - Calls `ultrasonic_ping_reset()` after draining stale echoes and right before `ultrasonic_trigger()`.
  - New context field `far_run` (open-space pings in a row).
  - **Reported-range glitch filter** replaces `if (b_valid) last_range_mm = range;`:
    - A valid reading below `CFG_US_MAX_MM` is reported at once and resets `far_run`.
    - A valid-at-max reading, or an answered-but-invalid one, increments `far_run`. The range jumps to `CFG_US_MAX_MM` only once `far_run >= CFG_SCAN_FAR_CONFIRM` (3).
    - An unanswered ping leaves the last range alone (`silent_run` still handles sensor health).
  - This filters only the **reported** (telemetry) range. Obstacle detection still uses the raw per-ping reading.

## 11. Lift / overturn detection replaces collision detection (`imu_calc.c/.h`, `imu_task.c`, `imu_msgs.c/.h`, `safety_task.c`, `tlm_format.c`, `navigation.h`, `nav_task.c`)

**Requirement change:** the IMU no longer detects collisions. It detects the car being **lifted or overturned** and stops the motors. Lifting the car ends a demo run.

- `imu_calc.c/.h`: new `imu_lift_t`, `imu_lift_init()`, `imu_lift_step()`, `imu_lift_clear()`. "Up" is the gravity vector averaged at calibration. Each sample is projected onto it:
  - **Lifted:** acceleration along "up" ≥ `CFG_IMU_LIFT_MG` above 1 g for `CFG_IMU_LIFT_SAMPLES` samples in a row.
  - **Overturned:** tilt from "up" > 60° (`CFG_IMU_OVERTURN_COS_PM`) for `CFG_IMU_OVERTURN_SAMPLES` samples in a row.
- `imu_task.c`:
  - Calibration also initialises the lift detector from the mean x/y/z.
  - Monitoring calls `imu_lift_step()` each sample. During a run (`nav_is_running()`), a detection reports `FAULT_CAUSE_LIFTED` to Safety (detail 1 = lifted, 2 = overturned) and prints `imu: lifted (up N mg)`. Safety latches and brakes at once; RESET then START to run again.
  - `imu_impact()` removed. An over-range sample (> 3.5 g) is now only left out; it no longer latches a fault.
- `imu_msgs.h/.c`, `safety_task.c`: `FAULT_CAUSE_COLLISION` renamed `FAULT_CAUSE_LIFTED` (same value, 1).
- `tlm_format.c`: cause text `"collision"` → `"lifted"`.
- `navigation.h`, `nav_task.c`: new `bool nav_is_running(void)`, true in LINE FOLLOWING, HUMP CROSSING, LINE SEARCH, AT JUNCTION and AVOIDING. Lifting the car in READY / SAFE HOLD / COMPLETE does nothing, so it can be carried to the start without a RESET.
- Verified on the car: `fault: lifted, detail 1` fired when picked up mid-run.

## 12. Obstacle width correction (`nav_sm.c`, `nav_sm_plan()`)

The ultrasonic beam is about ±15° wide, so the scan puts each edge too far out (a ~15 cm box read as 36–40 cm). The sideways leg now subtracts `closest × CFG_NAV_BEAM_SPREAD_PM / 1000` from the edge before adding the pass gap and half the car width. The width in telemetry is still the raw value.

## 13. Passing and rejoining (`nav_sm.c/.h`, `scan_task.c`)

- **Passing leg length** (new `nav_sm_pass_mm()`): `closest + depth + CFG_CAR_LENGTH_MM + CFG_NAV_PASS_MARGIN_MM`, capped at `CFG_NAV_BYPASS_MAX_MM`, where depth = measured width clamped to `CFG_NAV_DEPTH_MIN/MAX_MM`.
  - If the side scan reports "behind" first, the leg ends early. If that happens before the car has driven `closest`, the missing distance is added to the extra travel.
  - If the leg finishes without "behind", the car **turns back in** ((dom): SAFE HOLD).
- **Watching ahead on the way back:** `scan_task.c` now ranges in REJOINING once Navigation sends `SCAN_REQ_REJOIN` (new field `b_rejoin_watch`). Two readings ≤ `CFG_SCAN_REJOIN_BLOCK_MM` send `OBST_EVT_AHEAD`. Navigation then turns parallel again (new step `NAV_STEP_CLEAR_TURN`), drives `CFG_NAV_REJOIN_MORE_MM` (new step `NAV_STEP_CLEAR_MOVE`), turns in and retries, up to `CFG_NAV_REJOIN_RETRIES` times, then SAFE HOLD.
- **Rejoin misses the line:** with `CFG_NAV_REJOIN_MISS_SEARCH` = 1 the car starts a 5 s LINE SEARCH (scan resumed) instead of SAFE HOLD. This happened when the box sat at a bend.
- **Turning back along the line:** after the 80 mm align move, the car turns towards the line by up to `CFG_NAV_ALIGN_MAX_CDEG` and stops as soon as a sensor sees the line after at least `CFG_NAV_ALIGN_MIN_CDEG` (checked every tick using the fused heading). (dom) turns a fixed 90°, which lost the line at bends.
- New `nav_sm_t` fields: `avoid_closest_mm`, `avoid_width_mm`, `avoid_pass_from_mm`, `rejoin_tries`.

## 14. No room on either side (`nav_sm.c`)

When the scan finds no clear side even after reversing 10 cm, (dom) goes to SAFE HOLD (FR11). With `CFG_NAV_NO_ROOM_FALLBACK` = 1, (mine) goes round on `CFG_NAV_FALLBACK_LEFT`'s side with a `CFG_NAV_FALLBACK_LATERAL_MM` sideways leg and `CFG_NAV_FALLBACK_DEPTH_MM` assumed depth; passing and rejoin watching then work as in §13. The trigger seen on the mat was clutter next to the box (pink ramp, barcode sheets) making both sides look blocked.

## 15. Line-loss smoothing (`nav_sm.c`, `nav_sm_running_tick()`)

While following with no sensor on the line, `last_error_pm` is multiplied by `CFG_NAV_LOST_DECAY_PM / 1000` each 5 ms tick. In (dom) it is held, so a line between the sensors kept the car turning one way. When searching or avoiding it is still held, since it picks the sweep direction.

---

## 16. Host tests (`testing/host`, `make`)

| | (dom) | (mine) |
|---|---|---|
| Result | 106 passed, 3 failed | **107 passed, 3 failed** |
| Failing | S2-U01/02/03 (geometry, because of CPR 624) | S4-U05, S4-U06 (expect `CFG_IMU_IMPACT_MG` 1000; it is 2800), S5-U02 (expects the old 30° coarse angles) |

New or changed tests in (mine):
- **S4-U09** (`test_s4.c`): lift / overturn detector.
- **NAV-U15** (`test_nav.c`): beam-spread leg, passing-leg length, extra travel when "behind" comes early, turn-in without "behind", align turn stopping on the line, blocked rejoin and retries.
- **NAV-U16** (`test_nav.c`): no-room fallback when enabled.
- `ref/ref_tlm_format.c`: reference cause text `"collision"` → `"lifted"`.

The remaining failures come from the scan-angle and impact-threshold changes in §1; those tests' expected values were never updated.

---

## Things to note

- **Not tested on the car since the last changes:** the rejoin watch and retries, the computed passing leg, the no-room fallback and the line-loss decay were checked only by host tests. `scan_task.c`, `imu_task.c` and `nav_task.c` are not built by the host tests; the WSL build is their first compile.
