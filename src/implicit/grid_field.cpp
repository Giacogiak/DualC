#include "dualc/implicit.h"

#include "internal/parallel.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace dualc {

// ===========================================================================
// GridField -- a dense trilinear signed-distance grid.
// ===========================================================================

struct GridField::Impl {
  BBox               region;
  Vector3i           res;     // samples per axis (>= 2)
  std::vector<float> v;       // res.x*res.y*res.z, x-fastest then y then z
  Vector3            cell;    // lattice spacing per axis = extent / (res - 1)
  Vector3            inv;     // 1 / cell (0 on a degenerate axis)

  std::size_t index(int ix, int iy, int iz) const {
    return static_cast<std::size_t>(ix) +
           static_cast<std::size_t>(res.x) *
               (static_cast<std::size_t>(iy) +
                static_cast<std::size_t>(res.y) *
                    static_cast<std::size_t>(iz));
  }
};

namespace {

// Map a world coordinate to a grid coordinate, clamped to [0, res-1] so that
// probes outside the baked region read the nearest face value rather than
// extrapolating. Splits the result into a base cell index `i` (in
// [0, res-2]) and an in-cell fraction `f` (in [0, 1]).
void locate(double world, double regionMin, double inv, int res,
            int& i, double& f) {
  double g = (world - regionMin) * inv;
  g = std::clamp(g, 0.0, static_cast<double>(res - 1));
  i = std::min(static_cast<int>(g), res - 2);
  if (i < 0) i = 0;
  f = std::clamp(g - static_cast<double>(i), 0.0, 1.0);
}

} // namespace

GridField::GridField(const BBox& region, const Vector3i& resolution,
                     std::vector<float> samples)
    : impl_(std::make_unique<Impl>()) {
  if (resolution.x < 2 || resolution.y < 2 || resolution.z < 2) {
    throw std::invalid_argument(
        "GridField: resolution must be >= 2 on every axis");
  }
  const std::size_t expected = static_cast<std::size_t>(resolution.x) *
                               static_cast<std::size_t>(resolution.y) *
                               static_cast<std::size_t>(resolution.z);
  if (samples.size() != expected) {
    throw std::invalid_argument(
        "GridField: samples.size() != resolution.x*y*z");
  }

  impl_->region = region;
  impl_->res    = resolution;
  impl_->v      = std::move(samples);

  const Vector3 ext = region.extent();
  impl_->cell = Vector3{ext.x / (resolution.x - 1),
                        ext.y / (resolution.y - 1),
                        ext.z / (resolution.z - 1)};
  impl_->inv = Vector3{impl_->cell.x > 0.0 ? 1.0 / impl_->cell.x : 0.0,
                       impl_->cell.y > 0.0 ? 1.0 / impl_->cell.y : 0.0,
                       impl_->cell.z > 0.0 ? 1.0 / impl_->cell.z : 0.0};
}

GridField::~GridField() = default;

const std::vector<float>& GridField::values() const { return impl_->v; }
Vector3i GridField::resolution() const { return impl_->res; }

double GridField::valueAt(const Vector3& p) const {
  const Impl& m = *impl_;
  int ix, iy, iz;
  double fx, fy, fz;
  locate(p.x, m.region.min.x, m.inv.x, m.res.x, ix, fx);
  locate(p.y, m.region.min.y, m.inv.y, m.res.y, iy, fy);
  locate(p.z, m.region.min.z, m.inv.z, m.res.z, iz, fz);

  const double c000 = m.v[m.index(ix,   iy,   iz  )];
  const double c100 = m.v[m.index(ix+1, iy,   iz  )];
  const double c010 = m.v[m.index(ix,   iy+1, iz  )];
  const double c110 = m.v[m.index(ix+1, iy+1, iz  )];
  const double c001 = m.v[m.index(ix,   iy,   iz+1)];
  const double c101 = m.v[m.index(ix+1, iy,   iz+1)];
  const double c011 = m.v[m.index(ix,   iy+1, iz+1)];
  const double c111 = m.v[m.index(ix+1, iy+1, iz+1)];

  const double c00 = c000 + (c100 - c000) * fx;
  const double c10 = c010 + (c110 - c010) * fx;
  const double c01 = c001 + (c101 - c001) * fx;
  const double c11 = c011 + (c111 - c011) * fx;
  const double c0  = c00 + (c10 - c00) * fy;
  const double c1  = c01 + (c11 - c01) * fy;
  return c0 + (c1 - c0) * fz;
}

Vector3 GridField::trueGradientAt(const Vector3& p) const {
  const Impl& m = *impl_;
  int ix, iy, iz;
  double fx, fy, fz;
  locate(p.x, m.region.min.x, m.inv.x, m.res.x, ix, fx);
  locate(p.y, m.region.min.y, m.inv.y, m.res.y, iy, fy);
  locate(p.z, m.region.min.z, m.inv.z, m.res.z, iz, fz);

  const double c000 = m.v[m.index(ix,   iy,   iz  )];
  const double c100 = m.v[m.index(ix+1, iy,   iz  )];
  const double c010 = m.v[m.index(ix,   iy+1, iz  )];
  const double c110 = m.v[m.index(ix+1, iy+1, iz  )];
  const double c001 = m.v[m.index(ix,   iy,   iz+1)];
  const double c101 = m.v[m.index(ix+1, iy,   iz+1)];
  const double c011 = m.v[m.index(ix,   iy+1, iz+1)];
  const double c111 = m.v[m.index(ix+1, iy+1, iz+1)];

  // Analytic gradient of the trilinear interpolant within the cell. The
  // partial w.r.t. the in-cell fraction is itself a bilinear blend of the
  // 4 corner differences; multiply by 1/cell to get the world-space rate.
  auto bilerp = [](double a, double b, double c, double d,
                   double u, double w) {
    const double ab = a + (b - a) * u;
    const double cd = c + (d - c) * u;
    return ab + (cd - ab) * w;
  };

  const double dfx = bilerp(c100 - c000, c110 - c010,
                            c101 - c001, c111 - c011, fy, fz);
  const double dfy = bilerp(c010 - c000, c110 - c100,
                            c011 - c001, c111 - c101, fx, fz);
  const double dfz = bilerp(c001 - c000, c101 - c100,
                            c011 - c010, c111 - c110, fx, fy);

  return Vector3{dfx * m.inv.x, dfy * m.inv.y, dfz * m.inv.z};
}

