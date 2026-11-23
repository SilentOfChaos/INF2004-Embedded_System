# Lecture digest (W1 to W6) and Barr-C cheat sheet

For the W12 lecture quiz (50 questions in 30 min, closed book, questions mostly
from the weekly practice pools, slightly tweaked) and the practical skills
assessment (spot logic errors and Heisenbugs in code snippets, no AI). Sources
are the slide PDFs in `lectures.zip` plus the lecture 1 transcripts.

## W1 Introduction

- Embedded system: a computer inside a larger system doing a dedicated job,
  usually with real-time constraints and limited resources.
- Real-time means **correct result delivered by its deadline** ("a late answer
  is a wrong answer"), not "fast". Non real-time (TV remote, lawn sprinkler,
  window lifter), soft (phone, air-con, fridge), hard (reactor temperature,
  airbag, flight control). Requirements must state the deadline; "as fast as
  possible" is not a requirement.
- Hardware options: µP, µC (µP plus GPIO, memory, timers, ADC, serial), SoC
  (several embedded systems on one chip), DSP, ASIC, FPGA, NPU, GPU.
- Case studies: Therac-25 (race condition between UI and control, a Heisenbug,
  lethal overdoses, 1985–87); Toyota unintended acceleration (spaghetti code);
  Ariane 5 (64-bit float to 16-bit int overflow in code reused from Ariane 4,
  destroyed 37 s into flight); Boeing 737 MAX MCAS; Mars Climate Orbiter
  (units).
- Build chain: compiler (C to object), assembler (.s to object, one-to-one),
  linker guided by a linker script (.ld), loader/locator; cross compiling
  (host builds for another target) vs native. Pico: SDK + compiler → UF2 →
  BOOTSEL bootloader writes flash.
- Startup code (crt0.s) runs before `main`: disable interrupts, copy initialised
  data to RAM, zero uninitialised data, set up stack, set up heap, enable
  interrupts, call `main`.
- Embedded C: exact-width types (`uint8_t`, `int16_t`, `uint32_t`); overflow
  (`uint8_t x = 255; x++` → 0), truncation (`uint8_t y = 300` keeps the low
  8 bits), mixed signed/unsigned comparisons. Bit ops: `REG |= BIT(3)` set,
  `REG &= ~BIT(3)` clear, `REG ^= BIT(3)` toggle, `REG & BIT(3)` test.
  `const` (do not modify), `volatile` (may change outside normal flow: hardware
  or ISR), `const volatile` (read-only status register). **volatile does not
  make a read-modify-write atomic.** Memory-mapped I/O through
  `(*(volatile uint32_t *)ADDR)`. Scope: local, static local (keeps value
  between calls), static file scope (module-private), extern (documented
  sharing only). Structs may contain padding; never send raw structs over a
  link, serialise explicitly (byte order too).

## W2.1 GPIO

- Input levels: above VIH(min) is 1, below VIL(max) is 0, in between undefined;
  devices need a common ground; a floating input has no defined value.
- Pull-up: open = 1, pressed = 0 (active-low). Pull-down: the reverse.
- Push-pull output drives high or low; it is a limited current source, not a
  supply. LED: I ≈ (3.3 − 2.0) V / 1 kΩ ≈ 1.3 mA.
- One pin, many functions (pin multiplexing); one owner per pin.
- Mask example: 0xA5 | 0x08 = 0xAD. Use `|=`, `&= ~`, `^=` so other bits are
  preserved; plain `=` overwrites the whole port.
- Configure: identify pin and function, preload a safe level, enable the
  driver, verify. SDK: `gpio_init`, `gpio_put`, `gpio_set_dir`, `gpio_pull_up`.
- Edges vs levels: count accepted transitions, not loops that see low.
  Debounce: accept a state after it stays stable (example 30 ms).
- PIO (RP2040): 2 blocks × 4 state machines, any GPIO, 9 instructions
  (`in out push pull mov irq wait jmp set`); runs independently of the CPU.

## W2.2 Interrupts

- With T the shortest time between events, D the deadline, p the polling
  period, c the handling time, h the hardware response time: polling needs
  2c ≤ c + p ≤ D ≤ T; interrupts need h + c ≤ D ≤ T.
- Poll for short predictable waits; interrupt for sporadic, time-sensitive
  events; DMA plus interrupt for sustained streams.
