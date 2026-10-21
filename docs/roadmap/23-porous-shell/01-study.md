# The P01 study — engine, distributions, distances, GPU layout, defaults

The exploration phase the [porous shell plan](../../raw/2026-10-19-porous-shell-plan.md)
§ 3 P01 asks for, run 2026-10-21 in scratch (prototypes never committed). The driver outputs
are in [the measurement dump](../../raw/2026-10-21-porous-study-measurements.md). Host: this
Linux box, GCC 15, `-O2`, one thread. The section that binds P02–P07 is the last one,
[the defaults table](#the-defaults-table).

## (a) The Delaunay engine

| Candidate | Verdict |
| --- | --- |
| geogram Delaunay PSM 1.10.1 (BSD-3, `Delaunay_psm.{h,cpp}`, 1.47 MB) | **rejected** — global state, see below |
| Own Bowyer–Watson on Shewchuk's `predicates.c` (public domain, 171 KB) | **chosen** ([D-74](../../decisions/03-settled-continued.md)) |
| TetGen | AGPL-3: rejected by the owner's licence rule |
| CGAL | GPL / LGPL: rejected by the licence rule |
| Qhull | its own licence, a whole library for one call; not needed |
| fTetWild | a constrained mesher; the boundary is clipped by `intersection`, so a point Delaunay is enough |

**geogram's PSM** builds clean here: zero warnings with `-std=c++17 -Wall -Wextra` (its README
asks for C++20; C++17 works), one 46 s translation unit. On 100k random points its sequential
`BDEL` takes 580 ms for 672,288 tets (its threaded `PDEL` 220 ms on 8 threads), and the 20³ grid
takes 400 ms for 41,154 tets. It fails two of the plan's gates:

- **Global state.** `GEO::initialize()` must run first; it sets up the command-line table, the
  logger and the process layer. The insertion order (BRIO) and the walk draw from one
  process-wide `static std::mt19937_64 random_engine` (`Delaunay_psm.cpp:8916`), with no lock.
  Two fields built on two host threads race on it.
- **Determinism.** The tet *list* changes from one call to the next in the same process: three
  consecutive `BDEL` calls give three hashes, and only the first call of a fresh process repeats.
  The draws go through `std::uniform_int_distribution` and `std::shuffle`, whose outputs differ
  between libstdc++, libc++ and MSVC, so the order differs across the three CI OSes too.
  Sorted canonically, the tet *set* is stable (`BDEL` and `PDEL` agree), so sorting the output
  would mask the order, but the unsynchronised global would remain.

