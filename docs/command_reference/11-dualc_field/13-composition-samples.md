# Six composition samples (`examples/samples/`)

Part of [Tool 11: `dualc_field`](README.md). Six small graphs that show composition: analytic
booleans, a smooth blend, a displaced surface, and a mesh source hollowed, carved and warped.
They were [`dualc_csg_demo`](../05-dualc_csg_demo.md)'s six hand-built recipes; each is now a
JSON file in `examples/samples/`, copied next to `dualc_field` (and `dualc_field_view`) by the
build, with every parameter at the old recipe's default — and each writes the same bytes as
`dualc_csg_demo --recipe=NAME` ([roadmap 21/07](../../roadmap/21-cli-consolidation/07-docs-migration.md)).
The old tool's `displaced` needed a C++ lambda; the graph's `displace(fn=sine,…)` is the same
function ([12 § displace functions](12-primitive-catalogue.md#the-three-displace-functions)).

## The samples

| # | File | Graph | Was |
| --- | --- | --- | --- |
| K1 | `cube_minus_sphere.json` | `difference(box(min=[-1,-1,-1],max=[1,1,1]),sphere(center=[1,1,1],radius=1.2))` | R1 `cube-minus-sphere` |
| K2 | `smooth_blend.json` | `smooth-union(sphere(center=[-0.6,0,0],radius=0.8),sphere(center=[0.6,0,0],radius=0.8),k=0.5)` | R2 `smooth-blend` |
| K3 | `displaced_sphere.json` | `displace(sphere(center=[0,0,0],radius=1),fn=sine,amplitude=0.08,frequency=8)` | R3 `displaced-sphere` |
| K4 | `mesh_shell.json` | `onion(mesh(path="cube.obj"),thickness=0.05)` | R4 `mesh-shell` |
| K5 | `mesh_minus_sphere.json` | `difference(mesh(path="cube.obj"),sphere(center=[0.5,0.5,0.5],radius=0.5))` | R5 `mesh-minus-sphere` |
| K6 | `twisted_mesh.json` | `twist(mesh(path="cube.obj"),radiansPerUnit=2,axis=y)` | R6 `twisted-mesh` |

The old recipes' parameter names, for reading the graphs: K1 is `boxHalf` 1 and a sphere at
(1, 1, 1) of radius 1.2; K2 two radius-0.8 spheres centred at x = ±0.6, blended with `k` 0.5; K3
radius 1, amplitude 0.08, frequency 8; K4 a 0.05 wall; K5 a radius-0.5 sphere at
(0.5, 0.5, 0.5); K6 2 radians per unit about y.

**A mesh of your own.** The three mesh samples read `cube.obj` by bare name (from the current
directory); `dualc_csg_demo` took the mesh as its positional argument. Copy the file and
change its `"path"` — or write the graph inline, as the K5 variant below does. The
old `--list` is the table above; `--depth` / `--collapse` / `-o` are `dualc_field`'s options.

## Recipes

Run from the build's tool folder, where the samples and the demo meshes sit:

```bash
dualc_field cube_minus_sphere.json -o cube-minus-sphere.obj    # K1
dualc_field smooth_blend.json -o smooth-blend.obj              # K2
dualc_field displaced_sphere.json -o displaced-sphere.obj      # K3
dualc_field mesh_shell.json -o mesh-shell.obj                  # K4
dualc_field mesh_minus_sphere.json -o mesh-minus-sphere.obj    # K5
dualc_field twisted_mesh.json -o twisted-mesh.obj              # K6

# K5 on another mesh, inline:
dualc_field --expr 'difference(mesh(path="torus.obj"),sphere(center=[0.5,0.5,0.5],radius=0.5))' -o torus_carved.obj
# any of them, previewed:
dualc_field_view smooth_blend.json
```

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
