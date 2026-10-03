// dualc_view -- interactive Polyscope viewer for DualC fields and meshes.
//
// Four interactive modes, all funnelling through the same async contour +
// diagnostics pipeline:
//   - Lattice   : a TPMS lattice (optionally clipped to an input mesh).
//   - Primitive : an analytic primitive + a fixed-order post-op stack.
//   - Boolean   : an SDF boolean of two operands, each a loaded mesh OR a
//                 primitive, with a live blend radius for the smooth ops.
//   - Csg       : a named CSG recipe whose baked constants are live sliders.
//
// The field tree is assembled on the MAIN thread (cheap -- it composes
// already-built operands; mesh operands come from a MeshSourceCache so their
// BVH is built once). The worker thread only does the expensive part: sample
// -> walk-for-diagnostics -> contour -> field heatmap. The diagnostic overlays
// (octree leaves/wireframe, Hermite crossings, sign-oracle corners, field
// heatmap) and the heavy-leaf safety gate are unchanged.
//
// Async + coalescing semantics: the UI stays responsive during long contours;
// later changes during a long contour replace the pending state, and the
// latest snapshot dispatches once the in-flight job finishes.

#include "example_common.h"

#include "dualc/contourer.h"
#include "dualc/dualc.h"
#include "dualc/hermite_octree.h"
#include "dualc/sampler.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include "polyscope/curve_network.h"
#include "polyscope/point_cloud.h"
#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using dualc::FieldPtr;
using dualc::Vector3;

namespace {

// --- Small helpers (lifted-from-dualc_lattice geometry utilities) ------

dualc::BBox meshAABB(geometrycentral::surface::SurfaceMesh& mesh,
                     geometrycentral::surface::VertexPositionGeometry& geom) {
  dualc::BBox b;
  bool first = true;
  for (auto v : mesh.vertices()) {
    const Vector3 p = geom.inputVertexPositions[v];
    if (first) {
      b.min = b.max = p;
      first = false;
    } else {
      b.min = Vector3{std::min(b.min.x, p.x), std::min(b.min.y, p.y),
                      std::min(b.min.z, p.z)};
      b.max = Vector3{std::max(b.max.x, p.x), std::max(b.max.y, p.y),
                      std::max(b.max.z, p.z)};
    }
  }
  return b;
}

dualc::BBox padBBox(const dualc::BBox& b, double frac) {
  const Vector3 e = b.extent() * frac;
  dualc::BBox r;
  r.min = b.min - e;
  r.max = b.max + e;
  return r;
}

std::vector<double> dbl(const std::vector<float>& f) {
  return std::vector<double>(f.begin(), f.end());
}

// --- Sign-method picker (only meaningful when an input mesh is loaded) -

const char* const kSignMethodLabels[] = {
    "PSEUDONORMAL",
    "WINDING_NUMBER",
    "GENERALIZED_WINDING_NUMBER",
};
constexpr int kNumSignMethods = 3;

dualc::SignMethod signMethodFromIndex(int idx) {
  switch (idx) {
    case 0: return dualc::SignMethod::PSEUDONORMAL;
    case 1: return dualc::SignMethod::WINDING_NUMBER;
    case 2: return dualc::SignMethod::GENERALIZED_WINDING_NUMBER;
    default: return dualc::SignMethod::WINDING_NUMBER;
  }
}

// Standard DC corner indexing: corner c has coords (c.x = c&1, c.y = (c>>1)&1,
// c.z = (c>>2)&1). HermiteLeafData::cornerInside follows this convention; we
// reuse it for both the sign-oracle corner cloud and the wireframe corners.
Vector3 cornerPosition(const dualc::BBox& b, int c) {
  return Vector3{
      (c & 1) ? b.max.x : b.min.x,
      (c & 2) ? b.max.y : b.min.y,
      (c & 4) ? b.max.z : b.min.z};
}

// The 12 cube edges as pairs of corner indices.
constexpr std::array<std::array<int, 2>, 12> kCubeEdgeCorners = {{
    {0, 1}, {2, 3}, {4, 5}, {6, 7},
    {0, 2}, {1, 3}, {4, 6}, {5, 7},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
}};

// Heavy-structure safety gate (see the long-form rationale in prior revisions):
// above this leaf count the wireframe + sign-oracle corner cloud require the
// "Allow heavy diagnostics" opt-in.
constexpr std::size_t kHeavyDiagLeafThreshold = 50000;

// --- Loaded inputs + MeshSource cache ----------------------------------

// A mesh + its geometry, owned for the whole session in main().
struct MeshSlot {
  std::unique_ptr<geometrycentral::surface::SurfaceMesh>            mesh;
  std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> geom;
  bool loaded() const { return static_cast<bool>(mesh); }
};

// Up to two operands (A, B). A also serves as the Lattice clip / Csg input.
struct LoadedInputs {
  MeshSlot a, b;
};

// Caches MeshSources so the (expensive) BVH is built once per distinct
// (mesh, interpolateNormals, signMethod) key. Tuning a blend radius reuses the
// cached source; only changing `sharp` / sign method builds a new one.
struct MeshSourceCache {
  struct Entry {
    geometrycentral::surface::SurfaceMesh* mesh;
    bool interp;
    int  sign;
    std::shared_ptr<dualc::MeshSource> src;
  };
  std::vector<Entry> entries;

  std::shared_ptr<dualc::MeshSource> get(
      geometrycentral::surface::SurfaceMesh* mesh,
      geometrycentral::surface::VertexPositionGeometry* geom, bool interp,
      dualc::SignMethod sign) {
    const int si = static_cast<int>(sign);
    for (Entry& e : entries)
      if (e.mesh == mesh && e.interp == interp && e.sign == si) return e.src;
    auto src = std::make_shared<dualc::MeshSource>(*mesh, *geom, interp, sign);
    entries.push_back({mesh, interp, si, src});
    return src;
  }
};

// --- Viewer state ------------------------------------------------------

enum class Mode { Lattice, Primitive, Boolean, Csg };
enum class BoundsPolicy { AutoFit, Fixed };
enum class OperandKind { MeshA, MeshB, Primitive };

struct LatticeState {
  int    tpmsIdx = 0;
  float  wavelength = 0.25f;
  float  offset = 0.0f;
  bool   normalizeThickness = false;
  int    signMethodIdx = 1;  // WINDING_NUMBER
};

// A generic builder selection (index into a catalogue) + its current numeric
// parameter values. Floats so ImGui can edit them directly.
struct ParamEdit {
  int idx = 0;
  std::vector<float> params;
};

// One post-op stage in the primitive stack.
struct PostOpUI {
  bool  enabled = false;
  std::vector<float> params;
  int   axis = 0;                  // twist only
  char  bump[64] = "sine,0.08,8";  // displace only
};

struct PrimitiveState {
  ParamEdit prim;
  // Fixed-order post-op stack (applied in this order).
  PostOpUI offset, onion, twist, scale, elongate, mirror, repeat, displace;
};

struct Operand {
  OperandKind kind = OperandKind::MeshA;
  ParamEdit   prim;  // used when kind == Primitive
};

struct BooleanState {
  int     opIdx = 0;        // index into BoolOp enum order
  float   k = 0.25f;        // blend radius for the smooth ops
  bool    sharp = false;
  int     signMethodIdx = 1;
  Operand a, b;
};

struct CsgState {
  ParamEdit recipe;
};

struct ViewerState {
  Mode mode = Mode::Lattice;
  LatticeState   lat;
  PrimitiveState prim;
  BooleanState   boo;
  CsgState       csg;

