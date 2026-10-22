# The seeds as built — #63, the generator, its choices, tests and rates

The detail behind [02 #63](02-record.md#63-deterministic-seeds-in-any-volume) (P02.1,
2026-10-21). *(Moved here verbatim from 02 on 2026-10-22 by P06.1: 02 was at its byte cap and
#67–#68 still had to land there; 02 keeps the item's acceptance line and a summary.)*

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

---

← Back to [block 23](README.md).
