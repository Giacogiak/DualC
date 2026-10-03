#pragma once

#include "geometrycentral/utilities/vector3.h"

#include <cmath>
#include <cstddef>

namespace dualc {

using geometrycentral::Vector3;

// Integer 3-vector. Used for per-axis tile counts in finite repetition.
struct Vector3i {
  int x = 0;
  int y = 0;
  int z = 0;
};

enum class SignMethod {
  PSEUDONORMAL,    // Bærentzen-Aanæs angle-weighted pseudonormal sign.
                   // Assumes watertight, oriented input.
  WINDING_NUMBER,  // 3-ray majority parity vote. Correct for closed,
                   // oriented meshes; robust against single-ray grazing.
                   // NOT a winding number despite the name -- kept for
                   // backwards compatibility.
  GENERALIZED_WINDING_NUMBER
                   // Jacobson generalized winding number, hierarchically
                   // accelerated. Tolerates non-watertight input: open
                   // shells, triangle soup, self-intersections.
};

struct BBox {
  Vector3 min{0.0, 0.0, 0.0};
  Vector3 max{0.0, 0.0, 0.0};

  static BBox unit();        // [-0.5, 0.5]^3
  Vector3 center() const;
  Vector3 extent() const;    // max - min
  bool isValid() const;      // every component of max >= min and finite

  // The "no box at all" sentinel: an inverted box, which isValid() rejects
  // (and isInfinite() does not claim). Use this, never a default-constructed
  // BBox{}, to say a field has no extent -- `BBox{}` is the degenerate box at
  // the origin, and it is VALID, so an emptiness guard written as
  // `!b.isValid()` never fires for it and the origin is silently dragged into
  // whatever it is unioned with.
  static BBox empty();

  // Sentinel for unbounded fields (a plane, an infinite cylinder/cone, an
  // infinite repetition). A field reporting this from bounds() cannot be
  // auto-fitted: the sampler requires an explicit SamplerParams::rootBounds.
  static BBox infinite();    // [-inf, +inf]^3
  bool isInfinite() const;   // true on the infinite() sentinel
};

// Row-major 4x4 affine transform; the bottom row is assumed (0, 0, 0, 1).
// The builders produce rigid transforms (rotation + translation), for which
// inverseRigid() is exact. Used by the `transformed` field decorator.
struct Mat4 {
  double m[4][4];

  static Mat4 identity() {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) r.m[i][j] = (i == j) ? 1.0 : 0.0;
    return r;
  }

  static Mat4 translation(const Vector3& t) {
    Mat4 r = identity();
    r.m[0][3] = t.x;
    r.m[1][3] = t.y;
    r.m[2][3] = t.z;
    return r;
  }

  // Rotation by `angleRad` about `axis` (need not be unit).
  static Mat4 rotation(const Vector3& axis, double angleRad) {
    Mat4 r = identity();
    const double n = std::sqrt(axis.x * axis.x + axis.y * axis.y +
                               axis.z * axis.z);
    if (n <= 0.0) return r;
    const double x = axis.x / n, y = axis.y / n, z = axis.z / n;
    const double c = std::cos(angleRad), s = std::sin(angleRad), t = 1.0 - c;
    r.m[0][0] = t*x*x + c;   r.m[0][1] = t*x*y - s*z; r.m[0][2] = t*x*z + s*y;
    r.m[1][0] = t*x*y + s*z; r.m[1][1] = t*y*y + c;   r.m[1][2] = t*y*z - s*x;
    r.m[2][0] = t*x*z - s*y; r.m[2][1] = t*y*z + s*x; r.m[2][2] = t*z*z + c;
    return r;
  }

  // Matrix product: (a * b) applied to a point equals a(b(point)).
  Mat4 operator*(const Mat4& o) const {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) {
        double s = 0.0;
        for (int k = 0; k < 4; ++k) s += m[i][k] * o.m[k][j];
        r.m[i][j] = s;
      }
    return r;
  }

  Vector3 transformPoint(const Vector3& p) const {
    return Vector3{
        m[0][0]*p.x + m[0][1]*p.y + m[0][2]*p.z + m[0][3],
        m[1][0]*p.x + m[1][1]*p.y + m[1][2]*p.z + m[1][3],
        m[2][0]*p.x + m[2][1]*p.y + m[2][2]*p.z + m[2][3]};
  }

  // Applies the linear part only (no translation).
  Vector3 transformDirection(const Vector3& d) const {
    return Vector3{
        m[0][0]*d.x + m[0][1]*d.y + m[0][2]*d.z,
        m[1][0]*d.x + m[1][1]*d.y + m[1][2]*d.z,
        m[2][0]*d.x + m[2][1]*d.y + m[2][2]*d.z};
  }

  // Inverse for a rigid transform [R | t]: [R^T | -R^T t]. Only correct when
  // the linear part is a rotation -- true for anything the builders produce.
  Mat4 inverseRigid() const {
    Mat4 r = identity();
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j) r.m[i][j] = m[j][i];  // R^T
    const double tx = m[0][3], ty = m[1][3], tz = m[2][3];
    r.m[0][3] = -(r.m[0][0]*tx + r.m[0][1]*ty + r.m[0][2]*tz);
    r.m[1][3] = -(r.m[1][0]*tx + r.m[1][1]*ty + r.m[1][2]*tz);
    r.m[2][3] = -(r.m[2][0]*tx + r.m[2][1]*ty + r.m[2][2]*tz);
    return r;
  }
};

