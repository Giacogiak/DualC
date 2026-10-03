# Correctness & robustness — the `Diagnostics` item (#26)

Part of [03 — Correctness & robustness](README.md): the audit's highest-value item as tracked here; its record and evidence are
[05](../05-diagnostics-channel.md) and [11](../11-diagnostics-verification.md). Headings are frozen at ID +
title; status, date and trigger live on the body lines.

## #26 A `Diagnostics` channel for the silent-failure surface
**DONE (2026-09-09).** Full record, including the re-scoping (three of the six
degradations below no longer exist, and one never did) and what is deliberately
still not reported: [**05**](../05-diagnostics-channel.md). The item as originally
written is preserved verbatim below.

**PLANNED.** Six documented degradations return a plausible answer instead of reporting a
problem: an invalid `field.bounds()` silently becomes `BBox::unit()`; `windingNumberFast`
reports "everything outside" when no tree was built; grid probes outside the baked region
clip silently; the 4096-hit ray cap truncates; a non-manifold edge falls back to the face
normal; `seed` does nothing. Each is the correct answer in the intended case and an obscure
failure otherwise. An empty contour is the same shape of problem —
`contourHermiteOctree` synthesizes a placeholder triangle because geometry-central rejects
an empty polygon list, so "the field had no surface here" is indistinguishable from "there
was one triangle", and there is no error channel out of the function at all. The audit calls
a `Diagnostics{watertight, oriented, rayCapHits, boundsClipped}` out-parameter the single
highest-value change across the whole surface, and notes that
`buildPseudoNormalTopology` **already computes** the watertightness test and discards it.
*Source:* `docs/raw/study/B3-memory-ownership-performance.md:239-271`,
`docs/raw/study/A4-contouring-recursion.md:352-362`, `docs/raw/study/A2-sampling-octree-oracles.md:266`.
*Verified 2026-08-31:* no `Diagnostics` type in `include/` or `src/`; `src/sampler.cpp:171`
still falls back to `BBox::unit()`.

---

← Back to the [correctness index](README.md) · the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
