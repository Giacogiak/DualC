# Interactive viewer — `dualc_view` (Polyscope)

The interactive Polyscope viewer and its evolution into a four-mode workbench.
Per-command catalogue (modes, controls, diagnostics, recipes):
[../command_reference/08-dualc_view.md](../command_reference/08-dualc_view.md).

## 10. Polyscope integration in the demo. [DONE — 2026-05-31]

New `dualc_view` CLI opens an interactive Polyscope window with live ImGui controls
(TPMS-family combo, wavelength / offset / depth / collapse-error sliders,
normalize-thickness checkbox, sign-method combo when an input mesh is loaded).
Background-thread re-contour fires on slider release
(`ImGui::IsItemDeactivatedAfterEdit`) with coalescing semantics — slider changes during
a long contour replace the pending state, the latest snapshot dispatches once the
in-flight job finishes, so the UI stays responsive for the full lifetime of a depth-8
contour. Toggleable diagnostic overlays expose the sampler / contourer internals: octree
leaf centers (scalar = depth), octree wireframe (curve network), Hermite edge crossings
+ their interpolated normals (paired toggles), sign-oracle corner classification
(boolean scalar), and the implicit-field value as a per-vertex heatmap on the output
mesh. The two heavy overlays (wireframe + corner cloud, both `O(leaf count)` with a 12x
/ 8x multiplier) are gated by a 50k-leaf safety check with an `Allow heavy diagnostics`
opt-in — sized to pass depth-5 TPMS (32,768 leaves) and gate depth-6+ (262,144 leaves),
since TPMS primitives always return `cellOverlaps = true` and the sampler refines every
cell to `maxDepth`. **Opt-in build:** `cmake -DDUALC_BUILD_POLYSCOPE_VIEWER=ON` plus a
sibling polyscope checkout at `<repo-parent>/polyscope` (override via
`-DDUALC_POLYSCOPE_DIR`); default library build untouched (no GLFW / ImGui / GLAD
dependency in `libdualc`). **Refactor that landed alongside:** `makeTpmsField` +
`tpmsKinds()` lifted from `dualc_lattice.cpp`'s anonymous namespace to
`dce::makeTpmsField` in `examples/example_common.{h,cpp}` since `dualc_view` needs the
same dispatch table. **Architecture-clean choices:** the `HermiteOctree` public API was
NOT extended with a visitor — the viewer writes a 10-line inline recursive leaf walk in
its own anonymous namespace (one consumer, YAGNI; lift to a header when #19 C-ABI needs
it). The contour pipeline is split as `sampleFieldToHermiteOctree →
walkOctreeLeaves(diag) → contourHermiteOctree → fieldHeatmap` inside the worker so both
the diagnostic data and the output mesh are produced atomically and moved to a
main-thread-stable `RegisteredDiag` buffer on consume, avoiding races with the next
worker overwrite. Full documentation in
[`../command_reference/08-dualc_view.md`](../command_reference/08-dualc_view.md) (CLI
flag table, ImGui control catalog, structure-toggle list with heavy-gate semantics,
coalescing notes, GLFW warning footnote, output-mode recipes); a one-paragraph
"Interactive viewer" subsection in `README.md` covers the sibling-clone prerequisite.
Tools count in Part I bumped from 6 → 7.

## 21. `dualc_view` multi-mode interactive workbench. [DONE — 2026-06-09]

