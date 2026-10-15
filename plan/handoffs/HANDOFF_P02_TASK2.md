# Handoff — P02.2: per-node `bake` on `mesh` and `winding` (#57)

Unit P02.2 of [`plan/11_P02_mesh-node-params.md`](../11_P02_mesh-node-params.md) § T2, the
last unit of P02; session of 2026-10-15, branch `feat/cli-consolidation` (pushed to `origin`;
`main` untouched). The record is
[roadmap 21 #57](../../docs/roadmap/21-cli-consolidation/01-record.md#57-per-node-bake-on-the-mesh-nodes)
and its evidence page [21/04](../../docs/roadmap/21-cli-consolidation/04-mesh-node-bake.md),
which also holds the full measurement tables; this handoff repeats them (the plan asks for
both) and adds what the next unit and Giacomo need.

## What changed, and why

Commits: `3c7c288` (code, tests, parity case, docs, plan correction), then this handoff.

> **Read with § Resumed at the P02 gate.** The sections from here to *Measured* describe the
> first pass (`3c7c288`), before Giacomo's answers. Where they say a smooth op needs
> `band ≥ k` by hand, or count 75 parity cases, D-62 (`1b126db`) superseded them: the margin
> is now handed down automatically and the count is 76.

- **One bake rule, two callers.** `examples/field_graph.{h,cpp}` export `kMaxNodeBakeRes`
  (512), `NodeBake`, `checkNodeBake`, `nodeBakeRegion`, `bakeMeshNode`, `bakeWindingNode`.
  `buildFieldUnchecked` (`mesh`, `winding`) and `field_glsl.cpp` (`emitMesh`, `emitWinding`)
  each read `bake` / `band` through their own `findParam` (so the unread-key check sees them on
  both paths) and call the shared validator and bakers.
  - Region: the node's own `bounds()` padded by 10 % of their largest extent **+ `band`** when
    given (see *Where the plan was wrong*). `winding`'s bounds are already padded (10 % of the
    diagonal), so it is padded twice; harmless.
  - `mesh`: `MeshSource::bakeToGrid(region, N³, band)`, band default 3 bake cells (as
    `dualc_boolean` uses for hard ops; the library floors at 1.5 cells). `winding`: the
    generic full-lattice `bakeToGrid`; `band` on it is rejected as an unknown parameter (the
    unread-key rule) — the plan said "ignored"; rejecting is what the 2026-09-11 rule does to
    every key an op does not read.
  - Validation, located on the node and before the mesh is loaded: `bake` an integer in
    [2, 512] (`'mesh' bake must be an integer in [2, 512] (samples per axis; the grid holds
    N^3 floats)`); `band` only with `bake` (`'band' needs 'bake' …`), and > 0.
  - Ceiling 512: 512³ floats = 512 MiB per node. The GPU path uploads the node's own grid, so
    N > 256 needs `GL_MAX_3D_TEXTURE_SIZE` ≥ N (GL 3.3 guarantees only 256; desktop GPUs
    report ≥ 2048). Not exercised above 256 on a GPU — *Open* 3.
- **GPU:** a baked node's `GridField` goes straight to `emitBakedGrid`; `--grid-res` and the
  trace box do not re-bake it. The `nodeKey` memo keys on every param, so baked and unbaked
  forms of one mesh are two textures (tested). `dualc_field_view`'s compile line now reports
  each distinct `N^3 (cell …)` (it assumed one shared resolution); usage text for
  `--grid-res` says it does not apply to a baked node.
- **`dualc_glsl_parity`**: case `mesh-bake` — `mesh(path=cube.obj,bake=24)` over `[-1,1]³`
  (wider than the bake's own `[-0.6,0.6]³`, so the edge clamp is compared too), GPU vs the
  **built** CPU field at 2e-3. `parity_expected_cases` 74 → 75.
- **`dualc_field --list`**: `mesh (…, bake=N, band=3 cells)`, `winding (path, bake=N)`, and a
  two-line note (2..512, own box + 10 %, band ≥ k under a smooth op).
- **Tests** (`[bake]`): `test_field_graph.cpp` — *mesh and winding bake=N sample the node into
  its own grid* (GridField type, resolution, region; within one bake cell of the exact field
  where `|exact|` < 2 cells, same sign beyond; `band=0.5` lattice nodes hold the exact
  distance and the region grows by the band; winding region and sign); *a baked child under a
  smooth op needs band >= k* (default band: sign errors exist — pinned; `band=k`: no sign error
  on a 31³ sample over `[-2.9,3.1]³` away from the surface); *mesh bake round-trips through
  dumpJson + parseJson*; *… reject bad values with a located error* (bake 1, 513, 32.5, 0,
  −8; band without bake; band 0, −1; band on winding). `test_field_glsl.cpp` — *GLSL codegen
  uploads a baked node's own grid* (texture values == the built GridField bit for bit, at
  `gridRes` 48 and 129, for mesh, mesh+band, winding). No passing test was edited.
