# The complex and its bins as built — #64, the packed complex

The numbers and rules behind [02 #64](02-record.md#64-delaunay-tetrahedralization-and-the-packed-complex)'s
second half (P03 T2, 2026-10-21): the dual of the tets of [03](03-delaunay-engine.md), packed
in the arrays #66 uploads and #65 reads, and the distance query of
[01 § (c)](01-study.md#c-the-distance-formulas) over them.

## What was built

| File | What |
| --- | --- |
| `include/dualc/porous.h` | `PorousCells { Voronoi, Delaunay }`, `PorousMode { Walls, Struts }` |
| `src/internal/porous_complex.h`, `src/implicit/porous_complex.cpp` | `PorousDual` + `derivePorousDual`; `PorousComplex` + `packPorousComplex` / `buildPorousComplex`; `porousDistance`; the shared pieces (`porousSmin`, `porousSmoothFold`, the bisector, segment and plane distances) |
| `CMakeLists.txt` | the new source, `-ffp-contract=off` like the seeds and the tets |

**The one function.** `buildPorousComplex(seeds, cells, mode, cancel)` rounds the seeds to
float, tetrahedralizes exactly those points (so the packed seeds *are* the triangulated ones),
derives the dual and packs one case; `porousDistance(complex, p, smooth)` answers. One
`PorousDual` serves the four cases through `packPorousComplex`.

**The dual** (doubles): each Delaunay face once, as face (t, k) where `neighbours[4t+k]` is
−1 or > t, with its two tets (the Voronoi edge); each edge once, sorted; each seed's
neighbours (CSR, ascending); each tet's circumcentre, clamped to the padded box and counted
when outside (or when its double determinant disagrees with the exact `orient3d`).

## The packed layout (what #66 uploads)

All indices `int32` (−1 = none), all geometry `float`. The bin grid is `origin[3]`, `size`,
`dims[3]`, bin `(i, j, k)` at flat `(k·dims[1] + j)·dims[0] + i`, a point outside falling in
the nearest boundary bin.

| Case | Arrays | Bins |
| --- | --- | --- |
| Voronoi walls | `seeds` (x, y, z, 1 if pad), `neighbourOffsets` (n + 1), `neighbours` | the smaller spacing over the padded box; `binStart` = a seed near each bin centre (rule 7) |
| Delaunay walls | `tetPlanes` (4 × (n, d) per tet, face k opposite vertex k, n outward; a shared face is exactly (n, d) and (−n, −d)), `tetNeighbours` | the same grid; `binStart` = the tet holding the centre |
| Struts (both) | `segments` (a.xyz, 0, b.xyz, 0), `binOffsets`, `binItems` (every bin a segment's box touches, in segment order) | `B` = max(the larger spacing, the largest cell radius) over the volume's box grown by `B`; `bound` = `B` |

## The rules

Each is **Chosen in-run (pending owner review)** where it refines the defaults table.

1. **The fold order is ascending distance, ties by packed index** (the table said packed
   order). Ascending, the accumulator never exceeds the minimum m, so a primitive at m + k or
   beyond changes nothing: the value depends only on the primitives within k of m, is a
   symmetric continuous function of them, and stays 1-Lipschitz. In packed order a far
   primitive folded first changes the result, which makes the value depend on which bins
   were searched; the GPU can keep the few candidates within k in a small sorted array.
2. **Struts are clamped at 0 too**, as the walls are: a smoothed junction dips below 0, and an
   unsigned field under `onion`'s `|·|` would then thin the material there.
3. **Delaunay walls are 0 outside the seeds' hull** (solid). The walk only leaves by a hull
   face when p is beyond that face's plane, so outside the hull. 0 is continuous with the
   faces, keeps the field 1-Lipschitz, and needs no extra texture; the plane of the face the
   walk left by (the study's rule) depends on the path.
4. **The struts' bin is the larger of the larger spacing and the largest cell radius** (the
   plan's "bin ≥ the largest cell radius"): the largest circumradius over the tets that have
   a non-pad vertex and a circumcentre in the volume's box. Measured: 1.22 × the spacing
   (poisson), 1.17 × the larger spacing (graded 2:1), 0.87 × (regular grid).
5. **The drop rule.** Struts are clipped to the volume's box grown by `B`; a Voronoi edge
   (or a hull face's ray) that misses it is dropped. A volume point is at least `B` from
   anything outside that box, so `min(distance, B)` cannot see the cut, for any volume.
   Segments that are a point, or the same float segment twice (the two triangles of a
   regular grid's square give one Voronoi edge), are dropped too, else the fold would count
   one strut twice. Walls drop nothing: the walks need the whole graph. A tighter rule needs
   the volume's values and a Lipschitz bound, which only #65 has.
6. **Walks are capped at 1,024 steps** (then 0). Measured maximum: 3 seed steps, 20 tet steps.
7. **Start bins and the bin cap.** A bin's start seed is the one nearest its centre among the
   seeds in it; an empty bin takes the nearest of its 26 neighbours' starts, by a forward and
   a backward sweep (O(bins); a per-bin nearest search was quadratic in an empty box). The
   walk only starts there, so this changes speed, not values. A grid past `kPorousMaxBins` =
   2048 × 8192 (a sparse volume in a large box) gets bins 1.25 × coarser until it fits; the
   struts' `B` and visible box grow with them. Tested with the cap lowered to 4,096.

## Tested (`ctest -R porous`, 5 new cases)

- **The dual adjacency**, on blue noise in a sphere (601 seeds, 3,351 tets) and the regular
  grid (335 seeds, 1,288 tets): every face once and ascending; each face's tets hold it and are
  each other's neighbour across it; 4T = 2F_int + F_hull; edges unique and sorted, V − E + F
  − T = 1; the neighbour lists ascending and equal to the edges; circumcentres equidistant
  from their corners and from their faces' vertices, or clamped into the box (301 of 3,351
  on the sphere, the pad's hull slivers); a shared face's planes exactly opposite.
- **The bin query equals brute force** on 10k points over the padded box and half a spacing
  beyond, both sets, the four cases, k = 0 and k = 0.3 × the spacing. Brute force: the nearest
  seed by scanning, its neighbours read off the tets (and, at k = 0, the bisectors of every
  seed); the tet located by exact `orient3d` over every tet; every packed segment. **All 16
  runs are bit-equal** (the Delaunay walls allow 1e-6 for a point within rounding of a face;
  none needed it). Against the geometry, Delaunay walls match every face's point–triangle
  distance within 8.9e-8 (float planes). No point in a volume seed's cell reaches `B`.
- **Determinism**: two builds give the same arrays, and the four digests are pinned.
- **A capped grid**: two clusters 30 apart, cap 4,096: coarser bins, still equal to brute force.
- **Errors and cancel**: a `SeedSet` with a short `isPad` or no spacing throws
  `std::invalid_argument`; a requested token throws `Cancelled` from the dual and every pack.

## Measured: 100k seeds

`build/tests/dualc_tests "porous: complex sizes and query rates"` (hidden); this Linux host,
GCC 15, Release, one thread, two runs, the slower given. Seeds: `poisson`, box ±0.65,
spacing 0.0245: **99,712 seeds**, 646,985 tets, 1,294,262 faces, 746,988 edges, 35,100
clamped circumcentres; dual 1.83 s, the tetrahedralization most of it. Queries: 200k uniform points
in the padded box.

| Case | Packed | **B / seed** | Elements per seed | Pack | Queries/s k = 0 / k > 0 |
| --- | --- | --- | --- | --- | --- |
| Voronoi walls | 8.7 MB | **87.0** | seeds 4, offsets 1, neighbours 15.0, start bins 1.76 | 0.37 s | 1.39M / 1.09M |
| Voronoi struts | 49.0 MB | **491.8** | segments 99.5 (12.4 segments), offsets 0.98, items 22.5 | 1.13 s | 215k / 91k |
| Delaunay walls | 52.5 MB | **526.1** | planes 103.8 (6.5 tets), neighbours 26.0, start bins 1.76 | 0.61 s | 2.07M / 1.90M |
| Delaunay struts | 36.7 MB | **367.7** | segments 59.9 (7.5 edges), offsets 0.98, items 31.0 | 0.55 s | 227k / 93k |

Walls: 0.50 seed steps (max 2) and 1.55 tet steps (max 20) per query. Struts: `B` = 0.0300 (cell radius 1.22 × 0.0245), 46³ bins, items per bin mean 23.0 / max 56
(Voronoi; 54,057 of 1,294,262 edges dropped outside, 1 a point) and 31.8 / 89 (Delaunay);
73–76 and 160–169 items visited per query at k = 0 and k > 0. Graded 0.03 → 0.06 along x (27,192
seeds): 96 / 463 / 533 / 324 B per seed, `B` = 0.070, items per bin mean 53 / 61, max 179 /
170: a bin sized for the sparse side holds up to 8 × (= (0.06/0.03)³) more on the dense side.

**For #66's texture limits** (2048 wide, items at `ivec2(i % 2048, i / 2048)`): a 2048 × 8192
texture holds 16.7M texels. Per seed, the largest array is the tet planes (26 RGBA texels:
≈ 645k seeds), the Voronoi segments (24.9 texels: ≈ 673k), the Delaunay struts' bin items
(31 R32 texels: ≈ 540k) and the Voronoi neighbours (15: ≈ 1.1M). Past those the node takes
`emitBakedGrid`, as the study planned; 2M seeds fits no case at that size.

---

← Back to [block 23](README.md).
