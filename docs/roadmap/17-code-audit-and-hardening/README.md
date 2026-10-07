# Code audit & engine hardening

The tracked ledger of everything the **2026-08-19 code audit** raised and the engine has
not yet answered. The audit itself — 11 documents, ~70,000 words, an interactive explorer
and 24 figures — lives in [`docs/raw/study/`](../../raw/study/README.md) and is deliberately left
verbatim as a point-in-time artifact; it is **not** a status document. This folder is the
status document: every finding gets a disposition here, and the ones worth work get a
numbered item with a revisit trigger.

Technical detail is **not duplicated**. The audit's prose is the home for the argument;
[`docs/design/07-limitations.md`](../../design/07-limitations.md) (§ 9) is the home
for the design-level statement of the surviving limitations; this folder holds identity,
status and trigger, and links to both.

- [`01-findings-ledger-engine.md`](01-findings-ledger-engine.md) — the 33 findings **E1–E33** from
  study documents **A2–A5** (sampling / octree / BVH / sign oracles, the QEF, the
  contouring recursion, the implicit-field algebra).
- [`02-findings-ledger-craft.md`](02-findings-ledger-craft.md) — the 41 findings **C1–C41** from
  **B1–B5** (architecture, API and type design, memory and performance, concurrency,
  build and licensing).
- [`06-findings-ledger-testing.md`](06-findings-ledger-testing.md) — the 13 findings **C42–C54** from
  **T** (testing). (All finding ids are the ledger's, assigned 2026-08-31; the study has none.) Split out of `02` on 2026-09-10 at the 15 KB size limit.
- [`03-correctness-and-robustness/`](03-correctness-and-robustness/README.md) — tracked items
  **#22–#26** and **#46**: the findings that can make DualC produce a wrong answer or abort.
- [`04-engineering-quality.md`](04-engineering-quality.md) — tracked items **#27–#35**:
  speed, API hygiene, test coverage and the build.
- [`12-engineering-quality-records.md`](12-engineering-quality-records.md) — the delivery
  records of #28, #30 and #33 (the dead parameters, the determinism test, the three build
  batches), moved out of `04` on 2026-09-18.
- [`13-parity-gate-binding.md`](13-parity-gate-binding.md) — the delivery record of **#31**:
  the `gpu` CI job made required, the llvmpipe evidence, the CTest-half decision and the
  deliberate `opXor` break that turned the job red.
- [`14-build-hardening-ci.md`](14-build-hardening-ci.md) — the delivery record of **#33**'s
  rest: `DUALC_WERROR`, `DUALC_SANITIZE`, the layering assertion and the per-target dialect,
  the first warnings and sanitizer findings, and the CI runs that proved each one red.
- [`15-test-coverage-batches.md`](15-test-coverage-batches.md) — the delivery record of
  **#32**: Batch A (the corrected counts, the 38 `Approx(0.0)` margins and their gate check,
  a working directory per test case, the serial-`ctest` decision) and Batch B (the missing tests).
- [`07-repeat-tiling-fix.md`](07-repeat-tiling-fix.md) — the full record for #34's first
  item: `repeat` read only the folded tile, which put wrong geometry in shipped strut
  lattices; the conditional fix, its measured 2.3× cost, and the oracle test it showed to
  be certifying less than it claims.
- [`08-argument-validation.md`](08-argument-validation.md) — #34's second batch: fail-fast
  argument validation across the field layer, the `BBox{}` sentinel that was invisible to
  `isValid()`, and the negative-`scaled()` complement bug the guards exposed.
- [`05-diagnostics-channel.md`](05-diagnostics-channel.md) — the full record for
  **#26**, the `Diagnostics` out-parameter: what the item turned out to be once
  checked against the code, what shipped, what is deliberately not reported and
  why, and the C-ABI mirror.
- [`11-diagnostics-verification.md`](11-diagnostics-verification.md) — #26's evidence
  pages: the claim-by-claim re-scoping, the verification digests, the untested branch
  (moved out of `05` on 2026-09-18).
- [`09-local-checks-gate/`](09-local-checks-gate/README.md) — the local checks gate (a folder since 2026-09-21)
  (`scripts/check.py`) that stands in for CI: the check
  list, the doc-size ratchet, why every check was proven to fail, and exactly
  which of #31/#32/#33 it does and does not move.
- [`10-docs-system-screening.md`](10-docs-system-screening.md) — the 2026-09-11 audit
  of the documentation system itself: the verdict (accurate data, redundant and
  half-enforced system), the seven semantic gate checks, the one-ledger clean-up, the
  unknown-parameter-key defect the recipes exposed, and the split pass (four frozen files → folders).

87 findings total. **Five defects and two coverage gaps were fixed by the 2026-08-21
screening batch** — see [01 §
4.9](../01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch)
for what changed and its measured effect; that record is not restated here.

