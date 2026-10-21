# Handoff — P03.2: the complex and its bin grid

Unit P03.2 of [`plan/12_P03_delaunay-and-complex.md`](../12_P03_delaunay-and-complex.md) § T2,
the last unit of P03; session of 2026-10-21, branch `feat/porous-shell` in
`~/Documents/DualC-porous`. Nothing was pushed, no remote was touched, and `main`,
`feat/hygiene-and-editor` and `~/Documents/DualC` were left alone. The record:

- [23/02 #64](../../docs/roadmap/23-porous-shell/02-record.md#64-delaunay-tetrahedralization-and-the-packed-complex)
  is now **DONE** (T1 + T2), with a dated T2 append;
- the layout, the rules and the numbers are in the new child page
  [23/04](../../docs/roadmap/23-porous-shell/04-complex-and-bins.md).

This handoff points at those pages and adds what P04 and P05 need.

## What changed, and why

| File | What |
| --- | --- |
| `include/dualc/porous.h` | `enum class PorousCells { Voronoi, Delaunay }`, `enum class PorousMode { Walls, Struts }` — public, beside `SeedDistribution`, because P04's field takes them |
| `src/internal/porous_complex.h` (new) | `PorousDual`, `PorousBins`, `PorousComplex` (+ `packedBytes()`), `PorousComplexStats`, `PorousQueryStats`; `derivePorousDual`, `packPorousComplex`, `buildPorousComplex`, `porousDistance`; the shared pieces `porousSmin`, `porousSmoothFold`, `porousBisectorDistance`, `porousSegmentDistance`, `porousPlaneDistance`, `porousPackedPoint`; `kPorousMaxWalk` = 1024, `kPorousMaxBins` = 2048 × 8192 |
| `src/implicit/porous_complex.cpp` (new) | The dual, the four packers, the start-bin sweeps, the three query kinds |
| `CMakeLists.txt` | The new source, in the `-ffp-contract=off` list with the seeds and the tets |
| `tests/test_porous.cpp` | 4 new `porous:` cases + the hidden bench `porous: complex sizes and query rates` (`[.][porous-bench]`); a brute-force reference (`Brute`) |
| `plan/12_P03_delaunay-and-complex.md` | One italic note on T2 step 2 (see "Plan corrected" below) |
| `docs/roadmap/23-porous-shell/{02-record,04-complex-and-bins,README,01-study}.md`, `docs/roadmap/README.md`, `STRUCTURE.md` | #64 DONE; the new page 04; italic P03 notes on four rows of the defaults table; the status phrases; the file map |

### The contract P04 and P05 build on

- **One function:** `internal::buildPorousComplex(const SeedSet&, PorousCells, PorousMode,
  const CancelToken*)` → `PorousComplex`; query `internal::porousDistance(complex, p, smooth,
  stats = nullptr)`, `const` and thread-safe (no shared state; a stack buffer of 64
  candidates, spilling to the heap). To build several cases from one Delaunay, call
  `derivePorousDual(seeds)` once and `packPorousComplex(dual, seeds, cells, mode)` per case.
- **The seeds are rounded to float first**, and the tetrahedralization is that of the float
  points: what is packed is exactly what was triangulated. `dual.points` holds them.
- **The value** (k = `smooth` ≥ 0; k = 0 is the exact distance):
  - Voronoi walls: greedy walk to the nearest seed from the bin's start, then the bisector
    distances to its Delaunay neighbours, folded, `max(0, ·)`; exact everywhere.
  - Delaunay walls: visibility walk on the packed float planes, the 4 face-plane distances,
    folded, `max(0, ·)`; **0 outside the seeds' hull**.
  - Struts: the segments within `B` in the 3×3×3 bins (neighbour bins pruned by
    `min(m + k, B)`), folded, `max(0, min(·, B))`.
  - **The fold is ascending distance, ties by packed index**, over the candidates with
    `d < m + k`, each index once (`porousSmoothFold`).
- **The layout** (all `int32` with −1 = none, all geometry `float`) is the table in
  [23/04 § The packed layout](../../docs/roadmap/23-porous-shell/04-complex-and-bins.md#the-packed-layout-what-66-uploads).
  Bin `(i, j, k)` is flat `(k·dims[1] + j)·dims[0] + i`; a point outside the grid falls in the
  nearest boundary bin; `bins.coord` is the one formula (floor of `(v − origin)/size`,
  clamped), which the GLSL must copy.
- **Errors:** `std::invalid_argument("porous complex: …")` for a `SeedSet` whose `isPad`
  does not match its points, non-positive or inverted spacings, a non-finite box, a negative
  pad; whatever `tetrahedralize` throws; `Cancelled("porous complex")`, polled every 4,096
  items in every loop.

## The in-run choices

Each is **Chosen in-run (pending owner review)**, recorded in 23/04 § The rules and in the
02 #64 append, and each refines a row of the defaults table (italic notes added there):

1. **The smooth fold runs in ascending distance order, ties by packed index,** not in packed
   order. Ascending, the accumulator never exceeds the minimum m, so a primitive at m + k or
   beyond leaves the value unchanged. The value then depends only on the primitives within k
   of m, is symmetric and continuous in them, and stays 1-Lipschitz. In packed order a far
   primitive folded first changes the result, so the value would depend on which bins were
   searched, and a primitive crossing the bin bound would make it jump.
2. **Struts are clamped at 0,** as the walls are. A smoothed junction dips below 0, and
   `onion`'s `|·|` would thin the material there. Found by the test (`q >= 0` failed at k > 0).
3. **Delaunay walls are 0 outside the seeds' hull** (solid). The study's "plane of the face
   the walk left by" depends on the walk's path, so it is discontinuous. 0 is continuous with
   the faces, keeps the field 1-Lipschitz, and needs no extra texture.
4. **The struts' bin `B` = max(the larger spacing, the largest cell radius).** The cell radius
   is the largest circumradius over the tets that have a non-pad vertex and a circumcentre in
   the volume's box. Measured: 1.22 × the spacing for `poisson`, 1.17 × the larger spacing
   for a 2:1 grading, 0.87 × for the regular grid.
5. **The drop rule:**
   - Struts are clipped to the volume's box grown by `B`. A Voronoi edge or a hull face's ray
     that misses that box is dropped, which is 4.2 % of the edges at 100k.
   - A segment that is a point, or the same float segment twice, is dropped. On the regular
     grid that is 1,288 points and 732 duplicates out of 2,784 candidates.
   - Walls drop nothing.
   - This is exact for any volume. A tighter rule needs the volume's values (see Open).
6. **The start bins come from sweeps, not from a nearest search per bin.** A bin starts at
   the nearest of its own seeds, or else at the nearest of its 26 neighbours' starts, filled
   by a forward and a backward sweep. A grid past `kPorousMaxBins` gets bins 1.25 × coarser
   until it fits. Found when the capped-grid test hit `bad_alloc` and then ran for minutes
   (see below).

## Measured, and under what

This Linux host, GCC 15, Release, one thread; two runs, the slower given. Bench:
`build/tests/dualc_tests "porous: complex sizes and query rates"`.

**100k seeds** (`poisson`, box ±0.65, spacing 0.0245 → **99,712 seeds**, 646,985 tets,
1,294,262 faces, 746,988 edges, 35,100 clamped circumcentres; dual 1.83 s):

| Case | **Bytes per seed** | Packed | Pack | Queries/s k = 0 / k = 0.3 × spacing |
| --- | --- | --- | --- | --- |
| Voronoi walls | **87.0** | 8.7 MB | 0.37 s | 1.39M / 1.09M |
| Voronoi struts | **491.8** | 49.0 MB | 1.13 s | 215k / 91k |
| Delaunay walls | **526.1** | 52.5 MB | 0.61 s | 2.07M / 1.90M |
| Delaunay struts | **367.7** | 36.7 MB | 0.55 s | 227k / 93k |

- **Array elements per seed** (for P05's texture sizes):
  - Voronoi walls: seeds 4 floats, offsets 1, neighbours 15.0, start bins 1.76.
  - Voronoi struts: 99.5 floats of segments (12.4 segments, 2 RGBA texels each), offsets
    0.98, items 22.5.
  - Delaunay walls: 103.8 floats of planes (6.5 tets × 4 RGBA texels), neighbours 26.0,
    start bins 1.76.
  - Delaunay struts: 59.9 floats of segments (7.5 edges), offsets 0.98, items 31.0.
- **Seeds that fit a 2048 × 8192 texture** (16.7M texels), set by the largest array:
  - Delaunay walls (the planes): ≈ 645k;
  - Voronoi struts (the segments): ≈ 673k;
  - Delaunay struts (the bin items): ≈ 540k;
  - Voronoi walls (the neighbours): ≈ 1.1M.

  Past those, the node falls back to `emitBakedGrid`, as the study planned.
- **Struts:** `B` = 0.0300, 46³ bins. Items per bin: mean 23.0, max 56 (Voronoi); mean 31.8,
  max 89 (Delaunay). Items visited per query: 73–76 at k = 0, 160–169 at k > 0.
- **Walls:** 0.50 seed steps (max 2) and 1.55 tet steps (max 20) per query.
- **Graded 0.03 → 0.06 along x** (27,192 seeds):
  - 96 / 463 / 533 / 324 bytes per seed;
  - `B` = 0.070;
  - items per bin: mean 53 / 61, max 179 / 170.

## Checks run

These follow the rules for P01–P07: no full gate.

- `cmake --build build -j`: no warnings.
- `ctest --test-dir build -R porous`: 20/20 passed (15 earlier and 5 new, 9.1 s; the
  brute-force case takes about 8 s).
- `ctest --test-dir build -R 'porous|field-graph|GLSL codegen' -j8`: 39/39 passed.
- The driver's smoke filter `ctest -R 'porous|field_graph|field_glsl'`: 20/20 passed.
- `python3 scripts/check.py --docs`: 27 OK + 1 INFO, PASS. The pre-commit hook ran on the
  commit.
- No docs file was deleted or moved, so no loss audit was due.
- **Not run:** the full gate, `--docs --strict`, any MSVC or Apple Clang build. These belong
  to P08 and CI.

## What I got wrong on the way

- **ADL ambiguity:** my local `dot`/`cross` clashed with geometry-central's through argument
  lookup. They are now `dot3`/`cross3`.
- **Negative strut values at k > 0.** I had copied the study's clamp to the own-cell cases
  only. The `q >= 0` assertion caught it.
- **The "no clamp inside the volume" check first used the volume's box,** whose corners lie
  outside a sphere volume and have no seeds near them. It now checks the points whose
  nearest seed is not a pad seed.
- **The start bins were a `SeedGrid::nearest` per bin.**
  - On a sparse box this allocated a grid of 3000³ cells (`bad_alloc`).
  - Once the grid was sized to the bins, each query in empty space scanned rings across the
    whole grid, and the test ran for more than 5 minutes.
  - Replaced by the two sweeps (rule 6), and the cap became a test parameter (`maxBins`).
  - Two pinned digests (the walls' start bins) changed with the sweeps and were re-pinned in
    the same uncommitted change.
- **Ran `"[porous-bench]"` as a tag,** which also started P02's slow mesh benches. Run the
  bench by its case name.

## Plan corrected

T2 step 2 says "the bin size ≥ the largest cell radius". That holds for the struts' bins,
where the value is exact only below `B`. The own-cell bins only hold a walk's start, so they
keep the defaults table's smaller spacing: the result does not depend on their size, and a
finer bin shortens the walk. An italic note in the plan file says so. The section's
acceptance criteria themselves needed no change.

## T2 acceptance and the P03 exit criteria

- **`ctest --test-dir build -R porous` is green:** 20/20 passed, as listed above.
- **The handoff records the packed sizes for a 100k-seed volume:** 87.0 / 491.8 / 526.1 /
  367.7 bytes per seed at 99,712 seeds, in the table above and in 23/04.
- **T2 step 1:**
  - faces, edges, Voronoi vertices (clamped and counted), Voronoi edges and neighbour lists
    are in `PorousDual`;
  - the drop rule is recorded (rule 5).
- **T2 step 3:** `porous: the dual adjacency is consistent` and `porous: the bin query
  equals brute force in all four cases` (10k points, k = 0 and k = 0.3 × spacing, on blue
  noise in a sphere and on the regular grid).
- **Exit criterion — "One function turns seeds into a queryable, packed complex for every
  `cells` × `mode` combination, exact against brute force": holds.**
  - The function is `buildPorousComplex`, queried with `porousDistance`.
  - All 16 runs (2 seed sets × 4 cases × 2 values of k, 10k points each) are **bit-equal**
    to the brute force, which uses no bins and no walks. The Delaunay-walls check allows
    1e-6 for a point within rounding of a face, and none needed it.
  - Against the geometry, the Delaunay walls match the point–triangle distance to every
    face within 8.9e-8 (the float planes).
  - The capped-grid case is equal to brute force within 1e-6 in all four cases.
- **Exit criteria left open by P03.1:** none. Its four properties are still green within
  the 20/20.
- **Everything is committed and the tree is clean.**

## Open, and what the next units need

- **P04 (`PorousField`):**
  - Build one `PorousComplex` per node, at construction or lazily under a `once_flag`, and
    call `porousDistance`.
  - The field is 1-Lipschitz for all four cases and both values of k, so the default
    `cellOverlaps` holds. P04's sampled test is what confirms it.
  - The gradient is not provided. Use central differences, or derive it from the minimum
    primitive, which the query could return later.
  - **A tighter drop rule** (rule 5) needs the volume. P04 has it and could prune struts by
    the volume's value along each segment, but only when the volume is 1-Lipschitz
    (`MeshSource` and the exact primitives are; a TPMS is not).
  - The smoothing bound for P04's test: < k in general, k/4 for two primitives (the defaults
    table). The ascending fold keeps both.
  - The struts' clamp is `B` ≈ 1.2 × the larger spacing, so a strut radius plus `smooth`
    must stay below it (study (c)).
- **P05 (GLSL):**
  - Port `bins.coord`, the walks and `porousSmoothFold` exactly as written. The fold needs a
    small sorted array of the candidates with `d < m + k`, each index once. At 100k seeds,
    160–169 strut items are visited per query at k > 0, but only the few within k are kept.
  - Indices are `int32` with −1 = none: upload them as `R32I` / `RGBA32I` (`isampler2D`), or
    as UI with `0xFFFFFFFF` for −1.
  - The texture limits are above.
  - Only the planes of `tetPlanes` are needed for the Delaunay walls; there is no vertex
    texture.
- **Cross-OS determinism:** `porous: the complex is deterministic and pinned` pins four
  digests of the packed arrays of the sphere set. They come from this host and are only
  claimed to be OS-independent: the code uses IEEE `+ − × ÷ √`, no FMA, exact predicates for
  every sign and index-fixed orders. The first Windows and macOS CI run confirms or refutes
  this. If only the struts digests fail, look at `voronoiEdge`'s line parameters first.
- **MSVC `/W4`** is unverified for the new file. I kept to explicit casts and no shadowing in
  nested scopes. CI's `windows-2022` job checks it on the first push.
- **Test time:** the brute-force case takes about 8 s, mostly the strut brute force over
  6,378 segments × 10k points. If the gate's time matters, precompute each segment's box
  once in `Brute`.
- **The owner's review list (P08.2) gets** the six in-run choices above.
- **No human check is raised by this unit.** Everything was verified headless, and the
  cross-OS and MSVC points are CI's job.
