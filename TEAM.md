# Team and subsystems (IS19)

The single place that says **who owns what**, so that five people can work in
one firmware image without clashing over pins, priorities or kernel objects.
The architecture is the one in the W6 design review
([summary](docs/notes/04_design_review_summary.md)); every choice the report
left open, and every change made since, is in
[docs/decisions.md](docs/decisions.md) (D-numbers below). Every adjustable
value is in [app_program/app_config.h](app_program/app_config.h), and none of
them has been measured on the car yet (they are marked UNVALIDATED there).

A **subsystem** is one student's part of the product, with its own folder.
It may contain several tasks and own several peripherals (section 3).

Rule: **only the owner edits a subsystem's folders.** To change another
subsystem, ask its owner or open a pull request for them to review.

## 1. Members and subsystems

Student IDs are left out on purpose: the repository is public.
GitHub usernames are still to be filled in by each member.

| # | Subsystem (what it does) | Owner | GitHub username | Code | Tests |
|---|---|---|---|---|---|
| 1 | Wi-Fi / MQTT: commands, telemetry, route log, run summary | Ang Hung Ling | *to fill* | `app_program/subsystem1/` | `testing/subsystem1/` |
| 2 | Motion: motors, encoders, wheel PID, moves and turns, odometry | Dominic Ng Jun Jie | *to fill* | `app_program/subsystem2/` | `testing/subsystem2/` |
| 3 | Line following, junctions, Code 39 barcodes, steering, letter map | Nga Wei Yang | *to fill* | `app_program/subsystem3/` | `testing/subsystem3/` |
| 4 | IMU (tilt, humps, motion class, collision) and Safety | Yee Sheng Chua | *to fill* | `app_program/subsystem4/` | `testing/subsystem4/` |
| 5 | Ultrasonic and servo: scans, obstacle profile, side choice | Wong Qian Hui | *to fill* | `app_program/subsystem5/` | `testing/subsystem5/` |
| – | Navigation: the system statechart (Buddy 3 owns the steering and letter-map code it runs) | whole team | | `app_program/navigation/` | `testing/navigation/` |
| – | Common helpers: fixed-point maths, GPIO interrupt dispatcher, PWM, time, registers | whole team | | `app_program/common/` | `testing/common/` |

Shared files (`app_program/app_main.c`, `app.h`, `app_config.h`,
`navigation/`, `common/`, this file, `docs/decisions.md`) are changed only by
agreement of the team. Each subsystem's tunable values live in its own
section of `app_config.h`; its owner may change those.

## 2. Pins

One owner per pin (report section 2; `app_config.h` section 2). Claim a pin
here **before** you wire or code it.

