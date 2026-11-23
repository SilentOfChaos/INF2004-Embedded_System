# 04 — Hardware and wiring (coverage B)

Tags and source keys: see `00_README.md`. Nothing in this file has been
checked on the physical car in this session. [Verified: no measurement exists
in REPO or EV]

## 1. Board facts from `Robo_Pico.pdf` (RP), with page references

Page numbers: "PDF p." is the page index in the 11-page PDF; "printed" is
the number in the page footer. RPT §8.3 cites the Grove table as
"Robo_Pico.pdf, p. 4" (PDF page); KB 03 cites "p.3" (printed). Same page.
[Verified: page-split text extract of `Robo_Pico.pdf`]

### Grove ports — RP PDF p.4 (printed 3), "Table 1: ROBO-PICO Board Functions"

| Grove | GPIO | PWM | SPI | I2C | UART | Analog |
|---|---|---|---|---|---|---|
| 1 | 0 | PWM0-A | SDI0 | SDA0 | TX0 | – |
| 1 | 1 | PWM0-B | CSn0 | SCL0 | RX0 | – |
| 2 | 2 | PWM1-A | SCK0 | SDA1 | – | – |
| 2 | 3 | PWM1-B | SDO0 | SCL1 | – | – |
| 3 | 4 | PWM2-A | SDI0 | SDA0 | TX1 | – |
| 3 | 5 | PWM2-B | CSn0 | SCL0 | RX1 | – |
| 4 | 16 | PWM0-A | SDI0 | SDA0 | TX0 | – |
| 4 | 17 | PWM0-B | CSn0 | SCL0 | RX0 | – |
| 5 | 6 | PWM3-A | SCK0 | SDA1 | – | – |
| 5 | 26 | PWM5-A | – | SDA1 | – | ADC0 |
| 6 | 26 | PWM5-A | – | SDA1 | – | ADC0 |
| 6 | 27 | PWM5-B | – | SCL1 | – | ADC1 |
| 7 | 7 | PWM3-B | SDO0 | SCL1 | – | – |
| 7 | 28 | PWM6-A | – | – | – | ADC2 |

[Verified: RP PDF p.4] Same page: "Maker Port … The pins are shared with the
Grove 2 port (GPIO 2, 3)"; "NeoPixel RGB LEDs … Connected to GP18";
"Programmable Buttons … Connected to GP20 and GP21"; "Piezo Buzzer …
Connected to GP22"; "Buzzer Mute Switch"; "Servo Ports … Signal is connected
to GP12, GP13, GP14 and GP15. V+ voltage is equal to power source voltage";
"Motor Test Buttons … Motor will run at full speed".

### Motor terminals and VBAT sense — RP PDF p.5 (printed 4)

[Verified: RP PDF p.5] "M1A : GP8, M1B : GP9, M2A : GP10, M2B : GP11";
"Motor voltage at full speed is equal to power source voltage"; "Solder the
jumper on the back of the board to allow voltage measurement of the VBAT via
GP-28-ADC2. VADC2 = VBAT / 2".

### Specifications — RP PDF p.7 (printed 6), "3. SPECIFICATIONS"

| Parameter | Value | Tag |
|---|---|---|
| Power input (USB, LiPo or VIN) | 3.6 V min, 6 V max | [Verified: RP PDF p.7] |
| Digital input | low −0.3…0.8 V, high 2.0…3.6 V | same |
| Digital output | low 0…0.5 V, high 2.6…3.3 V | same |
| Analog input | 0…3.3 V | same |
| Vmotor & Vservo, only VIN connected | = VIN | same |
| Max DC motor current per channel | 1 A continuous; 1.5 A peak (< 5 s) | same |
| DC motor driver PWM frequency | 20 kHz max | same |
| Total +3V3 output current (Grove ports) | 300 mA | same |

### Motor driver truth table — RP PDF p.9 (printed 8), "Table 3"

| Input A (GP8 / GP10) | Input B (GP9 / GP11) | Motor |
|---|---|---|
| Low | Low | **Brake** |
| High | Low | Forward* |
| Low | High | Backward* |
| High | High | Coast (outputs Hi-Z) |

\* "Actual motor direction is depending on the motor connection."
[Verified: RP PDF p.9] This table is why braking = both inputs low, which
the Safety pin override uses (07).

### Power tree [Verified: KB 03, citing RP p.6 and the power-tree page of `Robo Pico Board Datasheet.pdf`]

- The board takes the highest of VIN, micro-USB and 1S LiPo (3.6–6 V).
- Motors and servos run directly from the selected input; there is no 5 V rail.
- A buck-boost converter (800 mA) makes 3.3 V for the Pico W, Grove/Maker
  ports, buzzer, NeoPixel and LEDs.
