# Testing

All test records for the project live in this folder. A test only counts as
done when it is **written down here with its result and a saved log**.

```
testing/
├── README.md              this guide
├── TRACEABILITY.md        IS19: requirement → design → code → test matrix
├── subsystem1/            one folder per subsystem, kept by its owner
│   ├── TEST_LOG.md        test cases + results
│   └── logs/              saved console output, photos, scope captures
├── ...
├── subsystem5/
├── navigation/            IS19: the shared Navigation task (NAV-U.. IDs)
├── common/                IS19: the shared helpers in app_program/common (COM-U.. IDs)
├── host/                  IS19: host unit tests for the pure logic modules
├── integration/           tests across two or more subsystems (shared)
└── system/                tests of the whole product against its requirements (shared)
```

## Three levels of test

| Level | ID format | Tests what | Written by | Where |
|---|---|---|---|---|
| Unit | `S1-U01`, `S2-U01`, … | one subsystem on its own | the subsystem owner | `subsystemN/TEST_LOG.md` |
| Integration | `INT-01`, … | an interface between subsystems (TEAM.md section 5) | the owners on both sides | `integration/TEST_LOG.md` |
| System | `SYS-01`, … | the finished product against a requirement | whole team | `system/TEST_LOG.md` |

Test IDs never change and are never reused, even if a test is dropped.

IS19 numbering inside each unit-test log: `-U01`..`-U19` host tests,
`-U20`..`-U29` on-target `TEST_CHECK`s, `-U30`..`-U39` manual tests, and
`-U40`..`-U59` more host tests (added for the minimum-bit change, when
the first block was full; later host tests use the first block while it
has room).
[`TRACEABILITY.md`](TRACEABILITY.md) lists, for every requirement, the
design, the code and these tests (Lecture 6). Whenever you add or change a
test, put the requirement in its *Requirement / interface* column and
update that table in the same pull request.

## Host unit tests (IS19)

The pure logic modules (decoders, state machines, maths, message formats)
include no kernel or SDK headers, so they are also tested on a laptop with
any C99 compiler. These tests use IDs `S1-U01`..`S5-U19`, `NAV-U01`..`U19`
and `COM-U..`; the on-target tests start at `-U20`.

```sh
cd testing/host
make            # build and run; exit code 1 if any test fails
make SAN=1      # the same with AddressSanitizer and UBSan (gcc / clang)
make CC=clang   # the same with another compiler
```

Each test prints one `[TEST] <ID> PASS|FAIL` line, the same format as the
on-target tests. `testing/host/ref/` holds frozen copies of code from
before the minimum-bit change, renamed `ref_...`; only the tests that
compare the old and the packed versions use them (for example S3-U43
and S1-U43), and they are never built into the firmware. Save the
output to the subsystem's `logs/` and record the result as in step 5
below.

The makefile rebuilds whenever `SAN`, `CC` or the flags change, and the
output starts with `[INFO]` lines naming the compiler and the flags, so a
saved log shows what was really run: a sanitized run lists
`-fsanitize=address,undefined`. With `SAN=1`, AddressSanitizer's leak check
runs at exit on Linux; it cannot run under some debuggers and containers
(it then prints a LeakSanitizer error and the run fails). There, run
`ASAN_OPTIONS=detect_leaks=0 make SAN=1` and write in the result's notes
that the leak check was off. Host tests check logic only: they are not a
result for any timing, sensor or hardware requirement.

## How to run and record a test

1. **Define it first.** Add a row to the *Test cases* table in the right
   `TEST_LOG.md`: ID, what it checks, how, and the expected result.
2. **Automate it if you can.** Add a `TEST_CHECK` with the same ID to
   `app_program/subsystemN/subsystemN_test.c` (see `app_program/test_report.h`).
   A test that needs eyes or instruments (an LED blinks, a motor turns, a
   scope shows 1 kHz) is a *manual* test. Write its steps in the table instead.
3. **Build the test image and flash it.**
   ```sh
   cd build_make
   make TEST=1 -j8        # -> mtk3pico_smp1_usb_cdc_wifi_test.uf2
   ```
   The test image runs every subsystem's tests at power-up and prints one
   `[TEST] <ID> PASS|FAIL` line per check. (A normal `make` leaves the tests out.)
