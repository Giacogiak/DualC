#pragma once

#include "dualc/dualc.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Shared glue for the DualC example executables: argument parsing helpers, a
// small analytic-bump library for the `displaced` operator, and the
// field-to-OBJ writer extracted from dualc_demo.

namespace geometrycentral {
namespace surface {
class SurfaceMesh;
class VertexPositionGeometry;
}  // namespace surface
}  // namespace geometrycentral

namespace dce {

// Parse a comma-separated list of doubles ("1,2,3"). False on malformed input.
bool parseDoubles(const std::string& s, std::vector<double>& out);

// Parse "x,y,z" into a Vector3 / "x,y" into a Vector2. False on bad input.
bool parseVec3(const std::string& s, dualc::Vector3& out);
bool parseVec2(const std::string& s, dualc::Vector2& out);

// Parse "x0,y0,z0,x1,y1,z1" into a BBox. False on bad input.
bool parseBounds(const std::string& s, dualc::BBox& out);

// True if `token` is a numeric literal or comma-separated number list (so a
// leading '-' on a negative number is not mistaken for a flag).
bool isNumberToken(const std::string& token);

// Build a bump function for the `displaced` operator from a spec string
// "name,amplitude,frequency"; name is one of sine | gyroid | bumps. An
// unknown name yields a zero bump.
std::function<double(const dualc::Vector3&)> namedBump(const std::string& spec);

// Post-contour QEM mesh decimation request (Approach A -- monolithic: contour
// the whole mesh, decimate the whole mesh, then write). Backed by meshoptimizer
// (host-side, example-only). Mode::None leaves the mesh untouched.
//   Ratio : keep `value` (0,1) of the triangles (e.g. 0.1 = 10x lighter).
//   Error : simplify to an absolute geometric error of `value` world units (mm).
// Mutually exclusive; the CLI rejects combining it with tiled/streaming export
// (per-tile locked-border decimation is the deferred Approach B).
struct DecimateOpts {
  enum class Mode { None, Ratio, Error } mode = Mode::None;
  double value = 0.0;
};

// The text of the last `[dualc] error:` line the writer path printed on THIS
// thread, without the prefix -- empty after a successful writeField* call, or
// one that failed before reaching the writers. A caller with no console (the
// C ABI, a host plugin) reads it back here; every writeField* clears it on
// entry. Per thread, because several exports may run at once in one process.
const std::string& lastError();

// Sample `field`, contour it once, and write `path` in the format chosen by
// its extension: `.obj` (per-vertex normals), `.stl` (binary), or `.3mf`
// (3MF-mesh, 1 world unit = 1 mm). When `dec.mode != None`, a global QEM
// decimation pass runs on the contoured mesh before export (the decimated
// `.obj` carries recomputed angle-weighted vertex normals). Returns 0 on
// success; 1 on the infinite-bounds sampler error (prints a --bounds hint);
// 2 on an unknown extension or a writer failure; 3 when `cancel` was
// requested (dualc/progress.h). The file is written as `path + ".part"` and
// renamed over `path` only on success, so on every non-zero rc nothing is
// left at `path` and a file that was already there is untouched.
//
// `progress`, when non-null, sees Stage::Sample and Stage::Contour from the
// engine, then Stage::Write as (0, 1) before the writer and (1, 1) after the
// rename -- always on the calling thread.
//
// Degradations (an empty contour, a bounds fallback, a non-closed output) are
// printed as `[dualc] warning:` lines either way; pass `diag` to receive the
// same facts programmatically -- that is how the C ABI's
// dualc_field_export_with_diagnostics reports them to a host with no console.
// Its output counts describe the CONTOURED mesh; when `dec` requests a
// decimation pass the written file has fewer triangles than they report.
int writeField(const dualc::ImplicitField& field,
               const std::string& path,
               const dualc::SamplerParams& sp,
               const dualc::ContourerParams& cp,
               const DecimateOpts& dec = {},
               dualc::Diagnostics* diag = nullptr,
               const dualc::CancelToken* cancel = nullptr,
               dualc::ProgressSink* progress = nullptr);

// Streaming / tiled binary-STL export -- breaks the RAM wall for dense parts.
// Contours `field` in uniform, grid-aligned cubic tiles of `tileDepth` octree
// levels each (so peak RAM is one tile, not the whole part) and streams the
// owned triangles to `path` incrementally. The overall resolution is
// sp.maxDepth (the global cell size); `tileDepth` (<= sp.maxDepth) sets how much
// of that grid each in-memory tile holds, and thus the RAM ceiling. The tile
// count per axis is derived. Tiles overlap by one ghost cell so every boundary
// quad is emitted exactly once (ownership by the global cell grid); when
// tileDepth >= sp.maxDepth this collapses to a single streamed pass.
//
// NOTE: STL stores independent triangles, so seam vertices are duplicated
// (bit-identical in the dyadic case). The output is geometrically watertight for
// slicers, NOT topologically vertex-welded -- true welding needs a global vertex
// hash, i.e. unbounded RAM, which would defeat streaming.
//
// Returns 0 on success; 1 on the infinite-bounds sampler error (prints a
// --bounds hint); 2 on a writer or validation failure; 3 when `cancel` was
// requested -- polled at the top of every tile and inside every tile's
// contour, so a request lands within one tile's contour time. Same `.part`
// guarantee as writeField: on every non-zero rc nothing is left at `path`.
// `progress` sees Stage::Tile only: (0, T) before the loop, (i, T) as tile
// i starts, (T, T) after the rename -- except on the single-pass fallback
// (tileDepth >= depth), whose one contour forwards the sink instead.
int writeFieldTiledStl(const dualc::ImplicitField& field,
                       const std::string& path,
                       const dualc::SamplerParams& sp,
                       const dualc::ContourerParams& cp,
                       int tileDepth,
                       const dualc::CancelToken* cancel = nullptr,
                       dualc::ProgressSink* progress = nullptr);

// Streaming / tiled 3MF export -- the compact, slicer-native sibling of
// writeFieldTiledStl. Same tiling (one-cell ghost ring + centroid ownership,
// one-tile peak RAM), but each non-empty tile is written as one deflated 3MF
// <object> whose vertices are shared *within* the tile; the model part is
// streamed to a temp file and packed into the .3mf with incremental deflate, so
// RAM stays one tile. Result: ~3x smaller on disk than the STL soup and carries
// the slicer-native `1 unit = 1 mm` metadata. Across-tile seam vertices stay
// duplicated (as in the STL path). `path` must end in .3mf. Same return codes as
// writeFieldTiledStl.
//
// `weld = true` instead produces a SINGLE globally-manifold <object>: coincident
// vertices that adjacent tiles emit at their shared seam are welded (position
// hash over the seam-plane subset only, so RAM stays bounded). Use it when the
// consumer needs topology/connectivity (re-booleans, FEA, decimation) or a single
// object; the default (weld = false) is cheaper and slicer-equivalent for print.
int writeFieldTiled3mf(const dualc::ImplicitField& field,
                       const std::string& path,
                       const dualc::SamplerParams& sp,
                       const dualc::ContourerParams& cp,
                       int tileDepth,
                       bool weld = false,
                       const dualc::CancelToken* cancel = nullptr,
                       dualc::ProgressSink* progress = nullptr);

// Parse a human RAM budget for `--mem` into bytes: a positive magnitude with an
// optional binary suffix -- `G`/`GiB`, `M`/`MiB`, `K`/`KiB`, `B`, or none
// (bytes); a fractional magnitude ("1.5G") is accepted. Case-insensitive.
// Returns false on any malformed / non-positive input.
bool parseMemBudget(const std::string& s, std::uint64_t& bytesOut);

// `--mem` auto-budget: choose the LARGEST tile-depth D in [2, sp.maxDepth-1]
// whose estimated peak per-tile RAM fits `budgetBytes`, then hand D to the same
// writeFieldTiled{Stl,3mf} path an explicit --tile-depth would use. A single
// cheap coarse whole-field contour supplies the spatial face distribution; the
// busiest tile's face count (not the average) is extrapolated to sp.maxDepth with
// a conservative fixed growth model + a byte-per-face constant with margin, so
// the pick errs toward a SMALLER D (more tiles, slower, but never an OOM). The
// probe, chosen D, and its estimate are logged. Returns the chosen D (>= 2), or
// -1 if the field is unbounded and no --bounds was given (a hint is printed).
int chooseTileDepthForBudget(const dualc::ImplicitField& field,
                             const dualc::SamplerParams& sp,
                             const dualc::ContourerParams& cp,
                             std::uint64_t budgetBytes);

// Named TPMS family + a one-line description, shared by every example that
// exposes a TPMS picker (dualc_lattice, dualc_view, ...).
struct TpmsKind {
  std::string name;
  std::string blurb;
};
const std::vector<TpmsKind>& tpmsKinds();

// Build a TPMS implicit field by name (one of `tpmsKinds()`). Returns nullptr
// for unknown names so callers can validate the user's --type flag.
dualc::FieldPtr makeTpmsField(const std::string& kind,
                              const dualc::Vector3& center,
                              double wavelength);

// --- Strut-based (wireframe crystal) lattices ------------------------------
//
// Where a TPMS is a smooth periodic *surface*, a strut lattice is a periodic
// *wireframe*: a unit cell of capsule struts along the nodes/edges of a crystal
// (simple-cubic, body-centered, face-centered, octet truss), tiled through
// space with `repeated`. The tiled field is an exact union (min) of Lipschitz-1
// capsule SDFs, so it dual-contours cleanly and gets the sparse-octree speedup;
// clip it to a volume with `intersectionOf(strut, MeshSource)` (mesh-fill) or a
// `box` / explicit `--bounds` (box-fill).

// Named strut crystal + a one-line description (shared by every picker, mirrors
// tpmsKinds()).
struct StrutKind {
  std::string name;
  std::string blurb;
};
const std::vector<StrutKind>& strutKinds();

// The unit-cell strut segments (endpoints, in a cell centered at the origin
// with side `wavelength`) for one crystal `kind`. This is the single source of
// truth for the crystal geometry: makeStrutLattice() unions a CapsuleField over
// each segment and tiles it; the strut-lattice test tiles the SAME segments
// independently to certify the tiling. Empty for an unknown kind.
std::vector<std::pair<dualc::Vector3, dualc::Vector3>> strutCellSegments(
    const std::string& kind, double wavelength);

// Build a tiled strut-lattice field by crystal name (one of `strutKinds()`):
// the union of `radius`-thick capsules over strutCellSegments(kind, wavelength),
// tiled infinitely and translated so a cell is centered at `center`. Returns
// nullptr for an unknown name so callers can validate the user's --type flag.
// bounds() is infinite (like TPMS / `repeated`): the sampler needs an explicit
// region, supplied by the mesh/box clip or --bounds.
//
// `nodeRadius` tapers the struts: each is fat (`nodeRadius`) at both end-nodes
// and pinches to `radius` at mid-span, built as two RoundCone SDFs split at the
// segment midpoint. The default sentinel (< 0) means `nodeRadius = radius`, i.e.
// no taper -- the plain-capsule path, byte-identical to an untapered lattice.
dualc::FieldPtr makeStrutLattice(const std::string& kind,
                                 const dualc::Vector3& center,
                                 double wavelength, double radius,
                                 double nodeRadius = -1.0);

// ===========================================================================
// Shared builder registries (primitives, post-ops, booleans, CSG recipes).
//
// Hosted here so both the CLI tools (dualc_primitive / dualc_boolean /
// dualc_csg_demo) and the interactive viewer (dualc_view) drive ONE source of
// truth, instead of each owning a private copy that can drift.
// ===========================================================================

// One analytic primitive: a name, a human-readable parameter signature, the
// per-parameter defaults (used for any value the caller omits), a builder that
// turns a full parameter vector into a field, and an `infinite` flag -- true
// for fields of unbounded extent (plane / infinitecylinder / infinitecone),
// which need an explicit sampling region.
struct PrimEntry {
  std::string name;
  std::string signature;
  std::vector<double> defaults;
  std::function<dualc::FieldPtr(const std::vector<double>&)> build;
  bool infinite = false;
};
const std::vector<PrimEntry>& primitiveCatalogue();
const PrimEntry* findPrimitive(const std::string& name);

// Merge `params` over the named primitive's defaults and build it. Params
// beyond the default count are ignored. Returns nullptr for an unknown name.
dualc::FieldPtr buildPrimitive(const std::string& name,
                               const std::vector<double>& params);

// Split a primitive/recipe signature into per-parameter labels, stopping at a
// trailing "[...]" note. Used to label generic parameter widgets in the viewer.
std::vector<std::string> paramNames(const std::string& signature);

// --- Post-operators (decorators + domain warps) ----------------------------
enum class PostOpKind {
  Offset, Round, Onion, Scale, Elongate, Translate, Rotate,
  Twist, Bend, Mirror, Repeat, RepeatLimited, Displace
};
struct PostOp {
  PostOpKind kind;
  std::vector<double> params;   // numeric arguments (see parsePostOp)
  std::string bumpSpec;         // Displace only: "name,amp,freq"
};
// True if `flag` (e.g. "--twist") names a post-op.
bool isPostOpFlag(const std::string& flag);
// Parse one "--flag value" pair into a PostOp. False (with `err` set) on a
// malformed value or an unknown flag.
bool parsePostOp(const std::string& flag, const std::string& val,
                 PostOp& out, std::string& err);
// Apply a post-op stack to `f`, left to right. An op with the wrong parameter
// count is skipped (treated as a no-op).
dualc::FieldPtr applyPostOps(dualc::FieldPtr f, const std::vector<PostOp>& ops);

// --- Boolean operators -----------------------------------------------------
enum class BoolOp {
  Union, Intersection, Difference, Xor,
  SmoothUnion, SmoothIntersection, SmoothDifference
};
const char* boolOpName(BoolOp op);   // CLI spelling, e.g. "smooth-union"
bool        parseBoolOp(const std::string& s, BoolOp& out);
bool        boolOpIsSmooth(BoolOp op);
dualc::FieldPtr applyBoolOp(BoolOp op, dualc::FieldPtr a, dualc::FieldPtr b,
                            double k);

// --- CSG recipes (parametric: each baked constant is a tunable parameter) ---
struct RecipeEntry {
  std::string name;
  std::string blurb;
  bool needsMesh = false;
  std::string signature;          // parameter labels (may be empty)
  std::vector<double> defaults;   // defaults reproduce the original recipe
};
const std::vector<RecipeEntry>& csgRecipes();
const RecipeEntry* findRecipe(const std::string& name);

// Build a recipe; `params` is merged over its defaults (empty => all defaults,
// reproducing the original baked recipe). `mesh`/`geom` may be null for recipes
// whose needsMesh is false. Returns nullptr for an unknown name or a
// missing-but-required mesh.
dualc::FieldPtr buildRecipe(
    const std::string& name, const std::vector<double>& params,
    geometrycentral::surface::SurfaceMesh* mesh,
    geometrycentral::surface::VertexPositionGeometry* geom);

} // namespace dce
