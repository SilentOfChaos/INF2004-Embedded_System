# 12 — Next steps (only those already named in the sources)

Tags and source keys: see `00_README.md`. **No new recommendation is added
here**: every step below is quoted or condensed from the named source.
Order within a group is the source's order where it gives one.

## 1. Decisions and confirmations

| Step | Source |
|---|---|
| Owners of S3, S4 and S5 confirm D-79, D-81 and D-80: "the owners of S3, S4 and S5 still have to confirm them" | [Verified: decisions §H] |
| Decide G11–G14 and G18 ("G18 is the S2 owner's choice; G11–G14 are still open") | [Verified: decisions §H, §E] |
| Answer G1, G2, G4, G5: the user wrote "i will answer ltr" | [Verified: TR; decisions §E] |
| Each member fills in their GitHub username in TEAM.md | [Verified: decisions §E; TEAM.md §1; TPKG §8.3 step 4] |
| Get instructor agreement for A11 and A14 ("pending instructor agreement") | [Verified: TEAM.md §7] |
| Ask the instructors the KB 06 questions (barcode/hump side, junction types, START after placing, demo SSID and broker, bar height and barcode-to-junction distance, hump heights, obstacle sizes) "ideally before Mon 19 Oct" | [Verified: KB 06 heading] |
| Send the official Tribunal 1 brief, template and rubric ("Please send them") | [Reported: TPKG W8-1] |

## 2. Measurements and tests (all DEFERRED)

[Reported: TPKG §8.1, "definitions are in `testing/*/TEST_LOG.md`"]

| Owner | Tests |
|---|---|
| Ang Hung Ling (S1) | S1-U30 to S1-U35; INT-13, INT-14, INT-15, INT-19 |
| Dominic Ng Jun Jie (S2) | S2-U30 to S2-U34 and S2-U37. S2-U35 and S2-U36 wait for G18. |
| Nga Wei Yang (S3) | S3-U30 to S3-U36; INT-20; SYS-02 |
| Yee Sheng Chua (S4) | S4-U30 to S4-U38 |
| Wong Qian Hui (S5) | S5-U30 to S5-U34 |
| Team | the on-target `TEST=1` image (every `-U20` to `-U22` check); INT-01 to INT-12 and INT-16 to INT-18; SYS-01 and SYS-03 to SYS-20; NAV-U30; A12; NFR19 measured at run time |

Changes since TPKG §8.1 was written [Verified: TEST_LOGs at `58f7d61`]:
S5-U35 (D-80) is a new S5 manual test; S4-U37 now expects D-81's
behaviour (TPKG's "will fail until G17 is decided" no longer applies);
S3-U18, S4-U06, S5-U06 are host tests and already pass.

Rules for recording [Verified: `testing/README.md`; each TEST_LOG]: "Add a
new row for every run. Never edit or delete old rows." Measured values
replace `[UNVALIDATED …]` with "[MEASURED <date> <who> <log file>]"
[Verified: `app_config.h` header]. Integration tests are signed off by
both owners of the interface [Reported: TPKG §8.1; Verified:
`testing/integration/TEST_LOG.md`].

S1-U20 on target: "If it fails on the Pico, check that the free buffer
space rises within the window instead." [Verified: decisions §H]

## 3. Review and ownership

| Step | Source |
|---|---|
| "Each owner should read their pull request and the D-entries it names: these are the "why" behind each change." | [Reported: TPKG §8.4] |
| AI use is allowed "but own every line" | [Verified: KB 02 (instructor, L1 L337–343)] |
| `testing/TRACEABILITY.md` (evidence by owner) "is the starting point for the impact analysis of the requirement change the lecturer will announce at Tribunal 1" | [Reported: TPKG §8.4] |
| Weak spots to close: NFR4, NFR5, NFR7, NFR12, NFR19 and FR12's field list are verified only on the car; accuracy requirements depend on UNVALIDATED values; NFR15 (Barr-C) "is verified by review" | [Verified: TRACEABILITY.md "Weak spots"] |
| Change Fig 10 at the next report revision if D-57 is kept (TPKG's option for G11) | [Reported: TPKG §5.6] — an option, not a decision |

## 4. Not to be repeated

- TPKG §8.3 steps 1–3 (`open_prs.sh`, merge order, PR texts) are done:
  the 17 commits were merged directly into `main` on 16 Oct on the user's
  choice "Merge branch into main, no PRs". [Verified: git; TR L5713]
- TPKG §8.3 step 5 ("Refresh `handover/`") is this pack.
