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

**2026-10-21 — P02.1, the seeds.** `generatePorousSeeds` (`dualc/porous.h`,
`src/implicit/porous_seeds.cpp`): PCG32, bit-identical on any OS and at any thread count;
the six properties tested (`ctest -R porous`, 10 cases); about 55,000 seeds/s in an analytic
sphere and 5,500/s in a mesh torus, unrelaxed. **Chosen in-run (pending owner review):**
Lloyd weighted by ⁵ not ³ and drawing its own streams, Bridson's grid at `max(rmin, rmax/2)`,
`jitter` / `random` graded by thinning, a box ceiling before allocation. What was built, the
choices, the tests and the rates: [09](09-seeds-as-built.md).

## #64 Delaunay tetrahedralization and the packed complex

**DONE** 2026-10-21 — P03.1 built the tetrahedralization (T1), P03.2 the complex and its
bins (T2). Planned as: the engine of D-74 under `src/internal/` + `THIRD_PARTY.md`,
then the faces, edges, Voronoi vertices and edges, neighbour lists and bins packed as P05
uploads them; acceptance: Euler, empty sphere, the regular grid, determinism, dual adjacency
and the brute-force equality of the four `cells` × `mode` queries; the 100k-seed tet count,
time and bytes per seed recorded here.

**2026-10-21 — T1, the tetrahedralization.** `internal::tetrahedralize`
(`src/implicit/porous_delaunay.cpp`) on Shewchuk's `predicates.c`, vendored byte-identical
and compiled through DualC's `src/internal/exact_predicates.c`, which renames its symbols
`dualc_pred_*`. The hull is exact, closed by an infinite vertex, and the output is canonical.
100k white-noise points give 672,339 tets in 1.43 s (Release, one thread). On the study's
points the tet set is geogram's, hash for hash. The library now needs a C compiler. Five
`porous:` cases: Euler and the convex hull, the empty sphere, the regular grid, pinned
digests, and errors. **Chosen in-run (pending owner review):** the infinite vertex instead of
the table's finite super-tet; the symbol renaming; C as a project language. The files, the
numbers and the tests are in [03](03-delaunay-engine.md).

**2026-10-21 — T2, the complex and its bins.** `internal::buildPorousComplex(seeds, cells,
mode)` (`src/implicit/porous_complex.cpp`, with `PorousCells` / `PorousMode` in
`dualc/porous.h`) rounds the seeds to float, tetrahedralizes exactly those, derives the
faces, edges, neighbour lists and clamped circumcentres, and packs one case in flat `float` /
`int32` arrays with a bin grid; `internal::porousDistance(complex, p, smooth)` is the query.
Tested: the dual adjacency on blue noise and the regular grid; the query **bit-equal to brute
force** on 10k points in all four cases, k = 0 and k > 0; pinned digests; errors and cancel.
At 99,712 seeds the packed sizes are **87 (Voronoi walls), 492 (Voronoi struts), 526
(Delaunay walls) and 368 (Delaunay struts) bytes per seed**. **Chosen in-run (pending owner
review):** the smooth fold in ascending distance order, not packed order; struts clamped at
0; Delaunay walls 0 outside the seeds' hull; the struts' bin max(larger spacing, largest cell
radius); struts clipped to the volume's box grown by `B`, points and float duplicates
dropped. The layout, rules and numbers are in [04](04-complex-and-bins.md).

## #65 The porous node on the CPU

**DONE** 2026-10-22 — P04.1 built the field and the token (T1), P04.2 contoured the sponge
end to end (T2). Planned as: `PorousField` and the `porous` graph token (build, shorthand, JSON round
trip, `rejectUnreadParams`, `--list`); acceptance: Lipschitz-1 sampled, the smoothing bound of
the defaults table, unknown keys rejected, and a contoured `walls` sponge that is watertight
with more than one connected component; samples in `examples/samples/`.

