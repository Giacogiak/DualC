# B5 · Build, dependencies and licensing

> **In one paragraph.** DualC is a 132-line root `CMakeLists.txt` with nine knobs, a static library whose entire link line is `geometry-central`, and a hard rule that every host-side dependency — miniz, stb, nlohmann/json, meshoptimizer, GLFW, polyscope — lives above the library in the examples tree and is never linked into it. Dependencies arrive three ways: a sibling checkout (geometry-central, polyscope), verbatim vendoring under a permissive licence, and exactly one `FetchContent` (Catch2, tests only). The licensing posture is genuinely strong: MIT throughout, PD/MIT/BSD only in-tree, and the LGPL octree code from the same upstream project as the vendored QEF was deliberately re-derived rather than copied, precisely so the kernel can be embedded in a closed-source CAD plugin. The weak points are equally concrete: no `install()` rules so the library is `add_subdirectory`-only, a `CMAKE_SOURCE_DIR` that breaks even that, a clean clone that does not build because the demo meshes are gitignored, and a CPU-vs-GPU formula parity gate that is opt-in and outside CTest.

**Read this after:** B1 · Architecture and module boundaries   **Time:** 45 min

## 1. The option surface

Root `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.14)`, `project(dualc VERSION 0.3.0 LANGUAGES CXX)`.

| Option / cache variable | Default | Line |
| --- | --- | --- |
| `DUALC_BUILD_TESTS` | **ON** | `CMakeLists.txt:4` |
| `DUALC_BUILD_EXAMPLES` | **ON** | `CMakeLists.txt:5` |
| `DUALC_BUILD_POLYSCOPE_VIEWER` | OFF | `CMakeLists.txt:6-8` |
| `DUALC_BUILD_RAYMARCH_VIEWER` | OFF | `CMakeLists.txt:9-11` |
| `DUALC_BUILD_FIELD_VIEW` | OFF | `CMakeLists.txt:12-14` |
| `DUALC_BUILD_GLSL_PARITY` | OFF | `CMakeLists.txt:15-17` |
| `DUALC_BUILD_C_ABI` | OFF | `CMakeLists.txt:18-20` |
| `DUALC_GC_DIR` (`CACHE PATH`) | `${CMAKE_SOURCE_DIR}/../geometry-central` | `CMakeLists.txt:27-28` |
| `DUALC_POLYSCOPE_DIR` (`CACHE PATH`) | `${CMAKE_SOURCE_DIR}/../polyscope` | `CMakeLists.txt:39-40` |

Every option string is a sentence, not a label — `DUALC_BUILD_GLSL_PARITY` describes itself as "GPU vs C++ per-node acceptance gate. Needs a GL context." Small thing, but it is the difference between `cmake -LH` being useful and being noise.

Language setup is belt and braces. The globals `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` (`CMakeLists.txt:22-24`) fix the dialect and switch off GNU/MSVC extensions, *and* every target additionally carries `target_compile_features(... cxx_std_17)` — on `dualc` it is `PUBLIC` (`:96`) so the requirement propagates to consumers, on executables `PRIVATE`.

The wart is where the globals sit: line 22, *before* `add_subdirectory` of geometry-central at line 34 and polyscope at line 52, so DualC's dialect choice leaks into two third-party trees that never asked for it. Both happen to be C++17-clean, so nothing breaks — but this is the mechanism that bites when an upstream needs C++14 or relies on an extension. The clean form is to drop the globals and rely on `target_compile_features`, which is per-target by construction.

## 2. The dependency graph

![The dependency layering: libdualc links only geometry-central; examples, viewers and the C ABI sit above it](figures/fig-layers.svg)

*Look at how thin the bottom layer is: everything with a file format, a window or a JSON parser sits above the library, not inside it.*

```
geometry-central ──PUBLIC──► dualc (STATIC)  [alias dualc::dualc]
                              │  PUBLIC  include/   (BUILD_INTERFACE + INSTALL_INTERFACE)
                              │  PRIVATE src/
                              ├─► dualc_examples_common ──PUBLIC──► dualc_examples_io (miniz, MIT)
                              │                          ──PUBLIC──► dualc_examples_decimate (meshoptimizer, MIT)
                              │        └─► dualc_primitive, dualc_boolean, dualc_lift,
                              │            dualc_csg_demo, dualc_lattice, dualc_slice
                              │            (dualc_slice additionally ──► dualc_examples_img (stb, PD))
                              ├─► dualc_examples_fieldgraph (nlohmann/json) ──► dualc_field
                              │        └─► dualc_examples_fieldglsl  [GL-FREE by design]
                              ├─► dualc_demo_meshes ──► dualc_gen_demo
                              ├─► dualc_demo
                              ├─[gated]─► dualc_examples_gl (raymarch_gl.cpp) ──► glfw, glad, stb
                              │        ├─[RAYMARCH]──► dualc_raymarch
                              │        ├─[FIELD_VIEW]► dualc_field_view
                              │        └─[GLSL_PARITY]► dualc_glsl_parity
                              ├─[POLYSCOPE]─► dualc_view ──► polyscope
                              ├─[TESTS]─► dualc_tests ──► Catch2::Catch2WithMain
                              └─[C_ABI]─► capi/  (hard-requires EXAMPLES)
```