**The fallback**, prototyped in about 150 lines: Morton-sorted insertion, a visibility walk
from the last tet, the cavity of tets whose circumsphere *strictly* contains the point, a
finite super-tetrahedron at 1000× the extent. Two runs give one canonical hash, no tet has
`orient3d ≤ 0`, and 100,000 sampled empty-sphere checks find no violation, on 100k random points
(1.24 s, 2.1× `BDEL`) and on the 20³ grid (0.23 s). On the same 100k points it keeps 672,241
tets with no super vertex: geogram's 672,288 less 47 hull tets the finite super-tet hides. The
grid gives 41,154 tets, as geogram does, but a different valid triangulation of the
cospherical sets. Why it is robust with no
symbolic perturbation: if the new point were coplanar with a cavity boundary face and strictly
inside the cavity tet's sphere, it would be strictly inside the outer tet's sphere too (the two
spheres share that face's circle), so the outer tet would be in the cavity. A strict cavity
therefore never makes a flat tet. Cospherical sets (a regular grid) give one of the valid
triangulations, chosen by the insertion order alone, so the result is deterministic.

Hazards for P03: `predicates.c` uses K&R definitions, which C23 removed (GCC 15's default C), so
compile it as `-std=gnu89` (or C++ with prototypes added, marked `LOCAL MODIFICATION (DualC)`).
It also needs **no FMA contraction**: `-ffp-contract=off` on GCC/Clang (Apple Clang contracts by
default on arm64, the macOS CI) and `/fp:precise` on MSVC. A finite super-tet leaves the hull
tets slightly short (the 47 above), which the pad makes harmless. A test that counts tets
against Euler's formula must allow for that, or P03 takes the ghost-vertex variant
(tets on an infinite vertex, about 100 more lines) for an exact hull.

## (b) Distributions

A 10³ periodic box at spacing `s` = 0.5. Cell-volume CV is by Monte-Carlo, 200 samples per cell
(sampling noise 0.071, removed in quadrature in the last column).

| Distribution | Seeds / s³ | Mean NN / s | Min NN / s | Max empty / s | Cell CV (net) |
| --- | --- | --- | --- | --- | --- |
| `random` | 1.000 | 0.556 | 0.028 | 1.500 | 0.424 |
| `jitter` (1.0) | 1.000 | 0.632 | 0.067 | 1.168 | 0.248 |
| `jitter` (0.5) | 1.000 | 0.777 | 0.507 | 0.963 | 0.138 |
| `poisson` (Bridson, k = 30) | 0.573 | 1.051 | 1.000 | 1.162 | 0.136 |
| `poisson` + `relax=5` | 0.573 | 1.086 | 0.731 | 1.098 | 0.090 |
| `poisson` + `relax=20` | 0.573 | 1.109 | 0.800 | 1.104 | 0.077 |
| `random` + `relax=20` | 1.000 | 0.917 | 0.640 | 0.880 | 0.088 |

`random`'s 0.42 is the known Poisson–Voronoi value, a sanity check. Bridson's rate here was 23,500
seeds/s in a box. A mesh volume adds a `valueAt` per candidate, up to 30 per seed, which P02
measures. Lloyd by sampling costs one nearest-seed query per sample: 32 samples per seed,
4,583 seeds, about 0.3 s per iteration with the scratch grid.

**What looks natural.** White noise packs tiny cells beside huge ones (CV 0.42, NN down to
0.03 s), unlike foam or bone. Blue noise (`poisson`) has a hard minimum distance and an even
cell size (CV 0.14), the look of a sponge. A few Lloyd steps push it towards soap foam (CV
0.08, faces meeting near 120°). `poisson` stays the default; `relax` is the knob for the
foam look. For a **graded** density, Lloyd must weight each sample by the target density
`(spacing / spacing(x))³` *(P02: by `(spacing / spacing(x))⁵`, see the table)*. Plain Lloyd evens out the cell volumes and would erase the grading.

## (c) The distance formulas

Every case takes the unsigned distance to the complex from **the cell that contains p**, so it
is exact. Inside a convex cell, the distance to the cell's boundary is the minimum over its
facet planes, and any other primitive lies behind that boundary.

- **Voronoi walls.** The nearest seed `s1` comes from a greedy walk over the Delaunay graph
  (move to the closest neighbour while one is closer; on a Delaunay graph this always reaches
  the true nearest seed). The value is then `min_j ((p−s1)·(sj−s1) − |sj−s1|²/2) / |sj−s1|`
  over `s1`'s Delaunay neighbours, negated to be positive. This is exact everywhere, needs no
  bin bound, and costs about 15 bisectors plus 1–3 walk steps from the bin's start seed.
- **Delaunay walls (faces).** The tet containing p comes from a visibility walk, which ends on
  a Delaunay triangulation, started at the bin's start tet. The value is the minimum of the
  distances to the tet's four face planes. Outside the seeds' hull it is the distance to the
  plane of the face the walk left by.
- **Struts (Voronoi edges between adjacent circumcentres, or Delaunay tet edges).** The own-cell
  argument fails here: across a wall, a neighbour's edge can be nearer than any own edge. So
  segments go in a uniform bin grid of size `B`, each registered in every bin its box touches.
  With `d` the minimum over the 3×3×3 bins around p, the value is `min(d, B)`. That equals
  `min(true distance, B)`: below `B` every primitive within `B` was searched; at or above `B`
  the clamp wins. It is continuous and 1-Lipschitz.
- **Smoothing.** The quadratic `smin_k(a, b) = min(a, b) − h²k/4`, with
  `h = max(k − |a − b|, 0)/k`, folded over the primitives in their **packed order**. `smin` is
  not associative, so the CPU and the GPU must fold in the same order. The gradient
  `h∇a + (1−h)∇b` keeps each piece 1-Lipschitz. A fold of `m` values lowers the minimum by k/4
  for `m = 2`, 0.39k for `m = 3` (a generic cell corner), 0.48k for `m = 4`, and always
  by less than `k`. For the two own-cell cases the value is `max(0, smin)`. That clamp is what
  makes the field continuous across a wall, where both sides tend to 0, so it stays
  1-Lipschitz. For struts the fold is exact where `d + k ≤ B`.
- **The bin bound.** For struts, `B` (the bin size) is the larger spacing. The value is exact
  below it, so a strut radius plus `smooth` must stay below `B`; a radius near a spacing closes
  the pores anyway. A graded density puts `(spacing2/spacing)³` times more segments in a dense
  bin; P03 measures the items per bin. For the own-cell cases the bins only
  hold the start seed and the start tet (the ones nearest the bin centre, found exactly on the
  CPU), so `B` affects their speed, not their result.

## (d) The GPU layout

- **Textures.** 2D, read with `texelFetch` only (no filtering, so RGBA32F needs no float
  extension in WebGL2). Geometry is `RGBA32F` (a seed `xyz`; a tet face plane `n.xyz, d`; a
  segment end). Indices are `R32UI` / `RGBA32UI` through `usampler2D`, which is exact past
  2²⁴, where a float index is not. The width is **2048** (WebGL2's guaranteed
  `MAX_TEXTURE_SIZE`), with item `i` at `ivec2(i % 2048, i / 2048)`. The ES header adds
  `precision highp usampler2D;`.
- **Limits.** GL 3.3 guarantees 1024 and WebGL2 2048 for `MAX_TEXTURE_SIZE`. This host's GL
  driver (renderer `NV92`) reports 8192 for `MAX_TEXTURE_SIZE`, 2048 for
  `MAX_3D_TEXTURE_SIZE` and 16 units. The GTX 1050 Ti (Pascal, 32768) and HD 630 (16384) class
  are vendor figures not measured here. A 2048 × 8192 texture holds 16.7M items. The viewer
  queries `GL_MAX_TEXTURE_SIZE` and the unit count at run time.
- **Budget.** One `porous` node uses 4 textures for Voronoi walls (seeds, neighbour offsets,
  neighbour list, start bins), 3 for Delaunay walls (face planes, tet neighbours, start bins)
  and 3 for struts (segments, bin offsets, bin items). Two nodes and two meshes fit in 16
  units.
- **Fallback.** If a texture would pass the queried limit, or the unit count would, the node
  goes through `emitBakedGrid`: `valueAt` sampled to an R32F `sampler3D`, as `mesh` does. The
  viewer says so on stderr.

## The defaults table

The raw plan's node design stands except where a row says otherwise. Every row marked † is
**Chosen in-run (pending owner review)**.

| Topic | What later phases follow |
| --- | --- |
| Token | `porous`; no clash in `field_graph.cpp` / `field_glsl.cpp` |
| Engine † | Own Bowyer–Watson, sequential, on Shewchuk's `predicates.c` vendored in `src/internal/` (`-std=gnu89 -ffp-contract=off`; MSVC `/fp:precise`); finite super-tet at 1000× the extent *(P03: an infinite vertex instead, for an exact hull: [03](03-delaunay-engine.md))*; strict-conflict cavity; insertion in 63-bit Morton order of the seed array, ties by index; tets reported canonically (vertices sorted, list sorted) |
| RNG | PCG32 XSH-RR 64/32 (O'Neill's `pcg32_srandom_r(seed, 0)`); a double in `[0,1)` from two draws, `((a >> 5)·2²⁶ + (b >> 6))·2⁻⁵³`; no `std::*_distribution` |
| `spacing` † | `poisson`: Bridson minimum distance (k = 30, measured 0.573 seeds/s³); `jitter`: grid pitch; `random`: density 1/spacing³ (the P02 plan file's definitions; switching `dist` changes the cell count by the measured factor) *(P02: graded `jitter` / `random` thin a layout at the smaller spacing by `(min spacing/spacing(x))³`)* |
| Ramp | `u = clamp((control − d0)/(d1 − d0), 0, 1)`; `linear` u; `smooth` u²(3 − 2u); `gaussian` (1 − e^(−u²/2σ²))/(1 − e^(−1/2σ²)), σ = 1/√(2 ln 100) ≈ 0.3295 (99 % at u = 1, normalised to 1); `spacing(x) = spacing + (spacing2 − spacing)·w` |
| Variable-radius Bridson † | Accept a candidate at `c` when every seed `s` is at least `(r(c) + r(s))/2` away; background grid at `rmin/√3`; search radius `rmax` *(P02 built it on a linked-list grid at `max(rmin, rmax/2)` with a restart scan: [02 #63](02-record.md#63-deterministic-seeds-in-any-volume))* |
| Pad † | Seeds accepted where `V(x) < pad`, `pad = max(spacing, spacing2)`; those with `V(x) > 0` are flagged as pad |
| `relax` † | Lloyd by sampling, 32 samples per seed per iteration, drawn uniformly in the box of V ∪ pad and rejected outside; samples weighted by `(spacing/spacing(x))³`; a seed whose centroid leaves V ∪ pad stays put *(corrected by P02: the weight is `(spacing/spacing(x))⁵`, the CVT cell-size law in 3D, measured to hold the grading better: [02 #63](02-record.md#63-deterministic-seeds-in-any-volume))* |
| Ceilings | 2,000,000 seeds, `relax` ≤ 50, as the raw plan (P02/P03 may lower them on measurement) |
| Voronoi walls | Greedy nearest seed from the bin's start seed, then min over the neighbours' bisectors (section c) |
| Delaunay walls † | Visibility walk to the containing tet from the bin's start tet, then min over its 4 face planes; planes stored per tet *(P03: 0 outside the seeds' hull: [04](04-complex-and-bins.md))* |
| Struts | Segments in bins of size `B`, 3×3×3 search, `min(d, B)` (section c); Voronoi edges with a circumcentre beyond the padded box are clamped to it and counted *(P03: clipped to the volume's box grown by `B`, and the value clamped at 0 too)* |
| Bin grid | Struts: cell `B` = the larger spacing *(P03: or the largest cell radius, if larger)*, a segment registered in every bin its box touches; own-cell cases: cell = the smaller spacing, holding one start seed / start tet |
| `smooth` † | Quadratic `smin_k` folded in packed order *(P03: in ascending distance order, ties by packed index: [04](04-complex-and-bins.md))*; own-cell cases clamped at 0; the bound is k/4 for two primitives and < k in general (**this corrects the raw plan's "at most k/4"**; P04's test checks k/4 where only two primitives are within k, and < k elsewhere) |
| `cellOverlaps` | The default (Lipschitz-1) — no override |
| Determinism | Same graph → bit-identical seeds, tets, packed arrays on any OS; parity relies on the CPU and GPU reading the same packed arrays |
| GPU | Section d: 2D textures 2048 wide, `texelFetch`, RGBA32F + R32UI/RGBA32UI, `nodeStepScale` 1.0, `nodeFeatureScale` = the smaller spacing, `smooth` a uniform; fallback `emitBakedGrid` past the queried limits |
| Test names | Every new Catch2 case is named `porous: …`, so a `ctest -R porous` filter finds it; the existing graph and codegen cases answer to `field-graph` and `GLSL codegen`, not to `field_graph` / `field_glsl` (measured: that pattern matches no test) |
| Cancel | Poll the `CancelToken` once per seed batch of 4096, per Lloyd iteration and per 4096 insertions |

---

← Back to [block 23](README.md).
