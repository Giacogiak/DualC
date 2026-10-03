#pragma once

#include "dualc/types.h"

#include <functional>
#include <memory>
#include <vector>

namespace geometrycentral {
namespace surface {
class SurfaceMesh;
class VertexPositionGeometry;
} // namespace surface
} // namespace geometrycentral

namespace dualc {

// A real-valued implicit field. SDF convention: valueAt < 0 inside, > 0
// outside, == 0 on the surface. Every v2 source, boolean combinator and
// decorator is an ImplicitField; the sampler walks an octree driven by one.
//
// `valueAt`, `gradientAt` and `bounds` are what every field must implement.
// `isInside`, `edgeHit`, `cellOverlaps` and `closestSurfacePoint` have correct
// default implementations layered on top of them, and are overridden only when
// a source has a cheaper closed-form path (e.g. MeshSource uses its BVH, and
// overrides all four). Those four are exactly the sampler's call graph:
// corner signs, the refine/prune decision, the Hermite data, and the
// miss fallback.
class ImplicitField {
public:
  // THREAD SAFETY -- read this before deriving from ImplicitField.
  //
  // Every method below is called CONCURRENTLY from multiple worker threads: the
  // sampler defaults to SamplerParams::numThreads = 0 (all hardware threads),
  // and the contourer parallelizes its leaf solves the same way. A subclass
  // must therefore be safe to call from several threads at once on a single
  // instance -- in practice, treat `const` as meaning genuinely immutable.
  //
  // The trap this warns about: a `mutable` memoisation cache -- the obvious
  // optimisation for an expensive custom field -- compiles, passes every
  // single-threaded test, and then races silently in production. To amortise an
  // expensive field, bake it once instead (see bakeToGrid / GridField below)
  // and sample the result, which is immutable and shareable by construction.
  virtual ~ImplicitField() = default;

  // Field value at p (negative inside, positive outside, zero on surface).
  virtual double valueAt(const Vector3& p) const = 0;

  // Field gradient at p. Normalized, this is the outward surface normal
  // where valueAt == 0.
  virtual Vector3 gradientAt(const Vector3& p) const = 0;

  // Conservative axis-aligned extent of the surface. May be invalid (an
  // empty field); the sampler then falls back to a unit root box.
  virtual BBox bounds() const = 0;

  // --- derived; overridable for closed-form fast paths ---

  // Inside test. Default: valueAt(p) < 0.
  virtual bool isInside(const Vector3& p) const;

  // Find the surface crossing on segment [a, b]. On success returns true and
  // fills outP (crossing point) and outN (unit outward normal). The default
  // brackets by endpoint sign and bisects on valueAt; it reports a crossing
  // only when the endpoints straddle the surface.
  virtual bool edgeHit(const Vector3& a, const Vector3& b,
                       Vector3& outP, Vector3& outN) const;

  // True when the surface may pass through the cell. Default: corner-sign
  // disagreement plus a conservative Lipschitz-bound centre test.
  //
  // This is the sampler's only pruning gate, so it is a CORRECTNESS opt-out as
  // well as a fast path. Two obligations follow:
  //  - A field whose value is not a Lipschitz-1 distance (every TPMS, and the
  //    winding-number field) must override this and report an overlap
  //    unconditionally -- the default centre test under-refines on it and
  //    silently drops surface cells.
  //  - Any wrapper that warps, offsets or transforms the domain must override
  //    this too, forwarding to its child on a box that CONTAINS the child's
  //    image of the cell. Forwarding is what carries a non-Lipschitz child's
  //    always-overlap signal up the tree; a wrapper that leaves the default in
  //    place silently discards it. Over-refining costs time; under-refining
  //    loses geometry, so any superset of the image is a valid answer.
  virtual bool cellOverlaps(const BBox& cell) const;