// Optional out-parameter reporting what the pipeline would otherwise degrade
// silently. Every driver -- the samplers, the contourer and both
// `dualContour*` one-call wrappers -- takes a trailing
// `Diagnostics* diag = nullptr`; pass one to learn that the answer you got
// was produced under a fallback rather than from the input you meant.
//
// A default-constructed Diagnostics reads "nothing to report", and
// `anyIssue()` is false. Each driver OVERWRITES exactly the fields its own
// stage owns and leaves the rest untouched, so a single instance can be
// threaded through a hand-rolled sample -> simplify -> contour sequence and
// end up carrying the whole picture. The one-call wrappers fill every field.
//
// Thread-safety: every field is written by the calling thread, outside the
// parallel region -- never from a worker. Passing a Diagnostics therefore
// cannot perturb the bit-identical-across-thread-counts guarantee that
// SamplerParams::numThreads documents.
struct Diagnostics {
  // --- Input mesh. Set only by the mesh entry points (`dualContourMesh` /
  // `sampleMeshToHermiteOctree`), which build the MeshSource themselves. A
  // field graph that happens to contain a MeshSource is not introspected. ---

  // The input mesh had no triangles. Every sign query then answers "outside"
  // and the contour is empty -- a correct answer to a degenerate question.
  bool inputEmpty = false;
  // Edges of the input incident to exactly one triangle (an open boundary)
  // and to three or more (a non-manifold junction). Both make the
  // PSEUDONORMAL pseudonormal tables fall back to the raw face normal there,
  // which is only correct on watertight oriented input; prefer
  // SignMethod::GENERALIZED_WINDING_NUMBER when either is non-zero.
  std::size_t inputBoundaryEdges    = 0;
  std::size_t inputNonManifoldEdges = 0;
  // inputBoundaryEdges == 0 && inputNonManifoldEdges == 0.
  bool inputWatertight = true;

  // --- Sampling. Set by both samplers. ---

  // `field.bounds()` was neither valid nor infinite, so the root box fell
  // back to BBox::unit() -- the sampled region is almost certainly not the
  // one you wanted. Set SamplerParams::rootBounds explicitly.
  bool boundsFallback = false;
  // The sampled field is a GridField (the product of bakeToGrid) and the
  // root box reaches outside its baked region, so probes out there read the
  // clamped nearest-face value rather than a real sample
  // (src/implicit/grid_field.cpp maps and clamps every coordinate).
  //
  // Reported only for an EXPLICIT SamplerParams::rootBounds, and only when
  // the GridField is the ROOT of the sampled graph. The auto-fit path pads
  // the field's own bounds outward by padFraction and so always lands just
  // outside the bake -- flagging that would fire on every default contour of
  // a grid while saying nothing about intent, so this flag means strictly
  // "you asked for a region the bake does not cover". A GridField nested
  // under a combinator or a domain op is not found either, and the per-probe
  // clamp is NOT counted: a counter on the field would mean mutable state
  // written from every worker thread, which contradicts the ImplicitField
  // thread-safety contract and the bit-identical-across-thread-counts
  // guarantee.
  //
  // Note this is deliberately NOT a general "root box exceeds field.bounds()"
  // test. For every other field bounds() is where the surface is, not where
  // the field is defined, so sampling past it is the normal case -- a flag
  // that fired there would fire on almost every run.
  bool gridBoundsExceeded = false;

  // --- Contouring. Set by the contourer. ---

  // No surface was found, so the returned mesh is a single placeholder
  // triangle rather than an empty mesh (geometry-central rejects an empty
  // polygon list). Without this flag "the field had no surface here" is
  // indistinguishable from "there was one triangle".
  bool emptyContour = false;
  std::size_t outputVertices  = 0;
  std::size_t outputTriangles = 0;
  // Edges of the OUTPUT mesh incident to exactly one triangle / to three or
  // more. Both are zero on a closed manifold mesh, which is what dual
  // contouring is supposed to produce.
  std::size_t outputBoundaryEdges    = 0;
  std::size_t outputNonManifoldEdges = 0;
  // outputBoundaryEdges == 0 && outputNonManifoldEdges == 0.
  bool outputWatertight = true;

  // True if any degradation above was reported. Note that a non-watertight
  // OUTPUT counts, but a non-watertight INPUT on its own does not -- open
  // input is a legitimate case for GENERALIZED_WINDING_NUMBER, and the edge
  // counts are there to be read rather than to raise an alarm.
  bool anyIssue() const {
    return inputEmpty || boundsFallback || gridBoundsExceeded ||
           emptyContour || !outputWatertight;
  }
};

} // namespace dualc
