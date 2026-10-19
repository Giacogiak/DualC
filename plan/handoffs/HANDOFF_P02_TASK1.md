# Handoff — P02.1: `ContourerParams::numThreads`, through the C ABI, `dualc_field` and the bakes

Unit P02.1 of [`plan/11_P02_contourer-threads.md`](../11_P02_contourer-threads.md) § T1, the
last unit of P02; session of 2026-10-19, branch `feat/hygiene-and-editor` (nothing pushed, no
remote touched, `main` untouched). The record is
[17/18](../../docs/roadmap/17-code-audit-and-hardening/18-contourer-threads.md) (what was
wrong, what shipped, the evidence) with the struck bullet in
[17/04 § #34](../../docs/roadmap/17-code-audit-and-hardening/04-engineering-quality.md#34-api-hygiene-batch);
this handoff points at it and adds what the next units need.

## What changed, and why

1. **Library.** `ContourerParams::numThreads` (default 0 = all, worded as the sampler's);
   both pre-solve call sites in `src/contourer.cpp` read it; the #34 comment is gone.
   `bakeToGrid` (both overloads) and `MeshSource::bakeToGrid` take a trailing
   `unsigned numThreads = 0` — source-compatible, no global state.
2. **Host layer.** `FieldGraph::build(root, meshes, bakeThreads = 0)` threads the count
   through `buildField` to `bakeMeshNode` / `bakeWindingNode` (new trailing argument);
   `compileToGlsl(…, gridRes, bakeThreads = 0)` passes it to the viewer's texture bakes
   (`Ctx::threads`). `dce::parseThreadCount` (+ `kMaxThreadCount = 1024`) in
   `example_common` parses the flag for both CLIs.
3. **C ABI.** `applyParams` sets `cp.numThreads`; header comment updated; project version
   0.5.1 → **0.5.2** (CMake `project()`, which is where `dualc_version()` reads it).
4. **CLIs.** `dualc_field --threads N` → sampler, contourer, per-node bakes (with `--slice`,
   the bakes only — it is not one of the refused mesh flags). `dualc_field_view --threads N`
   → its bakes (bounds build, compile, reload).
5. **Tests.** In `tests/test_parallel.cpp`: contourer determinism at 1/2/0/64 on one octree
   (extends #30); the serial-cadence proof that `numThreads = 1` reaches the contourer; both
   bakes bit-identical at 1/2/3/0/default. In `examples/CMakeLists.txt`: `cli_field_threads`,
   `cli_field_threads_same` (byte-compare with `cli_field_boolean_bake`),
   `cli_field_threads_bad`. In `capi/`: `dualc_c_demo … threads` + `cli_c_abi_threads`.
6. **Docs.** cmd-ref 11 (row, F4 recipe, recipes index row; Diagnostics moved verbatim to
   new page `11-dualc_field/15-diagnostics.md` for cap room, the `## Diagnostics` heading
   kept in the README as a pointer), cmd-ref 12 (row + synopsis), cmd-ref README matrix row;
   design 03, 06, 10, 12; 14/02 mapping row, 14/04 § 0.5.2, 14 README index, 14/05 dated
   note; `capi/README.md` bullet; 17/04 bullet struck, 17/18 new, 17 README index + status
   cell; semantic lint run [19/08/08](../../docs/roadmap/19-docs-layers/08-semantic-lint/08-2026-10-19-after-contourer-threads.md)
   (AGENTS.md: src + design edited in one session) — 2 findings, both fixed in the commit.

**Chosen in-run (pending owner review)** — also written on 17/18:
- the CLI cap `--threads` ≤ 1024 (a typo such as `-1` is refused, exit 2);
- the C ABI's create calls still bake `bake=N` nodes on all hardware threads: create takes
  no params and `DualcContourParams` arrives only at contour time; honouring it needs a new
  create twin (an ABI minor, not the raw plan's 0.5.2 patch). Written on 14/02, 14/04,
  `capi/README.md`.

## Measured, and under what

All on Linux, GCC 15, Ninja, Release, Intel i7-920 (4 cores / 8 threads).

| What | Result |
| --- | --- |
| `cli_c_abi_threads` (`build-capi/`, `-DDUALC_BUILD_C_ABI=ON`) | 262,144 leaves; 63 of 63 serial-cadence polls, 0 other; 1/2/0 threads → 101,476 verts, 163,740 tris, byte-identical |
| Proven red: pre-solve back on `resolveThreadCount(0)` | Catch2 `ContourerParams::numThreads = 1 runs the leaf solves serially` FAILS |
| Proven red: `cp.numThreads = p.numThreads;` removed | `cli_c_abi_threads` FAILS (0 of 63 polls) |
| `dualc_field gyroid_box.json --depth 7 -o x.obj` | 41.7 s at `--threads 1`, 22.6 s default (2 runs each, ±0.2 s); RSS 3.39 GB both; OBJ byte-identical |
| `smooth-union(mesh(torus.obj,bake=192),winding(cube.obj,bake=128),k=0.2)` depth 7 | 45.1 s vs 11.2 s (1 run each); OBJ byte-identical |
| `dualc_field_view --snapshot`, mesh + baked winding | PNG byte-identical at `--threads 1` vs default |
| GPU tier `cmake --build build-gl -j && check.py --gpu --strict --build-dir build-gl` | parity **82/82**, exit 0 |
| Full gate `python3 scripts/check.py` | 32 checks, 30 passed, 0 failed, 1 skipped (configure: already configured); warnings: **11 TUs compiled** (the changed sources touched first), 0 DualC-origin warnings; **ctest 350 / 350** (was 344: +3 Catch2 cases, +3 CLI tests); exit 0 |
| `python3 scripts/check.py --docs --strict` | 28 checks, 27 OK + 1 INFO (`index-staleness`, report-only, pre-existing), exit 0 |

Loss audit (before the commit, as AGENTS/memory require for a docs move):
`scripts/docs_loss_audit.py --base HEAD --head <scratch commit-tree of the staged index> --scope docs/`
with a scratch, empty triage in `/tmp` (the repo's `docs_loss_audit.json` untouched). On the
pages this unit touched: **3 `gone` + 2 `partial` rows, all intentional** — design/03's two
code-excerpt lines (`resolveThreadCount(0)` → `params.numThreads`), 12's synopsis line
(re-wrapped with `--threads`), design/06's "no thread knob" sentence (now false, replaced),
14/02's mapping row (extended). The moved Diagnostics text is `kept` (verbatim in
11/15). The other ~9,975 untriaged rows are the whole-tree baseline noise P01 also saw
(9,979), on pages this unit did not touch.

## What I got wrong on the way

- First matrix row on `command_reference/README.md` and the first 17 README index entry were
  over the 15,360-byte cap (15,366 and 15,451); both tightened.
- First wording of the design/10 fix lost "disjoint"; restored as "owns its subtree".
- A test run against the temporarily reverted contourer left `dualc_test_work/` in the repo
  root; deleted.

## Exit criteria (plan/11_P02 § Exit criteria)

- **#34's thread bullet is DONE in 04 with its evidence** — 17/04 § #34 bullet struck,
  "DONE 2026-10-19", status line dated, evidence on 17/18 (linked); 17 README status cell
  updated. #34 itself stays PARTIAL (the `gradientAt` and `GridField` bullets are P03/P04).
- **The cmd-ref pages agree with the parser** — `flag-table` check OK in the docs tier
  (every `"--flag"` in `dualc_field.cpp` and `dualc_field_view.cpp` is on its pages; F4's
  `--threads` is in 11's table); defaults written as the code's (`0`, cap `1024`).
- Section T1 acceptance: contourer, both bakes, C ABI and `dualc_field` honour the count and
  the determinism tests pin it (above); full gate and `--docs --strict` green; committed, not
  pushed.

## Left open — for the next units

- **`cli_c_abi_threads` is not in the default gate** (C ABI is opt-in, and no CI job builds
  it today). P05 (#53, Linux `.so` in CI) should make sure its job runs `ctest -R cli_c_abi`
  so this test — and the other four — run remotely.
- **Boletus** gets 0.5.2 on its next re-vendor: no source change, only `numThreads` now also
  bounds the contourer. `capi/CSHARP_WRAPPER_HANDOFF.md` mirrors the old header comment
  ("0 = all hardware threads"), still true; left as the hand-over snapshot.
- **Cap room now:** 17/04 14,959 (≈400 left), 17 README 15,318, cmd-ref README 15,354,
  cmd-ref 11 README 15,158, design/10 15,352. P03/P04 owe #34 lines in 04: keep them to one
  line + a record page, as here.
- Next free IDs unchanged by this unit: **D-67, #62, block 22**; block 17's next page is
  **19**; the semantic-lint folder's next run is **09**.
- `docs/roadmap/README.md` § Current focus untouched (P07's).
