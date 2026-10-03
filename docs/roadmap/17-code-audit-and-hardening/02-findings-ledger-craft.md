# Findings ledger — craft (study B1–B5)

The 41 findings the [2026-08-19 audit](../../raw/study/README.md) raises in its engineering
documents: **B1** (architecture), **B2** (API and type design), **B3** (memory, ownership,
performance), **B4** (concurrency and determinism) and **B5** (build, dependencies,
licensing). The algorithm half — A2–A5 — is in
[`01-findings-ledger-engine.md`](01-findings-ledger-engine.md); the testing document **T**
was split out to [`06-findings-ledger-testing.md`](06-findings-ledger-testing.md) on
2026-09-10, when this file passed the 15 KB size limit.

Each row is the audit's claim, its citation, and a disposition. **Nothing here is an
independently verified defect** unless the [topic README](README.md) says so; the citation
is the evidence, and the audit's reasoning is not restated.

**Dispositions.** `→ #NN` promoted to a tracked item ([#22–#26](03-correctness-and-robustness/README.md),
[#27–#35](04-engineering-quality.md)) ·
`ACCEPTED` a deliberate trade-off or an honest limitation, recorded so no future thread
re-litigates it · `MINOR-OPEN` real but below the tracking bar · `DONE <date>` with no
`→ #NN` (C13, C14): fixed in a batch without a tracked item. The five defects and two
coverage gaps the 2026-08-21 screening batch closed are **not** listed in any of the three
ledgers — they are recorded in
[01 § 4.9](../01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch).

## B1 — Architecture and module boundaries

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| C1 | `types.h:9` is `using geometrycentral::Vector3;`, so `dualc::Vector3` *is* geometry-central's type — the library is unusable without geometry-central headers even for a pure-analytic field graph. | `B1:77-87,160-162` | ACCEPTED — geometry-central is a declared hard dependency ([README](../../../README.md) dependency layout); the audit concedes the attack "lands" but the coupling is deliberate |
| C2 | The third-party layering rule (host-only deps never link into `libdualc`) is enforced by comments only — nothing stops `target_link_libraries(dualc PRIVATE dualc_examples_io)`. | `B1:99-106`, `B5:190` | → **#33** (a CI assertion on `LINK_LIBRARIES`) |
| C3 | Internal-organisation smells: `implicit.h` does four jobs in 329 lines; `primitives.h` publishes 36 classes' private layouts across 555 lines (ABI-fragile) while combinators and decorators hide behind factories; math helpers (`clampd`, `sgn`, `vabs`, `vmax0`, `len2max0`, `boxMinMax`) are duplicated across five files. | `B1:121-129` | MINOR-OPEN — called the easiest concrete improvement in the codebase |
| C4 | `ContourerParams::weldEdges` and `SamplerParams::seed` are declared, documented and referenced nowhere. | `A4:364-367` | → **#28** — **DONE 2026-09-01** (both deleted; source-compatibility break, no behaviour change) |

## B2 — API and type design

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| C5 | `cellOverlaps` conflates a performance fast path with a correctness opt-out in one virtual, and forgetting to override it is silent rather than a compile error. The domain-operator forwarding bug was one symptom; the design flaw and the proposed `lipschitzBound()` accessor are untouched. | `B2:45-49,221-223` | → **#25** |
| C6 | `FieldPtr` is `shared_ptr<ImplicitField>`, not `shared_ptr<const ImplicitField>`, though `implicit.h:67-68` calls the graph an immutable expression tree. Compiles unchanged if fixed. | `B2:58,193,231,243` | → **#34** — **DONE 2026-09-01**; the audit's "compiles unchanged" claim proved false ([04 #34](04-engineering-quality.md#34-api-hygiene-batch)) |
| C7 | No CPU-side memoisation of the field-graph DAG: `UnionField::valueAt` and friends re-evaluate shared subtrees per reference — O(paths), not O(nodes) — while the GLSL backend dedupes by structural equality. | `B2:110,244`, `A5:394` | MINOR-OPEN — a quotable inconsistency; the fix is hash-consing or a per-sample memo |
| C8 | `gradientAt`'s contract says "gradient" and it returns a unit direction, which is why `NormalizedField::trueGradMag` recomputes the magnitude by finite differences. | `B2:166-176,248` | → **#34** |
| C9 | No argument validation anywhere: `SphereField(c,-5)`, `scaled(f,0)` (division by zero at `decorators.cpp:334`), `GyroidField(c,0)`, `transformed(f,M)` misusing `inverseRigid()` on a sheared matrix, `repeatedLimited(count=0)`. | `B2:158-164,227,249` | → **#34** — the audit calls it "not really defensible"; debug asserts would catch all of it |
| C10 | `UnionField::gradientAt` (and Intersection, Xor) evaluate both operands redundantly — ~2D extra subtree evaluations through a depth-D boolean chain. | `B2:132,207,246` | MINOR-OPEN — a three-line fix |
| C11 | Minor C++ hygiene: no `final` anywhere, `noexcept` used twice in the whole public API, and the abstract base's implicit copy operations left live rather than Rule-of-Five-compliant. | `B2:178-186,249` | MINOR-OPEN — latent, not live, since combinators are only ever `make_shared`'d |
| C12 | `std::variant` + `std::visit` was never benchmarked against virtual dispatch; the audit argues variant loses on this architecture but concedes nobody measured. | `B2:134-145,215,247` | ACCEPTED — an open question, not a defect |

## B3 — Memory, ownership, performance

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| C13 | `SignOracle`'s non-owning reference is correct only by member declaration order in `MeshSource::Impl` (`mesh_source.cpp:44-46`); swap `bvh` and `oracle` and it still compiles, binding to an unconstructed object. No `-Wreorder` catches it. | `B3:59-74,324` | **DONE 2026-09-01** — the constraint is now stated at the declaration site in `mesh_source.cpp`, including why `-Wreorder` is not a guard here (MSVC has no equivalent, and reordering the init list to match silences it everywhere) |
| C14 | `MeshSource`'s lifetime warning (`implicit.h:73-74`) is over-restrictive: `MeshBVH` copies everything by value at construction (`mesh_bvh.h:32-34`), so callers are told to keep a mesh alive that need not be. | `B3:76-85,316-318,325` | **DONE 2026-09-01** — comment corrected on `MeshSource`, and on `WindingNumberField`, which carried the identical false claim (both hold `MeshBVH` **by value**; the mesh is read during construction only) |
| C15 | The octree is a pointer-chased tree of ~10⁶ individually `new`ed nodes — ~1 GB in ~2×10⁶ allocations at depth 9. | `B3:101-123,165,294,326-327` | → **#27** — called the largest untaken performance win in the core |
| C16 | One heap allocation per emitted triangle: `std::vector<std::vector<size_t>> tris` (`contourer.cpp:362`, emission at `:486`) — 20.9 M allocations at depth 9. | `B3:126-134,302,328` | → **#27** — forced by geometry-central's nested-polygon input type |
| C17 | A malloc/free pair per BVH query, from `stack.reserve(64)` on a function-local vector in every hand-rolled traversal (`mesh_bvh.cpp:615,507,730`). | `B3:136-142,317,328` | → **#27** — the most easily fixed performance criticism in the codebase |
| C18 | Two full-size transient buffers (`leaves`, `solved`) are alive simultaneously with the result map in the contourer's pre-solve (`contourer.cpp:625-632`). | `B3:144-146` | MINOR-OPEN |
| C19 | `QefData::getData()` returns a 14-field struct by value to read one `int` (`contourer.cpp:288`). | `B3:148-150,302` | ACCEPTED — vendored code; the audit would not patch it |
| C20 | The BVH closest-point kernel gathers through double indirection (`vertOf`, `mesh_bvh.cpp:603-607`) because nanort permutes triangles during the SAH build; pre-flattening to 9 floats per triangle is ~3× leaf throughput. | `B3:166,294,305` | → **#27** — the second-largest untaken win |
| C21 | The float/double seam: two independent, undocumented narrowings (BVH float via nanort, QEF/SVD float via the vendored solver) with no re-centring. Invisible near the origin, catastrophic at site coordinates. Also makes signs (double) and crossings (float) occasionally disagree, absorbed by the sampler's miss fallback. | `B3:181-207,243-247,286-331`, `A3:§11` | → **#24** |
| C22 | A pattern of silent degradation — six documented cases — plus the observation that `buildPseudoNormalTopology` already computes the watertightness test and discards it. A `Diagnostics` out-parameter costs one struct and four assignments. | `B3:239-271,332-333` | → **#26** — **DONE 2026-09-09**; the watertightness observation holds, the count of six did not (four real degradations shipped, plus the empty-contour channel) — full record in [05](05-diagnostics-channel.md) |
| C23 | `minDepth` / `maxDepth` are never validated: negatives are accepted and `maxDepth = 20` exhausts memory rather than failing fast. | `B3:236-237`, `A2:§1` | → **#34** |
| C24 | `parallelFor` has no exception handling. | `B3:235` | → **#22** — **DONE 2026-08-31** (full detail at C29) |

## B4 — Concurrency and determinism

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| C25 | `collectFrontier` duplicates `buildNode`'s refine-decision logic (`sampler.cpp:126-154`); the code's own comment admits it. | `B4:85,225` | MINOR-OPEN — a maintenance hazard; one shared `refineDecision` helper fixes it |
| C26 | The frontier's `stopDepth = min(maxDepth, 3)` caps it at 512 nodes with no reference to thread count; with `minDepth=0` and a small object it can shrink to ~20, leaving a 64-core machine at ~25% utilisation. | `B4:87,224` | MINOR-OPEN |
| C27 | Threading determinism is a documented guarantee and is never tested — `numThreads` appears nowhere in the suite. | `B4:106,196,227,570-572` | → **#30** — **DONE 2026-08-31** |
| C28 | The thread-safety contract for user-derived `ImplicitField` is nowhere documented: a `mutable` memoisation cache compiles, passes every single-threaded test, and races silently under the default `numThreads = 0`. | `B4:108-121,212,228` | → **#29** — **DONE 2026-08-31**; the audit called it the cheapest fix in the library |
| C29 | `parallelFor` has no exception handling: an escaping exception from a spawned worker *and* from the main thread's own `worker()` call each reach `std::terminate` by different paths. | `B4:132-143,206-208,229` | → **#22** — **DONE 2026-08-31** |
| C30 | Possible false sharing in the narrow-band bake's phase 2 (adjacent `value[i]` / `bandAbs[i]` floats, per-index round-robin scheduling), unmeasured. | `B4:145-153,216,230` | MINOR-OPEN — the ratio argument suggests <1% of cost; 64-index chunking is trivial and preserves determinism |
| C31 | `ContourerParams` has no thread-count knob — `contourer.cpp:626` hard-codes `resolveThreadCount(0)` while `SamplerParams::numThreads` exists, so restricting the sampler silently gets all hardware threads in the contourer. | `B4:173,231` | → **#34** |

## B5 — Build, dependencies, licensing

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| C32 | Global C++ dialect flags are set before the third-party `add_subdirectory` calls, leaking DualC's dialect choice into geometry-central and polyscope. | `B5:25-27,188` | → **#33** |
| C33 | `DUALC_GC_DIR` / `DUALC_POLYSCOPE_DIR` resolve against `CMAKE_SOURCE_DIR`, not `CMAKE_CURRENT_SOURCE_DIR`, so DualC cannot be `add_subdirectory`'d into a larger project without overriding cache variables — despite that being the only supported consumption mode. | `B5:82,224,228,254` | → **#33** — **DONE** (probes 2026-08-31; the 22 subdirectory uses 2026-09-01) |
| C34 | No `find_package` fallback for geometry-central, and transitive dependencies (Eigen, nanort, nanoflann, happly) are inherited by a comment at `CMakeLists.txt:92-94`; nanort is structurally load-bearing for the BVH but undeclared. | `B5:84,86,228` | → **#33** |
| C35 | No compiler-hardening infrastructure: no `-Werror`/`/WX`, no sanitizer option, no `clang-tidy`, no IPO/LTO, no `CMAKE_EXPORT_COMPILE_COMMANDS`. ASan/UBSan flagged as high-value and near-free given the raw-pointer octree, hand-written BVH and vendored float SVD. | `B5:110,246-248,255,261` | → **#33** — `CMAKE_EXPORT_COMPILE_COMMANDS` **DONE 2026-08-31**; sanitizers / `-Werror` / `clang-tidy` still open |
| C36 | Catch2 is pinned by mutable git tag (`GIT_TAG v3.5.4`), with no `GIT_SHALLOW` and no offline or system fallback, so configure requires network access and is not strictly reproducible. | `B5:137,228,257,566` | → **#33** — **DONE 2026-08-31** (SHA pin; `GIT_SHALLOW` deliberately not enabled) |
| C37 | `DUALC_BUILD_TESTS` / `DUALC_BUILD_EXAMPLES` default ON, so any subproject consumer triggers Catch2's network fetch and builds 21 test translation units unasked. | `B5:138,186,257` | → **#33** — **DONE 2026-08-31**, with a CMake-3.14-compatible fallback |
| C38 | Every SDF formula is hand-written twice (double C++, float GLSL) and `dualc_glsl_parity` is the only guard — opt-in, outside CTest, needs a GL context. Editing a shared formula yields a green build and a green `ctest` with a silently diverged previewer. | `B5:172-181,218,238-240,260` | → **#31** — named the most substantive architectural criticism available against the project |
| C39 | No `install()` rules, export set or `dualcConfig.cmake`; `$<INSTALL_INTERFACE:include>` is a dead generator expression and the library is consumable only via `add_subdirectory`. | `B5:184,224,244,261` | → already tracked as [10 #8](../10-infrastructure-and-integration.md); blocked in part on geometry-central shipping no Config package |
| C40 | A clean clone does not build: the demo `.obj` meshes are gitignored, the `copy_if_different` POST_BUILD step fails against a missing source, and the generator that would produce them is itself one of the targets being built. | `B5:185,244,261` | → **#33** |
| C41 | The third-party layering rule is enforced by comments only. | `B5:190,261` | → **#33** — same defect as C2 |

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