  // Closest point on the surface to q, with the field's normal there. Used
  // by the Hermite sampler as a fallback when `edgeHit` misses a known
  // crossing (single-precision grazing on a thin feature). Default impl
  // does one Newton step via valueAt / gradientAt -- good enough on a
  // Lipschitz-1 SDF; MeshSource overrides with a direct BVH closest-point
  // query. Returns false on a degenerate / empty field.
  virtual bool closestSurfacePoint(const Vector3& q,
                                   Vector3& outP, Vector3& outN) const;
};

// Shared handle to a field. Sources, combinators and decorators are all
// composed through this type, forming an immutable expression tree.
//
// The `const` is the type system enforcing that immutability rather than a
// comment asserting it: once a field is built it is only ever evaluated, and
// every evaluation method is const. A `shared_ptr<Derived>` from make_shared
// converts implicitly, so factories and call sites are unaffected.
using FieldPtr = std::shared_ptr<const ImplicitField>;

// An ImplicitField backed by a triangle mesh: the field is the signed
// distance to the mesh surface (sign from ray-parity, magnitude from a
// closest-point query).
//
// Lifetime: the mesh and geometry are read during construction ONLY. The
// internal MeshBVH copies them into packed float arrays and owns that copy
// (see src/internal/mesh_bvh.h), and the AABB is computed up front, so nothing
// here points back at the caller's data. The arguments may be destroyed as soon
// as the constructor returns -- a temporary mesh is fine, and so is building a
// field from geometry you then release. (This comment previously claimed the
// opposite; it was never true of the shipped code.)
class MeshSource : public ImplicitField {
public:
  // `interpolateNormals` controls the edge-crossing normal: a barycentric
  // blend of the input's per-vertex normals (smooth, default) or the hit
  // triangle's face normal (sharp). Mirrors SamplerParams::interpolateNormals.
  //
  // `signMethod` selects the inside/outside oracle:
  //   - WINDING_NUMBER (default) -> 3-ray majority parity; tolerant of
  //     thin features and near-coplanar configurations.
  //   - PSEUDONORMAL -> single closest-point + angle-weighted feature
  //     normal; faster but only correct on watertight oriented input.
  MeshSource(geometrycentral::surface::SurfaceMesh& mesh,
             geometrycentral::surface::VertexPositionGeometry& geometry,
             bool       interpolateNormals = true,
             SignMethod signMethod         = SignMethod::WINDING_NUMBER);
  ~MeshSource() override;

  MeshSource(const MeshSource&)            = delete;
  MeshSource& operator=(const MeshSource&) = delete;

  double  valueAt(const Vector3& p)    const override;
  Vector3 gradientAt(const Vector3& p) const override;
  BBox    bounds() const override;
  bool    isInside(const Vector3& p)   const override;
  bool    edgeHit(const Vector3& a, const Vector3& b,
                  Vector3& outP, Vector3& outN) const override;
  bool    cellOverlaps(const BBox& cell) const override;
  bool    closestSurfacePoint(const Vector3& q,
                              Vector3& outP, Vector3& outN) const override;

  // Narrow-band bake into a GridField. The exact signed distance is computed
  // only within `bandWidth` (world units) of the mesh surface; beyond it the
  // sign is resolved per connected component (one inside/outside query each)
  // and the magnitude grown by a chamfer transform. Far faster than the generic
  // bakeToGrid() for a mesh -- the expensive BVH SDF query runs only on the
  // band, not the whole res^3 lattice. `bandWidth` is raised to a safe floor
  // (1.5x the largest cell) if smaller; pick it >= the boolean blend radius so
  // a later smooth blend stays exact.
  //
  // `region` may be ANY box: padded around the mesh, flush with its AABB, only
  // partly overlapping it, or strictly inside the solid. (It did once have to
  // be padded -- the far-field sign was flood-filled from the region faces on
  // the assumption they were outside, which dissolved interior material for
  // every other case, and filled enclosed cavities.) This rests on the same
  // watertight-input precondition the rest of MeshSource carries; open/soup
  // input has no well-defined inside here -- use WindingNumberField.
  //
  // Beyond the band the magnitude is a monotone OVER-estimate of the true
  // distance -- deliberate, so trilinear interpolation stays monotone across the
  // band boundary and the contour octree does not over-refine the far field. A
  // consumer that needs a conservative (never-overshooting) distance -- a sphere
  // tracer -- must not step by this field unnormalized.
  FieldPtr bakeToGrid(const BBox& region, const Vector3i& resolution,
                      double bandWidth) const;

