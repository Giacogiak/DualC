// Seeds for the porous shell (roadmap 23 #63): PCG32, Bridson Poisson-disk
// with a variable radius, a jittered grid, white noise, the pad outside the
// volume, and Lloyd relaxation by sampling. Every number below comes from
// the defaults table in docs/roadmap/23-porous-shell/01-study.md.
//
// Bit-identical on every OS: the only randomness is Pcg32, the only
// transcendental is detExp (basic arithmetic), and the file is compiled
// without FMA contraction (CMakeLists.txt). The volume's and the control's
// own valueAt are the caller's.

#include "dualc/porous.h"

#include "internal/parallel.h"
#include "internal/seed_grid.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace dualc {

// ---------------------------------------------------------------- Pcg32 --

Pcg32::Pcg32(std::uint64_t initState, std::uint64_t initSeq) {
  state_ = 0;
  inc_ = (initSeq << 1u) | 1u;
  next();
  state_ += initState;
  next();
}

std::uint32_t Pcg32::next() {
  const std::uint64_t old = state_;
  state_ = old * 6364136223846793005ULL + inc_;
  const std::uint32_t xorshifted =
      static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
  const std::uint32_t rot = static_cast<std::uint32_t>(old >> 59u);
  return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}

double Pcg32::nextDouble() {
  const std::uint64_t a = next() >> 5;  // 27 bits
  const std::uint64_t b = next() >> 6;  // 26 bits
  return static_cast<double>((a << 26) + b) * (1.0 / 9007199254740992.0);
}

namespace {

// e^x by range reduction x = k ln2 + r, |r| <= ln2 / 2, and a degree-13
// Taylor polynomial (truncation < 3e-18 relative): basic arithmetic only, so
// the same bits everywhere, unlike libm's exp. Used on x in [-ln 100, 0].
double detExp(double x) {
  const double kLn2Hi = 6.93147180369123816490e-01;
  const double kLn2Lo = 1.90821492927058770002e-10;
  const double k = std::floor(x * 1.44269504088896338700 + 0.5);
  const double r = (x - k * kLn2Hi) - k * kLn2Lo;
  double p = 1.0;
  for (int i = 13; i >= 1; --i) p = 1.0 + p * r / i;
  return std::ldexp(p, static_cast<int>(k));
}

// ln 100: the gaussian ramp's exponent scale, 1 / (2 sigma^2) with
// sigma = 1 / sqrt(2 ln 100).
constexpr double kLn100 = 4.60517018598809136804;

// Bridson's tries around one active seed.
constexpr int kBridsonTries = 30;
// Tries per scan cell when the active list is empty (a new component, or a
// hole Bridson's walk did not reach).
constexpr int kScanTries = 4;
// Lloyd: uniform samples per seed per iteration, and the chunk the samples
// are drawn, evaluated (in parallel) and accumulated (in order) by.
constexpr int kLloydSamplesPerSeed = 32;
constexpr std::size_t kLloydChunk = 65536;
constexpr std::size_t kLloydBlock = 1024;
// A centroidal Voronoi tessellation of density rho has cells of size
// rho^(-1/(d+2)) (Du, Faber, Gunzburger 1999), so in 3D the sample weight
// (spacing / spacing(x))^5 keeps a cell size proportional to spacing(x).
constexpr int kLloydDensityExponent = 5;
// The cancel token is polled every this many candidates.
constexpr std::size_t kPollEvery = 4096;
// A volume's box past this many cells at the smallest spacing is refused
// before anything is allocated: even a volume filling 1/32 of its box would
// pass the seed ceiling.
constexpr double kMaxBoxCells = 32.0 * static_cast<double>(kPorousMaxSeeds);

[[noreturn]] void fail(const std::string& what) {
  throw std::invalid_argument("porous seeds: " + what);
}

std::string num(double v) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%g", v);
  return buf;
}

struct Spacing {
  const ImplicitField* control = nullptr;
  double s1 = 0.0, s2 = 0.0, d0 = 0.0, d1 = 0.0;
  DensityRamp ramp = DensityRamp::Linear;
  bool graded = false;

  double at(const Vector3& p) const {
    if (!graded) return s1;
    const double u = (control->valueAt(p) - d0) / (d1 - d0);
    return s1 + (s2 - s1) * densityRampWeight(ramp, u);
  }
};

class Builder {
public:
  Builder(const ImplicitField& volume, const Spacing& spacing,
          const SeedParams& params, const CancelToken* cancel, SeedSet& out)
      : volume_(volume), spacing_(spacing), params_(params), cancel_(cancel),
        out_(out), rng_(params.seed, 0) {}

  void poisson();
  void jitter();
  void random();
  void relax();

