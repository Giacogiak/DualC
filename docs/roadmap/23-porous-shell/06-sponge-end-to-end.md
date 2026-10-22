# The sponge end to end — #65, the four recipes contoured

The measurements and findings behind
[02 #65](02-record.md#65-the-porous-node-on-the-cpu)'s second half (P04 T2, 2026-10-22). It covers
the raw plan's four recipes run through `dualc_field -o out.stl` on a primitive and a mesh, for
both `cells` and both `mode`s. It also holds the component test and the three samples.

## Conditions

This Linux host (22 GB, 8 cores), GCC 15, Release; `dualc_field` with its defaults, **one
process at a time** under `prlimit --as=8000000000`; time and peak RSS from `/usr/bin/time`.
**NM** is the engine's non-manifold edge count (`Diagnostics`); no case had a boundary edge.
**Surfaces** counts connected surfaces (a union-find over faces sharing an edge, on the STL
welded by float32 position): the outer skin is one, each closed cavity adds one.

The volumes:

- **sphere**: `sphere(radius=1)`;
- **bracket**: `mesh(path="bracket.obj")`, the demo mesh of `build/data/` (1 × 1 × 0.6).

Depth 7 is about 3 GB for every case, so depth 8 was not run (the run's memory rule).

## The recipes as run

`V` is the volume and `P` is `porous(V, cells=…, spacing=S)`. "Smooth" swaps the outer
`intersection` for a `smooth-intersection(…, k=K)`.

| Recipe | Graph |
| --- | --- |
| foam | `intersection(V, onion(P, thickness=t))` |
| sponge | `intersection(V, graded-onion(P, sphere(radius=0), t1, t2, d1))` |
| skinned | `union(intersection(V, onion(P, thickness=t)), onion(V, thickness=ts))` |
| bone | `intersection(V, offset(porous(V, cells=…, mode=struts, spacing=S'), r=r))` |

| Volume | Voronoi S, t | Delaunay S, t | ts | t1, t2, d1 | Struts S', r (Voronoi; Delaunay) | K |
| --- | --- | --- | --- | --- | --- | --- |
| sphere | 0.25, 0.05 | 0.5, 0.03 | 0.05 | 0.03, 0.08, 1 | 0.25, 0.04; 0.4, 0.03 | 0.1 |
| bracket | 0.15, 0.025 | 0.3, 0.02 | 0.03 | 0.015, 0.04, 0.7 | 0.15, 0.02; 0.25, 0.015 | 0.05 |

## The complexes

From the hidden case `porous: the end-to-end cases' seeds and tets` (`[porous-bench]`; set
`DUALC_POROUS_BENCH_MESH` to the bracket's absolute path). Seeds include the pad; faces and
edges are the Delaunay complex's. Every build took under 13 ms.

| Node | Seeds (pad) | Tets | Faces | Edges | Segments | Packed |
| --- | --- | --- | --- | --- | --- | --- |
| sphere, Voronoi, S 0.25 | 346 (196) | 1,849 | 3,795 | 2,291 | 3,534 (struts) | 29 KB; struts 149 KB |
| sphere, Delaunay, S 0.5 | 79 (60) | 334 | 707 | 451 | — | 28 KB |
| sphere, Delaunay struts, S 0.4 | 123 (86) | 567 | 1,186 | 741 | 741 | 38 KB |
| sphere, S 0.4, seed 3 (the test) | 133 (90) | 630 | 1,315 | 817 | — | 11 KB |
| bracket, Voronoi, S 0.15 | 232 (167) | 1,290 | 2,636 | 1,577 | 2,092 (struts) | 19 KB; struts 85 KB |
| bracket, Delaunay, S 0.3 | 65 (55) | 281 | 593 | 376 | — | 23 KB |
| bracket, Delaunay struts, S 0.25 | 86 (69) | 400 | 834 | 519 | 519 | 27 KB |

## The cases

Each cell reads faces / NM / surfaces. The time and the peak are for the depth-7 run; depth 6
took 0.7–2.8 s and 0.30–0.44 GB.

| Volume | Cells | Recipe | Rim | Depth 6 | Depth 7 | Depth 7 time, peak |
| --- | --- | --- | --- | --- | --- | --- |
| sphere | voronoi | foam | crisp | 111,012 / 8 / 105 | 457,868 / 7 / 126 | 5.7 s, 2.9 GB |
| sphere | voronoi | sponge | crisp | 91,156 / 10 / 115 | 376,192 / 2 / 134 | 5.6 s, 3.1 GB |
| sphere | voronoi | skinned | crisp | 109,088 / 16 / 196 | 449,656 / 8 / 201 | 5.6 s, 2.9 GB |
| sphere | voronoi | foam | smooth | 103,148 / 1 / 80 | 423,624 / 2 / 78 | 6.4 s, 3.1 GB |
| sphere | voronoi | sponge | smooth | 87,444 / 3 / 93 | 359,556 / 1 / 93 | 6.5 s, 3.1 GB |
| sphere | voronoi | skinned | smooth | 110,716 / 24 / 202 | 458,300 / 29 / 208 | 6.6 s, 3.0 GB |
| sphere | voronoi | bone | crisp | 100,292 / 27 / 44 | 419,472 / 29 / 74 | 8.4 s, 2.1 GB |
| sphere | voronoi | bone | smooth | 87,428 / 23 / 22 | 363,876 / 20 / 5 | 12.3 s, 2.7 GB |
| sphere | delaunay | foam | crisp | 143,336 / 44 / 85 | 606,028 / 70 / 116 | 5.3 s, 3.1 GB |
| sphere | delaunay | sponge | crisp | 75,072 / 46 / 81 | 316,040 / 34 / 98 | 4.3 s, 3.0 GB |
| sphere | delaunay | skinned | crisp | 140,088 / 54 / 206 | 587,212 / 88 / 209 | 5.2 s, 3.1 GB |
| sphere | delaunay | foam | smooth | 129,772 / 52 / 61 | 548,844 / 60 / 54 | 5.2 s, 3.2 GB |
| sphere | delaunay | sponge | smooth | 72,928 / 31 / 71 | 306,476 / 30 / 74 | 4.6 s, 3.0 GB |
| sphere | delaunay | skinned | smooth | 142,544 / 69 / 218 | 600,380 / 120 / 240 | 5.6 s, 3.2 GB |
| sphere | delaunay | bone | crisp | 62,848 / 8 / 44 | 266,980 / 6 / 52 | 5.7 s, 1.6 GB |
| sphere | delaunay | bone | smooth | 54,496 / 6 / 43 | 230,364 / 2 / 16 | 10.3 s, 2.4 GB |
| bracket | voronoi | foam | crisp | 113,108 / **0** / 23 | 449,460 / **0** / 23 | 11.4 s, 3.0 GB |
| bracket | voronoi | sponge | crisp | 102,848 / **0** / 24 | 409,832 / **0** / 24 | 11.5 s, 3.1 GB |
| bracket | voronoi | skinned | crisp | 98,164 / 1 / 100 | 410,444 / 2 / 108 | 17.4 s, 3.0 GB |
| bracket | voronoi | foam | smooth | 98,712 / **0** / 15 | 415,152 / 1 / 15 | 16.1 s, 3.1 GB |
| bracket | voronoi | sponge | smooth | 98,212 / **0** / 23 | 399,012 / 2 / 17 | 16.6 s, 3.1 GB |
| bracket | voronoi | skinned | smooth | 98,400 / 1 / 101 | 414,576 / 3 / 111 | 23.0 s, 3.1 GB |
| bracket | voronoi | bone | crisp | 82,916 / 6 / 7 | 331,808 / 3 / 7 | 15.8 s, 2.7 GB |
| bracket | voronoi | bone | smooth | 65,512 / 5 / 16 | 270,704 / 4 / 27 | 22.2 s, 2.9 GB |
| bracket | delaunay | foam | crisp | 126,136 / 26 / 18 | 511,088 / 34 / 19 | 11.5 s, 3.1 GB |
| bracket | delaunay | sponge | crisp | 94,232 / 19 / 18 | 382,248 / 21 / 20 | 11.2 s, 3.1 GB |
| bracket | delaunay | skinned | crisp | 105,072 / 21 / 95 | 446,184 / 41 / 107 | 18.0 s, 3.1 GB |
| bracket | delaunay | foam | smooth | 111,108 / 15 / 10 | 480,516 / 28 / 11 | 16.0 s, 3.1 GB |
| bracket | delaunay | sponge | smooth | 94,216 / 14 / 19 | 387,580 / 17 / 16 | 15.1 s, 3.1 GB |
| bracket | delaunay | skinned | smooth | 105,432 / 23 / 96 | 450,840 / 42 / 110 | 21.8 s, 3.1 GB |
| bracket | delaunay | bone | crisp | 46,612 / 1 / 10 | 189,024 / **0** / 7 | 13.5 s, 2.5 GB |
| bracket | delaunay | bone | smooth | 36,324 / 1 / 45 | 150,944 / 1 / 14 | 20.5 s, 2.7 GB |

Every walls case has more than one surface: the walls enclose cavities in every case. The bone
cases' extra surfaces are loose strut stubs that the clip cut off from the network.

## What the cases showed

1. **A crisp rim on a curved volume leaves a few non-manifold edges, at every depth.**
   - Where it happens: a pore corner that nearly breaks through the sphere leaves a membrane
     thinner than a cell. A field slice through one such edge (the sphere foam at S 0.25,
     seed 0) showed a gap of about half a cell. All the edges sit at r ≈ 0.98–1.0.
   - It does not go away with depth or thickness. On the sphere foam (S 0.4, t 0.06, seeds
     0–7), the crisp rim gave 2–7 edges per seed at depth 6 and 3–16 at depth 7. At S 0.25,
     walls of t 0.08 instead of 0.05 still gave 7.
   - **It is not the porous node's.** The existing `intersection(sphere(radius=1),
     onion(normalize(gyroid(wavelength=0.5|0.7)), thickness=0.05))` gives 0 edges at depth
     6 and 6 at depth 7. This is the [resolution rule](../../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist)
     at a rim: the rule has no answer for a wedge that thins to zero. The flat-faced bracket
     shows it far less.
2. **A `smooth-intersection` rounds the rim and removes most of it.** On the sphere foam (S
   0.4, t 0.06), over seeds 0–7, these runs were watertight:

   | Rim, depth | Watertight seeds |
   | --- | --- |
   | crisp, depth 6 | 0 of 8 |
   | `k` 0.1, depth 6 | 3 of 8 |
   | `k` 0.2, depth 6 | 6 of 8 |
   | `k` 0.05, depth 7 | 2 of 8 |
   | **`k` 0.1, depth 7** | **8 of 8** |

   At S 0.25, `k` 0.1 and depth 7 left 4 of 8 seeds open. At S 0.15, t 0.03, `k` 0.1 and
   depth 7 (625,152 faces, 452 surfaces, 18.3 s, 3.1 GB), 6 edges remained. The count grows
   with the number of pores at the rim.
3. **Delaunay walls need the node's own `smooth`.** Sliver tets leave pores thinner than a
   cell, in the interior as well as at the rim. The bare `onion(porous(sphere, cells=delaunay,
   spacing=0.5), thickness=0.03)` gave 122 non-manifold edges at depth 6. With the sphere
   clip, these runs (S 0.5, t 0.03) gave:

   | Node `smooth`, rim, depth | NM edges |
   | --- | --- |
   | 0, crisp, 6 | 44 |
   | 0.03, crisp, 6 | 14 |
   | 0.06, crisp, 6 | 7 |
   | 0.1, crisp, 6 | 7 |
   | Lloyd `relax=10`, crisp, 6 | 40 |
   | **0.06 with `k` 0.1, 7** | **0** (439,348 faces, 45 surfaces, 5.2 s, 3.1 GB) |
4. **The bracket's Voronoi foam and sponge are watertight with a crisp rim** at depth 6 and 7.
   The Delaunay struts bone is watertight at depth 7.
5. **Struts cut by the clip leave loose stubs.** The bone cases count 5–74 surfaces. A skin
   (`union(…, onion(V, …))`) anchors them: the bracket's Voronoi bone with a 0.02 skin is 3
   surfaces, with 6 non-manifold edges at depth 7 and 25.0 s.
6. **Memory.** Depth 7 takes 1.6–3.2 GB whatever the recipe; depth 6 takes 0.30–0.44 GB.
7. **A float32 STL can weld two vertices** the engine kept apart: the bracket foam at depth 7
   is watertight (engine and OBJ), its STL welded by position shows 2 non-manifold edges.

## The test

`porous: a walls sponge contours watertight with more than one component` (`tests/test_field_graph.cpp`, `[field_graph][porous]`):

- **The graph:** `smooth-intersection(sphere(radius=1), onion(porous(sphere(radius=1),
  spacing=0.4, seed=3), thickness=0.06), k=0.1)`, at depth 6.
- **What it asserts:**
  - the contour is not empty;
  - 0 boundary edges and 0 non-manifold edges, from `Diagnostics`;
  - `countSurfaces` > 1. That is a union-find over faces sharing an edge, written in the test.
- **Measured:** 80,116 faces and 14 surfaces, in 1.0 s at a 0.40 GB peak. The CLI and the
  Python counter agree on both numbers.
- **The seed is a measured choice:** seeds 3–5 of 0–7 are watertight at depth 6 (finding 2);
  depth 7 would take all eight but costs 3 GB, over a committed ctest's 1 GB.

## The samples

In `examples/samples/`, copied next to `dualc_field`, each with a depth-5 smoke ctest
(`cli_field_sample_porous-*`). Their depth-7 runs:

| Sample | Graph | Depth 7 |
| --- | --- | --- |
| `porous-sponge.json` | sphere, Voronoi S 0.4, `graded-onion` t1 0.04 → t2 0.08 at d1 1 from the centre, `smooth-intersection` k 0.1 | 318,424 faces, watertight, 14 surfaces, 6.8 s, 3.0 GB |
| `porous-foam-bracket.json` | bracket, Voronoi S 0.15, `onion` 0.025, crisp `intersection` | 449,460 faces, watertight, 23 surfaces, 13.5 s, 3.0 GB |
| `porous-bone-bracket.json` | bracket, Delaunay struts S 0.25, `offset` r 0.015, crisp `intersection` | 189,024 faces, watertight, 7 surfaces, 14.7 s, 2.5 GB |

```bash
# Run from build/examples, where the samples and bracket.obj are copied.
dualc_field porous-sponge.json --depth 7 -o sponge.stl
dualc_field porous-foam-bracket.json --depth 7 -o bracket_foam.stl
dualc_field porous-bone-bracket.json --depth 7 -o bracket_bone.stl
```

## The in-run choices

Each is **Chosen in-run (pending owner review)**:

1. **The test and the sphere sample round the rim with a `smooth-intersection`.** The raw
   plan's recipes use a crisp `intersection`, which leaves rim edges on the sphere at every
   depth (findings 1 and 2). The bracket samples keep the crisp rim, which is watertight
   there.
2. **The test pins seed 3 at depth 6**, under the 1 GB cap; see the test above.
3. **No tracked item for the crisp rim, and no `design/10` line yet.** It is the engine's
   resolution rule, not the node's (finding 1). Both homes are full: `design/10` is at the
   15,360-byte cap, the decisions index at 15,092. Splitting either is the owner's call (P08).
4. **#67's command-reference page should carry findings 2, 3 and 5 as advice:** a
   `smooth-intersection` for a curved volume, `smooth` ≈ 2 × the thickness for Delaunay walls,
   a skin for struts.

## For the owner's review

Whether the samples look like a sponge is a visual judgement. **Look:** open the three STLs
of [the samples](#the-samples) in a viewer or slicer and cut a section — closed cavities
inside, the sponge's walls thickening outwards, the bone's struts. **Verified headless:**
watertightness (engine and OBJ) and the surface counts.

---

← Back to [block 23](README.md).
