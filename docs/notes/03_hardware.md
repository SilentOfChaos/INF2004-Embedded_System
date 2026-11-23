# Hardware

Sources: `Robo_Pico.pdf`, `Robo Pico Board Datasheet.pdf` (same content plus a
power-tree page), `RP-008312-DS-2-pico-w-datasheet.pdf`, sensor datasheets,
`PicoCar.pptx`, the 28 photos, and design review §2 / Fig 1.

## Kit inventory (from the 28 Sep photos)

Current kit (photos `*_15-11-52`, `*_15-39-00`):

| Item | Notes |
|---|---|
| Raspberry Pi Pico W on a Cytron **Robo Pico** | Photos 1–2 of `15-39-00` |
| 2 × black "encoder TT motors" | Bag label: 码盘TT电机 (6V 350RPM 金属输出轴), i.e. encoder TT motor, 6 V, 350 rpm, metal output shaft. PH2.0-6P lead to Dupont |
| 2 wheels + metal ball caster, purple 2WD aluminium chassis | Wheel diameter looks like the usual ~65 mm TT wheel: measure it |
| SG90 servo (Cytron T-SG90) | Brown GND, red V+, orange signal |
| HC-SR04**P** ultrasonic + acrylic holder | Back of board reads HC-SR04P; UART and IIC mode pads (R5, R4) are unpopulated, so it is in plain trigger/echo mode |
| GY-511 (LSM303DLHC) | Pins: VIN, 3.3V, GND, SCL, SDA, INT2, INT1, DRDY |
| IR reflective modules (TCRT5000 on an "MH-Sensor-Series" board); the briefing says 3 | Pins VCC, GND, D0, A0; LM393 comparator with a 3362 trim pot setting the D0 threshold (photos 5–7 of `12-48-00`; photo 7 is the comparator side of the same board type). The kit photos do not show how many were issued |
| 20 000 mAh power bank | Label: input 5 V 2.1 A, outputs 5 V 2.1 A (OUT1 green, OUT2 red); the "PD66W / 66W" on the front is marketing for USB-C PD, not the USB-A rating |
| USB-A to bare-wire lead | Power bank to the Robo Pico VIN terminal |
| Raspberry Pi **Debug Probe** (SWD + UART) | In the kit box; not mentioned in the design yet |
| Orange bracket, black 3D-printed box, standoffs, tools | Probably sensor bracket and power-bank holder: confirm |

Photos that are **not** this year's kit (the zip says "not all are relevant"):
Maker Pi Pico board (`12-48-00` 1–2), slotted photo-interrupter speed sensors
(`12-48-00` 3–4, all of `15-41-20` and `15-53-04`), L298N motor driver
(`12-48-00` 8–9), yellow TT motor without encoder (`12-48-00` 12). The design's
8.1 entry for Buddy 2 records that the first AI design wrongly assumed these TT
motors with single-channel encoders.

## Robo Pico board

### Grove ports (`Robo_Pico.pdf` p.3)

| Grove | Pins | Alternate functions |
|---|---|---|
| 1 | GP0, GP1 | UART0 TX/RX, I2C0 SDA/SCL, PWM0 A/B |
| 2 | GP2, GP3 | I2C1 SDA/SCL, PWM1 A/B. **Shared with the Maker port** |
| 3 | GP4, GP5 | UART1 TX/RX, I2C0, PWM2 A/B |
| 4 | GP16, GP17 | UART0, I2C0, PWM0 A/B |
| 5 | GP6, **GP26** | GP26 = ADC0. **GP26 is shared with Grove 6** |
| 6 | GP26, GP27 | ADC0, ADC1, I2C1, PWM5 A/B |
| 7 | GP7, GP28 | GP28 = ADC2, PWM6 A. GP28 is also the VBAT sense pin if the back jumper is soldered (VADC2 = VBAT/2) |

Every Grove port carries 3.3 V and GND. Total 3.3 V draw across all Grove
ports: 300 mA max.

### Fixed on-board functions

| Function | Pin |
|---|---|
| Motor 1 (M1A, M1B) | GP8, GP9 (PWM slice 4 A/B) |
| Motor 2 (M2A, M2B) | GP10, GP11 (PWM slice 5 A/B) |
| Servo ports 1–4 | GP12, GP13, GP14, GP15 (V+ = supply voltage) |
| NeoPixel RGB LED | GP18 |
| Programmable buttons | GP20, GP21 |
| Piezo buzzer (with mute switch) | GP22 |
| Status LEDs | One per Grove GPIO, lit when the pin is high |

Motor driver truth table (p.8): A low + B low = **brake**; A high + B low =
forward; A low + B high = backward; both high = **coast** (outputs Hi-Z). So
"stop" means both pins low, not both pins high and not "PWM disabled" with an
unknown pin state. Driver PWM: 20 kHz max. Motor current: 1 A continuous,
1.5 A peak (< 5 s) per channel. Motor test buttons on the board run a motor at
full speed.

### Power (p.6 and the power-tree page)

- Inputs: VIN terminal, micro-USB, 1S LiPo connector; the board takes the
  highest of them (3.6 to 6 V).
- **Motors and servos run directly from the selected input** (minus ~0.4 V if
  only USB is connected). There is no 5 V rail.
- A buck-boost converter (800 mA) makes 3.3 V for the Pico W, Grove/Maker ports,
  buzzer, NeoPixel and LEDs.
- Digital input low ≤ 0.8 V, high ≥ 2.0 V; analog input 0 to 3.3 V.

With the kit power bank on VIN the motors and servo see about 5 V (the motors
are rated 6 V, so expect roughly 290 rpm no-load instead of 350).

