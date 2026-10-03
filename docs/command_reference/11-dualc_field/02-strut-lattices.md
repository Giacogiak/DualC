# Strut lattices (`sc` · `bcc` · `fcc` · `octet`)

Part of [Tool 11: `dualc_field`](README.md). The wireframe-crystal source nodes:
parameters, tapered struts (`nodeRadius`), clipping to a mesh, and the recipe block
(infill, spatial blend, hollow, graded and tapered struts).

## Strut lattices (wireframe crystals)

Periodic **strut** (beam) lattices: a unit cell of capsule struts along the
nodes/edges of a crystal, tiled infinitely. The counterpart to the smooth TPMS
[sources](01-op-vocabulary.md#sources) — where a TPMS is a surface you thicken into a
shell, a strut lattice is already a *solid* wireframe. Roadmap
[05
#17](../../roadmap/05-tpms-lattices/01-strut-lattices.md#17-strut-based-lattices-sc-bcc-fcc-octet-truss).

| crystal | struts / cell | shape |
| --- | --- | --- |
| `sc` | 3 | simple cubic — three orthogonal axis rods |
| `bcc` | 8 | body-centered — centre node to the 8 corners |
| `fcc` | 24 | face-centered — the 6 face "X"s (face-diagonal half-struts) |
| `octet` | 36 | octet truss — `fcc` + the 12 octahedral (face-centre) edges |

Params (same for every crystal):

| param | default | meaning |
| --- | --- | --- |
| `center` | `[0,0,0]` | world position of a unit-cell centre (shifts the whole tiling) |
| `wavelength` | `1` | unit-cell side (the tiling period), world units |
| `radius` | `0.1` | metric strut **half-thickness** (the beam is `2·radius` across) |
| `nodeRadius` | = `radius` | **optional** half-thickness at the strut end-nodes → **tapered struts** (see below). Omitted ⇒ uniform struts. |

### Tapered struts (`nodeRadius`)

By default a strut is a **uniform** capsule (`radius` everywhere). Setting
`nodeRadius ≠ radius` **tapers** it: the beam is `nodeRadius` thick at **both**
end-nodes and pinches to `radius` at mid-span.

Why (stress-aligned strength, printable joints, an organic look) and how (each
segment is two exact round cones meeting at mid-span, so the field stays a true
SDF that tiles like the capsule version) are the record's
[05/02 § Phase 4](../../roadmap/05-tpms-lattices/02-strut-enhancements.md#phase-4----tapered-struts).

**How to choose the values.**
- `nodeRadius > radius` (e.g. `radius=0.03, nodeRadius=0.1`) is the usual
  fat-node look. A ratio of **2–4×** reads clearly.
- The **thinnest** part is `2·radius` (the mid-span). Give it resolution:
  `--depth` such that a cell (`root_extent / 2^depth`) is **≲ radius**, or the
  pinch fragments (the [resolution rule](../../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist)).
  `nodeRadius` sets the widest part and the bounds growth.
- Keep `nodeRadius < wavelength/2` so neighbouring nodes don't fully merge
  (unless a near-solid is what you want).
- `nodeRadius = radius` (or omitting it) is the **uniform** strut — byte-for-byte
  identical to a pre-taper lattice, so existing graphs are unchanged.
- **Inverse taper** (`nodeRadius < radius`) is legal too: thin nodes, fat
  mid-span — a beaded/knuckle look. Uncommon but occasionally useful.

Both `radius` and `nodeRadius` are **live uniforms** in
[`dualc_field_view`](../12-dualc_field_view/README.md) — `tab` to either and nudge to
reshape the taper in real time.

**Notes.**
- The field is a **true SDF** (a union of exact capsules) — no `normalize`
  needed before a metric `onion`/smooth boolean (contrast TPMS).
- Extent is **infinite** (like TPMS / `repeat`), so you must clip it, or the
  contour has nothing finite to fill. Three ways:
  - **mesh-fill** — `intersection(mesh(path="cube.obj"), bcc(…))`
  - **box-fill (closed solid)** — `intersection(box(min=…,max=…), bcc(…))`
  - **open inspection view** — bare `bcc(…)` + `--bounds` (struts are cut open
    at the sample-box walls; not watertight, fine for viewing/slicing).
- Keep `radius < wavelength/2` for a recognisable open lattice; larger radii fuse
  the nodes into a denser solid (still valid, just less "wireframe").
- **`radius` is uniform.** To make it *vary across space* (dense near a load
  path, sparse elsewhere), wrap the strut node in
  [`graded-offset`](03-graded-and-morph.md#graded-shell--graded-inflation-two-children-base-control) —
  the local half-thickness becomes `radius + t(control)`. See the graded
  strut-radius recipes below.

## Recipes

| # | What it builds |
| --- | --- |
| S1 | BCC infill of a mesh → 3MF |
| S2 | Octet truss in a box (closed solid) |
| S3 | Spatial blend — a different crystal per region, hard seam |
| S4 | Compound cell — two crystals superimposed and welded |
| S5 | `mix` morph between two different crystals (gapped by construction) |
| S6 | Clean morph — one family, graded radius via `mix` |
| S7 | Radial density grade in a mesh (dense core → light shell) → 3MF |
| S8 | Hollow struts ("straws") via `onion` |
| S9 | Graded strut radius, radial (`graded-offset`) |
| S10 | Graded strut radius, axial / load-path |
| S11 | Graded strut radius from the clip surface |
| S12 | Uniform-inflate sanity check (`t1 == t2`) |
| S13 | Tapered struts (`nodeRadius`) |
| S14 | Tapered octet in a real mesh → 3MF |
| S15 | Tapered + graded |
| S16 | Inverse taper (beaded look) |
| S17 | Mixed lattice — struts ∪ TPMS shell |

### Examples & recipes

> Bash form (single-quoted `--expr`, bare `path="…"`); on Windows PowerShell 5.1
> escape each path quote as `\"` — the [quoting rule](../../design/09-conventions.md#windows-powershell-51-quoting),
> worked forms in [`--expr` and shell quoting](01-op-vocabulary.md#--expr-and-shell-quoting).

```bash
# S1 — BCC infill of a mesh, exported to a 3MF for print (1 unit = 1 mm):
dualc_field --expr 'intersection(mesh(path="cube.obj"),bcc(wavelength=0.5,radius=0.05))' \
            --depth 7 -o bcc_cube.3mf

# S2 — Octet truss in a box (closed solid) — the manufacturing workhorse:
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),octet(wavelength=0.5,radius=0.05))' \
            --bounds -1.2,-1.2,-1.2,1.2,1.2,1.2 --depth 7 -o octet_box.obj

# S3 — Spatial blend — a DIFFERENT crystal per region (BCC in x<0, FCC in x>0), hard
# seam, works today by clipping each crystal to a half-box and unioning:
dualc_field --expr 'union(intersection(box(min=[-1,-1,-1],max=[0,1,1]),bcc(wavelength=0.5,radius=0.06)),intersection(box(min=[0,-1,-1],max=[1,1,1]),fcc(wavelength=0.5,radius=0.06)))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o blend_spatial.obj

# S4 — Compound cell — BOTH crystals superimposed in the same space and welded (a
# denser hybrid lattice, NOT a spatial transition -- the weld piles up at the
# bcc cell-centre hub):
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),smooth-union(bcc(wavelength=0.5,radius=0.05),fcc(wavelength=0.5,radius=0.05),k=0.1))' \
            --bounds -1.2,-1.2,-1.2,1.2,1.2,1.2 --depth 6 -o compound_box.obj

# S5 — Value-blend MORPH between two DIFFERENT crystals via `mix` (value = lerp(A,B,w),
# control = plane x=0, w ramps 0->1 as x goes -0.6->0.6). mix blends distance VALUES,
# so bcc/fcc struts taper to nothing at the mid-plane and the result is TWO bodies
# with a gap slab -- inherent, not a bug (design/10 § mix). Only for a deliberately
# faded look; for a CONTINUOUS body GRAFT instead (03 § Recipes, J1):
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),mix(bcc(wavelength=0.5,radius=0.06),fcc(wavelength=0.5,radius=0.06),plane(1,0,0,0),lo=-0.6,hi=0.6))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o morph_bcc_fcc.obj

# S6 — CLEAN morph — same crystal family, only the strut RADIUS changes across x (thin
# 0.03 -> thick 0.09). Struts COINCIDE, so the blend is a graded radius on ONE
# connected lattice: no gap, one watertight body -- the only truly fluid mix form
# (graded-offset is the cheaper equivalent, 03 § Spatial morph):
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),mix(bcc(wavelength=0.5,radius=0.03),bcc(wavelength=0.5,radius=0.09),plane(1,0,0,0),lo=-0.6,hi=0.6))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o morph_radius.obj

# S7 — RADIAL density grade clipped to a mesh, for print — a dense octet CORE thinning
# to a light octet SHELL with distance from the centre (control = sphere(radius=0)
# = |p|; radius 0.05 where |p|<=0.3 down to 0.02 where |p|>=0.9). SAME family, so the
# struts coincide -> ONE continuous body (a radial graded radius). 3MF = 1mm:
dualc_field --expr 'intersection(mesh(path="cube.obj"),mix(octet(wavelength=0.4,radius=0.05),octet(wavelength=0.4,radius=0.02),sphere(radius=0),lo=0.3,hi=0.9))' \
            --depth 7 -o core_shell.3mf

# S8 — Hollow struts ("straws") — onion turns each solid strut into a tube. The wall
# spans radius-thickness .. radius+thickness (here 0.09..0.15, a 0.06 wall) and
# the core (axis) is empty. The 0.06 wall needs depth 7 on a 1.5-unit box (cell
# ~0.012, the resolution rule); a depth-6 run looks rough at the joints.
dualc_field --expr 'intersection(mesh(path="cube.obj"),onion(bcc(wavelength=0.5,radius=0.12),thickness=0.03))' \
            --depth 7 -o hollow_bcc.obj

# S9 — Graded strut radius (radial) — SOLID struts thickened by a control field via
# graded-offset (inflate: base - t). control = distance from the origin
# (sphere radius 0): t=0 at the centre (thinnest, radius 0.02) ramping to +0.06
# at |p|=1 (radius 0.08, 4x fatter). Use graded-offset (NOT onion) for a solid
# lattice -- onion would hollow each strut into a tube.
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),graded-offset(bcc(wavelength=0.4,radius=0.02),sphere(radius=0),t1=0.0,t2=0.06,d0=0.0,d1=1.0))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o graded_bcc.obj

# S10 — Graded strut radius (axial / load-path) — thin at one face, fat at the other.
# control = plane with normal +x (signed distance along x): struts run from
# radius 0.02 at x=-1 (control=-1 -> t1) to 0.07 at x=+1 (control=+1 -> t2). This
# is the "dense near the load path" pattern -- swap t1/t2 to reverse the taper.
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),graded-offset(octet(wavelength=0.4,radius=0.02),plane(1,0,0,0),t1=0.0,t2=0.05,d0=-1.0,d1=1.0))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o graded_octet_axial.obj

# S11 — Graded strut radius (from the clip surface) — reuse the CLIP mesh as the
# control so struts fatten toward the part's skin (control = signed distance,
# negative inside): t1 deep inside -> t2 near the wall. One mesh, two roles.
dualc_field --expr 'intersection(mesh(path="cube.obj"),graded-offset(bcc(wavelength=0.4,radius=0.02),mesh(path="cube.obj"),t1=0.06,t2=0.0,d0=-0.5,d1=0.0))' \
            --depth 7 -o graded_skin.obj

# S12 — Uniform-inflate sanity check — t1==t2 makes graded-offset identical to a plain
# offset (same mesh). Useful to confirm the base radius + t arithmetic.
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),graded-offset(bcc(wavelength=0.4,radius=0.02),sphere(radius=0),t1=0.04,t2=0.04,d0=0.0,d1=1.0))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o graded_uniform.obj

# S13 — Tapered struts — nodeRadius fattens the joints, radius pinches the spans (an
# organic / stress-aligned truss). Here nodes are 0.1 thick, mid-spans 0.03.
# Leave nodeRadius off for uniform struts. Mind --depth: 2*radius must resolve.
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),bcc(wavelength=0.5,radius=0.03,nodeRadius=0.1))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o taper_bcc.obj

# S14 — Tapered octet in a real mesh — the stress-aligned print workhorse: fat joints
# (nodeRadius 0.06) for robust nodes, pinched spans (radius 0.02) to save mass.
# Exported to 3MF (1 unit = 1 mm). Mind --depth: 2*radius=0.04 must resolve.
dualc_field --expr 'intersection(mesh(path="cube.obj"),octet(wavelength=0.4,radius=0.02,nodeRadius=0.06))' \
            --depth 8 -o taper_octet_cube.3mf

# S15 — Tapered + graded — compose the two: a tapered octet whose whole
# lattice also thickens outward (graded-offset inflates the already-tapered SDF).
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),graded-offset(octet(wavelength=0.4,radius=0.02,nodeRadius=0.05),sphere(radius=0),t1=0.0,t2=0.04,d0=0.0,d1=1.0))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o taper_graded.obj

# S16 — Inverse taper (nodeRadius < radius) — thin nodes, fat mid-span: a beaded /
# knuckled look. Legal and occasionally useful; not the usual profile.
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),bcc(wavelength=0.5,radius=0.08,nodeRadius=0.03))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o beaded_bcc.obj

# S17 — Mixed lattice — struts UNION a TPMS shell in one field, contoured once:
dualc_field --expr 'intersection(mesh(path="cube.obj"),union(bcc(wavelength=0.5,radius=0.04),onion(normalize(gyroid(wavelength=0.5)),thickness=0.04)))' \
            --depth 6 -o strut_plus_gyroid.obj
```

Every recipe previews live, preview == export, in
[`dualc_field_view`](../12-dualc_field_view/02-nodes-and-examples.md#recipes) (the
tapered-octet and morph previews are its P12 and P13).

JSON is equivalent — a strut source is a leaf node with no `in` (add
`"nodeRadius"` for a taper):
```json
{ "op": "intersection", "in": [
    { "op": "mesh", "path": "cube.obj" },
    { "op": "bcc", "wavelength": 0.5, "radius": 0.03, "nodeRadius": 0.08 } ] }
```

A `mix` morph is a three-child node — `in` = `[A, B, control]`, params `lo`/`hi`
(bcc→fcc across x, clipped to a box):
```json
{ "op": "intersection", "in": [
    { "op": "box", "min": [-1,-1,-1], "max": [1,1,1] },
    { "op": "mix", "lo": -0.6, "hi": 0.6, "in": [
        { "op": "bcc", "wavelength": 0.5, "radius": 0.06 },
        { "op": "fcc", "wavelength": 0.5, "radius": 0.06 },
        { "op": "plane", "params": [1,0,0,0] } ] } ] }
```

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
