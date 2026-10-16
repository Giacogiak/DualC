# #61 — dark specks on the viewer's grazing silhouettes

The record of a viewer-quality defect seen while checking the plan-run unit P03.2 of
[block 21](../21-cli-consolidation/README.md) (2D profiles and lifts in `dualc_field_view`).
No ledger finding behind it.

## #61 Dark specks on grazing silhouettes in dualc_field_view

**DEFERRED** — trigger: the specks seen on a second renderer (CI's llvmpipe, a current
desktop GPU, or a user's report), or a change to the sphere-tracer's hit / miss test.
Opened 2026-10-16 by Giacomo at the P03.2 human check: "Open a tracked item", no work now.

`dualc_field_view --snapshot` (800×600) leaves a handful of dark pixels, about 128 on the
revolve of P03.2's polygon profile, where a ray grazes the silhouette. They are not the new
nodes' doing: the plain `torus(major=1.2,minor=0.4)` primitive, which P03 did not touch,
shows the same specks on the inner rim, and a `difference` of two capped cylinders shows
none. Seen on one renderer only: this host's `NV92` (an old NVIDIA part); the parity harness
compares field values, not shaded pixels, so it cannot catch them
([D-53](../../decisions/README.md) defers a snapshot check of the viewers). The likely place is
the tracer's step cap or miss threshold at grazing incidence (`examples/field_glsl.cpp`, the
march loop) — a hypothesis, not yet looked at.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
