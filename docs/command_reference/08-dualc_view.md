# Tool 8: `dualc_view` (interactive Polyscope viewer)

Interactive Polyscope viewer for fields + diagnostic overlays (opt-in build).

```
dualc_view [<inputs...>] [--mode M] [lattice|primitive|boolean|csg flags]
           [--depth N] [--collapse E] [--bounds x0,y0,z0,x1,y1,z1]
```

Opens an interactive 3D window (Polyscope) that dual-contours a DualC field
and re-contours live as you tune it. It has **four modes**, all sharing the
same async pipeline, depth/collapse/bounds controls, and diagnostic overlays:

| Mode | What it builds | Picks it |
| --- | --- | --- |
| `lattice` | a TPMS lattice, optionally clipped to one input mesh (the original `dualc_view`). | default, or `--mode lattice` |
| `primitive` | one analytic primitive (`dualc_primitive`'s catalogue) + a fixed-order post-op stack. | `--prim NAME` |
| `boolean` | an SDF boolean (`dualc_boolean`) of **two operands**, each a loaded mesh **or** a primitive, with a live blend radius. | `--op OP` |
| `csg` | a named recipe (`dualc_csg_demo`), its baked constants exposed as **live sliders**. | `--recipe NAME` |

The CLI flags **preload** the initial state; the ImGui panel then exposes a
**Mode** combo plus every parameter as live sliders/combos and re-contours on
slider release. Re-contouring runs on a background thread so the window stays
responsive during long contours. Mesh operands are loaded **at startup** (no
in-UI file picker) and their narrow-band SDF (BVH) is built once and cached, so
tuning the blend radius `k` is instant — only changing **sharp** or the **sign
method** rebuilds it.

**Opt-in build** (`-DDUALC_BUILD_POLYSCOPE_VIEWER=ON` and a sibling Polyscope
checkout — the [build prelude](README.md#build-prelude)); `libdualc` is unaffected.

## CLI flags (preload only)

| Flag | Mode | Default | Meaning |
| --- | --- | --- | --- |
| `<inputs...>` | all | (none) | Input mesh(es). `boolean` takes up to two (operands A, B); the other modes take at most one (lattice clip / csg input). |
| `--mode M` | — | inferred | `lattice` \| `primitive` \| `boolean` \| `csg`. Inferred from `--prim`/`--op`/`--recipe` if omitted; else `lattice`. |
| `--type NAME` | lattice | `gyroid` | TPMS family: `gyroid`, `schwarz-p`, `diamond`, `fischer-koch`, `lidinoid`, `neovius`. |
| `--wavelength W` | lattice | mesh-diag / 10 if input, else `0.25` | Unit-cell side (world units). |
| `--offset T` | lattice | `0` | Thick-wall offset via `onionOf`. |
| `--normalize-thickness` | lattice | off | Wrap the field in `normalizedOf` so `T` is metric thickness. |
| `--prim NAME` | primitive | `sphere` | Primitive name (see `dualc_primitive --list`). Params are tuned **in the panel**, not on the CLI. Post-op flags (`--onion`, `--twist`, …) preload the stack. |
| `--op OP` | boolean | `union` | `union` \| `intersection` \| `difference` \| `xor` \| `smooth-*`. |
| `--k K` | boolean | `0.25` | Smooth-op blend radius (world units). |
| `--sharp` | boolean | off | Use face normals (no smooth-normal interpolation). |
| `--recipe NAME` | csg | `cube-minus-sphere` | Recipe name (see `dualc_csg_demo --list`). Constants become sliders. |
| `--depth N` | all | `7` | Octree max depth. |
| `--collapse E` | all | `0` | Adaptive cell-collapse threshold (QEF energy, length²). |
| `--bounds x0,y0,z0,x1,y1,z1` | all | see Bounds note | Explicit sampling region (forces **Fixed** bounds). |

## Live ImGui controls (after window opens)

A **Mode** combo at the top switches pipeline; below it the panel shows that
mode's controls, then the shared depth/collapse/bounds/diagnostics.

| Control | Mode | Notes |
| --- | --- | --- |
| `Type` / `Wavelength` / `Offset` / `Normalize thickness` | lattice | TPMS family (6), cell size 0.01–5.0, offset 0–1.0; normalize disabled when offset = 0. |
| `Primitive` + param drags | primitive | Catalogue combo; one drag-float per parameter, labelled from the primitive's signature. |
| `Post-ops` (Offset/Onion/Twist/Scale/Elongate/Mirror/Repeat/Displace) | primitive | Each a checkbox + params, applied **in this fixed order** (the CLI's free left-to-right order is simplified to this stack). `--round` preloads Offset; `--translate`, `--rotate`, `--bend` and `--repeat-limited` are not in the stack and are dropped at preload with `[dualc_view] post-op not supported in the viewer preload; ignoring`. |
| `Operation` / `Blend k` / `Sharp` / `Sign method` | boolean | 7 ops; `k` editable only for smooth ops, and **does not** rebuild the BVH (instant). `Sharp`/sign rebuild it (brief stall). |
| `Operand A` / `Operand B` | boolean | Each a source combo (`Mesh A` / `Mesh B` / `Primitive`); choosing `Primitive` reveals an inline primitive editor. A red note appears if a chosen mesh was not loaded. |
| `Recipe` + param drags | csg | Recipe combo (with blurb); constants exposed as drag-floats. Red note if the recipe needs a mesh and none was loaded. |
| `Bounds` | non-lattice | `Auto-fit` (fits the field AABB +5%) or `Fixed` (editable min/max). Forced to **Fixed** for infinite fields (plane, infinite cylinder/cone, `repeat`). Lattice is always Fixed. |
| `Depth` / `Collapse error` / `Recontour` | all | Depth 3–9; collapse 0–0.1; manual re-trigger. |

**Bounds default.** `lattice` defaults to the input-AABB +5% (or `[-0.5, 0.5]^3`
bare) and stays **Fixed**. `primitive`/`boolean`/`csg` default to **Auto-fit**
(the sampling box tracks the geometry as you tune it); pass `--bounds` to start
Fixed.

**Re-contour trigger.** Sliders fire on release, never per-frame; combo and
checkbox changes fire immediately. A change made while a contour is still running
is buffered and dispatched when it finishes, so the viewer always converges to
your most recent inputs (the coalescing design:
[roadmap 07 § 10](../roadmap/07-viewer-polyscope.md#10-polyscope-integration-in-the-demo-done--2026-05-31)).
The status line shows the last-contour stats (`N verts  M faces  K leaves  T.T ms`)
or `Contouring...` while a worker is in flight.

## Diagnostic overlay toggles

Each toggle registers (or removes) a Polyscope structure visualising a
slice of the sampler / contourer internals at the current contour state.
Toggling on/off does NOT trigger a re-contour; the data is cached from the
most recent contour.

| Toggle | Polyscope structure | Heavy gate? |
| --- | --- | --- |
| `Octree leaves` | Point cloud of leaf centers, scalar quantity `depth`. | no |
| `Octree wireframe` | Curve network of all leaf cell edges (12 segments per leaf). | yes |
| `Hermite crossings` | Point cloud at each Hermite-edge crossing. | no |
| `Hermite normals` | Vector quantity on the crossings cloud. Enabled only when `Hermite crossings` is on. | no |
| `Sign-oracle corners` | Point cloud at each surface-touching leaf's 8 corners, scalar `inside` (1.0 / 0.0). | yes |
| `Field-value heatmap` | Scalar quantity `field value` added to the output mesh. | no |

**Safety gate.** Above 50,000 leaves (a depth-5 TPMS passes, depth 6+ trips it)
the wireframe + sign-oracle toggles are disabled and a `Heavy: N leaves > 50000
threshold` warning appears in the panel with an `Allow heavy diagnostics` opt-in
checkbox. Override consciously; rendering 600k+ wireframe segments or 400k+
corner points can stutter on integrated GPUs (the sizing:
[roadmap 07 § 10](../roadmap/07-viewer-polyscope.md#10-polyscope-integration-in-the-demo-done--2026-05-31)).

## Operational notes

- **Initial contour blocks** before the window opens so the user sees
  content, not a blank scene. All subsequent contours are fully async.
- **Closing the window during a contour** is safe: `main()` joins the
  worker before exiting.
- **GLFW startup warnings on older Intel iGPU drivers** (e.g. `WGL: Failed
  to make context current: The handle is invalid.`) are harmless — the window
  opens correctly past them.

## Recipes

Run these from the build output folder (`build/examples/Release/`) so the
gen-demo palette (`cube.obj`, `sphere.obj`, `torus.obj`, …) resolves by bare
name. `bunny.obj` / `foot.obj` are **not** copied next to the exe — pass a path
(e.g. `data/bunny.obj` from the repo root, or an absolute path).

**Lattice mode** (the original viewer; unchanged):

| # | Command | What it opens |
| --- | --- | --- |
| V1 | `dualc_view` | Bare gyroid in the unit cube — quick smoke test, no input mesh. |
| V2 | `dualc_view cube.obj --type gyroid --wavelength 0.5 --depth 5` | Solid-phase gyroid clipped to the cube; below the heavy-diagnostics gate. |
| V3 | `dualc_view cube.obj --type gyroid --wavelength 0.5 --offset 0.05 --depth 6` | Thick-wall double-sheet shell; depth 6 trips the heavy gate (262k leaves). |
| V4 | `dualc_view molde.obj --type schwarz-p --wavelength 8 --offset 1 --normalize-thickness --depth 7` | Schwarz-P infill of `molde.obj` with metric 1 mm walls; tune live. |

**Primitive mode** (`--prim`; params + post-ops in the panel):

| # | Command | What it opens |
| --- | --- | --- |
| V5 | `dualc_view --prim torus` | Drag `major`/`minor`; tick **Onion** + **Twist** (set axis); Auto-fit bounds track the size. |
| V6 | `dualc_view --prim sphere --onion 0.1 --twist 1.5,1` | Post-op **preload** from the CLI — Onion + Twist arrive pre-ticked. |
| V7 | `dualc_view --prim boxframe` | A different catalogue entry; its own signature labels the sliders. |
| V8 | `dualc_view --prim plane --bounds -2,-2,-2,2,2,2` | Infinite field: bounds are forced **Fixed** (the panel says so). |

**Boolean mode** (`--op`; two operands, each a mesh or a primitive):

| # | Command | What it opens |
| --- | --- | --- |
| V9 | `dualc_view --op smooth-union data/bunny.obj data/bunny.obj --k 0.3` | Smooth-union of two meshes; drag **Blend k** live (no BVH rebuild). |
| V10 | `dualc_view --op difference cube.obj` | One mesh loaded as A; set **Operand B → Primitive** sphere in the panel to carve it. |
| V11 | `dualc_view --op smooth-intersection` | No meshes ⇒ both operands default to **Primitive**; set A=`box`, B=`sphere`. |
| V12 | `dualc_view --op union cube.obj cube.obj --sharp` | Hard union; **Blend k** is greyed; `--sharp` uses face normals (BVH rebuilds on toggle). |

**CSG mode** (`--recipe`; baked constants become sliders):

| # | Command | What it opens |
| --- | --- | --- |
| V13 | `dualc_view --recipe smooth-blend` | Drag `separation` / `radius` / `blendK`; defaults match `dualc_csg_demo --recipe=smooth-blend`. |
| V14 | `dualc_view --recipe cube-minus-sphere` | Self-contained recipe; tune `boxHalf` + the sphere params. |
| V15 | `dualc_view --recipe mesh-shell cube.obj` | Mesh recipe: hollow shell of the input; drag `thickness`. |
| V16 | `dualc_view --recipe twisted-mesh data/bunny.obj` | Mesh recipe: domain-warp the bunny; drag `radPerUnit` + `axis`. |

**Cross-cutting checks** (any mode):

| # | Action | What it verifies |
| --- | --- | --- |
| V17 | Launch `dualc_view` (Lattice), then switch the **Mode** combo to Primitive → Boolean → Csg | Each non-lattice mode **Auto-fits** (does not clip the geometry to the old bounds). |
| V18 | In any window, raise **Depth** past ~50k leaves | The heavy-diagnostics gate appears; wireframe + sign-oracle corners require the **Allow heavy diagnostics** opt-in. |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
