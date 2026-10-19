# DualC porous shell plan — a natural sponge from a cell complex, in the graph, the viewer and the C ABI

**Drafted 2026-10-19**, from the owner's request and their answers in the planning session. An
input, not a record: status lives in the roadmap block the plan opens (P01 T1), never here. Each
task is **one fresh session** driven by the `plan-run` driver, opened and closed with
`/repo-docs-lifecycle`. The run is **unattended overnight**: no gate stops for a person, and the
heavy checks run at the end (§ 1, *Running*). A task that does not fit a session is closed
partial, with what was verified and what remains written into its record page.

## 0. The goal, in the owner's words

> This is an exploration to implement a new feature in DualC: Porous shell generator. The porous
> shell generator must be implemented in dualc_field and dualc_field_view, plus exposed in the C
> ABI and plan the implementation in Boletus. Just like TPMS, it takes primitives or meshes (any
> volume), and generates a porous shell in the whole volume, by filling it with a tetrahedral
> mesh. After that, using that structure just as the isosurface of the tpms, it should be
> possible to add a thickness to the shells (tetrahedra "isosurfaces") and also smooth the corners
> implicitly, hence creating an organic and natural sponge structure. There will be several
> hollow separated surfaces in it (can DualC handle that?). In terms of parameters and controls:
> for the tetgen inside the volume, imagine some different options, maybe extendable later, like
> blue noise, gaussian distribution, or find the more appropriate, it would be nice to get
> something that mimics nature. For the thickness, just replicate the same controls used for the
> graded thickness already in place. Dedicate a preliminary exploration phase to find the more
> appropriate and robust external tool for the tetrahedral generation if necessary, or for the
> distribution algorithm. Make up a plan that can run on batch for the night by handing each unit
> to a fresh runner. […] without stopping at a gate for human validation every time. Put all
> checks at the end if possible.