- [Verified: KB 03] "With the kit power bank on VIN the motors and servo see
  about 5 V (the motors are rated 6 V, so expect roughly 290 rpm no-load
  instead of 350)." This is a calculation, not a measurement.

## 2. IS19 pin map (RPT §2) and the code constants

[Verified: RPT §2 table; `app_config.h` §2; TEAM.md §2] All three agree.

| Device | Signal (RPT) | Pin | Port | Interface | Owner task | `app_config.h` name |
|---|---|---|---|---|---|---|
| Line IR left | LINE_L, A0 | GP26 | Grove 6 | ADC0 | Line | `CFG_PIN_LINE_L` 26, `CFG_ADC_CH_LINE_L` 0 |
| Line IR right | LINE_R, A0 | GP27 | Grove 6 | ADC1 | Line | `CFG_PIN_LINE_R` 27, `CFG_ADC_CH_LINE_R` 1 |
| Barcode IR | BARCODE, D0 | GP7 | Grove 7 | GPIO IRQ | Barcode ISR → Line | `CFG_PIN_BARCODE` 7 |
| GY-511 IMU | IMU_SDA / IMU_SCL | GP2 / GP3 | Grove 2 | I2C1 | IMU | `CFG_PIN_IMU_SDA` 2, `CFG_PIN_IMU_SCL` 3 |
| HC-SR04P | US_TRIG / US_ECHO | GP4 out / GP5 IRQ | Grove 3 | GPIO | Obstacle Scan (Echo ISR) | `CFG_PIN_US_TRIG` 4, `CFG_PIN_US_ECHO` 5 |
| Left encoder | ENC_L_A / ENC_L_B (connector pins 2 / 3) | GP0 / GP1 | Grove 1 | GPIO IRQ | Encoder ISR → Motion | `CFG_PIN_ENC_L_A` 0, `CFG_PIN_ENC_L_B` 1 |
| Right encoder | ENC_R_A / ENC_R_B | GP16 / GP17 | Grove 4 | GPIO IRQ | Encoder ISR → Motion | `CFG_PIN_ENC_R_A` 16, `CFG_PIN_ENC_R_B` 17 |
| Left motor | M1A / M1B | GP8 / GP9 | M1 terminal (on-board driver) | PWM slice 4 | Motion | `CFG_PIN_MOTOR_L_A` 8, `_L_B` 9 |
| Right motor | M2A / M2B | GP10 / GP11 | M2 terminal | PWM slice 5 | Motion | `CFG_PIN_MOTOR_R_A` 10, `_R_B` 11 |
| SG90 servo | SERVO | GP12 | Servo 1 | PWM slice 6 A | Obstacle Scan | `CFG_PIN_SERVO` 12 |
| START button | START | GP20 | on-board | GPIO input | Navigation | `CFG_PIN_START` 20 |
| RESET button | RESET | GP21 | on-board | GPIO input | Safety | `CFG_PIN_RESET` 21 |
| Status LED (NeoPixel) | STATUS_LED | GP18 | on-board | PIO | Safety | `CFG_PIN_STATUS_LED` 18 |
| Piezo buzzer | BUZZER | GP22 | on-board | PWM slice 3 A | Safety | `CFG_PIN_BUZZER` 22 |
| Power bank | VIN | VIN + GND | terminal | 5 V, 2.1 A (OUT1) | — | — |
| Spare | — | GP6 (Grove 5), GP28 (Grove 7), GP13–GP15 (Servo 2–4), GP19 | — | — | — | `CFG_PIN_TIMING_PROBE` (−1 = off; GP6 proposed, D-20) |
| Radio (reserved) | — | GP23, GP24, GP25, GP29 | Pico W internal | CYW43439 | kernel `cyw43_utk_task` | — |

RPT notes kept verbatim [Verified: RPT §2]: GY-511 "3.3 V to VIN pin; the
Maker Port shares these pins, so leave it empty"; left encoder "pin 1 GND,
pin 4 3.3 V; unplug for CRASH_REPORT=1 builds"; motors "never wired to GPIO
directly"; buzzer "mute switch off"; spare "Grove 5's GP26 is shared with
Grove 6; leave the VBAT jumper open so GP28 stays free."

[Verified: TEAM.md §2] GP19 is listed as *spare* in TEAM.md; RPT §2 does
not list GP19 at all. [Inferred] No conflict, GP19 is simply unused.

### PWM slice use [Verified: KB 03; slice = (GPIO / 2) mod 8]