The headline fact is one line of CMake: `target_link_libraries(dualc PUBLIC geometry-central)` (`CMakeLists.txt:95`). That is the whole link line — twenty-four source files (`:55-83`), of which two are vendored, and one dependency. Everything else in the diagram hangs off targets *above* `dualc`, and none of them can reach down into it.

The include interface is equally disciplined: `include/` is `PUBLIC` (behind a `BUILD_INTERFACE`/`INSTALL_INTERFACE` pair) and `src/` is `PRIVATE` (`:86-90`), so consumers physically cannot `#include "internal/qef.h"`. That is the CMake half of the public/internal split in **B1 · Architecture and module boundaries**; the C++ half is the `dualc::internal` namespace.

What the graph inherits silently is geometry-central's own dependencies. The comment at `:92-94` is honest about it: "Linking geometry-central transitively pulls Eigen, nanort, nanoflann, happly (they are PUBLIC INTERFACE deps inside geometry-central's build)". Convenient, but it makes nanort — which the private BVH header depends on structurally, not incidentally — an **undeclared transitive dependency**. If geometry-central drops it or makes it `PRIVATE`, DualC stops compiling for a reason no line of DualC's CMake mentions.

## 3. The sibling checkout, and why it is a locked decision

```cmake
# CMakeLists.txt:27-34
set(DUALC_GC_DIR "${CMAKE_SOURCE_DIR}/../geometry-central" CACHE PATH ...)
if(NOT EXISTS "${DUALC_GC_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "Could not find geometry-central at ${DUALC_GC_DIR}. "
    "Clone it as a sibling directory or set -DDUALC_GC_DIR=/path/to/geometry-central.")
endif()
add_subdirectory("${DUALC_GC_DIR}" geometry-central-build)
```

Three good details in eight lines: the probe is for `CMakeLists.txt`, not bare directory existence (which would pass on an empty clone); the `FATAL_ERROR` gives both remedies rather than saying "not found"; and the explicit binary directory is required because the source lies outside the tree.

The rationale is a locked project decision (`CLAUDE.md`): "geometry-central is a sibling, not vendored — no FetchContent, no submodule for it." The defence is that geometry-central is **co-developed** here, not merely consumed. `FetchContent` would pin a revision and roughly double clean-configure build time; a submodule would fight local edits on every `git status`. A sibling checkout is the only arrangement where "edit geometry-central, rebuild DualC" is one build.

Three real attacks, in order of severity.

**(a) It uses `CMAKE_SOURCE_DIR`, not `CMAKE_CURRENT_SOURCE_DIR`.** Both defaults (`:27`, `:39`) resolve against the *top-level* project's root. Add DualC via `add_subdirectory(external/dualc)` and both point at `<outer>/../geometry-central`, which is almost certainly wrong. So **DualC cannot be `add_subdirectory`'d into a larger project without overriding the cache variables by hand** — unfortunate, because `add_subdirectory` is the *only* supported consumption mode (§10).

**(b) No `find_package` fallback.** A system or vcpkg geometry-central cannot be used. The roadmap records why this is not trivial: geometry-central ships no `geometrycentralConfig.cmake` and no namespaced alias (`docs/roadmap/10-infrastructure-and-integration.md`, item 8), so the fallback needs a DualC-shipped Find module.

**(c) Transitive dependencies are inherited implicitly** — the nanort point above.

Worth crediting on the other side: the polyscope block (`:39-53`) is materially better than the geometry-central one. Four flags can require polyscope, and the guard at `:41-42` ensures `add_subdirectory` runs **exactly once** whatever combination is on — double-`add_subdirectory` of one source tree is a real and confusing bug class, avoided by construction. The error message (`:44-50`) names all four flags, states the probed directory, and hands you the literal `git clone --recurse-submodules https://github.com/nmwsharp/polyscope ...` line — `--recurse-submodules` because polyscope's GLFW/glad/ImGui *are* submodules and a plain clone fails later and more confusingly. Polyscope is also added `EXCLUDE_FROM_ALL` (`:52`).

## 4. Warning configuration: nine lines, three good decisions

```cmake
# CMakeLists.txt:98-102
if(MSVC) target_compile_options(dualc PRIVATE /W4 /permissive-)
else()   target_compile_options(dualc PRIVATE -Wall -Wextra -Wpedantic) endif()

# CMakeLists.txt:106-116 — vendored Unlicense code quieted per-source, not globally
set_source_files_properties(src/internal/qef.cpp src/internal/svd.cpp
  PROPERTIES COMPILE_OPTIONS "/W3;/wd4458;/wd4505")   # or "-w" on non-MSVC
```

