# DualC — the field graph's own rules

The field graph (`examples/field_graph.{h,cpp}`) is the examples layer's one description of
a field: `dualc_field` builds it into a `FieldPtr` tree and contours it, `dualc_field_view`
and the C ABI read the same tree, and the GLSL codegen (`examples/field_glsl.{h,cpp}`) emits
the preview from it. Most nodes are a one-to-one wiring of the library factories of
[§ 10.4](08-implicit-field-layer.md); this page holds the rules the graph adds on top of
them. How to write a node is the command reference's
([11/01](../command_reference/11-dualc_field/01-op-vocabulary.md)).

## One reader per rule, two callers

Every rule below is implemented once, in `field_graph.cpp`, and called by both the builder
and the codegen: `checkNodeBake`, `bakeMeshNode` / `bakeWindingNode` and `childBakeMargin`
for baking; `readProfile`, `checkLiftParam` and `throwProfileOutsideLift` for the 2D layer.
Each translation unit reads a node's keys through its own `findParam`, so the unread-key
check (an op's unknown key is a located `GraphError`) holds on both paths, and the same bad
graph fails with the same pointer and message on either.

## Per-node bake

`mesh(…, bake=N[, band=W])` and `winding(…, bake=N)` replace the source by a `GridField` of
N³ samples; N is an integer in [2, `kMaxNodeBakeRes`] = 512 (N³ floats, 512 MiB at the
ceiling), and `band` exists only with `bake`, on `mesh` only.

- **The region** is the node's own `bounds()` padded by 10 % of their largest extent plus
  max(`band`, margin) (`nodeBakeRegion`) — not the sibling's, not the root box.
- **The band** (a mesh's exact narrow band, `MeshSource::bakeToGrid`) defaults to 3 bake
  cells and is never below margin + 3 cells; an explicit larger `band` wins. A `winding` node
  bakes the full lattice (`bakeToGrid`). Both run on `FieldGraph::build`'s `bakeThreads`
  workers (`--threads`; 0 = all), the grid identical at any count.
- **The margin** is what the ancestors read below the surface. Outside its region a grid
  clamps to its face value, which never grows, so a parent that reads the field further out —
  a smooth blend, an offset — would turn the clamp into material. `childBakeMargin(parent,
  i, margin)` hands each child, from 0 at the root: through a smooth boolean, max(margin,
  |`k`|); through `offset` / `round`, + max(`r`, 0); `onion`, + |`thickness`|; `displace`,
  + |`amplitude`|; the base of `graded-onion` / `graded-offset`, + max(|`t1`|, |`t2`|);
  `scale`, ÷ |`s`|; every other op passes it unchanged. The builder threads it as a
  parameter of `buildField`, the codegen on its `Ctx::path` stack.

A baked node's preview is its export: the codegen uploads the builder's grid bit for bit
and `--grid-res` does not apply to it. The codegen's dedup memo keys a subtree on its
margin only when the subtree holds a baked node, so a duplicated unbaked mesh still bakes
once.

## The 2D layer

Four profiles (`circle`, `rect`, `segment`, `polygon`) are 2D fields
([§ 10.4](08-implicit-field-layer.md)); a profile is legal **only** as the one child of a
lift — `revolve(profile, offset=O)` spins it around y with profile x = distance from the
axis − O, `extrude(profile, height=H)` sweeps it along z by ± H. A profile anywhere else is
a located `GraphError` on the profile; a 3D or unknown child under a lift, one on the child.
The 2D box's token is `rect` because `box` is the 3D primitive's: a misplaced profile is
always a typing error, never a silent reinterpretation. `readProfile` resolves defaults,
named keys, then `params` / positionals from the front (a longer flat list is refused) and
returns the values in `profileKinds()`'s flat order. A nested array (`points=[[x,y],…]`) is
read as its flattening by every reader.

## What the preview cannot show as exported

The preview is the export's field ([10 § Preview equals export](10-invariants-and-tolerances.md#output-invariants))
except where the GPU needs a bounded form:

- **An unbaked `mesh` / `winding` node** is baked by the codegen over the trace box at
  `--grid-res` (`kDefaultMeshGridRes`, at most `kMaxMeshGridRes`): its preview is a sampled
  approximation of the exact distance the export evaluates. A node with its own `bake` is
  exact on both sides, as above.
- **A `polygon`** compiles to a constant vertex array and a loop of that fixed length, so
  the preview takes at most `kMaxGlslPolygonVertices` = 64 vertices and refuses more with a
  located error; the export has no ceiling.
- **Uniform types.** A binding's `glslType` is `float`, `vec2`, `vec3` or `mat4`; a
  profile's 2D points (`center`, `half`, `a`, `b`) are the `vec2`s, so a host that uploads
  the bindings itself must handle all four.

---

← Back to the [design index](README.md) · the [docs index](../README.md)
