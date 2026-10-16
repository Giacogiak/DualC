# Tool 10: `dualc_raymarch` (GPU raymarch viewer)

**Retired 2026-10-16.** `dualc_raymarch`, the opt-in GPU viewer that sphere-traced one
TPMS-in-mesh field, was removed as-is, and its build option `DUALC_BUILD_RAYMARCH_VIEWER` with
it: `dualc_field_view` sphere-traces any field graph, that one included (D-55 in
[the settled decisions](../decisions/01-settled.md)); tool number 10 is never reused. The record of the retirement is
[roadmap 21/08](../roadmap/21-cli-consolidation/08-retire-tools.md) ([#60](../roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools)).

The same density-independent preview, as a graph, is
[12/05](12-dualc_field_view/05-tpms-in-mesh-preview.md):

| Was | Now |
| --- | --- |
| `dualc_raymarch in.obj --type T …` and its CLI flags | the TPMS-in-mesh graph with the tool's defaults written in, `dualc_field_view --expr …` ([12/05 § The graph and its defaults](12-dualc_field_view/05-tpms-in-mesh-preview.md#the-graph-and-its-defaults)); built with `-DDUALC_BUILD_FIELD_VIEW=ON` |
| The wavelength and offset keys (`↓`/`↑`, `PgDn`/`PgUp`), `t` (TPMS family), `n` (normalisation) | `tab` + nudge for the two values; edit the file, which reloads on save, for the other two ([12/05 § What replaces the hotkeys](12-dualc_field_view/05-tpms-in-mesh-preview.md#what-replaces-the-hotkeys)) |
| Orbit, zoom, reset, the section planes | the same keys ([12/01 § Controls](12-dualc_field_view/01-controls.md#controls)) |
| `--snapshot PATH` | `dualc_field_view --snapshot PATH` ([12 § CLI flags](12-dualc_field_view/README.md#cli-flags)) |
| Recipes M1–M8 | M1–M8 ([12/05 § Recipes](12-dualc_field_view/05-tpms-in-mesh-preview.md#recipes)) |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
