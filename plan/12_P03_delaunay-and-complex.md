# P03 — Delaunay and the cell complex

§ 3 P03 of [the raw plan](../docs/raw/2026-10-19-porous-shell-plan.md); the engine and defaults
from the block's `01-study.md`. Branch `feat/porous-shell`. Never push.

### T1. Vendor the Delaunay engine and tetrahedralize the seeds

1. If P01 chose a vendored engine: copy its files into `src/internal/` unchanged, with the
   original headers. Compile them as SYSTEM (as `CMakeLists.txt` does for nanort and the QEF
   files), so `DUALC_WERROR` does not apply to them. Write a `THIRD_PARTY.md` section: origin URL,
   version or commit, licence, files, and any `LOCAL MODIFICATION (DualC)`. Make sure the gate's
   `vendoring` check (`scripts/check.py`, `scripts/check_data.json`) knows the new files.
   If P01 chose the fallback: write Bowyer–Watson with Shewchuk's public-domain predicates
   (vendored as above), in a DualC source file.
2. Add a `src/implicit/porous_delaunay.cpp` wrapper: seeds in, a tet list (vertex indices +
   neighbour indices) out, run sequentially and deterministically.
3. Tests in `tests/test_porous.cpp`: the four properties of the raw plan § 3 P03 T1.

#### Acceptance criteria

- `ctest --test-dir build -R porous` is green, `THIRD_PARTY.md` names every new vendored file,
  and the handoff records the tet count and time for 100k seeds.

### T2. The complex and its bin grid

1. From the tets, derive:
   - the Delaunay faces and edges, each once;
   - the Voronoi vertices (circumcentres; any outside the padded box are clamped and counted);
   - the Voronoi edges (adjacent tet pairs);
   - each seed's neighbour list, for the analytic Voronoi walls.

   Drop the primitives that lie wholly in the pad beyond what the clipped volume can see (the
   rule is recorded).
2. Pack the arrays in the layout P05 uploads: flat `float` / `int32` arrays, a CSR bin list,
   and the bin origin, size and dimensions, with the bin size ≥ the largest cell radius. Add a
   query returning the `smin_k` distance for the four `cells` × `mode` cases, with a clamped
   bound beyond 3×3×3.
3. Tests: the dual adjacency is consistent; the bin query equals the brute-force minimum on 10k
   random points for all four cases, with `k = 0` and `k > 0`.

#### Acceptance criteria

- `ctest --test-dir build -R porous` is green.
- The handoff records the packed sizes (bytes per seed) for a 100k-seed volume, which P05 needs
  for the texture limits.

## Exit criteria

- One function turns seeds into a queryable, packed complex for every `cells` × `mode`
  combination, exact against brute force.

← Back to the [plan](README.md).