  // Shared sampler / contourer params.
  int    depth = 7;
  float  collapseError = 0.0f;

  // Bounds: edited as float arrays so ImGui DragFloat3 binds persistently.
  BoundsPolicy boundsPolicy = BoundsPolicy::Fixed;
  bool   forcedFixed = false;  // set when the assembled field is infinite
  float  boundsMin[3] = {-0.5f, -0.5f, -0.5f};
  float  boundsMax[3] = { 0.5f,  0.5f,  0.5f};

  // TPMS phase center for Lattice mode.
  Vector3 fieldCenter{0.0, 0.0, 0.0};

  // Loaded-input availability (fixed at startup).
  bool hasMeshA = false;
  bool hasMeshB = false;

  // Diagnostic-structure toggles.
  bool   showOctreeLeaves   = false;
  bool   showOctreeWire     = false;
  bool   showHermite        = false;
  bool   showHermiteNormals = false;
  bool   showSignOracle     = false;
  bool   showFieldHeatmap   = false;
  bool   allowHeavyDiag     = false;

  // Stats from the most recent contour (for the ImGui status line).
  std::size_t lastVerts   = 0;
  std::size_t lastFaces   = 0;
  std::size_t lastLeaves  = 0;
  double      lastWallMs  = 0.0;
};

dualc::BBox boundsBBox(const ViewerState& vs) {
  return dualc::BBox{
      Vector3{vs.boundsMin[0], vs.boundsMin[1], vs.boundsMin[2]},
      Vector3{vs.boundsMax[0], vs.boundsMax[1], vs.boundsMax[2]}};
}
void setBoundsArr(ViewerState& vs, const dualc::BBox& b) {
  vs.boundsMin[0] = static_cast<float>(b.min.x);
  vs.boundsMin[1] = static_cast<float>(b.min.y);
  vs.boundsMin[2] = static_cast<float>(b.min.z);
  vs.boundsMax[0] = static_cast<float>(b.max.x);
  vs.boundsMax[1] = static_cast<float>(b.max.y);
  vs.boundsMax[2] = static_cast<float>(b.max.z);
}

void seedParams(ParamEdit& pe, const std::vector<double>& defs) {
  pe.params.assign(defs.begin(), defs.end());
}

void initPostOps(PrimitiveState& ps) {
  ps.offset.params   = {0.1f};
  ps.onion.params    = {0.1f};
  ps.twist.params    = {1.0f};
  ps.scale.params    = {1.0f};
  ps.elongate.params = {0.2f, 0.0f, 0.0f};
  ps.mirror.params   = {1.0f, 0.0f, 0.0f};
  ps.repeat.params   = {1.0f, 1.0f, 1.0f};
}

// --- Field-tree composition (MAIN thread) ------------------------------

FieldPtr buildPostOpStack(FieldPtr f, const PrimitiveState& ps) {
  std::vector<dce::PostOp> ops;
  if (ps.offset.enabled)
    ops.push_back({dce::PostOpKind::Offset, dbl(ps.offset.params), {}});
  if (ps.onion.enabled)
    ops.push_back({dce::PostOpKind::Onion, dbl(ps.onion.params), {}});
  if (ps.twist.enabled && !ps.twist.params.empty())
    ops.push_back({dce::PostOpKind::Twist,
                   {static_cast<double>(ps.twist.params[0]),
                    static_cast<double>(ps.twist.axis)},
                   {}});
  if (ps.scale.enabled)
    ops.push_back({dce::PostOpKind::Scale, dbl(ps.scale.params), {}});
  if (ps.elongate.enabled)
    ops.push_back({dce::PostOpKind::Elongate, dbl(ps.elongate.params), {}});
  if (ps.mirror.enabled)
    ops.push_back({dce::PostOpKind::Mirror, dbl(ps.mirror.params), {}});
  if (ps.repeat.enabled)
    ops.push_back({dce::PostOpKind::Repeat, dbl(ps.repeat.params), {}});
  if (ps.displace.enabled)
    ops.push_back({dce::PostOpKind::Displace, {}, std::string(ps.displace.bump)});
  return dce::applyPostOps(f, ops);
}

FieldPtr primitiveFromEdit(const ParamEdit& pe) {
  const auto& cat = dce::primitiveCatalogue();
  if (pe.idx < 0 || pe.idx >= static_cast<int>(cat.size())) return nullptr;
  return dce::buildPrimitive(cat[pe.idx].name, dbl(pe.params));
}

FieldPtr operandField(const Operand& op, const BooleanState& bs,
                      MeshSourceCache& cache, const LoadedInputs& in) {
  const dualc::SignMethod sign = signMethodFromIndex(bs.signMethodIdx);
  switch (op.kind) {
    case OperandKind::MeshA:
      if (in.a.loaded())
        return cache.get(in.a.mesh.get(), in.a.geom.get(), !bs.sharp, sign);
      return nullptr;
    case OperandKind::MeshB:
      if (in.b.loaded())
        return cache.get(in.b.mesh.get(), in.b.geom.get(), !bs.sharp, sign);
      return nullptr;
    case OperandKind::Primitive:
      return primitiveFromEdit(op.prim);
  }
  return nullptr;
}

// Assemble the field tree for the current state. Returns nullptr when a
// required operand/mesh is missing (the caller surfaces a warning).
FieldPtr buildField(const ViewerState& vs, MeshSourceCache& cache,
                    const LoadedInputs& in) {
  switch (vs.mode) {
    case Mode::Lattice: {
      const std::string type = dce::tpmsKinds()[vs.lat.tpmsIdx].name;
      FieldPtr lattice = dce::makeTpmsField(
          type, vs.fieldCenter, static_cast<double>(vs.lat.wavelength));
      if (vs.lat.offset > 0.0f) {
        if (vs.lat.normalizeThickness) lattice = dualc::normalizedOf(lattice);
        lattice = dualc::onionOf(lattice, static_cast<double>(vs.lat.offset));
      } else if (vs.lat.normalizeThickness) {
        lattice = dualc::normalizedOf(lattice);
      }
      if (in.a.loaded()) {
        auto src = cache.get(in.a.mesh.get(), in.a.geom.get(),
                             /*interpolateNormals=*/true,
                             signMethodFromIndex(vs.lat.signMethodIdx));
        return dualc::intersectionOf(lattice, src);
      }
      return lattice;
    }
    case Mode::Primitive: {
      FieldPtr f = primitiveFromEdit(vs.prim.prim);
      if (!f) return nullptr;
      return buildPostOpStack(f, vs.prim);
    }
    case Mode::Boolean: {
      FieldPtr fa = operandField(vs.boo.a, vs.boo, cache, in);
      FieldPtr fb = operandField(vs.boo.b, vs.boo, cache, in);
      if (!fa || !fb) return nullptr;
      return dce::applyBoolOp(static_cast<dce::BoolOp>(vs.boo.opIdx), fa, fb,
                              static_cast<double>(vs.boo.k));
    }
    case Mode::Csg: {
      const auto& recs = dce::csgRecipes();
      if (vs.csg.recipe.idx < 0 ||
          vs.csg.recipe.idx >= static_cast<int>(recs.size()))
        return nullptr;
      return dce::buildRecipe(recs[vs.csg.recipe.idx].name,
                              dbl(vs.csg.recipe.params), in.a.mesh.get(),
                              in.a.geom.get());
    }
  }
  return nullptr;
}

// --- Diagnostic data extraction ----------------------------------------

struct DiagExtract {
  std::size_t leafCount = 0;
  std::vector<Vector3> leafCenters;
  std::vector<double>  leafDepths;
  std::vector<Vector3> wireSegments;
  std::vector<Vector3> hermitePos;
  std::vector<Vector3> hermiteNrm;
  std::vector<Vector3> cornerPos;
  std::vector<double>  cornerSign;
  std::vector<double>  fieldHeatmap;
};

void walkOctreeLeaves(const dualc::HermiteNode* node, DiagExtract& d) {
  if (node == nullptr) return;
  if (!node->isLeaf) {
    for (const auto& child : node->children) walkOctreeLeaves(child.get(), d);
    return;
  }

  d.leafCenters.push_back(node->bounds.center());
  d.leafDepths.push_back(static_cast<double>(node->depth));

  for (const auto& e : kCubeEdgeCorners) {
    d.wireSegments.push_back(cornerPosition(node->bounds, e[0]));
    d.wireSegments.push_back(cornerPosition(node->bounds, e[1]));
  }

  if (node->leaf) {
    for (int e = 0; e < 12; ++e) {
      const auto& he = node->leaf->edges[e];
      if (he.hasCrossing) {
        d.hermitePos.push_back(he.position);
        d.hermiteNrm.push_back(he.normal);
      }
    }
    for (int c = 0; c < 8; ++c) {
      d.cornerPos.push_back(cornerPosition(node->bounds, c));
      d.cornerSign.push_back(node->leaf->cornerInside[c] ? 1.0 : 0.0);
    }
  }

  d.leafCount += 1;
}

// --- Async contour pipeline -------------------------------------------

struct AsyncContourer {
  std::thread       worker;
  std::atomic<bool> running     {false};
  std::atomic<bool> resultReady {false};

