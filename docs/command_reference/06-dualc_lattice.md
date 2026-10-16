# Tool 6: `dualc_lattice` (TPMS infill bounded by a mesh)

**Retired 2026-10-16.** `dualc_lattice`, the CLI that clipped a TPMS lattice to a mesh's
volume, was removed with its defaults written out as a recipe: it composed one fixed graph,
which `dualc_field` takes directly (D-55 in [the settled decisions](../decisions/01-settled.md)); tool number 6 is
never reused. The record of the retirement is [roadmap 21/08](../roadmap/21-cli-consolidation/08-retire-tools.md) ([#60](../roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools)).

The graph, the three defaults the tool computed from the mesh, and the guide to choosing
wavelength and thickness are [11/14](11-dualc_field/14-tpms-in-mesh.md) — the same bytes for
the same mesh:

| Was | Now |
| --- | --- |
| `dualc_lattice in.obj --type T` | `intersection(T(…), mesh(path="in.obj"))`; the wavelength (the mesh's AABB diagonal / 10), the centre (its AABB centre) and the box (the AABB + 5 % per axis) written in by hand, or computed by T14 ([11/14 § The graph and its defaults](11-dualc_field/14-tpms-in-mesh.md#the-graph-and-its-defaults)) |
| `--offset T`, `--normalize-thickness` | `onion(…, thickness=T)` around the TPMS, `normalize(…)` inside it |
| `-o out.obj\|.stl\|.3mf` | the same `-o` dispatch ([export formats](README.md#export-formats--o-extension-dispatch)) |
| Wavelength vs. offset — the conceptual guide | [11/14 § Wavelength vs. offset](11-dualc_field/14-tpms-in-mesh.md#wavelength-vs-offset) |
| The TPMS formulas, the six types Y1–Y6 | [11/14 § The six TPMS](11-dualc_field/14-tpms-in-mesh.md#the-six-tpms) |
| Recipes T7–T13 | Y1, T7–T14 ([11/14 § Recipes](11-dualc_field/14-tpms-in-mesh.md#recipes)); the preview: [12/05](12-dualc_field_view/05-tpms-in-mesh-preview.md) |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
