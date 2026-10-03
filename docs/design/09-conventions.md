# Conventions — project-wide

The conventions that hold across the library, the CLIs and the viewers, each stated once
with its reason. The engine-internal conventions — cube corner, edge and child indexing, the
DC descent tables, the four-cell order around an edge — are
[§ 6](05-conventions-and-tables.md#6-conventions) and are not repeated here. The user-facing
tables that follow from these rules (the export-format table, the keyboard map, the flag
tables) live in the [command reference](../command_reference/README.md) and link back here for
the *why*.

## Units and frames

**The library is unit-less; the tool layer fixes 1 world unit = 1 mm.** Nothing in
`include/dualc/` or `src/` names a unit: a field is a real-valued function of a point, and a
mesh arrives in whatever units it was made in. The commitment is made once, at export: the 3MF
writer in `examples/example_common.cpp` declares `unit="millimeter"` in the model header, so a
slicer reads one world unit as one millimetre. Every CLI inherits that reading — `--simplify E`
is an error in mm, wall-thickness advice is in mm, `--bounds` is in mm — because a lattice
part is made to be printed, and the print pipeline is the only consumer that cares about
absolute size.

**Meshes arrive in one global frame; I/O and spatial transforms are the host's job.** The
library takes geometry-central meshes and returns geometry-central meshes; it neither reads
nor writes a file format, and it does not place, scale or orient a *mesh* (the `transformed`
/ `scaled` / `elongated` *field* decorators are part of the field algebra, which is a
different thing — they warp a distance function, not a vertex buffer). Every reader and writer
lives under `examples/` (`example_common.cpp`, with the deflate and image writers in
`examples/third_party/`), and a host that already has a mesh in memory passes it as it is.
The rule keeps `libdualc` free of format code and of the frame-convention arguments that come
with it; the record of the decision, and the one case where a format-aware feature was
proposed and turned down, is [roadmap 09](../roadmap/09-io-formats.md).

**Sign convention.** A field is negative inside, positive outside; the surface is the zero
level set ([§ 10.1](08-implicit-field-layer.md#101-implicitfield)). Every oracle, boolean
and diagnostic assumes it.

## Export-format dispatch

The output format is chosen **by the `-o` extension, lower-cased, and by nothing else**:
`.obj`, `.stl` or `.3mf`, read by `lowerExt` in `examples/example_common.cpp` and dispatched
after a single contour pass — in `dispatchWrite` on the plain path, in `writeField`'s
decimation branch, and as a guard in each tiled writer; any other extension is an error
naming the three. There is no format flag because the extension is the one thing the user
types anyway, and a format is never inferred from content or from a tool's default. What
each format carries follows from what its consumer needs, not from the engine:

- `.obj` is the inspection format: per-vertex normals, smooth-shaded, human-readable, and the
  only one `dualc_demo` / `dualc_gen_demo` write.
- `.stl` is the print format: binary, no normals worth keeping, accepted by every slicer.
- `.3mf` is the compact print format: deflated, shared vertices, and the `1 unit = 1 mm`
  header above — the interchange a desktop slicer expects.

Streaming export (`--tile-depth`) writes `.stl` or `.3mf` only: a tiled OBJ would be huge
and share no vertices, so it is refused rather than written badly. The user-facing table — extension, format, one-line note — is the command reference's
[Export formats](../command_reference/README.md#export-formats--o-extension-dispatch).

Every writer targets `path + ".part"`, not `path`: the driver (`writeField`, the tiled
driver behind both `writeFieldTiled*`) holds an `AtomicOutput` that renames the temp over
`path` with `std::filesystem::rename` — which replaces an existing file on every platform —
only after the writer's `finish()` succeeded, and removes the temp on any other exit. The
tiled sinks keep their own scratch beside it (`<path>.part.model.tmp`, and for the welded
3MF `<path>.part.verts.tmp` / `.tris.tmp`); each has an `abort()` that closes its streams
and removes them, run by the driver before the `.part` is removed, because Windows will not
unlink a file that is still open. So a user only ever sees a complete `<path>` or a stray
`<path>.part` — never a truncated output — which is the invariant
[10 § Output invariants](10-invariants-and-tolerances.md#output-invariants) states; the
`dispatchWrite` dispatch above receives the temp as its target and the extension check
still reads `path`.

## Windows PowerShell 5.1 quoting

The field-graph shorthand needs a **real `"`** around a mesh path (`mesh(path="cube.obj")`),
and Windows PowerShell 5.1 consumes a bare inner `"` in its native-argument layer before the
`.exe` sees it — the parser then receives an unquoted path and reports
`expected ',' or ')'`. The rule, everywhere a recipe is written for that shell:

- **bash, zsh, PowerShell 7+**: single-quote the whole expression; inner double quotes pass
  through unchanged. This is the form the recipes use.
- **Windows PowerShell 5.1**: keep the outer single quotes and escape every path quote as
  `\"` — the backslash carries a literal quote to the exe. A bare `"` never works, whichever
  outer quote surrounds it; a JSON file avoids the question entirely.

Analytic expressions have no inner quotes and need no escaping in any shell. The worked
forms, including the fiddlier alternates, are on the
[`dualc_field` vocabulary page](../command_reference/11-dualc_field/01-op-vocabulary.md#--expr-and-shell-quoting).

## Keyboard-layout independence

GLFW reports a key by its **physical US-layout position**, so a binding written as
`GLFW_KEY_Z` or `GLFW_KEY_LEFT_BRACKET` lands on a different — often dead or AltGr — key on an
AZERTY or QWERTZ keyboard. The rule for every GL viewer (`dualc_raymarch`,
`dualc_field_view`):

- A **letter** control is matched by the character the key prints (`glfwGetKeyName`), not
  by keycode: the section keys `x`/`y`/`z`/`f` are, and `z` is the case that bites (it swaps
  with `w` on AZERTY). The remaining letters (`r`, `t`, `n`, `l`) are keycode-matched and
  work only because those keys sit in the same place on every Latin layout — a new letter
  control is matched by character.
- A **value** control (nudge, slide, wavelength, offset) sits on the arrow and Page keys,
  which are identical on every layout.
- The US-position keys (`[`/`]`, `-`/`=`) are kept only as alternates for US keyboards; their
  glyphs live on the AltGr layer elsewhere and `glfwGetKeyName` cannot see them, so they are
  never the *only* binding.

The key map itself is the command reference's
[GPU viewer keyboard map](../command_reference/README.md#gpu-viewer-keyboard-map-dualc_raymarch--dualc_field_view).

## Primitive parameters: grouped keys, positional fallback, one registry

Every primitive is registered once, in the shared registry of `examples/example_common`, with
its parameter order and defaults; the CLIs and the viewer read the same table, so a primitive
added there is available everywhere with the same spelling. In a field graph (JSON, `.fld`,
`--expr`) a primitive takes its parameters in one of two forms, resolved by
`primitiveParams` in `examples/field_graph.cpp`:

- **Grouped semantic keys** for the ops pinned in `primitiveLayouts()` — `sphere`, `box`,
  `roundbox`, `capsule`, `cappedcylinder`, `torus`, `ellipsoid` — each key filling a named
  slice of the parameter vector (`center`, `radius`, `min`, `max`, `a`, `b`, `major`, `minor`,
  `radii`).
- **A flat `params:[…]` array** for every primitive, mapping positionally onto the registry's
  order (the order the [`dualc_primitive` page](../command_reference/02-dualc_primitive.md)
  lists); omitted trailing values fall back to the registry defaults, and a flat array
  overrides grouped keys where both are given.

The grouped form is the readable one and is extended by adding a layout row; the flat form is
the universal escape hatch, which is why it is never removed from an op that gains grouped
keys. Unknown keys and out-of-range values are rejected with the node's locator, not absorbed
([17/08](../roadmap/17-code-audit-and-hardening/08-argument-validation.md)).

## Where the other conventions live

- Docs conventions — IDs never renumbered, a README at every level, the footer, the size cap —
  are [`docs/README.md` § Conventions](../README.md#conventions).
- The vendoring rule (permissive licences only; geometry-central pinned and fetched, never vendored) is
  [§ 8](06-parameters-and-vendoring.md#8-vendored-third-party-code) and
  [`THIRD_PARTY.md`](../../THIRD_PARTY.md).
- Numeric tolerances and the invariants the tests pin are [10](10-invariants-and-tolerances.md);
  the vocabulary is [11](11-glossary.md).

---

← Back to the [design index](README.md) · the [docs index](../README.md)
