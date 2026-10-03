# Shared behaviour: rejected parameters and diagnostics warnings

What every exporting tool does the same way, stated once: which parameters are
refused at build time and how the refusal reads, the engine's runtime warnings
with what to do about each, and how the output file lands on disk. The tool
index, the shared options and the export formats are on the
[command reference index](README.md).

## Invalid parameters are rejected, not absorbed

A parameter that cannot mean anything is refused at build time instead of
producing NaNs, an empty field, or an inverted solid. The tool prints the message
with the **offending node's locator** — a JSON pointer for JSON input, a character
offset for `--expr` — and exits non-zero:

```
$ dualc_field --expr "union(sphere(radius=1),gyroid(wavelength=0))" ...
[dualc_field] field-graph error at 23: GyroidField: wavelength must be > 0 (got 0.000000)
```

Offset 23 is the `gyroid`, not the enclosing `union`. What is rejected: a
negative `radius`; a `wavelength` of 0 or less on any TPMS node; a `scale` of
0; a negative `repeat` period; a `repeat-limited` count below 1; a `transform`
whose matrix is not a rotation + translation (scale, shear and reflection are
not invertible the way the engine inverts it — use `scale` and `mirror`); and a
`--depth` that is negative or below the minimum depth.

Legal despite looking degenerate: `radius=0` (a distance-to-centre field — the
control input of the graded recipes on [page 11](11-dualc_field/README.md)), a `0`
component in a `repeat` period (that axis is not repeated), and a large
`--depth` (legitimate with `--tile-depth` / `--mem`; no ceiling — mind your RAM).
The `BBox` sentinels behind the bounds checks are
[design/10 § Bounding-box sentinels](../design/10-invariants-and-tolerances.md#bounding-box-sentinels);
the rationale is
[roadmap 17 #34](../roadmap/17-code-audit-and-hardening/08-argument-validation.md).

## Diagnostics warnings (every exporting tool)

Every tool that exports through the shared writer (`dualc_field`,
`dualc_primitive`, `dualc_boolean`, `dualc_lattice`, `dualc_lift`,
`dualc_csg_demo`) prints the engine's raised signals to **stderr** before the
`[dualc] wrote ...` line — always on, no flag, silent on a clean run. The
**tiled/streaming** path (`--tile-depth`) runs none of the per-mesh checks — it
contours per tile, where an empty tile is normal — and prints one warning of its
own, `[dualc] warning: the field produced an empty mesh -- no surface crossed the
sampled region.`, if *no* tile produced a surface.

| Warning | What happened | What to do |
| --- | --- | --- |
| `no surface crossed the sampled region -- the output is a single placeholder triangle, not geometry.` | The field had no zero crossing anywhere in the sampled box. The file still exists and still parses — it holds one meaningless triangle. | Check `--bounds`, and check the sign convention (a field that is positive everywhere has no surface). |
| `the field reported unusable bounds, so the sampled region fell back to the unit cube [-0.5,0.5]^3.` | `bounds()` was neither valid nor infinite, so the sampler used `[-0.5,0.5]³`. Anything outside that cube was never looked at. | Pass `--bounds x0,y0,z0,x1,y1,z1`. |
| `the sampled region reaches outside the baked grid, where the field reads clamped edge values instead of real samples.` | The graph is a baked grid and the root box is bigger than the bake. Outside the bake every probe reads the nearest face value, so the far field is flat, not real. | Shrink `--bounds` to the baked region, or re-bake over a larger one. |
| `the output mesh is not closed (N boundary edge(s), M non-manifold edge(s))` | The contoured mesh has holes or non-manifold junctions. Two causes: a wall or gap thinner than the cells can resolve, or a surface **deliberately clipped by `--bounds`**, which leaves a genuine open rim. | If unintended, raise `--depth` or thicken the feature — the [resolution rule](../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist). If you clipped on purpose the warning is correct and expected: the mesh really is open, and a slicer may reject it as-is. |

The invariants these warnings guard are
[design/10 § Output invariants](../design/10-invariants-and-tolerances.md#output-invariants);
the library-side contract and the record are
[roadmap 17 #26](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md).
A host embedding the engine reads the same facts programmatically rather than
from stderr — `dualc::Diagnostics` in `include/dualc/types.h` for C++, and the
`dualc_field_contour_with_diagnostics` / `dualc_field_export_with_diagnostics`
twins of the C ABI ([`capi/README.md`](../../capi/README.md)) for everyone else.

## The output file appears only complete (`.part`)

Every tool that exports through the shared writer (the six above, monolithic or
`--tile-depth`) writes to **`<path>.part`** and renames it over `<path>` only after
the file is finished, so `<path>` is either the complete output or absent — never
a truncated file:

- while a run is in progress there is a growing `<path>.part` next to the
  destination and no `<path>` yet (a previous `<path>` stays as it was);
- a run that fails (an unbounded field, a writer error, an unknown extension)
  removes its `.part` and exits non-zero with the message; `<path>` is untouched;
- a run **killed from outside** (Ctrl+C, Task Manager) cannot clean up: it leaves
  `<path>.part` — and, on the streaming 3MF paths, `<path>.part.model.tmp` or
  `<path>.part.verts.tmp` / `.tris.tmp` — behind, and `<path>` untouched. Delete the
  strays; nothing in them is usable.

A tool never reads a `.part`, and there is no flag: the temp name is derived from
`-o` and the rename is `std::filesystem::rename`, which replaces an existing
`<path>` on every platform. A host driving the C ABI gets the same guarantee, plus
a cooperative cancel that cleans up ([`capi/README.md`](../../capi/README.md)).
The mechanism is [design/09 § Export-format dispatch](../design/09-conventions.md#export-format-dispatch);
the decision, D-46, is recorded in [roadmap 14/05](../roadmap/14-c-abi/05-progress-and-cancel.md#the-decisions).

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