Three decisions worth naming, because each has a common wrong alternative:

- **`PRIVATE`, not `PUBLIC`.** Warning flags are a project's own quality bar, not part of its interface. A `PUBLIC /W4` propagates into every consumer's build and drowns *their* diagnostics in warnings about *their* code. This is one of the most common CMake mistakes in the wild.
- **`/permissive-`.** MSVC's real conformance mode: two-phase name lookup in templates, no binding of non-const references to temporaries, correct `for`-loop scoping. Adding it to a codebase that already compiles is usually a day of work, and it is the difference between "compiles on MSVC" and "is C++".
- **Per-source suppression.** The two vendored Unlicense files are quieted with `set_source_files_properties`, not by lowering the project-wide level, so `/W4` stays meaningful for DualC's own twenty-two sources. The alternative — dropping to `/W3` globally because the vendored code is noisy — is how a warning budget dies.

The same pattern recurs: miniz, stb and the two meshoptimizer TUs get `/W0` or `-w` per source in the examples (`examples/CMakeLists.txt:44-56`) and again in the tests (`tests/CMakeLists.txt:63-76`). `nlohmann/json` is pragma-suppressed at its single include site instead (`examples/CMakeLists.txt:84-85`) — right for a header-only library that would otherwise need suppression at every consumer.

**What is missing:** no `-Werror`/`/WX`, so warnings accumulate silently; no sanitiser configuration (`-fsanitize=address,undefined` behind an option is cheap and is exactly what a QEF solving near-degenerate 3×3 systems wants); no `clang-tidy` via `CXX_CLANG_TIDY`; no IPO/LTO despite this being a compute kernel; and no `CMAKE_EXPORT_COMPILE_COMMANDS`, so clangd needs it passed by hand.

## 5. Opt-in gating, and the linker war story

The gating pattern is uniform: an `option()` at the root, an `if(OPTION)` in `examples/CMakeLists.txt` wrapping `add_executable` + `target_link_libraries`, and a comment stating what the flag costs. `dualc_view` (`:135-139`), `dualc_raymarch` (`:165-169`), `dualc_field_view` (`:175-180`) and `dualc_glsl_parity` (`:185-190`) each sit behind their own flag, and the comment at `:131-134` states the invariant that makes the scheme safe: "The library binary is unaffected when the flag is off — nothing here changes the `dualc` target's link line."

The interesting decision — worth telling as a story, because it shows the target graph encoding a real constraint rather than taste — is the **stb/miniz split** (`examples/CMakeLists.txt:11-19`):

> `stb_image_write.h` — Public Domain (PNG heatmaps). Used ONLY by `dualc_slice`, so it lives in its OWN lib and is NOT pulled into `dualc_examples_common` — otherwise `dualc_view` would link both this and polyscope's vendored stb, multiply-defining the `stbi_write_*` symbols (LNK2005).