| Slice | Pins on this board | IS19 use |
|---|---|---|
| 0 | GP0/GP1, GP16/GP17 | encoder inputs (not PWM) |
| 1 | GP2/GP3, GP18/GP19 | I2C1, NeoPixel (PIO) |
| 2 | GP4/GP5, GP20/GP21 | ultrasonic, buttons |
| 3 | GP6/GP7, GP22 | buzzer PWM on 3A; GP7 barcode input on 3B |
| 4 | GP8/GP9 | motor 1 |
| 5 | GP10/GP11, GP26/GP27 | motor 2; GP26/27 are ADC inputs |
| 6 | GP12/GP13, GP28 | servo 1 (50 Hz) on 6A; GP28 also maps to 6A |
| 7 | GP14/GP15 | spare servo ports |

[Verified: KB 03] "slice 3 cannot be used to count barcode edges in PWM
input mode while the buzzer uses it, and GP28 cannot be a PWM output while
the servo uses slice 6A. Neither is planned".

### Encoder motor connector [Verified: KB 03, citing `PicoCar.pptx` slide 2]

PH2.0-6P: 1 encoder GND, 2 encoder phase A, 3 encoder phase B, 4 encoder
supply (3.3 V from the Grove port), 5 motor +, 6 motor −.

## 3. Kit (from the photos) [Verified: KB 03 "Kit inventory"]

| Item | Notes |
|---|---|
| Raspberry Pi Pico W on a Cytron Robo Pico | — |
| 2 × encoder TT motors | bag label "码盘TT电机 (6V 350RPM 金属输出轴)"; PH2.0-6P lead |
| 2 wheels + ball caster, 2WD aluminium chassis | wheel "looks like the usual ~65 mm TT wheel: measure it" |
| SG90 servo (Cytron T-SG90) | brown GND, red V+, orange signal |
| HC-SR04**P** ultrasonic | back reads HC-SR04P; trigger/echo mode (UART/IIC pads unpopulated) |
| GY-511 (LSM303DLHC) | VIN, 3.3V, GND, SCL, SDA, INT2, INT1, DRDY |
| IR modules (TCRT5000 on "MH-Sensor-Series" boards, LM393 comparator, trim pot) | briefing says 3; photos do not show how many |
| 20 000 mAh power bank | label 5 V 2.1 A per output; "PD66W" is USB-C marketing |
| Raspberry Pi Debug Probe | in the kit; not used in the design (KB 06 G8) |

Photos of other boards (Maker Pi Pico, slotted speed sensors, L298N, plain
TT motor) are not this year's kit [Verified: KB 03].

## 4. Sensor datasheet numbers used by the code

| Part | Fact used | Tag |
|---|---|---|
| TCRT5000 | peak response at 2.5 mm, usable 0.2–15 mm | [Verified: KB 03] |
| LM393 module | no hysteresis on these modules, slow edges can chatter → glitch filter (D-25) | [Verified: KB 03, KB 05 §5] |
| HC-SR04 | 10 µs trigger; cm = echo µs / 58; 2–400 cm; cycle > 60 ms | [Verified: KB 03; `app_config.h` §7 `[DATASHEET HCSR04.pdf …]`] |
| LSM303DLHC | accel 0x19, mag 0x1E; no accelerometer WHO_AM_I; mag IRA/IRB/IRC = 0x48, 0x34, 0x33 ("H43"); mag output order X, Z, Y | [Verified: KB 03; `drv_lsm303.c` header] |
| LSM303DLHC registers set | `CTRL_REG1_A` 0x57 (100 Hz), `CTRL_REG4_A` 0x98 (BDU, ±4 g, high resolution), 2 mg/LSB, `CRA_REG_M` 0x18 (75 Hz), `CRB_REG_M` 0x20 (±1.3 gauss), `MR_REG_M` 0x00 (continuous) | [Verified: `app_config.h` §6, tags `[DATASHEET Table 20/27/3/72/75/78]`] |
| Pico W | RP2040, 264 KB SRAM; GPIO 3.3 V only; GP26–28 = ADC0–2; GP23/24/25/29 radio | [Verified: KB 03] |

## 5. Hardware-dependent values in the code — all UNVALIDATED

[Verified: `app_program/app_config.h` at `58f7d61`] `grep -c UNVALIDATED`
gives 52 lines (the legend and the rule in the header plus the setting
comments). A script that assigns every `#define` to the comment above it
finds **93 of 206 `CFG_` settings under a comment that carries
UNVALIDATED** (list: `handoff_artifacts.zip` →
`evidence/final_main/unvalidated_settings.txt`; a comment can cover several
settings). The header rule, reworded on 13 Oct and attributed correctly on
16 Oct, is quoted verbatim:

