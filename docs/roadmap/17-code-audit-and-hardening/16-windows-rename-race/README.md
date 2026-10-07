# #52 — the Windows export I/O flake Boletus reported

The record of a robustness question raised from outside the audit: Boletus's Windows CI
loses one file-writing test in about half its runs and has asked DualC for a bounded
retry of the `.part` → `<path>` rename in `AtomicOutput::commit()`. This page holds the
desk analysis written *before* the harness ran, the evidence, and the verdict. A folder
from birth: the item page is this README, its two children hold the analysis and the
evidence. Headings are frozen at ID + title; status, date and trigger live on the body
lines. Opened 2026-10-07 on branch `exp/io-stress-windows`.

## #52 Windows export I/O flake — the `.part` rename under an on-access scanner
**DONE (2026-10-07) — the bug is real and DualC's; the cause is not the one asked about.**
Five experiment samples on the same `windows-2022` image as Boletus's gate (the evidence is on [02](02-evidence-and-verdict.md), the raw reports in [`docs/raw/2026-10-07-io-stress-windows-runs.md`](../../../raw/2026-10-07-io-stress-windows-runs.md))
found: no on-access scanner runs on that image at all; 1,890 wild exports of Boletus's own
scenario never failed; and a child process started with handle inheritance while a tiled
export has its `.part` open makes **every** such export fail at the rename with error 32 —
Boletus's CLI parity test starts `dualc_field.exe` exactly so, in parallel with the exports
that failed. What shipped: every output handle non-inheritable (the fix), a bounded Windows
rename retry (hardening, for a scanner's momentary hold on a user's machine — it would not
have saved one of the measured failures), the writer's error line in the ABI `err` buffer
(ABI 0.5.1), the regression tests, the harness as an opt-in gate tier. Decision
[D-54](../../../decisions/01-settled.md). The desk analysis as it was written before the runs is
[01](01-desk-analysis.md); the evidence, the verdict and what shipped, [02](02-evidence-and-verdict.md).

**PLANNED (2026-10-07).** The question: does `AtomicOutput::commit()`
(`examples/example_common.cpp`, one `std::filesystem::rename` with no retry) fail on
Windows while an on-access scanner still holds the file it just saw closed — and is that
what Boletus's CI is hitting? The verdict is owed with evidence, not inference; the fix
(a bounded retry, the OS error text into the ABI `err` buffer) is conditional on it.

## The pages of this item

| Page | Holds |
| --- | --- |
| [01 — Desk analysis](01-desk-analysis.md) | what Boletus observed, the rc 2 sites ranked, the alternative hypotheses, the instrument, the sample-size rule, the decision criteria — all fixed before the first run |
| [02 — Evidence and verdict](02-evidence-and-verdict.md) | the five samples, the mechanism, the verdict, what shipped, the paragraph for Boletus |

---

← Back to the [block README](../README.md) · the [Roadmap index](../../README.md).