So `dualc_examples_io` (miniz, needed by every CLI's `-o .3mf` path) is `PUBLIC`-linked into the shared common library (`:62-64`), while `dualc_examples_img` (stb) is a separate library attached to exactly one target, `dualc_slice` (`:76`). The split is not tidiness; collapsing them produces a duplicate-symbol link failure in an unrelated binary.

The same reasoning is applied *in reverse* at `:150-154`: the GL support library `dualc_examples_gl` **does** `PUBLIC`-link stb, and the comment says why that is safe — "none of these targets link polyscope, so there is no second stb to collide with (the LNK2005 that bites `dualc_view`)". That is a rule understood rather than cargo-culted: applied where the condition holds, relaxed where it does not.

Two more decisions in the same file:

- **`dualc_examples_fieldglsl` deliberately carries no GL dependency** (`:97-105`): "it carries no GL dependency — the GL apps below assemble its output — so it always builds, catching codegen regressions even in a GL-less configuration." The codegen produces *text*; keeping it GL-free means `test_field_glsl.cpp` runs in any configuration. Good instinct; §9 explains why it does not go far enough.
- **Option interdependency is enforced** (`CMakeLists.txt:125-132`): `DUALC_BUILD_C_ABI` without `DUALC_BUILD_EXAMPLES` is a `FATAL_ERROR` whose message states the *reason* — "the C ABI links the host-side `dualc_examples_fieldgraph` + `dualc_examples_common` libraries" — not a bare "invalid combination".

## 6. Test wiring

Catch2 v3.5.4 by `FetchContent` (`tests/CMakeLists.txt:1-8`) is the **only** fetched dependency in the project. `catch2_SOURCE_DIR/extras` is appended to `CMAKE_MODULE_PATH` (`:10`) so `include(Catch)` finds `catch_discover_tests(dualc_tests)` (`:78-80`), which registers each `TEST_CASE` as its own CTest entry rather than one lump target — you get per-case pass/fail and `ctest -R` filtering for free.

Two details worth defending. The test binary **compiles the host-side example sources directly in** — `demo_meshes.cpp`, `field_graph.cpp`, `field_glsl.cpp`, `example_common.cpp`, plus the vendored miniz and meshoptimizer TUs (`tests/CMakeLists.txt:34-48`) — for the stated reason "so this does not depend on `DUALC_BUILD_EXAMPLES` being on". Tests stay orthogonal to an unrelated option and get direct access to the registries that are the single source of truth for the CLIs. And `target_include_directories(dualc_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ...)` (`:57-61`) grants white-box access to `src/internal/` without widening the public interface by one header. What the tests actually assert is **T · Unit testing**'s topic.

Two criticisms:

- **`GIT_TAG v3.5.4` is a mutable tag, not a commit SHA.** No `GIT_SHALLOW`, no `FIND_PACKAGE_ARGS` fallback to a system Catch2, no offline path. Configuring DualC therefore **requires network access**, and the build is not reproducible against an upstream that re-tags. CMake ≥ 3.24's `FIND_PACKAGE_ARGS` makes the fallback a one-liner; pinning the SHA makes it correct.
- **`DUALC_BUILD_TESTS` defaults ON** (`CMakeLists.txt:4`), so any subproject consumer triggers that fetch and builds 21 test TUs it did not ask for. The idiom since CMake 3.21 is `option(... ${PROJECT_IS_TOP_LEVEL})`; likewise for `DUALC_BUILD_EXAMPLES`.

## 7. Licensing and vendoring policy

DualC's own code is MIT (`LICENSE`, `THIRD_PARTY.md:101`). The in-tree policy is stated in `CLAUDE.md` and is three clauses: third-party code vendored in-tree must be **PD/MIT/BSD**; **no GPL/LGPL copies**; **prefer re-implementation over copying restrictive code**.

The proof that the policy is real, not aspirational, is what was *not* copied. The QEF and SVD solvers in `src/internal/{qef,svd}.{h,cpp}` are vendored **byte-identical** from Nick Gildea's DualContouringSample under the Unlicense, with the original licence headers preserved in each file (`src/internal/qef.cpp:25`, `src/internal/svd.h:25`). But the **octree code from that same project is LGPL**, and it was deliberately left alone. `docs/ARCHITECTURE.md:236`: "We deliberately did **not** vendor the LGPL-licensed octree code from the same project (or from Tao Ju's reference) — those are read-only references. The DC topology tables, octree representation, and triangle emission logic are re-derived in this codebase to keep DualC permissively licensed." The roadmap agrees (`docs/roadmap/01-core-dual-contouring.md:16`): "we re-derived from cube combinatorics rather than copy."

That is a costly decision — re-deriving the DC tables from cube combinatorics is the fiddly part of the algorithm (**A4 · The contouring recursion**) — taken for a licensing reason. It is the single most convincing thing in the dependency story.

`THIRD_PARTY.md` splits the inventory into honest categories:

| Category | Items | Licence |
| --- | --- | --- |
| Vendored into `libdualc` | `qef.{h,cpp}`, `svd.{h,cpp}` | Unlicense (PD) |
| Vendored, host-only (`examples/third_party/`) | miniz 3.0.2, `stb_image_write.h`, `json.hpp` 3.11.3, meshoptimizer 1.2 subset | MIT / PD / MIT / MIT |
| Opt-in, via the sibling polyscope checkout | GLFW, glad, Dear ImGui, glm | zlib / PD / MIT / MIT |
| Transitive, via geometry-central | Eigen, nanort, nanoflann, happly | MPL-2.0 / MIT / BSD-2 / MIT |
| Reference only, no code copied | PicoGK (TPMS conventions) | Apache-2.0 |

Two things to notice. First, the transitive row is *listed*, with licences — Eigen's MPL-2.0 is the only weak-copyleft item anywhere near the project, it is file-level, satisfied by not modifying Eigen, and it arrives through a linked dependency rather than a copy. Second, the reference-only row is visible **in a source comment**, not just a licence file: `src/implicit/primitives_tpms.cpp:11` reads "conventions cross-checked against PicoGK's TPMS module (Apache-2.0, LEAP 71 — reference only, no code copied)." That is a developer recording, at the point of temptation, that they read a competitor's implementation and did not copy it — exactly the note that matters if provenance is ever questioned.

One honest caveat, also recorded: `data/bunny.obj` is the Stanford Bunny, carrying a request-for-acknowledgment term (`THIRD_PARTY.md:84-99`). It is data, not linked code, and it is gitignored rather than committed.

**Why this matters commercially.** A permissively-licensed geometry kernel can be statically linked into a closed-source CAD plugin — a Rhino `.gha`, a SolidWorks add-in, a proprietary slicer — with no obligation beyond attribution. An LGPL kernel cannot, or not without dynamic linking, shipping relinkable object files, and a legal review of whether static linking into a plugin loaded by a proprietary host counts as combination. Deciding that up front, and paying for it by re-deriving the octree, is what makes "can we embed this in a product" a five-minute conversation.

