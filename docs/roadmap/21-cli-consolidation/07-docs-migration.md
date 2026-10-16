# The retiring pages' content moved into the framework docs (#60, P05)

The evidence page of the first half of [#60](01-record.md#60-retire-the-eight-tools), plan
P05 T1, landed 2026-10-16 on branch `feat/cli-consolidation`. Nothing was deleted: the old
tools still run, and every migrated recipe was run once against them. The deletions are P06.

## What landed

- **Four new `dualc_field` pages**: [11/11](../../command_reference/11-dualc_field/11-mesh-in.md)
  mesh in → mesh out and *Choosing a sign oracle* (from 01), and mesh booleans with `k` sizing
  (from 03, so the framework pages hold `dualc_boolean`'s capability too, though D-60 keeps it),
  [11/12](../../command_reference/11-dualc_field/12-primitive-catalogue.md) the primitive
  catalogue P1–P30 with the decorators and domain operators (from 02),
  [11/13](../../command_reference/11-dualc_field/13-composition-samples.md) the six
  composition samples (from 05), [11/14](../../command_reference/11-dualc_field/14-tpms-in-mesh.md)
  the TPMS lattice in a mesh with its defaults written out (from 06).
- **One new `dualc_field_view` page**: [12/05](../../command_reference/12-dualc_field_view/05-tpms-in-mesh-preview.md),
  the density-independent lattice-in-mesh preview and what replaces `dualc_raymarch`'s `t` / `n`
  and wavelength / offset keys (from 10).
- **Added to existing pages**: 11/09 the table L1–L8 (04's recipes as graphs); 11/10 X6–X8
  (07's S2–S4) and `dualc_slice`'s defaults; 03 a pointer to the graph's per-node `bake=` (D-60
  kept the tool) and B8 corrected (below); a one-line pointer to the new home on 01, 02, 05, 06 and 10; the feature map
  of [`command_reference/README.md`](../../command_reference/README.md) points at the new homes.
- **Six graph files** in `examples/samples/` — `cube_minus_sphere`, `smooth_blend`,
  `displaced_sphere`, `mesh_shell`, `mesh_minus_sphere`, `twisted_mesh` (`.json`), copied next
  to `dualc_field` and `dualc_field_view` with the two older samples.
- **27 new ctest cases** (`examples/CMakeLists.txt`): per sample, `cli_csg_sample_<name>` (the
  recipe at `--depth 5`), `cli_field_sample_<name>` (the file at `--depth 5`), its content check
  and `cli_field_sample_<name>_same` (`cmake -E compare_files`); and `cli_field_lattice`
  (`cli_lattice`'s field as a graph), its content check and `cli_field_lattice_same`. No
  existing test edited.

## Recipe IDs

Recipes keep their IDs where the folder has no other recipe of that letter: P1–P30, D1–D7,
O1–O6, F1–F3, C1–C7 (02), Y1–Y6 and T7–T13 (06 — tool 11's T1–T2 are different numbers),
L1–L8 (04), M1–M8 (10). Where tool 11 already used the letter, the recipe was born again with
the old ID in a *Was* column: 01's D1–D10 are **G1–G9** (G2 and D10 folded into G1 and G9), 05's
R1–R6 are **K1–K6** (11/09 owns R1–R4), 07's S2–S4 are **X6–X8** (11/02 owns S1–S17). T14 is
new: the lattice defaults computed from any OBJ.

## Every recipe, run once

Linux, GCC 15, Ninja, Release, `build/examples/` (the viewer: `build-gl/examples/`, rebuilt
first, GL renderer NV92); outputs compared with `cmp`; the defaults of each recipe unless named.

| Recipes | Against | Result |
| --- | --- | --- |
| K1–K6, the six samples | `dualc_csg_demo --recipe=…` (cube.obj for the three mesh ones), depth 7 | all **byte-identical**: 164,268 / 58,300 / 135,132 / 272,184 / 164,268 / 150,204 faces |
| P1–P30 bare (`--bounds` on P8, P18, P28; `onion` on P29, P30) | `dualc_primitive <name>` | all 30 **byte-identical** |
| D1–D7, O1–O6, F2–F3, C1–C7 | the old page's command for each | all **byte-identical** |
| G1, G3, G4, G6–G9 (+ `winding` on torus, depth 6) | `dualc_demo` with the flag of each | all **byte-identical**; G9 on `mesh-soup.obj` 65,508 faces |
| Y1 (with and without `--bounds`), Y3 on the off-centre `knot.obj` (T14's awk, depth 6), a normalised shell on knot (depth 6) | `dualc_lattice` | all **byte-identical** (847,356 / 39,364 / 48,836 faces) |
| L1–L8 | `dualc_lift` | all **byte-identical** |
| H1, H2 (11/11's mesh booleans) | `dualc_boolean smooth-union torus cube --k 0.25`; `difference genus2 sphere --sharp` | both **byte-identical** (96,236 / 155,736 faces) |
| T8, T9; T7, T12, T13, G9 as written, K5's torus variant | T8, T9 against `dualc_lattice` | T8, T9 **byte-identical** (535,980 / 534,048); the rest run (`sealed.stl` 65,508 faces) |
| X7 | `dualc_slice` S3 | PNG and SVG **byte-identical** (3,200 segments) |
| X6, X8 | run; X6 looked at | 12,338 / 17,474 segments; X6 shows the gyroid walls inside the bunny's outline |
| M1–M8, and M4 at `thickness=0.002` | `--snapshot`, looked at | all render: the lattice in the box, cube, foot, ankle sub-box; M4 a solid bunny, at 0.002 a gyroid bunny |

Not run: G5 (`molde` at depth 9), T10, T11 and the foot lattice at depth 7 — T14's knot covers
the off-centre defaults; the foot run (`dualc_lattice … --depth 7`) wrote a 371 MB OBJ and its
graph twin was stopped before it finished.

## Found on the way

- **07's S2 sliced empty space**: `y=0` lies below the bunny (box from `y ≈ 0.025`; the tool
  warned *plane y=0 is outside the sampling bounds*), and its 1-unit wall is thicker than the
  bunny (≈ 0.15 across). X6 cuts `y=0.11` with `thickness=0.002`; 10's M4 has the same wall and
  is kept as written, with a note.
- **03's B8 carved nothing**: `difference cube.obj sphere.obj` — the radius-1 sphere swallows
  the half-unit cube, so the tool wrote the one-triangle placeholder, baked or not. B8 now
  carves `genus2.obj` (44,776 faces baked at 128³); 11/11's H2 is the same carve, unbaked.
- **The displace defaults differ**: `dualc_primitive --displace sine` meant amplitude 0.1,
  frequency 4; the `displace` node's defaults are 0.1 and 6 (`box --displace sine` against
  `displace(box,fn=sine)`: 205,644 against 191,052 faces). Written on 11/12; neither changed.
- **`dualc_raymarch --snapshot` is no reference on this host**: on NV92 its frames show
  saturated face colours, or a black box on a blue ground (M8) — where `dualc_field_view`
  renders the same graphs as expected. The M recipes were judged by eye, not against the old
  tool. Not pursued: the tool retires in P06.
- **Region padding**: `dualc_lattice`, `dualc_slice` and `dualc_raymarch` padded the mesh box
  5 % per axis; `dualc_field`'s auto-fit pads 5 % of the largest extent (`dualc_field_view` per
  axis, as the tools). On a cube they agree; elsewhere 11/14's T14 passes the tool's box.

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
