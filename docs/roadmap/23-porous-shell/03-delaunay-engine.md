# The Delaunay engine as built — #64, the tetrahedralization

The numbers and choices behind [02 #64](02-record.md#64-delaunay-tetrahedralization-and-the-packed-complex)'s
first half (P03 T1, 2026-10-21): the engine D-74 chose, built from the P01 prototype of
[01 § (a)](01-study.md#a-the-delaunay-engine). The complex and its bins (T2) append below.
The engine as it is today is [design/13](../../design/13-porous-shell.md) (2026-10-22, P08.1).

## What was built

| File | What |
| --- | --- |
| `src/internal/third_party/shewchuk/predicates.c` | Shewchuk's public-domain predicates, byte-identical (sha256 `f8662c3f…2029`); attribution in `THIRD_PARTY.md` |
| `src/internal/exact_predicates.c` | DualC's one unit that compiles it: every external symbol renamed `dualc_pred_*`, its `random()` calls sent to a local stub |
| `src/internal/exact_predicates.h` | The C++ declarations: `orient3d`, `insphere`, `orient2d`, `exactPredicatesInit` |
| `src/internal/compat/sys/time.h` | An empty header for MSVC, which has no `<sys/time.h>` |
| `src/internal/porous_delaunay.h`, `src/implicit/porous_delaunay.cpp` | `internal::DelaunayTets` and `internal::tetrahedralize(points, cancel)` |
| `CMakeLists.txt` | C a project language; the C file as C90 + extensions, `-ffp-contract=off` (`/fp:precise`), warnings off; `porous_delaunay.cpp` `-ffp-contract=off` |

**The output.** Tet `t` has `verts[4t…4t+3]`, input indices in ascending order, and
`neighbours[4t+k]`, the tet across the face opposite `verts[4t+k]`, or −1 on the hull. The
list is sorted by vertex quadruple. Points equal to a kept point (all three doubles) are
listed in `duplicates` (the later index) and named by no tet. Errors are
`std::invalid_argument` (`porous delaunay: …`): a non-finite coordinate, more than
`kPorousMaxSeeds` points, or points that do not span 3D. A requested token throws `Cancelled`
(polled every 4,096 insertions).

## The choices made in the run

Each is **Chosen in-run (pending owner review)**.

1. **An infinite vertex, not the prototype's finite super-tetrahedron.** The table's row said
   "finite super-tet at 1000× the extent", which leaves the hull about 47 tets short per 100k
   points, so Euler's formula fails at the hull. A hull tet names the infinite vertex, and
   the rest of the code is unchanged. A point coplanar with a hull face conflicts with that
   hull tet exactly when it is strictly inside the sphere of the finite tet across the face,
   which is CGAL's rule written with Shewchuk's `insphere`. This makes the two always agree,
   so no flat tet appears on the hull. The first tet is the first four points in Morton order
   that span 3D; collinearity is tested exactly by three `orient2d` projections.
2. **The predicates' symbols are renamed, and the file stays byte-identical.** This is done
   through the wrapper unit, not a `LOCAL MODIFICATION`. Many hosts link their own
   `predicates.c` (Triangle, TetGen), so unprefixed `orient3d` / `exactinit` would collide
   with them. `nm` on the built `libdualc.a` shows 55 defined `dualc_pred_*` symbols (40
   functions, 15 globals) and no other defined C symbol.
3. **C is now a project language** (`project(… LANGUAGES CXX C)`), so the library needs a C
   compiler. This supersedes 20/02's line "the library alone still needs no C compiler".
   The other options were a C++ port with prototypes (a `LOCAL MODIFICATION` of about 60
   K&R definitions) or a separate object library. `README.md` says so.
4. **The output is canonical**, as the table states: vertices sorted and the list sorted, so
   the orientation is not stored. T2 derives it with `orient3d`.

## Measured

Host: this Linux box, GCC 15, Release (`-O3`), one thread. The hidden case is
`build/tests/dualc_tests "[porous-bench]"`; two runs were made and both are given.

| Input | Points | Tets | Per point | Hull faces | ms |
| --- | --- | --- | --- | --- | --- |
| White noise in the unit cube (`Pcg32(42, 1000)`) | 100,000 | 672,339 | 6.72 | 368 | 1,432 / 1,442 |
| `poisson` seeds of a box ±0.65, spacing 0.025, with the pad | 94,189 | 611,392 | 6.49 | 574 | 1,279 / 1,269 |

**Cross-check against geogram,** in scratch, `-O2`, on the P01 prototype's 100k PCG32
points:

- 672,288 tets, geogram's `BDEL` / `PDEL` count, where the prototype had 672,241;
- the canonical FNV-1a hash `023e8635cf0fd7a5` is geogram's own, from
  [the dump](../../raw/2026-10-21-porous-study-measurements.md), so the tet set is
  identical. Time 1.59–1.61 s;
- the 20³ integer grid gives 41,154 tets with the prototype's hash `9e3d91fe4728546b`: the
  same Morton order makes the same choice among the cospherical sets.

The infinite vertex costs about 15 % over the prototype's 1.24 s (`-O2`). An ASan + UBSan
scratch build ran 20k points with duplicates, a 12³ grid with duplicates and a flat input
clean.

Memory: the output costs 16 + 16 B per tet (vertices + neighbours), on top of the 32 B per tet
of the working array. Speed lead, not built: BRIO or a parallel build, as long as it stays
deterministic (the build is sequential, about 1.4 s per 100k seeds).

A pitfall the P01 prototype hit (it segfaulted): when the cavity's tets are freed, record
each outer neighbour's back-pointer index **before** a freed slot can be reused; relinking
after reuse corrupts the adjacency.

## The tests

Five `porous:` cases in `tests/test_porous.cpp`, 0.8 s together:

| Case | What it checks |
| --- | --- |
| The tets fill the convex hull by Euler's formula | 3,000 white-noise points, and a sphere's `poisson` seeds. V − E + F − T = 1 and the hull surface's χ = 2. Every hull edge is on two hull faces. No point lies beyond a hull face. Every point is used. The neighbours are symmetric across the same face. No flat tet. 5–7 tets per point |
| No point is inside a tet's circumsphere | 400 random tets against every point, on white noise and on a sphere's `random` seeds |
| A regular grid gives no degenerate tet | The `jitter=0` lattice of a box, and an 8³ integer grid: all the structure checks above, the empty sphere, and Σ\|orient3d\| = 6 × 7³ exactly |
| Same points, same tet list | Two runs are equal. Pinned digests: 5,000 white-noise points `0x05c997ba5690c4c5`, which is unique for general position, and the 9³ grid `0xf248fc73c6345e82`, which also pins the Morton order. A CI OS that differs fails here. Reading it: only the grid digest fails → the Morton key's float arithmetic differs on that OS; the cloud digest fails → a predicate is not exact there (FMA or x87 first) |
| Delaunay rejects what does not span 3D, skips duplicates | 3 points, coplanar, collinear, all equal, NaN and the ceiling all throw. Three repeats are reported as 500, 501, 502, and the tets equal those without them. A requested token throws `Cancelled` |

---

← Back to [block 23](README.md) · [the record](02-record.md).