### RP2040 PWM slices in use (slice = (GPIO / 2) mod 8)

| Slice | Pins on this board | Our use |
|---|---|---|
| 0 | GP0/GP1, GP16/GP17 | Encoder inputs (not PWM) |
| 1 | GP2/GP3, GP18/GP19 | I2C, NeoPixel (PIO) |
| 2 | GP4/GP5, GP20/GP21 | Ultrasonic, buttons |
| 3 | GP6/GP7, GP22 | **Buzzer PWM on 3A**; GP7 barcode input on 3B |
| 4 | GP8/GP9 | Motor 1 |
| 5 | GP10/GP11, GP26/GP27 | Motor 2; GP26/27 are ADC inputs |
| 6 | GP12/GP13, GP28 | Servo 1 (50 Hz) on 6A; GP28 also maps to 6A |
| 7 | GP14/GP15 | Spare servo ports |

Consequences: slice 3 cannot be used to count barcode edges in PWM input mode
while the buzzer uses it, and GP28 cannot be a PWM output while the servo
uses slice 6A. Neither is planned, but it matters for requirement changes.

## Our pin map (design review §2, checked against the board)

| Device | Signal | Pin | Port | Check |
|---|---|---|---|---|
| Line IR left | A0 | GP26 / ADC0 | Grove 6 | OK. One Grove 6 cable split to both line modules |
| Line IR right | A0 | GP27 / ADC1 | Grove 6 | OK |
| Barcode IR | D0 | GP7, GPIO IRQ | Grove 7 | OK. The same cable also carries GP28 (ADC2), so the module's A0 can be wired too at no cost |
| GY-511 | SDA, SCL | GP2, GP3 (I2C1) | Grove 2 | OK; leave the Maker port empty. Design feeds 3.3 V into the module's VIN pin |
| HC-SR04P | Trig, Echo | GP4 out, GP5 IRQ | Grove 3 | OK; Echo is ≤ 3.3 V because the sensor runs from 3.3 V |
| Left encoder | A, B | GP0, GP1 | Grove 1 | OK. Template `CRASH_REPORT=1` builds use GP0/GP1 (UART), so unplug the encoder for those |
| Right encoder | A, B | GP16, GP17 | Grove 4 | OK |
| Motors | M1A/M1B, M2A/M2B | GP8–GP11 | Terminals | OK |
| Servo | Signal | GP12 | Servo 1 | OK |
| START / RESET | Buttons | GP20 / GP21 | On board | OK |
| Status LED / buzzer | | GP18 (PIO) / GP22 (PWM) | On board | OK |
| Power | | VIN + GND | Terminal | Power bank OUT1 through the USB-to-terminal lead |

Spare: Grove 5 GP6 (its GP26 is taken), GP28 on Grove 7 (keep the VBAT jumper
open), servo ports 2–4 (GP13–GP15). Rough Grove 3.3 V budget: 3 IR modules
(~20 mA LED each), 2 encoders, HC-SR04P, GY-511, comfortably under 300 mA.

## Encoder motor connector (PicoCar.pptx slide 2)

PH2.0-6P: 1 encoder GND, 2 encoder phase A, 3 encoder phase B, 4 encoder supply
(3.3 V from the Grove port), 5 motor +, 6 motor −. Counts per wheel turn are
unknown until measured (assumption A2).

## Pico W (RP-008312-DS-2)

- RP2040, dual Cortex-M0+ up to 133 MHz, **264 KB SRAM**, 2 MB QSPI flash.
- GPIO is 3.3 V only. GP26–GP28 are the usable ADC inputs (ADC0–ADC2).
- GP23, GP24, GP25 and GP29 belong to the CYW43439 radio (power, SPI data/IRQ,
  CS, clock). VSYS/3 on ADC3 (GP29) can only be read when no radio transfer is
  in progress. The on-board LED is on the radio chip (WL_GPIO0).
- ADC reference is the filtered 3.3 V supply (201 Ω + 2.2 µF), so expect about
  30 mV offset; forcing the SMPS into PWM mode (WL_GPIO1 high) reduces ripple.

## Sensor datasheet numbers

| Part | Key facts |
|---|---|
| TCRT5000 | Reflective IR, 950 nm, daylight-blocking filter. Peak response at **2.5 mm**; usable 0.2 to 15 mm (>20% relative output) |
| LM393 (on the IR module) | Open-collector comparator, ~1.3 µs response; the cheap modules have no hysteresis, so slow edges can chatter |
| `Infrared-Reflective-Sensor-UserManual.pdf` | Describes a Waveshare **ST188** module (DOUT, AOUT, GND, VCC, 3.0–5.3 V). Not our part; the team noted this in §8.1 |
| HC-SR04 (5 V original datasheet) | 2 to 400 cm, 15° cone, 10 µs trigger, distance (cm) = echo µs / 58, use a measuring cycle over 60 ms. Our HC-SR04P variant runs from 3 to 5.5 V |
| LSM303DLHC | Accelerometer I2C 0x19, magnetometer 0x1E. Accel ODR 1, 10, 25, 50, 100, 200, 400 Hz (more in low-power); full scale ±2/4/8/16 g at 1/2/4/12 mg/LSB. Mag ODR 0.75, 1.5, 3, 7.5, 15, 30, 75, 220 Hz. **No accelerometer WHO_AM_I**; magnetometer IRA/IRB/IRC read 0x48, 0x34, 0x33 ("H43"). Mag output order is X, Z, Y, high byte first. Auto-increment needs bit 7 of the sub-address set. Chip supply 2.16 to 3.6 V; the GY-511 board has a regulator for 3 to 5 V |
| SG90 | Standard 50 Hz servo, powered from VIN (~5 V) via the servo port |