| Pin | Owner | Used for |
|---|---|---|
| GP23, GP24, GP25, GP29 | *reserved* | Pico W radio |
| GP0 | S2 | left encoder A (Grove 1). Taken by UART0 in `CRASH_REPORT=1` builds: unplug the encoder for those |
| GP1 | S2 | left encoder B (Grove 1). Same note as GP0 |
| GP2 | S4 | IMU SDA, I2C1 (Grove 2; leave the Maker port empty, it shares GP2/GP3) |
| GP3 | S4 | IMU SCL, I2C1 (Grove 2) |
| GP4 | S5 | ultrasonic trigger (Grove 3) |
| GP5 | S5 | ultrasonic echo, edge interrupt (Grove 3) |
| GP6 | *spare* | Grove 5. Optional Line timing probe for A12 (`CFG_PIN_TIMING_PROBE`, off; claim it here first, D-20) |
| GP7 | S3 | barcode sensor D0, edge interrupt (Grove 7) |
| GP8 | S2 | motor 1 (left) M1A, PWM slice 4 A. Safety may force it low (A14) |
| GP9 | S2 | motor 1 (left) M1B, PWM slice 4 B. Safety may force it low (A14) |
| GP10 | S2 | motor 2 (right) M2A, PWM slice 5 A. Safety may force it low (A14) |
| GP11 | S2 | motor 2 (right) M2B, PWM slice 5 B. Safety may force it low (A14) |
| GP12 | S5 | servo 1 signal, PWM slice 6 A |
| GP13 | *spare* | servo port 2 |
| GP14 | *spare* | servo port 3 |
| GP15 | *spare* | servo port 4 |
| GP16 | S2 | right encoder A (Grove 4) |
| GP17 | S2 | right encoder B (Grove 4) |
| GP18 | S4 | on-board NeoPixel status LED (PIO) |
| GP19 | *spare* | |
| GP20 | Navigation | START button (on board) |
| GP21 | S4 | RESET button (on board) |
| GP22 | S4 | on-board buzzer, PWM slice 3 A |
| GP26 | S3 | left line sensor A0, ADC0 (Grove 6) |
| GP27 | S3 | right line sensor A0, ADC1 (Grove 6) |
| GP28 | *spare* | ADC2 on Grove 7 (keep the board's VBAT jumper open) |

## 3. Peripherals

Each peripheral is driven by exactly one task, its *owning task*. Every
other task asks the owner through a kernel object (section 5).

| Peripheral | Owner subsystem | Owning task | Pins |
|---|---|---|---|
| ADC (kernel `adca` device driver) | S3 | `line_task` | GP26, GP27 |
| I2C1 (own polled driver `drv_i2c1.c`, D-11) | S4 | `imu_task` | GP2, GP3 |
| PWM slice 3 A | S4 | `safety_task` | GP22 |
| PWM slices 4 and 5 | S2 | `motion_task` | GP8–GP11 |
| GPIO output override of GP8–GP11 | S4 | `safety_task` | GP8–GP11 (A14, section 7) |
| PWM slice 6 A | S5 | `scan_task` | GP12 |
| PIO: one free state machine, claimed at run time | S4 | `safety_task` | GP18 |
| Watchdog | S4 | `safety_task` | – |
| GPIO interrupts (IO_BANK0, IRQ 13) | common dispatcher `common/gpio_irq.c` (D-09), enabled on core 0 by `nav_task` | handlers: S2 encoders, S3 barcode, S5 echo | GP0, GP1, GP5, GP7, GP16, GP17 |
| TIMER (free-running 1 µs counter, read only) | any | any | – |
| UART0 | *kernel* (`CRASH_REPORT=1` builds only) | | GP0, GP1 |
| WiFi radio, TCP/IP, MQTT | *kernel* | `cyw43_utk_task` (built in) | GP23–25, GP29 |

### WiFi and MQTT

No IS19 task calls `cyw43_...`, lwIP or MQTT functions. The MQTT code in
`lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c` runs inside
`cyw43_utk_task` and calls hooks that subsystem 1 provides
(`app_program/subsystem1/mqtt_bridge.c`); the hooks only move records
through message buffers with `TMO_POL` (IF-13, IF-14, IF-15). This extends
shared library code, so it needs the instructor's agreement (A11, section
7). It is active only in the `NET=lwip MQTT=1` build.

## 4. Task priorities

1 is the highest; 1–4 belong to the kernel. IS19 uses the report's order
5–12 (report section 3) instead of the template's default bands.
Periodic tasks are released by a cyclic handler that sets an event-flag bit
(D-07); the kernel tick is 1 ms (A12, D-05).

| Task | Subsystem | Priority | Core | Release | Stack (bytes) |
|---|---|---|---|---|---|
| `safety_task` | S4 | 5 | 0 | every 10 ms and on a fault event | 2048 |
| `line_task` | S3 | 6 | 0 | every 5 ms | 2048 |
| `nav_task` | Navigation | 7 | 0 | every 5 ms and on events | 3072 |
| `imu_task` | S4 | 8 | 1 | every 10 ms | 2048 |
| `motion_task` | S2 | 9 | 0 | every 20 ms | 2048 |
| `scan_task` | S5 | 10 | 1 | every 65 ms | 2048 |
| `command_task` | S1 | 11 | 1 | on a command; link check every 100 ms | 2048 |
| `telemetry_task` | S1 | 12 | 1 | every 200 ms and on urgent records | 3072 |
| USB console (kernel) | – | 3 | 0 | | 4096 |
| `cyw43_utk_task` (kernel) | – | 4 | 0 | radio poll; runs the MQTT hooks | 6144 / 8192 with lwIP |