## Verification pass (2026-08-31)

The audit's own headline list of 14 ranked items was re-checked **against the code at HEAD
`0cdcc88`**, not against the documentation. Result: **5 fixed, 1 half-fixed, 8 untouched.**

| Rank | Item | State at `0cdcc88` |
| --- | --- | --- |
| 1 | `dualContourMesh` drops `signMethod` | **FIXED** — `src/pipeline.cpp:49-56` delegates to `sampleMeshToHermiteOctree` |
| 2 | `ARCHITECTURE.md` materially stale | **FIXED** — rewritten; the correction is recorded in its own opening note *(2026-09-17: that note went with the split into `docs/design/`; the section → page map is [19/02](../19-docs-layers/02-record-phase-3.md))* |
| 3 | `cellOverlaps` hides a Lipschitz-1 assumption | **HALF** — the nine wrappers forward, but no `lipschitzBound()` exists and 73 hand-written `cellOverlaps` sites remain → **#25** |
| 4 | `pinv` truncates large eigenvalues | **FIXED** — `src/internal/svd.cpp:457-459` |
| 5 | Adaptive collapse untested | **FIXED** — 12 references across `tests/test_contourer.cpp` and `tests/test_demo_meshes.cpp` |
| 6 | DC descent tables untested | **FIXED** — `tests/test_dc_tables.cpp`, 276 lines |
| 7 | `countRayHits` epsilon is scale-relative | **OPEN** — `std::max(tHit * 1e-5f, 1e-6f)` at `src/internal/mesh_bvh.cpp:486`; 4096-hit cap at `:480` → **#23** |
| 8 | Threading determinism is a comment, not a test | **OPEN** — `numThreads` occurs 0 times under `tests/` → **#30** |
| 9 | `parallelFor` terminates on exception | **OPEN** — no `try` / `catch` / `exception_ptr` in `src/internal/parallel.h` → **#22** |
| 10 | One heap allocation per emitted triangle | **OPEN** — `std::vector<std::vector<std::size_t>> tris` at `src/contourer.cpp:385` → **#27** |
| 11 | GLSL parity gate is not in CI | **OPEN** — no `add_test` for `dualc_glsl_parity`; still behind opt-in `DUALC_BUILD_GLSL_PARITY` → **#31** |
| 12 | Two dead public parameters | **OPEN** — `weldEdges` (`contourer.h:23`, annotated but present), `seed` (`sampler.h:25`, `(void)`-cast at `src/sampler.cpp:190`) → **#28** |
| 13 | No install rules; `CMAKE_SOURCE_DIR` in the sibling probe | **OPEN** — dead `INSTALL_INTERFACE` at `CMakeLists.txt:88`; `CMAKE_SOURCE_DIR` at `:27` and `:39` → **#33**; the export set is [10 #8](../10-infrastructure-and-integration.md) |
| 14 | Silent failure everywhere | **OPEN** — no `Diagnostics` type in `include/` or `src/`; `BBox::unit()` fallback at `src/sampler.cpp:171` → **#26** |

The table is left as the reading taken at `0cdcc88` — a dated record, not a live status
board; the ledger below carries the current state. What closed it, batch by batch (the
record of each is the linked page; nothing is restated here):

- **2026-08-31** — #22, #29, #30 DONE, #33 partial (ranks 8, 9, part of 13):
  [03](03-correctness-and-robustness/README.md), [12](12-engineering-quality-records.md).
