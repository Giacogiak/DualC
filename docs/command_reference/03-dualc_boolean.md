# Tool 3: `dualc_boolean` (the 7 boolean operations)

Booleans of two meshes.

```
dualc_boolean <op> a.obj b.obj [-o PATH] [--k K] [--depth N] [--collapse E] [--sharp] [--bake N]
```

`<op>` is one of `union`, `intersection`, `difference`, `xor`, `smooth-union`,
`smooth-intersection`, `smooth-difference`.

| Flag | Default | Meaning |
| --- | --- | --- |
| `<op> a.obj b.obj` | (required) | The operation and its two operand meshes, A and B. |
| `-o PATH` | `boolean.obj` | Output; `.obj` / `.stl` / `.3mf` by extension ([export formats](README.md#export-formats--o-extension-dispatch)). |
| `--k K` | `0.25` | Blend radius of the three `smooth-*` ops, in world units; the hard ops ignore it. |
| `--depth N` | `7` | Octree max depth; cell size ≈ root extent / `2^N`. |
| `--collapse E` | `0` (off) | Adaptive cell-collapse threshold (QEF energy, length²) — [common options](README.md#common-options--which-tool-takes-which). |
| `--sharp` | off | Record each crossing's geometric face normal instead of the smooth per-vertex blend ([Sharp vs. smooth normals](01-dualc_demo.md#sharp-vs-smooth-normals---sharp)). |
| `--bake N` | off (direct) | Pre-sample each operand into an `N³` narrow-band signed-distance grid before combining. |

## The options in detail

- **`--k K`** sets the fillet width of the Quilez smooth-min used by the three
  `smooth-*` ops. Scale it to the model, not to a fixed small number: a fillet
  smaller than one contour cell (≈ model extent ÷ `2^depth`) is invisible — if
  `smooth-union` looks identical to `union`, raise `--k` or `--depth`. When `--k`
  is below 1 % of the model extent the tool warns:
  `[dualc_boolean] warning: --k 0.001 is tiny next to the model (extent ~2); the blend fillet will be far smaller than one contour cell and invisible. Pick a k on the order of the model size.`
- **`--depth N`** — each `+1` roughly quadruples the triangle count and runtime.
  Raise it to resolve thin features and keep seams crisp; lower it for speed.
  Pair it with `--collapse` to keep a high-depth result from exploding in size.
- **`--sharp`** — use it when the operands have crisp CAD seams you want to keep
  square; omit it for organic meshes, which come out faceted with it on.
- **`--bake N`** — off by default, when the meshes are contoured **directly**:
  exact distances and normals, sharp features preserved. Baking is an opt-in
  accelerator for very large meshes and a speed/quality trade: the grid rounds
  off sub-cell sharp edges, so leave it off unless the direct path is too slow.
  The band width is auto-tuned to keep the `--k` blend region exact. An operand
  with an enclosed cavity (a hollow shell) keeps its cavity on the baked path,
  the same as on the direct one — the fix and its test are
  [roadmap 12/07 § H2](../roadmap/12-field-graph-and-app/07-mesh-preview-sweep.md#h2-the-bakes-far-field-sign-flood-assumed-a-padded-region).

## Recipes

| # | Operation | Default command | Result |
| --- | --- | --- | --- |
| B1 | `union` | `dualc_boolean union cube.obj cube.obj -o b_union.obj` | A ∪ B, sharp seam. |
| B2 | `intersection` | `dualc_boolean intersection cube.obj cube.obj -o b_intersection.obj` | A ∩ B, sharp seam. |
| B3 | `difference` | `dualc_boolean difference cube.obj cube.obj -o b_difference.obj` | A − B, sharp seam. |
| B4 | `xor` | `dualc_boolean xor cube.obj cube.obj -o b_xor.obj` | Symmetric difference, sharp seam. |
| B5 | `smooth-union` | `dualc_boolean smooth-union cube.obj cube.obj --k 0.25 -o b_sunion.obj` | A ∪ B with a fillet of radius `--k`. |
| B6 | `smooth-intersection` | `dualc_boolean smooth-intersection cube.obj cube.obj --k 0.25 -o b_sinter.obj` | A ∩ B, rounded. |
| B7 | `smooth-difference` | `dualc_boolean smooth-difference cube.obj cube.obj --k 0.25 -o b_sdiff.obj` | A − B, rounded. |
| B8 | `difference`, baked | `dualc_boolean difference cube.obj sphere.obj --sharp --bake 128 -o carved.obj` | A − B on the `--bake` accelerator path, face normals keeping the seams square. |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