  // The volume's value at every seed, kept beside out_.points.
  std::vector<double> values;

private:
  bool inBox(const Vector3& p) const {
    const BBox& b = out_.box;
    return p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y &&
           p.y <= b.max.y && p.z >= b.min.z && p.z <= b.max.z;
  }
  Vector3 uniformInBox() {
    const BBox& b = out_.box;
    const double x = rng_.nextDouble(), y = rng_.nextDouble(),
                 z = rng_.nextDouble();
    return Vector3{b.min.x + x * (b.max.x - b.min.x),
                   b.min.y + y * (b.max.y - b.min.y),
                   b.min.z + z * (b.max.z - b.min.z)};
  }
  void tick() {
    if (++candidates_ % kPollEvery == 0 && cancel_)
      cancel_->throwIfRequested("porous seeds");
  }
  // Keeps p if the volume accepts it; returns whether it did.
  bool acceptIfInside(const Vector3& p) {
    const double v = volume_.valueAt(p);
    if (!(v < out_.pad)) return false;
    if (out_.points.size() >= kPorousMaxSeeds)
      fail("more than " + std::to_string(kPorousMaxSeeds) +
           " seeds (the ceiling) at spacing " + num(out_.minSpacing) +
           "; raise the spacing");
    out_.points.push_back(p);
    values.push_back(v);
    return true;
  }
  // Graded thinning for jitter / random: keep a candidate drawn at the
  // densest spacing with probability (minSpacing / spacing(p))^3.
  bool thinnedOut(const Vector3& p, double u) const {
    if (!spacing_.graded) return false;
    const double r = out_.minSpacing / spacing_.at(p);
    return !(u < r * r * r);
  }

  const ImplicitField& volume_;
  const Spacing& spacing_;
  const SeedParams& params_;
  const CancelToken* cancel_;
  SeedSet& out_;
  Pcg32 rng_;
  std::size_t candidates_ = 0;
};

// Bridson (2007) in 3D with a variable radius: a candidate c is kept when
// every seed s is at least (r(c) + r(s)) / 2 away. Candidates are drawn
// uniformly in the shell [r(a), 2 r(a)] around a random active seed a, by
// rejection from the cube (no trigonometry). When the active list empties,
// a scan over cells of the largest spacing restarts it, so every connected
// part of the volume is reached.
void Builder::poisson() {
  const double smin = out_.minSpacing, smax = out_.maxSpacing;
  internal::SeedGrid grid(out_.box, std::max(smin, 0.5 * smax));
  std::vector<double> radius;
  std::vector<std::int32_t> active;

  auto tryAccept = [&](const Vector3& c) -> bool {
    tick();
    if (!inBox(c)) return false;
    const double rc = spacing_.at(c);
    bool clash = false;
    grid.forEachNear(c, 0.5 * (rc + smax), [&](std::int32_t i) {
      if (clash) return;
      const Vector3 d = out_.points[static_cast<std::size_t>(i)] - c;
      const double m = 0.5 * (rc + radius[static_cast<std::size_t>(i)]);
      if (d.x * d.x + d.y * d.y + d.z * d.z < m * m) clash = true;
    });
    if (clash || !acceptIfInside(c)) return false;
    const auto idx = static_cast<std::int32_t>(out_.points.size() - 1);
    grid.insert(idx, c);
    radius.push_back(rc);
    active.push_back(idx);
    return true;
  };

  auto drain = [&]() {
    while (!active.empty()) {
      const std::size_t slot = static_cast<std::size_t>(
          (static_cast<std::uint64_t>(rng_.next()) * active.size()) >> 32);
      const std::size_t a = static_cast<std::size_t>(active[slot]);
      const Vector3 pa = out_.points[a];
      const double ra = radius[a];
      bool found = false;
      for (int t = 0; t < kBridsonTries && !found; ++t) {
        Vector3 d;
        double l2;
        do {
          d = Vector3{(rng_.nextDouble() - 0.5) * 4.0 * ra,
                      (rng_.nextDouble() - 0.5) * 4.0 * ra,
                      (rng_.nextDouble() - 0.5) * 4.0 * ra};
          l2 = d.x * d.x + d.y * d.y + d.z * d.z;
        } while (l2 < ra * ra || l2 > 4.0 * ra * ra);
        found = tryAccept(pa + d);
      }
      if (!found) {
        active[slot] = active.back();
        active.pop_back();
      }
    }
  };

  const BBox& b = out_.box;
  const Vector3 e = b.extent();
  const int nx = std::max(1, static_cast<int>(std::ceil(e.x / smax)));
  const int ny = std::max(1, static_cast<int>(std::ceil(e.y / smax)));
  const int nz = std::max(1, static_cast<int>(std::ceil(e.z / smax)));
  for (int z = 0; z < nz; ++z)
    for (int y = 0; y < ny; ++y)
      for (int x = 0; x < nx; ++x)
        for (int t = 0; t < kScanTries; ++t) {
          const Vector3 c{b.min.x + (x + rng_.nextDouble()) * smax,
                          b.min.y + (y + rng_.nextDouble()) * smax,
                          b.min.z + (z + rng_.nextDouble()) * smax};
          if (tryAccept(c)) {
            drain();
            break;
          }
        }
}

