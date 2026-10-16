# Tool 4: `dualc_lift` (2D primitives + the 2 lifts)

**Retired 2026-10-16.** `dualc_lift`, the CLI that revolved or extruded one 2D profile into
3D, was removed once the 2D profiles and the two lifts became field-graph nodes (#58; D-57 in
[the settled decisions](../decisions/01-settled.md)); tool number 4 is never reused. The record of the retirement is
[roadmap 21/08](../roadmap/21-cli-consolidation/08-retire-tools.md) ([#60](../roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools)).

The same profiles, defaults and lifts, now composable with every other node, are
[11/09](11-dualc_field/09-2d-profiles-and-lifts.md) — the same bytes for the same parameters:

| Was | Now |
| --- | --- |
| `dualc_lift revolve <profile> … --offset O` | `revolve(<profile>, offset=O)` ([11/09 § The two lifts](11-dualc_field/09-2d-profiles-and-lifts.md#the-two-lifts)) |
| `dualc_lift extrude <profile> … --height H` (half-height) | `extrude(<profile>, height=H)`, still the half-height |
| The four 2D profiles `circle`, `box`, `segment`, `polygon` | `circle`, `rect` (the graph's name for `box`), `segment`, `polygon` ([11/09 § The four profiles](11-dualc_field/09-2d-profiles-and-lifts.md#the-four-profiles)) |
| The eight recipes, `revolve` and `extrude` per profile | L1–L8, each beside its old command ([11/09 § The four profiles](11-dualc_field/09-2d-profiles-and-lifts.md#the-four-profiles)); more in [§ Recipes](11-dualc_field/09-2d-profiles-and-lifts.md#recipes) |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