Extended the Polyscope viewer (#10) from a TPMS-lattice-only tool into **four
live-tuning modes**, selected by a `Mode` combo at the top of the panel, all sharing the
existing async re-contour pipeline, diagnostic overlays, heavy-leaf gate, and
depth/collapse/bounds controls:
- **Lattice** — unchanged (TPMS + optional onion/normalize, optionally clipped to one input mesh).
- **Primitive** — any of the `dualc_primitive` catalogue entries (count: [command_reference/README](../command_reference/README.md)) rendered by a **generic N-slider widget** (labels derived from the primitive's signature), plus a **fixed-order post-op stack** (offset / onion / twist+axis / scale / elongate / mirror / repeat / displace) of checkbox-gated stages. The CLI's free left-to-right post-op order is simplified to this fixed stack; `--onion`/`--twist`/… flags **preload** it.
- **Boolean** — an SDF boolean of **two operands, each a loaded mesh OR an inline primitive** (a per-operand `Mesh A` / `Mesh B` / `Primitive` source combo). The 7 ops (hard + smooth) are a combo; the smooth blend radius `k` is a live drag that re-contours **without** rebuilding the mesh BVH (only `Sharp` / sign-method change does).
- **CSG** — a `dualc_csg_demo` recipe combo; each recipe's previously-baked constants are now **tunable sliders** (defaults reproduce the CLI output exactly).
**The headline win is fast parameter fine-tuning** — finding a fillet `k` or a primitive
dimension is a slider drag, not a CLI re-run + reopen. Scope, decided by Giacomo:
boolean operands can be mesh-or-primitive; csg recipes are
parametric; meshes load **at startup** (no in-UI file picker).
**Single-source-of-truth refactor (the structural core).** The per-tool registries that
used to live inside the CLI `.cpp` files were lifted into
`examples/example_common.{h,cpp}` under `dce::`, now used by BOTH the CLIs and the
viewer: `primitiveCatalogue()`/`findPrimitive()`/`buildPrimitive()` (+
`PrimEntry.infinite`, `paramNames()`); `parsePostOp()`/`applyPostOps()`/`isPostOpFlag()`
(+ `PostOp`/`PostOpKind`);
`parseBoolOp()`/`applyBoolOp()`/`boolOpName()`/`boolOpIsSmooth()` (+ `BoolOp`); and
`csgRecipes()`/`findRecipe()`/`buildRecipe()` (recipes made parametric — the
recipe-construction `if/else` was lifted out of `dualc_csg_demo`'s `main()` into
per-recipe builders). The three CLIs (`dualc_primitive`, `dualc_boolean`,
`dualc_csg_demo`) were rewired to call these and their local copies deleted. **Parity is
gated** by the existing exit-code smoke tests (`cli_primitive`, `cli_boolean`,
`cli_csg_demo`) — empty params == defaults == original output; full suite green (156
ctest).
**Key architecture choices.** (a) The field tree is assembled on the **main thread**
(cheap — it composes already-built operands), and only the expensive sample→contour runs
on the worker (`AsyncContourer` now carries a `FieldPtr jobField` + resolved
sampler/contourer params instead of mesh pointers). This is what makes two-operand
booleans, bounds-from-`field->bounds()`, and the BVH cache all fall out cleanly. (b) A
`MeshSourceCache` keyed on `(mesh ptr, interpolateNormals, signMethod)` builds each BVH
once, so tuning `k` is instant. (c) **Bounds policy:** Lattice = Fixed (TPMS infinite);
the other modes default to **Auto-fit** (pad `field->bounds()` by 5%), forced to Fixed
when `field->bounds().isInfinite()` (plane / infinite cylinder/cone / a `repeat`
post-op). One non-obvious bug fixed (caught in review, invisible to the green
compile/CLI/CTest checks because they all launch directly into a mode): the `Mode` combo
handler must reset **both** `forcedFixed=false` and `boundsPolicy` per mode, or a stale
`forcedFixed` (e.g. from the infinite lattice gyroid) clobbers the policy back to Fixed
later in the same frame and clips the new mode's geometry to the old bounds. **No new
dependencies and no CMake target changes** (polyscope already vendors imgui/glfw/glad;
`example_common` already linked everywhere). Not in CTest (needs a display) — verified
via the CLI parity tests + the pre-`init` CLI validation paths; interactive testing is
manual. Docs:
[`../command_reference/08-dualc_view.md`](../command_reference/08-dualc_view.md)
rewritten (modes table, per-mode flags + live controls, recipes V1–V18).

---

← Back to the [Roadmap index](README.md).