**2026-10-21 — T1, the field and the token.** `PorousField` (`dualc/porous.h`,
`src/implicit/porous.cpp`) builds the seeds and the complex from the volume, then keeps only
the packed complex and the volume's box. Its value is the packed query; its gradient is
analytic (`porousDistanceGradient`, 3.2–4.6× the central difference's rate); `cellOverlaps`
is the default, argued in the file. The `porous` token takes volume[, control] and the twelve
keys; `dualc_field --list` shows it. Ten `porous:` cases: Lipschitz-1 on random pairs, the
smoothing bound (k/4 at two primitives, < k elsewhere), the gradient, a mesh volume, errors,
the round trip. **Chosen in-run (pending owner review):** the struts capped at `B − smooth`
(P03's cap at `B` steps by about 0.18 k at k > 0, measured); `smooth` ≥ `B` an error for
struts; `childBakeMargin` hands the volume the pad; no cancel token reaches the graph build.
The contract, the key table and the numbers are in [05](05-porous-field.md).

**2026-10-22 — T2, the sponge end to end.** The four recipes ran through `dualc_field -o` on
the unit sphere and the bracket, both `cells`, both `mode`s, at depth 6 and 7 (7: 4–23 s,
1.6–3.2 GB). Every walls case encloses cavities. The bracket's Voronoi foam and sponge are
watertight with a crisp rim. On the sphere, a crisp rim leaves 2–16 non-manifold edges at
any depth, as an existing clipped gyroid does; a `smooth-intersection` (k 0.1, depth 7) closes
it on 8 seeds of 8. Delaunay walls need the node's `smooth`. Test: `porous: a walls sponge
contours watertight with more than one component`. Samples: `examples/samples/porous-*.json`
(three). **Chosen in-run (pending owner review):** the smoothed rim in the test and the
sphere sample, seed 3 at depth 6, no new item for the rim. Numbers: [06](06-sponge-end-to-end.md).

## #66 The porous node in GLSL and dualc_field_view

**DONE** 2026-10-22 — P05; the NV92 run is the owner's review. Planned: data textures by [01 § (d)](01-study.md#d-the-gpu-layout),
the four cases, the `emitBakedGrid` fallback; acceptance: GL-free tests, four parity cases
(count bumped), the viewer's structural rebuild, the GPU tier green.

**2026-10-22 — T1.** The builder's `packed()` as data textures, a `smooth` uniform,
the bake past `GlslLimits`; 20 graphs within 2.4e-7 on llvmpipe; a first draft hung the
NV92 ([07](07-glsl-node.md)).
**2026-10-22 — T2.** Five parity cases, 87/87 through the gate's wrapper on llvmpipe (X
hung); the viewer takes the context's limits; snapshots
([08](08-parity-and-viewer.md)).
**Chosen in-run (pending owner review):** [07](07-glsl-node.md#the-in-run-choices),
[08](08-parity-and-viewer.md#the-in-run-choices).

## #67 The C ABI test and the documentation

**DONE** 2026-10-22 — P06.1. Planned: `cli_c_abi_porous` (an in-memory mesh volume, the
expression form), the "vocabulary widening" entry in 14/04, the `11-dualc_field` and
`12-dualc_field_view` pages, the design section and `STRUCTURE.md`.

**2026-10-22 — T1.** `dualc_c_demo … porous` over an in-memory cube: the JSON and the
shorthand contour byte-identical, 144,140 faces, watertight; the plain expression call over a
sphere 80,116, watertight; a missing `spacing` and an ungraded `spacing2` are located errors;
a token cancels the contour and the export (6.3 s, 0.45 GB). `c_abi` 6/6. The version stays
`dualc 0.5.2` by block 14's rule ([14/04](../14-c-abi/04-abi-0-4-0.md#the-porous-source-vocabulary-widening-no-abi-version-move)).
The usage page is [11/16](../../command_reference/11-dualc_field/16-porous-shell.md) (keys,
nine recipes run or taken from [06](06-sponge-end-to-end.md), the cost guide, depth 8 tiled
at 0.44 GB); the viewer's node and P17–P18 are on 12/02; the invariants on
[design/12](../../design/12-field-graph.md#the-porous-source); a host's usage in
`capi/README.md`. **Chosen in-run (pending owner review):** #63's detail moved to
[09](09-seeds-as-built.md) to make room here; the node's `relax` ignores `--threads` and
`numThreads` (documented, not changed); the skinned sphere recipe is listed with its 8
non-manifold edges at depth 7. **For the owner's review:** P17 live and P18 on the NV92
(the steps are in the P06.1 handoff, gathered by P08).

## #68 The Boletus implementation plan

**DONE** 2026-10-22 — P07.1. Planned: a raw plan on Boletus's branch `feat/porous-shell-plan`, commit only: the
DualC pin, the `Ops.cs` entry, a *Porous Shell* component, the preview, the tests, the D-NN
rows it would open; one pointer line here or in [15](../15-boletus-handoff.md).

**2026-10-22 — T1.** Boletus `docs/raw/2026-10-22-porous-shell-plan.md` and its `docs/raw/README.md`
row, commit `1ccbe23` on Boletus `feat/porous-shell-plan` (cut from Boletus `main` at `2423275`,
which is untouched; not pushed); Boletus's `check.py --docs --strict` PASS. Its findings: the
node needs no ABI change, and Boletus's `dualc-links` reads this tree with 0 missing; Boletus's
`OpSchema.ChildCount` is exact, so the plan widens it to a 1–2 range; Boletus's `Proxy preview`
creates the field on the solve thread, so the uncancellable build blocks the canvas (its D-50,
DEFERRED); its Live preview hands the viewer a mesh volume as an OBJ, and whether that yields
the in-memory path's cells is untested (its Step 5 test, D-52 if they differ). The re-pin
waits for this branch's merge into `main` on GitHub. It names Boletus D-49 … D-52, block 11,
items from #35, without writing them. **Chosen in-run (pending owner review):** one Sources
component, all twelve keys as inputs (`Jitter` appended to the raw plan's list), a key emitted
only off DualC's default, the ranges left to DualC's located errors, `Seed` a 32-bit integer.

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

## #70 Smooth booleans keep the viewer's metric step

**DONE** 2026-10-27 — acceptance: a porous sponge recipe (a `smooth-intersection`)
compiles at step scale 1.0, so `dualc_field_view` steps by the value without the per-step
gradient, and its image does not move against a fine reference. Opened the same day from
the owner's report: the sponge was "extremely laggy" to orbit on a GTX 1050.
`nodeStepScale` (`examples/field_glsl.cpp`) no longer clamps a smooth boolean to 0.6: the
polynomial smin of 1-Lipschitz operands is 1-Lipschitz. The sponge now costs 4–7× fewer
evaluations per pixel, and 5–9× fewer in the worst pixel. The snapshots are identical
except 3 pixels in 480,000. Gate: `check.py` 30/30 (1 skipped; ctest 408/408, 0 warnings), parity 87/87 on llvmpipe. The measurements are in [11](11-viewer-trace-cost.md). Pending:
the owner's orbit on the GTX 1050. Merged into the local `main` only, 2026-10-27, without a
PR (the owner's choice): it reaches `origin` with the next PR from that `main`.

**2026-10-28:** pushed straight to `origin/main` on the owner's explicit request
(`4f02b51..01acc74`, a fast-forward, with the session-close commits `ecb5351` and `01acc74`).
CI run 40648310760 on `01acc74` passed all 7 jobs: `docs`, `gpu`, `sanitize`, and `build` on
Ubuntu 24.04, macOS 14 and Windows 2022.

---

← Back to [block 23](README.md).
