# 05 — Software architecture and interfaces (coverage C)

Tags and source keys: see `00_README.md`. Everything here was read from
REPO at `58f7d61` unless tagged otherwise. "Implemented" means the code
exists and compiles; none of it has run on a Pico W (02, 10).

## 1. Platform and template

| Item | Value | Tag |
|---|---|---|
| Board | Raspberry Pi Pico W (RP2040, 2 cores, 264 KB SRAM) on a Cytron Robo Pico | [Verified: RPT §2; KB 03] |
| RTOS | µT-Kernel 3.0 in the course template (SMP build on both cores by default) | [Verified: TPL = REPO `e870827`] |
| SDK and compilers used here | Pico SDK 2.2.0, `arm-none-eabi-gcc` 13.2.1; host tests gcc 13.3 and clang 18.1.3 | [Verified: EV `env.txt`, host logs] |
| Coding standard | Barr-C style (the template's), checked by reading only; no static checker was run | [Verified: TPL README]; [Inferred] no Barr-C tool exists in this session |
| Application entry | `app_program/app_main.c` (template file, modified): creates each subsystem's objects and tasks through `subsystemN_init()`/`navigation_init()` | [Verified: REPO] |
| Tunable values | `app_program/app_config.h` only (macros, no kernel headers, so host tests can include it) | [Verified: file header] |

## 2. Tasks (report section 3; TEAM.md §4)

| Task | Subsystem | Priority (1 = highest) | Core | Release | Stack (bytes) |
|---|---|---|---|---|---|
| `safety_task` | S4 | 5 | 0 | every 10 ms and on a fault event | 2048 |
| `line_task` | S3 | 6 | 0 | every 5 ms | 2048 |
| `nav_task` | Navigation | 7 | 0 | every 5 ms and on events | 3072 |
| `imu_task` | S4 | 8 | 1 | every 10 ms | 2048 |
| `motion_task` | S2 | 9 | 0 | every 20 ms | 2048 |
| `scan_task` | S5 | 10 | 1 | every 65 ms (pings ≥ 60 ms apart, D-75) | 2048 |
| `command_task` | S1 | 11 | 1 | on a command; link check every 100 ms | 2048 |
| `telemetry_task` | S1 | 12 | 1 | every 200 ms and on urgent records | 3072 |
| USB console (kernel) | – | 3 | 0 | | 4096 |
| `cyw43_utk_task` (kernel) | – | 4 | 0 | radio poll; runs the MQTT hooks | 6144 / 8192 with lwIP |

[Verified: TEAM.md §4; `app_config.h` §1] Periodic tasks are released by a
cyclic handler that sets an event-flag bit (D-07). Kernel tick 1 ms
(`CNF_TIMER_PERIOD 1`, A12, D-05). Since the teammate's commit `9378c9c`,
`app_config.h` refuses to compile if any period is not a whole number of
ticks (`#error "every task period must be a whole number of CFG_TICK_MS
ticks"`), and `app_main.c` checks `CFG_TICK_MS` against `CNF_TIMER_PERIOD`.
[Verified: REPO]

[Verified: TEAM.md §4] "Core 0 runs the control path (Safety, Line,
Navigation, Motion) and every GPIO interrupt; core 1 runs IMU, Scan,
Command and Telemetry. Data crosses cores only through kernel objects; the
report's two justified shared variables are the encoder counters and the
latest line-error record." And: "the kernel's radio task (priority 4) also
runs on core 0, above every IS19 task, so the 5 ms timing test should be
done with the Wi-Fi build too." [Unknown] Measured timing: none.

### Interrupts [Verified: TEAM.md §3; D-09]

One GPIO IRQ (IO_BANK0, IRQ 13) with a common dispatcher
`app_program/common/gpio_irq.c`, enabled on core 0 by `nav_task`. Handlers:
S2 encoders (GP0, GP1, GP16, GP17: count edges into the shared counters),
S3 barcode (GP7: time-stamp each edge into a message buffer, overflow
flag), S5 echo (GP5: echo pulse width).

