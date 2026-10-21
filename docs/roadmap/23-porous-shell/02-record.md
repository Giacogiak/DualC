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

**PLANNED** — P02. A PCG32 seed generator (`poisson` / `jitter` / `random`, graded density,
pad, Lloyd `relax`, ceilings, cancel polling) by the [defaults table](01-study.md#the-defaults-table);
acceptance: the six properties of the raw plan § 3 P02 in `tests/test_porous.cpp`, and the
seeds-per-second rate on a mesh volume at three spacings recorded here.

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