## 8. The C ABI boundary

`DUALC_BUILD_C_ABI=ON` builds `capi/`, an opt-in **shared** library linking `libdualc` plus the host-side static libraries `dualc_examples_fieldgraph` and `dualc_examples_common` (`CMakeLists.txt:125-132`). It is deliberately **not** part of `libdualc`, for the library-scope reason in **B1**: the ABI takes a field-graph *JSON string* and writes *files*, so it needs a JSON parser and mesh I/O — both things the kernel refuses to own. `docs/roadmap/10-infrastructure-and-integration.md` records the deviation explicitly: it was sketched as `src/c_abi/` + `include/dualc_c.h` and moved to `capi/` once it became clear it must link host libraries.

The surface is **eleven flat `extern "C"` entry points** over an opaque `DualcFieldHandle`: `dualc_version`, `dualc_default_params`, `dualc_field_create_from_json`/`_from_expr`, the two `_with_meshes` twins, `dualc_field_destroy`, `dualc_field_contour`, `dualc_mesh_release`, `dualc_field_export`, `dualc_field_export_tiled_stl`. The field-graph string *is* the construction API, so there is no per-primitive factory wall to maintain. No C++ exception may cross the boundary — errors are integer codes plus an `err` buffer carrying a JSON-pointer/char-offset locator. Meshes are ABI-allocated and caller-released via `dualc_mesh_release`, avoiding the cross-heap free hazard. A **C-compiled demo** is built alongside, whose real job is to prove the header is C-clean rather than C++ that happens to parse.

Why it exists: it is the boundary a Rhino/Grasshopper plugin crosses, since `std::shared_ptr<ImplicitField>`, `std::unique_ptr<SurfaceMesh>`, exceptions and templates do not survive P/Invoke. The in-memory mesh-source resolver added at v0.3.0 — pass `DualcMeshSource` buffers to `*_with_meshes` and reference them by id as `mesh(id="…")` — exists specifically because the non-owning-mesh lifetime hazard described in **B2 · API and type design** is unmanageable across a plugin boundary, where the host's geometry lives in RAM and never touches disk.

## 9. The CPU/GPU duplication problem

This is the most substantive architectural criticism available against the project, and the project already knows it. **Every SDF formula exists twice**: once in `src/implicit/*.cpp` in `double`, once as a GLSL prelude helper in `examples/field_glsl.cpp` in `float`. The header says so (`examples/field_glsl.h:33-38`): "the codegen accepts the SAME node vocabulary as the contour path… Each prelude helper is bit-faithful to its `src/implicit/` formula (gated by `dualc_glsl_parity`)."

**The defence, made properly.** A single source is not actually available. The CPU path must be `double` — the QEF solves near-degenerate 3×3 systems where a truncated pseudo-inverse is already fighting conditioning (**A3 · The QEF**), and `float` there is a correctness loss, not a performance trade. The GPU path must be `float`, because the target is WebGL2 `#version 300 es` (`field_glsl.h:40-41`), where `double` does not exist. A macro- or template-shared formula would force one precision on both. So the choice is between *not having a GPU previewer* and *duplicating the formulas*. Given duplication, the right move is to make equivalence a **testable, enumerable property** — which is what `dualc_glsl_parity` is: per node, render `sceneSDF` over a lattice on the GPU and compare against the C++ field, currently **62/62 passing** (`CLAUDE.md`). That is the standard answer to CPU/GPU parity, and 62 node-level comparisons is a real gate, not a token one.

**Now concede the sharp part.** `DUALC_BUILD_GLSL_PARITY` defaults **OFF** (`CMakeLists.txt:15-17`) and deliberately has **no CTest** because it needs a GL context (`examples/CMakeLists.txt:182-184`: "Needs a GL context (a hidden window), so — like the viewers — no CTest; run manually"). The only thing preventing two sources of truth from diverging is therefore a manually-run, opt-in binary. Edit `smoothMinValue` in `src/implicit/combinators.cpp` and you get a green build and a green `ctest` with a silently diverged previewer — and the previewer's whole value proposition is "the previewed field is the exported field".

Two nameable fixes. **(1)** A headless-GL CI job — llvmpipe/OSMesa on Linux, SwiftShader anywhere — running `dualc_glsl_parity`; the harness already has a hidden `--snapshot` single-frame path, so it is designed for scripting. **(2)** A CPU reference evaluator for the emitted AST, making parity testable with **no GL at all**. Half of (2) is already done: `dualc_examples_fieldglsl` is deliberately GL-free (`examples/CMakeLists.txt:97-105`), so the codegen is already linkable and testable in a GL-less configuration; what is missing is an interpreter for its output.

## 10. The build's real weak points, ranked

