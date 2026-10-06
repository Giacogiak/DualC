# Engineering-quality delivery records (#28, #30, #33)

The delivery records of the [04](04-engineering-quality.md) items that have no record
page of their own — #26 has [05](05-diagnostics-channel.md), #34 has [07](07-repeat-tiling-fix.md)
and [08](08-argument-validation.md). Each item's entry in 04 keeps the audit's claim, the
status line, what is still open and the trigger, and links here for what was delivered and
how it was verified.

*(Every paragraph below was moved here verbatim from 04 on 2026-09-18, when that page was
brought under the size cap — [19 Phase 5](../19-docs-layers/README.md).)*

## #28 — the two dead parameters, deleted

**DONE (2026-09-01).** Both fields **deleted** rather than kept as reserved no-ops — the audit's
argument is that a public field which silently does nothing is a documented promise the
library does not keep, and keeping it as a labelled no-op preserves exactly that. Removed
from `include/dualc/contourer.h`, `include/dualc/sampler.h` and the now-pointless
`(void)params.seed;` in `src/sampler.cpp`; the two rows are gone from
`docs/ARCHITECTURE.md` § 7's parameter tables and its § 9 entry is now a "fixed since"
note.

**API impact, stated plainly:** this is a **source-compatibility break**. Any caller
assigning `params.weldEdges` or `params.seed` no longer compiles. Neither field ever had
an effect, so **no behaviour changes** for anyone — the break is loud by design, which is
the point of deleting rather than deprecating: a silent no-op is what was wrong. Nothing
in this repo set either (`grep` across `include/`, `src/`, `examples/`, `capi/`, `tests/`:
nothing), and the C ABI never exposed them, so Boletus is unaffected.

*Verified:* `ctest` **231/231** after removal, and the build is clean of DualC-origin
warnings (the remaining C4100/C4305 come from geometry-central and polyscope headers).

## #30 — the determinism test

**DONE (2026-08-31).** `tests/test_parallel.cpp` (`[parallel][determinism]`) contours a torus
unioned with an off-centre sphere at `maxDepth 6` and requires the result to be **bit-identical**
-- not approximately equal -- across `numThreads` of 1, 2, 0 (all hardware threads, the
default every caller gets) and 64 (above the work-item count, exercising parallelFor's clamp).
Vertex count, face count, and every position and normal component are compared exactly:
289,426 assertions.

*Verified:* passes, and **also passes against the pre-fix library** -- which is the correct
result. The guarantee was already true; what was missing was anything holding it true. This
is a regression gate, not a bug fix.

## #33 — build hardening, three batches

**PARTIAL: DONE (2026-08-31, 2026-09-01, 2026-09-10); the rest DEFERRED** — the open list
and the trigger are on the [04 entry](04-engineering-quality.md#33-build--tooling-hardening).

**Delivered (2026-08-31).** Four one-liners, all verified by a clean configure + full build:
- the sibling probes resolve against `CMAKE_CURRENT_SOURCE_DIR`, not `CMAKE_SOURCE_DIR`
  (`CMakeLists.txt:48`, `:60`), so they still point next to DualC under `add_subdirectory`;
- `DUALC_BUILD_TESTS` / `DUALC_BUILD_EXAMPLES` default to `DUALC_IS_TOP_LEVEL` instead of
  `ON`, so a consumer no longer inherits a Catch2 network fetch and 21 test TUs.
  `PROJECT_IS_TOP_LEVEL` needs CMake 3.21 and this project supports 3.14, so it is used when
  available with a source-dir comparison as the fallback;
- Catch2 is pinned to commit `b5373dadca40b7edc8570cf9470b9b1cb1934d40` rather than the
  mutable tag `v3.5.4`. The SHA was **verified locally**, not looked up: the fetched tree's
  `catch_version_macros.hpp` reports 3.5.4 and its `v3.5.4` tag resolves to this commit.
  `GIT_SHALLOW` was deliberately **not** enabled -- it is unreliable when `GIT_TAG` names a
  commit hash rather than a ref, and breaking the fetch would cost more than it saves;
- `CMAKE_EXPORT_COMPILE_COMMANDS` is on when top-level (only then: a parent project owns that
  setting otherwise).

**Follow-up (2026-09-01): the same defect in the subdirectories.** The 2026-08-31 pass fixed
the two sibling probes but left `${CMAKE_SOURCE_DIR}` in the subdirectory lists -- **22 more
occurrences**, not the six first reported: 17 in `tests/CMakeLists.txt`, 4 in
`examples/CMakeLists.txt` and 1 in `capi/CMakeLists.txt`, covering compiled-in example
sources, include directories, per-source compile options and the demo-mesh copy commands.
All now use `${PROJECT_SOURCE_DIR}`, which resolves to DualC's own root (the nearest
enclosing `project()`) rather than the top-level build's, so every path survives
`add_subdirectory`. The one deliberate remaining `CMAKE_SOURCE_DIR`, in the root
`CMakeLists.txt`, is the top-level-detection fallback, where comparing against the global
source dir is exactly the intent. Verified by a clean configure + full build + `ctest`
**231/231**.

**Follow-up (2026-09-10): the local checks gate** — [09](09-local-checks-gate/README.md).
`python scripts/check.py` is the one command; a `pre-commit` hook runs its fast tier. This
item's trigger was "CI being set up"; for everything not needing a GPU it now is.
**No sub-item below closes** — the gate is where they will run.

**Follow-up (2026-09-21): two sub-items close from block 20.** The undeclared nanort became a
compile failure against upstream geometry-central v1.1.0 and is now a vendored header; the
clean-clone bootstrap is a build-time `dualc_gen_demo` run. Record: [20 #47](../20-public-delivery/01-pin-geometry-central.md#47-pin-geometry-central-to-upstream-own-nanort-self-bootstrapping-clone).

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
