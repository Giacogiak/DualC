# C ABI — #53 the shared library on Linux

Part of the [C ABI](README.md) (block 14). Born 2026-10-07, when the hosted-CI plan's
untracked findings were given homes ([20/04](../20-public-delivery/04-hosted-ci-plan-run.md)).

## #53 `libdualc_capi.so` links on Linux without a caller-supplied `-fPIC`

**PARTIAL: the libraries DONE (2026-10-07); the CI proof PLANNED.** Landed with #52's merge the
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

---

← Back to the [C ABI index](README.md) · the [Roadmap index](../README.md).