1. **No `install()` rules, no export set, no `dualcConfig.cmake`, no `CPack`** — and yet `target_include_directories` declares `$<INSTALL_INTERFACE:include>` (`CMakeLists.txt:88`), a generator expression that is **dead** without `install(TARGETS dualc EXPORT dualcTargets)`. So the library is consumable only by `add_subdirectory`, which `CMAKE_SOURCE_DIR` (§3) also breaks. This is the single clearest build-system criticism. The mitigating fact is that the roadmap records it as deferred with a reason: geometry-central ships no Config file, so `find_dependency(geometrycentral)` inside a `dualcConfig.cmake` cannot work without a DualC-shipped Find module (`docs/roadmap/10-infrastructure-and-integration.md`, item 8).
2. **A clean clone does not build.** `examples/CMakeLists.txt:197-207` copies nine `.obj` demo meshes POST_BUILD for eight binaries with `cmake -E copy_if_different`, and the comment admits "Mesh files are gitignored (`*.obj`), so they must already exist under `data/` — run `dualc_gen_demo all --dir <repo>/data` once after checkout". `copy_if_different` on a missing source **fails**, so the POST_BUILD step fails, so the build fails — and the chicken-and-egg is that `dualc_gen_demo`, the thing that produces those meshes, is one of the targets being built. Two fixes: `if(EXISTS ...)` guards, so a fresh clone builds and only the smoke tests fail until you generate; or better, `add_custom_command(OUTPUT data/sphere.obj ... DEPENDS dualc_gen_demo)`, putting the dependency in the graph so CMake orders it for you.
3. **`DUALC_BUILD_TESTS` and `DUALC_BUILD_EXAMPLES` default ON** (`CMakeLists.txt:4-5`), imposing a network fetch and 21 test TUs on any consumer. `${PROJECT_IS_TOP_LEVEL}` is the fix.
4. **The parity gate is outside CTest** (§9) — defensible for the three interactive viewers, which need a human; not defensible for `dualc_glsl_parity`, the only guard on the duplicated formulas.
5. **`CMAKE_CXX_STANDARD` and friends set globally before two third-party `add_subdirectory` calls** (§1).

A sixth, structural rather than mechanical: the "no host deps in `libdualc`" rule is enforced by **comments and discipline only**. Nothing in CMake stops `target_link_libraries(dualc PRIVATE dualc_examples_io)`. A CI assertion on `dualc`'s `LINK_LIBRARIES` property — or simply not defining the host libs when `DUALC_BUILD_EXAMPLES` is off — would make the architecture's most important rule structural rather than cultural.

## 11. What is genuinely good

Lead with these, because a senior reviewer recognises them as experience rather than tidiness:

- **Option→target gating is clean and every gate has a documented reason** — no flag exists without a comment saying what it costs and what it does not touch (`examples/CMakeLists.txt:131-134`).
- **The `FATAL_ERROR` messages are actionable**: probed path, override variable, and the literal command to fix it (`CMakeLists.txt:29-33`, `:44-50`, `:127-129`).
- **Warnings are `PRIVATE`, strict, and suppressed per-source** (`CMakeLists.txt:98-116`).
- **The stb/miniz split encodes a real linker constraint** — and is correctly relaxed where it does not apply (`examples/CMakeLists.txt:11-19` vs `:150-154`).
- **The codegen library is deliberately GL-free**, so it always builds (`examples/CMakeLists.txt:97-105`).
- **The CLI smoke tests explain the *numerical* choice of each parameter.** The onion wall is 0.35 because it is "kept comfortably thicker than the depth-6 cell so the shell resolves into a clean manifold (a thin wall under-resolved at low depth produces non-manifold junk — one-crossing-per-edge limit)"; the twist is about x, "NOT the torus's y symmetry axis — a twist about a body-of-revolution's own axis is a no-op" (`examples/CMakeLists.txt:241-246`); every lattice test states why its wavelength and offset resolve at the chosen depth (`:266-268`). That is a build and test suite written by someone who understood why each case was fragile — the difference between a smoke test that catches regressions and one that passes for the wrong reason.

## Key terms

