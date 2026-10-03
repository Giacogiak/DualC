# Phase 0 benchmark — the measured RAM wall

Part of [11 — Dense-lattice deliverable](README.md). The measured RAM wall (2026-06-12)
that every later step is sized against, and the mesh-boolean timing that motivated the
field-graph.

## Phase 0 benchmark
**2026-06-12.**


The 2026-06-02 table in [01](01-decision.md#the-decision) was *estimated*. A measured run (16 GB / 8-core Win10
box; gyroid λ=5 mm, `--offset 1 --normalize-thickness`; mm-scale boxes) confirmed
and refined it. Drives the re-sequencing in
[12-field-graph-and-app/](../12-field-graph-and-app/README.md).

**Lattice generation** (`dualc_lattice`, analytic gyroid ∩ box → binary STL):

| Box | Depth | Cell | Time | Peak RAM | Faces | Status |
|---|---|---|---|---|---|---|
| 80³ mm | 6 | 1.25 mm | 7 s | 0.6 GB | 0.59 M | ok (walls under-resolved) |
| 80³ mm | 7 | 0.63 mm | 37 s | 4.6 GB | 2.9 M | ok |
| 80³ mm | 8 | 0.31 mm | 402 s | 10.1 GB | — | no usable output (thrashed) |
| 200³ mm | 6 | 3.1 mm | 7 s | 0.6 GB | 0.62 M | ok (way under-resolved) |
| 200³ mm | 7 | 1.56 mm | 50 s | 5.0 GB | 4.5 M | ok (cell > wall) |
| 200³ mm | 8 | 0.78 mm | — | est. >16 GB | — | not attempted (would OOM) |
| 200³ mm | 9 | 0.39 mm | — | ~100 GB+ | — | unreachable |

The RAM wall is **conditioned on wall thickness** (~2–3 cells/wall): at **2 mm
walls** (the chosen target) the 80³ part is viable today (depth 7, measured) and
only 200³ needs a bigger box or streaming; at **1 mm walls** neither part meshes on
16 GB (80³ dies at depth 8). The wall is machine-independent (200³ full-res ≈
100 GB+; depth+1 ≈ 4× faces, so 8× RAM buys only ~1.5 levels).

**Boolean over the lattice** (difference, 2.9 M-face 80³-d7 lattice − a R30 mm
sphere, depth 7), both paths measured: **exact BVH = 48.9 min** (2.26 M faces);
**`--bake 256` = 10.5 min but lossy** (1.57 M faces, ~30 % detail dropped) — and 9.2
of those minutes were the bake itself, because a space-filling gyroid puts nearly
every grid cell in-band so the narrow-band SDF degenerates toward full-volume BVH
queries. **There is no cheap mesh-side path for lattice booleans** — which is the
case for the field-graph (`dualc_field`): the same composition as a *field* contours
once in ~40 s (≈ the measured 37 s lattice + a near-free `− tool` node). See
[12](../12-field-graph-and-app/README.md). **`dualc_field` now ships this** (JSON core,
2026-06-14): `difference(intersection(mesh, gyroid-shell), tool)` is one contour
pass — see [command_reference/11](../../command_reference/11-dualc_field/README.md).

**Export bug found:** `dualc_boolean` ignores the `-o` extension and always writes
OBJ (confirmed for both `.stl` and `.3mf`); `dualc_lattice` correctly writes binary
STL. The boolean tool needs wiring to the `example_common` extension dispatch — a
prerequisite for boolean→STL/3MF manufacturing output. *(Fixed 2026-06-12, `6ae7dc9` —
[09](../09-io-formats.md).)*

---

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
