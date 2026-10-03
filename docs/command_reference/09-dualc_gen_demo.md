# Tool 9: `dualc_gen_demo` (procedural demo meshes)

Generate the procedural demo meshes (sphere, torus, knot, …).

```
dualc_gen_demo <shape> [-o out.obj] [--depth N]
dualc_gen_demo all [--dir DIR] [--depth N]
```

Writes the procedural demo meshes used for contouring QA into `data/`. Mesh
files are gitignored (`*.obj`), so this generator — not the `.obj` — is the
version-controlled source of truth (every demo mesh is first-party code, not a
downloaded asset). The build runs `dualc_gen_demo all` itself into `build/data/`
and copies the result next to every tool (target `dualc_demo_data`), so a clean
clone needs no manual step; run `dualc_gen_demo all --dir <repo>/data` to
populate the repo's `data/` for recipes launched from the root. Each shape targets a distinct topology / curvature /
feature regime; pair them with `dualc_demo` (CLI) or, as a `mesh(path=…)` source, with
`dualc_field_view` (interactive) for side-by-side visual QA. **Not generated** (hand-authored or downloaded, see
`THIRD_PARTY.md` § Demo mesh assets): `molde`, `foot`, `opA`/`opB`, `mesh-soup`
(project-authored) and `bunny` (Stanford); recipes that name them need those files
in place.

| Shape | Output | Euler χ / genus | What it exercises |
| --- | --- | --- | --- |
| `cube` | `cube.obj` | 2 / 0 | The unit cube most recipes on every page take as input (12 triangles), so `all` yields every recipe input. |
| `sphere` | `sphere.obj` | 2 / 0 | Smooth curvature, uniform icosphere tessellation (baseline). |
| `uvsphere` | `uvsphere.obj` | 2 / 0 | Pole-sliver tessellation — contrast against the icosphere. |
| `torus` | `torus.obj` | 0 / 1 | Non-zero genus — the χ = 0 validator. |
| `knot` | `knot.obj` | 0 / 1 | Trefoil-knot tube: varying curvature + near-approaching strands. |
| `genus2` | `genus2.obj` | −2 / 2 | Higher genus (two tori merged in one neck, self-contoured). |
| `cylinder` | `cylinder.obj` | 2 / 0 | Mixed: sharp circular rims + smooth wall + flat caps. |
| `bracket` | `bracket.obj` | 2 / 0 | CAD part with **both** convex (90°) and concave (270°) sharp edges. |
| `hexbore` | `hexbore.obj` | 0 / 1 | Many sharp facets + a curved cylindrical bore. |

| Option | Default | Meaning |
| --- | --- | --- |
| `-o PATH` | `<shape>.obj` | Output path for a single shape. |
| `--dir DIR` | (cwd) | Output directory prefix for `all`. |
| `--depth N` | `7` | Octree depth for `genus2` only (it is contoured by DualC; the parametric shapes ignore it). |

## Recipes

| # | Command | What it does |
| --- | --- | --- |
| G1 | `dualc_gen_demo all --dir ../../../data` | Regenerate every generated demo mesh in `data/` from the build output folder (`--dir data` from the repo root); the `.obj` files are gitignored, this is their source. |
| G2 | `dualc_demo torus.obj torus_dc.obj --depth 6` | Re-mesh the torus; output stays genus 1 (χ = 0). |
| G3 | `dualc_demo --sharp bracket.obj bracket_sharp.obj --depth 6` | Sharp-feature toggle: the reentrant edge stays crisp (drop `--sharp` to see it round). |
| G4 | `dualc_field_view --expr "mesh(path=\"knot.obj\")"` | Inspect the trefoil knot interactively (its baked narrow-band SDF, sphere-traced). |
| G5 | `dualc_demo --gwn-field bunny.obj bunny_sealed.obj --depth 7` | Seal the open Stanford bunny scan (hole in the base) into a watertight solid. |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