> Rule (13 Oct 2026, docs/decisions.md): every value that describes the
> car, its sensors or the course, or that has to be tuned on the car,
> carries [UNVALIDATED] until it is measured, also when a datasheet or a
> decision gave the starting value. [REPORT] and [TEMPLATE] values are
> requirements or fixed settings, not measurements. Register values, bus
> addresses, protocol timings and the scale factors that follow from a
> register setting are definitions copied from a datasheet, not
> measurements, so they keep [DATASHEET] alone.

and the instruction for each tag: "Measure or tune it on the physical car
before relying on it, then change the tag to [MEASURED <date> <who> <log
file>]." **None has been measured.** [Verified: no `MEASURED` tag in the
file]

| Area (owner) | Settings (value) | Basis written in `app_config.h` (abridged; full text in 06 Appendix A) | Assumption / test |
|---|---|---|---|
| Buttons (shared) | `CFG_BUTTON_ACTIVE_LOW` 1, `CFG_BUTTON_DEBOUNCE_MS` 30 | "Robo_Pico.pdf does not show the button circuit; assumed to pull the pin to GND when pressed"; "tactile switches typically bounce for under 10 ms" | FR16 |
| Line sensors (S3) | `CFG_LINE_L/R_WHITE` 300, `_BLACK` 3000; `CFG_LINE_DARK_PM` 800, `_WHITE_PM` 150; `CFG_LINE_JUNCTION_REARM` 4; `CFG_LINE_ADAPT_SHIFT` 6, `_BAND_PM` 150, `_LIMIT` 600; `CFG_LINE_MIN_SPAN` 400; `CFG_LINE_SELFTEST_MIN` 5, `_MAX` 4090 | "typical TCRT5000 module A0 swing at 2-5 mm height … Replace with lab-measured values, A9"; self-test limits "a few counts inside the 12-bit rails" | A9; NF-5 (11) |
| Barcode (S3) | `CFG_BC_DARK_LEVEL` 1; `CFG_BC_GLITCH_US` 1000; `CFG_BC_GAP_RATIO_X100` 150 | "typical MH-sensor module behaviour (D0 low on white, high on black)"; "1 ms is 0.35 mm of travel at 0.35 m/s"; "printed ratio is 3:1 (sample measured, docs/notes/05)" | A3 |
| Steering (S3) | `CFG_STEER_KP_MMPS` 150, `KD_X1000` 300, `MAX_DIFF_MMPS` 200, `SLOWDOWN_PM` 400 | "starting gains; tune on the course" | FR1 |
| Geometry and encoders (S2) | `CFG_WHEEL_DIAMETER_MM` 65, `CFG_TRACK_WIDTH_MM` 130, `CFG_ENC_COUNTS_PER_REV` 528, `CFG_ENC_L/R_INVERT` 0, **`CFG_ENC_PULL_UP` 1** | "typical 65 mm TT wheel"; 528 "placeholder from a common TT encoder motor (11-pulse Hall encoder, 1:48 gearbox …). Measure (A2)"; pull-ups "assumes open-collector Hall outputs … Check the edges on a scope (S2-U37)" | A2 |
| Motors and PID (S2) | `CFG_MOTOR_L/R_INVERT` 0, `MIN_DUTY_PM` 200, `MAX_DUTY_PM` 1000, `RAMP_PM` 100; `CFG_PID_FF_PM_PER_100MMPS` 100, `KP_X1000` 1000, `KI_X1000` 2000, `KD_X1000` 0, `I_LIMIT_PM` 300; `CFG_MOTION_MOVE_MMPS` 150, `TURN_MMPS` 120, `SLOWDOWN_MM` 20 | "TT gear motors typically need about 20% duty to start"; "6 V 350 rpm scaled to the ~5 V supply is about 290 rpm"; ramp "0 to full duty in 0.2 s; tune with the A6 power test" | NFR18, FR19, A6 |
| Navigation (shared) | `CFG_NAV_CRUISE_MMPS` 250, `HUMP_MMPS` 150, `SWEEP_CDEG` 4500, `SENSOR_TO_AXLE_MM` 80, `BYPASS_MAX_MM` 800, `PAST_EXTRA_MM` 150, `CFG_CAR_WIDTH_MM` 150, `STOP_SETTLE_MS` 200, `HUMP_MAX_MS` 5000, `COMPASS_WEIGHT_PM` 20 | cruise/hump "that the car follows the line at them is checked by SYS-17"; "measure the car"; "obstacle size on the course unknown" | A2, A4, A10, FR11 |
| IMU mounting and calibration (S4) | `CFG_IMU_SWAP_XY`, `INVERT_X/Y/Z` 0; `MAG_OFF_X/Y/Z` 0 | "assumes the GY-511 is flat with its X arrow forward"; "zero until a hard-iron calibration is done" | — |
| Hump (S4) | `CFG_IMU_HUMP_ON_CDEG` 300, `OFF_CDEG` 150, `MIN_MS` 100; table {0, 200, 500, 1000, 1500, 2000, 3000} cdeg → {0, 5, 11, 23, 34, 44, 65} mm | "geometric estimate h = track width x sin(roll) … replace with the A4 calibration table" | A4, FR8 |
| Motion class and plausibility (S4) | `CFG_IMU_IMPACT_MG` 1000 (also the calibration jolt, D-81), `ACCEL_MG` 80, `TURN_CDPS` 2000, `STILL_MG` 15, `CLASS_WINDOW` 10; **`CFG_IMU_MIN_G_MG` 300, `MAX_G_MG` 3500** (above MAX: collision while monitoring, restart while calibrating, D-81); `RATE_SMOOTH` 4 | "starting thresholds, tune with logs"; "jolts on the course not measured yet; check with a hard tap (S4-U37)" | FR15, FR17 |
| Safety (S4) | `CFG_WDT_ARM_MAX_WAIT_MS` 15000; `CFG_BUZZER_HZ` 2000, `ALARM_ON/OFF_MS` 250, `CFG_LED_BRIGHTNESS` 32 | "the radio's start-up time is not measured"; "tone for a passive piezo" | A1, NFR6 |
| Ultrasonic (S5) | **`CFG_US_US_PER_CM` 58, `CFG_US_MIN_MM` 20, `CFG_US_MAX_MM` 4000** (tagged by the teammate's `b30ee40`); `CFG_US_ECHO_TIMEOUT_MS` 40 | "the factor follows the speed of sound, which changes with the air temperature, and our 3.3 V module's range is assumption A5; check with S5-U31 and S5-U34"; timeout "must be longer than our module's no-target pulse, or open space reads as invalid (S5-U34)" | A5; D-80 relies on S5-U34 |
| Scan and servo (S5) | `CFG_SCAN_OBSTACLE_MM` 500, `JUMP_MM` 150, `DEPTH_MM` 300, `SIDE_LEFT_DEG` 180, `SIDE_RIGHT_DEG` 0, `SIDE_CLEAR_MM` 400, `BEHIND_COUNT` 2, `SETTLE_PINGS` 1; `CFG_SERVO_US_AT_0` 500, `US_AT_180` 2400, `INVERT` 0 | "obstacle sizes and spacing on the course are not known yet"; "check with boxes of known size (S5-U33)"; "typical SG90 pulse range" | FR10, FR11 |
| Command (S1) | `CFG_CMD_BROKER_DELAY_MS` 300 | "S1-U33 checks that the ack arrives on the old broker" | NFR14 |

### Input pulls set by the code [Verified: driver sources at `58f7d61`]

| Pin | Pull | Why (code comment) | Decision |
|---|---|---|---|
| GP7 barcode D0 | pull to the **white** level (pull-down while `CFG_BC_DARK_LEVEL` is 1) | "an unplugged sensor reads white and never starts a capture instead of floating; the weak internal pull-down (about 50 kOhm) does not stop the module's own pull-up giving a valid high level" | D-73 (teammate, 13 Oct; before it, a pull-up made an unplugged sensor read dark) |
| GP0, GP1, GP16, GP17 encoders | pull-up if `CFG_ENC_PULL_UP` 1 (default) | open-collector assumption, UNVALIDATED | D-04 note, S2-U37 |
| GP5 echo | pull-down | "so an unplugged sensor reads "no echo" (A5: the HC-SR04P is powered from 3.3 V, so Echo never exceeds 3.3 V, NFR11)" | — |
| GP20 START, GP21 RESET | pull-up while `CFG_BUTTON_ACTIVE_LOW` is 1, else pull-down (`nav_task.c:101`, `safety_task.c:253`) | button circuit assumed, UNVALIDATED | FR16 |

[Verified: `docs/decisions.md` D-73] D-73's own limit: idle white is "not
unplug detection" (TPKG C-04): an unplugged sensor simply never starts a
capture; nothing reports it.

## 6. Hardware questions still open (from KB 06, not answered in this session)

[Verified: KB 06 "Questions to ask the instructors"] Which side the barcodes
and the hump are on; whether all junctions are cross junctions with a barcode
before each; whether pressing START after placing the car is allowed; demo-day
SSID and broker; printed bar height and barcode-to-junction distance; hump
height range; obstacle size range and free space. [Unknown] Answers.