  // Facts about the input mesh, measured once at construction and free to
  // read (the edge counts fall out of the pseudonormal topology pass). They
  // exist because the preconditions this class carries -- watertight,
  // consistently oriented -- are otherwise unobservable to a caller: the
  // PSEUDONORMAL sign path degrades to the raw face normal on a boundary or
  // non-manifold edge and says nothing. `sampleMeshToHermiteOctree` reads
  // them into dualc::Diagnostics; prefer SignMethod::GENERALIZED_WINDING_NUMBER
  // when either edge count is non-zero.
  std::size_t triangleCount() const;
  std::size_t boundaryEdgeCount() const;      // edges with 1 incident triangle
  std::size_t nonManifoldEdgeCount() const;   // edges with 3 or more

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// An ImplicitField whose surface is the 0.5-level set of the generalized
// winding number (GWN) of a triangle mesh. Unlike MeshSource, *every* query
// -- sign, value and edge crossings -- derives from the GWN scalar, so the
// field stays self-consistent on non-watertight input: open shells, triangle
// soup and self-intersecting meshes contour into a watertight solid, with
// open holes sealed by a smooth cap.
//
// `valueAt` returns 0.5 - w(p): negative inside, positive outside, zero on
// the isosurface. It is a sign-correct pseudo-SDF -- its magnitude is NOT a
// metric distance. Accepts arbitrary triangle soup (disconnected, non-manifold,
// open) -- GWN never inspects connectivity.
//
// Lifetime: as with MeshSource, the mesh and geometry are read during
// construction only; the internal MeshBVH owns its copy, so they need not
// outlive the field.
class WindingNumberField : public ImplicitField {
public:
  WindingNumberField(
      geometrycentral::surface::SurfaceMesh& mesh,
      geometrycentral::surface::VertexPositionGeometry& geometry);
  ~WindingNumberField() override;

  WindingNumberField(const WindingNumberField&)            = delete;
  WindingNumberField& operator=(const WindingNumberField&) = delete;

  double  valueAt(const Vector3& p)    const override;
  Vector3 gradientAt(const Vector3& p) const override;
  BBox    bounds() const override;
  bool    isInside(const Vector3& p)   const override;
  bool    edgeHit(const Vector3& a, const Vector3& b,
                  Vector3& outP, Vector3& outN) const override;
  bool    cellOverlaps(const BBox& cell) const override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Build a WindingNumberField over `mesh` and return it as a FieldPtr.
FieldPtr windingNumberField(
    geometrycentral::surface::SurfaceMesh& mesh,
    geometrycentral::surface::VertexPositionGeometry& geometry);

// An ImplicitField backed by a dense 3D signed-distance grid. `valueAt` is a
// trilinear interpolation (O(1), a few dozen flops); `gradientAt` is the
// analytic gradient of that interpolant. A baked SDF stays ~Lipschitz-1, so
// the default cellOverlaps / edgeHit behave well on it.
//
// This exists so an *expensive* field -- notably a MeshSource, or a whole
// tree of them -- can be sampled once into a grid and then queried millions
// of times for free. Build one with bakeToGrid(). Queries are const and
// thread-safe.
class GridField : public ImplicitField {
public:
  // `samples` holds resolution.x*resolution.y*resolution.z signed-distance
  // values, x-fastest then y then z, on a lattice spanning `region`'s corners
  // inclusively. Every axis resolution must be >= 2.
  GridField(const BBox& region, const Vector3i& resolution,
            std::vector<float> samples);
  ~GridField() override;

  GridField(const GridField&)            = delete;
  GridField& operator=(const GridField&) = delete;

  double  valueAt(const Vector3& p)    const override;
  Vector3 gradientAt(const Vector3& p) const override;
  BBox    bounds() const override;