- Source (peripheral) raises an IRQ (request) serviced by an ISR (handler).
  Three gates: source enabled, controller (NVIC) enabled, CPU mask and
  priority allow. Pending is not running; masking does not erase an event.
- Vector table; reset vector; context saved on the stack and restored.
- One pending bit is not a queue: three events may give one service.
- Service (clear) the cause before returning, or the ISR re-enters at once.
- Configure first, enable last: keep source disabled, set mode, install
  handler and initialise shared state, clear stale status and set priority,
  then enable.
- ISRs: short, bounded, no `printf`, no long loops, no blocking; capture,
  acknowledge, notify, and process later in a task.
- Shared counter race: main reads 7, ISR makes 8, main writes 8; expected 9.
- SDK: `irq_set_exclusive_handler`, `irq_set_priority`, `irq_set_enabled`
  (affects the calling core), `gpio_set_irq_enabled_with_callback`.
- ISR load ≈ event rate × time per ISR (1 kHz × 20 µs = 2%; 20 kHz × 20 µs =
  40%). RP2040 NVIC supports tail-chaining and late arrival.

## W3.1 Serial communication

| | UART | SPI | I²C |
|---|---|---|---|
| Timing | Asynchronous (baud rate, start/stop bits) | Synchronous | Synchronous |
| Duplex | Full | Full | Half |
| Topology | Point to point | One master, many slaves (CS lines) | Multi-master bus, 7/10-bit addresses |
| Typical speed | 9.6–115.2 kbps | 10–50+ MHz | 100 kHz / 400 kHz (1 MHz FM+) |
| RP2040 | 2 × UART, 32-byte FIFOs, 5–8 data bits, parity, 1–2 stop bits | 2 × SPI, 8-entry FIFOs, 4–16-bit frames | 2 × I²C, 16-entry FIFOs |

- UART idles high; a low start bit begins a frame. Example: 7 data + 3 control
  = 10 bits per character; 100 characters/s = 1000 baud. Flow control RTS/CTS or
  XON/XOFF. RS-232 (±15 V) and RS-485 (differential) for distance and noise.
- SPI modes: 0 = CPOL 0, CPHA 0 (idle low, sample rising); 1 = 0,1 (idle low,
  sample falling); 2 = 1,0 (idle high, sample falling); 3 = 1,1 (idle high,
  sample rising). CS is active low.
- I²C: open-drain lines with pull-ups; master sends a 7-bit address and the
  matching slave ACKs; clock stretching lets a slow slave hold SCL low;
  arbitration: a master sending 1 that sees 0 backs off.

## W3.2 Timers and PWM

- Blocking `sleep`/`delay` stops all other work. A prescaler divides the clock
  (125 MHz / 125 = 1 MHz, 1 µs per tick).
- Up, down and centre-aligned (up/down, triangle) counting; centre-aligned PWM
  is smoother for motors. Interrupts on overflow or compare match. Input
  capture timestamps an external edge in hardware. Timer-triggered DMA moves
  ADC samples with no CPU. Cascading builds wider timers (the Pico has a native
  64-bit timer). RTC keeps calendar time; a watchdog resets a hung system.
- Traps: changing the compare value without shadow registers (runaway timer);
  torn multi-byte reads; **N − 1 rule** (write 99 to divide by 100); forgetting
  to clear the interrupt flag (infinite ISR); assuming the timer clock equals
  the CPU clock.
- PWM: f = 1 / T, duty D = T_on / T, average ≈ D × supply.
  f_PWM = f_timer / ((PSC + 1)(ARR + 1)), duty ≈ CCR / (ARR + 1).

## W3.3 ADC

- N = 4095 × V / V_ref; at 3.3 V, 1.23 V → 1526. LSB = 3.3 / 4096 ≈ 0.806 mV;
  maximum quantisation error ±½ LSB ≈ ±0.403 mV.
- RP2040: one 12-bit SAR converter behind a 5-input mux (GPIO26–29 = inputs
  0–3, temperature sensor = 4); 48 MHz clock, 96 cycles per conversion = 2 µs,
  500 kSPS; 8-entry FIFO; DMA; effective resolution 8.7 ENOB.
- ADC_CS bits: EN (0), TS_EN (1), START_ONCE (2), START_MANY (3), READY (8),
  AINSEL (14:12). RESULT register for occasional polled reads; FIFO for
  continuous capture (threshold 1 = low latency/high IRQ rate, 8 = no margin;
  flags OVER, UNDER, ERR).
