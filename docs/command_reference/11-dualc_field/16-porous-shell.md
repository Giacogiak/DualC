# The porous shell (`porous`)

Part of [Tool 11: `dualc_field`](README.md). `porous` is a source node that turns a volume
into a natural sponge, foam or bone. It places seeds in the volume, tetrahedralizes them once,
and evaluates to the **unsigned distance to the cell complex**: the Voronoi walls or the
Delaunay faces (`mode=walls`), or the edges (`mode=struts`). The value is 0 on the complex and
≥ 0 everywhere, so a contour of the bare node is empty. Give it a thickness with `onion`,
`graded-onion`, `offset` or `graded-offset`, then clip it with the volume. The record is
[roadmap 23](../../roadmap/23-porous-shell/README.md); the node's invariants are
[design/12 § The porous source](../../design/12-field-graph.md#the-porous-source).

## Synopsis

```text
porous(volume[, control], spacing=S, cells=…, mode=…, dist=…, spacing2=…, d0=…, d1=…,
       ramp=…, relax=…, seed=…, smooth=…, jitter=…)
```

- **`volume`** (child 0) is read **only while the node is built**, to place the seeds; it is
  never evaluated again. The node's `bounds()` are the volume's box.
- **`control`** (child 1, optional) grades the seed spacing from `spacing` to `spacing2`
  between the control values `d0` and `d1`. `sphere(radius=0)` is the distance from a point.
- A pad of seeds one `max(spacing, spacing2)` thick is placed outside the volume, so the
  boundary cells are whole; the clip cuts them open, so the boundary pores are open.

## Parameters

| Key | Default | Meaning |
| --- | --- | --- |
| `spacing` | required | The mean seed distance (where the control is ≤ `d0`); finite, > 0 |
| `cells` | `voronoi` | `voronoi`: the cells of the seeds (foam, bone). `delaunay`: the tets' faces and edges |
| `mode` | `walls` | `walls`: closed cells, each interior cell a sealed cavity. `struts`: open cells, the edges only |
| `dist` | `poisson` | `poisson`: blue noise (Bridson, minimum distance `spacing`). `jitter`: a grid of pitch `spacing`, each point moved by up to `jitter`. `random`: white noise of density 1/`spacing`³ |
| `spacing2` | `spacing` | The seed distance where the control is ≥ `d1`; a value ≠ `spacing` needs the control and `d1` |
| `d0` | `0` | The control value where the spacing is `spacing` |
| `d1` | `0`; required when graded | The control value where the spacing is `spacing2` |
| `ramp` | `linear` | The grading curve between `d0` and `d1`: `linear`, `smooth` (smoothstep) or `gaussian` |
| `relax` | `0` | Lloyd iterations, a whole number in 0–50: more regular cells, slower to build |
| `seed` | `0` | The random stream, a whole number in 0–2⁵³; the same graph gives the same cells on any OS |
| `smooth` | `0` | The smooth-min radius that rounds the cells' corners; ≥ 0, and below `B` for `struts` |
| `jitter` | `1` | `dist=jitter` only: the displacement, in grid pitches; `0` is a regular grid |

**What the value is.** For `walls`, the distance to the nearest wall; for `struts`, the
distance to the nearest edge, capped at `B − smooth`, where `B` is the larger spacing (or the
largest cell radius, if larger). `smooth` only lowers the value, by less than `smooth`. The
field is 1-Lipschitz, so every decorator and boolean takes it as a metric field, with no
`normalize`.

**Ceilings and errors.** More than 2,000,000 seeds, or `relax` above 50, is a located error,
as is a missing `spacing`, a `spacing2` without a control, an unknown key or name. Read the
whole list from `dualc_field --list`.

## The recipes

`V` is the volume and `P` is `porous(V, …)`:

| Recipe | Graph |
| --- | --- |
| Uniform foam | `intersection(V, onion(P, thickness=t))` |
| Sponge (walls thickening outwards) | `intersection(V, graded-onion(P, control, t1=…, t2=…, d1=…))` |
| Skinned part | `union(intersection(V, onion(P, thickness=t)), onion(V, thickness=ts))` |
| Open-cell bone | `intersection(V, offset(porous(V, mode=struts, …), r=r))`, or `graded-offset` |

**Advice, from the measured cases** ([23/06](../../roadmap/23-porous-shell/06-sponge-end-to-end.md#what-the-cases-showed)):

- **Round the rim on a curved volume.** A crisp `intersection` with a sphere leaves a few
  non-manifold edges at any depth, where a pore nearly breaks through; a
  `smooth-intersection(…, k=0.1)` at depth 7 closed it on 8 seeds of 8. Flat-faced meshes
  (the bracket) are watertight with a crisp rim.
- **Delaunay walls need the node's own `smooth`**, about twice the thickness: sliver tets
  leave pores thinner than a cell.
- **Clipped struts leave loose stubs.** A skin (`union(…, onion(V, …))`) anchors them.
- **A wall must span 2–3 cells** ([the resolution rule](../../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist)):
  a wall is `2t` thick for `onion(…, thickness=t)`; a cell is the box's extent / 2^depth.
- **Write `.3mf` or `.obj` when watertightness matters**: a float32 STL can weld two vertices
  the engine kept apart.

## Cost

Measured on this project's Linux host (8 cores, 22 GB), GCC 15, Release, one process at a
time; roadmap 23 [#63](../../roadmap/23-porous-shell/02-record.md#63-deterministic-seeds-in-any-volume),
[05](../../roadmap/23-porous-shell/05-porous-field.md#measured) and
[06](../../roadmap/23-porous-shell/06-sponge-end-to-end.md#the-cases).

- **Seeds.** `poisson` places about 0.57 seeds per `spacing`³ of volume, plus the pad: a unit
  sphere holds 346 seeds at `spacing` 0.25 and 23,344 at 0.05, the pad included. The seeds are placed at about
  55,000/s in an analytic volume and 5,500/s in a mesh (each costs ~9 distance queries);
  `relax=5` cuts that by 2–6×. The tetrahedralization adds about 1.4 s per 100,000 seeds.
- **The build** is under 13 ms at the recipes' spacings (65–346 seeds) and about 0.9 s at
  23,344 seeds.
- **Memory of the cells:** 87 (Voronoi walls), 492 (Voronoi struts), 526 (Delaunay walls)
  and 368 (Delaunay struts) bytes per seed.
- **Evaluation:** walls are 10–25× faster than struts (Voronoi walls 1.28M values/s, Delaunay
  walls 3.44M, struts 112k–134k, one thread), so a `struts` part contours 1.1–1.9× slower than a walls part.
- **The contour dominates:** depth 6 takes 0.7–2.8 s and 0.30–0.44 GB, depth 7 4–23 s and
  1.6–3.2 GB, whatever the spacing. For depth 8 use the tiled path: `--mem 2G` gave the
  sponge sample 1,280,976 triangles in 54 s at a 0.44 GB peak (V8).
- **Threads.** `relax` runs on every hardware thread whatever `--threads` says; the seeds
  are bit-identical at any count.

## Through the C ABI and the viewer

The ABI's create calls take the same graph string, so `porous(mesh(id="…"))` over a host's
in-memory mesh works as it does here — the `id` may appear twice, as the volume and as the
clip ([`capi/README.md`](../../../capi/README.md#the-porous-source-from-a-host-mesh)). The
cells are built inside the create call, which takes no cancel token: contour and export
cancel as usual. `dualc_field_view` previews the same node live
([12/02](../12-dualc_field_view/02-nodes-and-examples.md#supported-nodes)); `smooth` and the
thicknesses nudge without a rebuild, every other key rebuilds the cells.

## Recipes

Run from `build/examples/`, where the `porous-*.json` samples and `bracket.obj` are copied.

| # | What it does | Depth 7 (faces, closed?) |
| --- | --- | --- |
| V1 | The sponge sample: a sphere, walls 0.04 → 0.08 outwards, the rim rounded | 318,424, watertight |
| V2 | A uniform foam in a sphere, the rim rounded | 322,880, watertight |
| V3 | A foam in a mesh: the bracket, a crisp rim | 449,460, watertight |
| V4 | A skinned sphere: foam inside, a 0.05 skin outside | 449,656, 8 non-manifold edges |
| V5 | An open-cell bone in the bracket: Delaunay struts, `offset` 0.015 | 189,024, watertight |
| V6 | A graded foam: small cells at the centre, large at the rim | 386,132, watertight |
| V7 | Delaunay walls, with the node's `smooth` | 439,348, watertight |
| V8 | The sponge at depth 8, tiled under a 2 GB budget | 1,280,976 (depth 8) |
| V9 | Look at the cells without contouring: a slice through the sponge | — |

```bash
# V1 — the sponge sample (6.8 s, 3.0 GB):
dualc_field porous-sponge.json --depth 7 -o sponge.stl
# V2 — a uniform foam in a sphere, the rim rounded (5.6 s, 2.9 GB):
dualc_field --expr "smooth-intersection(sphere(radius=1),onion(porous(sphere(radius=1),spacing=0.4),thickness=0.06),k=0.1)" --depth 7 -o foam.stl
# V3 — a foam in the bracket (13.5 s):
dualc_field porous-foam-bracket.json --depth 7 -o bracket_foam.stl
# V4 — a skinned sphere (5.4 s, 2.9 GB):
dualc_field --expr "union(intersection(sphere(radius=1),onion(porous(sphere(radius=1),spacing=0.25),thickness=0.05)),onion(sphere(radius=1),thickness=0.05))" --depth 7 -o skinned.stl
# V5 — an open-cell bone in the bracket (14.7 s, 2.5 GB):
dualc_field porous-bone-bracket.json --depth 7 -o bracket_bone.stl
# V6 — a graded foam: spacing 0.2 at the centre to 0.4 at r = 1 (5.9 s, 2.9 GB):
dualc_field --expr "smooth-intersection(sphere(radius=1),onion(porous(sphere(radius=1),sphere(radius=0),spacing=0.2,spacing2=0.4,d1=1),thickness=0.04),k=0.1)" --depth 7 -o graded.stl
# V7 — Delaunay walls rounded by the node's smooth (5.1 s, 3.1 GB):
dualc_field --expr "smooth-intersection(sphere(radius=1),onion(porous(sphere(radius=1),cells=delaunay,spacing=0.5,smooth=0.06),thickness=0.03),k=0.1)" --depth 7 -o delaunay.stl
# V8 — depth 8 through the tiled path (54 s, 0.44 GB):
dualc_field porous-sponge.json --depth 8 --mem 2G -o sponge8.stl
# V9 — the z=0 section of the sponge, PNG + SVG, in 0.2 s:
dualc_field porous-sponge.json --slice z=0 -o sponge_slice
```

---

← Back to [Tool 11: `dualc_field`](README.md) · the [Command Reference index](../README.md)