## 3. Kernel objects (TEAM.md §6)

[Verified: TEAM.md §6] Limits in `config/config.h` (message buffers raised
from 12 to 20).

| Object | Limit | S1 | S2 | S3 | S4 | S5 | Nav | Total |
|---|---|---|---|---|---|---|---|---|
| Task | 32 | 2 | 1 | 1 | 2 | 1 | 1 | 8 |
| Semaphore | 16 | 1 | | | | | | 1 |
| Event flag | 16 | 1 | 1 | 1 | 2 | 1 | 1 | 7 |
| Mutex | 12 | | | 1 | | | | 1 |
| Message buffer | 20 | 4 | 1 | 1 | 1 | 2 | 5 | 14 |
| Cyclic handler | 12 | 1 | 1 | 1 | 2 | 1 | 1 | 7 |
| Mailbox, memory pools, alarm handlers | 12 each | | | | | | | 0 |

"The kernel's init, USB and WiFi tasks use a few more: 3 tasks, 1
semaphore, 1 event flag." [Verified: TEAM.md §6]. [Inferred] The 16 Oct
changes (D-79…D-81) created no new kernel objects (they add fields, a
fault cause and logic only). The on-target object count has not been run
(10).

RAM estimate (NFR19, below 75% of 264 KB), from the linker map, **not
measured on the board**: default ≈ 65 KB (25%), `NET=lwip MQTT=1`
≈ 124 KB (47%). [Verified: TEAM.md §6; EV bss figures 10 §2]

## 4. Interfaces IF-01 … IF-18

[Verified: TEAM.md §5, quoted] "The **provider** (receiver) creates the
kernel object and publishes its ID in its own header; the **user** only
sends to it through the function named. Message formats are in the
`*_msgs.h` headers."

| ID | From → To | Mechanism | Object / function | Data / meaning | Agreed by |
|---|---|---|---|---|---|
| IF-01 | S3 Line → Navigation | message buffer (Nav) | `mbf_line_evt`, `nav_post()` | `line_evt_t`: junction, lost, found, capture started, letter, capture failed | report |
| IF-02 | S3 Line → Navigation | shared record under a mutex (S3) | `line_err_read()` | `line_err_t`: latest line error every 5 ms | report |
| IF-03 | S4 IMU → Navigation | message buffer (Nav) | `mbf_imu_evt`, `nav_post()` | `imu_evt_t`: heading, hump start/end, motion class | report |
| IF-04 | any task → S4 Safety | message buffer + flag (S4) | `mbf_fault`, `safety_report_fault()` | `fault_evt_t`: cause, source, detail | report |
| IF-05 | S5 Scan → Navigation | message buffer (Nav) | `mbf_obstacle`, `nav_post()` | `obstacle_evt_t` | report |
| IF-06 | Navigation → S5 Scan | message buffer (S5) | `mbf_scan_req`, `scan_request()` | `scan_req_t` | report |
| IF-07 | Navigation → S2 Motion | message buffer (S2) | `mbf_motion_cmd`, `motion_*()` | `motion_cmd_t` | report |
| IF-08 | S2 Motion → Navigation | message buffer (Nav) | `mbf_motion_sts`, `nav_post()` | `motion_sts_t`: odometry every 20 ms, move done by ID | report |
| IF-09 | S4 Safety ↔ S2 Motion | event flags | `motion_request_reset()`, `safety_ack_zero()` | RESET handshake | report |
| IF-10 | S1 Command → Navigation | message buffer (Nav) | `mbf_nav_cmd`, `nav_post()` | `nav_cmd_t`: remap a letter, default map; Navigation acknowledges | report |
| IF-11 | Navigation → S3 Line | event flag (S3) | `line_set_adaptation()`, `line_set_run_over()` | freeze level adaptation on a hump; RUN OVER | D-23, D-57 |
| IF-12 | every task → S1 Telemetry | message buffer + flag (S1) | `mbf_tlm`, `tlm_post()` | `tlm_msg_t`: status records, route log, acks, faults, summary requests, link, obstacle profiles (FR10) | report, D-72 |
| IF-13 | S1 Telemetry → MQTT code | message buffer (S1) | `mbf_publish` | topic number + JSON payload | A11 |
| IF-14 | MQTT code → S1 Command | message buffer (S1) | `mbf_cmd_in` | command text from `inf2004/IS19/cmd` | A11 |
| IF-15 | S1 Command → MQTT code | message buffer (S1) | `mbf_mqtt_cfg` | new broker address and port (NFR14) | A11, D-16 |
| IF-16 | S4 Safety → Navigation, S2 Motion | event-flag level and bits | `safety_is_faulted()`; `NAV_FLG_RESTART`, `NAV_FLG_RESET` | FAULT latched; restart after release; RESET in SAFE HOLD | report |
| IF-17 | S3, S4, S5 → Navigation | event-flag levels (Nav) | `nav_set_health()` | sensor health checked in SELF-TEST | D-57 |
| IF-18 | Line, Nav, Motion, IMU, Scan → S4 Safety | event-flag bits (S4) | `safety_checkin()`, `safety_set_imu_ok()` | watchdog check-ins; IMU health for "cause cleared" | report, D-44 |

