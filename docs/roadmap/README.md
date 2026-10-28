# DualC — Roadmap & development record

The development record, one numbered block per topic; each block's page keeps the
original entries verbatim, dated. For *current usage* of the CLIs see
[../command_reference/](../command_reference/README.md); for the engine as it is,
[../design/](../design/README.md); for every open or rejected decision with its trigger,
[../decisions/](../decisions/README.md).

**Status legend:** **DONE** · **NEXT** (the one item picked up next) · **PLANNED** ·
**DEFERRED** (with a trigger to revisit) · **DROPPED** (with the rationale).
**PARTIAL** qualifies a DONE whose remaining batches are listed in the topic file, on
the body line as `PARTIAL: DONE (dates); the rest PLANNED|DEFERRED`. `scripts/check.py`
rejects any other word in a Status cell and checks a cell against the entry it links.

This page is an **index and a current-state snapshot**, not a record: every claim
below is one line plus a link to the topic file that holds the evidence. Detail
belongs there, never here; a count or a version is stated only in its home.

## Current focus

**The porous shell run** ([23](23-porous-shell/README.md)) closed 2026-10-22, merged
the same day (PR #3; [23/10](23-porous-shell/10-plan-run-record/README.md)): a `porous` graph source — seeds in any
volume, one Delaunay, the distance to its complex — in `dualc_field`, `dualc_field_view` and the
C ABI, by the [porous shell plan](../raw/2026-10-19-porous-shell-plan.md); #63–#68 DONE; #70, the viewer's step on smooth booleans, DONE 2026-10-27 ([23/11](23-porous-shell/11-viewer-trace-cost.md)). Before it, **the hygiene + editor run**
([17/22](17-code-audit-and-hardening/22-hygiene-plan-run.md), merged 2026-10-22, PR #2): **#35** DONE,
**#34** PARTIAL (E12, the `GridField` normal: [17/20](17-code-audit-and-hardening/20-gridfield-normal.md)),
**#53** PARTIAL until the remote run, the standalone UI moved to DualCEditor
([22](22-dualceditor-handoff/README.md), [D-66](../decisions/01-settled.md)). The engine
track draws on the 2026-08-19 audit's ledger ([17](17-code-audit-and-hardening/README.md)); the
docs restructuring ([19](19-docs-layers/README.md)) leaves the monthly `/docs-semantic-lint` to
ordinary sessions; **#48** ([14/05](14-c-abi/05-progress-and-cancel.md)) left D-44 / D-45 open
and the re-vendor to Boletus ([15](15-boletus-handoff.md#dualc-retains--engine--abi-boletus-only-requests-these-upstream)).

**Recent milestones** — the last five: date, one clause, the record; older rows live in
the block records the table links ([D-43](../decisions/01-settled.md)).

| Date | What landed | Record |
| --- | --- | --- |
| 2026-10-28 | **D-52 closed** — the `parallelForPolled` CI flake was the test's early-out bound racing the thrower's unwind (23/400 on two cores); both early-out sections made deterministic. | [17/14](17-code-audit-and-hardening/14-build-hardening-ci.md) |
| 2026-10-27 | **#70** — smooth booleans keep the viewer's metric step: a porous sponge orbits on 4–7× fewer evaluations. | [23/11](23-porous-shell/11-viewer-trace-cost.md) |
| 2026-10-22 | **Porous shell run closed** — the `porous` source in `dualc_field`, the viewer and the C ABI, #63–#68 DONE; merged (PR #3). | [23/10](23-porous-shell/10-plan-run-record/README.md) |
| 2026-10-21 | **Hygiene + editor run closed** — #35 DONE (`repeat`'s neighbours pruned, exact), #34 PARTIAL (E12 reopened), the gate builds the C ABI (#53, the remote run owed), the standalone UI moved to DualCEditor; merged (PR #2). | [17/22](17-code-audit-and-hardening/22-hygiene-plan-run.md) |
| 2026-10-16 | **Block 21 closed** — the consolidation plan's eleven units harvested; its PR merged 2026-10-19 (`5ce5b1c`). | [21/09](21-cli-consolidation/09-plan-run.md) |

**The shipped keystone (2026-06-14 … 2026-07-06)** — builds #1–#3, the C ABI (#19) and
the client layer's move to Boletus, each DONE with its evidence in
[12 § The new sequence](12-field-graph-and-app/README.md#the-new-sequence-value-first-then-the-c-abi)
and [15](15-boletus-handoff.md); the ID vocabulary (`#N`, build #N, Tier 1 item N) is the
[glossary](../design/11-glossary.md)'s.

## Next up — DualC's own roadmap

**Two tracks**, each **highest-priority-first**; rationale and evidence in the linked
topic file, the trigger of every DEFERRED item in its [decisions](../decisions/README.md)
row (cited by ID).

### Track 1 — Engine & CLI expansion

1. **Porous shell** ([23](23-porous-shell/README.md)) — the `porous` source, run unattended;
   the one status table is [23 § The pages of this block](23-porous-shell/README.md#the-pages-of-this-block).
   #63–#68, #70 DONE; #69, the graded nodes' negative-`t` gap, DEFERRED ([#69](../decisions/README.md));
   the owner's review list is [23/10/03](23-porous-shell/10-plan-run-record/03-owners-review.md).
2. **Engine hardening — the audit's open findings**
   ([17](17-code-audit-and-hardening/README.md)) — a standing ledger, not a phase; the
   one status table is [17 § Tracked items](17-code-audit-and-hardening/README.md#tracked-items--status-at-a-glance).
   #34 PARTIAL 2026-10-21 (E12 reopened); #35 DONE 2026-10-21; D-52, the `parallelForPolled`
   flake, closed 2026-10-28 as a test defect ([17/14](17-code-audit-and-hardening/14-build-hardening-ci.md)); **NEXT:** **#53** (Linux `.so`, [14/06](14-c-abi/06-linux-shared-library.md))
   PARTIAL (libraries landed with #52; the gate builds the ABI since 2026-10-21; CI proof owed);
   the run's harvest is [17/22](17-code-audit-and-hardening/22-hygiene-plan-run.md).
3. **TPMS / lattice capability** ([05](05-tpms-lattices/README.md)) — **#17 strut
   lattices** and their Phases 3–5 DONE; **#18 tight Lipschitz `cellOverlaps`**
   DEFERRED ([#18](../decisions/README.md)).
4. **Streaming-export completion**
   ([11 § 5a](11-dense-lattice-deliverable/03-streaming-export.md#5a-deferred-enhancements-gains-vs-the-shipped-stl-version))
   — the shipped parts there and in
   [12 § G](12-field-graph-and-app/06-decimation.md#g-post-contour-qem-decimation---decimate----simplify).
   DEFERRED: per-tile locked-seam decimation (Approach B, [D-26](../decisions/README.md));
   `dualc_lattice --tile-depth` wiring dropped with the tool ([D-14](../decisions/02-dropped.md)).
5. **Core-DC algorithmic extensions** ([01 Tier 4](01-core-dual-contouring/README.md#tier-4--open-algorithmic-extensions-for-special-needs)) —
   **#13** intersection-free contouring, **#14** multi-material / open-boundary
   contouring (the home of an open lattice surface *as a mesh* —
   [12 § F](12-field-graph-and-app/05-open-surface.md)), **#15** GPU acceleration.
   PLANNED, unsequenced — no trigger.
6. **Infrastructure & packaging** — `install()` + `dualcConfig.cmake` Phase 2, Python
   bindings, a benchmark + perf-regression suite ([10](10-infrastructure-and-integration.md)),
   each DEFERRED ([#8, #9, #11](../decisions/README.md)).
7. **Additional export targets & I/O** — direct per-layer slicer, narrow-band VDB /
   voxel export, hybrid parametric descriptor
   ([11 § 6](11-dense-lattice-deliverable/README.md#6-not-on-the-critical-path)); read
   explicit OBJ `vn` ([09 #7](09-io-formats.md)). All DEFERRED, each gated on a concrete
   downstream consumer ([D-13, D-07, #7](../decisions/README.md)).
8. **DAG-ref serialization superset** (`id`/`ref`, `let … in …`)
   ([12 § A](12-field-graph-and-app/01-field-graph.md#a-the-field-graph-keystone)) —
   DualC-retained but **Boletus-driven**, so last ([D-12](../decisions/README.md)).

### Track 2 — Standalone raymarch app

Moved to **DualCEditor** ([D-66](../decisions/01-settled.md)): every interactive UI item —
section-plane UI and parameter edits (DONE there), the clip mask, the Web build, any future
one — is indexed, with its original intent verbatim, in
[22](22-dualceditor-handoff/README.md). DualC retains the codegen side, editor-driven: the
baked-preview hardening of
[12/07 § H](12-field-graph-and-app/07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep)
— a conservative baked far field ([D-31](../decisions/README.md)), a mesh-aware
`nodeFeatureScale` ([D-30](../decisions/README.md)), bake startup at high `--grid-res`
([D-29](../decisions/README.md)) — all DEFERRED.

## Principal blocks

| # | Topic | What it covers | Status |
| --- | --- | --- | --- |
| 01 | [Core dual contouring](01-core-dual-contouring/README.md) | The v1 DC engine (`dualc_demo`): sampler/contourer, QEF/octree, sign oracles, manifold DC, adaptive collapse, multi-threading, the bug catalogue. | DONE; #13/#14/#15 PLANNED |
| 02 | [Implicit / SDF foundation](02-implicit-sdf-foundation.md) | The v2 "SDF + DC" base: `ImplicitField`, `MeshSource`, sharp-features-for-free, the Phase 1–6 log. | DONE (v2 complete) |
| 03 | [Booleans & CSG](03-booleans-csg.md) | Hard/smooth combinators (`dualc_boolean`) + baked recipes (`dualc_csg_demo`). | DONE |
| 04 | [Primitives & operators](04-primitives-and-operators.md) | The analytic primitive catalogue, decorators, domain operators, 2D lifts (`dualc_primitive`, `dualc_lift`). | DONE |
| 05 | [TPMS lattices](05-tpms-lattices/README.md) | TPMS infill (`dualc_lattice`), the normalize-thickness fix; strut lattices (#17) and their enhancements (#17b); Lipschitz `cellOverlaps` (#18). | #16, #17 + Ph 3–5, #20 DONE; #18 DEFERRED |
| 06 | [Field-on-plane slicing](06-slice.md) | `dualc_slice` field cross-section → PNG/SVG. | DONE |
| 07 | [Interactive viewer](07-viewer-polyscope.md) | `dualc_view` Polyscope viewer (#10) + four-mode workbench (#21); its retirement with Polyscope, GLFW + glad owned (#50). | #10/#21 DONE; **#50 DONE** (2026-10-03) — tool retired |
| 08 | [Analytic raymarch viewer](08-raymarch.md) | `dualc_raymarch` GPU sphere-tracer for dense lattices. | DONE |
| 09 | [Input & output formats](09-io-formats.md) | STL/3MF/OBJ export dispatch; input vertex-normal handling. | export DONE; #7 DEFERRED |
| 10 | [Infrastructure & integration](10-infrastructure-and-integration.md) | Build/install (#8), the **C ABI (#19)**, Python bindings (#9), benchmarks (#11), the demo-data generator (#12). | **#19 DONE**; #54 DONE; #8 Ph2/#9/#11 DEFERRED; C# wrapper + `.gha` → **Boletus** ([15](15-boletus-handoff.md)) |
| 11 | [Dense-lattice deliverable](11-dense-lattice-deliverable/README.md) | The cross-cutting viz/manufacturing effort: rationale, the **Phase 0 benchmark**, the streaming/export steps. | steps 1–3 + 5 DONE; direct slicer + `dualc_lattice --tile-depth` DEFERRED; step 4 → **Boletus** |
| 12 | [Field-graph & standalone app](12-field-graph-and-app/README.md) | The 2026-06-12 re-eval: the **field-graph** keystone, field→GLSL codegen, the standalone raymarch app, open-surface isolation (§ F), decimation + its inspection report (§ G), the mesh-preview sweep (§ H). | #1/#2/#3, § D.1, § F, § G, § H DONE; Approach B, § H follow-ups DEFERRED; side-car → **Boletus**; sliders, parameter edits, clip mask, Web build → **DualCEditor** ([22](22-dualceditor-handoff/README.md)) |
| 13 | [Graded TPMS](13-graded-tpms/README.md) | Spatially-varying TPMS thickness via `graded-onion` over a control field; graded *wavelength* analyzed and dropped. | graded **thickness** DONE; graded **wavelength** DROPPED |
| 14 | [C ABI](14-c-abi/README.md) | The native-consumer boundary `dualc_capi` (#19): the host-side `capi/` library, the flat entry points, build/deployment, verification, the 0.4.0-and-later entries, the cancel token and progress callback (#48). | **DONE**; mesh resolver, diagnostics twins, unknown-key rejection, **#48** DONE; #53 PARTIAL; C# wrapper → **Boletus** |
| 15 | [Boletus hand-off](15-boletus-handoff.md) | The C# / Rhino / Grasshopper client, archived from this roadmap: hand-off table, the original DualC intent (verbatim), the DualC-retains boundary; links re-pointed 2026-09-21 after Boletus's docs restructuring. | index — DONE here; Boletus tracks its own status |
| 16 | [Field-graph & app — continued](16-field-graph-and-app-continued.md) | Retired 2026-09-18: its § H is [12/07](12-field-graph-and-app/07-mesh-preview-sweep.md); the number is never reused. | tombstone — § H DONE |
| 17 | [Code audit & engine hardening](17-code-audit-and-hardening/README.md) | The tracked ledger of the 2026-08-19 code audit ([`docs/raw/study/`](../raw/study/README.md)): every finding with a disposition, the tracked items, the record pages, the 2026-09-11 docs-system screening. | **PARTIAL** — a standing ledger; per-item status in [17 § Tracked items](17-code-audit-and-hardening/README.md#tracked-items--status-at-a-glance) |
| 18 | [C ABI — continued](18-c-abi-continued.md) | Retired 2026-09-18: its two entries are [14/04](14-c-abi/04-abi-0-4-0.md); the number is never reused. | tombstone — DONE |
| 19 | [Docs layers](19-docs-layers/README.md) | The 2026-09-17 docs restructuring: the plan (immutable, `docs/raw/`), items #36–#45, the phase table, per-phase evidence, the semantic-lint runs. | Phases 0–8 **DONE** (2026-09-20); the lint runs keep landing in [19/08](19-docs-layers/08-semantic-lint/README.md) |
| 20 | [Public delivery](20-public-delivery/README.md) | The repo as others build it: geometry-central pinned and fetched (options weighed), nanort vendored, the self-bootstrapping clean clone (#47); Linux as a build host (#49); hosted CI running the gate (#51), the plan's run (04). | **#47 DONE** (2026-09-21) · **#49 DONE** (2026-10-03) · **#51 DONE** (2026-10-06) |
| 21 | [CLI consolidation](21-cli-consolidation/README.md) | One framework, `dualc_field` + `dualc_field_view`: the demo meshes wound outward, `mesh(sign="gwn")`, per-node `bake`, 2D profiles + lifts and a graph slice in the graph, then seven side CLIs retired (#55–#60). | **DONE** (2026-10-16) — #55–#60; D-60 kept; the run harvested (09) |
| 22 | [DualCEditor hand-off](22-dualceditor-handoff/README.md) | The standalone-app UI (Track 2), archived from this roadmap: hand-off table (section-plane UI, parameter edits, clip mask, Web build), the boundary rule, the editor-driven items DualC retains, the original DualC intent (verbatim). | index — DONE here; DualCEditor tracks its own status |
| 23 | [Porous shell](23-porous-shell/README.md) | The `porous` source: seeds, one Delaunay, the distance to its Voronoi / Delaunay complex smoothed in the node; CPU, GLSL, C ABI, a Boletus plan. | #63–#68, #70 DONE; #69 DEFERRED |

## How the blocks relate

Blocks **02–04** are the v2 "SDF + DC" layer on the block-**01** engine; **05** (TPMS)
is a primitive family on top. Block **11** is cross-cutting: its delivered steps live
under their own tools ([06](06-slice.md), [08](08-raymarch.md), [09](09-io-formats.md))
while it holds the rationale, the Phase 0 benchmark and the open steps. Block **12** is
the keystone: the **field-graph** is the single source of truth the CLI, the GPU app and
the C ABI (**10**, **14**) build on; the client layer above the ABI lives in **Boletus**
(**15** records what moved), and the standalone-app UI above the codegen lives in
**DualCEditor** (**22**). Block **17** grades the engine those blocks built and is
where DualC's remaining work comes from; **19** is the documentation system itself; **21**
folds the side CLIs of blocks 01–08 into block 12's graph.
