// The porous field (roadmap 23 #65): the seeds (#63) of a volume, their
// Delaunay complex packed with its bins (#64), and the unsigned distance to it
// as an ImplicitField. The volume and the control are read here, at
// construction, and never again; the defaults are the table in
// docs/roadmap/23-porous-shell/01-study.md.
//
// Why the default cellOverlaps is right (the field is 1-Lipschitz):
//  - Each primitive's distance -- a bisector plane, a tet's face plane, a
//    segment -- is 1-Lipschitz (the packed float planes are unit to 6e-8).
//  - The smooth fold is the minimum at k = 0; at k > 0 each step smin(acc, d)
//    has the gradient (1 - h/2) grad acc + (h/2) grad d, a convex combination
//    of unit-bounded gradients, and the fold reads only the primitives within
//    k of the minimum in ascending order (porousSmoothFold), so it is
//    continuous in them. Clamping at 0 (and at a bound) keeps the constant.
//  - The walls are taken from the cell (the nearest seed / the containing tet)
//    p lies in. Two cells meet on a wall, where both sides' values are 0
//    (the clamp at 0 makes it so at k > 0 too), so the value is continuous
//    across the partition, and piecewise 1-Lipschitz plus continuous along any
//    segment is 1-Lipschitz. Outside the seeds' hull the Delaunay walls are 0,
//    continuous with the hull faces.
//  - The struts search the segments within B. The set changes only when a
//    segment crosses d = B, and including it moves the fold only where the
//    accumulator is within k of B: smin(acc, B) > B - k for acc > B - k. So the
//    field clamps the struts at B - k (B at k = 0), where that jump cannot
//    show; porousDistance's own clamp at B would let the value step by up to
//    k/4 there. Below B - k the value is the exact smoothed distance.
// The default test -- corner signs, then |value(centre)| <= the half diagonal
// -- is exact for a 1-Lipschitz field, so no override is needed, and every
// wrapper above (onion, offset, graded-*) forwards to it as it does for any
// metric source.

#include "dualc/porous.h"

#include "internal/porous_complex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace dualc {

struct PorousField::Impl {
  PorousParams params;
  BBox box;
  internal::PorousComplex complex;
  double smooth = 0.0;
  // The value's ceiling: struts B - smooth; walls +infinity (no clamp).
  double ceiling = 0.0;
  Stats stats;
};

namespace {

[[noreturn]] void fail(const std::string& what) {
  throw std::invalid_argument("porous: " + what);
}

std::string num(double v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.6g", v);
  return buf;
}

} // namespace

PorousField::PorousField(const ImplicitField& volume, const ImplicitField* control,
                         const PorousParams& params, const CancelToken* cancel) {
  if (!(params.smooth >= 0.0) || !std::isfinite(params.smooth))
    fail("smooth must be finite and >= 0 (got " + num(params.smooth) + ")");
  auto impl = std::make_unique<Impl>();
  impl->params = params;
  impl->smooth = params.smooth;
  impl->box = volume.bounds();

  const SeedSet seeds = generatePorousSeeds(volume, control, params.seeds, cancel);
  impl->complex = internal::buildPorousComplex(seeds, params.cells, params.mode, cancel);

  const internal::PorousComplex& c = impl->complex;
  impl->ceiling = std::numeric_limits<double>::infinity();
  if (params.mode == PorousMode::Struts) {
    const double B = double(c.bound);
    if (!(params.smooth < B))
      fail("smooth (" + num(params.smooth) + ") must stay below the struts' bound B = " +
           num(B) + " (about 1.2 x the larger spacing): the field would be 0 everywhere");
    impl->ceiling = B - params.smooth;
  }

  Stats& st = impl->stats;
  st.seeds = seeds.points.size();
  st.padSeeds = std::size_t(std::count(seeds.isPad.begin(), seeds.isPad.end(), 1));
  st.tets = c.stats.tets;
  st.delaunayFaces = c.stats.delaunayFaces;
  st.delaunayEdges = c.stats.delaunayEdges;
  st.segments = c.stats.segments;
  st.bins = c.bins.count();
  st.packedBytes = c.packedBytes();
  st.bound = double(c.bound);
  impl_ = std::move(impl);
}

PorousField::~PorousField() = default;

double PorousField::valueAt(const Vector3& p) const {
  const double v = internal::porousDistance(impl_->complex, p, impl_->smooth);
  return std::min(v, impl_->ceiling);
}

Vector3 PorousField::trueGradientAt(const Vector3& p) const {
  Vector3 g{0.0, 0.0, 0.0};
  const double v = internal::porousDistanceGradient(impl_->complex, p, impl_->smooth, &g);
  if (!(v < impl_->ceiling)) return Vector3{0.0, 0.0, 0.0};
  return g;
}

Vector3 PorousField::gradientAt(const Vector3& p) const {
  const Vector3 g = trueGradientAt(p);
  const double n = g.norm();
  return n > 1e-12 ? g * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
}

BBox PorousField::bounds() const { return impl_->box; }

const PorousField::Stats& PorousField::stats() const { return impl_->stats; }

const PorousParams& PorousField::params() const { return impl_->params; }

} // namespace dualc
