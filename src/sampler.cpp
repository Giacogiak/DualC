#include "dualc/sampler.h"

#include "dualc/implicit.h"

#include "internal/dc_tables.h"
#include "internal/octree.h"
#include "internal/parallel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace dualc {

namespace {

// Pad a valid bounding box outward by `padFraction` of its largest extent.
BBox padBBox(BBox b, double padFraction) {
  const Vector3 ext = b.extent();
  const double maxDim = std::max({ext.x, ext.y, ext.z, 1e-12});
  const Vector3 pad{maxDim * padFraction, maxDim * padFraction, maxDim * padFraction};
  b.min = b.min - pad;
  b.max = b.max + pad;
  return b;
}

// True iff `inner` lies entirely within `outer` (closed test, no tolerance).
bool contains(const BBox& outer, const BBox& inner) {
  return inner.min.x >= outer.min.x && inner.max.x <= outer.max.x &&
         inner.min.y >= outer.min.y && inner.max.y <= outer.max.y &&
         inner.min.z >= outer.min.z && inner.max.z <= outer.max.z;
}

Vector3 cornerPosition(const BBox& b, int cornerIdx) {
  const auto& off = tables::kCornerOffset[static_cast<std::size_t>(cornerIdx)];
  return Vector3{
      off[0] ? b.max.x : b.min.x,
      off[1] ? b.max.y : b.min.y,
      off[2] ? b.max.z : b.min.z};
}

// Populate a leaf's Hermite data from corner signs + edge crossings.
void populateLeaf(HermiteNode& node,
                  const std::array<bool, 8>& signs,
                  const ImplicitField& field) {
  auto data = std::make_unique<HermiteLeafData>();
  data->cornerInside = signs;

  for (std::uint8_t e = 0; e < 12; ++e) {
    const auto endpoints = tables::kEdgeEndpoints[e];
    const bool sa = signs[endpoints[0]];
    const bool sb = signs[endpoints[1]];
    if (sa == sb) continue;

    const Vector3 a = cornerPosition(node.bounds, endpoints[0]);
    const Vector3 b = cornerPosition(node.bounds, endpoints[1]);

    // Always query from the outside endpoint towards the inside one, so the
    // reported crossing is the surface entry point.
    Vector3 outP, outN;
    const bool hit = sa ? field.edgeHit(b, a, outP, outN)
                        : field.edgeHit(a, b, outP, outN);

    HermiteEdge& he = data->edges[e];
    if (hit) {
      he.position    = outP;
      he.normal      = outN;
      he.hasCrossing = true;
    } else {
      // segmentFirstHit missed a known crossing -- typically a near-grazing
      // hit lost to single-precision in the BVH. Substitute the closest
      // surface point + normal; far better for the downstream QEF than
      // (midpoint, 0), which contributes a zero row.
      const Vector3 mid = (a + b) * 0.5;
      Vector3 cpP, cpN;
      if (field.closestSurfacePoint(mid, cpP, cpN)) {
        he.position    = cpP;
        he.normal      = cpN;
        he.hasCrossing = true;
      } else {
        he.position    = mid;
        he.normal      = Vector3{0.0, 0.0, 0.0};
        he.hasCrossing = true;
      }
    }
  }

  node.leaf = std::move(data);
}

// Recursively subdivide a node. Refinement is purely geometric: the cell is
// split if the field's surface may pass through it. minDepth is a floor.
void buildNode(HermiteNode& node, int depth, int minDepth, int maxDepth,
               const ImplicitField& field, const CancelToken* cancel) {
  if (depth >= maxDepth) {
    std::array<bool, 8> signs{};
    for (int c = 0; c < 8; ++c) {
      signs[static_cast<std::size_t>(c)] =
          field.isInside(cornerPosition(node.bounds, c));
    }
    node.isLeaf = true;
    populateLeaf(node, signs, field);
    return;
  }

  // Cancellation checkpoint: one relaxed load per internal node, so at most
  // the 8 leaves under a maxDepth-1 node are built between two polls. The
  // throw unwinds through internal::parallelFor, which joins every worker
  // before rethrowing on the calling thread.
  if (cancel) cancel->throwIfRequested("sampling");

  const bool forceRefine   = (depth < minDepth);
  const bool surfaceInCell = field.cellOverlaps(node.bounds);

  if (!forceRefine && !surfaceInCell) {
    node.isLeaf = true;
    node.leaf.reset();
    return;
  }

  node.isLeaf = false;
  node.leaf.reset();
  for (int c = 0; c < 8; ++c) {
    auto child = std::make_unique<HermiteNode>();
    child->bounds = internal::childBounds(node.bounds, c);
    child->depth  = depth + 1;
    buildNode(*child, depth + 1, minDepth, maxDepth, field, cancel);
    node.children[static_cast<std::size_t>(c)] = std::move(child);
  }
}

// Refine serially down to `stopDepth`, collecting the surviving nodes at that
// depth into `frontier`. Each frontier node roots an independent subtree, so
// the heavy refinement below the fork depth can then run in parallel without
// any shared writes -- the resulting octree is bit-identical to a fully
// serial build. Mirrors buildNode's prune/refine decision exactly.
void collectFrontier(HermiteNode& node, int depth, int minDepth, int maxDepth,
                     int stopDepth, const ImplicitField& field,
                     std::vector<HermiteNode*>& frontier) {
  if (depth >= stopDepth || depth >= maxDepth) {
    frontier.push_back(&node);
    return;
  }

  const bool forceRefine   = (depth < minDepth);
  const bool surfaceInCell = field.cellOverlaps(node.bounds);
  if (!forceRefine && !surfaceInCell) {
    node.isLeaf = true;
    node.leaf.reset();
    return;
  }

  node.isLeaf = false;
  node.leaf.reset();
  for (int c = 0; c < 8; ++c) {
    auto child = std::make_unique<HermiteNode>();
    child->bounds = internal::childBounds(node.bounds, c);
    child->depth  = depth + 1;
    HermiteNode* childPtr = child.get();
    node.children[static_cast<std::size_t>(c)] = std::move(child);
    collectFrontier(*childPtr, depth + 1, minDepth, maxDepth, stopDepth,
                    field, frontier);
  }
}

} // namespace