  // Inputs to the in-flight contour, set on the main thread before launch and
  // not modified until the next dispatch (which only runs after consume).
  FieldPtr               jobField;
  dualc::SamplerParams   jobSampler;
  dualc::ContourerParams jobContour;

  // Outputs from the most recent completed contour.
  std::unique_ptr<geometrycentral::surface::SurfaceMesh>            outMesh;
  std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> outGeom;
  std::vector<Vector3>                                              outNormals;
  DiagExtract                                                       outDiag;
  double                                                            outWallMs = 0.0;
  std::string                                                       errorMsg;

  // Pending request, owned and written by the main thread only.
  ViewerState pendingState;
  bool        pendingDirty = false;
};

struct RegisteredDiag {
  DiagExtract data;
};

struct DiagFlags {
  bool octreeLeaves = false;
  bool octreeWire   = false;
  bool hermite      = false;
  bool corners      = false;
  bool fieldHeatmap = false;
};

// Worker body. Runs on the background thread; never touches Polyscope.
void contourOnWorker(AsyncContourer& ac) {
  FieldPtr field = ac.jobField;
  ac.outDiag = DiagExtract{};

  const auto t0 = std::chrono::steady_clock::now();
  try {
    dualc::HermiteOctree octree =
        dualc::sampleFieldToHermiteOctree(*field, ac.jobSampler);
    walkOctreeLeaves(octree.root(), ac.outDiag);

    auto t = dualc::contourHermiteOctree(octree, ac.jobContour);
    ac.outMesh    = std::move(std::get<0>(t));
    ac.outGeom    = std::move(std::get<1>(t));
    ac.outNormals = std::move(std::get<2>(t));

    if (ac.outMesh && ac.outGeom) {
      const std::size_t nv = ac.outMesh->nVertices();
      ac.outDiag.fieldHeatmap.reserve(nv);
      for (auto v : ac.outMesh->vertices()) {
        const Vector3 p = ac.outGeom->inputVertexPositions[v];
        ac.outDiag.fieldHeatmap.push_back(field->valueAt(p));
      }
    }
    ac.errorMsg.clear();
  } catch (const std::exception& e) {
    ac.errorMsg = std::string("dualc_view contour failed: ") + e.what();
  }
  const auto t1 = std::chrono::steady_clock::now();
  ac.outWallMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

  ac.running.store(false, std::memory_order_relaxed);
  ac.resultReady.store(true, std::memory_order_release);
}

void applyDiagStructures(const ViewerState& vs, const RegisteredDiag& d,
                         DiagFlags& flags) {
  const bool gateOpen =
      vs.allowHeavyDiag || d.data.leafCount <= kHeavyDiagLeafThreshold;

  if (vs.showOctreeLeaves && !d.data.leafCenters.empty()) {
    auto* pc = polyscope::registerPointCloud("octree leaves",
                                             d.data.leafCenters);
    pc->addScalarQuantity("depth", d.data.leafDepths);
    flags.octreeLeaves = true;
  } else if (flags.octreeLeaves) {
    polyscope::removeStructure("octree leaves", /*errorIfAbsent=*/false);
    flags.octreeLeaves = false;
  }

  if (vs.showOctreeWire && gateOpen && !d.data.wireSegments.empty()) {
    auto* cn = polyscope::registerCurveNetworkSegments("octree wireframe",
                                                       d.data.wireSegments);
    cn->setRadius(0.0008f, /*isRelative=*/true);
    flags.octreeWire = true;
  } else if (flags.octreeWire) {
    polyscope::removeStructure("octree wireframe", false);
    flags.octreeWire = false;
  }

  if (vs.showHermite && !d.data.hermitePos.empty()) {
    auto* pc = polyscope::registerPointCloud("hermite crossings",
                                             d.data.hermitePos);
    if (vs.showHermiteNormals &&
        d.data.hermiteNrm.size() == d.data.hermitePos.size()) {
      pc->addVectorQuantity("normals", d.data.hermiteNrm);
    }
    flags.hermite = true;
  } else if (flags.hermite) {
    polyscope::removeStructure("hermite crossings", false);
    flags.hermite = false;
  }

  if (vs.showSignOracle && gateOpen && !d.data.cornerPos.empty()) {
    auto* pc = polyscope::registerPointCloud("sign-oracle corners",
                                             d.data.cornerPos);
    pc->addScalarQuantity("inside", d.data.cornerSign);
    flags.corners = true;
  } else if (flags.corners) {
    polyscope::removeStructure("sign-oracle corners", false);
    flags.corners = false;
  }

  if (vs.showFieldHeatmap && !d.data.fieldHeatmap.empty() &&
      polyscope::hasSurfaceMesh("dualc output")) {
    auto* psOut = polyscope::getSurfaceMesh("dualc output");
    auto* q = psOut->addVertexScalarQuantity("field value",
                                             d.data.fieldHeatmap);
    q->setEnabled(true);
    flags.fieldHeatmap = true;
  } else if (flags.fieldHeatmap &&
             polyscope::hasSurfaceMesh("dualc output")) {
    polyscope::getSurfaceMesh("dualc output")->removeQuantity("field value");
    flags.fieldHeatmap = false;
  }
}

// Consume a finished contour, then dispatch a fresh worker if pending.
void pumpAsync(AsyncContourer& ac, ViewerState& uiState, RegisteredDiag& reg,
               DiagFlags& flags, MeshSourceCache& cache,
               const LoadedInputs& in) {
  // --- Consume any completed result.
  if (ac.resultReady.load(std::memory_order_acquire)) {
    if (ac.worker.joinable()) ac.worker.join();

    if (!ac.errorMsg.empty()) {
      polyscope::warning(ac.errorMsg);
    } else if (ac.outMesh && ac.outGeom) {
      auto* psOut = polyscope::registerSurfaceMesh(
          "dualc output", ac.outGeom->inputVertexPositions,
          ac.outMesh->getFaceVertexList());
      psOut->setSmoothShade(true);
      psOut->addVertexVectorQuantity("vertex normals", ac.outNormals);

      uiState.lastVerts  = ac.outMesh->nVertices();
      uiState.lastFaces  = ac.outMesh->nFaces();
      uiState.lastLeaves = ac.outDiag.leafCount;
      uiState.lastWallMs = ac.outWallMs;

      reg.data = std::move(ac.outDiag);
      flags.fieldHeatmap = false;  // re-registering the mesh wiped its qtys
      applyDiagStructures(uiState, reg, flags);
    }

    ac.outMesh.reset();
    ac.outGeom.reset();
    ac.outNormals.clear();
    ac.outDiag = DiagExtract{};
    ac.errorMsg.clear();
    ac.resultReady.store(false, std::memory_order_relaxed);
  }

  // --- Dispatch new work if pending and the worker is idle.
  if (ac.pendingDirty && !ac.running.load(std::memory_order_acquire)) {
    ac.pendingDirty = false;
    FieldPtr field = buildField(ac.pendingState, cache, in);
    if (!field) {
      polyscope::warning(
          "dualc_view: could not build the field (missing mesh operand?)");
      return;
    }

    dualc::SamplerParams sp;
    sp.maxDepth = ac.pendingState.depth;
    dualc::ContourerParams cp;
    cp.simplificationError = static_cast<double>(ac.pendingState.collapseError);

    const dualc::BBox fb = field->bounds();
    const bool inf = fb.isInfinite();
    uiState.forcedFixed = inf;

    dualc::BBox effective;
    if (ac.pendingState.mode == Mode::Lattice) {
      effective = boundsBBox(ac.pendingState);  // TPMS infinite: always fixed
    } else if (ac.pendingState.boundsPolicy == BoundsPolicy::AutoFit && !inf) {
      effective = padBBox(fb, 0.05);
      setBoundsArr(uiState, effective);  // reflect the fit in the UI box
    } else {
      effective = boundsBBox(ac.pendingState);
    }
    sp.rootBounds = effective;

    ac.jobField   = field;
    ac.jobSampler = sp;
    ac.jobContour = cp;
    ac.running.store(true, std::memory_order_release);
    ac.worker = std::thread([&ac]() { contourOnWorker(ac); });
  }
}

// --- ImGui callback ---------------------------------------------------

const char* const kModeLabels[]   = {"Lattice", "Primitive", "Boolean", "Csg"};
const char* const kBoolOpLabels[] = {"union",
                                     "intersection",
                                     "difference",
                                     "xor",
                                     "smooth-union",
                                     "smooth-intersection",
                                     "smooth-difference"};
const char* const kAxisLabels[]   = {"x", "y", "z"};

const std::vector<const char*>& primLabels() {
  static const std::vector<const char*> labels = [] {
    std::vector<const char*> r;
    for (const dce::PrimEntry& e : dce::primitiveCatalogue())
      r.push_back(e.name.c_str());
    return r;
  }();
  return labels;
}

void drawCallback(ViewerState& vs, AsyncContourer& ac, RegisteredDiag& reg,
                  DiagFlags& flags, MeshSourceCache& cache,
                  const LoadedInputs& in) {
  ImGui::PushItemWidth(180.0f * polyscope::options::uiScale);
  bool needsRecompute = false;
  bool needsApplyDiag = false;

  // Generic parameter editor (used by primitives, recipes, operand prims).
  auto paramSliders = [&](const char* tag, const std::string& sig,
                          std::vector<float>& ps) {
    const std::vector<std::string> names = dce::paramNames(sig);
    for (std::size_t i = 0; i < ps.size(); ++i) {
      const std::string lbl =
          (i < names.size() ? names[i] : ("p" + std::to_string(i))) + "##" +
          tag + std::to_string(i);
      ImGui::DragFloat(lbl.c_str(), &ps[i], 0.01f, 0.0f, 0.0f, "%.4f");
      if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;
    }
  };

  // --- Mode selector --------------------------------------------------
  int modeIdx = static_cast<int>(vs.mode);
  if (ImGui::Combo("Mode", &modeIdx, kModeLabels, 4)) {
    vs.mode = static_cast<Mode>(modeIdx);
    // Reset the bounds policy for the new mode. Both fields must be reset: a
    // stale forcedFixed (e.g. from the infinite lattice gyroid) would clobber
    // the policy back to Fixed in the bounds section below, before this
    // frame's pendingState is captured -- clipping the new mode's geometry to
    // the old bounds. The next dispatch recomputes forcedFixed from the new
    // field.
    vs.forcedFixed = false;
    vs.boundsPolicy = (vs.mode == Mode::Lattice) ? BoundsPolicy::Fixed
                                                 : BoundsPolicy::AutoFit;
    needsRecompute = true;
  }
  ImGui::Separator();

  if (vs.mode == Mode::Lattice) {
    static std::vector<const char*> typeLabels = [] {
      std::vector<const char*> r;
      for (const dce::TpmsKind& k : dce::tpmsKinds())
        r.push_back(k.name.c_str());
      return r;
    }();
    if (ImGui::Combo("Type", &vs.lat.tpmsIdx, typeLabels.data(),
                     static_cast<int>(typeLabels.size())))
      needsRecompute = true;

    ImGui::SliderFloat("Wavelength", &vs.lat.wavelength, 0.01f, 5.0f, "%.4f");
    if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;
    ImGui::SliderFloat("Offset", &vs.lat.offset, 0.0f, 1.0f, "%.4f");
    if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;
    ImGui::BeginDisabled(vs.lat.offset <= 0.0f);
    if (ImGui::Checkbox("Normalize thickness", &vs.lat.normalizeThickness))
      needsRecompute = true;
    ImGui::EndDisabled();
    if (vs.hasMeshA) {
      if (ImGui::Combo("Sign method", &vs.lat.signMethodIdx, kSignMethodLabels,
                       kNumSignMethods))
        needsRecompute = true;
    } else {
      ImGui::TextDisabled("(no input mesh: bare lattice in bounds)");
    }
  } else if (vs.mode == Mode::Primitive) {
    const auto& cat = dce::primitiveCatalogue();
    if (ImGui::Combo("Primitive", &vs.prim.prim.idx, primLabels().data(),
                     static_cast<int>(primLabels().size()))) {
      seedParams(vs.prim.prim, cat[vs.prim.prim.idx].defaults);
      needsRecompute = true;
    }
    paramSliders("prim", cat[vs.prim.prim.idx].signature, vs.prim.prim.params);

    ImGui::Separator();
    ImGui::Text("Post-ops (applied in order)");
    auto opHeader = [&](const char* name, PostOpUI& op) {
      if (ImGui::Checkbox(name, &op.enabled)) needsRecompute = true;
      return op.enabled;
    };
    if (opHeader("Offset", vs.prim.offset))
      paramSliders("p_off", "r", vs.prim.offset.params);
    if (opHeader("Onion", vs.prim.onion))
      paramSliders("p_oni", "thickness", vs.prim.onion.params);
    if (opHeader("Twist", vs.prim.twist)) {
      paramSliders("p_tw", "radians/unit", vs.prim.twist.params);
      if (ImGui::Combo("axis##tw", &vs.prim.twist.axis, kAxisLabels, 3))
        needsRecompute = true;
    }
    if (opHeader("Scale", vs.prim.scale))
      paramSliders("p_sc", "s", vs.prim.scale.params);
    if (opHeader("Elongate", vs.prim.elongate))
      paramSliders("p_el", "hx hy hz", vs.prim.elongate.params);
    if (opHeader("Mirror", vs.prim.mirror))
      paramSliders("p_mi", "nx ny nz", vs.prim.mirror.params);
    if (opHeader("Repeat", vs.prim.repeat))
      paramSliders("p_re", "px py pz", vs.prim.repeat.params);
    if (opHeader("Displace", vs.prim.displace)) {
      ImGui::InputText("bump##dis", vs.prim.displace.bump,
                       sizeof vs.prim.displace.bump);
      if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;
      ImGui::TextDisabled("name,amp,freq -- name: sine|gyroid|bumps");
    }
  } else if (vs.mode == Mode::Boolean) {
    if (ImGui::Combo("Operation", &vs.boo.opIdx, kBoolOpLabels, 7))
      needsRecompute = true;
    const bool smooth = vs.boo.opIdx >= 4;
    ImGui::BeginDisabled(!smooth);
    ImGui::DragFloat("Blend k", &vs.boo.k, 0.01f, 0.0f, 0.0f, "%.4f");
    if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;
    ImGui::EndDisabled();
    if (ImGui::Checkbox("Sharp (face normals)", &vs.boo.sharp))
      needsRecompute = true;
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("rebuilds the mesh BVH on change (brief stall)");
    if (ImGui::Combo("Sign method##b", &vs.boo.signMethodIdx, kSignMethodLabels,
                     kNumSignMethods))
      needsRecompute = true;

    auto operandUI = [&](const char* tag, Operand& op) {
      ImGui::Separator();
      ImGui::Text("Operand %s", tag);
      const auto& cat = dce::primitiveCatalogue();
      const char* kinds[] = {"Mesh A", "Mesh B", "Primitive"};
      int kindIdx = static_cast<int>(op.kind);
      const std::string clbl = std::string("source##") + tag;
      if (ImGui::Combo(clbl.c_str(), &kindIdx, kinds, 3)) {
        op.kind = static_cast<OperandKind>(kindIdx);
        if (op.kind == OperandKind::Primitive && op.prim.params.empty())
          seedParams(op.prim, cat[op.prim.idx].defaults);
        needsRecompute = true;
      }
      if (op.kind == OperandKind::Primitive) {
        const std::string plbl = std::string("prim##") + tag;
        if (ImGui::Combo(plbl.c_str(), &op.prim.idx, primLabels().data(),
                         static_cast<int>(primLabels().size()))) {
          seedParams(op.prim, cat[op.prim.idx].defaults);
          needsRecompute = true;
        }
        paramSliders((std::string("op") + tag).c_str(),
                     cat[op.prim.idx].signature, op.prim.params);
      } else {
        const bool ok =
            (op.kind == OperandKind::MeshA) ? vs.hasMeshA : vs.hasMeshB;
        if (!ok)
          ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                             "(mesh not loaded -- pass it on the command line)");
      }
    };
    operandUI("A", vs.boo.a);
    operandUI("B", vs.boo.b);
  } else {  // Mode::Csg
    const auto& recs = dce::csgRecipes();
    static std::vector<const char*> recLabels = [&recs] {
      std::vector<const char*> r;
      for (const dce::RecipeEntry& e : recs) r.push_back(e.name.c_str());
      return r;
    }();
    if (ImGui::Combo("Recipe", &vs.csg.recipe.idx, recLabels.data(),
                     static_cast<int>(recLabels.size()))) {
      seedParams(vs.csg.recipe, recs[vs.csg.recipe.idx].defaults);
      needsRecompute = true;
    }
    const dce::RecipeEntry& r = recs[vs.csg.recipe.idx];
    ImGui::TextWrapped("%s", r.blurb.c_str());
    if (r.needsMesh && !vs.hasMeshA)
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                         "needs an input mesh (pass one on the command line)");
    paramSliders("rec", r.signature, vs.csg.recipe.params);
  }

  // --- Shared sampler / contourer params ------------------------------
  ImGui::Separator();
  ImGui::SliderInt("Depth", &vs.depth, 3, 9);
  if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;
  ImGui::SliderFloat("Collapse error", &vs.collapseError, 0.0f, 0.1f, "%.5f");
  if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;

  // --- Bounds ---------------------------------------------------------
  ImGui::Separator();
  ImGui::Text("Bounds");
  auto boundsEditor = [&]() {
    ImGui::DragFloat3("min", vs.boundsMin, 0.05f);
    if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;
    ImGui::DragFloat3("max", vs.boundsMax, 0.05f);
    if (ImGui::IsItemDeactivatedAfterEdit()) needsRecompute = true;
  };
  if (vs.mode == Mode::Lattice) {
    boundsEditor();
  } else {
    if (vs.forcedFixed) {
      vs.boundsPolicy = BoundsPolicy::Fixed;
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                         "infinite field -- explicit bounds required");
    }
    int bp = static_cast<int>(vs.boundsPolicy);
    ImGui::BeginDisabled(vs.forcedFixed);
    if (ImGui::RadioButton("Auto-fit", &bp, 0)) {
      vs.boundsPolicy = BoundsPolicy::AutoFit;
      needsRecompute = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Fixed", &bp, 1)) {
      vs.boundsPolicy = BoundsPolicy::Fixed;
      needsRecompute = true;
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(vs.boundsPolicy != BoundsPolicy::Fixed);
    boundsEditor();
    ImGui::EndDisabled();
  }

  ImGui::PopItemWidth();

  // --- Diagnostic structure toggles -----------------------------------
  ImGui::Separator();
  ImGui::Text("Diagnostic overlays");
  const bool heavy = vs.lastLeaves > kHeavyDiagLeafThreshold;
  if (heavy) {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                       "Heavy: %zu leaves > %zu threshold", vs.lastLeaves,
                       kHeavyDiagLeafThreshold);
    if (ImGui::Checkbox("Allow heavy diagnostics", &vs.allowHeavyDiag))
      needsApplyDiag = true;
  }
  if (ImGui::Checkbox("Octree leaves", &vs.showOctreeLeaves))
    needsApplyDiag = true;
  {
    const bool gated = heavy && !vs.allowHeavyDiag;
    ImGui::BeginDisabled(gated);
    if (ImGui::Checkbox("Octree wireframe", &vs.showOctreeWire))
      needsApplyDiag = true;
    ImGui::EndDisabled();
  }
  if (ImGui::Checkbox("Hermite crossings", &vs.showHermite))
    needsApplyDiag = true;
  ImGui::BeginDisabled(!vs.showHermite);
  if (ImGui::Checkbox("Hermite normals", &vs.showHermiteNormals))
    needsApplyDiag = true;
  ImGui::EndDisabled();
  {
    const bool gated = heavy && !vs.allowHeavyDiag;
    ImGui::BeginDisabled(gated);
    if (ImGui::Checkbox("Sign-oracle corners", &vs.showSignOracle))
      needsApplyDiag = true;
    ImGui::EndDisabled();
  }
  if (ImGui::Checkbox("Field-value heatmap", &vs.showFieldHeatmap))
    needsApplyDiag = true;

  // --- Recontour button + status --------------------------------------
  ImGui::Separator();
  if (ImGui::Button("Recontour")) needsRecompute = true;
  ImGui::SameLine();
  const bool busy = ac.running.load(std::memory_order_relaxed);
  if (busy) {
    ImGui::Text("Contouring...");
  } else {
    ImGui::Text("%zu verts  %zu faces  %zu leaves  %.1f ms", vs.lastVerts,
                vs.lastFaces, vs.lastLeaves, vs.lastWallMs);
  }

  if (needsRecompute) {
    ac.pendingState = vs;
    ac.pendingDirty = true;
  }
  if (needsApplyDiag) applyDiagStructures(vs, reg, flags);

  pumpAsync(ac, vs, reg, flags, cache, in);
}

