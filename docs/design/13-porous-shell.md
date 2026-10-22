# DualC — the porous shell

The `porous` source (`include/dualc/porous.h`): seeds placed in a volume, their Delaunay
tetrahedralization, the cell complex packed with its bins, and the unsigned distance to it as
an `ImplicitField`. This page holds how the library builds and queries it. The field graph's
own rules for the node — the bake margin, no cancel through the graph, the preview's data
textures — are [12 § The porous source](12-field-graph.md#the-porous-source); the keys and
recipes are [11/16](../command_reference/11-dualc_field/16-porous-shell.md); why each choice
is the record of [block 23](../roadmap/23-porous-shell/README.md).

## The build: seeds, tets, packed complex

`PorousField`'s constructor runs three stages, each deterministic, each throwing
`std::invalid_argument` with its own prefix (`porous seeds:`, `porous delaunay:`,
`porous complex:`, and `porous:` for the field's checks), each polling the `CancelToken` it
is handed (`Cancelled`):

1. **Seeds** — `generatePorousSeeds` (`src/implicit/porous_seeds.cpp`). Seeds go where
   `volume < pad`, `pad = max(spacing, spacing2)`; `isPad[i]` marks a seed where the volume
   is > 0, which keeps the boundary cells well formed. `poisson` is Bridson's sampling at
   minimum distance `spacing` (30 tries per active seed; when the active list empties, a scan
   over cells of the larger radius, 4 tries each, restarts it, so every connected part of the
   volume gets seeds); `jitter` a grid of pitch `spacing`, each point moved by
   `jitter · spacing · U(−½, ½)³` (0 = the regular grid); `random` white noise of density
   1 / spacing³. A control grades the spacing: `spacing` where it is ≤ `d0`, `spacing2`
   where ≥ `d1`, the ramp's weight between (`densityRampWeight`: linear, smoothstep, or a
   gaussian normalised to reach 1 at `d1`). `poisson` grades its radius; `jitter` and
   `random` lay their points at the smaller spacing and keep one with probability
   (min spacing / spacing(x))³. `relax` runs Lloyd iterations: 32 fresh samples per seed per
   iteration, weighted (spacing / spacing(x))⁵ — a centroidal tessellation's cell size goes
   as ρ^(−1/5) in 3D — samples outside `volume < pad` dropped. The volume and the control
   are evaluated here and nowhere else.
2. **Tets** — `internal::tetrahedralize` (`src/implicit/porous_delaunay.cpp`): Bowyer–Watson,
   sequential, on Shewchuk's exact `orient3d` / `insphere`
   ([06 § 8](06-parameters-and-vendoring.md#8-vendored-third-party-code)). Points go in in
   63-bit Morton order of their box (ties by index), each found by a visibility walk from the
   last tet made, its cavity the tets whose circumsphere *strictly* contains it, so no flat
   tet is made. An infinite vertex closes the hull exactly. The output is canonical: each
   tet's four vertex indices ascending, the list sorted, `neighbours[4t+k]` the tet across
   the face opposite vertex `k` (−1 on the hull); a point equal to an earlier one is listed
   in `duplicates` and named by no tet. A cospherical set (a regular grid) gets the one
   tetrahedralization its insertion order picks.
3. **Packed complex** — `internal::buildPorousComplex` (`src/implicit/porous_complex.cpp`)
   rounds the seeds to `float` and tetrahedralizes exactly those points, so the packed seeds
   are the triangulated ones; derives the dual (each Delaunay face and edge once, each seed's
   neighbours, each tet's circumcentre clamped to the padded box); and packs the one
   (`cells`, `mode`) case the field reads. `PorousField::packed()` exposes the arrays.

## The packed layout

All indices `int32` (−1 = none), all geometry `float`. Bin `(i, j, k)` covers
`binOrigin + [i, i+1) × [j, j+1) × [k, k+1)` times `binSize`, flat index
`(k·dims[1] + j)·dims[0] + i`; a point outside the grid falls in the nearest boundary bin.

| Case | Arrays | Bins |
| --- | --- | --- |
| Voronoi walls | `seeds` (x, y, z, 1 if pad), `neighbourOffsets` (n + 1), `neighbours` (CSR, ascending) | the smaller spacing over the padded box; `binStart` = the seed nearest the bin's centre among its own, an empty bin the nearest of its 26 neighbours' starts |
| Delaunay walls | `tetPlanes` (4 × (n, d) per tet, face `k` opposite vertex `k`, n outward; a shared face is exactly (n, d) and (−n, −d)), `tetNeighbours` | the same grid; `binStart` = the tet holding the centre |
| Struts (both cells) | `segments` (a.xyz, 0, b.xyz, 0): the Voronoi edges or the tet edges, `binOffsets`, `binItems` (every bin a segment's box touches, in segment order) | `B` over the volume's box grown by `B`; `bound` = `B` |

**`B`**, the struts' bin size and clamp, is the larger of the larger spacing and the largest
cell radius (the largest circumradius over the tets with a non-pad vertex and a circumcentre
in the volume's box). A grid past `kPorousMaxBins` = 2048 × 8192 bins (a sparse volume in a
large box) gets bins 1.25× coarser until it fits, walls and struts alike; the struts' `B` and
their visible box grow with them. **The drop rule:** struts are clipped to the volume's box
grown by `B`, and an edge or a hull face's ray that misses it is dropped — a volume point is
at least `B` from anything outside that box, so `min(distance, B)` cannot see the cut. A
segment that is a point, or a second copy of the same float segment, is dropped too, so no
strut counts twice. Walls drop nothing: the walks need the whole graph.

## The query

`internal::porousDistance(complex, p, k)` with `k = smooth`:

- **Voronoi walls** — the nearest seed (a greedy walk on the Delaunay graph from the bin's
  start seed), then the distances to the bisector planes of its neighbours.
- **Delaunay walls** — the tet holding `p` (a visibility walk from the bin's start tet), then
  the distances to its four face planes; outside the seeds' hull, 0 (solid: continuous with
  the hull faces, and the pad keeps the hull outside the volume).
- **Struts** — the segments in the 3 × 3 × 3 bins around `p`'s bin, all those within `B`.
- **The fold.** The quadratic smooth minimum `min(a, b) − h²k/4`, `h = max(k − |a−b|, 0)/k`,
  folded over the candidates within `k` of the minimum in ascending (distance, index) order,
  so the value depends only on those, symmetrically and continuously. At `k = 0` it is the
  minimum. The result is clamped at 0 — walls and struts alike, as a smoothed junction
  dips below 0 and `onion`'s `|·|` would thin the material there.
- **The walks** stop at `kPorousMaxWalk` = 1024 steps and return 0.

## The field

- **Value.** `porousDistance`, the struts then capped at `B − smooth`: below it, a segment
  at `B` entering the search cannot move the fold. ≥ 0 and 0 on the complex, so the bare
  node contours to nothing; it is a source to give a thickness to.
- **Gradient.** `trueGradientAt` is analytic, from the same query, magnitude kept: a
  bisector's is `−e/|e|`, a face plane's `−n`, a segment's the unit offset from its nearest
  point, weighed `(1 − h/2, h/2)` by each fold step, zero where a clamp holds the value.
  `gradientAt` normalises it, `(0, 0, 1)` where it vanishes.
- **1-Lipschitz, so the default `cellOverlaps`.** Each primitive's distance is 1-Lipschitz;
  each fold step's gradient is a convex combination; two cells meet on a wall where both
  sides read 0, so the piecewise value is continuous. The argument is the comment at the top
  of `src/implicit/porous.cpp`.
- **`bounds()`** is the volume's box. The field keeps the packed complex and that box, nothing
  that points back at the caller's fields; evaluation is `const` and thread-safe.
- **Errors** past the seed stages: a negative or non-finite `smooth`, and for struts a
  `smooth` ≥ `B` (the field would be 0 everywhere).

## Determinism

The same inputs give bit-identical seeds, tets and packed arrays on any OS and at any thread
count. PCG32 (O'Neill's XSH-RR 64/32) is the only randomness — placement draws from
`Pcg32(seed, 0)`, Lloyd iteration `i` from `Pcg32(seed, i + 1)` — and the gaussian ramp's
exponential is DualC's own polynomial, not libm's. The exact predicates decide every
orientation; the only floating point outside them in the tetrahedralization is the Morton
key. The four porous sources compile with `-ffp-contract=off` (GCC / Clang; MSVC's default
does not contract). Lloyd's samples are evaluated in parallel (`SeedParams::numThreads`,
0 = all) and summed in sample order; every other stage is sequential.

## The constants

| Constant | Value | Decides | Where |
| --- | --- | --- | --- |
| `kPorousMaxSeeds` | 2,000,000 | seeds, and points tetrahedralized, past which the build throws | `porous.h` |
| `kPorousMaxRelax` | 50 | the most Lloyd iterations | `porous.h` |
| Box ceiling | 32 × `kPorousMaxSeeds` cells of the smaller spacing | a padded box refused before anything is allocated | `kMaxBoxCells`, `porous_seeds.cpp` |
| Bridson tries | 30 per active seed; 4 per scan cell | `poisson`'s coverage | `porous_seeds.cpp` |
| Lloyd samples | 32 per seed per iteration, weight exponent 5 | the relaxation's centroids | `porous_seeds.cpp` |
| Cancel polls | every 4,096 candidates and 65,536 Lloyd samples; 4,096 insertions; 4,096 items of each packing loop | the cancel latency of each stage | the three sources |
| `kPorousMaxBins` | 2048 × 8192 | the bin grid's ceiling, then 1.25× coarser bins | `porous_complex.h` |
| `kPorousMaxWalk` | 1024 steps | a walk's give-up, value 0 | `porous_complex.h` |

---

← Back to the [design index](README.md) · the [docs index](../README.md)
