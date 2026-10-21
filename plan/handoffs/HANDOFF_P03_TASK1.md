# Handoff — P03.1: vendor the Delaunay engine, tetrahedralize the seeds

Unit P03.1 of [`plan/12_P03_delaunay-and-complex.md`](../12_P03_delaunay-and-complex.md) § T1;
session of 2026-10-21, branch `feat/porous-shell` in `~/Documents/DualC-porous`. Nothing was
pushed, no remote was touched, and `main`, `feat/hygiene-and-editor` and `~/Documents/DualC`
were left alone. The commits are `5d915e3` (code, tests, record) and `e3479ca` (the design
index line). The record:

- [23/02 #64](../../docs/roadmap/23-porous-shell/02-record.md#64-delaunay-tetrahedralization-and-the-packed-complex)
  is now **PARTIAL**: T1 is done, T2 is next;
- the files, numbers and tests are in the new child page
  [23/03](../../docs/roadmap/23-porous-shell/03-delaunay-engine.md).

This handoff points at those pages and adds what P03.2 needs.

## What changed, and why

| File | What |
| --- | --- |
| `src/internal/third_party/shewchuk/predicates.c` (new, vendored) | Shewchuk 1996, public domain, **byte-identical** (sha256 `f8662c3f…2029`, the P01 download) |
| `src/internal/exact_predicates.c` (new, DualC) | The only TU that compiles it: `#define`s every one of its 55 external symbols to `dualc_pred_*`, sends `random()` to a static stub, then `#include <shewchuk/predicates.c>` through the existing SYSTEM include path |
| `src/internal/exact_predicates.h` (new) | C++ declarations plus inline `internal::orient3d / insphere / orient2d` taking `const double*`, and `exactPredicatesInit()` |
| `src/internal/compat/sys/time.h` (new) | An empty header, on that one file's include path on MSVC only (predicates.c includes `<sys/time.h>` and uses nothing from it) |
| `src/internal/porous_delaunay.h` (new), `src/implicit/porous_delaunay.cpp` (new) | `internal::DelaunayTets` and `internal::tetrahedralize(points, cancel)` |
| `CMakeLists.txt` | `project(… LANGUAGES CXX C)`, which replaces the conditional `enable_language(C)`; the two new sources; `C_STANDARD 90` + `C_EXTENSIONS ON` (`-std=gnu90`); the C file gets `-w;-ffp-contract=off`, or `/W0;/fp:precise` plus the compat include on MSVC; `porous_delaunay.cpp` gets `-ffp-contract=off` |
| `tests/test_porous.cpp` | 5 new `porous:` cases, a hidden `[porous-bench]` "tets per second" case, and the check helpers |
| `THIRD_PARTY.md` | A section "Shewchuk's exact predicates": origin URL, date, sha256, licence, files. There is no `LOCAL MODIFICATION`; the wrapper is described instead |
| `STRUCTURE.md`, `README.md`, `docs/design/06-parameters-and-vendoring.md` § 8, `docs/design/README.md` | The new files; "the library needs a C compiler"; three vendored pieces, not two |
| `docs/roadmap/23-porous-shell/{02-record,03-delaunay-engine,README,01-study}.md`, `docs/roadmap/README.md` | #64 PARTIAL; the new 03 page; an italic P03 note on the Engine † row; three status phrases |

The gate's `vendoring` check needed no data change: `src/internal/third_party/` is already a
root in `third_party_roots`, and the check now counts 18 vendored files, with no problems.

### The contract P03.2 builds on (`src/internal/porous_delaunay.h`)

- **Vertices.** `verts[4t … 4t+3]` are input indices, **ascending**.
- **Neighbours.** `neighbours[4t+k]` is the tet across the face *opposite* `verts[4t+k]`, or
  **−1 on the convex hull**.
- **Order.** Tets are sorted by quadruple.
- **Orientation is not stored.** Call `internal::orient3d` on the four points: its sign is
  ±, never 0, as tested.
- **Duplicates.** `duplicates` lists the input indices whose point equals a kept point;
  those indices appear in no tet. A `SeedSet` from P02 has none in practice.
- **Errors and cancel.** Errors are `std::invalid_argument("porous delaunay: …")`. Cancel
  throws `Cancelled("porous delaunay")`, polled every 4096 insertions.
- **Predicates.** `exactPredicatesInit()` runs once, under `std::call_once`, inside
  `tetrahedralize`. Code that calls `internal::orient3d` / `insphere` **before** any
  `tetrahedralize` must call `exactPredicatesInit()` itself. The tests always tetrahedralize
  first.

## The in-run choices

Each is **Chosen in-run (pending owner review)**, and each is recorded in 23/03 § The choices
made in the run:

1. **An infinite vertex instead of the defaults table's finite super-tet at 1000×.** With
   it, the hull is exact, so the Euler test is exact too. The P01 handoff offered this as
   "the ghost-vertex variant, about 100 lines".
   - Coplanar rule: a hull tet conflicts when the point is strictly beyond its face, or when
     the point is on the face's plane and strictly inside the sphere of the finite tet across
     that face. This is CGAL's rule, written with Shewchuk's `insphere` only.
2. **Symbol renaming through a wrapper TU, with `predicates.c` byte-identical.**
   - The alternatives were a `LOCAL MODIFICATION` (a guarded include) or leaving the
     symbols global. Global symbols would collide with any host that links Triangle or
     TetGen.
   - `nm` shows 55 defined `dualc_pred_*` symbols (40 T, 15 B) and no other defined C
     symbol in `libdualc.a`.
3. **C as a project language.** The library now needs a C compiler. This supersedes the line
   in 20/02 (a dated record, left as written) that "the library alone still needs no C
   compiler". `README.md` says so.
4. **The output is canonical** (vertices sorted), as the table's row says, so orientation is
   not stored (see the contract above).

## Measured, and under what

All on this Linux host with GCC 15 and one thread.

- **Bench, Release (`-O3`).** Run with `build/tests/dualc_tests "[porous-bench]"`; two runs.

  | Input | Points | Tets | Per point | Hull faces | Time |
  | --- | --- | --- | --- | --- | --- |
  | White noise, unit cube, `Pcg32(42, 1000)` | **100,000** | **672,339** | 6.72 | 368 | **1,432 / 1,442 ms** |
  | `poisson` seeds of box ±0.65, spacing 0.025, with the pad | 94,189 | 611,392 | 6.49 | 574 | 1,279 / 1,269 ms |

- **Cross-check against geogram,** in scratch at `-O2` on the P01 prototype's exact 100k
  points:
  - 672,288 tets, which is geogram's count. The prototype had 672,241, missing 47.
  - The canonical FNV hash `023e8635cf0fd7a5` is **geogram's own hash** from the raw dump,
    so the tet set is identical.
  - Time 1.59–1.61 s, against the prototype's 1.24 s. The infinite vertex and the
    `tested_` stamps cost about 15 %.
  - The 20³ grid gives 41,154 tets with the prototype's hash `9e3d91fe4728546b`.
- **Sanitizers.** valgrind is not installed on this host. A scratch ASan + UBSan build
  (`-O1 -g -fno-sanitize-recover=all`) of `porous_delaunay.cpp` plus the wrapper ran clean
  on 20k points with 2 duplicates, a 12³ grid with 12 duplicates (7,986 = 6·11³ tets) and a
  flat input (it threw, as it should).

## Checks run

These follow the rules for P01–P07: no full gate.

- `cmake --build build -j`: no warnings from DualC files. `compile_commands.json` shows
  `-std=gnu90 … -w -ffp-contract=off` on `exact_predicates.c`.
- `ctest --test-dir build -R porous`: 15/15 passed (10 from P02 and 5 new).
- `ctest --test-dir build -R 'porous|field-graph|GLSL codegen' -j8`: 34/34 passed.
- The driver's smoke filter, `ctest -R 'porous|field_graph|field_glsl'`: 15/15 passed.
- `python3 scripts/check.py --docs --strict` on the staged tree: 27 OK + 1 INFO
  (`index-staleness`, pre-existing), PASS. The pre-commit hook passed on both commits.
- No docs file was deleted or moved, so no loss audit was due.
- **Not run:** the full gate, and any MSVC or Apple Clang build. See below.

## What I got wrong on the way

- **The renaming macros first lived in the shared header.** That would have rewritten
  `std::numeric_limits<double>::epsilon()`, or any `compress` / `estimate` / `splitter`
  identifier, in every C++ file that included it. They now live only in the C wrapper. The
  header declares the `dualc_pred_*` names directly.
- **The background valgrind job "passed".** It was a `timeout` with no valgrind behind it
  (exit 127, `which valgrind` empty). I replaced it with the ASan build above. Check the
  tool exists before trusting a background run.
- **The golden digests started as `0` placeholders** to read the real values, and were then
  pinned. The values come from this host and are only claimed to be OS-independent. See
  "Open" below.

## Open, and what the next units need

- **Cross-OS determinism is now testable.** `porous: same points, same tet list` pins
  `0x05c997ba5690c4c5` (5k white noise, unique Delaunay) and `0xf248fc73c6345e82` (the 9³
  grid; it pins the Morton order too). The first CI run on Windows or macOS confirms or
  refutes the claim. **If the grid digest alone fails on one OS**, the Morton key's float
  arithmetic differs there. If the cloud digest fails, a predicate is not exact there,
  probably through FMA or x87.
- **The MSVC and Apple Clang builds of the C file are unverified here.** The pieces that
  only a Windows build will confirm:
  - K&R definitions under MSVC's C compiler, with `/W0`;
  - the `compat/sys/time.h` shim through the `INCLUDE_DIRECTORIES` source property;
  - the D9025 "overriding /W4 with /W0" command-line warning, which is the same class as
    `qef.cpp`'s accepted `/W3`.

  CI's `windows-2022` job checks all of these automatically on the first push, so this is
  not a human check.
- **P03.2 (T2):**
  - derive the faces and edges from `verts`/`neighbours`: a face is (t, k) with
    `neighbours[4t+k]` < 0 or > t, which counts each face once;
  - take each tet's orientation from `orient3d` before computing a circumcentre or a face
    plane;
  - `SeedSet::box` is the clamp box, and `isPad` gives the pad seeds;
  - the 100k-seed packed size (bytes per seed) is still owed to the record;
  - `internal::SeedGrid` gives the start bins.
- **Speed.** 1.4 s per 100k seeds, sequential, so 2M seeds would take about 30 s plus
  allocation: 13.4M tets × 32 B in the working array, about 430 MB, plus the output (16 + 16
  B per tet). Not a problem for this run. A later item could add BRIO or a parallel build,
  as long as it stays deterministic.
- **The owner's review list (P08.2) gets** the four in-run choices above.
- **No human check is raised by this unit.** Everything was verified headless, and the
  cross-OS points above are CI's job.

## T1 acceptance

- `ctest --test-dir build -R porous` is green (15/15).
- `THIRD_PARTY.md` names every new vendored file. There is one, `predicates.c`, and the
  DualC wrapper, header and shim around it are described there and in `STRUCTURE.md`.
- The handoff records the tet count and time for 100k seeds: 672,339 tets in 1.43 s for
  white noise, and 611,392 tets in 1.27 s for 94k `poisson` seeds. The record holds the
  same numbers in 23/03.
- The four properties of the raw plan § 3 P03 T1 each have a case:
  - Euler for the convex hull;
  - the empty circumsphere on a random subset;
  - the regular grid with `jitter=0`;
  - same input, identical list.
- Everything is committed and the tree is clean. The plan file's criteria needed no
  correction.
