# Porous shell — the tracked items and the owner's decisions

Part of [block 23](README.md). Each item keeps its acceptance line and takes dated appends;
the study and the defaults every phase follows are [01](01-study.md).

## The owner's decisions

**DONE** 2026-10-21 — decided by Giacomo 2026-10-19 in the planning session
([raw plan § 1](../../raw/2026-10-19-porous-shell-plan.md#1-the-owners-decisions)), recorded
by P01 as rows in [the settled decisions, continued](../../decisions/03-settled-continued.md):

- **D-67** — the cells are both, a parameter `cells=voronoi|delaunay`, `voronoi` the default:
  Voronoi cells of relaxed blue-noise seeds are what foam, bone and sponge look like, and the
  Delaunay faces are the literal reading of the request; one tetrahedralization gives both.
- **D-68** — two porosities, `mode=walls` (closed cells, each interior cell a sealed cavity) and
  `mode=struts` (open-cell foam along the edges); boundary cells are open pores in both;
  perforated walls are out of scope.
- **D-69** — the thickness reuses `onion`, `graded-onion` and `graded-offset` unchanged: the
  node is the unsigned-distance "isosurface" source, as a TPMS is.
- **D-70** — corner smoothing is implicit, in the node: a polynomial smooth-min of radius
  `smooth=k` over the nearby primitives; `k = 0` is the sharp complex.
- **D-71** — the distributions are several and extensible: `poisson` (the default), `jitter`,
  `random`, `relax=N` Lloyd iterations, and a graded density over a control field with a
  `linear|smooth|gaussian` ramp.
- **D-72** — Boletus gets a plan document only, on its own branch; nothing is implemented there.
- **D-73** — the run is unattended overnight: every gate `auto`, no session stops on a
  question (an open choice takes the plan's default, marked *Chosen in-run (pending owner
  review)*), and the heavy checks — the full gate, the GPU tier, `--docs --strict`, the
  semantic lint — run at the end (P08); P01–P07 run a targeted build and their own tests.

## The engine choice

**DONE** 2026-10-21 — **Chosen in-run (pending owner review):** D-74, the Delaunay engine is
our own sequential Bowyer–Watson on Shewchuk's public-domain predicates, not geogram's
Delaunay PSM, the default the plan asked the study to beat. The PSM builds clean but runs on a
process-wide, unlocked `mt19937_64` behind `GEO::initialize()`, and its tet list changes
between calls in one process; the plan's rule gives the fallback the win. Evidence:
[01 § (a)](01-study.md#a-the-delaunay-engine). D-75 records where these rows live:
`01-settled.md` was at its byte cap, so the settled rows from D-67 on open a child page
`03-settled-continued.md`, as D-61 did for the dropped rows.

## #63 Deterministic seeds in any volume

**DONE** 2026-10-21 — P02.1. Planned as: a PCG32 seed generator (`poisson` / `jitter` /
`random`, graded density, pad, Lloyd `relax`, ceilings, cancel polling) by the
[defaults table](01-study.md#the-defaults-table); acceptance: the six properties of the raw
plan § 3 P02 in `tests/test_porous.cpp`, and the seeds-per-second rate on a mesh volume at
three spacings recorded here.

**2026-10-21 — what was built.** `include/dualc/porous.h` (`Pcg32`, `SeedDistribution`,
`DensityRamp`, `densityRampWeight`, `SeedParams`, `SeedSet`, `generatePorousSeeds`,
`kPorousMaxSeeds` = 2,000,000, `kPorousMaxRelax` = 50), `src/implicit/porous_seeds.cpp`
(compiled `-ffp-contract=off` on GCC/Clang) and the header-only `src/internal/seed_grid.h`
(a uniform grid of point indices with O(1) insertion and an exact nearest query, ties to the
smaller index; P03 can reuse it for the start bins). Bit-identical on any OS: the only
randomness is PCG32 (its first six outputs for `(42, 54)` equal O'Neill's reference, tested),
and the only transcendental, the gaussian ramp's exponential, is DualC's own range-reduced
polynomial (exact to 1e-14 against libm, tested). Lloyd's samples are evaluated in parallel
and summed in sample order, so the thread count does not change the bits (tested 0 vs 1).
Errors are `std::invalid_argument` (P04's node maps them to `GraphError`); a requested token
throws `Cancelled` (polled every 4,096 candidates and every 65,536 Lloyd samples).

**Chosen in-run (pending owner review)**, each a refinement of a † row of the defaults table:

- **Lloyd weights by (spacing / spacing(x))⁵, not ³.** A centroidal Voronoi tessellation of
  density ρ has cells of size ρ^(−1/(d+2)) (Du, Faber, Gunzburger 1999), so ρ ∝ spacing(x)⁻⁵
  is what keeps the cell size proportional to the spacing in 3D. Measured on the graded box of
  the test (spacing 0.5 → 0.8, expected dense/sparse ratio 4.096), mean of four seeds: no relax
  4.01; relax 10 3.91 with either exponent; relax 30 3.96 with ⁵ and 3.78 with ³.
- **Lloyd draws 32 fresh samples per seed per iteration from the stream `Pcg32(seed, i + 1)`**
  (placement uses `Pcg32(seed, 0)`). Samples outside `V < pad` are dropped, not redrawn.
- **Bridson's background grid** is a linked-list grid at `max(rmin, rmax/2)`, not `rmin/√3`:
  the conflict search then spans at most 5³ cells even with a graded radius. When the active
  list empties, a scan over cells of `rmax` (4 tries each) restarts it, so every connected
  part of the volume gets seeds. Shell candidates come from rejection in a cube, no trigonometry.
- **`jitter` and `random` grade by thinning:** the grid or the white noise is laid at the
  smaller spacing and a point is kept with probability (min spacing / spacing(x))³.
- **A box ceiling before allocation:** a padded box holding more than 32 × 2,000,000 cells of
  the smaller spacing is refused up front; the seed ceiling itself is checked per accepted seed.
- `spacing2` ≠ `spacing` without a control, or a graded density with `d1 = d0`, is an error.

**2026-10-21 — the properties tested** (`ctest -R porous`, 10 cases, 3.7 s with the graph and
codegen cases): same seed → same digest and another seed → another, on a sphere and a box,
all three distributions, graded, `relax=2`; Poisson minimum distance ≥ 0.99 × the local
`(r(a) + r(b))/2`, uniform and graded; graded density ratio within 20 % of
(spacing2/spacing)³ for every distribution and for `poisson` + `relax=10` (measured 4.23,
4.34, 3.47 and 3.93 against 4.096 for seed 5; means over 20 seeds 4.03, 4.18, 4.08); every
seed has `V < pad` and is flagged pad exactly where `V > 0`; `relax=10` lowers the interior
cell-volume CV (box: poisson 0.119 → 0.092, random 0.421 → 0.119; sphere: 0.152 → 0.095,
0.388 → 0.124); a requested token, and one flipped mid-build, mid-relax, throws `Cancelled`;
bad parameters and both ceilings throw; the grid's nearest equals brute force.

**2026-10-21 — measured: seeds per second** (the hidden case
`build/tests/dualc_tests "[porous-bench]"`; this Linux host, 8 threads, GCC 15, Release,
`poisson`, two runs, the slower given). The volume's `valueAt` dominates: 8.6 calls per seed
on the torus at spacing 0.05, about 21 µs each for the `MeshSource` default sign.

| Volume | Spacing | Seeds | `relax=0` seeds/s | `relax=5` seeds/s |
| --- | --- | --- | --- | --- |
| demo torus 1 / 0.35, 9,216 triangles, `MeshSource` | 0.1 | 2,562 | 5,760 | 1,129 |
| the same | 0.05 | 15,594 | 5,511 | 991 |
| the same | 0.025 | 106,484 | 5,466 | 913 |
| analytic sphere r = 1 | 0.1 | 3,458 | 54,949 | 29,888 |
| the same | 0.05 | 23,344 | 54,647 | 27,482 |
| the same | 0.025 | 171,247 | 56,514 | 23,302 |

The ceilings stand: 2,000,000 seeds on a mesh volume is about 6 minutes unrelaxed, and each
Lloyd iteration costs 32 `valueAt` per seed. A lead for P04's docs, not built: bake a mesh
volume to a grid (`bakeToGrid`) before `porous`, or reuse one sample pool across iterations.

## #64 Delaunay tetrahedralization and the packed complex

**PLANNED** — P03. The engine of D-74 under `src/internal/` + `THIRD_PARTY.md`, then the
faces, edges, Voronoi vertices and edges, neighbour lists and bins packed as P05 uploads them;
acceptance: Euler, empty sphere, the regular grid, determinism, dual adjacency and the
brute-force equality of the four `cells` × `mode` queries; the 100k-seed tet count, time and
bytes per seed recorded here.

## #65 The porous node on the CPU

**PLANNED** — P04. `PorousField` and the `porous` graph token (build, shorthand, JSON round
trip, `rejectUnreadParams`, `--list`); acceptance: Lipschitz-1 sampled, the smoothing bound of
the defaults table, unknown keys rejected, and a contoured `walls` sponge that is watertight
with more than one connected component; samples in `examples/samples/`.

## #66 The porous node in GLSL and dualc_field_view

**PLANNED** — P05. Data textures by [01 § (d)](01-study.md#d-the-gpu-layout), the emitter for
the four cases, the `emitBakedGrid` fallback; acceptance: GL-free tests, at least four parity
cases with `parity_expected_cases` bumped, the structural-key rebuild in the viewer, and the
GPU tier green.

## #67 The C ABI test and the documentation

**PLANNED** — P06. `cli_c_abi_porous` (an in-memory mesh volume, the expression form), the
"vocabulary widening" entry in 14/04, the `11-dualc_field` and `12-dualc_field_view` pages, the
design section and `STRUCTURE.md`.

## #68 The Boletus implementation plan

**PLANNED** — P07. A raw plan on Boletus's branch `feat/porous-shell-plan`, commit only: the
DualC pin, the `Ops.cs` entry, a *Porous Shell* component, the preview, the tests, the D-NN
rows it would open; one pointer line here or in [15](../15-boletus-handoff.md).

## #69 Graded nodes miss a negative thickness in cellOverlaps

**DEFERRED** — trigger: a `graded-offset` with `t1` or `t2` below 0 that loses part of its
surface, or the next change to `src/implicit/decorators.cpp`'s graded nodes; opened
2026-10-21 by P01, the raw plan § 2 and § 4 (recorded here, not fixed in this run).
`GradedOnionField` and `GradedOffsetField` grow the probed cell by `max(t1, t2, 0)`
(`src/implicit/decorators.cpp`, their `maxThickness()`), so a negative `t` — a `graded-offset`
that shrinks the solid — is not covered: the surface sits `|t|` inside the base's zero set,
and the base's Lipschitz test can call the cell empty. `OffsetField` grows by `|r|` and has no
such gap. The fix is to grow by `max(|t1|, |t2|)`, the
[#46](../17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#46-offsetfield-does-not-forward-celloverlaps)
bug class ([17/03 #25](../17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#25-lipschitzbound--the-silent-correctness-opt-out-behind-celloverlaps)
would retire it with the rest). `graded-onion` with a negative `t` is empty, so the gap only
bites `graded-offset`.

---

← Back to [block 23](README.md).
