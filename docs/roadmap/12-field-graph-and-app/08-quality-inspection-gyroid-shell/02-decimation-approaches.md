# Decimation approaches

Part of the [gyroid-shell inspection report](README.md) (roadmap 12/08): the engine
enhancement the report proposed — shipped as Approach A on 2026-07-05, record in
[§ G](../06-decimation.md#g-post-contour-qem-decimation---decimate----simplify) — and the
monolithic-vs-per-tile analysis behind it.

## Proposed engine enhancement — `--decimate` / `--simplify`

> **Implemented 2026-07-05 (Approach A — monolithic).** `dualc_field --decimate
> RATIO` / `--simplify ERR` run a global `meshopt_simplify` pass on the contoured
> mesh before export; `meshoptimizer` (MIT) is vendored under
> `examples/third_party/meshoptimizer/` (host-side only, never linked into
> `libdualc`). Rejected in combination with `--tile-depth` pending Approach B. The
> scope notes below describe exactly what shipped.

Turn the demonstrated 10× win into a one-flag workflow.

- **CLI:** add `dualc_field --decimate RATIO` (keep fraction, e.g. `0.1`) and/or
  `--simplify ERR` (target absolute error in world units) that runs a QEM pass on the
  contoured mesh *before* export.
- **Backend:** vendor **`meshoptimizer`** (`meshopt_simplify` / `meshopt_simplifySloppy`,
  **MIT**) into `examples/third_party/` — host-side only, **never linked into
  `libdualc`**, exactly per the library-scope and permissive-only-vendoring rules in
  `CLAUDE.md` / `THIRD_PARTY.md` (decimation is a mesh operation, not core DC).
- **Scope notes:**
  - **Monolithic path first** (works to depth 8, which fits RAM): contour → decimate
    → `writeField`. Simple, high value.
  - **Streaming path (bounded RAM at depth 9):** per-tile decimation with
    seam-plane vertices **locked** (`meshopt_simplify` supports locked border
    vertices) so seams stay crack-free — a follow-on, more engineering.
  - Guard: reject `--decimate` with `--tile-depth` until the per-tile locked-border
    path lands (or apply only within each welded tile).
- **Acceptance:** on the r=25 depth-8 part, `--decimate 0.1` yields ≤ ~0.5 M faces
  with true-surface p99 error ≤ ~0.05 mm (verify by sampling the field at output
  vertices, as in Step 1's demo), and the mesh stays watertight (F/V ≈ 2.0).

**Explicitly not recommended:** the feature-curve-aware QEF placement. Step 4/4b show
it removes only ~0.02–0.09 mm of rim excess the user won't see at depth 9, is a
research-grade contourer change, and does nothing for the cost problem — decimation
is where the effort pays off.

---

## Decimation: monolithic vs. per-tile streaming — the two implementation paths

Both are the *same* QEM decimation, but they sit on **opposite sides of the RAM
wall**, and that one difference drives everything else.

### The core axis: where does decimation run relative to peak memory?

Contouring builds cells → faces. The RAM wall is that a dense part's **full face
set** may not fit in memory. Decimation shrinks the face set — but *when* you shrink
it (before vs. after the whole mesh exists) decides whether it also helps peak RAM
or only the file on disk.

### Approach A — monolithic `--decimate` (contour whole → decimate whole → write)

**Flow:** build the entire octree and mesh in RAM → one global `meshopt_simplify`
call → write the result.

**Advantages**
- **Simplest possible** — a handful of lines: after `contourHermiteOctree`, hand the
  full `(V, F)` to `meshopt_simplify`, then `writeField`. Low risk, small test
  surface.
- **Best decimation quality/ratio** — global QEM sees the *whole* surface, so it
  collapses optimally everywhere with no artificial constraints. This is exactly the
  10×-at-0.035 mm result measured above.
- **Naturally a single welded mesh** — the input is already one connected mesh, so no
  seam bookkeeping; output is clean for FEA/re-boolean too.
- **Covers the recommended workflow** — depth 8 fits in RAM (~3 GB, contoured in
  3m28s), so "depth 8 + decimate 10×" runs entirely in Approach A. That *is* the
  sweet-spot workflow.

**Limitation**
- **Does not break the RAM wall.** You must hold the *full* undecimated mesh in RAM
  first. So it caps at the depth where the base contour fits — ~depth 8 for this
  part. **Depth 9 monolithic OOMs during contouring, before decimation ever runs.**
  A lightens the *file*, not the *peak RAM to generate it*.

**Use it for:** any part that fits at contour time (≤ depth 8 here). This is the
primary recommendation.

### Approach B — per-tile decimation on the streaming path (bounded RAM)

**Flow:** contour one tile → decimate *that tile* with its **seam/border vertices
locked** (`meshopt_SimplifyLockBorder`) → stream to disk → free → next tile. Peak
RAM = one tile.

**Advantages**
- **Breaks the RAM wall *and* lightens the file at once.** You can export a depth-9
  (or depth-10) part that would never fit monolithically — at one-tile peak RAM. This
  is the only way to get **depth-9 quality + light file + bounded RAM** together.
- **Starts from a cleaner base.** It decimates depth-9 (pristine, 4 cells/void)
  rather than depth-8 (2 cells/void, ~0.12 mm rougher), so the decimated result is
  higher-fidelity than anything A can produce.
- **Composes with the existing tiled/welded plumbing** (`forEachOwnedTile` + the
  sinks) — the locked-border option keeps seam vertices exact, so adjacent tiles
  still match → crack-free, and it can still weld to one object.

**Trade-offs**
- **More complex** — integrates into the tile loop, needs locked-border handling, and
  must interact correctly with the ghost-ring + centroid-ownership rule. Real
  feature, real test surface.
- **Lower decimation ratio than global.** Locked seam vertices stay at **full
  resolution** (that's what keeps seams crack-free), so the ~2-cell-thick seam ring
  per tile is never decimated. With coarse tiles (`--tile-depth 7`, 126-cell
  interiors) that's a small fraction and decimation is still strong; with **fine
  tiles it degrades** — the seam skeleton dominates → less reduction and visible
  density variation near seams. So B trades some ratio for the RAM bound, and the
  finer you tile the worse that trade.
- **Slightly suboptimal quality** — QEM can't collapse across seams, so the result
  isn't quite the global optimum A gives.

**Use it for:** depth 9+ dense parts destined for a high-resolution process
(SLA/DLP/CNC) where you need both the pristine quality *and* bounded RAM.

### Side by side

| | **A — monolithic `--decimate`** | **B — per-tile streaming decimate** |
|---|---|---|
| Peak RAM | full mesh (same wall as today) | **one tile** (breaks the wall) |
| Max feasible depth | ~8 (what fits) | **9, 10, … unbounded** |
| Base quality decimated | depth 8 (2 cells/void) | **depth 9 (4 cells/void)** |
| Decimation ratio | **global-optimal** (best) | reduced by locked seams (worse with fine tiles) |
| File size win | full 10× | ~8–10× (seam skeleton survives) |
| Engineering | **few lines, low risk** | real feature, seam-locking + tile-loop integration |
| Single welded object | yes, naturally | yes (with `--weld`) |
| Covers the recommended workflow | **yes** (depth 8 + decimate) | overkill for it |

### Recommendation on sequencing

**Build A first.** It *is* the recommended workflow (depth 8 + decimate ~10×), it's
cheap and low-risk, and it delivers the demonstrated lighter-and-faster-than-depth-9
result for the actual printable case. **Add B later, only if** genuine depth-9
fidelity (a high-res process) is needed on parts too big to contour monolithically —
at which point B's extra complexity buys the one thing A can't: the RAM bound.

In short: **A optimizes the file; B optimizes the file *and* the memory to make it** —
A is the 90% workflow win for a few lines; B is the specialist tool for dense
high-res parts, at real engineering cost.

---

← Back to the [report README](README.md) · the [topic README](../README.md).