Vector3 GridField::gradientAt(const Vector3& p) const {
  // The trilinear blend of per-node gradients, normalised. The interpolant's
  // own gradient (trueGradientAt) is only C0 inside a cell and jumps across
  // its faces, so its direction did too (#34); a node's central difference is
  // shared by every cell touching it, so the blend is continuous everywhere.
  // Computed on the fly from the samples (48 loads), not stored: 17/20.
  const Impl& m = *impl_;
  int ix, iy, iz;
  double fx, fy, fz;
  locate(p.x, m.region.min.x, m.inv.x, m.res.x, ix, fx);
  locate(p.y, m.region.min.y, m.inv.y, m.res.y, iy, fy);
  locate(p.z, m.region.min.z, m.inv.z, m.res.z, iz, fz);

  // d/d(axis) at a node: central over its two neighbours, one-sided at the
  // grid boundary (res >= 2, so a neighbour always exists).
  auto diff = [&](int i, int res, double inv, auto at) {
    if (i == 0)       return (at(1) - at(0)) * inv;
    if (i == res - 1) return (at(res - 1) - at(res - 2)) * inv;
    return (at(i + 1) - at(i - 1)) * (0.5 * inv);
  };
  auto nodeGradient = [&](int jx, int jy, int jz) {
    return Vector3{
        diff(jx, m.res.x, m.inv.x,
             [&](int k) { return double(m.v[m.index(k, jy, jz)]); }),
        diff(jy, m.res.y, m.inv.y,
             [&](int k) { return double(m.v[m.index(jx, k, jz)]); }),
        diff(jz, m.res.z, m.inv.z,
             [&](int k) { return double(m.v[m.index(jx, jy, k)]); })};
  };

  const Vector3 g00 = nodeGradient(ix, iy,     iz    ) * (1.0 - fx) +
                      nodeGradient(ix + 1, iy,     iz    ) * fx;
  const Vector3 g10 = nodeGradient(ix, iy + 1, iz    ) * (1.0 - fx) +
                      nodeGradient(ix + 1, iy + 1, iz    ) * fx;
  const Vector3 g01 = nodeGradient(ix, iy,     iz + 1) * (1.0 - fx) +
                      nodeGradient(ix + 1, iy,     iz + 1) * fx;
  const Vector3 g11 = nodeGradient(ix, iy + 1, iz + 1) * (1.0 - fx) +
                      nodeGradient(ix + 1, iy + 1, iz + 1) * fx;
  const Vector3 g0 = g00 * (1.0 - fy) + g10 * fy;
  const Vector3 g1 = g01 * (1.0 - fy) + g11 * fy;
  const Vector3 g  = g0 * (1.0 - fz) + g1 * fz;

  const double n = g.norm();
  if (n > 1e-12) return g / n;
  // The node gradients cancel (a feature thinner than two cells): fall back
  // to the cell's own gradient direction, then the contract's (0,0,1).
  const Vector3 t = trueGradientAt(p);
  const double tn = t.norm();
  return (tn > 1e-12) ? (t / tn) : Vector3{0.0, 0.0, 1.0};
}

BBox GridField::bounds() const { return impl_->region; }

// ===========================================================================
// bakeToGrid
// ===========================================================================

FieldPtr bakeToGrid(const ImplicitField& src, const BBox& region,
                    const Vector3i& resolution, unsigned numThreads) {
  if (resolution.x < 2 || resolution.y < 2 || resolution.z < 2) {
    throw std::invalid_argument(
        "bakeToGrid: resolution must be >= 2 on every axis");
  }

  const int rx = resolution.x, ry = resolution.y, rz = resolution.z;
  const Vector3 ext = region.extent();
  const Vector3 cell{ext.x / (rx - 1), ext.y / (ry - 1), ext.z / (rz - 1)};

  std::vector<float> samples(static_cast<std::size_t>(rx) *
                             static_cast<std::size_t>(ry) *
                             static_cast<std::size_t>(rz));

  // Sample the field on the lattice. Each z-slab is independent, so the bake
  // -- the only place the expensive source field is touched -- parallelises
  // trivially across `numThreads` workers.
  internal::parallelFor(
      static_cast<std::size_t>(rz), numThreads,
      [&](std::size_t izu) {
        const int iz = static_cast<int>(izu);
        const double z = region.min.z + cell.z * iz;
        std::size_t off = static_cast<std::size_t>(rx) *
                          static_cast<std::size_t>(ry) * izu;
        for (int iy = 0; iy < ry; ++iy) {
          const double y = region.min.y + cell.y * iy;
          for (int ix = 0; ix < rx; ++ix) {
            const double x = region.min.x + cell.x * ix;
            samples[off++] =
                static_cast<float>(src.valueAt(Vector3{x, y, z}));
          }
        }
      });

  return std::make_shared<GridField>(region, resolution, std::move(samples));
}

FieldPtr bakeToGrid(const ImplicitField& src, const BBox& region,
                    int resolution, unsigned numThreads) {
  return bakeToGrid(src, region, Vector3i{resolution, resolution, resolution},
                    numThreads);
}

} // namespace dualc
