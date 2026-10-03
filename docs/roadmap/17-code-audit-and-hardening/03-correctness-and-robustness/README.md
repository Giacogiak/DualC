# Tracked items — Correctness & robustness (#22–#26)

The audit findings that can make DualC produce a wrong answer or abort, rather than a
slow or awkward one. Performance, API and build items are in
[`04-engineering-quality.md`](../04-engineering-quality.md).

Part of the [code audit ledger](../README.md); the full 87 findings are in
[`01-findings-ledger-engine.md`](../01-findings-ledger-engine.md),
[`02-findings-ledger-craft.md`](../02-findings-ledger-craft.md) and
[`06-findings-ledger-testing.md`](../06-findings-ledger-testing.md).

Headings are frozen at ID + title; status, date and trigger live on the body lines, so an
item changing state never breaks an inbound link. Entries carry the audit's claim and its
citation — **audit claims, not independently verified defects**, except where a line says so.

The page became this folder on 2026-09-20, when #46 — the first item born after the
audit, from the first semantic lint — did not fit under the cap; the children keep the
entries verbatim and every `#NN` anchor. The status ledger of these items is the
[audit README](../README.md#tracked-items--status-at-a-glance); this table only says where
each entry is.

| Item | Title | Entry |
| --- | --- | --- |
| #22 | `parallelFor` exception propagation | [01](01-parallelfor-and-ray-parity.md#22-parallelfor-exception-propagation) |
| #23 | Ray-parity advance epsilon is relative to hit distance, not feature size | [01](01-parallelfor-and-ray-parity.md#23-ray-parity-advance-epsilon-is-relative-to-hit-distance-not-feature-size) |
| #24 | Far-from-origin precision — re-centre before the float seam | [02](02-precision-and-celloverlaps.md#24-far-from-origin-precision--re-centre-before-the-float-seam) |
| #25 | `lipschitzBound()` — the silent correctness opt-out behind `cellOverlaps` | [02](02-precision-and-celloverlaps.md#25-lipschitzbound--the-silent-correctness-opt-out-behind-celloverlaps) |
| #46 | `OffsetField` does not forward `cellOverlaps` | [02](02-precision-and-celloverlaps.md#46-offsetfield-does-not-forward-celloverlaps) |
| #26 | A `Diagnostics` channel for the silent-failure surface | [03](03-diagnostics-item.md#26-a-diagnostics-channel-for-the-silent-failure-surface) |

---

← Back to the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