4. **Save the console output** to `logs/`, named `YYYY-MM-DD_<what>.txt`:
   ```sh
   # macOS (find the port name with: ls /dev/cu.usbmodem*)
   cat /dev/cu.usbmodem1101 | tee ../testing/subsystem1/logs/2026-10-14_unit.txt
   # Linux / WSL with usbipd
   cat /dev/ttyACM0 | tee ../testing/subsystem1/logs/2026-10-14_unit.txt
   ```
   Start the command first, then reset the Pico. Press Ctrl-C when it is done.
   On Windows with PuTTY, use *Session → Logging → All session output*.
   For manual tests, save photos or scope screenshots in `logs/` the same way
   (keep each file under a few MB). Videos go in `videos/` instead (see below).
5. **Record the result.** Add a row to the *Results* table: date, test ID,
   the commit you tested (`git rev-parse --short HEAD`), who ran it,
   PASS/FAIL, and the log file. Commit the log and the table together.
   For a measured test also write in *Notes*: the build options (for
   example `NET=lwip MQTT=1`) and any setting changed in `app_config.h`;
   the set-up (surface, lighting, speed, which barcode or obstacle); what
   was measured, and with what; the values; and the tolerance they were
   compared with. For network tests, say which broker was used (built in
   from `config/wifi_secrets.h`, or set by the `broker` command, D-16).
   Logs show the Wi-Fi name but never the password: never commit
   `config/wifi_secrets.h` or paste the password anywhere.

A requirement is met only when its tests have PASS rows with evidence. A
row in `TRACEABILITY.md`, a passing host test or an UNVALIDATED starting
value in `app_config.h` is not a result for a physical requirement.

## Measurement build (minimum-bit change, D-82)

For the before and after figures of the minimum-bit change, build the
same profile from both commits with `CFG_STATS_ENABLE` set to 1 in
`app_program/app_config.h` (do not commit that setting), run the same
test on the car, and save the console output. Every 5 s it prints:

```
stats: task period_min_us period_max_us work_max_us releases stack_used stack_size
stats: <task> <min> <max> <work> <count> <used> <size>
stats: isr dispatch_max_us <us>
stats: isr pin <gpio> max_us <us>
stats: queue <name> bytes <size> free <now> free_min <least>
```

A period is the time between two starts of a task's periodic work; work
is the time from that start to the end of the work. `stack_used` is the
deepest point the task ever reached, counted from the top of its stack
(`stack_size` is what the kernel allocated). Static RAM comes from the
build itself: `arm-none-eabi-size` on the `.elf` (`data` + `bss`).
Record both runs in the same result row's *Notes*, with the two commits.

What the figures include, and what they do not:

- Times are wall-clock times from the 1 µs timer. A task's work includes
  any time it was pre-empted by higher-priority tasks and interrupts, so
  it is not CPU time, and adding up work over period does not give the
  CPU load (A12 asks for that: measure it separately).
- Navigation's work starts after it has taken its queued events
  (`nav_drain_queues()`), so it leaves out event handling, where packed
  events are unpacked, and wake-ups for events alone. To include them,
  move `app_stats_release(APP_STATS_NAV)` to just after the wake-up in
  both builds.
- `free_min` is sampled every 200 ms, so a shorter peak can be missed.
- Stack figures cover the IS19 tasks only, not the kernel's USB and
  radio tasks; `data` + `bss` is not the whole RAM in use (NFR19 also
  counts those stacks and the kernel's own memory).
- The statistics are read across cores without a kernel object (TEAM.md
  section 7): each word has one writer, but the figures in one printed
  line can come from different moments.
- The timing probe (`CFG_PIN_TIMING_PROBE`, S3-U33) marks the Line
  task's 5 ms releases only.

## Videos of tests and demos

A manual test (an LED, a motor, a sensor reacting) is best shown with a short
video. Record 15–60 s showing the test ID and the hardware, share it as an
**unlisted YouTube** link (the only way videos are shared), list it in
**`videos/README.md`**, and put the link in the *Log / evidence* column of
the test's result row. `videos/README.md` explains how.

## Rules

- **Never edit or delete an old result.** If a test fails, record the FAIL,
  fix the code, re-run, and add a new PASS row. The history is the evidence.
- **Always record the commit.** A result is meaningless without the exact
  code it was run on. Commit your work *before* testing.
- **A FAIL needs a note** saying what went wrong and, later, which commit fixed it.
- Before merging a change into `main`, re-run your subsystem's unit tests.
  Before a demo or hand-in, run every test and record the full set.