// A grid at the smallest spacing, centred in the box, each point moved by
// jitter * spacing * U(-1/2, 1/2)^3; graded densities thin it.
void Builder::jitter() {
  const double s = out_.minSpacing, j = params_.jitter;
  const BBox& b = out_.box;
  const Vector3 e = b.extent();
  const int nx = std::max(1, static_cast<int>(std::ceil(e.x / s)));
  const int ny = std::max(1, static_cast<int>(std::ceil(e.y / s)));
  const int nz = std::max(1, static_cast<int>(std::ceil(e.z / s)));
  const Vector3 o = b.center() - 0.5 * s * Vector3{double(nx - 1), double(ny - 1),
                                                   double(nz - 1)};
  for (int z = 0; z < nz; ++z)
    for (int y = 0; y < ny; ++y)
      for (int x = 0; x < nx; ++x) {
        tick();
        const double jx = rng_.nextDouble() - 0.5, jy = rng_.nextDouble() - 0.5,
                     jz = rng_.nextDouble() - 0.5;
        const double keep = spacing_.graded ? rng_.nextDouble() : 0.0;
        const Vector3 p = o + s * Vector3{x + j * jx, y + j * jy, z + j * jz};
        if (!inBox(p) || thinnedOut(p, keep)) continue;
        acceptIfInside(p);
      }
}

// White noise: boxVolume / spacing^3 uniform candidates, rejected outside;
// graded densities thin them.
void Builder::random() {
  const Vector3 e = out_.box.extent();
  const double s = out_.minSpacing;
  const auto n = static_cast<std::size_t>(std::llround(e.x * e.y * e.z / (s * s * s)));
  for (std::size_t i = 0; i < n; ++i) {
    tick();
    const Vector3 p = uniformInBox();
    const double keep = spacing_.graded ? rng_.nextDouble() : 0.0;
    if (thinnedOut(p, keep)) continue;
    acceptIfInside(p);
  }
}

// Lloyd by sampling: per iteration 32 uniform samples per seed in the box,
// those with volume < pad kept, each assigned to its nearest seed with the
// weight (minSpacing / spacing(x))^5; every seed moves to its samples'
// weighted centroid unless that leaves the acceptance region. Iteration i
// draws from its own stream, Pcg32(seed, i + 1); the samples are evaluated
// in parallel and accumulated in sample order, so the thread count does not
// show in the result.
void Builder::relax() {
  const std::size_t n = out_.points.size();
  if (n == 0) return;
  const unsigned threads = params_.numThreads;
  struct Sample {
    Vector3 p;
    double w = 0.0;
    std::int32_t owner = -1;
  };
  std::vector<Sample> chunk;
  std::vector<Vector3> sum(n);
  std::vector<double> wsum(n);
  for (int it = 0; it < params_.relax; ++it) {
    if (cancel_) cancel_->throwIfRequested("porous relax");
    internal::SeedGrid grid(out_.box, out_.minSpacing);
    for (std::size_t i = 0; i < n; ++i)
      grid.insert(static_cast<std::int32_t>(i), out_.points[i]);
    std::fill(sum.begin(), sum.end(), Vector3{0.0, 0.0, 0.0});
    std::fill(wsum.begin(), wsum.end(), 0.0);
    rng_ = Pcg32(params_.seed, static_cast<std::uint64_t>(it) + 1);
    const std::size_t total = n * kLloydSamplesPerSeed;
    for (std::size_t first = 0; first < total; first += kLloydChunk) {
      if (cancel_) cancel_->throwIfRequested("porous relax");
      const std::size_t m = std::min(kLloydChunk, total - first);
      chunk.resize(m);
      for (std::size_t k = 0; k < m; ++k) chunk[k].p = uniformInBox();
      const std::size_t blocks = (m + kLloydBlock - 1) / kLloydBlock;
      internal::parallelFor(blocks, threads, [&](std::size_t blk) {
        const std::size_t end = std::min(m, (blk + 1) * kLloydBlock);
        for (std::size_t k = blk * kLloydBlock; k < end; ++k) {
          Sample& s = chunk[k];
          if (!(volume_.valueAt(s.p) < out_.pad)) {
            s.owner = -1;
            continue;
          }
          s.owner = grid.nearest(s.p, out_.points);
          double w = 1.0;
          if (spacing_.graded) {
            const double r = out_.minSpacing / spacing_.at(s.p);
            for (int e = 0; e < kLloydDensityExponent; ++e) w *= r;
          }
          s.w = w;
        }
      });
      for (const Sample& s : chunk) {
        if (s.owner < 0) continue;
        const auto o = static_cast<std::size_t>(s.owner);
        sum[o] += s.w * s.p;
        wsum[o] += s.w;
      }
    }
    internal::parallelFor(n, threads, [&](std::size_t i) {
      if (!(wsum[i] > 0.0)) return;
      const Vector3 c = sum[i] / wsum[i];
      if (!inBox(c)) return;
      const double v = volume_.valueAt(c);
      if (!(v < out_.pad)) return;
      out_.points[i] = c;
      values[i] = v;
    });
  }
}

} // namespace