- **Docs:** 11/01 § Sources (rows + a *Baking a mesh node* paragraph: the speed/exactness
  trade with numbers, band ≥ the smooth op's `k`, the coarser auto-fit root, why
  `dualc_boolean`'s combined region is not reproduced, one recipe — run verbatim, 3,564
  faces); 11/README error list; 12/README `--grid-res` row; 12/03 `winding` paragraph; 14/04
  entry + 14/README row (ABI below); 21/01 #57 DONE + *Landed*, #58 NEXT; new child 21/04;
  21/README page list + table (D-60 row notes the evidence is in); `roadmap/README.md`
  § Current focus, Track 1 NEXT, principal-blocks row, milestone row for #57 (#33,
  2026-10-06, rolled off per D-43; record 17/14). No `src/` or `docs/design/` edit (the design
  pages describe the library's `bakeToGrid` / `GridField`, unchanged), so no
  `/docs-semantic-lint` obligation.
- **ABI: no version move** — the rule 14/04 applied twice (vocabulary moved; signatures,
  structs, codes did not); `bake` works for `id=` nodes on the C ABI path too.

## Where the plan was wrong (corrected in the plan file and 21/04)

T2 step 1 put the region at "the node's own AABB padded by 10 %" (the raw plan: "without the
sibling and without `k`"). Measured, that makes every smooth op with a `k` above the margin
wrong: outside its region a `GridField` clamps to the face value, which never grows, and
`smin` turns that constant into material. `smooth-union torus cube --k 1`, both children
`bake=64`: 28,160 faces against the exact 9,116, max deviation 1.56 on a 2.7-wide model.
`dualc_boolean` pads its region by `k` for exactly this reason. The fix keeps the plan's knob:
an explicit `band` also widens the region, so the documented rule *band ≥ the parent's `k`*
covers both the exact shell and the clamp margin; with `band=1` the same run gives 3,564
faces, max deviation 0.0085 (the tool: 3,468, 0.0082). Step 1 of the plan file now carries a
dated correction.

## Measured, and under what

Linux, GCC 15, Ninja, Release, `build/examples/`, run from `data/` (demo meshes as generated
after #55); Intel i7-920 (4 cores / 8 threads), 22 GB. Wall: median of 3, peak RSS: max of 3,
`/usr/bin/time -f "%e %M"`. Faces: `f` lines of the written OBJ. Deviation: `|valueAt|` of
the **exact** graph (no bake) at every output vertex, max / mean — a scratch evaluator
(`/tmp`, not committed) linked against `libdualc_examples_fieldgraph`. Both CLIs auto-fit the
octree root to the field's bounds + 5 %, and a baked field's bounds are its region, so baking
also coarsens cells at a given depth; the "… root box" rows pin `--bounds` to the exact run's
or the tool's root to separate the two effects (the tool-root rows reproduce the tool's face
count exactly, which validates the box arithmetic). In all three cases `dualc_boolean`
without `--bake` and the exact graph wrote byte-identical OBJ (`cmp`).

`difference sphere.obj cube.obj --depth 6`, bake 64:

| Run | Wall s | RSS MB | Faces | max dev | mean dev |
| --- | ---: | ---: | ---: | ---: | ---: |
| `dualc_boolean --bake 64` | 0.60 | 54 | 20,280 | 0.00655 | 0.000377 |
| graph `mesh(bake=64)` ×2 | 0.81 | 81 | 29,496 | 0.0052 | 0.000258 |
| … exact root box | 1.03 | 105 | 41,640 | 0.00123 | 0.000141 |
| … tool root box | 0.66 | 57 | 20,280 | 0.00713 | 0.000374 |
| exact graph (= `dualc_boolean` exact) | 2.19 | 136 | 41,736 | 0.00139 | 0.000167 |

`smooth-union torus.obj cube.obj --k 1 --depth 6`, bake 64 (k ≈ 0.37 × the 2.7 extent):

| Run | Wall s | RSS MB | Faces | max dev | mean dev |
| --- | ---: | ---: | ---: | ---: | ---: |
| `dualc_boolean --bake 64` | 0.73 | 29 | 3,468 | 0.00819 | 0.00195 |
| graph `mesh(bake=64)` ×2, default band | 1.42 | 370 | 28,160 | 1.56 | 0.67 |
| graph `mesh(bake=64,band=1)` ×2 | 0.59 | 29 | 3,564 | 0.00851 | 0.00186 |
| … exact root box | 0.76 | 78 | 9,116 | 0.00363 | 0.000647 |
| … tool root box | 0.61 | 29 | 3,468 | 0.00673 | 0.00188 |
| exact graph (= `dualc_boolean` exact) | 1.69 | 58 | 9,116 | 0.00456 | 0.000882 |

`difference opA.obj opB.obj --depth 7`, bake 128 (local only; 8,387 + 50 faces, ≈ 550 mm).
Page 03 recommends no depth / bake for this pair (the plan assumed it did): depth 7 is the
tool's default, 128 its recipe B8's bake.

| Run | Wall s | RSS MB | Faces | max dev | mean dev |
| --- | ---: | ---: | ---: | ---: | ---: |
| `dualc_boolean --bake 128` | 3.36 | 250 | 45,688 | 3.11 | 0.0331 |
| graph `mesh(bake=128)` ×2 | 6.16 | 496 | 108,660 | 1.71 | 0.0129 |
| … exact root box | 7.19 | 630 | 162,620 | 1.47 | 0.0138 |
| … tool root box | 4.87 | 248 | 45,688 | 3.11 | 0.0333 |
| exact graph (= `dualc_boolean` exact) | 25.79 | 1,988 | 162,612 | 1.32 | 0.00806 |

**Reading:** per-node `bake` keeps the speed advantage — on the large pair 6.2 s / 0.5 GB
against exact 25.8 s / 2.0 GB (4.2×), 3.6× even at the exact run's octree — and against the
tool's combined-region bake it loses **no** accuracy: at the tool's root box it writes the
tool's face count to the face and its deviation (3.11 / 0.0333 vs 3.11 / 0.0331), at 1.5× the
tool's wall (4.9 s vs 3.4 s). By default it is finer than the tool (its root box is the
operand's own): 2.4× the faces, 2.6× lower mean deviation, ~2× time and memory. The cost is
ergonomic: under a smooth op the user had to pass `band=k` by hand, or the result was wrong
— the trap D-62 then closed (§ Resumed).

Other numbers:

- **Parity:** `mesh-bake` maxErr 2.72e-4 (0/4096 over tol). Made to fail on purpose
  (`emitMesh`'s baked branch disabled, so the codegen re-bakes over the trace box at 96): maxErr
  5.85e-1, 3,168/4,096 over tol → FAIL 74/75. Reverted, rebuilt: 75/75.
- **GPU tier:** `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict
  --build-dir build-gl` → `parity 75/75 passed` (final tree; GL-only TUs rebuilt, 0 warnings).
- **Full gate:** `python3 scripts/check.py` → 32 checks, 30 passed, 1 SKIP (configure,
  `build/` configured), 0 failed; with the seven changed TUs touched: **7 TUs compiled,
  0 DualC-origin warnings, ctest 313 / 0 failed** (99 s; 308 + 5 new cases).
- **`dualc_field_view --snapshot`** on a graph with one baked (`bake=64,band=1`) and one
  unbaked mesh: `2 mesh texture(s) @ 64^3 (cell 0.0831746), 96^3 (cell 0.0676211)`; the
  baked `smooth-union(torus, cube, k=1)` snapshot renders the expected blended solid (looked at).
- **`docs_loss_audit.py --base e31382f --head 3c7c288 --scope docs/`** (empty scratch triage):
  on the pages this unit edited, the only residual rows that are not `0.000/1.000` scoring
  noise are intentional — the 21/README D-60 and #57 status cells, the snapshot's NEXT clause
  in `roadmap/README.md`, and #33's milestone row (rolled off per D-43; its record is 17/14).
  No docs file deleted.

## Resumed at the P02 gate — Giacomo's answers (2026-10-15)

The unit returned `blocked` with two questions; the answers, and what was done with each:

1. **D-60: "Keep it."** `dualc_boolean` stays as the convenience CLI for "two meshes, one op".
   Recorded: D-60's row left the open index (`decisions/README.md`) for
   `decisions/01-settled.md` as DONE (kept), with the deferral's date and trigger in its status
   cell; 21/01 § The dualc_boolean decision rewritten as DONE — decided 2026-10-15 (the
   owner's words of both days kept); 21/README's row; `roadmap/README.md` § Current focus, the
   principal-blocks row ("seven side CLIs", "D-60 DONE (kept)") and the #57 milestone row.
   The plans: P05 T1 step 3 and P06's header now say *Decided 2026-10-15: kept (D-60)* — page
   03 stays and gains a pointer to `bake=` in P05; P06 retires seven tools. Nothing in this
   unit touched `dualc_boolean` itself.
2. **The trap: "(b) Propagate k."** Implemented as part of #57 and recorded as **D-62**
   (settled, 21/04 § The margin (decision (b))). `childBakeMargin(parent, i, margin)` in
   `field_graph.{h,cpp}` hands each child a margin; `buildField` takes it as a parameter
   (`child(i)` in the dispatcher), `emitNode` tracks it on a `Ctx::path` stack (so no emit
   call site changed). A baked mesh pads its region by max(`band`, margin) and keeps its band
   ≥ margin + 3 cells; an explicit larger `band` wins; `winding` pads by the margin. Two
   choices beyond the literal answer, both recorded in 21/04 and D-62:
   - **max of the smooth `k`s above, not only the nearest:** an outer blend reads the inner
     blend's values, which carry the clamp too (test case: inner `k`=0.1, outer `k`=1).
   - **`offset` / `round` / `onion` / `displace` / the graded ops' base add their amount:**
     the same clamp, and worse — measured before the change, `offset(mesh(cube,bake=32),
     r=0.3)` contoured to **1 face** and `onion(…,thickness=0.3)` to 588, against the exact
     9,804 / 10,392 (`--depth 5`), because 0.3 exceeds the 10 % pad. If Giacomo wants the
     literal "smooth `k` only", it is three lines in `childBakeMargin`.
   - The GLSL dedup memo keys a subtree on its margin **only when it holds a baked node**,
     so the existing "duplicated unbaked mesh bakes once" contract holds (tested both ways).

Evidence of the resumed work (commit `1b126db`, then 21/04's preservation line and this
handoff):

- **Tests:** *childBakeMargin hands each child the distance its ancestors read* (the rule, op
  by op); *a baked child inherits its ancestors' margin* — sign agrees with the exact field
  off the surface (|exact| ≥ 0.25 on a 31³ grid over [-2.9,3.1]³) for: direct smooth child,
  through `translate`, through `scale(0.6)`, nested smooth with the larger `k` outside,
  `offset` 0.3, `onion` 0.3; with `childBakeMargin` forced to 0 the first case fails; the bare
  library composition (`smoothUnionOf(bakeMeshNode(…, 0), …)`) still shows the trap;
  *an explicit band larger than the margin wins* (regions 1 + 0.2 + 2 vs 1 + 0.2 + 1, winding
  pad); *GLSL codegen uploads a baked node's own grid* gained six nested graphs (texture ==
  `bakeMeshNode(src, nb, expected)` bit for bit: margins 0, 0.7, 0.5, 1.0, 0.5, 0.4) and the
  memo split. **Replaced, not weakened:** my own test *a baked child under a smooth op needs
  band >= k* (committed in `3c7c288`) asserted the trap in the graph, which decision (b)
  removes; its trap assertion now runs on the library composition, as P02.1 replaced the
  "gwn throws" assertion its own decision removed.
- **Parity:** new case `mesh-bake-smooth` — a baked cube under `translate` inside a
  `smooth-union(k=0.5)`, GPU vs the built CPU field: maxErr 5.70e-4; with the GLSL side forced
  to margin 0: FAIL, 8.66e-1, 3,752/4,096 over tol. `parity_expected_cases` 75 → **76**.
- **Measurements:** the smooth-union table re-run (same conditions as below): the graph with
  default band now 0.66 s / 29 MB / 3,564 faces / max 0.00851 / mean 0.00186 — the tool's
  0.65 s / 29 MB / 3,468 / 0.00819 / 0.00195; at the tool's root box 3,468 faces, 0.00673 /
  0.00188. The hard-op tables are unchanged by construction (no margin), and the graph's
  `difference` output is `cmp`-identical before and after. 21/04 keeps the *before* row.
- **Gate:** `python3 scripts/check.py` with the five changed TUs touched → 30 passed, 1 SKIP
  (configure), **7 TUs, 0 DualC-origin warnings, ctest 315 / 0 failed**; `cmake --build
  build-gl -j && python3 scripts/check.py --gpu --strict --build-dir build-gl` → **76/76**
  (0 warnings in the `build-gl` rebuild); `--docs --strict` green.
- **Loss audit** `--base 284920c --head 1b126db --scope docs/`: the residuals on the edited
  pages are the D-60 row's move (its text now in the settled row), 21/01's deferral section
  condensed (the trigger detail, "the face counts on difference sphere cube", lives in 21/04's
  tables; "until D-60 says retire, no unit removes …" is moot, kept), 11/01's `band ≥ k`
  guidance (superseded by D-62), and 21/04's first smooth-union run — whose explicit `band=1`
  row was the one fact not carried over; it is now a sentence in 21/04.

## What I got wrong on the way

- Implemented the region exactly as the plan said, measured the smooth-union, found it wrong
  (above), then fixed the rule — the plan's text, not the code, was the error.
- First error tests asserted pointer `/in/1`; shorthand pointers are character offsets
  (`23`). The test now compares against the parsed child's own pointer.
- A per-file loss-audit loop re-ran the whole audit per page and timed out; one run with
  `--write-dump` + grep replaced it.
- The gate's warnings scan reported SKIP on the incremental build; touching the changed TUs
  and re-running `--build` made it meaningful (7 TUs).
- Resumed work: twice took a raw pointer from a temporary `FieldPtr` / `make_shared` in a
  test (dangling; one read 0.0 for a bound) — now held in named locals.
- The first return ran `remote_run_check.py` before GitHub had registered the run ("no run
  for HEAD"); the next run on that commit was then cancelled by my own later push.

## Open — for Giacomo

1. Decision (b) went slightly beyond its words (max of the smooth `k`s, and offset-type ops
   add their amount) — see *Resumed* 2; say if the literal rule is wanted.
2. Bakes above 256 on the GPU are untested (GL 3.3's guaranteed 3D-texture size is 256).
3. Still open from P02.1: `mesh-gwn` cannot tell GWN from parity (closed cube); `types.h`'s
   parity comment; CI's `gpu` job fetches Eigen from GitLab cold (no `_deps` cache).

## Exit criteria of P02 — bullet by bullet

- *Both parameters shipped CPU + GPU with parity cases; `parity_expected_cases` matches.* —
  `sign="gwn"` (P02.1, `mesh-gwn`) and `bake` (this unit, `mesh-bake`, `mesh-bake-smooth`)
  on both paths; `scripts/check_data.json` says 76 and the harness runs 76/76.
- *`python3 scripts/check.py` and `--gpu --build-dir build-gl` green.* — both PASS on the
  final code tree `1b126db` (numbers in *Resumed*); later commits touch `docs/` and `plan/`
  only, and the docs tier was re-run on them.
- *`python3 plan/remote_run_check.py` exits 0.* — green on the first pass's code (`a906934`, run 38780112757; `284920c`, run 38782978749). For the resumed code, the run on `8211e7f` was still in progress when this unit had to return. This commit, which touches only `plan/`, supersedes that run with one on the final HEAD; the driver's `remote_run_check.py` on that run decides. Nothing in this unit touches `.github/workflows/`; the gate and the `--gpu` tier are green locally.

## What the next unit (P03.1, #58 — 2D profiles, CPU) needs to know

- #58 is NEXT in 21/README, 21/01 and `roadmap/README.md`; D-60 is decided (kept).
- **Byte room is nearly gone:** 21/01 at 15,132 of 15,360 — #58's landing must be one or two
  lines pointing at a new child `05-…md`; `roadmap/README.md` at 15,024.
- `parity_expected_cases` is **76**. Next free IDs: **#61**, **D-63** (D-62 = the margin).
- Pattern that worked for a node param read on both paths: a small `readX(n)` in each TU
  (own `findParam`), validation and maths shared through `field_graph.h`. A rule that depends
  on ancestors: a parameter on `buildField`, a `Ctx::path` stack in `emitNode`.
- A new decorator that reads its child's field further out than the child's surface (an
  offset-like amount) should get a line in `childBakeMargin`.

---

← Back to the [plan](../README.md).
