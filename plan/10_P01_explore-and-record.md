# P01 — Explore, open the record, configure the worktree

§ 1, § 2 and § 3 P01 of [the raw plan](../docs/raw/2026-10-19-porous-shell-plan.md). Branch
`feat/porous-shell` in `~/Documents/DualC-porous`. Never push.

### T1. The exploration study, the new roadmap block, the build trees

1. Check that you are on `feat/porous-shell` and that the branch contains the hygiene run's
   commits (`git log --oneline feat/hygiene-and-editor -1` is an ancestor). If it does not,
   continue anyway and record it in the handoff: the owner's rebase was skipped.
2. **Read the next free IDs from the files:** the last roadmap block in `docs/roadmap/`, the last
   `#N` (the ledgers and block READMEs), the last `D-NN` in `docs/decisions/README.md` and
   `01-settled.md`. The hygiene run takes block 22, #62 and D-65 first. Never trust a number in
   the plan over the files.
3. **Study, in scratch** (never commit prototypes):
   - (a) The Delaunay engine, by the raw plan § 3 P01 criteria. Fetch geogram's Delaunay PSM
     (`Delaunay_psm.h` / `.cpp` from the geogram release or repository) into its own scratch
     directory, build a 30-line driver against it, and time 100k random points and a 20³
     regular grid. Check its licence text and its determinism: two runs give the same tet list.
     Size the own Bowyer–Watson + Shewchuk fallback by reading, then choose. If geogram's PSM
     will not build cleanly with `-Wall -Wextra` under SYSTEM, needs a global state that is not
     thread-safe, or is not deterministic, the fallback wins.
   - (b) The distributions, with a 2D or 3D scratch rendering if it helps: Bridson
     variable-radius Poisson-disk, jittered grid, white noise, Lloyd/CVT (sample-based, its cost
     per iteration), a gaussian density.
   - (c) The distance formulas: the Voronoi wall (bisector), the Voronoi edge (circumcentre
     segments), the Delaunay face and edge, the smooth-min and its bound, and the bin-grid bound
     (the largest cell radius against the bin size; what to return beyond 3×3×3).
   - (d) The GPU layout: RGBA32F 2D data textures, `texelFetch`, the per-texture size limits on
     the GTX 1050 Ti / Intel HD 630 class and WebGL2, and the `emitBakedGrid` fallback.
4. **Open the record.** The next free block, as a folder: `README.md` (index with the items and
   pages table, and status) plus `01-study.md` holding the study, ending in **the defaults
   table** that binds P02–P07. The node design in the raw plan § 3 stands unless the table
   changes it, with the reason.
   - Tracked items, IDs read from the files: seeds, Delaunay + complex, CPU node, GPU node,
     ABI + docs, Boletus plan. Add the graded nodes' negative-`t` `cellOverlaps` gap (raw plan
     § 2) as its own DEFERRED item unless the hygiene run already tracks it.
   - Decision rows in the decisions index's style: the § 1 rows (cells, mode, thickness reuse,
     smoothing in the node, distributions, Boletus plan-only, unattended + checks at the end),
     and the engine choice marked `**Chosen in-run (pending owner review)**`.
   - `docs/roadmap/README.md`: § Current focus, § Next up and the blocks table, within the
     page's size cap (split by the cap rule if needed, never append past it).
5. **Configure and build the worktree's trees** (a worktree has none):
   - `build/`: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDUALC_BUILD_TESTS=ON -DDUALC_BUILD_EXAMPLES=ON`,
     plus the C ABI option if the gate's configure in `scripts/check.py` passes it now (hygiene
     P05), so the trees match the gate.
   - `build-gl/`: the same, plus `-DDUALC_BUILD_FIELD_VIEW=ON -DDUALC_BUILD_GLSL_PARITY=ON`.
   - Build both once, and record the wall time in the handoff.

#### Acceptance criteria

- The block exists, with `01-study.md` and its defaults table. The decision rows and tracked
  items have IDs read from the files and listed in the handoff.
- `build/` and `build-gl/` exist and build. `python3 scripts/check.py --docs --strict` is green,
  and everything is committed.

## Exit criteria

- Every later unit can take its defaults from the block's `01-study.md` without asking anyone.

← Back to the [plan](README.md).