| Term | Meaning |
| --- | --- |
| Sibling checkout | A dependency cloned next to the project and pulled in with `add_subdirectory` against an absolute path, rather than fetched, vendored or found. |
| `FetchContent` | CMake's download-and-`add_subdirectory` mechanism. Used here for Catch2 only. |
| Vendoring | Copying third-party source into the tree. DualC vendors two files into the library and four into the examples. |
| Transitive dependency | A dependency inherited through a linked target's `PUBLIC`/`INTERFACE` properties — here Eigen, nanort, nanoflann and happly via geometry-central. |
| `PRIVATE`/`PUBLIC`/`INTERFACE` | CMake usage requirements. `PRIVATE` applies to the target only; `PUBLIC` also propagates to consumers; `INTERFACE` propagates only. |
| `$<BUILD_INTERFACE>` / `$<INSTALL_INTERFACE>` | Generator expressions selecting an include path for the in-tree build versus an installed copy. The latter is inert without `install()` rules. |
| Export set / Config package | `install(TARGETS ... EXPORT ...)` plus a generated `<pkg>Config.cmake` — what makes `find_package` work. Absent here. |
| `catch_discover_tests` | Catch2 helper that runs the test binary at build time to register each `TEST_CASE` as its own CTest entry. |
| `/permissive-` | MSVC's conformance mode: two-phase lookup, stricter reference binding, correct scoping. |
| Unlicense | A public-domain dedication with a permissive fallback. The QEF/SVD files carry it. |
| Weak copyleft (LGPL/MPL) | Licences imposing relinking or file-level source obligations. Avoided in-tree; MPL-2.0 Eigen arrives only transitively. |
| Parity gate | `dualc_glsl_parity` — per-node GPU-vs-CPU comparison, 62/62 passing, opt-in and outside CTest. |

## If they ask…

**"How do I consume this library?"**

Honestly: `add_subdirectory` only. `CMakeLists.txt:84` provides the namespaced alias `dualc::dualc` and `:86-90` sets `include/` as a `PUBLIC` `BUILD_INTERFACE` path, so linking works inside a source tree. There are no `install()` rules and no `dualcConfig.cmake`, so `find_package(dualc)` does not work — the `$<INSTALL_INTERFACE:include>` at `:88` is a placeholder that is inert without them. Worse, the geometry-central default at `:27` uses `CMAKE_SOURCE_DIR`, so even `add_subdirectory` from a parent project resolves the sibling path against the *outer* root and you must pass `-DDUALC_GC_DIR` by hand. The blocker on fixing it properly is documented: geometry-central ships no Config package or namespaced alias, so `find_dependency(geometrycentral)` cannot resolve without DualC shipping a Find module. I would take the `CMAKE_CURRENT_SOURCE_DIR` fix today and treat the Config package as a half-day once there is a consumer.

**"Why is geometry-central a sibling checkout rather than a submodule or FetchContent?"**

Because it is co-developed, not merely consumed. A submodule pins a SHA and fights every local edit — constant pointer bumps and detached-HEAD surgery. `FetchContent` pins a revision and roughly doubles clean-tree build time. The sibling checkout is the only arrangement where "edit geometry-central, rebuild DualC" is one build. The mechanism is defensive: it probes for `CMakeLists.txt` rather than a bare directory, and the `FATAL_ERROR` gives both remedies (`CMakeLists.txt:29-33`). The cost I would concede is that there is no `find_package` fallback, and that Eigen, nanort, nanoflann and happly are inherited implicitly (`:92-94`) — which makes nanort an undeclared dependency of the private BVH header.

**"Why vendor a QEF solver rather than use Eigen, which you already have transitively?"**

Three reasons. It is the *reference* implementation — Nick Gildea's DualContouringSample is what the DC literature's tuning constants refer to, and DualC uses them verbatim (`include/dualc/contourer.h:19`, "Matches Nick Gildea's reference"), so re-implementing would mean re-tuning the pseudo-inverse threshold from scratch. It is the Unlicense, so vendoring costs nothing legally. And byte-identical vendoring is deliberate policy: four files dropped in unchanged, original headers preserved, so an upstream diff is trivial. Eigen's `JacobiSVD` would work numerically, but you would be swapping a known-good 3×3 solver whose failure modes are documented in the literature for one whose behaviour on this rank-deficient problem you would have to re-establish. The more interesting half is what was *not* vendored: the octree code in that same repository is LGPL and was deliberately re-derived from cube combinatorics rather than copied, to keep DualC permissive (`docs/ARCHITECTURE.md:236`).

**"Is this safe to ship in a commercial, closed-source product?"**

Yes, and by design rather than luck. DualC's own code is MIT. The only code vendored into the library is the Unlicense QEF/SVD pair, which is public domain. The host-side vendored libraries — miniz, stb, nlohmann/json, meshoptimizer — are MIT or PD and never link into `libdualc` anyway. The only weak-copyleft item anywhere is Eigen (MPL-2.0), which arrives transitively, is file-level rather than viral, and is satisfied by not modifying Eigen. The policy is explicit — PD/MIT/BSD in-tree, no GPL/LGPL copies, prefer re-implementation — and it was paid for by re-deriving the LGPL octree. One caveat to state proactively: `data/bunny.obj` carries a request-for-acknowledgment term, but it is data, not linked code, and it is gitignored (`THIRD_PARTY.md:84-99`).

**"You have the same mathematics written twice, in C++ and GLSL. How do you stop them diverging?"**

