# Post-contour QEM decimation (§ G)

Part of [12 — Field-graph & standalone raymarch app](README.md). Usage:
[command_reference/11 §
Decimation](../../command_reference/11-dualc_field/05-decimation.md).

## G. Post-contour QEM decimation (`--decimate` / `--simplify`)
**DONE (2026-07-05)** — Approach A (monolithic); per-tile Approach B DEFERRED.


Origin: the [gyroid-shell contouring-quality
inspection](08-quality-inspection-gyroid-shell/README.md).
The inspection found that on a dense TPMS shell the octree `--collapse` lever is
structurally dead (~6.5% — a gyroid is high-frequency everywhere, so no cell is
QEF-flat), while **post-hoc QEM decimation on the smooth-curved walls** (curvature
radius ~0.7 mm ≫ the ~0.1 mm tessellation) is the real "adaptively lighter files"
win: ~10× lighter at sub-print-resolution error, *lighter and faster than* going
to depth 9.

**Shipped — Approach A (monolithic):** `dualc_field --decimate RATIO` (keep a
triangle fraction) / `--simplify ERR` (cap an absolute error in mm) run one global
`meshopt_simplify` pass on the finished mesh, then export. The vertex pool is
compacted (`meshopt_optimizeVertexFetch`) so no dangling verts reach the writers;
the decimated `.obj` carries recomputed angle-weighted normals. Backed by
**meshoptimizer** (MIT), vendored under `examples/third_party/meshoptimizer/` as a
minimal verbatim subset (`meshoptimizer.h` + `simplifier.cpp` + `vfetchoptimizer.cpp`)
— host-side only, compiled into `dualc_examples_decimate`, **never linked into
`libdualc`** (library-scope + permissive-vendoring rules). The seam is
`example_common`'s `writeField` (the one place a monolithic contour becomes a
mesh); the CLI adds the two flags + validation and **rejects `--tile-depth`**.

**Verified:** r=25 depth-8 gyroid shell, `--decimate 0.1` → 5,189,100 → 518,856
faces, watertight (0 boundary edges), true-surface **p99 0.049 mm** (≤ the 0.05 mm
target); `--simplify` hits its error ceiling; all three formats + guards exercised;
**188/188 ctest** green. Detail + recipes in
[cmd-ref 11 §
Decimation](../../command_reference/11-dualc_field/05-decimation.md#decimation---decimate----simplify).

**Deferred — Approach B (per-tile streaming decimation):** decimate each tile with
its seam/border vertices **locked** (`meshopt_SimplifyLockBorder`) so adjacent tiles
still match → crack-free, at one-tile peak RAM. This is the only path to *depth-9
fidelity + light file + bounded RAM* together, but it trades some ratio (locked
seams stay full-res) and adds tile-loop + ghost-ring integration. Build it only when
a high-res process (SLA/DLP/CNC) needs depth-9 quality on parts too big to contour
monolithically. The `--decimate` + `--tile-depth` guard reserves the combination for
it.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
