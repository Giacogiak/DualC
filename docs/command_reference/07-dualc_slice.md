# Tool 7: `dualc_slice` (field cross-section sampler)

**Retired 2026-10-16.** `dualc_slice`, the CLI that sampled a TPMS-in-mesh field on an axis
plane into a PNG heatmap and an SVG contour, was removed once `dualc_field --slice` could
slice any graph (#59; D-58 in [the settled decisions](../decisions/01-settled.md)); tool number 7 is never reused. The
record of the retirement is [roadmap 21/08](../roadmap/21-cli-consolidation/08-retire-tools.md) ([#60](../roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools)).

Slicing is [11/10](11-dualc_field/10-slice.md) — the same PNG and SVG bytes for the same field:

| Was | Now |
| --- | --- |
| `dualc_slice in.obj --type T …` | the TPMS-in-mesh graph of [11/14](11-dualc_field/14-tpms-in-mesh.md) with `--slice` ([11/10 § Synopsis](11-dualc_field/10-slice.md#synopsis)) |
| `--plane AXIS=VALUE`, `--res N`, `--bounds` | `--slice AXIS=VALUE`, `--res N`, `--bounds` ([11/10 § Options](11-dualc_field/10-slice.md#options)); the tool's defaults are written there |
| `-o base.png` → `base.png` + `base.svg` | the same pair ([11/10 § What it writes](11-dualc_field/10-slice.md#what-it-writes)) |
| Recipes S1–S4 | X4, X6–X8 ([11/10 § Recipes](11-dualc_field/10-slice.md#recipes)) |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
