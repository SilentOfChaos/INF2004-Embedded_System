# 10 — Verification and test evidence (coverage G)

Tags and source keys: see `00_README.md`. This file reports **exactly**
what was run, on which tree, with which result. All logs are in
`handoff_artifacts.zip` → `evidence/`. "Tested" here means host unit
tests on a Linux PC unless stated.

## 1. What was never run

[Verified: every `testing/*/TEST_LOG.md` Results table holds only its
template placeholder row (`YYYY-MM-DD | … | PASS / FAIL`); no `MEASURED`
tag in `app_config.h`]

- Nothing on a Pico W, on the Robo Pico or on the car: no `TEST=1`
  on-target run, no manual, integration or system test.
- No timing (5 ms period ± 1 ms, A12), CPU-load, run-time RAM (NFR19),
  braking-time (NFR6), MQTT-broker or Wi-Fi test.
- No static analyser or Barr-C checker; no code coverage measurement.
- No physical measurement of any `UNVALIDATED` value.

## 2. Firmware builds

Script: `scripts/verify_tree.sh <ref> <outdir>` — clean `git archive`
export, `wifi_secrets.example.h` copied in as `wifi_secrets.h`, seven
`make` profiles, then the extra-warnings recompile `scripts/warncheck.sh`.
Toolchain: `arm-none-eabi-gcc` 13.2.1, Pico SDK 2.2.0. "warnings" counts
lines containing "warning" in the build log (template warning level, no
`-Wall`); warncheck recompiles every `app_program` C file with `-Wall
-Wextra -Wshadow -Wno-unused-parameter`.