// --- CLI -------------------------------------------------------------

void printUsage() {
  std::cerr <<
      "dualc_view -- interactive Polyscope viewer for DualC fields.\n"
      "\n"
      "USAGE\n"
      "  dualc_view [<inputs...>] [options]\n"
      "\n"
      "MODES (pick with --mode, or inferred from --prim / --op / --recipe;\n"
      "default lattice). Every parameter is also a live control once open.\n"
      "  lattice                TPMS lattice; with one input mesh it is\n"
      "                         clipped to the mesh interior.\n"
      "  primitive (--prim N)   one analytic primitive + a post-op stack.\n"
      "                         (primitive params are tuned in the panel, not\n"
      "                         on the CLI.)\n"
      "  boolean   (--op OP)    SDF boolean of two operands (each a loaded\n"
      "                         mesh A/B or a primitive); --k sets the smooth\n"
      "                         blend radius, --sharp uses face normals.\n"
      "  csg       (--recipe N) a named recipe; its constants are sliders.\n"
      "\n"
      "OPTIONS\n"
      "  --mode lattice|primitive|boolean|csg\n"
      "  --type NAME            TPMS family (lattice). Default: gyroid.\n"
      "  --wavelength W         Lattice unit-cell side (world units).\n"
      "  --offset T             Lattice thick-wall offset via onionOf.\n"
      "  --normalize-thickness  Normalise the TPMS before the offset.\n"
      "  --prim NAME            Primitive name (see dualc_primitive --list).\n"
      "  --op OP                Boolean op: union|intersection|difference|xor|\n"
      "                         smooth-union|smooth-intersection|smooth-difference.\n"
      "  --k K                  Boolean smooth blend radius (default 0.25).\n"
      "  --sharp                Boolean: face normals (no interpolation).\n"
      "  --recipe NAME          CSG recipe (see dualc_csg_demo --list).\n"
      "  --depth N              Octree max depth (default 7).\n"
      "  --collapse E           Adaptive cell-collapse threshold (default 0).\n"
      "  --bounds x0,y0,z0,x1,y1,z1   Explicit sampling region.\n"
      "  --help, -h             Show this message.\n"
      "\n"
      "EXAMPLES\n"
      "  dualc_view cube.obj --type gyroid --wavelength 0.5\n"
      "  dualc_view --prim torus\n"
      "  dualc_view --op smooth-union a.obj b.obj --k 0.3\n"
      "  dualc_view --recipe smooth-blend\n";
}

