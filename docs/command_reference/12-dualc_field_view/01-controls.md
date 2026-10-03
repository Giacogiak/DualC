# Controls: keys, live parameter editing, reload vs reset, the two tiers, section planes

Part of [Tool 12: `dualc_field_view`](README.md). Everything you press: the orbit and
section-plane keys, how `tab` + nudge edits a parameter live (and why some edits
recompile), and the viewport-only cuts.

## Controls

| Input | Action |
| --- | --- |
| left-drag | orbit |
| scroll | dolly (zoom) |
| `tab` | select the next editable parameter (a *binding*) |
| `↓` / `↑` (or `[` / `]`) | nudge the selected **scalar** parameter ×0.8 / ×1.25 — updates a uniform with **no recompile** (instant) |
| `x` / `y` / `z` | toggle the **section plane** on that axis on/off (and make it the *active* plane) |
| `←` / `→` | slide the active section plane along its axis (− / +) |
| `f` | flip the active section plane's kept side |
| `0` | clear all section planes |
| `l` | reload the source file and recompile (a **structural** edit) — also happens **automatically** when the file changes on disk |
| `r` | reset the view |
| `Esc` | quit |

### How the live parameter controls work

When the viewer compiles your graph it builds a **list of editable parameters** —
the codegen's *binding table*: every single-number knob in the field (a sphere's
`radius`, a gyroid's `wavelength`, an `onion`'s `thickness`, a smooth boolean's
`k`, a `scale`'s `s`, a `twist`'s `radiansPerUnit`, …). The keyboard controls are
a cursor over that list:

1. **`tab` selects** the next parameter — it prints `selected param: <name>` to
   the console and changes nothing else. Press it repeatedly to cycle through
   every knob in the graph.
2. **`↓` / `↑` (or `[` / `]`) change the *currently selected* parameter**
   (×0.8 / ×1.25 per press). The viewport updates **instantly** and the console
   prints the new value, e.g. `wavelength = 1.25`. The arrow keys are the
   layout-independent path; the brackets are US-keyboard alternates.

