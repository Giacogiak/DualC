# Handoff — P05.1: the C ABI in the default gate, the red run, the TESTS=OFF copy

Unit P05.1 of [`plan/14_P05_linux-so-ci.md`](../14_P05_linux-so-ci.md) § T1, the only unit of
P05, so it closes P05's exit criteria too. Session of 2026-10-21, branch
`feat/hygiene-and-editor`, start commit `7c472e5`. Nothing pushed, no remote touched, `main`
untouched. The record is
[14/06 § The gate builds the C ABI](../../docs/roadmap/14-c-abi/06-linux-shared-library.md#the-gate-builds-the-c-abi-the-remote-proof-waits-for-the-owners-push):
the gate change, the green and sanitized runs, the red run and Boletus's `TESTS=OFF` case.
This handoff points at it and adds what P06–P08 and the owner need.

## Outcome in one paragraph

`python3 scripts/check.py` now configures with `DUALC_BUILD_C_ABI=ON` next to `_TESTS` and
`_EXAMPLES`, so every local gate run, every CI `build` job and the `sanitize` job link
`libdualc_capi.so` / `.dll` / `.dylib` and run the **five** `cli_c_abi*` cases. Locally the
gate is green with them: ctest **367 / 367**, and **367 / 367 under ASan + UBSan**. Without the PIC lines the link
fails as #53 predicts (the red run). Boletus's `TESTS=OFF` copy of the demo meshes works at
`7c472e5`; there is nothing to fix. **#53 stays PARTIAL**: the remote run is the owner's,
after the push.

## What changed, and why

1. **`scripts/check.py`.** A tuple `GATE_OPTIONS = (DUALC_BUILD_TESTS, _EXAMPLES, _C_ABI)`
   is what `configure` passes, all `ON`. A new helper `cache_options_on` reads the existing
   `CMakeCache.txt`. `configure` now SKIPs only when every gate option is already `ON` there,
   so a build dir configured before this change (every local `build/`) is re-configured in
   place, not silently left without the ABI. `.github/workflows/gate.yml` is unchanged: it
   runs `check.py` and nothing else.
2. **`capi/CMakeLists.txt`**, comments only. The stale "PIC … lets the same target build a
   Linux .so later" sentence is gone (06 said it was corrected; it was still there). The
   PIC comment now names the local-exec TLS error and says the gate builds the ABI.
3. **Docs.** 14/06: the status line and the new dated section. 14 README: the 06 index row.
   14/03 § 7: a dated note (the CMake default stays OFF, the gate turns it on).
   17/09/01: a dated note under *Build tier* (the page that lists the tier's configure).
   17 README § Latest verification: the local count is now **367, C ABI in**, linked to 14/06.
   `AGENTS.md` and `README.md`: the opt-in lists say the gate turns `_C_ABI` on. The CMake
   default stays `OFF` everywhere.

## Measured, and under what

Linux, Ubuntu, GCC 15.2.0, GNU ld 2.46, CMake 4, Ninja, i7-920 (4 c / 8 t).

- **Gate**: `python3 scripts/check.py --clean`, with the `build/` from P04b re-configured in
  place: 32 checks, 31 passed, 0 failed, 0 skipped. **254 TUs compiled** (turning PIC on
  changes every TU's flags, so this is a real full rebuild), **0 DualC-origin warnings**,
  build 255 s, ctest 133 s, **367 / 367**. 367 = P04b's 362 + 5: `cli_c_abi`,
  `cli_c_abi_mesh`, `cli_c_abi_mesh_inmem`, `cli_c_abi_cancel`, `cli_c_abi_threads`. Each was
  re-run by `ctest -R cli_c_abi`: 5/5, about 4 s each. `libdualc_capi.so` is 2,640,496 bytes,
  with no `TEXTREL` in `readelf -d`. The final gate on the committed tree: `python3 scripts/check.py` with every edit in place, before the commit. 32 checks: 29
passed, 0 failed, and 2 skipped. The two skips were `configure` (the cache already holds
every gate option) and `warnings` (an incremental build, 0 TUs). ctest **367 / 367** in
114 s. `cache_options_on` was checked by hand: it returns `True` on `build/CMakeCache.txt`
and `False` on a copy with `DUALC_BUILD_C_ABI:BOOL=OFF`.
- **Sanitized**: `python3 scripts/check.py --build --build-dir /tmp/p05/san --config
  RelWithDebInfo -D DUALC_SANITIZE=address,undefined`, a fresh tree with sources fetched:
  4 checks, 4 passed. Configure 85 s, build 692 s, 254 TUs, 0 DualC-origin warnings; ctest
  1,038 s, **367 / 367**; 30 min 15 s wall-clock. CI's `sanitize` job has a 90-minute
  timeout. The five ABI cases add about 20 s unsanitized, so the risk to that limit is small.
- **Red run**: scratch `/tmp/p05/red`, fresh fetch, `-DDUALC_BUILD_TESTS=OFF
  -DDUALC_BUILD_EXAMPLES=ON -DDUALC_BUILD_C_ABI=ON`, the root `CMakeLists.txt` PIC block and
  `capi/`'s `POSITION_INDEPENDENT_CODE` property both removed, `--target dualc_capi`.
  The configure passes. The build fails at step 111/111, the link of `libdualc_capi.so`:
  `examples/libdualc_examples_fieldgraph.a(field_graph.cpp.o): relocation R_X86_64_TPOFF32
  against '_ZN3dce10fieldgraph12_GLOBAL__N_1L10g_readKeysE' can not be used when making a
  shared object; local-exec is incompatible with -shared`, then `failed to set dynamic
  section sizes: bad value`. Both files were restored with `git checkout`, and the break
  never reached the index.
- **TESTS=OFF**: at `7c472e5`, fresh trees `/tmp/p05/notests` (whole build, 128 steps,
  174 s) and `/tmp/p05/notests2` (`--target dualc_capi dualc_field`, Boletus's targets) both
  build green. `data/` holds the nine meshes, and they are copied next to `dualc_c_demo`,
  `dualc_field` and `dualc_boolean`. `./dualc_c_demo /tmp/p05/m.stl mesh`, run from `capi/`,
  writes 70,032 v / 120,612 f.

## P05's exit criteria and T1's acceptance, one by one

- **Acceptance: "`check.py` builds `dualc_capi` and runs the four `cli_c_abi*` cases, all
  green."** — Met, with five cases, not four. `cli_c_abi_threads` came with ABI 0.5.2
  (17 #34, P02). Evidence: the gate run and the `ctest -R cli_c_abi` run above. The plan
  file's "four" is corrected in a dated note.
- **Acceptance: "The red run and the TESTS=OFF result are recorded. The work is committed,
  not pushed."** — Both are in 14/06 § *The gate builds the C ABI*. The commits are listed
  in the status object. There is no `git push`, and the branch has no upstream.
- **Exit: "14/06 and 17's verification line are current, and #53 waits only for the
  owner's remote run."** — 14/06 has the 2026-10-21 section and the status "the gate proof
  DONE locally (2026-10-21); the remote run owed". The 17 README's local line reads "ctest
  **367**, C ABI in (2026-10-21, Linux, [14])". 14 README's index row says the same.
  The docs tier's status-sync passes.

## Chosen in-run (pending owner review) — for P08

- **(a) The flag goes in for every job, macOS included.** Nothing broke locally, so the plan's
  default applies unchanged. macOS and Windows will build the C ABI in CI for the first time.
  Neither can be run from this host. 06 puts macOS out of #53's scope. If `build (macos-14)`
  goes red on the `.dylib`, the owner's options are: fix it (most likely trivial, since
  Apple's toolchain is PIC by default), or exclude the C ABI there. The second needs a
  per-OS rule in `GATE_OPTIONS`.
- **(b) Re-configure in place when a gate option is missing from the cache**, rather than
  require `--clean`. Without it, every existing local `build/` would SKIP configure and never
  build the ABI, and nothing would notice, because the ctest count is not asserted.
- **(c) The sanitized check ran in its own build dir** (`--build-dir /tmp/p05/san --config
  RelWithDebInfo -D DUALC_SANITIZE=address,undefined`, which is CI's `sanitize` command).
  The plan's literal command (`-D CMAKE_BUILD_TYPE=RelWithDebInfo` with the default build
  dir) would have re-configured `build/` itself into a sanitized tree.

## What I got wrong on the way

- I first gave the 14/06 section a dated heading. `heading-status` caught it, and the date
  moved into the body before any commit.
- My first 17 README wording went 29 bytes over the 15,360 cap. Two trims fit it: "(306
  locally with the C ABI's four)" → "(306 locally with the C ABI)", and the link text `14/06`
  → `14`.
- The plan says the 2026-10-07 error was `__tls_guard` in `example_common.cpp`. Today's red
  run stops at `field_graph.cpp`'s `g_readKeys`, because ld reports the first offending object
  only. Same cause, different first victim. Recorded as such, not "corrected".

## Loss audit

The audit ran per touched page, each in its own foreground call:
`--base 7c472e5 --head <scratch commit-tree of the staged tree> --scope docs/ --source <page>
--untriaged --triage /tmp/p05/triage.json`. The triage was keyed `7c472e5`, with the `3d73220`
thresholds. The pages: 14/03, 14/06, 14 README, 17/09/01 and the 17 README. Each run took
about 5 min. The whole-tree untriaged count is **10,049**, the known baseline noise (P04b saw
10,060).

Only one row falls on a line this unit edited. It is intentional:
- **17 README :129**, *partial*: the local verification line. "ctest **362** (2026-10-21, Linux,
  [20])" became "ctest **367**, C ABI in (… [14])". 362 is still on 17/20 and in 14/06's
  "362 + the five".

The other rows sit on lines this unit did not touch. They were flagged only because text was
inserted near them:
- 14/03 :72 (`2026-06-17`, unverifiable context);
- 14/06 :3 and :42;
- 17/09/01 :19, :28, :33 and :48;
- 17 README :46, :76, :84, :142 and :147.

Every deleted line in `git diff -U0 -- docs/ AGENTS.md README.md` was also checked by hand:
- the two `AGENTS.md` lines, re-wrapped with "the gate turns it on";
- `README.md`'s "all `OFF` by default", now with "; `scripts/check.py` turns `_C_ABI` on";
- 14/03's "subdir).", which gained the dated note;
- 14/06's status line;
- 14 README's #53 row;
- the two 17 README lines: the local count, and "(306 locally with the C ABI's four)" →
  "(306 locally with the C ABI)". The count of four is in 14/06.

## Left open — for P06–P08 and the owner

- **The owner's remote proof.** Run `git push -u origin feat/hygiene-and-editor` (GitHub
  needs a PR to reach `main`, which is protected). Then read the ctest output in each job
  log, or the job's annotations, for the five `cli_c_abi*` lines:
  - `build (ubuntu-24.04)`: the #53 proof itself. Green means #53 → DONE.
  - `sanitize`: the same five under ASan + UBSan.
  - `build (windows-2022)` and `build (macos-14)`: these are the first hosted builds of the
    DLL and the `.dylib`. See (a).
  Expected CI ctest counts, by analogy with 2026-10-07 (local 306 = CI Linux 302 + the
  ABI's four): 367 on Linux and macOS, and 369 on Windows, which runs two more writer cases
  (17 README). This is an expectation, not a measurement. After that, 17 README's verification line and 14/06's status
  flip, and the owner or P08 sets #53 DONE in 14/06, 14 README and the roadmap README.
- **Cap room:** **17 README is at 15,360 / 15,360 bytes, exactly at the cap.** Any growth there
  means condensing or splitting it into a folder. The CI-count update has to replace text,
  not add it. 14/06 is ≈ 7.5 KB, 14 README ≈ 6.6 KB, 17/09/01 ≈ 7.4 KB, AGENTS.md is 80 / 80
  lines.
- Boletus can drop `-DCMAKE_POSITION_INDEPENDENT_CODE=ON` at its next pin bump. Its
  `TESTS=OFF` copy failure does not reproduce at `7c472e5`. Its commit `d6b2808` is not in
  this history, so the old failure was not replayed. #54's fix of 2026-10-07 is the likely
  cause, but that is unverified.
- Next free IDs (unchanged by this unit): **D-67, #63, block 22**. Block 17's next page is
  **21**.
- No `src/` or `docs/design/` change, so this unit adds nothing to P08's semantic lint.
  No GPU tier was run either: no field node, `field_graph.cpp`, `field_glsl.cpp` or
  `parity_expected_cases` moved.