geometrycentral::surface::SurfaceMesh* loadInto(
    MeshSlot& slot, const std::string& path) {
  std::cout << "[dualc_view] reading " << path << "\n";
  auto pair = geometrycentral::surface::readSurfaceMesh(path);
  slot.mesh = std::move(std::get<0>(pair));
  slot.geom = std::move(std::get<1>(pair));
  std::cout << "[dualc_view] input: " << slot.mesh->nVertices() << " verts, "
            << slot.mesh->nFaces() << " faces\n";
  return slot.mesh.get();
}

} // namespace

int main(int argc, char** argv) {
  Mode mode = Mode::Lattice;
  bool modeExplicit = false;
  std::string type = "gyroid";
  std::string primName, recipeName;
  std::string boolOpName = "union";
  double wavelength = -1.0, offset = 0.0, kBlend = 0.25;
  bool normalizeThickness = false, sharp = false, haveBounds = false;
  dualc::BBox userBounds;
  int cliDepth = 7;
  double cliCollapse = 0.0;
  std::vector<std::string> positionals;
  std::vector<dce::PostOp> cliPostOps;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_view] " << f << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    } else if (a == "--mode") {
      const char* v = needValue("--mode"); if (!v) return 2;
      const std::string m = v;
      if (m == "lattice") mode = Mode::Lattice;
      else if (m == "primitive") mode = Mode::Primitive;
      else if (m == "boolean") mode = Mode::Boolean;
      else if (m == "csg") mode = Mode::Csg;
      else { std::cerr << "[dualc_view] unknown --mode '" << m << "'\n"; return 2; }
      modeExplicit = true;
    } else if (a == "--type") {
      const char* v = needValue("--type"); if (!v) return 2; type = v;
    } else if (a == "--wavelength") {
      const char* v = needValue("--wavelength"); if (!v) return 2;
      wavelength = std::atof(v);
    } else if (a == "--offset") {
      const char* v = needValue("--offset"); if (!v) return 2;
      offset = std::atof(v);
    } else if (a == "--normalize-thickness") {
      normalizeThickness = true;
    } else if (a == "--prim") {
      const char* v = needValue("--prim"); if (!v) return 2;
      primName = v;
      if (!modeExplicit) mode = Mode::Primitive;
    } else if (a == "--op") {
      const char* v = needValue("--op"); if (!v) return 2;
      boolOpName = v;
      if (!modeExplicit) mode = Mode::Boolean;
    } else if (a == "--k") {
      const char* v = needValue("--k"); if (!v) return 2;
      kBlend = std::atof(v);
    } else if (a == "--sharp") {
      sharp = true;
    } else if (a == "--recipe") {
      const char* v = needValue("--recipe"); if (!v) return 2;
      recipeName = v;
      if (!modeExplicit) mode = Mode::Csg;
    } else if (a == "--depth") {
      const char* v = needValue("--depth"); if (!v) return 2;
      cliDepth = std::atoi(v);
    } else if (a == "--collapse") {
      const char* v = needValue("--collapse"); if (!v) return 2;
      cliCollapse = std::atof(v);
    } else if (a == "--bounds") {
      const char* v = needValue("--bounds"); if (!v) return 2;
      if (!dce::parseBounds(v, userBounds)) {
        std::cerr << "[dualc_view] --bounds expects six comma-separated "
                     "doubles\n";
        return 2;
      }
      haveBounds = true;
    } else if (dce::isPostOpFlag(a)) {
      const char* v = needValue(a.c_str()); if (!v) return 2;
      dce::PostOp op; std::string err;
      if (!dce::parsePostOp(a, v, op, err)) {
        std::cerr << "[dualc_view] " << err << "\n";
        return 2;
      }
      cliPostOps.push_back(std::move(op));
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_view] unknown flag: " << a << "\n";
      return 2;
    } else {
      positionals.push_back(a);
    }
  }

  // --- Validate mode-specific selections ------------------------------
  if (mode == Mode::Lattice &&
      dce::makeTpmsField(type, Vector3{0, 0, 0}, 1.0) == nullptr) {
    std::cerr << "[dualc_view] unknown --type '" << type << "'\n";
    return 2;
  }
  if (offset < 0.0) {
    std::cerr << "[dualc_view] --offset must be >= 0\n";
    return 2;
  }
  int primIdx = 0;
  if (mode == Mode::Primitive && !primName.empty()) {
    const dce::PrimEntry* e = dce::findPrimitive(primName);
    if (e == nullptr) {
      std::cerr << "[dualc_view] unknown --prim '" << primName
                << "' (see dualc_primitive --list)\n";
      return 2;
    }
    const auto& cat = dce::primitiveCatalogue();
    for (std::size_t i = 0; i < cat.size(); ++i)
      if (cat[i].name == primName) { primIdx = static_cast<int>(i); break; }
  }
  dce::BoolOp boolOp = dce::BoolOp::Union;
  if (mode == Mode::Boolean && !dce::parseBoolOp(boolOpName, boolOp)) {
    std::cerr << "[dualc_view] unknown --op '" << boolOpName << "'\n";
    return 2;
  }
  int recipeIdx = 0;
  if (mode == Mode::Csg && !recipeName.empty()) {
    const dce::RecipeEntry* r = dce::findRecipe(recipeName);
    if (r == nullptr) {
      std::cerr << "[dualc_view] unknown --recipe '" << recipeName
                << "' (see dualc_csg_demo --list)\n";
      return 2;
    }
    const auto& recs = dce::csgRecipes();
    for (std::size_t i = 0; i < recs.size(); ++i)
      if (recs[i].name == recipeName) { recipeIdx = static_cast<int>(i); break; }
  }

  // --- Load input meshes per mode -------------------------------------
  LoadedInputs inputs;
  if (mode == Mode::Boolean) {
    if (positionals.size() > 2) {
      std::cerr << "[dualc_view] boolean mode takes at most two input meshes\n";
      return 2;
    }
    if (positionals.size() >= 1) loadInto(inputs.a, positionals[0]);
    if (positionals.size() >= 2) loadInto(inputs.b, positionals[1]);
  } else {
    if (positionals.size() > 1) {
      std::cerr << "[dualc_view] this mode takes at most one input mesh\n";
      return 2;
    }
    if (positionals.size() == 1) loadInto(inputs.a, positionals[0]);
  }

  // --- Bounds + lattice center ----------------------------------------
  Vector3 fieldCenter{0.0, 0.0, 0.0};
  dualc::BBox bounds;
  if (inputs.a.loaded()) {
    const dualc::BBox aabb = meshAABB(*inputs.a.mesh, *inputs.a.geom);
    fieldCenter = aabb.center();
    if (wavelength <= 0.0) {
      const Vector3 e = aabb.extent();
      wavelength = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z) / 10.0;
    }
    bounds = haveBounds ? userBounds : padBBox(aabb, 0.05);
  } else {
    if (wavelength <= 0.0) wavelength = 0.25;
    bounds = haveBounds ? userBounds : dualc::BBox::unit();
  }

  // --- Initial state --------------------------------------------------
  ViewerState state;
  state.mode = mode;
  state.lat.tpmsIdx = 0;
  for (std::size_t i = 0; i < dce::tpmsKinds().size(); ++i)
    if (dce::tpmsKinds()[i].name == type) {
      state.lat.tpmsIdx = static_cast<int>(i);
      break;
    }
  state.lat.wavelength = static_cast<float>(wavelength);
  state.lat.offset = static_cast<float>(offset);
  state.lat.normalizeThickness = normalizeThickness;

  initPostOps(state.prim);
  seedParams(state.prim.prim, dce::primitiveCatalogue()[primIdx].defaults);
  state.prim.prim.idx = primIdx;
  // Map any CLI post-op flags onto the fixed UI stack (widget ops only).
  for (const dce::PostOp& op : cliPostOps) {
    auto set = [&](PostOpUI& u) {
      u.enabled = true;
      u.params.assign(op.params.begin(), op.params.end());
    };
    switch (op.kind) {
      case dce::PostOpKind::Offset:
      case dce::PostOpKind::Round:   set(state.prim.offset); break;
      case dce::PostOpKind::Onion:   set(state.prim.onion); break;
      case dce::PostOpKind::Scale:   set(state.prim.scale); break;
      case dce::PostOpKind::Elongate:set(state.prim.elongate); break;
      case dce::PostOpKind::Mirror:  set(state.prim.mirror); break;
      case dce::PostOpKind::Repeat:  set(state.prim.repeat); break;
      case dce::PostOpKind::Twist:
        state.prim.twist.enabled = true;
        if (!op.params.empty())
          state.prim.twist.params = {static_cast<float>(op.params[0])};
        if (op.params.size() > 1)
          state.prim.twist.axis = static_cast<int>(op.params[1]);
        break;
      case dce::PostOpKind::Displace: {
        state.prim.displace.enabled = true;
        const std::size_t cap = sizeof state.prim.displace.bump - 1;
        const std::size_t n = std::min(op.bumpSpec.size(), cap);
        std::memcpy(state.prim.displace.bump, op.bumpSpec.data(), n);
        state.prim.displace.bump[n] = '\0';
        break;
      }
      default:
        std::cerr << "[dualc_view] post-op not supported in the viewer "
                     "preload; ignoring\n";
        break;
    }
  }

  state.boo.opIdx = static_cast<int>(boolOp);
  state.boo.k = static_cast<float>(kBlend);
  state.boo.sharp = sharp;
  // Default boolean operands to the loaded meshes, else to primitives.
  state.boo.a.kind = inputs.a.loaded() ? OperandKind::MeshA
                                       : OperandKind::Primitive;
  state.boo.b.kind = inputs.b.loaded() ? OperandKind::MeshB
                                       : OperandKind::Primitive;
  seedParams(state.boo.a.prim, dce::primitiveCatalogue()[0].defaults);
  seedParams(state.boo.b.prim, dce::primitiveCatalogue()[0].defaults);

  state.csg.recipe.idx = recipeIdx;
  seedParams(state.csg.recipe, dce::csgRecipes()[recipeIdx].defaults);

  state.depth = cliDepth;
  state.collapseError = static_cast<float>(cliCollapse);
  state.fieldCenter = fieldCenter;
  state.hasMeshA = inputs.a.loaded();
  state.hasMeshB = inputs.b.loaded();
  setBoundsArr(state, bounds);
  // Non-lattice modes auto-fit by default (TPMS lattice stays fixed/infinite).
  state.boundsPolicy =
      (mode == Mode::Lattice) ? BoundsPolicy::Fixed : BoundsPolicy::AutoFit;
  if (haveBounds) state.boundsPolicy = BoundsPolicy::Fixed;

  std::cout << "[dualc_view] mode = " << kModeLabels[static_cast<int>(mode)]
            << ", depth = " << cliDepth << "\n";
  std::cout << "[dualc_view] contouring (initial)...\n";

  // --- Polyscope -------------------------------------------------------
  polyscope::init();

  AsyncContourer ac;
  RegisteredDiag reg;
  DiagFlags      flags;
  MeshSourceCache cache;

  // Initial contour: dispatch via the async pipeline, then block on this first
  // job so the window opens with content registered.
  ac.pendingState = state;
  ac.pendingDirty = true;
  pumpAsync(ac, state, reg, flags, cache, inputs);  // dispatch
  if (ac.worker.joinable()) ac.worker.join();        // wait
  pumpAsync(ac, state, reg, flags, cache, inputs);  // consume

  if (inputs.a.loaded()) {
    auto* psIn = polyscope::registerSurfaceMesh(
        "input mesh A", inputs.a.geom->inputVertexPositions,
        inputs.a.mesh->getFaceVertexList());
    psIn->setTransparency(0.25f);
  }
  if (inputs.b.loaded()) {
    auto* psIn = polyscope::registerSurfaceMesh(
        "input mesh B", inputs.b.geom->inputVertexPositions,
        inputs.b.mesh->getFaceVertexList());
    psIn->setTransparency(0.25f);
  }

  polyscope::state::userCallback =
      [&state, &ac, &reg, &flags, &cache, &inputs]() {
        drawCallback(state, ac, reg, flags, cache, inputs);
      };

  polyscope::show();

  if (ac.worker.joinable()) ac.worker.join();
  return 0;
}