Core 0 runs the control path (Safety, Line, Navigation, Motion) and every
GPIO interrupt; core 1 runs IMU, Scan, Command and Telemetry. Data crosses
cores only through kernel objects; the report's two justified shared
variables are the encoder counters and the latest line-error record (the
measurement build's statistics are the one other exception, section 7).
Stack sizes: D-19. Note for A12: the kernel's radio task (priority 4) also
runs on core 0, above every IS19 task, so the 5 ms timing test should be
done with the Wi-Fi build too.

## 5. Interfaces between subsystems

The **provider** (receiver) creates the kernel object and publishes its ID
in its own header; the **user** only sends to it through the function
named. Message formats are in the `*_msgs.h` headers.

| ID | From → To | Mechanism | Object / function | Data / meaning | Agreed by |
|---|---|---|---|---|---|
| IF-01 | S3 Line → Navigation | message buffer (owned by Nav) | `mbf_line_evt`, `nav_post()` | `line_evt_t` (16 bits, D-84): junction, lost, found, capture started, letter, capture failed | report |
| IF-02 | S3 Line → Navigation | shared record under a mutex (owned by S3) | `line_err_read()` | `line_err_t` (two words, D-84): latest line error every 5 ms (justified shared variable); while running, a sample count that stops moving for 20 ms is a fault (D-94) | report |
| IF-03 | S4 IMU → Navigation | message buffer (owned by Nav) | `mbf_imu_evt`, `nav_post()` | `imu_evt_t` (one word, D-89): heading, hump start/end, motion class | report |
| IF-04 | any task → S4 Safety | message buffer + flag (owned by S4) | `mbf_fault`, `safety_report_fault()`, `safety_report_status()` | `fault_evt_t` (one word, D-89, D-102): cause (11, in 4 bits), source, detail, and for a status cause (no echo, Wi-Fi loss) a cleared flag; Safety alone writes the fault mask from these reports | report, D-102 |
| IF-05 | S5 Scan → Navigation | message buffer (owned by Nav) | `mbf_obstacle`, `nav_post()` | `obstacle_evt_t` (two words, D-90): ahead, path clear, plan, no room, behind, no safe route | report |
| IF-06 | Navigation → S5 Scan | message buffer (owned by S5) | `mbf_scan_req`, `scan_request()` | `scan_req_t` (one byte, D-90): car stopped, bypass, reversed, rejoin, line found/missed, run over, start, resume | report |
| IF-07 | Navigation → S2 Motion | message buffer (owned by S2) | `mbf_motion_cmd`, `motion_*()` | `motion_cmd_t` (one word, D-88): drive, forward, backward, turn left/right, stop, hold speed, keepalive | report |
| IF-08 | S2 Motion → Navigation | message buffer (owned by Nav) | `mbf_motion_sts`, `nav_post()` | `motion_sts_t` (two words, D-88): odometry every 20 ms, move done by ID | report |
| IF-09 | S4 Safety ↔ S2 Motion | event flags (each owns its own) | `motion_request_reset()`, `safety_ack_zero()` | RESET handshake: clear queue, command and PID, then acknowledge; a fault before the acknowledgement cancels the reset (D-95) | report |
| IF-10 | S1 Command → Navigation | message buffer (owned by Nav) | `mbf_nav_cmd`, `nav_post()` | `nav_cmd_t` (two words, D-91): remap a letter, default map; Navigation acknowledges | report |
| IF-11 | Navigation → S3 Line | event flag (owned by S3) | `line_set_adaptation()`, `line_set_run_over()` | freeze level adaptation on a hump; RUN OVER, ended by START or by the restart into SELF-TEST (D-98) | D-23, D-57, D-98 |
| IF-12 | every task → S1 Telemetry | message buffer + flag (owned by S1) | `mbf_tlm`, `tlm_post()` | `tlm_msg_t` (five words, D-92): status records (Safety's with the fault mask, D-102), route-log entries, acks, faults, summary requests, link, obstacle profiles from Scan (FR10) | report, D-72 |
| IF-13 | S1 Telemetry → MQTT code | message buffer (owned by S1) | `mbf_publish` | topic number + JSON payload | A11 |
| IF-14 | MQTT code → S1 Command | message buffer (owned by S1) | `mbf_cmd_in` | command text from `inf2004/IS19/cmd` | A11 |
| IF-15 | S1 Command → MQTT code | message buffer (owned by S1) | `mbf_mqtt_cfg` | `comms_broker_t` (two words, D-91): new broker address, each part's spelling in 9 bits, and port (NFR14) | A11, D-16 |
| IF-16 | S4 Safety → Navigation, S2 Motion | event-flag level and bits | `safety_is_faulted()`; `NAV_FLG_RESTART`, `NAV_FLG_RESET` | FAULT latched (Motion brakes, Navigation stops commanding); restart after release; RESET in SAFE HOLD | report |
| IF-17 | S3, S4, S5 → Navigation | event-flag levels (owned by Nav) | `nav_set_health()` | sensor health checked in SELF-TEST; line health drops at its first bad sample and while the Line task is idle, and comes back after a whole good 200 ms window (D-96, D-98); ultrasonic health is "the sensor answers its pings", a valid range or not (D-104) | D-57, D-96, D-98, D-104 |
| IF-18 | Line, Nav, Motion, IMU, Scan → S4 Safety | event-flag bits (owned by S4) | `safety_checkin()`, `safety_set_imu_ok()` | watchdog check-ins once per cycle; IMU health for "cause cleared" | report, D-44 |

A full control queue (IF-01, 03, 05, 06, 07, 08, 10) is reported to Safety
as a fault; a full telemetry queue (IF-12, IF-13) only drops and counts the
record (D-68); a STOP reaches Safety even when the command queue (IF-14)
is full (D-101). Each interface is tested by at least one integration test
(`testing/integration/TEST_LOG.md`); `testing/TRACEABILITY.md` links every
requirement to its design, code and tests.

## 6. Kernel object budget

Limits are in `config/config.h` (message buffers raised from 12 to 20).

| Object | Limit | S1 | S2 | S3 | S4 | S5 | Nav | Total |
|---|---|---|---|---|---|---|---|---|
| Task | 32 | 2 | 1 | 1 | 2 | 1 | 1 | 8 |
| Semaphore | 16 | 1 | | | | | | 1 |
| Event flag | 16 | 1 | 1 | 1 | 2 | 1 | 1 | 7 |
| Mailbox | 12 | | | | | | | 0 |
| Mutex | 12 | | | 1 | | | | 1 |
| Message buffer | 20 | 4 | 1 | 1 | 1 | 2 | 5 | 14 |
| Fixed memory pool | 12 | | | | | | | 0 |
| Variable memory pool | 12 | | | | | | | 0 |
| Cyclic handler | 12 | 1 | 1 | 1 | 2 | 1 | 1 | 7 |
| Alarm handler | 12 | | | | | | | 0 |

(The kernel's init, USB and WiFi tasks use a few more: 3 tasks, 1 semaphore,
1 event flag.)

### RAM (NFR19: below 75% of 264 KB)

Estimated from the linker map and the configured sizes, **not measured on
the board** (22 Oct, after the minimum-bit change; before it: static data
27.4 and 84.8 KB, message buffers 7.6 KB, totals ≈ 65 and 124 KB;
`docs/minimum_bit.md` section 3):

| | default build | `NET=lwip MQTT=1` |
|---|---|---|
| Static data (`.data` + `.bss`, from the map) | 27.2 KB | 84.5 KB |
| IS19 task stacks | 18.0 KB | 18.0 KB |
| Kernel task stacks (USB, radio) | 10.0 KB | 12.0 KB |
| IS19 message buffers | 5.7 KB | 5.7 KB |
| Exception stack | 2.0 KB | 2.0 KB |
| **Total** | **≈ 63 KB (24%)** | **≈ 122 KB (46%)** |

## 7. Exceptions to the template rules, and template files changed

These need the instructor's agreement (report assumptions); the code is
written so each can be switched back.

| Item | What | Where | Status |
|---|---|---|---|
| A11 | MQTT code extended with hooks that exchange message buffers with subsystem 1; a 10 ms publish pump; run-time broker address; report 6.1 reconnect back-off (1, 2, 4 … 30 s, D-70); larger MQTT output buffer and one more lwIP timer slot (D-49, D-50) | `lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c`, `mqtt_utk.h`, `lwipopts.h` | pending instructor agreement |
| A12 | 1 ms kernel tick instead of 10 ms, so 5 ms periods are whole ticks | `config/config.h` `CNF_TIMER_PERIOD` | to be verified: CPU ≤ 60%, 5 ± 1 ms |
| A14 | Safety forces Motion's pins GP8–GP11 low with the IO_BANK0 output override on a fault (switch: `CFG_SAFETY_PIN_OVERRIDE`, D-45) | `subsystem4/safety_task.c` | pending instructor agreement |
| – | Message-buffer limit 12 → 20 | `config/config.h` | team |
| – | Measurement build only (D-82, off by default): the statistics are written by tasks and handlers and read by Telemetry across cores without a kernel object; diagnostic data, one writer per word, figures in one line may come from different moments | `common/app_stats.c` | team |
| – | Template example tasks removed; template example test rows replaced by IS19 tests | `app_program/subsystemN/`, `testing/` | team |

Found in the template and **not changed** (reported instead): `GPIO_OUT` in
`include/sys/sysdepend/cpu/rp2040/sysdef.h` is defined as `SIO_BASE+0x008`,
which is `GPIO_HI_IN` on the RP2040 (the output register is at
`SIO_BASE+0x010`). IS19 code does not use it; see `app_program/common/hw_regs.h`.