[Verified: TEAM.md §5] "A full control queue (IF-01, 03, 05, 06, 07, 08,
10) is reported to Safety as a fault; a full telemetry queue (IF-12, IF-13)
only drops and counts the record (D-68)."

"Agreed by: report" means the interface is in RPT; it does **not** mean the
team or the instructors reviewed this code. [Inferred from 00 §6]

## 5. Message formats (verbatim field lists)

[Verified: `subsystem3/line_msgs.h`, `subsystem4/imu_msgs.h`,
`subsystem5/scan_msgs.h`, `subsystem2/motion_msgs.h` at `58f7d61`]

```c
typedef struct {            /* IF-01 */
    uint8_t  type;          /* LINE_EVT_JUNCTION 1, LOST 2, FOUND 3,
                               CAPTURE_STARTED 4, LETTER 5, CAPTURE_FAILED 6 */
    char     letter;        /* LINE_EVT_LETTER: 'A'..'Z' */
    uint8_t  b_reversed;    /* LINE_EVT_LETTER: read backwards */
    uint8_t  reason;        /* LINE_EVT_CAPTURE_FAILED: BC_FAIL_xxx */
    uint32_t time_ms;
} line_evt_t;

typedef struct {            /* IF-02 */
    int32_t  error_pm;      /* -1000..1000, + = line under the left sensor */
    uint16_t raw_l, raw_r;  /* ADC counts, 0..4095 */
    uint16_t dark_l_pm, dark_r_pm;  /* 0 = white level, 1000 = black level */
    uint8_t  b_line_seen;   /* at least one sensor not white */
    uint8_t  b_lost;        /* LINE LOST state */
    uint8_t  b_valid;       /* a sample has been taken; 0 after a failed
                               read (D-79, line_task.c:313) */
    uint8_t  reserved;
    uint32_t seq;           /* increments every 5 ms sample */
    uint32_t time_ms;
} line_err_t;

typedef struct {            /* IF-03 */
    uint8_t  type;          /* IMU_EVT_HEADING 1, HUMP_START 2, HUMP_END 3, CLASS 4 */
    uint8_t  motion_class;  /* STATIONARY, ACCELERATING, TURNING, CLIMBING,
                               DESCENDING, IMPACT, STEADY (D-58, G13) */
    uint8_t  b_level_ground;
    uint8_t  reserved;
    int32_t  heading_cdeg, turn_rate_cdps, roll_cdeg, pitch_cdeg;
    int32_t  peak_roll_cdeg, height_mm;   /* IMU_EVT_HUMP_END */
    uint32_t time_ms;
} imu_evt_t;

typedef struct {            /* IF-04 */
    uint8_t  cause;         /* FAULT_CAUSE_COLLISION 1, STOP_CMD 2, IMU_DATA 3,
                               SELF_TEST 4, QUEUE_FULL 5, LOST_RECORD 6,
                               LINE_DATA 7 (D-79) */
    uint8_t  source;        /* FAULT_SRC_LINE 1, NAV 2, IMU 3, MOTION 4, SCAN 5,
                               COMMAND 6, TELEMETRY 7, SAFETY 8 */
    uint16_t detail;        /* cause-specific; collision by overrange: |a| in mg
                               (D-81); LINE_DATA: -MERCD(ercd) (D-79) */
    uint32_t time_ms;
} fault_evt_t;

typedef struct {            /* IF-05 */
    uint8_t  type;          /* OBST_EVT_AHEAD 1, PATH_CLEAR 2, PLAN 3,
                               NO_ROOM 4, BEHIND 5, NO_SAFE_ROUTE 6 */
    uint8_t  side;          /* OBST_SIDE_NONE 0, LEFT 1, RIGHT 2 */
    uint8_t  reason;        /* OBST_REASON_NONE 0, INVALID 1, NO_ROOM 2,
                               NO_LINE 3, TOO_LONG 4 */
    uint8_t  reserved;
    int16_t  distance_mm, offset_mm, width_mm, closest_mm;
    int16_t  edge_left_mm, edge_right_mm;
    int16_t  clear_left_mm, clear_right_mm;  /* -1 = side never seen (D-80) */
    uint32_t time_ms;
} obstacle_evt_t;

typedef struct {            /* IF-06 */
    uint8_t  type;          /* SCAN_REQ_CAR_STOPPED 1, BYPASS 2, REVERSED 3,
                               REJOIN 4, LINE_FOUND 5, LINE_MISSED 6,
                               RUN_OVER 7, START 8, RESUME 9 */
    uint8_t  side;
    uint16_t reserved;
    uint32_t time_ms;
} scan_req_t;

typedef struct {            /* IF-07 */
    uint8_t  type;          /* MOTION_CMD_DRIVE 1, FORWARD 2, BACKWARD 3,
                               TURN_LEFT 4, TURN_RIGHT 5, STOP 6,
                               HOLD_SPEED 7, KEEPALIVE 8 */
    uint8_t  move_id;       /* echoed in MOTION_STS_MOVE_DONE */
    int16_t  speed_mmps;    /* DRIVE */
    int16_t  diff_mmps;     /* DRIVE: left = speed - diff, right = speed + diff */
    int16_t  reserved;
    int32_t  value;         /* distance mm, angle cdeg, or flag */
} motion_cmd_t;

typedef struct {            /* IF-08 */
    uint8_t  type;          /* MOTION_STS_ODOMETRY 1, MOVE_DONE 2 */
    uint8_t  move_id;
    uint8_t  state;         /* IDLE, DRIVING, HOLDING, TURNING, BRAKED */
    uint8_t  b_move_active;
    int32_t  count_l, count_r, distance_mm, heading_cdeg;
    int16_t  speed_l_mmps, speed_r_mmps;
    uint32_t time_ms;
} motion_sts_t;
```