So `tab` picks the knob, `↓` / `↑` turn it. Worked example — load the CSG graph
of [recipe P1](02-nodes-and-examples.md#recipes), then:

| Press | Effect |
| --- | --- |
| `tab` (a few times) | cycles `wavelength → thickness → radius → …` (console shows each). |
| stop on `wavelength`, `↑` `↑` | lattice gets **coarser** (bigger cells) live; `↓` makes it **finer**. |
| `tab` to `thickness`, `↓` / `↑` | gyroid walls get thinner / thicker. |
| `tab` to `radius`, `↓` / `↑` | the carved sphere shrinks / grows. |

For a **`graded-onion`** graph (variable wall thickness — see the foot recipe in
[Examples](02-nodes-and-examples.md#examples)) the same controls reach its four scalars,
no new keys:

| Press | Effect |
| --- | --- |
| `tab` to `t1`, `↓` / `↑` | thickness of the **near/thick** plateau (`control ≤ d0`). |
| `tab` to `t2`, `↓` / `↑` | thickness of the **far/thin** plateau (`control ≥ d1`). |
| `tab` to `d0` / `d1`, `↓` / `↑` | move where the falloff **starts** / **ends** — drag the gradient band across the part live. |

**`graded-offset`** (variable **strut/solid radius** — see the graded strut-radius
recipe in [Examples](02-nodes-and-examples.md#examples)) exposes the *identical* `t1`/`t2`/`d0`/`d1`
scalars and controls; the only difference is `t` **inflates the solid** (grows the
strut radius by `t`) instead of setting a hollow wall thickness. Tabbing to `t1`/
`t2` grows/shrinks the near/far strut radius live.

**Two things that commonly trip people up:**

- **Nudge only acts on single-number (scalar) parameters.** If you `tab` onto a
  *vector* knob — `center`, `min`, `max`, `by`, `h` (three numbers) — pressing
  `[` / `]` does **nothing** (there is no obvious "scale a vec3" meaning, so it is
  skipped). If a press seems dead, you are probably on a vector knob; `tab` on to
  a scalar like `radius` / `wavelength` / `thickness` / `k`.
- **Keyboard layout — just use `↓` / `↑`.** The arrows do the same nudge on
  every layout; `[` / `]` are physical-US positions that land on dead/AltGr keys
  on AZERTY/QWERTZ. The rule, and why the letter keys are safe:
  [design/09 § Keyboard-layout independence](../../design/09-conventions.md#keyboard-layout-independence).

### `l` (reload) vs `r` (reset) — different jobs

- **`r` resets the *camera* only.** If you have orbited/zoomed somewhere awkward,
  `r` snaps the view back to the framed default. The field is untouched.
- **`l` reloads the graph from its *file*** (and recompiles). The workflow: load a
  **file**, then edit that `.fld`/`.json` in a text editor (add a node, change an
  op, swap a mesh), save — and it **reloads on its own** (the viewer watches the
  file's mtime, ~7 Hz). `l` is a *manual force*; you rarely need it. With
  **`--expr`** / stdin there is no file to watch, so neither the auto-reload nor `l`
  does anything visible. On reload the field's **auto-bounds are recomputed** (unless
  you pinned `--bounds`), so a size-changing edit keeps the raymarch box and section
  planes framed; the **camera is kept** (press `r` to re-frame).

  > Why the auto-reload exists — a host that rewrites the graph file on every
  > solve gets a live viewer with no IPC channel — is
  > [12/03 § Disk file-watch](../../roadmap/12-field-graph-and-app/03-raymarch-app.md#disk-file-watch-auto-reload).

### Why two tiers

This split is the whole point of the binding-table design:

- `↓` / `↑` (or `[` / `]`) = a **parameter edit** → one GPU uniform update,
  **no recompile** (instant).
- `l` (or the **file-watch auto-reload**) = a **structural edit** → re-runs the
  codegen and recompiles the shader, because the *shape of the graph* changed, not
  just a number.

A host drives the structural tier over the file-watch; pushing a parameter edit
from a host as a uniform-only update would need a command channel, which is not
built — [12/03 § Real-time parameter push](../../roadmap/12-field-graph-and-app/03-raymarch-app.md#real-time-parameter-push-uniform-ipc-channel).

### Section planes (viewport-only inspection)

To look *inside* a dense lattice body, cut it with one or more axis-aligned
**section planes** — exactly the "slide a plane through the part" you'd use in a
CAD viewer. Press `x`, `y`, or `z` to switch on the plane for that global axis;
toggling a plane also makes it the **active** one. Then `←` slides the active
plane toward the low end of its axis and `→` slides it toward the high end (hold
to glide — the arrows auto-repeat); `f` flips which half is kept; `0` clears every
plane. Up to three planes can be on at once (an X+Y+Z corner cut). The console
echoes each change.

The cut is a **true (capped) section**: the slice face is shown as a solid,
lightly **warm-tinted** cross-section so you can read the lattice's internal wall
pattern at the plane. Sliding is **instant** at any lattice density (a per-pixel
shader op); a freshly toggled plane lands **mid-body** and each `←` / `→` press
moves it ~2 % of the extent. The mechanism is
[12/03 § Section planes](../../roadmap/12-field-graph-and-app/03-raymarch-app.md#section-planes-viewport-inspection).

Worked example — load any solid lattice (recipe O1 on
[Isolating a TPMS surface](04-open-surface-preview.md#recipes), or
`--expr "intersection(gyroid(wavelength=0.3),sphere(radius=1))"`), then:

| Press | Effect |
| --- | --- |
| `z` | a section plane appears on the Z axis, cutting the body in half; the cut face is a tan solid cross-section. |
| `→` (hold) | the plane slides toward +Z, sweeping the cross-section through the part live (`←` slides back). |
| `f` | the *other* half is kept instead (the cut flips to face the opposite way). |
| `x` | adds a second plane on X — now an L-shaped corner is removed (two cuts at once). |
| `0` | clears all planes; back to the full solid. |

Two things to know:

- **This is visualization only — it never changes geometry.** The clip lives
  entirely in the raymarch shader; a mesh from `dualc_field … -o out.stl` is
  identical whether or not you were sectioning in the viewer.
- **The keys are layout-independent** — the arrows for slide and nudge,
  `x`/`y`/`z`/`f` matched by the printed character
  ([design/09](../../design/09-conventions.md#keyboard-layout-independence)).

---

← Back to the [Tool 12 overview](README.md) · the [Command Reference index](../README.md).
