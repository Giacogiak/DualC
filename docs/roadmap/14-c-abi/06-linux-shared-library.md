# C ABI — #53 the shared library on Linux

Part of the [C ABI](README.md) (block 14). Born 2026-10-07, when the hosted-CI plan's
untracked findings were given homes ([20/04](../20-public-delivery/04-hosted-ci-plan-run.md)).

## #53 `libdualc_capi.so` links on Linux without a caller-supplied `-fPIC`

**PARTIAL: the libraries DONE (2026-10-07); the gate proof DONE locally (2026-10-21); the
remote run owed.** The libraries landed with #52's merge the
same day it was opened: the root `CMakeLists.txt` sets `CMAKE_POSITION_INDEPENDENT_CODE ON`
when `DUALC_BUILD_C_ABI` is on — before geometry-central is resolved, so a tree DualC fetches
or is pointed at gets it and an enclosing project's own tree is untouched — because #52's
`thread_local` in `example_common.cpp` hit the very `__tls_guard` error below on this
machine. Verified locally: `libdualc_capi.so` links with no `-fPIC` from the caller and the
four `cli_c_abi*` cases pass (306 with the full suite, [17/16](../17-code-audit-and-hardening/16-windows-rename-race/02-evidence-and-verdict.md));
`capi/CMakeLists.txt`'s comment and 03 § 7 are corrected. Still owed: the gate or CI building
the C ABI on Linux and running those cases, the red run included. Boletus can drop its
`-DCMAKE_POSITION_INDEPENDENT_CODE=ON` workaround at its next pin bump past this.

