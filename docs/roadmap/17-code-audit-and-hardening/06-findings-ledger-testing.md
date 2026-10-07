# Findings ledger — testing (study T)

The 13 findings the [2026-08-19 audit](../../raw/study/README.md) raises in its testing
document **T**. The other two thirds of the ledger are
[`01-findings-ledger-engine.md`](01-findings-ledger-engine.md) (the 33 algorithm findings,
study A2–A5) and [`02-findings-ledger-craft.md`](02-findings-ledger-craft.md) (the 41
engineering findings, study B1–B5). Split out of `02` on 2026-09-10 when that file passed
the 15 KB size limit; the rows are unchanged. The `C42`–`C54` ids (like every `E`/`C` id)
were assigned by this ledger on 2026-08-31, not by the audit — `docs/raw/study/` never
uses them; the `Source` column is the way back into the study. Kept for life.

Each row is the audit's claim, its citation, and a disposition. **Nothing here is an
independently verified defect** unless the [topic README](README.md) says so; the citation
is the evidence, and the audit's reasoning is not restated.

**Dispositions.** `→ #NN` promoted to a tracked item ([#22–#26](03-correctness-and-robustness/README.md),
[#27–#35](04-engineering-quality.md)) ·
`ACCEPTED` a deliberate trade-off or an honest limitation, recorded so no future thread
re-litigates it · `MINOR-OPEN` real but below the tracking bar. The two coverage gaps the
2026-08-21 screening batch closed are **not** listed here — they are recorded in
[01 § 4.9](../01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch).

## T — Testing

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| C42 | Legacy `Approx` throughout, zero v3 matchers, and 36 unguarded `== Approx(0.0)` comparisons whose relative default epsilon degenerates to exact equality at zero (`test_primitives.cpp:197` and 28 similar sites, plus `test_lift.cpp`, `test_mesh_bvh.cpp`, `test_octree.cpp`). | `T:422-428,528,584` | → **#32** — **DONE 2026-10-07**: the count was **38**, not 36 (29 in `test_primitives.cpp`; the one in `test_field_graph.cpp` is split over two lines); all take `.margin(1e-12)` and the `approx-zero` gate check holds 0 ([15](15-test-coverage-batches.md)) |
| C43 | No per-test working directory: `catch_discover_tests` sets none, so every case shares `CMAKE_CURRENT_BINARY_DIR` and parallel safety is luck. `test_field_graph.cpp:119-134` also uses a fixed temp filename two concurrent CI jobs would collide on. | `T:403,558,585` | → **#32** — **DONE 2026-10-07**, disposition corrected: two CI matrix jobs run on separate machines and never collide; the collision is `ctest -j` within one tree. Each Catch case now runs in a working directory of its own, the temp file is a bare name, and `cli_gen_demo` no longer rewrites the meshes its siblings read ([15](15-test-coverage-batches.md)) |
| C44 | Float bit-identity assertions are unpinned against compiler flags — sensitive to `-ffast-math`, FMA contraction and x87 excess precision; correct for the MSVC/x64 target but unproven as a portability claim. | `T:184,405,558` | MINOR-OPEN |
| C45 | `partitionCubeEdges` is tested on 9 of 256 configurations despite being pure, O(1) and allocation-free. | `T:308,371,455,481,542-546` | → **#32** — **DONE 2026-10-07**: all 256 configurations, with an independent region-count oracle; the 9 hand-picked cases kept ([15](15-test-coverage-batches.md#batch-b--the-missing-tests)) |
| C46 | Error and exception paths are almost entirely untested — exactly one `REQUIRE_THROWS_AS` outside the field-graph parser. | `T:456` | → **#32** — count corrected 2026-10-07: **17** `REQUIRE_THROWS_AS` across five files, not one ([15](15-test-coverage-batches.md)); **closed 2026-10-07 by that correction** — the premise did not hold, and `test_validation.cpp` covers the argument errors ([08](08-argument-validation.md)) |
| C47 | No refinement-convergence test: nothing asserts that increasing `maxDepth` reduces error against an analytic surface — the core value proposition of adaptive DC. | `T:457` | → **#32** — **DONE 2026-10-07**: ≥ 3× less error per level, depth 4→7, sphere and torus ([15](15-test-coverage-batches.md#batch-b--the-missing-tests)) |
| C48 | Four of five `ContourerParams` knobs are never varied; only `manifoldDC` is toggled. | `T:458,538,583`, `A3:§8-9` | → **#32** — **DONE 2026-10-07**: each knob against a measured effect ([15](15-test-coverage-batches.md#batch-b--the-missing-tests)) |
| C49 | `interpolateNormals`' sharp/smooth claim is never unit-tested — only touched by a CLI exit-code smoke test. | `T:459` | → **#32** — **DONE 2026-10-07**: at the BVH hit and end to end on a cube ([15](15-test-coverage-batches.md#batch-b--the-missing-tests)) |
| C50 | Output normals are never checked for content: `test_pipeline.cpp:32` checks array size only, not unit length or alignment with the surface. | `T:460` | → **#32** — **DONE 2026-10-07**: unit length and `n · ∇f ≥ 0.99` on three fields ([15](15-test-coverage-batches.md#batch-b--the-missing-tests)) |
| C51 | No sanitizers and no coverage measurement. | `T:462-463` | → **#33** — sanitizers **DONE 2026-10-06** ([14](14-build-hardening-ci.md)); coverage → **#32** — **measured 2026-10-07**: 87.9 % of `src/`'s lines; a coverage job in CI is D-50 ([15](15-test-coverage-batches.md#coverage-measured-once)) |
| C52 | CLI smoke tests assert exit code only — none check output existence, non-emptiness or parseability, so `cli_slice` writing a corrupt PNG would pass. | `T:464` | → **#32** — **DONE 2026-10-07**: 21 `*_content` checks — PNG header, STL and 3MF structure and triangle counts, OBJ, SVG, JSON ([15](15-test-coverage-batches.md#batch-b--the-missing-tests)) |
| C53 | No performance-regression guard; the tiling RAM claim's underlying `chooseTileDepthForBudget` estimate is never validated against measured RSS. | `T:465` | → already tracked as [10 #11](../10-infrastructure-and-integration.md) (benchmark + perf-regression suite) |
| C54 | `makeUnitCube` / `makeBox` are reimplemented in eight test files, while `test_main.cpp` sits empty, reserved for exactly this shared fixture. | `T:448` | MINOR-OPEN |

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
