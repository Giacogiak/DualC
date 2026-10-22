# DualC hygiene + DualCEditor plan — the rest of #34, #53's CI proof, #35, the editor hand-off

**Drafted 2026-10-19**, from the owner's request at session start, after block 21 closed. An
input, not a record: status lives in roadmap block 17's ledger, block 14 (#53) and the new block
the plan opens for the editor (P07), never here. Each task is **one fresh session** driven by the
`plan-run` driver, opened and closed with `/repo-docs-lifecycle`. A task that does not fit a
session is closed partial, with what was verified and what remains written into its record page.

## 0. The goal

In the owner's words: "implement all that: the rest of #34 (API hygiene) …, #53 (Linux .so) …,
#35 (RepeatField neighbour-set optimisation) …. Then, DualC Roadmap must acknowledge the work
done in DualCEditor repo, which in fact has embodied the Standalone track of DualC roadmap, for
all UI aspects."

## 1. The owner's decisions

Taken 2026-10-19.

They are recorded as decision rows by P01 T1. The IDs are read from `docs/decisions/` at phase
time: on the drafting day the last decision is **D-64**, the last tracked item **#61**, and the
next roadmap block **22**.

| Topic | Decision |
| --- | --- |
| `gradientAt` contract (#34) | **It stays a unit outward direction**, and that is documented as the contract. A true-magnitude path (a separate virtual, default a central difference of `valueAt`) serves the callers that need \|∇f\|: `NormalizedField` and `closestSurfacePoint`'s Newton step. Rejected: making every override return ∇f with its magnitude, because that touches about 30 overrides and every caller, while the QEF re-normalises anyway. |
| `GridField` normal (#34) | It becomes continuous across cell faces. |
| Contourer threads (#34) | `ContourerParams` gets a thread count, and the C ABI and `dualc_field` pass it through. |
| #53 | The gate builds the C ABI by default, so CI proves the Linux `.so` on every push. |
| #35 | Implemented now, without waiting for its trigger, and measured. The result must be byte-identical. |
| Track 2 (standalone raymarch app) | **All UI is owned by DualCEditor** (`https://github.com/Giacogiak/DualCEditor`). The section-plane UI (D-17) and the live parameter push (D-25) are already delivered there. The clip mask (D-16), the web build and future UI items move there. The codegen-side baked-preview hardening (D-29 / D-30 / D-31) stays in DualC, marked editor-driven. `dualc_field_view` stays DualC's reference viewer. The hand-off is an extracted index page with pointer stubs in the old homes, as block 15 did for Boletus, not inline banners. |
| Git | Work on **`feat/hygiene-and-editor`**, cut from `main` at `5ce5b1c` (the merge of the block-21 PR, 2026-10-19). **No session pushes or adds a remote.** Every push to `origin` is the owner's, by hand. Sessions commit locally. |
| Running | The run is unattended overnight. Every gate is `auto`, and a session never stops on a question. Where this plan leaves a choice open, the session takes the default stated here, writes `**Chosen in-run (pending owner review):** …` into its handoff and the record page, and goes on. |

## 2. Repo facts

Verified 2026-10-19 against `aef902e`, the tree merged as `5ce5b1c`.

- **Gradient contract.** `include/dualc/implicit.h:49-51` reads "Normalized, this is the outward
  surface normal". The census of overrides:
  - Unit: primitives' normalised FD in `primitives.h:25-41`, sphere/plane analytic, `MeshSource`, `GridField` (`grid_field.cpp:108-145`, normalised at `:142`), and the two `fdGradient` copies (`domain_ops.cpp:17-28`, `lift.cpp:12-23`).
  - Pass-through: offset, onion, transform, scale, mirror, repeat, hard booleans.
  - Blend-shortened: the smooth booleans and mix, `combinators.cpp:182,220,247,283`, and elongate, `decorators.cpp:222`.
  - Raw FD, not unit: `WindingNumberField` only, `winding_field.cpp:76-96`.
- **`NormalizedField`.** `decorators.cpp:337-381` recomputes the magnitude by a central
  difference of the child's value, with e=1e-4 (`:363-377`). Its regression test is
  `test_tpms.cpp:199-215`.
- **`closestSurfacePoint`.** It is in `implicit_field.cpp:71-86`, and its only caller is the
  sampler fallback at `sampler.cpp:80`. It steps `q - g*(f/|g|)`, which is exact only where |∇f| ≈ 1.
- **Normals into the QEF.** `edgeHit` normalises (`implicit_field.cpp:65`), and so does
  `QefSolver::add` (`qef.cpp:140-153`).
- **GLSL.** The shader emits no gradients: `sceneGrad` is an FD (`field_glsl.cpp:508-514`). The
  `normalize` node emits its own FD of |grad| (`:1446-1470`). The C ABI exposes no gradient.
- **Threads.**
  - `ContourerParams` (`contourer.h:19-29`) has no thread field.
  - `contourer.cpp:699-725` calls `resolveThreadCount(0)`, and its comment at `:703` names #34.
  - `SamplerParams::numThreads` is at `sampler.h:32-35`.
  - `bakeToGrid` (`grid_field.cpp:171`) and the mesh narrow-band bake (`mesh_source.cpp:167,195`) are hardcoded to all threads.
  - The C ABI's `DualcContourParams.numThreads` (`dualc_c.h:132`) reaches only the sampler: `dualc_c.cpp:183,249,355,411` default-construct `ContourerParams`.
  - `dualc_field` has no `--threads` flag; its parser is at `dualc_field.cpp:273-374`.
- **#53.**
  - `DUALC_BUILD_C_ABI` is OFF by default (`CMakeLists.txt:25-41`, `:257-263`).
  - The gate's configure (`check.py:1430-1467`) passes only TESTS and EXAMPLES.
  - None of gate.yml's jobs (`docs`, `build`×3 OS, `sanitize`, `gpu`) sets the C ABI option, so the `.so` and the four `cli_c_abi*` cases run nowhere.
  - The owed proof is listed in `docs/roadmap/14-c-abi/06-linux-shared-library.md`. It includes the `-DDUALC_BUILD_TESTS=OFF` demo-mesh copy Boletus reported (`examples/CMakeLists.txt:246-264`).
- **#35.**
  - `RepeatField` is at `domain_ops.cpp:182-332`. `childFitsOnePeriod` (`:278-288`) chooses between the fold and `nearestCopy` (`:302-327`).
  - `RepeatLimitedField::evaluate` is at `:414-445`.
  - The cost is recorded in 17/07: octet depth 7 goes from 39 s to 91 s. The levers are listed in `04-engineering-quality.md` § #35.
  - The pins are `test_domain_ops.cpp:57` (the off-centre sphere across a seam) and `:290-319` (the `cellOverlaps` superset).
  - There is no benchmark harness (#11 is DEFERRED).
- **Counts at `aef902e`.** ctest 344, parity 82/82 (21/09).
- **DualCEditor** (read 2026-10-19).
  - 46 commits, 2026-10-09 … 10-15, branch `main`, remote `https://github.com/Giacogiak/DualCEditor`.
  - Decisions E-01 … E-28.
  - It consumes DualC's host libraries `dualc_examples_fieldglsl` (+ fieldgraph, common, `dualc::dualc`) by FetchContent at `2fcb6ee` (its `CMakeLists.txt:24`, decision E-06), not through the C ABI.
  - Delivered:
    - The section-plane UI, commit `1b59389` (2026-10-14), on `uClip[3]` / `uClipMask`.
    - Live parameter edits without a recompile, in-process uniforms, `trace_renderer.cpp:284-291`. The dock has followed the selection since `95e1a8d`.
    - Its cmd-ref `docs/command_reference/01-viewport.md` § Section and § Parameters.
  - Not delivered: the clip mask and the web build. E-22, freezing an object to a baked grid, is deferred there.
  - E-20 asks to promote graph + codegen to a supported library.
  - 21/09 already notes the `vec2` uniform gap (`setBinding` lacks `glUniform2f`).
  - The editor's own docs ask DualC questions Q1–Q4 in `docs/roadmap/06-dualc-questions.md`.

## 3. Phases

Filename order is dependency order:

- P01 opens the record.
- P02–P04 are #34. P02 is independent. P04 uses P03's true-gradient path.
- P05 is #53. It changes the gate's configure, so it comes after the code units, keeping their gate runs comparable.
- P06 is #35.
- P07 is the editor hand-off. It is docs only and could run anywhere; it comes late so that § Next up is rewritten once.
- P08 closes.

| Phase | Tasks | Advances |
| --- | --- | --- |
| P01 | T1 open the record: ledger entries, decision rows, stale #53 cell | 17, 14, decisions |
| P02 | T1 `ContourerParams::numThreads`, C ABI + `dualc_field --threads`, bakes | #34 |
| P03 | T1 contract text + census · T2 the true-gradient path, Newton step, `WindingNumberField` unit, one FD helper | #34 |
| P04 | T1 continuous `GridField` normal, measured; #34 DONE | #34 |
| P05 | T1 the gate builds the C ABI; red run; TESTS=OFF copy | #53 |
| P06 | T1 measure + prune neighbours · T2 byte-identical proof, record; #35 DONE | #35 |
| P07 | T1 the DualCEditor hand-off index (block 22) · T2 stubs, Track 2, decision rows | Track 2 |
| P08 | T1 harvest, lint, loss audit, final gates, PR text for the owner | — |

Defaults for choices left open:

- The virtual's name is `trueGradientAt`.
- The C ABI version goes to 0.5.2. It is a patch, because `numThreads` already exists and starts being honoured.
- `--threads` default 0 means all threads.
- The C ABI is built by `check.py`'s default configure rather than a gate.yml-only flag.
- #35's levers go in the order (a), then (b), then (c), with (c) only if more than 1.3× is left.

## 4. Out of scope

- Any push. Opening the PR.
- DualCEditor's repo: no edit there. Its pin bump and the `vec2` fix are the editor's.
- Boletus's re-vendor.
- #24, #25, #27 and #61.
- The web build itself.
- The baked-preview hardening's code (D-29/30/31), which is only re-homed as editor-driven.
