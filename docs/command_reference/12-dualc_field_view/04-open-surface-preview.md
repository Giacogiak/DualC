# Isolating a TPMS surface inside a volume (meshless preview)

Part of [Tool 12: `dualc_field_view`](README.md). The thin-shell inspection workflow;
rationale in [roadmap 12 § F](../../roadmap/12-field-graph-and-app/05-open-surface.md).

A frequent task is to **inspect the lattice surface alone** — the gyroid membrane
filling a part — *without* the enclosing volume's skin welded onto it. With the
contourer that skin is unavoidable: dual contouring can only emit **closed**
surfaces, so a zero-thickness TPMS clipped to a volume comes out as a solid block
with gyroid tunnels (the volume's faces seamlessly joined to the lattice). The
raymarcher has no such constraint — it renders an isosurface directly — so the
isolated surface needs **no special mode**: render a **thin shell** of the TPMS,
clipped to the volume.

The recipe is `intersection(onion(normalize(tpms)), volume)`:

- `onion(f, t)` = `|f| − t` turns the zero-thickness membrane into a **thin
  closed shell** (solid only in the band `−t < f < +t`; *empty* in the bulk of
  both labyrinth channels). Because the shell is empty between its two sheets,
  the intersection shows the volume boundary **only** where shell material
  actually reaches it (tiny strut-end cross-sections) — there is no big welded
  skin. The wall is `≈ 2·thickness` (each sheet sits ±t from the membrane).
- `normalize` rescales the non-metric TPMS value so `thickness` is ~millimetres.
- A TPMS has **infinite** `bounds()`, so the **volume operand supplies the finite
  raymarch domain** (its bounding box) as well as the clip — no `--bounds` needed.

## Recipes

| # | What it shows |
| --- | --- |
| O1 | The isolated gyroid membrane in a box volume (analytic, no bake) |
| O2 | The same in a mesh volume, from a `.fld` file (no shell quoting) |
| O3 | O2 inline, in PowerShell 5.1 form |
| O4 | The contrast: drop the `onion` and the welded skin returns |
| O5 | A bare shell with no volume — `--bounds` supplies the domain |

**O1 — Box volume** (analytic, exact, no bake):

```bash
dualc_field_view --expr "intersection(onion(normalize(gyroid(wavelength=0.3)),thickness=0.03),box(min=[-0.5,-0.5,-0.5],max=[0.5,0.5,0.5]))"
```

**O2 — Mesh volume** (baked to the `sampler3D` clip). The cleanest path on any
shell is a `.fld` file, so the quoted `mesh` path never meets the shell:

```powershell
'intersection(onion(normalize(gyroid(wavelength=12)),thickness=1.2),mesh(path="data/foot.obj"))' | Set-Content -Encoding ascii foot_lattice.fld
dualc_field_view --grid-res 128 foot_lattice.fld
```

**O3** — inline equivalent in PowerShell 5.1 form (path quotes `\"` — the
[quoting rule](../../design/09-conventions.md#windows-powershell-51-quoting)):

```powershell
dualc_field_view --grid-res 128 --expr 'intersection(onion(normalize(gyroid(wavelength=12)),thickness=1.2),mesh(path=\"data/foot.obj\"))'
```

**O4 — See the contrast.** Drop the `onion` and you get the welded skin back — the
contourer-style artifact, now in the raymarch preview:

```bash
dualc_field_view --expr "intersection(gyroid(wavelength=0.3),box(min=[-0.5,-0.5,-0.5],max=[0.5,0.5,0.5]))"
```

**O5 — Bare shell, no volume** — drop the intersection and you must supply the domain
explicitly (the TPMS is infinite), and you'll see a rectangular block of shell
rather than a part shape:

```bash
dualc_field_view --expr "onion(normalize(gyroid(wavelength=12)),thickness=1.2)" --bounds -30,-30,0,30,30,60
```

> This thin shell is a **closed** solid (a tiny real wall), not a true
> zero-thickness open sheet — visually faithful for inspection, and it is exactly
> what you thicken and export next. To **export** the same isolated lattice as a
> watertight mesh (and to add a skin / boolean it), feed the identical expression
> to [`dualc_field`](../11-dualc_field/04-workflow-open-surface.md#workflow-isolate--thicken--skin--union-a-tpms-lattice).
> A genuinely *open* zero-thickness preview (a render-time clip mask) is not built —
> [roadmap 12 § F](../../roadmap/12-field-graph-and-app/05-open-surface.md#deferred-optional--render-time-clip-mask-isolated-open-surface-preview)
> holds the decision and its trigger.

---

← Back to the [Tool 12 overview](README.md) · the [Command Reference index](../README.md).