*The item as opened:* on Linux, `-DDUALC_BUILD_C_ABI=ON` does not link.
`libdualc_capi.so` links the static `libdualc.a`, geometry-central and the two example
libraries, and none of them is built position-independent: only the `dualc_capi` target
sets `POSITION_INDEPENDENT_CODE ON` (`capi/CMakeLists.txt`). The comment there, and
[03 § 7](03-implementation-and-verification.md#7-build--deployment), still say that this
"lets the same target build a Linux `.so` later". Finding 7 of
[17/14](../17-code-audit-and-hardening/14-build-hardening-ci.md#findings-and-what-was-done-with-each)
records the GCC 15 error, `R_X86_64_TPOFF32 against __tls_guard`. CI never saw it: the C ABI
is built on Windows only.

**Who pays for it.** Boletus builds its `linux-x64` native library from the pinned DualC
commit and passes `-DCMAKE_POSITION_INDEPENDENT_CODE=ON` to get past it (`relocation
R_X86_64_PC32 … recompile with -fPIC`). It reported the gap upstream
([Boletus roadmap 09/10](../../../../Boletus/docs/roadmap/09-docs-layers/10-linux-native.md)).
The same report notes that the post-build copy of the demo meshes fails in a tree configured
with `-DDUALC_BUILD_TESTS=OFF`; check that within this item.

**The intended fix.**

- **The libraries.** When `DUALC_BUILD_C_ABI` is ON, every static library `dualc_capi`
  links is built position-independent: `dualc`, the example libraries, and geometry-central
  when DualC added the tree itself. A tree an enclosing project owns is left alone, as in
  [`design/05` § 5](../../design/05-conventions-and-tables.md#5-geometry-central-integration).
- **The proof.** The gate or CI builds the C ABI on Linux and runs its CTest cases. The item
  is done when the `.so` links and those cases pass with no `-fPIC` from the caller. The
  failing configure is the red run.
- **The docs.** `capi/CMakeLists.txt`'s comment and 03 § 7 are corrected.

Out of scope: macOS (`.dylib`); and the pin bump Boletus then makes to drop its workaround,
which is Boletus's ([15](../15-boletus-handoff.md)).

### The gate builds the C ABI; the remote proof waits for the owner's push

**2026-10-21.** Plan-run unit P05.1, branch `feat/hygiene-and-editor`, start commit `7c472e5`. Linux, GCC
15.2, GNU ld 2.46, CMake 4, Ninja, i7-920 (4 c / 8 t).

- **The gate.** `scripts/check.py`'s `configure` sets `GATE_OPTIONS` —
  `DUALC_BUILD_TESTS`, `_EXAMPLES` and `_C_ABI`, all `ON`. A build dir whose cache lacks one
  of them is re-configured in place, not skipped, so an old local tree joins without
  `--clean`. Every CI `build` job (Ubuntu, Windows, macOS) and the `sanitize` job now link
  `dualc_capi` and run its cases; `gate.yml` is unchanged, because it runs `check.py` alone.
  The CMake default stays `OFF`. The gate's docs: [17/09/01](../17-code-audit-and-hardening/09-local-checks-gate/01-the-docs-checks.md#the-docs-checks-lead-and-that-is-the-point),
  `AGENTS.md`, `README.md`, [03 § 7](03-implementation-and-verification.md#7-build--deployment).
- **Green, local.** `python3 scripts/check.py --clean`: 32 checks, 31 passed, 0 failed;
  254 TUs compiled (the PIC flag recompiles all of them), 0 DualC-origin warnings; build
  255 s; ctest **367 / 367**, which is 362 + the **five** `cli_c_abi*` cases (`cli_c_abi`,
  `_mesh`, `_mesh_inmem`, `_cancel`, and `_threads`, which 17 #34 added in ABI 0.5.2; the plan
  said four). `libdualc_capi.so` is 2.6 MB, with no `TEXTREL`.
- **Sanitized, local.** CI's `sanitize` command in a build dir of its own
  (`--build --build-dir <scratch> --config RelWithDebInfo -D DUALC_SANITIZE=address,undefined`):
  4 checks green, 254 TUs, 0 DualC-origin warnings, ctest **367 / 367** under ASan + UBSan
  (the five ABI cases included), 30 min wall-clock on this host (build 692 s, ctest 1,038 s).
- **The red run.** In a scratch tree (`-DDUALC_BUILD_TESTS=OFF -DDUALC_BUILD_EXAMPLES=ON
  -DDUALC_BUILD_C_ABI=ON`, geometry-central fetched), with the root `CMakeLists.txt`'s
  `CMAKE_POSITION_INDEPENDENT_CODE` block and `capi/`'s target property both removed, the
  configure passes and the link of `libdualc_capi.so` fails:
  `examples/libdualc_examples_fieldgraph.a(field_graph.cpp.o): relocation R_X86_64_TPOFF32
  against 'dce::fieldgraph::(anonymous namespace)::g_readKeys' can not be used when making a
  shared object; local-exec is incompatible with -shared`. ld stops at the first such object.
  This time it is the field-graph parser's `thread_local`, not `example_common.cpp`'s
  `__tls_guard` of 2026-10-07; both have the same cause. Both files were restored, and the
  break was never committed.
- **Boletus's `TESTS=OFF` report.** At `7c472e5`, a fresh tree configured with
  `-DDUALC_BUILD_TESTS=OFF -DDUALC_BUILD_EXAMPLES=ON -DDUALC_BUILD_C_ABI=ON` builds green,
  both as a whole (128 steps, 174 s) and with Boletus's targets alone (`--target dualc_capi
  dualc_field`). The meshes are generated into `<build>/data/` and copied next to each
  binary, and `dualc_c_demo … mesh` runs from `capi/` (120 612 faces). There is nothing to fix.
  Boletus's commit `d6b2808` predates this history's root, so its failure cannot be replayed
  here. The likely cause is `dualc_gen_demo all --dir` aborting on a missing directory
  before [10 #54](../10-infrastructure-and-integration.md#54-dualc_gen_demo-creates-its---dir-and-a-failed-write-is-an-error-not-an-abort)
  (2026-10-07), but that is not verified.
- **Owed.** The remote run. The owner pushes the branch, then reads `cli_c_abi*` in the
  ctest output of `build (ubuntu-24.04)` and `sanitize`, and in `build (windows-2022)` and
  `build (macos-14)` too, since those are the first hosted builds of the DLL and `.dylib`.
  #53 flips to DONE when the Ubuntu job shows them green.

---

← Back to the [C ABI index](README.md) · the [Roadmap index](../README.md).