- Nyquist "zero-crossing trap": sampling at exactly 2× can read a flat line;
  oversample 2.5–3× and trigger from hardware timers, not software delays.
- Front end: `adc_gpio_init` (high impedance, no pulls, digital input off),
  anti-alias low-pass before sampling, keep inputs within 0..ADC_AVDD.
- Two-point calibration for offset and gain, done in a task with fixed-point
  maths. Debug method: known DC, repeat 1000 samples (mean/min/max/spread),
  check reference, vary timing, stress the consumer, compare polling/IRQ/DMA.

## W4.1 RTOS (µT-Kernel 3.0)

- Design for the latest acceptable response (worst case), not the average.
- µT-Kernel 3.0: compact, portable, IEEE 2050-2018; API `tk_<operation>_<object>`.
- Task states: ready, running, blocked (waiting), terminated. Preemptive
  priority scheduling; equal priorities FIFO or time-sliced; priority encodes
  urgency.
- Critical sections: short; never block, allocate, log or do slow I/O inside.

| Primitive | Represents | Calls | Use when |
|---|---|---|---|
| Semaphore | Count, no owner | `tk_cre_sem`, `tk_wai_sem`, `tk_sig_sem` | N identical resources, producer/consumer, event counting |
| Mutex | Ownership + exclusion | `tk_cre_mtx`, `tk_loc_mtx`, `tk_unl_mtx` | Protect one shared invariant; priority inheritance `TA_INHERIT` or ceiling `TA_CEILING` against inversion |
| Event flag | Condition bits | `tk_set_flg`, `tk_wai_flg` (`TWF_ORW` any, `TWF_ANDW` all) | Wait for combinations; no payload |
| Message buffer | Copied bytes | `tk_snd_mbf`, `tk_rcv_mbf` | Variable-size messages, simple lifetime |
| Mailbox | Queued pointers | `tk_snd_mbx`, `tk_rcv_mbx` | Large data, zero copy; lifetime must be agreed |
| Fixed pool | Fixed blocks | `tk_get_mpf`, `tk_rel_mpf` | Bounded allocation, no fragmentation |
| Time | Delay, periodic, one-shot | `tk_dly_tsk`, `tk_cre_cyc`/`tk_sta_cyc`, `tk_cre_alm`/`tk_sta_alm` | Avoid busy waiting; timeouts are normal design |

- For every kernel object define invariant, owner, capacity, timeout and
  worst-case blocking. Allowed calling contexts (for example from an ISR) are
  implementation questions: check before coding.

## W4.2 State machines

- Correct response = current state + event. Elements: state, event, guard,
  action, transition. Handle one event at a time; never wait inside an action
  (start the work, handle completion or timeout as a new event).
- Good fit: modes, protocols, handshakes, retries, timeouts, lifecycle, fault
  recovery, or many flags that secretly encode modes. Poor fit: pure
  calculation, linear algorithms, bulk data, state explosion.
- A state table removes ambiguous branches. Implementations: lookup table,
  `switch (state)`, superloop, or under an RTOS **one task owns the FSM** and
  blocks on an event queue (do not make one task per state; ISRs only enqueue).
- Keep filter maths separate from the FSM that decides when values are
  trustworthy (RESET → WARM_UP → VALID → DEGRADED).
- Test every state/event pair, including repeats, timeouts, invalid events,
  reset and recovery.

## Lecture 5: Requirements and design (Mr Aw)

- Requirements say what and how well, never how. FBI Virtual Case File ($170M,
  2005): undefined requirements, repeated changes, 80% scope creep.
- Good requirement: clear and focused; verifiable ("shall"; "should" cannot
  fail a test; numbers with tolerances; yes/no outcome); traceable (unique ID);
  justified and coherent. Problem types: no ID, untestable, imprecise, no
  tolerance, overly complex, describes implementation.
- NFR categories: emergent properties, performance and timeliness, safety /
  security / dependability (including "shall not"), resource budgets (size,
  weight, power), design and project constraints (standards, mandated platform).
- Product requirements (customer language) vs engineering requirements
  (measurable, traced to a product requirement).
- Approaches: text list, UML use cases, user stories, functional
  decomposition, prototyping.
