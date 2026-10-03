# Supported nodes and worked examples

Part of [Tool 12: `dualc_field_view`](README.md). The node vocabulary the previewer
accepts (identical to `dualc_field`'s — see [11 § Op
vocabulary](../11-dualc_field/01-op-vocabulary.md)) and the commented example block.

## Supported nodes

The codegen accepts the **same node vocabulary as `dualc_field`** —
[11 § Op vocabulary](../11-dualc_field/01-op-vocabulary.md#op-vocabulary): the 6 TPMS,
the 4 strut lattices (with `nodeRadius`), all 30 analytic primitives (the open
`triangle`/`quad` paired with `onion`, the infinite ones clipped by the sample
bounds), the 7 booleans, every decorator and domain operator, the three-child
`mix`, and the `mesh` / `winding` sources (baked to a `sampler3D`). Every scalar
parameter is a live uniform (`tab` + nudge), including the `graded-*` ramps and
`mix`'s `lo`/`hi` — and `mix` blends *values*, so between two different crystals it
leaves a gap ([design/10](../../design/10-invariants-and-tolerances.md#mix-blends-values-not-shapes)). Every node is
value-verified against the C++ field by `dualc_glsl_parity`; the current case
count is the *Latest verification* line of
[17 § Tracked items](../../roadmap/17-code-audit-and-hardening/README.md#tracked-items--status-at-a-glance).
Emission is a deduped DAG: a duplicated subtree compiles once and a duplicated
`mesh`/`winding` bakes once. A genuinely unknown op is rejected with a located
error.

## Recipes

| # | What it shows |
| --- | --- |
| P1 | Composed analytic CSG (perforated cube) from `--expr` — the controls walkthrough graph |
| P2 | A field-graph file with a mesh source (the `sampler3D` path), auto-reloaded on save |
| P3 | Smooth union of two spheres — nudge `k` live |
| P4 | Twisted bar — nudge `radiansPerUnit` live |
| P5 | Graded-thickness gyroid in `foot.obj` — nudge `t1`/`t2`/`d0`/`d1` |
| P6 | The same grade, mesh-free (radial from a point) |
| P7 | Graded strut radius (`graded-offset`), radial |
| P8 | Graded strut radius, axial / load-path |
| P9 | Tapered struts (`nodeRadius`) — nudge `radius`/`nodeRadius` |
| P10 | A lattice too dense to contour, with explicit `--bounds` |
| P11 | Sharpen a mesh clip: `--grid-res 256` over a `--bounds` sub-region |
| P12 | Tapered octet, live (`--preview-scale` on a weak GPU) |
| P13 | A `mix` morph, live — slide `lo`/`hi` |
| P14 | Headless PNG (`--snapshot`) |
| P15 | Headless PNG of an infinite TPMS with `--bounds` |
| P16 | `--es` portability check, headless |

### Examples

Run these from `build/examples/Release/` (the demo meshes + sample graphs are
copied next to the binary). On PowerShell prefix `.\` and append `.exe`; on a
POSIX shell prefix `./`. The `\` line-continuation below is bash — on PowerShell
put each command on one line.

> **Shell quoting.** The blocks are bash form; every recipe with a `mesh`/`winding`
> path gets a paired PowerShell 5.1 block with each path quote written `\"` — the
> [quoting rule](../../design/09-conventions.md#windows-powershell-51-quoting), worked forms in
> [11 › `--expr` and shell quoting](../11-dualc_field/01-op-vocabulary.md#--expr-and-shell-quoting).

**Interactive** (a window opens — orbit/zoom/edit with the [controls](01-controls.md#controls)):

```bash
# P1 — A composed analytic CSG (perforated cube) straight from text shorthand. This is
# the graph the controls page's walkthrough uses: tab to wavelength/thickness/radius
# and nudge with the arrows.
dualc_field_view --expr "difference(intersection(box(min=[-2,-2,-2],max=[2,2,2]),\
onion(normalize(gyroid(wavelength=1)),thickness=0.3)),sphere(radius=1))"

# P2 — A field-graph FILE: a mesh source clipped by a gyroid lattice (the sampler3D
# path). Edit mesh_lattice.json in a text editor and save: the viewer reloads it
# on its own (`l` forces a reload).
dualc_field_view mesh_lattice.json

# P3 — A mesh-free smooth union of two spheres -- tab to k and nudge to watch the
# fillet grow/shrink live.
dualc_field_view --expr "smooth_union(sphere(center=[-0.6,0,0],radius=0.7),sphere(center=[0.6,0,0],radius=0.7),k=0.4)"

# P4 — A twisted bar (domain warp) -- tab to radiansPerUnit and nudge.
dualc_field_view --expr "twist(box(min=[-1.2,-0.4,-0.4],max=[1.2,0.4,0.4]),radiansPerUnit=1.4,axis=x)"

# P5 — GRADED-THICKNESS lattice in a real mesh: a gyroid filling foot.obj whose walls
# are THICK inside a control sphere placed in the instep and thin out 45 mm beyond
# it. tab to t1/t2/d0/d1 and nudge to reshape the gradient live (no recompile).
# Run from the repo root so data/foot.obj resolves.
dualc_field_view --expr 'intersection(graded-onion(normalize(gyroid(wavelength=16)),sphere(center=[0,0,35],radius=15),t1=4.0,t2=1.6,d0=0,d1=45),mesh(path="data/foot.obj"))'

# P6 — Same idea, mesh-free (instant, no sampler3D bake): radial grade from a point,
# thin core -> thick rim, clipped to a box.
dualc_field_view --expr "intersection(graded-onion(normalize(gyroid(wavelength=2)),sphere(radius=0),t1=0.04,t2=0.18,d0=2,d1=7),box(min=[-8,-8,-8],max=[8,8,8]))"

# P7 — GRADED STRUT RADIUS (graded-offset — SOLID inflation, not a hollow shell): a
# bcc strut lattice whose struts are thin at the origin (radius 0.02) and thicken
# to 0.08 at the box corners. tab to t1/t2/d0/d1 and nudge to reshape the radius
# gradient live; the preview matches dualc_field's export exactly.
dualc_field_view --expr "intersection(box(min=[-1,-1,-1],max=[1,1,1]),graded-offset(bcc(wavelength=0.4,radius=0.02),sphere(radius=0),t1=0.0,t2=0.06,d0=0.0,d1=1.0))"

# P8 — Axial (load-path) grade: struts thin at x=-1, fat at x=+1 via a plane control.
dualc_field_view --expr "intersection(box(min=[-1,-1,-1],max=[1,1,1]),graded-offset(octet(wavelength=0.4,radius=0.02),plane(1,0,0,0),t1=0.0,t2=0.05,d0=-1.0,d1=1.0))"

# P9 — TAPERED struts (nodeRadius): fat joints (0.1), pinched spans (0.03). tab to
# radius / nodeRadius and nudge to reshape the taper live.
dualc_field_view --expr "intersection(box(min=[-1,-1,-1],max=[1,1,1]),bcc(wavelength=0.5,radius=0.03,nodeRadius=0.1))"

# P10 — A dense lattice that would OOM the contourer (~100 cells/axis in a 10-unit box);
# infinite/large fields take an explicit --bounds.
dualc_field_view --expr "onion(gyroid(wavelength=0.1),thickness=0.6)" --bounds -5,-5,-5,5,5,5

# P11 — SHARPEN A MESH CLIP. --bounds is also the mesh bake region, so the two flags
# compose: this puts the same grid over a 28 mm sub-region of a ~150 mm foot
# (0.09 mm cells at 256^3 instead of 1.6 mm at the default 96^3 over the whole
# part). The startup line reports "@ 256^3 (cell ...)" so you can see what you got.
dualc_field_view --grid-res 256 --bounds -14,-34,41,14,-6,69 --expr 'intersection(mesh(path="data/foot.obj"),graded-onion(normalize(gyroid(wavelength=6)),plane(0,1,0,0),t1=1.5,t2=0.5,d0=-34,d1=-6))'

# P12 — Tapered octet, live: tab to radius / nodeRadius and nudge to reshape the
# taper in real time. On a weak GPU add --preview-scale 0.35 for a smooth orbit.
dualc_field_view --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),octet(wavelength=0.5,radius=0.03,nodeRadius=0.08))'

# P13 — A morph, live: tab to the mix node's lo / hi and slide them to sweep the
# bcc->fcc transition band across the part in real time.
dualc_field_view --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),mix(bcc(wavelength=0.5,radius=0.06),fcc(wavelength=0.5,radius=0.06),plane(1,0,0,0),lo=-0.6,hi=0.6))'
```

The two recipes above that name `data/foot.obj`, in **PowerShell 5.1 form**
(verbatim copy-paste; identical apart from the `\"` path quotes and the `.exe`):

```powershell
# GRADED-THICKNESS lattice in a real mesh (run from the repo root)
.\dualc_field_view.exe --expr 'intersection(graded-onion(normalize(gyroid(wavelength=16)),sphere(center=[0,0,35],radius=15),t1=4.0,t2=1.6,d0=0,d1=45),mesh(path=\"data/foot.obj\"))'

# SHARPEN A MESH CLIP: the same grid over a 28 mm sub-region of the foot
.\dualc_field_view.exe --grid-res 256 --bounds -14,-34,41,14,-6,69 --expr 'intersection(mesh(path=\"data/foot.obj\"),graded-onion(normalize(gyroid(wavelength=6)),plane(0,1,0,0),t1=1.5,t2=0.5,d0=-34,d1=-6))'
```

**Headless** (`--snapshot` opens no window — it renders one frame to a PNG and
exits; for scripted / visual verification):

```bash
# P14 — One PNG of the mesh-clipped lattice, no window.
dualc_field_view mesh_lattice.json --snapshot preview.png

# P15 — An infinite TPMS needs --bounds, headless or not.
dualc_field_view --expr "neovius(wavelength=0.5)" --bounds -1,-1,-1,1,1,1 --snapshot neovius.png

# P16 — Portability check: render with the WebGL2-subset shader (#version 300 es)
# instead of desktop 330 core. The body is identical; renders the same here.
dualc_field_view --expr "sphere(radius=1)" --es --snapshot es.png
```

`--bounds` is needed only for **infinite** fields (a bare `plane` /
`infinitecylinder` / `infinitecone` / `repeat`) — finite graphs auto-fit to the
field's own bounds.

---

← Back to the [Tool 12 overview](README.md) · the [Command Reference index](../README.md).
