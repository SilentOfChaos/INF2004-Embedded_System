# 13 — Finalised content (verbatim)

Tags and source keys: see `00_README.md`. Text here is copied exactly from
its source; do not reword it when reusing it.

## 1. Four post-submission fixes in the W6 report

[Reported: PREV `10_final_content.md`, "Claude's handoff records these four
fixes as applied to the user's Week 6 DOCX"]. [Verified: each passage
occurs in the `pdftotext` extract of RPT, checked 16 Oct; in fix 4 the
extract interleaves table columns, so the words appear split across the
row "The 5 ms task periods need a kernel tick … finer than the template's
default." with the status "Partially fixed".]

### 1.1 §2 pin-table rows

```text
START button | User input | START | — | GP20 (onboard) | GPIO input | Navigation Task; onboard button, no wiring
RESET button | User input | RESET | — | GP21 (onboard) | GPIO input | Safety Task; onboard button, no wiring
Status LED (NeoPixel) | Indicator | STATUS_LED | — | GP18 (onboard) | PIO | Safety Task; onboard RGB LED, no wiring
Piezo buzzer | Actuator | BUZZER | — | GP22 (onboard) | PWM | Safety Task; onboard, no wiring; mute switch off
```

### 1.2 §6.3 corrected barcode wording

```text
DECODING takes 9 elements per character (5 bars, 4 spaces, 3 of them wide) and skips the narrow gap between characters, so one barcode ('*', the letter, '*') is 27 character elements plus 2 gaps. It calls an element wide if it is more than twice the narrowest and checks for '*' at both ends; if they do not match, it reverses the order and tries once more.
```

Note: the code's default classification differs (per character, bars and
spaces separately, approved improvement D-27); `CFG_BC_CLASSIFY_PER_CHAR`
0 selects the report's rule. [Verified: D-27]

### 1.3 §9 last answer

```text
Task periods, priorities and requirement tolerances are team-set targets; they will be checked by measurement, and any change will be justified at Tribunal 1. Assumptions A1–A14 are still open. The barcode mapping will change on demo day, and a requirements change is expected at Tribunal 1.
```

### 1.4 §8.3 status correction

```text
The 5 ms task periods need a kernel tick finer than the template's default.
```

Team response for that item: `Partially fixed`. "The run had no defined
end" stays `Fixed`. [Reported: PREV]

PREV also preserves one §8.1 prompt (Buddy 1) and says "The rest should be
read from the current DOCX rather than reconstructed from memory"; the
DOCX was not available here. [Reported: PREV]

## 2. The user's rules and answers (13 Oct)

Verbatim in 08 §1. The rules as recorded in the repository
(`docs/decisions.md`, top), verbatim:

```text
Rules for this code, given on 13 Oct 2026 to the Claude Code session that
wrote its first version (a team member's instructions; the whole team
has not confirmed them yet):
```

followed by five bullets (see 08 Appendix A). The `app_config.h` header
rule is quoted in 04 §5.

## 3. The user's 16 Oct choices (AskUserQuestion answers)

```text
Merge branch into main, no PRs
Fault after 4 failed reads (Recommended)
Unseen side = no room (Recommended)
Overrange = collision (Recommended)
```

[Verified: TR L5713]

## 4. Identifiers to preserve exactly

| Kind | Exact form |
|---|---|
| Repository | `purplemushmallow/INF2004_IS19` |
| Branches | `main`; `claude/upbeat-franklin-pkkhdt` |
| Final commit | `58f7d61302e686ff2e8c84df1a5016c7807ac99c` (tree `caeb76a31d2d56da058e6f0a8c44fc5b64b686e6`) |
| MQTT topics | `inf2004/IS19/telemetry`, `/event`, `/status`, `/cmd`, `/ack` |
| Fault causes | `FAULT_CAUSE_COLLISION` 1 … `FAULT_CAUSE_LINE_DATA` 7 (07 §2) |
| Decision IDs | D-01 … D-81; open questions G1, G2, G4, G5, G11–G18 (§E numbering); KB 06 G1–G10 |
| Test IDs | COM-U, NAV-U, S1-U … S5-U, INT-, SYS- (never renumbered: "Test IDs never change") |
| Tag syntax in `app_config.h` | `[REPORT …]`, `[DATASHEET …]`, `[TEMPLATE …]`, `[DECISION D-nn]`, `[UNVALIDATED: …]`, `[MEASURED <date> <who> <log file>]` |

## 5. Commit trailers used by Claude in this repository

Claude's commits end with a `Co-Authored-By:` line and a `Claude-Session:`
line pointing to this session; copy them from `git log` (for example
`git log -1 fb7a67b`) rather than from here. [Verified: git log]