double densityRampWeight(DensityRamp ramp, double u) {
  if (!(u > 0.0)) return 0.0;  // also NaN
  if (u >= 1.0) return 1.0;
  switch (ramp) {
  case DensityRamp::Linear: return u;
  case DensityRamp::Smooth: return u * u * (3.0 - 2.0 * u);
  case DensityRamp::Gaussian: return (1.0 - detExp(-u * u * kLn100)) / 0.99;
  }
  return u;
}

SeedSet generatePorousSeeds(const ImplicitField& volume,
                            const ImplicitField* control,
                            const SeedParams& params,
                            const CancelToken* cancel) {
  if (!(params.spacing > 0.0) || !std::isfinite(params.spacing))
    fail("spacing must be > 0 (got " + num(params.spacing) + ")");
  const double s2 = params.spacing2 > 0.0 ? params.spacing2 : params.spacing;
  if (!std::isfinite(s2)) fail("spacing2 must be finite");
  if (params.relax < 0 || params.relax > kPorousMaxRelax)
    fail("relax must be in 0.." + std::to_string(kPorousMaxRelax) + " (got " +
         std::to_string(params.relax) + ")");
  if (!(params.jitter >= 0.0) || !std::isfinite(params.jitter))
    fail("jitter must be >= 0 (got " + num(params.jitter) + ")");

  Spacing spacing;
  spacing.s1 = params.spacing;
  spacing.s2 = s2;
  spacing.d0 = params.d0;
  spacing.d1 = params.d1;
  spacing.ramp = params.ramp;
  spacing.control = control;
  spacing.graded = s2 != params.spacing;
  if (spacing.graded) {
    if (!control) fail("spacing2 differs from spacing but no control field is given");
    if (!(params.d1 != params.d0) || !std::isfinite(params.d0) ||
        !std::isfinite(params.d1))
      fail("a graded density needs finite d0 != d1 (got d0 = " + num(params.d0) +
           ", d1 = " + num(params.d1) + ")");
  }

  SeedSet out;
  out.minSpacing = std::min(params.spacing, s2);
  out.maxSpacing = std::max(params.spacing, s2);
  out.pad = out.maxSpacing;
  const BBox vb = volume.bounds();
  if (!vb.isValid() || vb.isInfinite() || !std::isfinite(vb.min.x) ||
      !std::isfinite(vb.min.y) || !std::isfinite(vb.min.z) ||
      !std::isfinite(vb.max.x) || !std::isfinite(vb.max.y) ||
      !std::isfinite(vb.max.z))
    fail("the volume has no finite bounds");
  const Vector3 grow{out.pad, out.pad, out.pad};
  out.box = BBox{vb.min - grow, vb.max + grow};

  const double cells = internal::SeedGrid::cellCount(out.box, out.minSpacing);
  if (cells > kMaxBoxCells)
    fail("the volume's box holds " + num(cells) + " cells of spacing " +
         num(out.minSpacing) + ", past " + num(kMaxBoxCells) +
         " (32 x the ceiling of " + std::to_string(kPorousMaxSeeds) +
         " seeds); raise the spacing");
  if (cancel) cancel->throwIfRequested("porous seeds");

  Builder b(volume, spacing, params, cancel, out);
  switch (params.dist) {
  case SeedDistribution::Poisson: b.poisson(); break;
  case SeedDistribution::Jitter: b.jitter(); break;
  case SeedDistribution::Random: b.random(); break;
  }
  b.relax();

  out.isPad.resize(out.points.size());
  for (std::size_t i = 0; i < out.points.size(); ++i)
    out.isPad[i] = b.values[i] > 0.0 ? 1 : 0;
  return out;
}

} // namespace dualc
