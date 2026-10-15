# Handoff — P03.2: GLSL for the 2D profiles and `revolve` / `extrude` (#58 GPU half)

Unit P03.2 of [`plan/12_P03_2d-lifts.md`](../12_P03_2d-lifts.md) § T2, the last unit of P03;
session of 2026-10-15, branch `feat/cli-consolidation` (pushed to `origin`; `main` untouched).
The record is
[roadmap 21 #58](../../docs/roadmap/21-cli-consolidation/01-record.md#58-2d-profiles-and-revolve--extrude-in-the-graph),
now **DONE**, with the GPU evidence appended to
[21/05 § The GPU half](../../docs/roadmap/21-cli-consolidation/05-2d-profiles-and-lifts.md#the-gpu-half-p03-t2);
the usage contract is [11/09](../../docs/command_reference/11-dualc_field/09-2d-profiles-and-lifts.md)
and [12/03 N20–N21](../../docs/command_reference/12-dualc_field_view/03-recipes-by-node.md#2d-profiles-under-the-lifts).

## What changed, and why

Commits: `9405702` (code, tests, parity, docs), then this handoff.

- **One profile reader for both paths** (the previous handoff's "better" option). The CPU
  reader moved from a file-local `buildProfileUnchecked` into the public
  `readProfile(profile, lift)` in `examples/field_graph.{h,cpp}`: typing check, its own
  `ReadScope` + unread-key check, defaults / named keys / flat-from-the-front, all value
  checks; it returns the flat values. Beside it `liftParamKey` (`offset` / `height`),
  `checkLiftParam(lift, const double*)` (the `checkNodeBake` shape: each TU reads the key
  itself so its own unread-key check sees it) and `throwProfileOutsideLift`. `buildProfile`
  now only constructs the library field from `readProfile`'s values. The CPU output is
  unchanged (same SHA-256 as P03.1, below).
- **`field_glsl.cpp`**: prelude helpers `sd2Circle`, `sd2Rect`, `sd2Segment` (ports of
  `src/implicit/field2d.cpp`), `dcRevolveQ` (`vec2(length(p.xz) - o, p.y)`) and `dcExtrude`
  (`min(max(d,w),0) + length(max(vec2(d,w),0))`, `src/implicit/lift.cpp`). A profile emits a
  `float fN(vec2 p)`, memoized under a `"2d|"` key so one profile under two lifts emits
  once; a lift emits `fN(vec3 p)` calling it. Dispatch sits where the builder's does (after
  the mesh sources); a bare profile throws the builder's error.
- **The polygon**: a function-local `const vec2 v[N] = vec2[N](…)` (literals at 9
  significant digits) and `for (int i = 0; i < N; ++i)` — `Polygon2D::valueAt` line for line,
  crossing-count sign included. Ceiling `kMaxGlslPolygonVertices = 64` in `field_glsl.h`
  (documented there, in 11/09, 11/01, 12/02, 12/03); above it a located `GraphError`:
  *'polygon' has 65 vertices; the GLSL preview takes at most 64 (the export path has no
  ceiling)*. Vertices are structural (an edit recompiles); 64 was chosen as a size where one
  profile's loop costs less than a TPMS evaluation — a judgment, not a measurement.
- **Bindings**: scalars are `float` uniforms — the profiles' `radius`, `revolve`'s `offset`,
  `extrude`'s `height` — so `dualc_field_view`'s `tab` + `↓`/`↑` (which acts on any `float`
  binding) nudges them. 2D points (`center`, `half`, `a`, `b`) are a **new binding type
  `vec2`**; `setBinding` in `dualc_field_view.cpp` and the uniform loop in
  `dualc_glsl_parity.cpp` gained the `glUniform2f` branch.
- **Parity** (`examples/dualc_glsl_parity.cpp`): six cases — `extrude-circle`,
  `extrude-rect`, `extrude-segment`, `extrude-polygon` (a concave L), `revolve-circle`,
  `revolve-polygon` (concave, notched) — at the analytic tier; `parity_expected_cases`
  76 → **82** in `scripts/check_data.json`.
- **Tests** (`tests/test_field_glsl.cpp`, tag `[field_glsl][lift]`, 3 new cases, no existing
  test edited): emission shape for each profile under each lift (helper call, lift wrapper,
  `nodeCount` 2, `stepScale` 1); binding types, values and node pointers; profile dedup
  under two lifts; 11 bad graphs fail with the **same pointer and message** on the GPU path
  as `FieldGraph::build`; 64 vertices compile, 65 are refused on the polygon while the CPU
  builds it.
- **Docs**: 11/09's "Not in the live viewer yet" note replaced by what the viewer does (and
  the ceiling); 11/01's "Not in `dualc_field_view` yet." replaced; 12/02 Supported nodes;
  12/03 new section + N20–N21 (both run, snapshots looked at); 12/README row N1–N21; 21/01
  #58 DONE (replaced, not grown past the cap: 15,276 / 15,360); 21/README row + page line;
  21/05 § The GPU half (evidence); `roadmap/README.md` § Current focus, Track 1 NEXT (#59,
  P04), principal-blocks row, milestone row for #58 (the 2026-10-07 #32 row left the
  five-row window, D-43; it lives in 17/15); D-11's row in `decisions/README.md` notes the
  landing. No `src/` or `docs/design/` edit → no `/docs-semantic-lint` obligation.

## Measured, and under what

Linux, GCC 15, Ninja, Release; GL renderer NV92 (this host's GPU through its driver).

- **Parity** `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict
  --build-dir build-gl` → **82/82**. The six new cases, 16³ samples over `[-2,2]³`, tolerance
  `A = R = 2e-3`: max error 3.51e-7 (extrude-circle), 3.09e-7 (rect), 4.86e-7 (segment),
  3.20e-7 (polygon), 4.60e-7 (revolve-circle), 3.75e-7 (revolve-polygon), 0/4096 over.
- **Full gate** `python3 scripts/check.py`, changed sources touched first: 32 checks, 30
  passed, 1 SKIP (configure), 0 failed; **5 TUs compiled, 0 DualC-origin warnings; ctest
  325, 0 failed** (85.8 s; 322 + 3 new cases).
- **`--docs --strict`** green (28 checks).
- **Equivalence, re-run after the reader refactor** (`build/examples/`, default depth 7,
  `cmp`): `dualc_lift revolve circle 0 0 0.5 --offset 2` ≡
  `dualc_field --expr "revolve(circle(0,0,0.5),offset=2)"`, SHA-256 `3a0d846f…0aa336bee`;
  `dualc_lift extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` ≡
  `extrude(polygon(points=[[0,0],[1,0],[1,1],[0,1]]),height=0.5)`, `a46c983c…80330d80d7` —
  both byte-identical, both hashes equal to P03.1's.
- **Snapshots looked at** (`dualc_field_view --snapshot`, 800×600, under `/tmp/p032/`, not
  committed):
  - `revolve.png` — `revolve(polygon(points=[[0,-0.6],[0.6,-0.6],[0.6,0.6],[0.3,0],[0,0.6]]),offset=0.5)`
    (N20): a ring with a conical groove in its top, as the profile says.
  - `extrude.png` — the concave L (`extrude-polygon`'s graph): the L prism, clean edges.
  - `n21.png` — N21: the L with a ball carved out of a corner.
  - `revolve-es.png` vs `torus-es.png` — `revolve(circle(radius=0.4),offset=1.2)` and
    `torus(major=1.2,minor=0.4)`: equal to 1/255 per channel; `revolve-poly-es.png` (`--es`)
    pixel-identical to the desktop-GL `revolve.png`.
  - **Dark specks:** ~128 dark / purple pixels where a silhouette grazes (the torus's inner
    rim; 232 on the grooved ring's creases). The `torus` primitive shows the identical 128,
    so they are the sphere-trace's on this renderer, not the lifts'. Recorded in 21/05; not
    investigated further.
- **CI**: see § Remote run below.
- **Loss audit** (`docs_loss_audit.py --base 0446142 --head 9405702 --scope docs/`, scratch
  triage): see § Loss audit below.

## What I got wrong on the way

- Wrote the first error-location tests against JSON pointers (`/root/in/0`); a shorthand
  graph's pointers are character offsets. Rewrote them to compare the GPU path's
  pointer + message with `FieldGraph::build`'s for the same graph — a stronger test anyway.
- A first draft of the ceiling test carried a meaningless `substr` comparison; removed
  before the gate.
- The shell loop that took the N20/N21 snapshots wrote both to the same file (a counter in a
  subshell); N21 was re-rendered on its own and looked at.

## Open — for Giacomo

1. **`vec2` is a new binding type.** `dualc_field_view` and the parity harness upload it;
   **DualCEditor's `src/trace_renderer.cpp:289`** uploads only `float` / `vec3` / `mat4`, so
   once it re-pins DualC a lifted profile there draws with its 2D points at zero (a `rect`
   with zero half extents vanishes). The fix is one `glUniform2f` line there; its parameter
   panel sizes editors by `value.size()` and should cope. The alternative — emitting 2D
   points as `vec3` with z = 0 — keeps every host working unchanged at the cost of a
   misleading type; I chose `vec2`. Say if you want the other.
2. The polygon ceiling (64) is a judgment; raise `kMaxGlslPolygonVertices` if an author
   needs more (the loop is bounded by the array, not by a uniform).
3. Still open from P03.1: nested arrays lenient off `points`; `revolve offset=1` /
   `extrude height=0.5` defaults kept, `polygon` without a default. From P02: decision (b)'s
   breadth, GPU bakes above 256, `mesh-gwn`'s discrimination, CI's cold Eigen fetch.
4. The dark silhouette specks above (the viewer's, not #58's) — no tracked item opened.

## Exit criteria of P03, one by one

- *2D profiles and lifts in the graph, CPU and GPU* — CPU: P03.1's `[lift]` cases in
  `test_field_graph.cpp`; GPU: the three new `[field_glsl][lift]` cases; ctest 325/0.
- *parity green* — 82/82 (`check.py --gpu --strict --build-dir build-gl`, above).
- *`dualc_lift`'s two recipes reproduced byte-identically* — both re-run after this unit's
  refactor, `cmp` identical, hashes above; the revolve one is also the ctest
  `cli_field_lift_same`.
- *`python3 plan/remote_run_check.py` exits 0* — § Remote run.

## Remote run

`python3 plan/remote_run_check.py` on the code commit `9405702`: run
[38806451193](https://github.com/Giacogiak/DualC/actions/runs/38806451193) — completed,
**success**; the script printed `OK branch feat/cli-consolidation HEAD 9405702 green` and exited
0. This handoff's own commit (docs only) is pushed after it; the driver's remote-run check
reads that HEAD's run.

## Loss audit

`python3 scripts/docs_loss_audit.py --base 0446142 --head 9405702 --scope docs/` with a
scratch triage (`/tmp`, an empty rule set for audit key `0446142`; the tool refuses a scope
narrower than `docs/`). It reports 9,004 untriaged rows tree-wide: without rules it flags
raw-only and unchanged-context rows in files this commit never touched (e.g. 19/README).
Filtered to the ten docs files the commit changed, **7 rows are gone / partial, all
intended**: 11/09's "Not in the live viewer yet" note (two rows — superseded; 21/05 keeps the
fact that the viewer refused lifts until T2); 21/01's "CPU half landed" line (reworded into
the DONE landing line); the #58 status cells in 21/README and `roadmap/README.md` § Current
focus / Track 1 (PARTIAL / NEXT → DONE / #59); and the 2026-10-07 **#32** milestone row, which
left the rolling five-row window (D-43) and lives in
[17/15](../../docs/roadmap/17-code-audit-and-hardening/15-test-coverage-batches.md). No docs
file was deleted. The rest of the changed-file rows are "rewritten" table rows the tool
re-parses with 0.000/1.000 scores (unchanged text).

## What the next unit (P04, #59 slice any graph) needs to know

- `parity_expected_cases` is **82**. Next free IDs: **#61**, **D-63**.
- Byte room: 21/01 at 15,276 of 15,360 — #59's landing line must replace text, or move
  detail to a new evidence page (21/06); `roadmap/README.md` at 15,042.
- `field_graph.h` now exports `readProfile` / `checkLiftParam` / `liftParamKey` /
  `throwProfileOutsideLift` — a slice of a lifted graph needs nothing new: it evaluates the
  built `FieldPtr`.
- P06 (retire `dualc_lift`) must still replace `cli_field_lift_same`'s comparison with a
  fixed expectation — it reads `cli_lift.obj`.

---

← Back to the [plan](../README.md).