(Field lists are condensed onto fewer lines than in the headers; names,
types, values and comments are the headers'. Comments marked D-79/D-80/D-81
paraphrase those entries.)

## 6. MQTT (S1; report 6.1; A11)

[Verified: `app_config.h` §8; `cmd_parse.h`; `tlm_format.h`]

| Topic | Direction | Content |
|---|---|---|
| `inf2004/IS19/telemetry` | car → | 5 Hz telemetry (NFR9, FR12, FR17) |
| `inf2004/IS19/status` | car → | 1 Hz heartbeat with link and fault state |
| `inf2004/IS19/event` | car → | route log (FR13), obstacle profile (FR10, D-72), faults, motion class, link, run summary (FR14, FR18) |
| `inf2004/IS19/ack` | car → | the answer to each command |
| `inf2004/IS19/cmd` | → car | commands |

Commands, verbatim from `cmd_parse.h`:

```
{"seq":12,"cmd":"remap","letter":"E","action":"left"}
{"seq":13,"cmd":"default_map"}
{"seq":14,"cmd":"summary"}
{"seq":15,"cmd":"stop"}
{"seq":16,"cmd":"broker","ip":"192.168.1.20","port":1883}
```

"Actions: none, straight, left, right, uturn. Only flat string and integer
values are understood; anything else is rejected." There is **no START
command** (FR16; G2 "No"). [Verified: `cmd_parse.h`; decisions §E]

