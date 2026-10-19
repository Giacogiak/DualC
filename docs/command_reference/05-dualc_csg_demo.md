# Tool 5: `dualc_csg_demo` (the 6 baked recipes)

**Retired 2026-10-16.** `dualc_csg_demo`, the CLI that built one of six hand-coded field trees
by name, was removed as-is: each recipe is now a graph file the build copies next to the
framework CLIs (D-55 in [the settled decisions](../decisions/01-settled.md)); tool number 5 is never reused. The record
of the retirement is [roadmap 21/08](../roadmap/21-cli-consolidation/08-retire-tools.md) ([#60](../roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools)).

The six recipes, as `examples/samples/*.json`, are [11/13](11-dualc_field/13-composition-samples.md)
— the same bytes for the same recipe:

| Was | Now |
| --- | --- |
| `dualc_csg_demo --recipe=NAME` | `dualc_field NAME.json -o out.obj` (the dashes in NAME become underscores) ([11/13 § The samples](11-dualc_field/13-composition-samples.md#the-samples)) |
| The mesh as the positional argument (`mesh-shell`, `mesh-minus-sphere`, `twisted-mesh`) | a copy of the sample with another `path`, or the graph inline (K5's variant) |
| Recipes R1–R6 | K1–K6 ([11/13 § Recipes](11-dualc_field/13-composition-samples.md#recipes)) |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
