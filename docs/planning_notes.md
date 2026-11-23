# ICT2212 Student Project

The project is not about just making a car move but a combination of:

- How do you design the embedded software?
- How do you integrate the sensors?
- How do you make a decision in real time?

---

## Deliverables (whole project)

1) **[W6] Requirements + Design document**  
2) **[W8] Tribunal 1 (15%) - Zoom, lecture slot**  
3) **[W10] Subsystem check-in - HURDLE, lab session**  
4) **[W13] Tribunal 2 (15%) - Zoom**  
5) **[W13] Final run: demo + final code + testing - (15%), lab session**

**45% in total.**

> **Hurdle = miss it or submit late, 5% off TOTAL group project marks every day**

---

## W6 Deliverables

"Week_6_Design_Review_Template.docx" on xSITe with 9 sections:

1. Requirements (FR + NFR)
2. Hardware pin layout (incl. power connections)
3. RTOS task table + priority justification
4. Task interaction diagram
5. System-level statechart
6. Subsystem statecharts
7. Assumptions — min 1 per buddy, each with risk + how you'll validate
8. AI-supported design evidence (8.1/8.2/8.3)
9. Design consistency

> **No code & hardware but my friends are alrdy building the car idk why - wy, to clarify tmr**

---

## Buddy split (fixed by prof, 5 people)

| Buddy | Subsystem |
|---|---|
| **Buddy 1** | WiFi + MQTT telemetry and commands |
| **Buddy 2** | Motion control, motors + encoders, PID |
| **Buddy 3** | IR line following + barcode decoding |
| **Buddy 4** | IMU, tilt, hump, collision |
| **Buddy 5** | Ultrasonic scan + obstacle avoidance |

---

## tmr

ask them to read the project details and decide the buddy split

---

## Week 4

**(21st to 27th Sept)**

OTOT on your own subsystem.

Each person owes by end of W4:

- your FRs and NFRs for your subsystem
- at least 1 assumption: what you're assuming, risk if wrong, how you'll test it
- your own row in section 8.1

> **8.1 is the one nobody can do for you. It asks for YOUR prompt, which AI ideas were useful, which were wrong, and what we decided instead. Tribunal 1 will ask you to explain your own row. Do not copy each other.**

**tmr(Mon) lecture** is Embedded OS + State Machine, both directly needed for sections 3 and 5.  
**Lab** is PWM + ADC.

---

## Week 5

**(28th Sept to 4th Oct)**

Mon lecture is literally Requirements & Design, so go for that one.

Mid-week: one person merges everything into the template. I can do the merge if nobody else wants it.

By end of w5 the draft should be complete. Then **EVERYONE reads the whole doc, not just your own part.** Tribunal 1 is a discussion and the prof can ask anyone about anything.

Meeting only if we're stuck, otherwise just clarify in tele.

---

## Week 6

**(5th to 11th Oct)**

Mon lecture is Traceability & Testing, useful for section 9.

Check the assumptions we wrote that we can actually check, and update the doc rather than leaving them all open.

Submit Sat.

---

## Submission deadline

**Sat 10th Oct 2359**

Aim to be done Friday night latest. Saturday deadline means nobody is around to fix it if something breaks.

---

## By ai

### 3 things to decide early, they affect parts and code

#### 1) Battery

Robo Pico ties motor and servo voltage straight to the input and has no 5V regulator. A 1S LiPo underpowers the servo and leaves the ultrasonic sensor with no valid 5V supply once USB is unplugged. We want a 5-6V pack on VIN. Sort this before anyone tunes anything.

#### 2) RTOS

RTOS is micro T-Kernel, NOT FreeRTOS. Prof said FreeRTOS gets penalised. He is not teaching uT-Kernel, we map it from the FreeRTOS lab ourselves. Repo will be given.

#### 3) AI-written code

Prof said most code will be AI-written and he's fine with that, BUT you must be able to explain every line in your own subsystem. If he asks and you can't answer, you lose marks personally.
