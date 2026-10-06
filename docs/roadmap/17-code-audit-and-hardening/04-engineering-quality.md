# Tracked items — Engineering quality (#27–#35)

The audit findings about speed, API hygiene, test coverage and the build. Items that affect the correctness of output are in [`03-correctness-and-robustness/`](03-correctness-and-robustness/README.md).

Part of the [code audit ledger](README.md), which indexes the three findings ledgers and
the per-item record pages. Headings are ID + title only; status, date and trigger live on
the body lines ([docs conventions](../../README.md#conventions)). Entries carry the audit's
claim and its citation — **audit claims, not independently verified defects**, except where
a line says so.

## #27 Allocation and traversal hot spots
**DEFERRED.** Four wins identified by inspection, none taken:

- the octree is a pointer-chased tree of individually `new`ed nodes — ~1 GB in ~2×10⁶
  allocations at depth 9; an arena (`std::deque<HermiteNode>`) is worth 2-3× and is called
  the largest untaken performance win in the core;
- one heap allocation per emitted triangle — `std::vector<std::vector<std::size_t>>` with a
  `push_back` per face, 20.9 M tiny allocations at depth 9, forced by geometry-central's
  nested-polygon input type;
- a malloc/free pair per BVH query, from `stack.reserve(64)` on a function-local vector in
  every hand-rolled traversal — the most easily fixed of the four;
- the closest-point kernel gathers through double indirection because nanort permutes
  triangles during the SAH build (pre-flattening to 9 floats per triangle is ~3× leaf
  throughput), and `findClosest` does not distance-order its children (another 1.5-3×).

Peak memory is solved one layer up by the tiled export path
([11 § 5a](../11-dense-lattice-deliverable/03-streaming-export.md)), so none of this is urgent.
*Trigger:* the benchmark suite ([10 #11](../10-infrastructure-and-integration.md)) landing,
so the gain is measured rather than asserted.
*Source:* `docs/raw/study/B3-memory-ownership-performance.md:101-166`,
`docs/raw/study/A2-sampling-octree-oracles.md:317`, `docs/ARCHITECTURE.md` § 9.

## #28 Delete the two dead public parameters
**DONE (2026-09-01).** `ContourerParams::weldEdges` and `SamplerParams::seed` were declared,
documented and read nowhere.

**Delivered.** Both fields **deleted** rather than kept as reserved no-ops — a
**source-compatibility break** with no behaviour change (neither field ever had an effect;
nothing in this repo or the C ABI set them). Record: [12 § #28](12-engineering-quality-records.md#28--the-two-dead-parameters-deleted).
*Source:* `docs/raw/study/A4-contouring-recursion.md:364-367`, `docs/ARCHITECTURE.md` §9.

## #29 Thread-safety contract for user-derived `ImplicitField`
**DONE (2026-08-31).** Nothing documented that `valueAt` / `gradientAt` are called concurrently. A
`mutable` memoisation cache — the most obvious thing to write for an expensive custom field —
compiles, passes every single-threaded test, and races silently under the default
`numThreads = 0`. The audit calls this a real API defect and the cheapest fix in the library:
one sentence on the base class, pointing at `bakeToGrid` as the supported way to amortise an
expensive field.

**Delivered.** A `THREAD SAFETY` block at the head of the `ImplicitField` public section
(`include/dualc/implicit.h`): every method is called concurrently, `const` must mean
genuinely immutable, the `mutable` memoisation cache is named as the specific trap, and
`bakeToGrid` / `GridField` are pointed to as the supported way to amortise an expensive
field. Documentation only -- no code changed, nothing to test.
*Source:* `docs/raw/study/B4-concurrency.md:108-121`.

## #30 Threading-determinism test
**DONE (2026-08-31).** `include/dualc/sampler.h` and [01 Tier 1](../01-core-dual-contouring/README.md#tier-1--high-leverage-do-these-next)
both promise bit-identical output regardless of thread count. Contour the same field at 1, 2
and 0 threads and compare vertex count, face count and positions — three lines.
**Delivered.** `tests/test_parallel.cpp` requires **bit-identical** output across
`numThreads` of 1, 2, 0 and 64 — and also passes against the pre-fix library: a regression
gate, not a bug fix. Record: [12 § #30](12-engineering-quality-records.md#30--the-determinism-test).
*Source:* `docs/raw/study/B4-concurrency.md:106`, `docs/raw/study/T-unit-testing.md`.

## #31 Make the CPU/GPU parity gate binding
**DEFERRED.** Every SDF formula exists twice — `double` in C++, `float` in GLSL — and
`dualc_glsl_parity` (73/73,
[12/07 § H](../12-field-graph-and-app/07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep)) is the
only thing keeping them honest. It defaults OFF, needs a GL context and has no CTest entry,
so editing a shared formula gives a green build and a green `ctest` with a silently diverged
previewer. The audit calls this the most substantive architectural criticism available
against the project. Two candidate fixes: a headless-GL CI job (llvmpipe / SwiftShader), or a
CPU reference evaluator for the emitted AST — half of which is already done, since the
codegen library is deliberately GL-free.
*2026-09-10:* `check.py --gpu` ([09](09-local-checks-gate/README.md)) asserts **73/73** from a known
cwd, closing the "exit 0 while silently skipping cases" hole. Still DEFERRED: needs a GL context.
*Trigger:* a CPU/GPU divergence reaching a user, or headless-GL CI becoming available.
*Source:* `docs/raw/study/B5-build-dependencies-licensing.md:172-181`.
*Verified 2026-08-31:* no `add_test` for `dualc_glsl_parity` in `examples/CMakeLists.txt`.

## #32 Test-coverage ledger
**PLANNED.** The audit's honest ledger, minus the two gaps already closed. Legacy `Approx`
throughout with 36 unguarded `== Approx(0.0)` comparisons, whose default epsilon is relative
and so degenerates to exact equality at zero (mechanical fix: `.margin(1e-12)`). No per-test
working directory, so parallel safety is luck, plus a fixed temp filename in
`test_field_graph.cpp` that two CI jobs would collide on. `partitionCubeEdges` — pure, O(1),
allocation-free — tested on 9 of 256 configurations. Exactly one `REQUIRE_THROWS_AS` outside
the field-graph parser. No refinement-convergence test, so nothing asserts that raising
`maxDepth` reduces error against an analytic surface. Four of five `ContourerParams` knobs
never varied. `interpolateNormals`' sharp/smooth claim never unit tested. Output normals
checked for array size but never unit length or orientation. CLI smoke tests assert exit code
only — a corrupt PNG would pass.
*Source:* `docs/raw/study/T-unit-testing.md` §§ 4-6, `docs/raw/study/A3-the-qef.md:218`.

*2026-09-10:* the [gate](09-local-checks-gate/README.md) **hosts** this item without advancing it: it
runs `ctest` serial (encoding the shared-cwd constraint) and `--metrics` counts the
`Approx(0.0)` uses. The tests are still unwritten.

**Added 2026-09-10 — a test that certifies less than it claims.**
`tests/test_strut_lattice.cpp`'s oracle comment promises more than its sampling strategy
checks (the `RepeatField` fix corrected 678 vertices it had passed); left in place by
decision and folded into this item — the evidence is in [07](07-repeat-tiling-fix.md).

## #33 Build & tooling hardening
**PARTIAL: DONE (2026-08-31); the rest DEFERRED.** Four of the cheap items landed --
see *Delivered* below the scope. What follows is the full original scope.
Global C++ dialect flags are set before the third-party `add_subdirectory`
calls and leak into geometry-central and polyscope. The sibling probes use
`CMAKE_SOURCE_DIR`, so DualC cannot be `add_subdirectory`'d into a larger tree without
overriding cache variables — despite that being the only supported consumption mode.
Transitive dependencies (Eigen, nanort, nanoflann, happly) are inherited by comment; nanort
is structurally load-bearing but undeclared. Catch2 is pinned by mutable git tag rather than
SHA, with no shallow clone and no offline fallback. `DUALC_BUILD_TESTS` and
`DUALC_BUILD_EXAMPLES` default ON, so any subproject consumer triggers a network fetch and 21
test translation units unasked (`PROJECT_IS_TOP_LEVEL` is the fix). No sanitizer option, no
`-Werror` / `/WX` switch, no `clang-tidy`, no `CMAKE_EXPORT_COMPILE_COMMANDS`, no coverage —
ASan/UBSan are flagged as high-value and near-free given the raw-pointer octree, hand-written
BVH and vendored float SVD. The third-party layering rule (host-only deps never link into
`libdualc`) is enforced by comments only; a CI assertion on `LINK_LIBRARIES` would make it
real. And a clean clone does not build: the demo `.obj` meshes are gitignored and the
`copy_if_different` POST_BUILD step fails against a missing source.

The `install()` / export-set / `dualcConfig.cmake` half of the audit's rank-13 item is
already tracked as [10 #8](../10-infrastructure-and-integration.md); only the
`CMAKE_SOURCE_DIR` half is new here.
**Delivered.** Three batches — four CMake one-liners (2026-08-31), the same
`CMAKE_SOURCE_DIR` defect in the subdirectory lists (2026-09-01), and the local checks gate
(2026-09-10, [09](09-local-checks-gate/README.md)), which hosts the rest without closing any
sub-item — record: [12 § #33](12-engineering-quality-records.md#33--build-hardening-three-batches).

Still open in this item: the dialect-flag leak into third-party subtrees, sanitizer /
`-Werror` / `clang-tidy` options, and the `LINK_LIBRARIES` layering assertion. *2026-09-21:*
the undeclared transitive dependency (nanort, now vendored) and the clean-clone bootstrap (the
build generates the demo meshes) closed in [20 #47](../20-public-delivery/01-pin-geometry-central.md#47-pin-geometry-central-to-upstream-own-nanort-self-bootstrapping-clone).
*Trigger:* the first external consumer that is not this repo.
*Source:* `docs/raw/study/B5-build-dependencies-licensing.md` §§ 2-10,
`docs/raw/study/T-unit-testing.md:462-463`.

## #34 API hygiene batch
**PARTIAL: DONE (2026-09-01, 2026-09-10); the rest PLANNED.** Two of the bullets have
landed — `FieldPtr` const-ness, and the `RepeatField` fold below, which turned out not to
be hygiene at all but the only wrong-**geometry** defect in the batch. Every claim in this
entry was re-verified against `ebe929a` before any of it was scoped, after
[#26](03-correctness-and-robustness/03-diagnostics-item.md#26-a-diagnostics-channel-for-the-silent-failure-surface)
found one of the audit's claims had never been true: **all of #34's held**. The list:

- ~~`FieldPtr` is `shared_ptr<ImplicitField>`, not `shared_ptr<const ImplicitField>`~~ —
  **DONE 2026-09-01**; the audit's "compiles unchanged" did **not** hold (four host-side
  `dynamic_cast` sites) — record: [08 § `FieldPtr`](08-argument-validation.md#the-earlier-batch-fieldptr-const-ness);
- `gradientAt` is documented as a gradient and returns a **unit direction**, which is why
  `NormalizedField` has to recompute the magnitude by finite differences — still open;
- ~~no argument validation anywhere~~ — **DONE 2026-09-10**: `std::invalid_argument` at
  construction, reported by `dualc_field` with the node's locator; three deliberate
  non-rejections and the full record: [**08**](08-argument-validation.md);
- ~~a default-constructed `BBox{}` reports `isValid()`~~ — **DONE 2026-09-10**, and it was
  **reachable** (both lift sources used `BBox{}` as their sentinel); `BBox::empty()` fills
  that role now — record: [08](08-argument-validation.md);
- ~~`RepeatField` uses the naive single-tile fold while `RepeatLimitedField`, in the same
  file, uses the corrected 8-neighbour minimum~~ — **DONE 2026-09-10**, and it was not a
  hygiene item: **wrong geometry in shipped strut lattices**, fixed conditionally at a
  measured **2.3×** on strut cells, handed to [#35](#35-repeatfield-neighbour-set-optimisation)
  — full record, including what it exposed in the test suite: [**07**](07-repeat-tiling-fix.md);
- **found while adding those guards: `scaled(f, s)` with negative `s` returned the
  complement of the solid** — fixed rather than rejected (a negative factor is a legitimate
  point reflection); record: [08](08-argument-validation.md);
- `ContourerParams` has no thread-count knob, so a caller restricting the sampler still gets
  all hardware threads in the contourer;
- `GridField::gradientAt` returns a normalised direction that is discontinuous across cell
  boundaries, which callers assuming a true gradient magnitude (such as
  `closestSurfacePoint`'s Newton step) silently rely on.

*(2026-09-18: the landed bullets are condensed to one line + link each — the removed
text is verbatim in 07 and 08, the `FieldPtr` paragraph in 08 — to bring this page under
the size cap; [19 Phase 5](../19-docs-layers/README.md).)*
*Source:* `docs/raw/study/B2-api-and-type-design.md:58,158-186`,
`docs/raw/study/A5-implicit-field-algebra.md:239-245,376-382`,
`docs/raw/study/B3-memory-ownership-performance.md:236-237`, `docs/raw/study/B4-concurrency.md:173`,
`docs/raw/study/A2-sampling-octree-oracles.md:385-391`.

## #35 `RepeatField` neighbour-set optimisation
**PLANNED.** [#34](#34-api-hygiene-batch)'s `RepeatField` fix costs **2.3×** on strut
lattices (39 s → 91 s, octet at depth 7) because a child that overhangs its period takes
eight child evaluations instead of one. The cost was accepted deliberately — correctness
first, with the number recorded rather than left for a user to discover — and this item is
the follow-up.

Two candidate levers, neither yet tried:
- **Per-axis neighbours.** Visit offsets only on axes where the child actually overhangs.
  Cheap and exact, but no help for the case that motivated it: a strut cell has corner caps
  and so overhangs on all three axes.
- **Early-out on the home copy.** Skip a neighbour when the home copy's value is already
  smaller than the closest the neighbour's translated box could possibly be. This is where
  the win would come from, since most points are nowhere near a seam.

A third, larger option is to make the *cells* fit their period, which would put strut
lattices back on the single-fold fast path by construction rather than by optimisation.
*Trigger:* a lattice contour where the 2.3× is felt — depth 8+, or the tiled export path.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