(Spelling normalised; the words are the owner's.)

**Several hollow separated surfaces — yes.** Manifold DC places one vertex per surface component
per cell, collapse refuses a merge that would join components
([design/04](../design/04-qef-manifold-collapse.md)), and the mesh bake resolves the far-field
sign per connected component (`src/implicit/mesh_source.cpp:210-225`). Only two things lose a
component: a `cellOverlaps` that under-reports ([design/07](../design/07-limitations.md)), or a
feature thinner than 2–3 octree cells ([design/10](../design/10-invariants-and-tolerances.md)).
Thickened closed cells therefore come out as many disjoint inner cavities, and P04 T2 proves it
with a component count.

## 1. The owner's decisions

Taken 2026-10-19; recorded as decision rows by P01 T1. The IDs are read from `docs/decisions/` and the
roadmap at phase time and never taken from this file: this plan runs **after** the hygiene +
editor plan ([2026-10-19-hygiene-and-editor-plan.md](2026-10-19-hygiene-and-editor-plan.md)),
which takes block 22, #62 and D-65 first.

| Topic | Decision |
| --- | --- |
| Cell structure | **Both**, as a parameter `cells=voronoi\|delaunay`, **`voronoi` the default**. Voronoi cells of relaxed blue-noise seeds are what soap foam, bone and sponge look like. Delaunay tet faces are the literal reading of the request. Both come from one Delaunay tetrahedralization: Voronoi is its dual. |
| Porosity | `mode=walls` (closed cells: every interior cell a sealed cavity) **and** `mode=struts` (open-cell foam along the cell edges, trabecular-bone-like). Cells cut by the volume boundary are open pores in either mode. Perforated walls are out of scope (§ 4), extensible later. |
| Thickness | **The existing graded controls, reused, not copied.** The new node is the "isosurface" source, an unsigned distance to the complex. Thickness comes from `onion`, `graded-onion` and `graded-offset` exactly as for a TPMS: `t1`, `t2`, `d1`, `d0`, `t = t1 + (t2 − t1) · clamp((control − d0) / (d1 − d0), 0, 1)`. |
| Corner smoothing | **Implicit**, in the node: a polynomial smooth-min of radius `smooth=k` over the nearby complex primitives. Junctions get fillets, and `k = 0` gives the sharp complex. |
| Distribution | Several, extensible: `poisson` (blue noise, the default), `jitter`, `random`, plus `relax=N` Lloyd/CVT iterations for the soap-foam look, and a graded density driven by a control field with a `linear\|smooth\|gaussian` ramp. A gaussian blob is the `gaussian` ramp over a sphere-distance control. |
| External tool | P01 is a dedicated exploration phase. It picks the Delaunay engine under the owner's licensing rule (MIT / BSD / public domain for code; LGPL / GPL / AGPL rejected, re-implement instead). The default it must beat is stated in § 3 P01. |
| Boletus | **A plan document only**, committed on a Boletus branch. Nothing is implemented in Boletus. |
| Git | Branch **`feat/porous-shell`**, in the worktree `~/Documents/DualC-porous`, cut from `feat/hygiene-and-editor` at `02f95a3`. Before the run, the owner rebases it onto the hygiene run's tip. **No session pushes or adds a remote**; every push is the owner's, by hand. Boletus: branch `feat/porous-shell-plan`, commit only, P07 only. |
| Running | **Unattended overnight.** Every gate is `auto`, and a session never stops on a question. Where this plan leaves a choice open, the session takes the default stated here, writes `**Chosen in-run (pending owner review):** …` into its handoff and the record page, and goes on. |
| Checks | **At the end.** Units P01–P07 are verified by the driver only for `tree-clean`, `new-commit`, `handoff`, plus a cheap `smoke` check: an incremental build and the porous / field-graph / GLSL ctests. The full `scripts/check.py`, the GPU tier, `--docs --strict` and `/docs-semantic-lint` run in P08, which also repairs what they find. For this run this replaces "the full gate before every code commit". Each session still builds and runs the tests it writes, because that is part of doing the work. |

## 2. Repo facts the plan relies on

Verified on the drafting day against `02f95a3` (`feat/hygiene-and-editor`, which holds `main` at
`5ce5b1c`). The hygiene run will move some lines, so a session re-reads the file before it trusts
a line number.

- **The field graph has no node registry.** Each op is an if-chain written twice:
  - The builder: `buildFieldUnchecked`, `examples/field_graph.cpp` (~1004–1131).
  - The codegen: `emitNodeImpl`, `examples/field_glsl.cpp` (~1424–1675).
  - The shorthand parser `parseShorthand` (~298–528) and `dumpJson` (~577) cover both forms.
  - Each file has a `rejectUnreadParams`, so a key the op never reads is a located `GraphError`:
    a new node reads every key in **both** files.
  - The `--list` text is in `examples/dualc_field.cpp` (~150–175).
  - The shared vocabulary lives in `examples/example_common.{h,cpp}` (`tpmsKinds`,
    `strutCellSegments`, `makeStrutLattice`).
- **The `ImplicitField` contract** is in `include/dualc/implicit.h` (29–95):
  - `valueAt`, `gradientAt` (a unit outward direction; the hygiene run's P03 makes that the
    written contract and adds a true-gradient path), `bounds`, `cellOverlaps`, `edgeHit`,
    `closestSurfacePoint`.
  - Methods are const and thread-safe.
  - The default `cellOverlaps` (`src/implicit/implicit_field.cpp:88-114`) assumes a Lipschitz-1
    field. A non-metric source returns `true`, which refines the whole root box to `maxDepth`;
    that is the TPMS cost trap, so the new node must stay Lipschitz-1 to avoid it.
  - A new domain wrapper is added by hand to the property test in `tests/test_domain_ops.cpp`.
- **Filling a volume is `intersection(volume, lattice)`** (`IntersectionField`,
  `src/implicit/combinators.cpp:75-101`):
  - `bounds` is the box intersection.
  - `cellOverlaps` is `a || b`.
  - A mesh volume is `MeshSource` (BVH closest point plus a sign oracle,
    `src/implicit/mesh_source.cpp`), optionally baked (`bake=N`).
  - In the C ABI an in-memory mesh is `mesh(id=…)` through `*_with_meshes`.
- **The graded nodes** are `graded-onion` (`|base| − t`) and `graded-offset` (`base − t`), in
  `src/implicit/decorators.cpp` (118–210). Their parameters are documented in
  [`command_reference/11-dualc_field/03-graded-and-morph.md`](../command_reference/11-dualc_field/03-graded-and-morph.md).
  An open code gap: their `cellOverlaps` grows the cell by `max(t1, t2, 0)`, so a negative `t` is
  not covered (the #46 bug class). This plan does not fix it. P01 records it as a tracked item,
  or adds it to an existing one if the hygiene run has already taken it.
- **Strut lattices** are the nearest precedent for an explicit-primitive source:
  - Capsules, or round cones when tapered, unioned and wrapped in `repeated` (`example_common.cpp`
    ~332–369).
  - GLSL: one `sdCapsule` line per segment, folded into one cell (`field_glsl.cpp` `emitStrut`).
  - A porous complex has thousands of primitives and no period, so it cannot be emitted as code
    lines: it needs data textures.
- **GLSL codegen:**
  - One `float fN(vec3 p)` per node; parameters become uniforms named `u_n<idx>_<key>`.
  - Mesh and winding nodes become a `sampler3D` R32F grid through `emitBakedGrid`.
  - `nodeStepScale` / `nodeFeatureScale` steer the sphere trace; a step scale ≥ 0.999 takes the
    fast metric path.
  - GL-free tests are in `tests/test_field_glsl.cpp`.
  - The parity harness `examples/dualc_glsl_parity.cpp` compares `sceneSDF` with `valueAt` on a
    16³ lattice (analytic tier 2e-3, finite-difference tier 3e-2).
  - `parity_expected_cases` is **82** in `scripts/check_data.json`. Every new case bumps it in the
    same commit.
- **The GL build tree** is `build-gl/` (`DUALC_BUILD_FIELD_VIEW`, `_GLSL_PARITY`). The GPU tier is
  `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict --build-dir build-gl`, and
  a stale tree reports false failures. A git worktree has no build trees, so P01 configures
  `build/` and `build-gl/` in `~/Documents/DualC-porous` first, and every session afterwards
  reuses them.
- **The C ABI** (`capi/dualc_c.h`, version 0.5.1, single source `project(… VERSION …)` in
  `CMakeLists.txt`):
  - The graph string *is* the construction API, so a new node reaches Boletus with **no new
    symbol**.
  - Vocabulary widenings are logged in
    [`roadmap/14-c-abi/04-abi-0-4-0.md`](../roadmap/14-c-abi/04-abi-0-4-0.md) ("vocabulary
    widening, no ABI version move" entries).
  - ABI tests are ctests driving `capi/dualc_c_demo.c` (`cli_c_abi*`, `capi/CMakeLists.txt`).
  - Cancellation is a host-owned `DualcCancelToken` that the engine polls; a build-time seed
    generator must poll it too.
- **Vendoring** follows `THIRD_PARTY.md`. Vendored sources live in `src/internal/` (the QEF/SVD
  files under the Unlicense, nanort under MIT) and are compiled as SYSTEM, with original headers
  kept and any local change marked `LOCAL MODIFICATION (DualC)`. geometry-central's nanoflann is
  PRIVATE to it and cannot be used.
- **Boletus** (`~/Documents/Boletus`, a .NET Rhino/Grasshopper client of the C ABI):
  - DualC is a submodule at `external/DualC`, pinned by commit.
  - The op schema table is `src/Boletus.Core/FieldGraph/Ops.cs`.
  - The components are in `src/Boletus.Grasshopper/`: `TpmsComponent.cs`,
    `StrutLatticeComponent.cs`, `GradedOnionComponent.cs`, `GradedOffsetComponent.cs`.
  - Its docs layout mirrors DualC's: `docs/raw/`, `docs/decisions/` with D-NN up to D-48 on the
    drafting day, and `docs/roadmap/`.
  - It has no `plan/` convention of its own.
- **Determinism across the three CI OSes** needs an in-house RNG. The `std::*_distribution`
  outputs differ between libstdc++, libc++ and MSVC.

## 3. Phases

The dependency order runs: study and record → seeds → Delaunay and complex → CPU field and graph →
GPU → ABI and docs → Boletus plan → gates and close. Every unit leaves the tree building and its
own tests green, because the `smoke` check holds it to that.

### The node, as designed (the defaults every unit follows unless P01 changes them)

```
porous(volume[, control],
       cells=voronoi|delaunay        (voronoi)
       mode=walls|struts             (walls)
       dist=poisson|jitter|random    (poisson)
       spacing=S                     (required, > 0: the mean seed distance where control ≤ d0)
       spacing2=S2                   (= spacing: the seed distance where control ≥ d1)
       d0=0, d1=…                    (d1 required when spacing2 ≠ spacing and a control is given)
       ramp=linear|smooth|gaussian   (linear)
       relax=N                       (0; Lloyd iterations, ceiling 50)
       seed=N                        (0)
       smooth=k                      (0; smooth-min radius)
       jitter=J                      (1.0; dist=jitter only, 0 = regular grid))
```

- **The token.** It is `porous`, unless P01 finds a clash.
- **The value.** The node evaluates to the **unsigned distance to the complex**:
  - `voronoi` + `walls`: the walls between Voronoi cells. This is analytic: the nearest seed
    `s1`, then `min` over the neighbours `j` of the bisector-plane distance
    `((p−s1)·(sj−s1) − |sj−s1|²/2) / |sj−s1|`, made positive.
  - `delaunay` + `walls`: the triangle faces of the tets.
  - `struts`: the edges, meaning Voronoi edges (segments between the circumcentres of adjacent
    tets) or Delaunay tet edges.
  - Smoothing is a polynomial `smin_k` over the primitives within `k` of the minimum. Its gradient
    `h∇a + (1−h)∇b` keeps the field 1-Lipschitz, so the default `cellOverlaps` holds and the
    step scale stays 1.0.
- **What a contour of the bare node gives.** An unsigned field has no inside, so a contour of the
  node alone is empty or degenerate, as with an open TPMS. The docs say it is a source to give a
  thickness to.
- **The volume.** It is evaluated **only at build time**, to place the seeds; `bounds()` is the
  volume's box. Seeds are also placed in a pad one `spacing2`-or-`spacing` thick outside the
  volume (where the volume's value is below the pad), so the boundary cells are well formed. The
  `intersection` with the volume clips them.
- **Recipes the docs carry, ready to run:**
  - Sponge:
    `intersection(V, graded-onion(porous(V, spacing=…), control, t1=…, t2=…, d1=…))`.
  - Uniform foam: `intersection(V, onion(porous(V, …), thickness=t))`.
  - Skinned part: `union(intersection(V, onion(porous(V, …), thickness=t)), onion(V, thickness=ts))`.
  - Open-cell bone: `mode=struts` + `offset(…, r)` or `graded-offset`.
- **Data structure.** One unconstrained Delaunay of the seeds (pad included) yields every complex.
  The explicit primitives (triangles, segments) and the seeds go in a **uniform bin grid** whose
  bin is at least the largest cell radius. A query searches 3×3×3 bins, and beyond that the value
  is clamped to a conservative bound. The **same packed arrays** feed the CPU and the GPU, which
  is what parity relies on.
- **Ceilings.** A clear `GraphError` on more than 2,000,000 seeds, or on a `relax` above 50. P02
  may lower the ceilings after measuring, recording why.

### P01 — Explore, open the record, configure the worktree

- **T1. Study + record.** The study is written to the new block's `01-study.md`, with prototypes
  kept in scratch and never committed. It covers:
  - (a) **The Delaunay engine.** Compare at least:
    - geogram's **Delaunay PSM** (the single-file "pluggable software module", BSD-3, exact
      predicates with symbolic perturbation): **the default to beat**.
    - Our own Bowyer–Watson on Shewchuk's public-domain predicates: **the fallback**.
    - TetGen, CGAL, Qhull, fTetWild: named with the reason each is rejected or not needed.
      TetGen is AGPL and CGAL is GPL/LGPL. Qhull's licence is custom and it is heavy. fTetWild is
      a constrained mesher, and the boundary is clipped by `intersection`, so a point Delaunay is
      enough.

    Criteria: licence; vendored size; builds under GCC 15 here, plus MSVC and Clang by reading
    for portability hazards; **deterministic output for a fixed input order** (sequential, no
    multithreaded insertion); robustness on a regular grid (cospherical points, `jitter=0`); time
    on 100k random points measured in scratch.
  - (b) **Distributions.** Bridson Poisson-disk with variable radius; jittered grid; white noise;
    Lloyd/CVT by Monte-Carlo sampling (cost per iteration); a gaussian density. Which one looks
    "natural", and why.
  - (c) The Voronoi-edge and Delaunay-face distance with smooth-min, and the bin-grid bound.
  - (d) The GPU layout: RGBA32F 2D data textures read with `texelFetch` (within the WebGL2
    subset), size limits, the fallback to `emitBakedGrid`.

  Then open the record:
  - The next free roadmap block, read from `docs/roadmap/` as a folder (`README.md` index +
    `01-study.md`), titled *Porous shell*.
  - Tracked items read from the files: seeds, Delaunay + complex, CPU node, GPU node, ABI + docs,
    Boletus plan; plus the graded-`cellOverlaps` gap of § 2 unless it is already tracked.
  - Decision rows for § 1, and for the engine choice marked
    `**Chosen in-run (pending owner review)**`.
  - `roadmap/README.md` § Current focus / § Next up / the blocks table, within its size cap.

  The study ends in a **defaults table** that binds every later unit. The node design above
  stands unless the table changes it, with the reason. Finally, configure `build/` (Release,
  TESTS + EXAMPLES + C ABI as the gate does) and `build-gl/` in the worktree and build both once.

### P02 — Seeds

- **T1.**
  - `include/dualc/porous.h` + `src/implicit/porous_seeds.cpp` hold a PCG32 with its own `[0,1)`
    conversion.
  - `poisson` (Bridson, variable radius from the control and the ramp), `jitter`, `random`.
  - The pad outside the volume; Lloyd relaxation, sample-based, with a fixed sample count per
    seed; the ceilings; cancel-token polling through the existing progress / cancel plumbing
    (`include/dualc/progress.h`).
  - `tests/test_porous.cpp`:
    - Same seed → identical seed array.
    - Poisson minimum distance ≥ `0.99·r` locally.
    - The graded density ratio follows `(spacing/spacing2)³` within 20 %.
    - Every seed is inside the volume or in the pad.
    - `relax` lowers the variance of the cell volumes.
    - Cancel stops the build.
  - Measured: seeds per second on a mesh volume (`data/bunny.obj` or a demo mesh) at three
    spacings.

### P03 — Delaunay and the cell complex

- **T1. Vendor + tetrahedralize.** Vendor P01's choice under `src/internal/` and record it in
  `THIRD_PARTY.md` (origin, licence, files, any local modification), compiled as SYSTEM. If the
  fallback was chosen, write it there under DualC's licence instead. Tests:
  - The tet count is consistent with Euler's formula for the convex hull.
  - The empty-circumsphere property holds on a random subset.
  - A regular grid (`jitter=0`) does not crash or produce degenerate tets.
  - Same input → identical tet list.
- **T2. Complex + bins.**
  - Derive the Delaunay faces and edges, the Voronoi vertices (circumcentres; tets with a
    circumcentre far outside the padded box are clamped and recorded) and the Voronoi edges.
  - Pack the primitives for both modes and build the bin grid, with the packed layout P05 uploads.
  - Tests: the dual adjacency is consistent (every Voronoi edge joins two adjacent tets); the bin
    query equals a brute-force minimum on 10k random points, for all four `cells`×`mode` cases.

### P04 — The field and the graph node

- **T1. `PorousField` + `porous`.**
  - The field class: value, gradient (analytic where it is cheap, central difference otherwise,
    under the hygiene run's gradient contract), `bounds`, the default `cellOverlaps` (justified by
    the Lipschitz argument in a code comment).
  - The graph token in `field_graph.cpp`: build, shorthand, `dumpJson` round trip,
    `rejectUnreadParams`, the `--list` group. The control child is optional.
  - Tests in `test_field_graph.cpp` and `test_porous.cpp`:
    - Lipschitz-1 sampled on random pairs, with `smooth` 0 and > 0.
    - `smooth` only lowers the value, and by at most `k/4`, the quadratic smin bound.
    - Unknown keys are rejected.
  - Add the node to `childBakeMargin` only if it reads a child below the zero level (it should not:
    the volume is build-time only).
- **T2. Contour end to end.** The § 3 recipes on a primitive (sphere) and a mesh (a demo mesh),
  both `cells`, both `mode`s, through `dualc_field -o`. A test asserts the output is watertight and
  has **more than one connected component** in `walls` mode (a component counter written in the
  test, not in the library). Commit two or three samples to `examples/samples/`. Record per case:
  seeds, tets, faces, components, time, and the depth used.

### P05 — `dualc_field_view` (GLSL)

- **T1. Codegen.**
  - Pack the CPU arrays into data textures (a texture kind beside the existing mesh `sampler3D`).
  - The emitter for the four cases: the bin search, the bisector or primitive distance, `smin`.
  - `smooth` as a uniform.
  - `nodeStepScale` 1.0 and `nodeFeatureScale` = `min(spacing, spacing2)`.
  - `test_field_glsl.cpp` cases (GL-free).
  - If the exact path does not fit the unit, route the node through `emitBakedGrid` and record it
    as chosen in-run, with the measured parity error.
- **T2. Parity + viewer.**
  - Parity cases, at least `porous-voronoi-walls`, `porous-delaunay-walls`, `porous-struts` and
    `porous-smooth`, with `parity_expected_cases` bumped.
  - In `dualc_field_view`, a structural key (`spacing`, `spacing2`, `seed`, `dist`, `relax`,
    `cells`, `mode`, the control) rebuilds the scene; `smooth` and the thickness nudge as uniforms.
  - Run the GPU tier yourself.
  - Snapshot PNGs of the sponge recipe (closed + struts) are written to the block's folder only if
    they fit the size rules, else under `build/` with their paths recorded for the owner.

### P06 — C ABI and the documentation

- **T1.**
  - **ABI:** the ctest `cli_c_abi_porous` drives `dualc_c_demo` with an in-memory mesh volume
    (`porous(mesh(id=…))`) and the expression form, then checks faces > 0 and watertightness; a
    cancel case if `dualc_c_demo` supports one. A "vocabulary widening, no ABI version move"
    entry goes in 14/04, applying block 14's version rule as written (if the rule says otherwise,
    follow it and record it). `capi/README.md` is updated if it lists vocabulary.
  - **CLI docs:**
    - A new `command_reference/11-dualc_field/` child page: the parameter table with the real
      defaults (the gate compares it with the parser), the four recipes ready to run, the
      cost/size guide (seeds ↔ spacing ↔ depth).
    - The `11` README and `01-op-vocabulary.md` rows.
    - The `12-dualc_field_view` nodes page.
  - **Design docs:** a section in `design/12-field-graph.md` or a new design page (whichever the
    size cap allows) covering the node's invariants: Lipschitz-1, build-time volume, determinism,
    the bin bound.
  - `STRUCTURE.md` for every new file.

### P07 — The Boletus implementation plan

- **T1.** In `~/Documents/Boletus`, branch `feat/porous-shell-plan` cut from its current branch's
  tip, write **commit only**:
  - `docs/raw/<date>-porous-shell-plan.md` and its row in `docs/raw/README.md`, in Boletus's own
    raw-plan style. Its content:
    - The DualC commit to re-vendor (the submodule pin).
    - The `Ops.cs` schema entry, every key with its default.
    - A *Porous Shell* GH component: inputs Volume, Cells, Mode, Distribution, Spacing,
      Spacing2, Control, d0/d1, Ramp, Relax, Seed, Smooth; output the field. It composes with
      the existing graded-onion / onion / intersection components; the sponge recipe goes in as a
      GH example.
    - The preview through `dualc_field_view` with the structural-key rebuild.
    - `CliParityTests` and serializer tests.
    - The D-NN rows it would open (read the next free one from Boletus's decisions).
  - Read Boletus's `AGENTS.md` / `CLAUDE.md` first and follow its gate for docs (its `scripts/`
    docs check if it has one).
  - In DualC, one line in [15](../roadmap/15-boletus-handoff.md) § DualC retains (or the block's
    README) points to it.

### P08 — Gates, repair, close

- **T1. All gates, then repair.**
  - `cmake --build build-gl -j`, then `python3 scripts/check.py`,
    `python3 scripts/check.py --gpu --strict --build-dir build-gl` and
    `python3 scripts/check.py --docs --strict`.
  - `/docs-semantic-lint` (both `src/` and `docs/design/` changed), its run file recorded where
    `check_data.json` says.
  - Fix everything that fails, commit by commit, and loop until all four are green. A failure that
    cannot be fixed in the unit is written into the record as open, with its output.
- **T2. Close.**
  - Harvest every handoff into the block's plan-run record page: the green-run table,
    measurements, plan-file corrections, lessons.
  - Item statuses with evidence; decision rows final; `roadmap/README.md` snapshot and milestone
    row.
  - **The owner's review list**, in the record and in the final handoff: every
    `Chosen in-run (pending owner review)` line, the snapshot paths, the cavity counts, the
    Boletus plan's path and branch, the compare link for the PR.
  - **Do not delete `plan/` in this unit.** The deletion is a separate commit after the driver
    marks P08.2 done (D-51), preceded by `scripts/docs_loss_audit.py --annex plan/`.

## 4. Out of scope

- Perforated walls (interconnected pores through closed cells).
- Anisotropic or oriented cells.
- Weaire–Phelan / Kelvin regular foams as presets.
- A constrained (boundary-conforming) tetrahedralization.
- Component counting or island removal in the library or the C ABI (it stays a test helper).
- A new ABI symbol or progress stage.
- The graded nodes' negative-`t` `cellOverlaps` fix (recorded, not done).
- Implementing anything in Boletus.
- Any push.