HermiteOctree sampleFieldToHermiteOctree(const ImplicitField& field,
                                         const SamplerParams& params,
                                         Diagnostics* diag,
                                         const CancelToken* cancel,
                                         ProgressSink* progress) {
  // Depths decide how much memory the build asks for, so a nonsense pair is
  // worth rejecting before it allocates rather than after. No upper cap is
  // imposed: a deep maxDepth is legitimate under the tiled export path, and
  // any fixed ceiling here would be an arbitrary policy in the wrong layer.
  if (params.minDepth < 0 || params.maxDepth < 0)
    throw std::invalid_argument(
        "SamplerParams: minDepth and maxDepth must be >= 0 (got minDepth=" +
        std::to_string(params.minDepth) + ", maxDepth=" +
        std::to_string(params.maxDepth) + ")");
  if (params.minDepth > params.maxDepth)
    throw std::invalid_argument(
        "SamplerParams: minDepth (" + std::to_string(params.minDepth) +
        ") must not exceed maxDepth (" + std::to_string(params.maxDepth) + ")");

  BBox bounds;
  const bool explicitRoot = params.rootBounds.has_value();
  if (explicitRoot) {
    bounds = *params.rootBounds;
  } else {
    const BBox raw = field.bounds();
    if (raw.isInfinite()) {
      throw std::invalid_argument(
          "sampleFieldToHermiteOctree: field has infinite bounds (e.g. a "
          "plane or an infinite cylinder/cone). Set SamplerParams::rootBounds "
          "to an explicit finite box to sample it.");
    }
    const bool valid = raw.isValid();
    bounds = valid ? padBBox(raw, params.padFraction) : BBox::unit();
    if (diag) diag->boundsFallback = !valid;
  }

  // A baked grid is the one field whose bounds() is a hard domain rather than
  // just where the surface is: outside it, grid_field.cpp clamps to the
  // nearest face value. Reported only for a GridField at the root -- see
  // Diagnostics::gridBoundsExceeded for why it stops there.
  //
  // Only for an EXPLICIT root box. The auto-fit path pads the field's own
  // bounds outward by padFraction, so it always lands slightly outside a
  // baked grid -- flagging that would fire on every default contour of a
  // grid and say nothing about the caller's intent. The flag means "you
  // asked for a region the bake does not cover".
  if (diag && explicitRoot) {
    if (const auto* grid = dynamic_cast<const GridField*>(&field)) {
      diag->gridBoundsExceeded = !contains(grid->bounds(), bounds);
    }
  }

  // A token requested before the call cancels before any field query or
  // allocation.
  if (cancel) cancel->throwIfRequested("sampling");

  HermiteOctree octree(bounds);
  if (octree.root() == nullptr) return octree;

  // Build the tree in parallel: refine serially to a shallow fork depth, then
  // build each independent frontier subtree on a worker thread. Output is
  // bit-identical to a serial build regardless of the thread count.
  const int stopDepth = std::min(params.maxDepth, 3);
  std::vector<HermiteNode*> frontier;
  collectFrontier(*octree.root(), /*depth=*/0, params.minDepth,
                  params.maxDepth, stopDepth, field, frontier);
  const auto body = [&](std::size_t i) {
    HermiteNode* n = frontier[i];
    buildNode(*n, n->depth, params.minDepth, params.maxDepth, field, cancel);
  };
  if (progress) {
    // The polled variant keeps the calling thread out of the work so every
    // report comes from it; the workers' scheduling and the output are the
    // same as parallelFor's.
    const std::size_t n = frontier.size();
    progress->report(Stage::Sample, 0, n);
    internal::parallelForPolled(n, params.numThreads, body,
                                [&](std::size_t done, std::size_t total) {
                                  progress->report(Stage::Sample, done, total);
                                });
    progress->report(Stage::Sample, n, n);
  } else {
    internal::parallelFor(frontier.size(), params.numThreads, body);
  }

  return octree;
}

HermiteOctree sampleMeshToHermiteOctree(
    geometrycentral::surface::SurfaceMesh& mesh,
    geometrycentral::surface::VertexPositionGeometry& geometry,
    const SamplerParams& params,
    Diagnostics* diag,
    const CancelToken* cancel,
    ProgressSink* progress) {
  MeshSource source(mesh, geometry, params.interpolateNormals,
                    params.signMethod);
  if (diag) {
    diag->inputEmpty            = source.triangleCount() == 0;
    diag->inputBoundaryEdges    = source.boundaryEdgeCount();
    diag->inputNonManifoldEdges = source.nonManifoldEdgeCount();
    diag->inputWatertight       = diag->inputBoundaryEdges == 0 &&
                                  diag->inputNonManifoldEdges == 0;
  }
  return sampleFieldToHermiteOctree(source, params, diag, cancel, progress);
}

} // namespace dualc