  // Read-only access to the baked grid, for handing it to a GPU 3D texture or
  // a voxel-file writer. `values()` is x-fastest then y then z (size =
  // resolution().x*y*z); the grid spans bounds() corner-inclusively at
  // resolution() samples per axis. This is numeric grid data, not mesh I/O,
  // so it stays in the library (a host can upload/export without re-baking).
  const std::vector<float>& values()     const;
  Vector3i                  resolution() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Sample `src` once into a dense SDF grid covering `region` and return it as
// a GridField. The grid has `resolution` samples along each axis (>= 2). Bake
// cost is resolution.x*y*z `src.valueAt` calls, paid once and parallelised
// across hardware threads; every later query is an O(1) trilinear lookup.
//
// Memory: 4 * rx * ry * rz bytes (float storage) -- e.g. 129^3 ~= 8 MB.
FieldPtr bakeToGrid(const ImplicitField& src, const BBox& region,
                    const Vector3i& resolution);

// Cubic-resolution convenience overload (`resolution` samples on every axis).
FieldPtr bakeToGrid(const ImplicitField& src, const BBox& region,
                    int resolution);

// --- Boolean combinators -------------------------------------------------
//
// Hard combinators give the compound field a C0 ridge along the seam; the
// QEF lands the cell vertex on it (sharp). Smooth combinators give a C-inf
// field; the surface is reconstructed smoothly. Same pipeline either way.

FieldPtr unionOf       (FieldPtr a, FieldPtr b);  // min(a, b)
FieldPtr intersectionOf(FieldPtr a, FieldPtr b);  // max(a, b)
FieldPtr differenceOf  (FieldPtr a, FieldPtr b);  // max(a, -b)  == a \ b
FieldPtr xorOf         (FieldPtr a, FieldPtr b);  // symmetric difference

// `k` sets the blend radius (world units). Larger k -> rounder fillet.
FieldPtr smoothUnionOf       (FieldPtr a, FieldPtr b, double k);
FieldPtr smoothIntersectionOf(FieldPtr a, FieldPtr b, double k);
FieldPtr smoothDifferenceOf  (FieldPtr a, FieldPtr b, double k);  // a \ b

// Position-driven linear blend (morph): value = lerp(a, b, w) with
// w = clamp((control - lo)/(hi - lo), 0, 1). Where control <= lo the field is
// a; where control >= hi it is b; in between it lerps. `control` is any field
// (e.g. a plane for a linear gradient along an axis, sphere(radius=0) for
// distance from a point, or a mesh for distance from a surface) -- the same
// control convention as gradedOnionOf. Use it to morph one solid smoothly into
// another across the part (e.g. bcc -> fcc struts). NOTE: a lerp of two SDFs is
// not itself a distance field, but it is a valid implicit for dual contouring.
// IMPORTANT -- mix blends VALUES, not shapes: a lerp of two SDFs is a continuous
// body only where the two solids OVERLAP. Same lattice family (mix(bcc(r1),
// bcc(r2),...)) => struts coincide => a graded radius on one connected body (fluid).
// Two DIFFERENT crystals (bcc<->fcc) => struts sit at different positions, so mid-
// band 0.5*(A+B) is empty where one strut is far from the other => struts taper to
// nothing at the transition plane and the output SPLITS INTO TWO SEPARATE BODIES
// with a gap (inherent, not a bug). To join two different crystals into one
// continuous body, GRAFT them (clip each to overlapping regions and union/
// smoothUnion so their struts cross and fuse) -- do not use mixOf.
FieldPtr mixOf(FieldPtr a, FieldPtr b, FieldPtr control, double lo, double hi);

// --- Decorators ----------------------------------------------------------

// Offset the surface outward by `r` (negative shrinks inward).
FieldPtr offsetOf(FieldPtr f, double r);

// Round/inflate by `r`. Semantic alias of offsetOf with r > 0.
FieldPtr roundedOf(FieldPtr f, double r);

// Hollow shell of the given wall thickness, centred on the surface.
FieldPtr onionOf(FieldPtr f, double thickness);

// Hollow shell whose wall thickness varies with a control field. t ramps from
// `t1` (where control <= d0) to `t2` (where control >= d1), clamped-linear in
// between; the two plateaus extend to +/-infinity. `control` is any field --
// e.g. sphere(radius=0) for distance from a point, a plane for a linear
// gradient along an axis, or a mesh for distance from a surface. To put the
// falloff on the other side, swap t1/t2 (or flip the control). Keep
// min(t1,t2) > 0 to avoid a zero-thickness (non-manifold) pinch.
FieldPtr gradedOnionOf(FieldPtr base, FieldPtr control, double t1, double t2,
                       double d0, double d1);

// Inflate a SOLID field outward by a control-driven amount: value is
// `base - t(p)`, so positive `t` grows the solid (thickens it locally) rather
// than hollowing it like gradedOnionOf's `|base| - t`. `t` ramps from `t1`
// (control <= d0) to `t2` (control >= d1), clamped-linear in between, with the
// same control-field convention as gradedOnionOf (e.g. sphere(radius=0) for
// distance from a point). Typical use: a graded strut-radius lattice, dense
// near a load path (small control -> small t) and sparse elsewhere. Composes
// with any solid field, not just struts.
FieldPtr gradedOffsetOf(FieldPtr base, FieldPtr control, double t1, double t2,
                        double d0, double d1);

// Stretch the field by inserting a slab of half-extent `h` along each axis.
FieldPtr elongated(FieldPtr f, const Vector3& h);

// Rigid placement: `worldFromLocal` maps the field's local frame to world.
FieldPtr transformed(FieldPtr f, const Mat4& worldFromLocal);

// Uniform scale by `s` about the origin (s > 0).
FieldPtr scaled(FieldPtr f, double s);

// First-order gradient normalisation: returns f / max(|grad f|, eps). Useful
// for non-SDF fields (notably TPMS) whose value magnitudes are not metric
// distances -- wrapping in normalizedOf() makes a subsequent onionOf(., t)
// produce a wall whose thickness is metrically close to t. Not a true SDF
// (only first-order accurate at the surface), but uniform enough that
// metric-thickness offsets behave intuitively.
FieldPtr normalizedOf(FieldPtr f);

// --- Domain operators ----------------------------------------------------
//
// These warp the sampling domain before evaluating the child. `mirrored` and
// `repeated`/`repeatedLimited` are piecewise isometries -- their tile/mirror
// seams stay sharp (the gradient is taken from the active copy, never an
// average). `twisted`/`bent`/`displaced` are non-Euclidean ("distorted")
// fields: their gradient is a finite difference of the warped field.

// Mirror the field across the plane through the origin with the given normal.
// The half-space the normal points into is kept and reflected onto the other.
FieldPtr mirrored(FieldPtr f, const Vector3& planeNormal);

// Tile the field infinitely with the given per-axis period (a zero component
// disables repetition on that axis). bounds() is infinite -- sampling needs
// an explicit SamplerParams::rootBounds.
FieldPtr repeated(FieldPtr f, const Vector3& period);

// Tile the field a finite number of times: `count` copies per axis, the
// un-warped field being tile (0,0,0).
FieldPtr repeatedLimited(FieldPtr f, const Vector3& period,
                         const Vector3i& count);

// Twist about `axis` (0 = x, 1 = y, 2 = z): the plane perpendicular to the
// axis is rotated by `radiansPerUnit` per unit of distance along the axis.
FieldPtr twisted(FieldPtr f, double radiansPerUnit, int axis);

// Bend driven by `axis` (0 = x, 1 = y, 2 = z): the (axis, next-axis) plane is
// rotated by `curvature` radians per unit of distance along the axis.
FieldPtr bent(FieldPtr f, double curvature, int axis);

// Add a scalar displacement `bump(p)` to the field value. NOTE: bounds() is
// the child's -- if the bump pushes the surface outward, set rootBounds.
FieldPtr displaced(FieldPtr f, std::function<double(const Vector3&)> bump);

} // namespace dualc