With `dualc_glsl_parity`, which renders each node's `sceneSDF` over a lattice on the GPU and compares against the C++ field — 62 node-level comparisons, all passing. The duplication is forced, not chosen: the CPU path must be `double` because the QEF solves near-degenerate systems, and the GPU path must be `float` because the target is WebGL2 where `double` does not exist, so no macro or template can share one formula. Given that, the correct move is to make equivalence an enumerable, testable property. **But I have to concede the gap**: `DUALC_BUILD_GLSL_PARITY` defaults OFF (`CMakeLists.txt:15-17`) and has no CTest because it needs a GL context (`examples/CMakeLists.txt:182-184`), so the gate is manual — edit a smooth-min formula and you get a green build and a green `ctest` with a silently diverged previewer. The fixes are a headless-GL CI job on llvmpipe or SwiftShader, or a CPU reference evaluator for the emitted AST so parity needs no GL at all; half of the second is already done, since the codegen library is deliberately GL-free.

**"What would you fix in the build first?"**

The clean-clone failure, because it is the first impression and it is cheap. The demo `.obj` meshes are gitignored, the examples copy them POST_BUILD with `copy_if_different`, and that fails on a missing source — while the generator that produces them is one of the targets being built (`examples/CMakeLists.txt:192-207`). An `if(EXISTS)` guard fixes it in five minutes; a custom command with `DEPENDS dualc_gen_demo` fixes it properly. Second, `CMAKE_SOURCE_DIR` → `CMAKE_CURRENT_SOURCE_DIR` at `:27` and `:39`, a one-word edit that unblocks sub-project consumption. Third, flipping the `TESTS`/`EXAMPLES` defaults to `${PROJECT_IS_TOP_LEVEL}`. Then the parity gate into CI, and `install()` rules once a consumer needs them.

**"What is missing from the compiler configuration?"**

No `-Werror`/`/WX`, so warnings can accumulate silently even under `/W4`. No sanitiser option — the omission I would fix first, since ASan/UBSan over the QEF and SVD paths, where near-degenerate 3×3 systems are routine, is exactly the coverage that finds a real bug. No `clang-tidy` hook, no IPO despite this being a compute kernel, and no `CMAKE_EXPORT_COMPILE_COMMANDS`. All five are single-line additions; none is architectural. What *is* there is the important part: `PRIVATE` warning flags, `/permissive-` conformance mode, and per-source suppression on the vendored files rather than a global level drop (`CMakeLists.txt:98-116`).

## One-minute recap

- Nine knobs: `TESTS`/`EXAMPLES` default **ON**; four GL flags and `C_ABI` default OFF; `DUALC_GC_DIR`/`DUALC_POLYSCOPE_DIR` are `CACHE PATH`s (`CMakeLists.txt:4-20, 27, 39`).
- **`libdualc`'s entire link line is `geometry-central`** (`:95`). 24 sources, 2 vendored. Everything else hangs off targets above it.
- geometry-central and polyscope are **sibling checkouts**, probed for `CMakeLists.txt` with actionable `FATAL_ERROR`s; polyscope is guarded so it is added exactly once across four flags (`:41-42`). But both defaults use **`CMAKE_SOURCE_DIR`**, so DualC cannot be `add_subdirectory`'d into a larger project without manual overrides.
- Warnings: `/W4 /permissive-` or `-Wall -Wextra -Wpedantic`, **`PRIVATE`**, per-source suppression on the two Unlicense files. No `-Werror`, no sanitisers, no IPO, no `compile_commands.json`.
- `dualc_examples_img` (stb) is split from `dualc_examples_io` (miniz) because pulling stb into the shared lib would multiply-define `stbi_write_*` against polyscope's vendored stb (`examples/CMakeLists.txt:11-19`) — and is safely reattached to the GL lib, which never links polyscope (`:150-154`).
- Catch2 v3.5.4 by `FetchContent` is the **only** fetched dependency; pinned by **mutable tag, not SHA**, with no offline fallback, and `TESTS` defaults ON so any consumer triggers it.
- Licensing: MIT project, Unlicense QEF/SVD vendored byte-identical, **LGPL octree deliberately re-derived not copied** (`docs/ARCHITECTURE.md:236`), PicoGK reference-only noted in a source comment (`src/implicit/primitives_tpms.cpp:11`).
- The C ABI is an opt-in `SHARED` lib in `capi/`, **eleven** flat `extern "C"` entry points over an opaque handle, outside `libdualc` because it needs JSON and file I/O; the v0.3.0 in-memory mesh resolver solves the non-owning-mesh lifetime hazard across a plugin boundary.
- Every SDF formula exists twice (`double` CPU / `float` WebGL2 GPU); `dualc_glsl_parity` checks 62/62 nodes — but it is **opt-in with no CTest**, so the gate is manual. Fix: headless GL in CI, or a CPU evaluator for the emitted AST.
- Ranked weak points: no `install()`/export set (with a dead `$<INSTALL_INTERFACE>`), clean clone does not build (gitignored meshes + `copy_if_different`), tests/examples default ON, parity gate outside CTest, globals leaking into third-party subdirectories.
