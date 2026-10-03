# Public delivery — the repo as others will build it

The record of making DualC buildable by someone who is not this machine: how the one
required dependency, geometry-central, is obtained and pinned, what the library owns
outright, and what a bare clone must do with no manual step. Born 2026-09-21 from the
question "how should geometry-central be delivered with DualC?", whose answer turned out to
need a port first. Headings are frozen at ID + title; status, date and trigger live on the
body lines. The present-tense rules live in [`docs/design/05` § 5](../design/05-conventions-and-tables.md#5-geometry-central-integration)
and [`THIRD_PARTY.md`](../../THIRD_PARTY.md); the settled decision is
[D-42](../decisions/01-settled.md).

## #47 Pin geometry-central to upstream, own nanort, self-bootstrapping clone

**DONE — 2026-09-21.** Three findings, then one port and one build change.

**What the audit found.** DualC had been building against `D:\geometry-central`, which was
not upstream: the owner's fork (`Giacogiak/geometry-central`, branch `wrapper`) forked from
upstream `8579865` (2025-10-13) and 19 commits behind `nmwsharp/geometry-central` master.
Every green number in this record up to here — the gate, ctest, the GLSL parity harness —
was fork-verified. Three facts decided what to do:

1. The fork's only library-affecting change was a one-line `COPY_ON_ERROR` on a nanoflann
   symlink in `deps/CMakeLists.txt` (Windows without Developer Mode cannot create symlinks);
   the rest is a C# `wrapper/` tree DualC never compiles. Upstream `23f6de1` deleted the
   symlink block entirely, so the patch was obsolete. The fork is therefore *not*
   co-developed with DualC — the original defence of the sibling checkout
   ([01/01 § 2](01-core-dual-contouring/01-engineering-record.md#2-architectural-decisions-locked-in-early-still-hold),
   study [B5 § 3](../raw/study/B5-build-dependencies-licensing.md#3-the-sibling-checkout-and-why-it-is-a-locked-decision))
   no longer held.
2. Upstream **v1.1.0** (`1e8e43d2d50b18c98ea0b4a53ffaf5848dbaaa26`) ships
   `GeometryCentralConfig.cmake.in`, `install(EXPORT GeometryCentralTargets NAMESPACE
   geometry-central::)` and `find_dependency(Eigen3)`. That is trigger (b) of
   [10 #8](10-infrastructure-and-integration.md#8-install-rules-and-dualcdualc-alias-target-phase-1-done--2026-05-26-phase-2-deferred),
   which had recorded "geometry-central ships no Config" as verified — against the fork.
3. DualC did **not compile** against upstream v1.1.0: it makes nanort a *private*
   dependency of its target, moves the header from `nanort/nanort.h` to `nanort.h`, and
   updates nanort to the current release whose `BVHAccel<T>` takes one template argument.
   `src/internal/mesh_bvh.cpp` instantiates the 2016 four-parameter `BVHAccel` and walks its
   nodes directly. Errors in order: C1083 (header), then C2977/C2955 (template arity). This
   is the "nanort is structurally load-bearing but undeclared" sub-item of
   [17/04 #33](17-code-audit-and-hardening/04-engineering-quality.md#33-build--tooling-hardening),
   materialised.

Two facts bound every option. geometry-central is **public API surface**, not an
implementation detail ([design 05 § 5](../design/05-conventions-and-tables.md#5-geometry-central-integration)
holds the present-tense description) — at this date `include/dualc/types.h` makes `dualc::Vector3` a `using` of
`geometrycentral::Vector3`, `implicit2d.h` likewise for `Vector2`, and `contourer.h`,
`sampler.h`, `implicit.h`, `pipeline.h` take and return `SurfaceMesh` +
`VertexPositionGeometry` — so no delivery mechanism can hide it from a C++ consumer; the
options only differ in who fetches it. And the C ABI exposes no geometry-central type
(`capi/dualc_c.h` is flat arrays; geometry-central is statically absorbed into
`dualc_capi`), so Boletus is unaffected by any of them.

**The options weighed** (mechanism for a consumer to obtain geometry-central; every one
pinned to v1.1.0):

| | Mechanism | For | Against |
| --- | --- | --- | --- |
| A | Sibling checkout, status quo + a version note in the README | zero code | reproducibility rests on the reader; worst first-run experience |
| B | Git submodule `deps/geometry-central` | pinned by commit, offline after clone; the shape 10 #8 had assumed | `--recurse-submodules` ceremony (geometry-central has its own nested `deps/happly` submodule); every bump is a submodule commit |
| **C** | **`FetchContent` at the pinned commit, `-DDUALC_GC_DIR` as the escape hatch — chosen** | `git clone && cmake` works from a bare clone; the pin is one line; the local co-development workflow survives via the override; the mechanism Catch2 already uses; no `cmake_minimum_required` bump (3.14 suffices without `FIND_PACKAGE_ARGS`) | network at first configure — already true via Eigen and Catch2 |
| D | `find_package(GeometryCentral)` + 10 #8 Phase 2 (`install()`, `dualcConfig.cmake`) | the vcpkg/Conan/prebuilt path, now genuinely unblocked upstream | the most infrastructure; the build-tree target is `geometry-central`, the installed one `geometry-central::geometry-central`; no consumer asking |
| E | C + D layered (target → `DUALC_GC_DIR` → `find_package` → fetch) | maximal flexibility | D's cost up front |

C now; D is additive later and stays on 10 #8's remaining trigger.

**What landed (code).**
- `src/internal/third_party/nanort/nanort.h` + `nanort_LICENSE.txt` — nanort vendored,
  byte-identical to the copy geometry-central bundled up to `8579865` (MIT, Light Transport
  Entertainment, the 2015-2016 single-header release), on `dualc`'s private include path.
  The older API is pinned on purpose; bumping nanort is a port of `mesh_bvh.cpp`.
  `scripts/check_data.json` `third_party_roots` gains the folder so the vendoring check
  covers it.
- `CMakeLists.txt` — the sibling-or-die block is a three-rung resolver: a `geometry-central`
  target an enclosing project already defines; else `-DDUALC_GC_DIR` (now empty by default,
  fatal with both remedies named if set to a non-tree); else `FetchContent` of
  `nmwsharp/geometry-central` at the SHA above, no `GIT_SHALLOW` (shallow needs a ref name and
  fights the submodule recursion that brings `deps/happly`). The link comment no longer claims
  nanort/nanoflann arrive transitively.
- `examples/CMakeLists.txt`, `capi/CMakeLists.txt` — the clean-clone bootstrap, #33's last
  open cheap item: a custom command runs `dualc_gen_demo all` into `<build>/data/`
  (`dualc_demo_data`), and `dualc_copy_demo_meshes(<target>)` gives every mesh-consuming
  binary a dependency on it plus the POST_BUILD copies, replacing five copy loops that read
  gitignored `*.obj` from the source tree and failed on a fresh clone. The checkout's `data/`
  is untouched by the build; `dualc_gen_demo all --dir data` remains the way to populate it
  for recipes run from the root. One wrong first version, caught by building the C ABI: the
  helper read the mesh list and data directory as directory variables of `examples/`, which
  a call from `capi/` (a sibling directory) does not see, so `dualc_c_demo` silently got no
  meshes; both now travel as global properties.

**What landed (docs).** `README.md` § Dependencies (the three rungs, the three network
fetches of a first configure, geometry-central as public API), `AGENTS.md`, `STRUCTURE.md`,
`THIRD_PARTY.md` (§ nanort, § runtime dependencies rewritten: Eigen + happly only),
`capi/CSHARP_WRAPPER_HANDOFF.md`, `docs/design/` README, 05 § 5, 06 § 8, 09, glossary,
`docs/command_reference/` README prelude and 09; decisions: D-42 settled, #8's stale
clause and #33's sub-items updated; the dated pointers in 10 § 8, 01/01 § 2, 14/03, 17/09.

**Verification.**
- Before the change, in a scratch tree: DualC configured against a pristine
  `git clone --branch v1.1.0 --recurse-submodules` of upstream; `geometry-central.lib` built
  clean; `dualc.lib` failed on nanort only (the errors above); with the old nanort header on
  DualC's own include path, everything built with 0 errors and `ctest` passed **254/254**
  (124 s). The port is exactly one header.
- After the change, from a copy of the working tree holding only tracked files (no
  sibling, no `data/*.obj`, no `build/`), `cmake -S . -B build -G "Visual Studio 17 2022" -A
  x64` with **no flags**: the log shows the FetchContent rung, the fetched tree is at the
  pinned SHA with `deps/happly/happly.h` present, the build generated the demo meshes
  itself, and the result is recorded on the line below.
- Result: 0 build errors, 0 warnings from DualC sources, all nine meshes in `build/data/`, `ctest`
  **254/254** (110 s). One caveat on the method: the copy was not a git work tree, so
  `check_selftest` (the gate's own fixture test) failed on that first run for that reason
  alone — `git init` + commit in the copy and it passes; a real `git clone` has its `.git`.
- The escape hatch: `-DDUALC_GC_DIR=D:/geometry-central` (the fork) still configures, builds
  and passes — recorded on the line below.
- Result: the `DUALC_GC_DIR` rung is logged and `dualc_demo` builds against the fork — nothing
  else in DualC had depended on it.
- The gate ran from scratch in a fresh build directory: the old `build/` cache carried the
  fork path and would have stayed fork-green. After the commit the two fork-pinned caches on the
  owner's machine (`build/`, `build-min/`) were deleted and `build/` rebuilt through the gate on
  the fetch path (30 checks, 0 failed), so no local tree still points at the fork. Two things the gate itself needed for that:
  `check.py`'s `configure` step had its own "`../geometry-central` sibling not found" probe,
  now gone; and its `warnings` scan matched the exclusion list against the header path only,
  so a C4267 raised inside MSVC's `xmemory` by upstream geometry-central's `embed_convex.cpp`
  (new code since the fork) landed as "DualC-origin" — the scan now matches the whole
  MSBuild line, whose `[project.vcxproj]` suffix names the TU that instantiated the header.
  `src/internal/third_party/` joins the exclusion list beside `examples/third_party/`, and the
  vendored include directory is `SYSTEM`. Result: 30 checks, 0 failed, 253 TUs, 0
  DualC-origin warnings, ctest 254/254.
- The opt-in GL targets, which no default gate builds, were checked separately because the
  same helper feeds them: configured with all four GL flags on, `dualc_glsl_parity`,
  `dualc_raymarch` and `dualc_field_view` each carry the mesh copies (`dualc_view` never
  had them), and `check.py --gpu` against that build passed **73/73**, the baked-source
  cases included — the harness found `cube.obj` next to itself.

**Out of scope, named.** Creating the public remote and pushing (an outward-facing step the
owner takes; done, see the update below); the absolute `D:\…` paths quoted in historical
roadmap text; `find_package` / 10 #8 Phase 2 (its trigger (a), a precompiled-distribution
consumer, still stands; trigger (b) fired and is retired); the polyscope mechanism (sibling,
opt-in; the owner's fork is upstream, 0 commits ahead).

**Update — 2026-10-03: published.** The remote is <https://github.com/Giacogiak/DualC>. Its
history is fresh: one root commit, `5989fb5` ("Initial public release of DualC"), with no
parent. The 191 pre-publication commits were deliberately left off the remote and are kept
offline by the owner, so the commit hashes cited anywhere in `docs/` before that date name
commits of that archive, not of the public history.

---

← Back to the [Roadmap index](README.md).