- **2026-09-01** — #28, then #23 (ranks 12 and 7; #23 demonstrated failing first, the eight
  demo meshes byte-identical on a new positional digest):
  [03 #23](03-correctness-and-robustness/01-parallelfor-and-ray-parity.md#23-ray-parity-advance-epsilon-is-relative-to-hit-distance-not-feature-size),
  [12 § #28](12-engineering-quality-records.md#28--the-two-dead-parameters-deleted). The
  228 → 229 test-count correction of the same day is recorded once, in
  [12/07 § H](../12-field-graph-and-app/07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep).
- **2026-09-09** — #26 (rank 14, the audit's highest-value item; it shrank on contact
  with the code): [05](05-diagnostics-channel.md), [11](11-diagnostics-verification.md).
- **2026-09-10** — #34 opened with the `RepeatField` fold, a wrong-geometry defect that
  handed its 2.3× cost to the new #35: [07](07-repeat-tiling-fix.md); the same day,
  argument validation and the `BBox{}` sentinel, plus the negative-`scaled()` complement
  bug the guards exposed: [08](08-argument-validation.md).
- **2026-09-11** — the documentation system screened; unknown parameter keys rejected
  on both paths (a #34-class fix): [10](10-docs-system-screening.md).

*(2026-09-18: the five `Update` paragraphs that stood here restated the record pages and
the roadmap README's milestones; condensed to this list — [19 Phase 5](../19-docs-layers/README.md).)*

## Tracked items — status at a glance

**PARTIAL** as a block — a standing queue, not a phase. This table is the one status
ledger for the tracked items (the roadmap README links it rather than restating it);
full entries, with evidence and triggers, live in two files:
[`03-correctness-and-robustness/`](03-correctness-and-robustness/README.md) (#22–#26, #46) and
[`04-engineering-quality.md`](04-engineering-quality.md) (#27–#35).

**Latest verification** (the one home of the counts; every other page links here):
`ctest` **270** (2026-10-06, the full gate on Linux and in CI's three `build` jobs;
256 on 2026-09-17, [19/01](../19-docs-layers/01-record-phases-0-2.md)); `dualc_glsl_parity` **73/73**
(2026-10-06, asserted by `check.py --gpu` against `parity_expected_cases` in CI's required
`gpu` job — [13](13-parity-gate-binding.md)); the same 270 pass under ASan + UBSan in CI's
`sanitize` job (2026-10-06, [14](14-build-hardening-ci.md)); the same 270 pass under `ctest -j 8`,
three runs in a row (2026-10-07, Linux, [15](15-test-coverage-batches.md)).

| Item | Title | Status |
| --- | --- | --- |
| [#22](03-correctness-and-robustness/01-parallelfor-and-ray-parity.md#22-parallelfor-exception-propagation) | `parallelFor` exception propagation | **DONE** 2026-08-31 |
| [#23](03-correctness-and-robustness/01-parallelfor-and-ray-parity.md#23-ray-parity-advance-epsilon-is-relative-to-hit-distance-not-feature-size) | Ray-parity advance epsilon is relative to hit distance, not feature size | **DONE** 2026-09-01 |
| [#24](03-correctness-and-robustness/02-precision-and-celloverlaps.md#24-far-from-origin-precision--re-centre-before-the-float-seam) | Far-from-origin precision — re-centre before the float seam | DEFERRED |
| [#25](03-correctness-and-robustness/02-precision-and-celloverlaps.md#25-lipschitzbound--the-silent-correctness-opt-out-behind-celloverlaps) | `lipschitzBound()` — the silent correctness opt-out behind `cellOverlaps` | DEFERRED |
| [#26](03-correctness-and-robustness/03-diagnostics-item.md#26-a-diagnostics-channel-for-the-silent-failure-surface) | A `Diagnostics` channel for the silent-failure surface | **DONE** 2026-09-09 ([05](05-diagnostics-channel.md)) |
| [#27](04-engineering-quality.md#27-allocation-and-traversal-hot-spots) | Allocation and traversal hot spots | DEFERRED |
| [#28](04-engineering-quality.md#28-delete-the-two-dead-public-parameters) | Delete the two dead public parameters | **DONE** 2026-09-01 |
| [#29](04-engineering-quality.md#29-thread-safety-contract-for-user-derived-implicitfield) | Thread-safety contract for user-derived `ImplicitField` | **DONE** 2026-08-31 |
| [#30](04-engineering-quality.md#30-threading-determinism-test) | Threading-determinism test | **DONE** 2026-08-31 |
| [#31](04-engineering-quality.md#31-make-the-cpugpu-parity-gate-binding) | Make the CPU/GPU parity gate binding | **DONE** 2026-10-06 ([13](13-parity-gate-binding.md)) |
| [#32](04-engineering-quality.md#32-test-coverage-ledger) | Test-coverage ledger | **PARTIAL** — Batch A 2026-10-07 ([15](15-test-coverage-batches.md)); Batch B, the missing tests, PLANNED |
| [#33](04-engineering-quality.md#33-build--tooling-hardening) | Build & tooling hardening | **DONE** 2026-10-06 ([14](14-build-hardening-ci.md)); batches 2026-08-31, 09-01, 09-10; nanort + clean-clone 2026-09-21 by [20 #47](../20-public-delivery/01-pin-geometry-central.md) |
| [#34](04-engineering-quality.md#34-api-hygiene-batch) | API hygiene batch | **PARTIAL** — batches 2026-09-10 (×2), 09-11 (unknown keys, [10](10-docs-system-screening.md)) |
| [#35](04-engineering-quality.md#35-repeatfield-neighbour-set-optimisation) | `RepeatField` neighbour-set optimisation | PLANNED — born from #34's fix on 2026-09-10, no ledger finding behind it |
| [#46](03-correctness-and-robustness/02-precision-and-celloverlaps.md#46-offsetfield-does-not-forward-celloverlaps) | `OffsetField` does not forward `cellOverlaps` | **DONE** 2026-09-21 — opened 2026-09-20 from the first semantic lint ([19/08/01](../19-docs-layers/08-semantic-lint/01-2026-09-20-first-run.md)), no ledger finding behind it |

---

← Back to the [Roadmap index](../README.md).