Worst-case record lengths measured on the host for `ee9601b` (14 Oct):
telemetry 259, heartbeat 213, route 128/104/132, summary 208, ack 77, fault
105, motion 89, link 73 bytes; payload limit `CFG_MQTT_PAYLOAD_MAX` 320.
[Verified: `evidence/ee9601b/worst_case_json.txt`]. The profile event (added
13 Oct by the teammate, D-72: `{"ev":"profile","x","y","w","close","cl","cr","side","rev"}`)
and the `line_data` cause name were not in that measurement. [Unknown]
their worst-case length; [Inferred] the profile's eight numeric fields are
int16/uint8, so it is shorter than the telemetry record.

What runs where for MQTT [Verified: TEAM.md §3]: "No IS19 task calls
`cyw43_...`, lwIP or MQTT functions. The MQTT code in
`lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c` runs inside
`cyw43_utk_task` and calls hooks that subsystem 1 provides
(`app_program/subsystem1/mqtt_bridge.c`); the hooks only move records
through message buffers with `TMO_POL` (IF-13, IF-14, IF-15). This extends
shared library code, so it needs the instructor's agreement (A11)."
Reconnect back-off 1, 2, 4 … 30 s (D-70; MQTT only, G5 in §E sense).
Wi-Fi name, password and the first broker come from
`config/wifi_secrets.h` at build time (not in git; template example file
`config/wifi_secrets.example.h`).

## 7. Build profiles and commands

[Verified: `rv/verify_tree.sh`; EV `final_main/build_summary.txt`]

From `build_make/` with `PICO_SDK_PATH` set and a `config/wifi_secrets.h`
(copy the example for a build test):

| Command | What it builds | Result at `b7bf737` |
|---|---|---|
| `make` | default: SMP, USB console, Wi-Fi (no lwIP) | 0 warnings; text 316096, data 88, bss 28032 |
| `make TEST=1` | + on-target `*_test.c` checks | 0 warnings; text 319164 |
| `make NET=lwip MQTT=1` | + lwIP and MQTT (the demo build) | 0 warnings; text 371100, data 124, bss 86716 |
| `make NET=lwip MQTT=1 TEST=1` | both | 0 warnings; text 374168 |
| `make SMP=0` | single core | 0 warnings; text 308776, bss 23900 |
| `make CONSOLE=uart WIFI=none` | UART console, no radio | 0 warnings; text 84314 |
| `make CRASH_REPORT=1` | UART0 crash report on GP0/GP1 (unplug the left encoder) | 0 warnings; text 316244 |

Host tests: `cd testing/host && make clean && make` (add `SAN=1` for
ASan/UBSan, `CC=clang` for clang). Since the teammate's I-06 fix the
Makefile rebuilds when `SAN`, `CC` or the flags change and prints
`[INFO] host compiler:` and `[INFO] host flags:`. [Verified: host logs]

"0 warnings" is at the template's warning level (no `-Wall`); an extra
recompile with more warnings is `scripts/warncheck.sh` (rc 0 at
`b7bf737`). [Verified: EV; TPKG NF-4 says the same]