- V-model; architecture = boxes (modules) and arrows (interfaces), every box
  and arrow gets an ID; HLD = architecture (nouns) + sequence diagrams (verbs);
  one sequence diagram per scenario including edge and error cases, each with
  pre- and postconditions; a unified statechart per object combines all
  scenarios. Trace check: every requirement has a box, every box serves a
  requirement ("no gaps, no gold plating").

## Lecture 6: Traceability and testing (Mr Aw)

- Verification = built right (to the spec); validation = built the right thing
  (user needs).
- Chain: requirement → HLD element → LLD function → code → test case, kept in a
  traceability matrix that exposes gaps and gold plating.
- Formal testing: workload, oracle for expected output, run, compare. In a
  healthy process **under 5%** of bugs are first found at system test.
- Unit tests: one module, white box (code paths) plus black box (requirements);
  boundaries **at and just outside** each limit (index 0..9: test −1, 0, 9, 10);
  exceptional inputs (NULL, NaN, Inf, empty); Unity framework runs on PC or
  Pico. 100% coverage ≠ fully tested.
- Peer review catches cheap bugs (the `>` vs `>=` example broke R-3.3).
- Integration tests: interfaces and messages, traced to HLD and sequence
  diagrams; an interface has a contract (units, range, rate); fault injection
  (stop messages, wrong units, out-of-range, wrong rate).
- Tolerance bands define tests: just below, just above, then measure where it
  actually triggers (3 ± 0.2 s: test 2.7 s, 3.3 s, then timestamp).
- System tests on real hardware with off-nominal conditions, performance,
  stress/soak, safety, environment. Acceptance testing confirms, not
  discovers. Writing tests is the cheapest time to find missing requirements.
- Defects cluster (about 80% in 20% of modules); cost escalates from failed
  unit test up to catastrophic failure.

## Barr-C (2018) cheat sheet

| Area | Rule |
|---|---|
| Language | C99; no keyword-renaming macros |
| Layout | Lines ≤ 80 characters; braces always, each on its own line; 4-space indentation, **no tabs**; one statement per line; LF line endings; file ends with an end-of-file comment |
| Spacing | One space after `if`, `while`, `for`, `switch`, `return`; spaces around binary and assignment operators; none inside parentheses; function *declaration* has a space before `(` |
| Parentheses | Do not rely on precedence; each operand of `&&` / `||` in parentheses unless a single identifier |
| Comments | `//` allowed; never comment out code (use `#if 0`); full sentences; Doxygen-style; `WARNING:`, `NOTE:`, `TODO:` markers; spell out assumptions |
| Modules | Lower-case file names, unique in 8 chars; one `.h` per `.c` with include guards; no storage in headers; `.c` includes its own `.h`; the file with `main` has "main" in its name |
| Types | Fixed-width integers; no `short`/`long`; `char` only for strings; new types lower case ending `_t`, via typedef, public ones prefixed with the module name |
| Signedness | No bitwise ops on signed data; do not mix signed and unsigned; unsigned constants end in `u` |
| Floats | Avoid (prefer fixed point); if used: `f` suffix, never test for equality, check `isfinite()` |
| Structs | Prevent padding in anything sent over a bus or network |
| Booleans | `bool`; convert with relational operators, not casts |
| Functions | Lower case with underscores, ≤ 31 chars, module prefix (`sensor_read()`), noun-verb; ≤ 100 lines; one exit preferred; private functions `static`; tasks end in `_task`/`_thread`; **ISRs end in `_isr`** and are `static`/at the end of the driver |
| Macros | Upper case; prefer functions; parenthesise body and each parameter |
| Variables | Lower case, ≥ 3 chars, descriptive; prefixes `g_` global, `p_` pointer, `pp_` pointer-to-pointer, `b_` boolean, `h_` handle; initialise before use; pointers start as NULL; no comma in declarations |
| Keywords | `static` for everything module-private; `const` wherever possible; `volatile` for ISR/thread-shared globals and hardware registers; never `auto`/`register`; avoid `goto` and `continue`; no `abort`, `exit`, `setjmp`, `longjmp` |
| Statements | ≤ 2 nested if levels; no assignment in conditions; `if … else if` ends with `else`; `switch` has `default`; fall-through commented; no magic numbers in loop bounds; infinite loop as `for (;;)`; empty loop body holds a comment; constant on the left of `==` (`if (NULL == p_obj)`) |
| Casts | Each cast commented with why the value range is safe |