| Tree | Date | Profiles rc / warnings | Default text/data/bss | MQTT text/data/bss | warncheck | Evidence |
|---|---|---|---|---|---|---|
| `b7bf737` (= `main` `58f7d61` code; later commit changed only `docs/decisions.md`) | 16 Oct | 7/7 rc 0, 0 warnings | 316096 / 88 / 28032 | 371100 / 124 / 86716 | rc 0 (both command sets) | `evidence/final_main/` (**master**) |
| `698e39d` (teammate merge + Claude's 2 wording fixes) | 16 Oct | 7/7 rc 0, 0 | 315492 / 88 / 28028 | 370496 / 124 / 86712 | rc 0 | `evidence/reviewed_merge/` |
| TPKG v2 tree (`9c3b4113`) | 16 Oct | 7/7 rc 0, 0 | 315492 / 88 / 28028 | 370496 / 124 / 86712 | rc 0 | `evidence/teammate_v2/` (same sizes as TPKG's own report [Reported: TPKG §6.3]) |
| each teammate branch alone (shared, S1…S5) | 16 Oct | default + MQTT rc 0, 0 warnings each | — | — | — | `evidence/per_branch/` |
| `ee9601b` (first code) | 14 Oct | 6 profiles rc 0, 0 | 314828 / 88 / 28008 | 369832 / 124 / 86692 | rc 0 | `evidence/ee9601b/` |
| template only (`e870827`) | 13 Oct 09:33Z | 3 profiles | text 285184, bss 24476 | 339156 / – / 82248 | — | `evidence/template_baseline/` |

Other profiles at `b7bf737`: `TEST=1` 319164; MQTT+TEST 374168; `SMP=0`
308776 / 76 / 23900; `CONSOLE=uart WIFI=none` 84314 / 12 / 19620;
`CRASH_REPORT=1` 316244. [Verified: `final_main/build_summary.txt`]

RAM: `.data + .bss` from these numbers plus configured stacks gives the
estimate in TEAM.md §6 (≈ 25% default, ≈ 47% MQTT). Estimate only.
[Verified: TEAM.md]

## 3. Host unit tests

`testing/host/` builds the pure-logic modules with
`-std=c99 -Wall -Wextra -Wshadow -Wpedantic -Werror -g -O1`, plus
`-fsanitize=address,undefined -fno-sanitize-recover=all` with `SAN=1`.
Each run was preceded by `make clean`.

| Tree | gcc 13.3 | gcc + ASan/UBSan | clang 18.1.3 | clang + ASan/UBSan | Evidence |
|---|---|---|---|---|---|
| `b7bf737` (`main` code) | 71/71 | 71/71 | 71/71 | 71/71 | `final_main/host_*.log`, `host_test_list.txt` |
| `698e39d` | 68/68 | 68/68 | 68/68 | 68/68 | `reviewed_merge/` |
| TPKG v2 | 68/68 | 68/68 | 68/68 | 68/68 (first attempt rc 2: sanitizer runtime missing; rerun after installing `libclang-rt-18-dev`; the summary file keeps the first line, the log is the rerun) | `teammate_v2/` |
| teammate branches alone | shared 64, S1 65, S2 64, S3 66, S4 64, S5 66 (gcc) | | | | `per_branch/per_branch.txt` |
| `ee9601b` | 64/64 | 64/64 | 64/64 | — | `ee9601b/host_*.log` |

### Host tests per area at `main` (71)

[Verified: `host_test_list.txt`; TEST_LOG method column]

| Area | Host | On-target "auto" (`TEST=1`, not run) | Manual (not run) | Total defined |
|---|---|---|---|---|
| Common (COM) | 6 | 0 | 0 | 6 |
| Navigation (NAV) | 19 | 2 | 1 | 22 |
| S1 | 11 | 3 | 6 | 20 |
| S2 | 5 | 2 | 8 | 15 |
| S3 | 18 | 3 | 7 | 28 |
| S4 | 6 | 3 | 9 | 18 |
| S5 | 6 | 2 | 6 | 14 |
| Integration (INT) | 0 | 0 | 20 | 20 |
| System (SYS) | 0 | 0 | 20 | 20 |
| **Total** | **71** | **15** | **77** | **163** |

Host tests added after `ee9601b`: S1-U11, S3-U16, S3-U17, S5-U05 (teammate);
S3-U18 (D-79), S4-U06 (D-81), S5-U06 (D-80) (Claude, 16 Oct).

What a passing host test shows: the pure function behaves as the test
expects for the inputs the test gives. It does not show timing, kernel
interaction, ISR behaviour, sensor polarity or anything physical.
[Inferred; also TPKG OA-4 "host tests prove only what they test"]

## 4. Probes (claims reproduced outside the test suite)

[Verified: `evidence/probes/` outputs, rerun 16 Oct]

| Claim | `ee9601b` | `698e39d` | `main` `58f7d61` |
|---|---|---|---|
| I-01 (a) loss marked after 30 stored edges | LETTER 'A' | LETTER 'A' | LETTER 'A' (correct: loss after the symbol) |
| I-01 (b) overflow mark at 29 stored edges, 30th drained | LETTER 'A' | FAILED, reason 8 | FAILED, reason 8 |
| I-01 (c) same-level edge at 29, 30th in same batch | LETTER 'A' | FAILED | FAILED |
| Claude's own I-01 pattern (loss inside, completed same batch) | FAILED | FAILED | not run |
| I-02 valid record frozen at 0 ms, tick at 10 s, no health bits | DRIVE 250 mm/s | DRIVE 250 | **DRIVE 250** (D-79 covers failed reads, not a record that stops updating; see 11) |
| I-03 invalid at 60° and 120°, 300 mm at 90° | L 4000, R 4000, side LEFT | same | **L −1, R −1, side NONE** (D-80) |
| I-03 coarse scan, centre invalid | "no hit" (pure-logic probe) | same | same in the pure function; the task-level `scan_coarse_done` now requires all readings valid (D-80; S5-U06), which this probe does not exercise |
| I-05 line lost / junction / fault + new run / READY | stale letter held in all 4 | none held | none held |
| I-05 hump during a read (must still count) | held | held | held |
| Claude's I-05 probe (junction mid-read) | stale 'C' held = 1 | 0 | not run |

## 5. Other checks

| Check | Result | Tag |
|---|---|---|
| Template import | 643/643 files blob-identical to the uploaded zip | [Verified: `evidence/template_import_check.txt`] |
| Teammate patches apply | 17 patches on `ee9601b` as six branches, merged tree = TPKG zip (tree `9c3b4113`) | [Verified: decisions §H; git] |
| Worst-case JSON lengths | telemetry 259 B etc. (`ee9601b`; profile event not included) | [Verified: `evidence/ee9601b/worst_case_json.txt`] |
| Message-buffer sizes | `evidence/ee9601b/mbf_sizes_now.txt` | [Verified] |
| Secrets | `config/wifi_secrets.h` never committed (`git log --all -- config/wifi_secrets.h` empty) | [Verified: git, 16 Oct] |
| `open_prs.sh` script tests (52/52) and ShellCheck | run in the teammate's session only | [Reported: TPKG §9] |
