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
ergonomic: under a smooth op the user must pass `band=k` by hand, or the result is wrong.

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

## What I got wrong on the way

- Implemented the region exactly as the plan said, measured the smooth-union, found it wrong
  (above), then fixed the rule — the plan's text, not the code, was the error.
- First error tests asserted pointer `/in/1`; shorthand pointers are character offsets
  (`23`). The test now compares against the parsed child's own pointer.
- A per-file loss-audit loop re-ran the whole audit per page and timed out; one run with
  `--write-dump` + grep replaced it.
- The gate's warnings scan reported SKIP on the incremental build; touching the changed TUs
  and re-running `--build` made it meaningful (7 TUs).

## Open — for Giacomo

1. **D-60 — decide `dualc_boolean`'s retirement** (the approve gate after P02). The evidence
   is the tables above / 21/04: a graph with `mesh(…,bake=N)` (and `band=k` under smooth ops)
   covers `--bake N` at equal accuracy and comparable speed; the exact path is already
   byte-identical. What retiring would lose: the tool's one-flag ergonomics (combined region,
   `k` folded into the band automatically). **Please decide: retire, or keep.**
2. **The smooth-op trap** (only if retire, or regardless): today a baked child under a smooth
   op with the default band is silently wrong; it is documented and tested, not prevented.
   Options, none taken: (a) leave it documented (now); (b) the builder passes the enclosing
   smooth op's `k` down so a baked descendant widens its band/region itself — closes the trap
   and reproduces the tool's ergonomics, but needs scale-aware propagation through decorators
   (`scale`, `offset`) on both CPU and GLSL paths; (c) a located error when a smooth boolean's
   *direct* child is a baked mesh with `band < k` — cheap, misses nested cases; (d) library:
   `GridField` extrapolates outside its region (face value + distance to the box) — fixes all
   cases at the root, but changes library semantics (`dualc_raymarch`, `gridBoundsExceeded`,
   design 08) and needs `/docs-semantic-lint`.
3. Bakes above 256 on the GPU are untested (GL 3.3's guaranteed 3D-texture size is 256).
4. Still open from P02.1: `mesh-gwn` cannot tell GWN from parity (closed cube); `types.h`'s
   parity comment; CI's `gpu` job fetches Eigen from GitLab cold (no `_deps` cache).

## Exit criteria of P02 — bullet by bullet

- *Both parameters shipped CPU + GPU with parity cases; `parity_expected_cases` matches.* —
  `sign="gwn"` (P02.1, `mesh-gwn`) and `bake` (this unit, `mesh-bake`) on both paths;
  `scripts/check_data.json` says 75 and the harness runs 75/75.
- *`python3 scripts/check.py` and `--gpu --build-dir build-gl` green.* — both PASS on the
  final code tree (numbers above); this handoff commit changes `plan/` only.
- *`python3 plan/remote_run_check.py` exits 0.* — **green on `a906934`**: run
  [38780112757](https://github.com/Giacogiak/DualC/actions/runs/38780112757), conclusion
  `success` (all jobs, `gpu` included). The earlier run on `3c7c288` (38779843837) ended
  `cancelled` (every job), superseded by the `a906934` push, not a failure. This line's own
  commit re-triggers the run; its result on the final HEAD is the driver's to read.

## What the next unit (P03.1, #58 — 2D profiles, CPU) needs to know

- #58 is NEXT in 21/README, 21/01 and `roadmap/README.md`; D-60 waits at this gate.
- **Byte room is nearly gone:** 21/01 at 15,207 of 15,360 — #58's landing must be one or two
  lines pointing at a new child `05-…md`; `roadmap/README.md` at 14,991.
- `parity_expected_cases` is **75**. Next free IDs unchanged: **#61**, **D-62**.
- Pattern that worked for a node param read on both paths: a small `readX(n)` in each TU
  (own `findParam`), validation and maths shared through `field_graph.h`.

---

← Back to the [plan](../README.md).
