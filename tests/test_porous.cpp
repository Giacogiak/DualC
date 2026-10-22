// The porous shell's seeds (roadmap 23 #63): the six properties of the raw
// plan's P02 list, on a sphere volume and on a box. Its Delaunay
// tetrahedralization (#64): the four properties of P03 T1; its complex and bins
// (#64, P03 T2): the dual adjacency, and the bin query against brute force for
// the four cells x mode cases. Every case is named `porous: ...` so
// `ctest -R porous` finds it.

#include "dualc/implicit.h"
#include "dualc/porous.h"
#include "dualc/primitives.h"

#include "internal/exact_predicates.h"
#include "internal/fd_gradient.h"
#include "internal/porous_complex.h"
#include "internal/porous_delaunay.h"
#include "internal/seed_grid.h"

#include "demo_meshes.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

using namespace dualc;

namespace {

std::uint64_t digest(const SeedSet& s) {
  std::uint64_t h = 1469598103934665603ULL;
  auto mix = [&](const void* data, std::size_t n) {
    const auto* b = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ULL;
  };
  for (const Vector3& p : s.points) {
    const double xyz[3] = {p.x, p.y, p.z};
    mix(xyz, sizeof xyz);
  }
  mix(s.isPad.data(), s.isPad.size());
  return h;
}

double dist2(const Vector3& a, const Vector3& b) {
  const Vector3 d = a - b;
  return d.x * d.x + d.y * d.y + d.z * d.z;
}

// The control of the graded cases: the signed distance to the plane x = 0,
// so control <= d0 on one side of the box and >= d1 on the other.
std::shared_ptr<PlaneField> xControl() {
  return std::make_shared<PlaneField>(Vector3{1.0, 0.0, 0.0}, 0.0);
}

// The coefficient of variation of the cell volumes of the seeds whose cell
// lies deep inside `volume` (volume(seed) < -depth), by Monte-Carlo over the
// box `probe`.
double cellVolumeCv(const SeedSet& s, const ImplicitField& volume, double depth,
                    const BBox& probe, std::size_t samples) {
  internal::SeedGrid grid(s.box, s.minSpacing);
  for (std::size_t i = 0; i < s.points.size(); ++i)
    grid.insert(static_cast<std::int32_t>(i), s.points[i]);
  std::vector<std::size_t> count(s.points.size(), 0);
  Pcg32 rng(7, 99);
  const Vector3 e = probe.extent();
  for (std::size_t k = 0; k < samples; ++k) {
    const Vector3 p{probe.min.x + rng.nextDouble() * e.x,
                    probe.min.y + rng.nextDouble() * e.y,
                    probe.min.z + rng.nextDouble() * e.z};
    ++count[static_cast<std::size_t>(grid.nearest(p, s.points))];
  }
  double sum = 0.0, sum2 = 0.0;
  std::size_t n = 0;
  for (std::size_t i = 0; i < s.points.size(); ++i) {
    if (!(volume.valueAt(s.points[i]) < -depth)) continue;
    const double c = static_cast<double>(count[i]);
    sum += c;
    sum2 += c * c;
    ++n;
  }
  REQUIRE(n > 30);
  const double mean = sum / n;
  return std::sqrt(sum2 / n - mean * mean) / mean;
}


// --- The Delaunay tetrahedralization (#64) -------------------------------

const double* xyz(const std::vector<Vector3>& pts, std::int32_t i) {
  return &pts[static_cast<std::size_t>(i)].x;
}

std::vector<Vector3> randomCube(std::size_t n, std::uint64_t seed) {
  Pcg32 rng(seed, 1000);
  std::vector<Vector3> pts(n);
  for (Vector3& p : pts) p = {rng.nextDouble(), rng.nextDouble(), rng.nextDouble()};
  return pts;
}

std::vector<Vector3> integerGrid(int n) {
  std::vector<Vector3> pts;
  for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j)
      for (int k = 0; k < n; ++k) pts.push_back({double(i), double(j), double(k)});
  return pts;
}

std::uint64_t digest(const internal::DelaunayTets& t) {
  std::uint64_t h = 1469598103934665603ULL;
  auto mix = [&](const std::vector<std::int32_t>& v) {
    for (std::int32_t x : v) h = (h ^ static_cast<std::uint32_t>(x)) * 1099511628211ULL;
  };
  mix(t.verts);
  mix(t.neighbours);
  mix(t.duplicates);
  return h;
}

// What the structure checks find. A valid tetrahedralization of the hull has
// every count below at zero, euler = 1 (V - E + F - T of a 3-ball) and
// hullEuler = 2 (the hull surface, a sphere).
struct TetCheck {
  long flat = 0;            // tets with orient3d == 0
  long badNeighbour = 0;    // a neighbour that does not point back across the same face
  long unusedPoints = 0;    // kept points no tet names
  long namedDuplicates = 0; // duplicate points a tet names
  long nonConvex = 0;       // a point strictly beyond a hull face
  long badHullEdges = 0;    // a hull edge not on exactly two hull faces
  long euler = 0, hullEuler = 0;
  double absVolume6 = 0.0;  // sum of |orient3d|: six times the volume
};

TetCheck checkTets(const std::vector<Vector3>& pts, const internal::DelaunayTets& d) {
  TetCheck c;
  const std::size_t nt = d.size();
  std::vector<char> used(pts.size(), 0), dup(pts.size(), 0);
  for (std::int32_t i : d.duplicates) dup[static_cast<std::size_t>(i)] = 1;
  auto face = [&](std::size_t t, int k) {
    std::array<std::int32_t, 3> f{};
    int m = 0;
    for (int j = 0; j < 4; ++j)
      if (j != k) f[m++] = d.verts[4 * t + j];
    return f;  // ascending: the vertices are
  };
  std::map<std::pair<std::int32_t, std::int32_t>, int> edges, hullEdges;
  std::vector<std::array<std::int32_t, 4>> hullFaces;  // face + the opposite vertex
  long faces = 0;
  for (std::size_t t = 0; t < nt; ++t) {
    const std::int32_t* v = &d.verts[4 * t];
    for (int k = 0; k < 4; ++k) {
      used[static_cast<std::size_t>(v[k])] = 1;
      if (dup[static_cast<std::size_t>(v[k])]) ++c.namedDuplicates;
      for (int j = k + 1; j < 4; ++j) edges[{v[k], v[j]}] = 1;
    }
    const double o = internal::orient3d(xyz(pts, v[0]), xyz(pts, v[1]),
                                        xyz(pts, v[2]), xyz(pts, v[3]));
    if (o == 0) ++c.flat;
    c.absVolume6 += std::fabs(o);
    for (int k = 0; k < 4; ++k) {
      const std::int32_t u = d.neighbours[4 * t + k];
      if (u < 0) {
        ++faces;
        const auto f = face(t, k);
        hullFaces.push_back({f[0], f[1], f[2], v[k]});
        ++hullEdges[{f[0], f[1]}];
        ++hullEdges[{f[0], f[2]}];
        ++hullEdges[{f[1], f[2]}];
        continue;
      }
      if (static_cast<std::size_t>(u) > t) ++faces;
      int back = 0;
      for (int j = 0; j < 4; ++j)
        if (d.neighbours[4 * static_cast<std::size_t>(u) + j] == static_cast<std::int32_t>(t)) {
          ++back;
          if (face(static_cast<std::size_t>(u), j) != face(t, k)) ++c.badNeighbour;
        }
      if (back != 1) ++c.badNeighbour;
    }
  }
  for (std::size_t i = 0; i < pts.size(); ++i)
    if (!dup[i] && !used[i]) ++c.unusedPoints;
  long verts = 0;
  for (char u : used) verts += u;
  c.euler = verts - long(edges.size()) + faces - long(nt);
  std::vector<char> onHull(pts.size(), 0);
  for (const auto& h : hullFaces)
    for (int k = 0; k < 3; ++k) onHull[static_cast<std::size_t>(h[k])] = 1;
  long hullVerts = 0;
  for (char u : onHull) hullVerts += u;
  for (const auto& e : hullEdges)
    if (e.second != 2) ++c.badHullEdges;
  c.hullEuler = hullVerts - long(hullEdges.size()) + long(hullFaces.size());
  // Convex: every point is on the inner side of every hull face, or on it.
  for (const auto& h : hullFaces) {
    const double* a = xyz(pts, h[0]);
    const double* b = xyz(pts, h[1]);
    const double* f = xyz(pts, h[2]);
    const double inner = internal::orient3d(a, b, f, xyz(pts, h[3]));
    for (std::size_t i = 0; i < pts.size(); ++i) {
      const double s = internal::orient3d(a, b, f, &pts[i].x);
      if ((inner > 0 && s < 0) || (inner < 0 && s > 0)) ++c.nonConvex;
    }
  }
  return c;
}

// Points strictly inside the circumsphere of `samples` tets drawn at random,
// each against every point.
long emptySphereViolations(const std::vector<Vector3>& pts,
                           const internal::DelaunayTets& d, std::size_t samples) {
  Pcg32 rng(11, 1001);
  long bad = 0;
  for (std::size_t s = 0; s < samples; ++s) {
    const std::size_t t = rng.next() % d.size();
    const double* a = xyz(pts, d.verts[4 * t]);
    const double* b = xyz(pts, d.verts[4 * t + 1]);
    const double* c = xyz(pts, d.verts[4 * t + 2]);
    const double* e = xyz(pts, d.verts[4 * t + 3]);
    const double o = internal::orient3d(a, b, c, e);
    for (const Vector3& p : pts) {
      const double in = o > 0 ? internal::insphere(a, b, c, e, &p.x)
                              : internal::insphere(b, a, c, e, &p.x);
      if (in > 0) ++bad;
    }
  }
  return bad;
}

void requireValid(const TetCheck& c) {
  CHECK(c.flat == 0);
  CHECK(c.badNeighbour == 0);
  CHECK(c.unusedPoints == 0);
  CHECK(c.namedDuplicates == 0);
  CHECK(c.nonConvex == 0);
  CHECK(c.badHullEdges == 0);
  CHECK(c.euler == 1);
  CHECK(c.hullEuler == 2);
}

} // namespace

TEST_CASE("porous: Pcg32 matches O'Neill's reference stream", "[porous]") {
  // pcg32-demo (pcg-c-basic), pcg32_srandom_r(&rng, 42u, 54u): round 1.
  Pcg32 rng(42u, 54u);
  const std::uint32_t expected[6] = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u,
                                     0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
  for (std::uint32_t e : expected) CHECK(rng.next() == e);

  Pcg32 d(1, 0);
  for (int i = 0; i < 10000; ++i) {
    const double u = d.nextDouble();
    REQUIRE(u >= 0.0);
    REQUIRE(u < 1.0);
  }
}

TEST_CASE("porous: the ramps", "[porous]") {
  for (DensityRamp r : {DensityRamp::Linear, DensityRamp::Smooth, DensityRamp::Gaussian}) {
    CHECK(densityRampWeight(r, -1.0) == 0.0);
    CHECK(densityRampWeight(r, 0.0) == 0.0);
    CHECK(densityRampWeight(r, 1.0) == 1.0);
    CHECK(densityRampWeight(r, 2.0) == 1.0);
    double prev = 0.0;
    for (int i = 1; i <= 100; ++i) {
      const double w = densityRampWeight(r, i / 100.0);
      CHECK(w >= prev);
      prev = w;
    }
  }
  CHECK(densityRampWeight(DensityRamp::Linear, 0.25) == 0.25);
  CHECK(densityRampWeight(DensityRamp::Smooth, 0.5) == 0.5);
  // Gaussian: (1 - exp(-u^2 / 2 sigma^2)) / 0.99 with sigma = 1/sqrt(2 ln 100).
  const double sigma = 1.0 / std::sqrt(2.0 * std::log(100.0));
  for (double u : {0.1, 0.3, 0.5, 0.8, 0.99}) {
    const double ref = (1.0 - std::exp(-u * u / (2.0 * sigma * sigma))) / 0.99;
    CHECK(std::abs(densityRampWeight(DensityRamp::Gaussian, u) - ref) < 1e-14);
  }
}

TEST_CASE("porous: same seed, same seeds; another seed, other seeds", "[porous]") {
  SphereField sphere({0.0, 0.0, 0.0}, 3.0);
  BoxField box({-2.0, -2.0, -2.0}, {2.0, 2.0, 2.0});
  auto ctl = xControl();
  for (const ImplicitField* vol : {static_cast<const ImplicitField*>(&sphere),
                                   static_cast<const ImplicitField*>(&box)}) {
    for (SeedDistribution d :
         {SeedDistribution::Poisson, SeedDistribution::Jitter, SeedDistribution::Random}) {
      SeedParams p;
      p.dist = d;
      p.spacing = 0.6;
      p.spacing2 = 0.9;
      p.d0 = -1.0;
      p.d1 = 1.0;
      p.relax = 2;
      p.seed = 11;
      const SeedSet a = generatePorousSeeds(*vol, ctl.get(), p);
      p.numThreads = 1;
      const SeedSet b = generatePorousSeeds(*vol, ctl.get(), p);
      REQUIRE(a.points.size() > 100);
      CHECK(a.points.size() == b.points.size());
      CHECK(digest(a) == digest(b));
      p.seed = 12;
      const SeedSet c = generatePorousSeeds(*vol, ctl.get(), p);
      CHECK(digest(a) != digest(c));
    }
  }
}

TEST_CASE("porous: poisson keeps the local minimum distance", "[porous]") {
  SphereField sphere({0.0, 0.0, 0.0}, 3.0);
  BoxField box({-2.5, -2.0, -2.0}, {2.5, 2.0, 2.0});
  auto ctl = xControl();
  for (const ImplicitField* vol : {static_cast<const ImplicitField*>(&sphere),
                                   static_cast<const ImplicitField*>(&box)}) {
    for (bool graded : {false, true}) {
      SeedParams p;
      p.spacing = 0.5;
      p.spacing2 = graded ? 1.0 : 0.0;
      p.d0 = -1.5;
      p.d1 = 1.5;
      p.ramp = DensityRamp::Smooth;
      p.seed = 3;
      const SeedSet s = generatePorousSeeds(*vol, graded ? ctl.get() : nullptr, p);
      REQUIRE(s.points.size() > 200);
      auto r = [&](const Vector3& x) {
        if (!graded) return p.spacing;
        const double u = (ctl->valueAt(x) - p.d0) / (p.d1 - p.d0);
        return p.spacing + (p.spacing2 - p.spacing) * densityRampWeight(p.ramp, u);
      };
      double worst = 1e9;
      for (std::size_t i = 0; i < s.points.size(); ++i)
        for (std::size_t j = i + 1; j < s.points.size(); ++j) {
          const double m = 0.5 * (r(s.points[i]) + r(s.points[j]));
          worst = std::min(worst, std::sqrt(dist2(s.points[i], s.points[j])) / m);
        }
      CHECK(worst >= 0.99);
    }
  }
}

TEST_CASE("porous: the graded density follows (spacing/spacing2)^3", "[porous]") {
  // A box 14 long in x, control = x: spacing 0.5 for x <= -2, spacing2 0.8
  // for x >= 2. Densities are counted in the slabs x in [-6, -3] and [3, 6],
  // |y|, |z| <= 3.5, away from the ramp and from the box's faces.
  BoxField box({-7.0, -4.0, -4.0}, {7.0, 4.0, 4.0});
  auto ctl = xControl();
  const double s1 = 0.5, s2 = 0.8;
  const double expected = (s2 / s1) * (s2 / s1) * (s2 / s1);
  for (SeedDistribution d :
       {SeedDistribution::Poisson, SeedDistribution::Jitter, SeedDistribution::Random}) {
    for (int relax : {0, 10}) {
      if (relax && d != SeedDistribution::Poisson) continue;
      SeedParams p;
      p.dist = d;
      p.spacing = s1;
      p.spacing2 = s2;
      p.d0 = -2.0;
      p.d1 = 2.0;
      p.relax = relax;
      p.seed = 5;
      const SeedSet s = generatePorousSeeds(box, ctl.get(), p);
      std::size_t dense = 0, sparse = 0;
      for (const Vector3& x : s.points) {
        if (std::abs(x.y) > 3.5 || std::abs(x.z) > 3.5) continue;
        if (x.x >= -6.0 && x.x <= -3.0) ++dense;
        if (x.x >= 3.0 && x.x <= 6.0) ++sparse;
      }
      REQUIRE(sparse > 100);
      const double ratio = static_cast<double>(dense) / static_cast<double>(sparse);
      INFO("dist " << int(d) << " relax " << relax << ": dense " << dense
                   << ", sparse " << sparse << ", ratio " << ratio
                   << ", expected " << expected);
      CHECK(std::abs(ratio / expected - 1.0) < 0.20);
    }
  }
}

TEST_CASE("porous: every seed is in the volume or in the pad", "[porous]") {
  SphereField sphere({1.0, -0.5, 0.25}, 2.5);
  BoxField box({-2.0, -1.0, -3.0}, {2.0, 1.0, 3.0});
  auto ctl = xControl();
  for (const ImplicitField* vol : {static_cast<const ImplicitField*>(&sphere),
                                   static_cast<const ImplicitField*>(&box)}) {
    for (SeedDistribution d :
         {SeedDistribution::Poisson, SeedDistribution::Jitter, SeedDistribution::Random}) {
      SeedParams p;
      p.dist = d;
      p.spacing = 0.4;
      p.spacing2 = 0.7;
      p.d0 = -1.0;
      p.d1 = 1.0;
      p.relax = 3;
      const SeedSet s = generatePorousSeeds(*vol, ctl.get(), p);
      REQUIRE(s.points.size() == s.isPad.size());
      CHECK(s.pad == 0.7);
      std::size_t pads = 0;
      for (std::size_t i = 0; i < s.points.size(); ++i) {
        const double v = vol->valueAt(s.points[i]);
        REQUIRE(v < s.pad);
        REQUIRE((s.isPad[i] != 0) == (v > 0.0));
        pads += s.isPad[i];
      }
      CHECK(pads > 0);
      CHECK(pads < s.points.size());
    }
  }
}

TEST_CASE("porous: relax lowers the variance of the cell volumes", "[porous]") {
  BoxField box({-5.0, -5.0, -5.0}, {5.0, 5.0, 5.0});
  SphereField sphere({0.0, 0.0, 0.0}, 5.0);
  for (const ImplicitField* vol : {static_cast<const ImplicitField*>(&box),
                                   static_cast<const ImplicitField*>(&sphere)}) {
    for (SeedDistribution d : {SeedDistribution::Poisson, SeedDistribution::Random}) {
      SeedParams p;
      p.dist = d;
      p.spacing = 1.0;
      p.seed = 9;
      const SeedSet raw = generatePorousSeeds(*vol, nullptr, p);
      p.relax = 10;
      const SeedSet relaxed = generatePorousSeeds(*vol, nullptr, p);
      const BBox probe{{-4.5, -4.5, -4.5}, {4.5, 4.5, 4.5}};
      const double cv0 = cellVolumeCv(raw, *vol, 2.5, probe, 400000);
      const double cv1 = cellVolumeCv(relaxed, *vol, 2.5, probe, 400000);
      INFO("dist " << int(d) << ": CV " << cv0 << " -> " << cv1);
      CHECK(cv1 < 0.8 * cv0);
    }
  }
}

TEST_CASE("porous: cancel stops the build", "[porous]") {
  SphereField sphere({0.0, 0.0, 0.0}, 3.0);
  CancelToken token;
  token.request();
  for (SeedDistribution d :
       {SeedDistribution::Poisson, SeedDistribution::Jitter, SeedDistribution::Random}) {
    SeedParams p;
    p.dist = d;
    p.spacing = 0.3;
    CHECK_THROWS_AS(generatePorousSeeds(sphere, nullptr, p, &token), Cancelled);
  }

  // A token flipped while the build runs: a field that requests it on its
  // 50,000th evaluation, well before a spacing-0.15 sphere is done.
  struct Tripwire : ImplicitField {
    SphereField s{{0.0, 0.0, 0.0}, 3.0};
    CancelToken* t;
    mutable std::size_t calls = 0;
    explicit Tripwire(CancelToken* tok) : t(tok) {}
    double valueAt(const Vector3& p) const override {
      if (++calls == 50000) t->request();
      return s.valueAt(p);
    }
    Vector3 gradientAt(const Vector3& p) const override { return s.gradientAt(p); }
    BBox bounds() const override { return s.bounds(); }
  };
  for (int relax : {0, 5}) {
    CancelToken live;
    Tripwire vol(&live);
    SeedParams p;
    p.spacing = relax ? 0.5 : 0.15;
    p.relax = relax;
    p.numThreads = 1;  // Tripwire's counter is not atomic
    CHECK_THROWS_AS(generatePorousSeeds(vol, nullptr, p, &live), Cancelled);
    CHECK(vol.calls < 50000 + 70000);
  }
}

TEST_CASE("porous: bad parameters and the ceilings are clear errors", "[porous]") {
  SphereField sphere({0.0, 0.0, 0.0}, 1.0);
  auto ctl = xControl();
  SeedParams p;
  CHECK_THROWS_AS(generatePorousSeeds(sphere, nullptr, p), std::invalid_argument);
  p.spacing = 0.5;
  p.relax = kPorousMaxRelax + 1;
  CHECK_THROWS_AS(generatePorousSeeds(sphere, nullptr, p), std::invalid_argument);
  p.relax = -1;
  CHECK_THROWS_AS(generatePorousSeeds(sphere, nullptr, p), std::invalid_argument);
  p.relax = 0;
  p.spacing2 = 1.0;  // graded with no control
  CHECK_THROWS_AS(generatePorousSeeds(sphere, nullptr, p), std::invalid_argument);
  p.d0 = p.d1 = 0.0;  // graded with d1 == d0
  CHECK_THROWS_AS(generatePorousSeeds(sphere, ctl.get(), p), std::invalid_argument);
  PlaneField plane({0.0, 0.0, 1.0}, 0.0);
  p.spacing2 = 0.0;
  CHECK_THROWS_AS(generatePorousSeeds(plane, nullptr, p), std::invalid_argument);
  // The box ceiling, refused before any allocation...
  p.spacing = 1e-3;
  CHECK_THROWS_AS(generatePorousSeeds(sphere, nullptr, p), std::invalid_argument);
  // ...and the seed ceiling itself: a box just under the cell limit whose
  // white noise passes 2,000,000 seeds.
  BoxField big({-60.0, -60.0, -60.0}, {60.0, 60.0, 60.0});
  p.spacing = 0.9;
  p.dist = SeedDistribution::Random;
  try {
    generatePorousSeeds(big, nullptr, p);
    FAIL("no ceiling error");
  } catch (const std::invalid_argument& e) {
    CHECK(std::string(e.what()).find("2000000") != std::string::npos);
  }
}

TEST_CASE("porous: the seed grid's nearest query equals brute force", "[porous]") {
  Pcg32 rng(123, 4);
  const BBox b{{-2.0, -1.0, -3.0}, {2.0, 1.0, 3.0}};
  std::vector<Vector3> pts;
  for (int i = 0; i < 500; ++i)
    pts.push_back({-2.0 + 4.0 * rng.nextDouble(), -1.0 + 2.0 * rng.nextDouble(),
                   -3.0 + 6.0 * rng.nextDouble()});
  internal::SeedGrid grid(b, 0.3);
  for (std::size_t i = 0; i < pts.size(); ++i)
    grid.insert(static_cast<std::int32_t>(i), pts[i]);
  for (int k = 0; k < 5000; ++k) {
    // Queries reach a unit past the box, where the grid clamps.
    const Vector3 q{-3.0 + 6.0 * rng.nextDouble(), -2.0 + 4.0 * rng.nextDouble(),
                    -4.0 + 8.0 * rng.nextDouble()};
    std::int32_t best = -1;
    double bd = 1e300;
    for (std::size_t i = 0; i < pts.size(); ++i) {
      const double d = dist2(q, pts[i]);
      if (d < bd) {
        bd = d;
        best = static_cast<std::int32_t>(i);
      }
    }
    REQUIRE(grid.nearest(q, pts) == best);
  }
}

// Not run by ctest (hidden tag): the seeds-per-second measurement recorded in
// roadmap 23/02 #63, on the demo torus as a mesh volume (MeshSource, its
// default sign) beside an analytic sphere. Run:
//   build/tests/dualc_tests "[porous-bench]"
TEST_CASE("porous: seeds per second on a mesh volume", "[.][porous-bench]") {
  auto [mesh, geom] = dce::makeTorus(1.0, 0.35, 96, 48);
  MeshSource torus(*mesh, *geom);
  SphereField sphere({0.0, 0.0, 0.0}, 1.0);
  const std::pair<const char*, const ImplicitField*> vols[2] = {
      {"torus mesh (9216 tris)", &torus}, {"analytic sphere r=1", &sphere}};
  for (const auto& [name, vol] : vols)
    for (int relax : {0, 5})
      for (double s : {0.1, 0.05, 0.025}) {
        SeedParams p;
        p.spacing = s;
        p.relax = relax;
        const auto t0 = std::chrono::steady_clock::now();
        const SeedSet seeds = generatePorousSeeds(*vol, nullptr, p);
        const double sec = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - t0).count();
        std::ostringstream os;
        os << name << ", poisson, spacing " << s << ", relax " << relax << ": "
           << seeds.points.size() << " seeds in " << sec << " s = "
           << seeds.points.size() / sec << " seeds/s";
        WARN(os.str());
      }
}

TEST_CASE("porous: the tets fill the convex hull by Euler's formula", "[porous]") {
  // White noise in a cube, and the seeds of a sphere volume with their pad.
  SphereField sphere({0.0, 0.0, 0.0}, 1.0);
  SeedParams sp;
  sp.spacing = 0.12;
  sp.seed = 3;
  const std::vector<Vector3> sets[2] = {randomCube(3000, 5),
                                        generatePorousSeeds(sphere, nullptr, sp).points};
  for (const std::vector<Vector3>& pts : sets) {
    const internal::DelaunayTets d = internal::tetrahedralize(pts);
    INFO(pts.size() << " points, " << d.size() << " tets");
    CHECK(d.duplicates.empty());
    requireValid(checkTets(pts, d));
    // About 6.5 tets per point in a cloud this size (6.7 at 100k).
    CHECK(d.size() > 5 * pts.size());
    CHECK(d.size() < 7 * pts.size());
  }
}

TEST_CASE("porous: no point is inside a tet's circumsphere", "[porous]") {
  SphereField sphere({0.0, 0.0, 0.0}, 1.0);
  SeedParams sp;
  sp.spacing = 0.12;
  sp.dist = SeedDistribution::Random;
  const std::vector<Vector3> sets[2] = {randomCube(3000, 6),
                                        generatePorousSeeds(sphere, nullptr, sp).points};
  for (const std::vector<Vector3>& pts : sets) {
    const internal::DelaunayTets d = internal::tetrahedralize(pts);
    INFO(pts.size() << " points");
    CHECK(emptySphereViolations(pts, d, 400) == 0);
  }
}

TEST_CASE("porous: a regular grid gives no degenerate tet", "[porous]") {
  // jitter = 0: the seeds are a lattice, every cube's eight corners
  // cospherical and every hull face coplanar with its neighbours.
  BoxField box({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
  SeedParams sp;
  sp.dist = SeedDistribution::Jitter;
  sp.jitter = 0.0;
  sp.spacing = 0.25;
  const SeedSet lattice = generatePorousSeeds(box, nullptr, sp);
  const std::vector<Vector3> grid = integerGrid(8);
  for (const std::vector<Vector3>* pts : {&lattice.points, &grid}) {
    const internal::DelaunayTets d = internal::tetrahedralize(*pts);
    INFO(pts->size() << " points, " << d.size() << " tets");
    const TetCheck c = checkTets(*pts, d);
    requireValid(c);
    CHECK(emptySphereViolations(*pts, d, 300) == 0);
    if (pts == &grid) CHECK(c.absVolume6 == 6.0 * 7 * 7 * 7);  // exact: integers
  }
}

TEST_CASE("porous: same points, same tet list", "[porous]") {
  const std::vector<Vector3> cloud = randomCube(5000, 9);
  const std::vector<Vector3> grid = integerGrid(9);
  const internal::DelaunayTets a = internal::tetrahedralize(cloud);
  const internal::DelaunayTets b = internal::tetrahedralize(cloud);
  CHECK(a.verts == b.verts);
  CHECK(a.neighbours == b.neighbours);
  const internal::DelaunayTets g1 = internal::tetrahedralize(grid);
  const internal::DelaunayTets g2 = internal::tetrahedralize(grid);
  CHECK(g1.verts == g2.verts);
  CHECK(g1.neighbours == g2.neighbours);
  // Pinned, so a CI OS whose list differs fails here. The cloud is in general
  // position, so its tetrahedralization is unique; the grid's depends on the
  // insertion order, so this also pins the Morton order.
  CHECK(digest(a) == 0x05c997ba5690c4c5ULL);
  CHECK(digest(g1) == 0xf248fc73c6345e82ULL);
}

TEST_CASE("porous: delaunay rejects what does not span 3D, skips duplicates", "[porous]") {
  using internal::tetrahedralize;
  CHECK_THROWS_AS(tetrahedralize({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}), std::invalid_argument);
  std::vector<Vector3> flat = randomCube(200, 2);
  for (Vector3& p : flat) p.z = 0.5;
  CHECK_THROWS_AS(tetrahedralize(flat), std::invalid_argument);
  std::vector<Vector3> line;
  for (int i = 0; i < 50; ++i) line.push_back({0.1 * i, 0.2 * i, 0.3 * i});
  CHECK_THROWS_AS(tetrahedralize(line), std::invalid_argument);
  std::vector<Vector3> same(10, Vector3{1, 2, 3});
  CHECK_THROWS_AS(tetrahedralize(same), std::invalid_argument);
  std::vector<Vector3> nan = randomCube(10, 3);
  nan[4].y = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS_AS(tetrahedralize(nan), std::invalid_argument);
  CHECK_THROWS_AS(tetrahedralize(std::vector<Vector3>(kPorousMaxSeeds + 1)),
                  std::invalid_argument);

  // A repeated point is reported (the later index) and named by no tet; the
  // rest is the tetrahedralization of the distinct points.
  const std::vector<Vector3> base = randomCube(500, 4);
  std::vector<Vector3> withDup = base;
  withDup.push_back(base[3]);
  withDup.push_back(base[10]);
  withDup.push_back(base[3]);
  const internal::DelaunayTets d0 = tetrahedralize(base);
  const internal::DelaunayTets d1 = tetrahedralize(withDup);
  CHECK(d1.duplicates == std::vector<std::int32_t>{500, 501, 502});
  CHECK(d1.verts == d0.verts);
  CHECK(d1.neighbours == d0.neighbours);
  requireValid(checkTets(withDup, d1));

  CancelToken token;
  token.request();
  CHECK_THROWS_AS(tetrahedralize(base, &token), Cancelled);
}

TEST_CASE("porous: tets per second", "[.][porous-bench]") {
  BoxField box({-0.65, -0.65, -0.65}, {0.65, 0.65, 0.65});
  SeedParams sp;
  sp.spacing = 0.025;
  const std::pair<const char*, std::vector<Vector3>> sets[2] = {
      {"100k white noise in a cube", randomCube(100000, 42)},
      {"poisson seeds of a box, spacing 0.025", generatePorousSeeds(box, nullptr, sp).points}};
  for (const auto& [name, pts] : sets)
    for (int run = 0; run < 2; ++run) {
      const auto t0 = std::chrono::steady_clock::now();
      const internal::DelaunayTets d = internal::tetrahedralize(pts);
      const double ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - t0).count();
      std::size_t hull = 0;
      for (std::int32_t u : d.neighbours) hull += (u < 0);
      std::ostringstream os;
      os << name << ": " << pts.size() << " points, " << d.size() << " tets ("
         << double(d.size()) / pts.size() << " per point), " << hull
         << " hull faces, " << ms << " ms";
      WARN(os.str());
    }
}

// --- The complex and its bins (#64, T2) -------------------------------------

namespace {

using internal::PorousCandidate;
using internal::PorousComplex;
using internal::PorousDual;

const PorousCells kCells[2] = {PorousCells::Voronoi, PorousCells::Delaunay};
const PorousMode kModes[2] = {PorousMode::Walls, PorousMode::Struts};

const char* caseName(PorousCells c, PorousMode m) {
  if (c == PorousCells::Voronoi) return m == PorousMode::Walls ? "voronoi walls" : "voronoi struts";
  return m == PorousMode::Walls ? "delaunay walls" : "delaunay struts";
}

// The two seed sets of the complex cases: blue noise in a sphere, and the
// regular grid (jitter = 0), whose cospherical cubes are the degenerate case.
SeedSet sphereSeeds() {
  SphereField sphere({0.1, -0.2, 0.05}, 1.0);
  SeedParams sp;
  sp.spacing = 0.2;
  sp.seed = 21;
  return generatePorousSeeds(sphere, nullptr, sp);
}
SeedSet gridSeeds() {
  BoxField box({-0.6, -0.6, -0.6}, {0.6, 0.6, 0.6});
  SeedParams sp;
  sp.dist = SeedDistribution::Jitter;
  sp.jitter = 0.0;
  sp.spacing = 0.25;
  return generatePorousSeeds(box, nullptr, sp);
}

double dot3(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }


// Ericson's closest point on a triangle, for the geometric reference.
double pointTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c) {
  const Vector3 ab = b - a, ac = c - a, ap = p - a;
  const double d1 = dot3(ab, ap), d2 = dot3(ac, ap);
  if (d1 <= 0 && d2 <= 0) return std::sqrt(dist2(p, a));
  const Vector3 bp = p - b;
  const double d3 = dot3(ab, bp), d4 = dot3(ac, bp);
  if (d3 >= 0 && d4 <= d3) return std::sqrt(dist2(p, b));
  const double vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0) return std::sqrt(dist2(p, a + (d1 / (d1 - d3)) * ab));
  const Vector3 cp = p - c;
  const double d5 = dot3(ab, cp), d6 = dot3(ac, cp);
  if (d6 >= 0 && d5 <= d6) return std::sqrt(dist2(p, c));
  const double vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0) return std::sqrt(dist2(p, a + (d2 / (d2 - d6)) * ac));
  const double va = d3 * d6 - d5 * d4;
  if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
    return std::sqrt(dist2(p, b + ((d4 - d3) / ((d4 - d3) + (d5 - d6))) * (c - b)));
  const double den = 1.0 / (va + vb + vc);
  return std::sqrt(dist2(p, a + ab * (vb * den) + ac * (vc * den)));
}

// The brute-force references: every primitive, no bins, no walks.
struct Brute {
  const SeedSet& seeds;
  const PorousDual& dual;
  std::vector<char> dup;
  std::vector<std::array<double, 6>> tetBox;
  std::vector<std::vector<std::int32_t>> tetNeighbours;  // per seed, read off the tets

  Brute(const SeedSet& s, const PorousDual& d)
      : seeds(s), dual(d), dup(d.points.size(), 0), tetNeighbours(d.points.size()) {
    for (std::int32_t i : d.tets.duplicates) dup[std::size_t(i)] = 1;
    for (std::size_t t = 0; t < d.tets.size(); ++t) {
      const std::int32_t* v = &d.tets.verts[4 * t];
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
          if (i != j) tetNeighbours[std::size_t(v[i])].push_back(v[j]);
    }
    for (auto& nb : tetNeighbours) {
      std::sort(nb.begin(), nb.end());
      nb.erase(std::unique(nb.begin(), nb.end()), nb.end());
    }
    for (std::size_t t = 0; t < d.tets.size(); ++t) {
      std::array<double, 6> b{1e300, 1e300, 1e300, -1e300, -1e300, -1e300};
      for (int k = 0; k < 4; ++k) {
        const Vector3& v = d.points[std::size_t(d.tets.verts[4 * t + std::size_t(k)])];
        const double c[3] = {v.x, v.y, v.z};
        for (int a = 0; a < 3; ++a) {
          b[std::size_t(a)] = std::min(b[std::size_t(a)], c[a]);
          b[std::size_t(3 + a)] = std::max(b[std::size_t(3 + a)], c[a]);
        }
      }
      tetBox.push_back(b);
    }
  }

  std::int32_t nearestSeed(const Vector3& p) const {
    std::int32_t best = -1;
    double bd = 1e300;
    for (std::size_t i = 0; i < dual.points.size(); ++i) {
      if (dup[i]) continue;
      const double d = dist2(p, dual.points[i]);
      if (d < bd) {
        bd = d;
        best = std::int32_t(i);
      }
    }
    return best;
  }

  // The tet holding p by exact orient3d, or -1 outside the hull.
  std::int32_t locate(const Vector3& p) const {
    const double q[3] = {p.x, p.y, p.z};
    for (std::size_t t = 0; t < dual.tets.size(); ++t) {
      const auto& b = tetBox[t];
      if (q[0] < b[0] || q[1] < b[1] || q[2] < b[2] || q[0] > b[3] || q[1] > b[4] || q[2] > b[5])
        continue;
      const double* v[4];
      for (int k = 0; k < 4; ++k) v[k] = xyz(dual.points, dual.tets.verts[4 * t + std::size_t(k)]);
      const double o = internal::orient3d(v[0], v[1], v[2], v[3]);
      bool in = true;
      for (int k = 0; k < 4 && in; ++k) {
        const double* w[4] = {v[0], v[1], v[2], v[3]};
        w[k] = q;
        const double s = internal::orient3d(w[0], w[1], w[2], w[3]);
        in = s == 0 || ((s > 0) == (o > 0));
      }
      if (in) return std::int32_t(t);
    }
    return -1;
  }

  double voronoiWalls(const PorousComplex& c, const Vector3& p, double k,
                      bool allSeeds) const {
    const std::int32_t s = nearestSeed(p);
    const Vector3 ps = internal::porousPackedPoint(&c.seeds[4 * std::size_t(s)]);
    std::vector<PorousCandidate> cand;
    if (allSeeds) {  // every other seed, not only the Delaunay neighbours
      for (std::size_t j = 0; j < dual.points.size(); ++j)
        if (!dup[j] && j != std::size_t(s))
          cand.push_back({internal::porousBisectorDistance(
                              p, ps, internal::porousPackedPoint(&c.seeds[4 * j])),
                          std::int32_t(j)});
    } else {  // s's neighbours, read off the tets
      for (std::int32_t j : tetNeighbours[std::size_t(s)])
        cand.push_back({internal::porousBisectorDistance(
                            p, ps, internal::porousPackedPoint(&c.seeds[4 * std::size_t(j)])),
                        j});
    }
    return std::max(0.0, internal::porousSmoothFold(cand, k));
  }

  // The fold over the packed planes of the tet located exactly.
  double delaunayWalls(const PorousComplex& c, const Vector3& p, double k) const {
    const std::int32_t t = locate(p);
    if (t < 0) return 0.0;
    std::vector<PorousCandidate> cand;
    for (int f = 0; f < 4; ++f)
      cand.push_back({-internal::porousPlaneDistance(&c.tetPlanes[16 * std::size_t(t) + 4 * std::size_t(f)], p),
                      4 * t + f});
    return std::max(0.0, internal::porousSmoothFold(cand, k));
  }

  // The geometric distance to every Delaunay face (k = 0), 0 outside the hull.
  double delaunayFaces(const Vector3& p) const {
    if (locate(p) < 0) return 0.0;
    double m = 1e300;
    for (std::size_t f = 0; f < dual.faces.size() / 3; ++f)
      m = std::min(m, pointTriangle(p, dual.points[std::size_t(dual.faces[3 * f])],
                                    dual.points[std::size_t(dual.faces[3 * f + 1])],
                                    dual.points[std::size_t(dual.faces[3 * f + 2])]));
    return m;
  }

  static double struts(const PorousComplex& c, const Vector3& p, double k) {
    const double B = double(c.bound);
    std::vector<PorousCandidate> cand;
    const double reject = B * (1 + 1e-9);
    for (std::size_t s = 0; s < c.segments.size() / 8; ++s) {
      const float* v = &c.segments[8 * s];
      // Skip a segment whose box is beyond B: its distance is too.
      double g2 = 0.0;
      for (int a = 0; a < 3; ++a) {
        const double q = a == 0 ? p.x : a == 1 ? p.y : p.z;
        const double lo = std::min(v[a], v[4 + a]), hi = std::max(v[a], v[4 + a]);
        const double g = q < lo ? lo - q : q > hi ? q - hi : 0.0;
        g2 += g * g;
      }
      if (g2 >= reject * reject) continue;
      const double d = internal::porousSegmentDistance(p, internal::porousPackedPoint(v),
                                                       internal::porousPackedPoint(v + 4));
      if (d < B) cand.push_back({d, std::int32_t(s)});
    }
    if (cand.empty()) return B;
    return std::max(0.0, std::min(internal::porousSmoothFold(cand, k), B));
  }
};

std::vector<Vector3> randomIn(const BBox& b, std::size_t n, std::uint64_t seed) {
  Pcg32 rng(seed, 2000);
  const Vector3 e = b.extent();
  std::vector<Vector3> pts(n);
  for (Vector3& p : pts)
    p = {b.min.x + rng.nextDouble() * e.x, b.min.y + rng.nextDouble() * e.y,
         b.min.z + rng.nextDouble() * e.z};
  return pts;
}

std::uint64_t digest(const PorousComplex& c) {
  std::uint64_t h = 1469598103934665603ULL;
  auto mix = [&](const void* data, std::size_t n) {
    const auto* b = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ULL;
  };
  auto mixf = [&](const std::vector<float>& v) { mix(v.data(), 4 * v.size()); };
  auto mixi = [&](const std::vector<std::int32_t>& v) { mix(v.data(), 4 * v.size()); };
  mix(c.bins.origin, sizeof c.bins.origin);
  mix(&c.bins.size, sizeof c.bins.size);
  mix(c.bins.dims, sizeof c.bins.dims);
  mix(&c.bound, sizeof c.bound);
  mixf(c.seeds);
  mixi(c.neighbourOffsets);
  mixi(c.neighbours);
  mixf(c.tetPlanes);
  mixi(c.tetNeighbours);
  mixi(c.binStart);
  mixf(c.segments);
  mixi(c.binOffsets);
  mixi(c.binItems);
  return h;
}

} // namespace

TEST_CASE("porous: the dual adjacency is consistent", "[porous]") {
  for (const SeedSet& seeds : {sphereSeeds(), gridSeeds()}) {
    const PorousDual d = internal::derivePorousDual(seeds);
    const std::size_t nt = d.tets.size(), np = d.points.size();
    const std::size_t nf = d.faceTets.size() / 2, ne = d.edges.size() / 2;
    INFO(np << " seeds, " << nt << " tets, " << nf << " faces, " << ne << " edges");
    REQUIRE(d.faces.size() == 3 * nf);
    // The points are the seeds rounded to float.
    for (std::size_t i = 0; i < np; ++i)
      REQUIRE(d.points[i].x == double(float(seeds.points[i].x)));

    // Each face once; its tets are adjacent across it: every Voronoi edge
    // joins two tets that are each other's neighbour, through that face.
    std::map<std::array<std::int32_t, 3>, int> faceSet;
    std::size_t hull = 0, interior = 0;
    long bad = 0;
    for (std::size_t f = 0; f < nf; ++f) {
      const std::array<std::int32_t, 3> fv{d.faces[3 * f], d.faces[3 * f + 1], d.faces[3 * f + 2]};
      if (!(fv[0] < fv[1] && fv[1] < fv[2])) ++bad;
      ++faceSet[fv];
      const std::int32_t t0 = d.faceTets[2 * f], t1 = d.faceTets[2 * f + 1];
      auto holds = [&](std::int32_t t) {
        const std::int32_t* v = &d.tets.verts[4 * std::size_t(t)];
        int n = 0;
        for (std::int32_t x : fv) n += std::find(v, v + 4, x) != v + 4;
        return n == 3;
      };
      auto across = [&](std::int32_t t, std::int32_t u) {  // u is t's neighbour opposite
        for (int k = 0; k < 4; ++k)                     // the vertex not on the face
          if (std::find(fv.begin(), fv.end(), d.tets.verts[4 * std::size_t(t) + std::size_t(k)]) == fv.end())
            return d.tets.neighbours[4 * std::size_t(t) + std::size_t(k)] == u;
        return false;
      };
      if (!holds(t0) || !across(t0, t1)) ++bad;
      if (t1 < 0) {
        ++hull;
        continue;
      }
      ++interior;
      if (!(t0 < t1) || !holds(t1) || !across(t1, t0)) ++bad;
      // Both circumcentres are on the face's dual line: equidistant from its
      // three vertices (unless clamped).
      for (std::int32_t t : {t0, t1}) {
        if (d.clamped[std::size_t(t)]) continue;
        const Vector3& c = d.circumcentres[std::size_t(t)];
        const double r0 = dist2(c, d.points[std::size_t(fv[0])]);
        for (int j = 1; j < 3; ++j)
          if (std::fabs(dist2(c, d.points[std::size_t(fv[std::size_t(j)])]) - r0) > 1e-9 * (1 + r0)) ++bad;
      }
    }
    CHECK(bad == 0);
    CHECK(faceSet.size() == nf);
    CHECK(4 * nt == 2 * interior + hull);
    std::size_t tetPairs = 0;
    for (std::int32_t u : d.tets.neighbours) tetPairs += u >= 0;
    CHECK(tetPairs == 2 * interior);

    // Each edge once, sorted; Euler for the 3-ball: V - E + F - T = 1.
    bad = 0;
    for (std::size_t e = 0; e < ne; ++e) {
      if (!(d.edges[2 * e] < d.edges[2 * e + 1])) ++bad;
      if (e > 0 && !(std::make_pair(d.edges[2 * e - 2], d.edges[2 * e - 1]) <
                     std::make_pair(d.edges[2 * e], d.edges[2 * e + 1])))
        ++bad;
    }
    CHECK(bad == 0);
    const long V = long(np - d.tets.duplicates.size());
    CHECK(V - long(ne) + long(nf) - long(nt) == 1);

    // The neighbour lists: ascending, symmetric, the edges seen from each end.
    REQUIRE(d.neighbourOffsets.size() == np + 1);
    CHECK(std::size_t(d.neighbourOffsets.back()) == 2 * ne);
    std::set<std::pair<std::int32_t, std::int32_t>> edgeSet;
    for (std::size_t e = 0; e < ne; ++e) edgeSet.insert({d.edges[2 * e], d.edges[2 * e + 1]});
    bad = 0;
    for (std::size_t i = 0; i < np; ++i)
      for (std::int32_t j = d.neighbourOffsets[i]; j < d.neighbourOffsets[i + 1]; ++j) {
        const std::int32_t q = d.neighbours[std::size_t(j)];
        if (j > d.neighbourOffsets[i] && !(d.neighbours[std::size_t(j) - 1] < q)) ++bad;
        if (!edgeSet.count({std::min(std::int32_t(i), q), std::max(std::int32_t(i), q)})) ++bad;
      }
    CHECK(bad == 0);

    // The Voronoi vertices: equidistant from their tet's corners, or clamped
    // into the padded box and counted.
    std::size_t clamped = 0;
    bad = 0;
    for (std::size_t t = 0; t < nt; ++t) {
      const Vector3& c = d.circumcentres[t];
      if (c.x < seeds.box.min.x || c.x > seeds.box.max.x || c.y < seeds.box.min.y ||
          c.y > seeds.box.max.y || c.z < seeds.box.min.z || c.z > seeds.box.max.z)
        ++bad;
      if (d.clamped[t]) {
        ++clamped;
        continue;
      }
      const double r0 = dist2(c, d.points[std::size_t(d.tets.verts[4 * t])]);
      for (int k = 1; k < 4; ++k)
        if (std::fabs(dist2(c, d.points[std::size_t(d.tets.verts[4 * t + std::size_t(k)])]) - r0) >
            1e-9 * (1 + r0))
          ++bad;
    }
    CHECK(bad == 0);
    CHECK(clamped == d.clampedCount);
    CHECK(clamped < nt / 5);  // the pad's hull slivers: 9 % of the sphere's tets

    // The packed tet planes: a shared face is (n, d) on one side and exactly
    // (-n, -d) on the other, and every tet's own corners are on the inner
    // side of its planes.
    const PorousComplex dw = internal::packPorousComplex(d, seeds, PorousCells::Delaunay, PorousMode::Walls);
    bad = 0;
    for (std::size_t t = 0; t < nt; ++t)
      for (int k = 0; k < 4; ++k) {
        const float* pl = &dw.tetPlanes[16 * t + 4 * std::size_t(k)];
        if (internal::porousPlaneDistance(pl, d.points[std::size_t(d.tets.verts[4 * t + std::size_t(k)])]) >= 0)
          ++bad;
        const std::int32_t u = d.tets.neighbours[4 * t + std::size_t(k)];
        if (u < 0) continue;
        for (int j = 0; j < 4; ++j)
          if (d.tets.neighbours[4 * std::size_t(u) + std::size_t(j)] == std::int32_t(t))
            for (int a = 0; a < 4; ++a)
              if (dw.tetPlanes[16 * std::size_t(u) + 4 * std::size_t(j) + std::size_t(a)] != -pl[a]) ++bad;
      }
    CHECK(bad == 0);
  }
}

TEST_CASE("porous: the bin query equals brute force in all four cases", "[porous]") {
  int set = 0;
  for (const SeedSet& seeds : {sphereSeeds(), gridSeeds()}) {
    ++set;
    const PorousDual dual = internal::derivePorousDual(seeds);
    const Brute brute(seeds, dual);
    // 10k points over the padded box and a margin beyond it.
    const double margin = 0.5 * seeds.maxSpacing;
    const BBox probe{seeds.box.min - Vector3{margin, margin, margin},
                     seeds.box.max + Vector3{margin, margin, margin}};
    const std::vector<Vector3> pts = randomIn(probe, 10000, 30 + std::uint64_t(set));
    for (PorousCells cells : kCells)
      for (PorousMode mode : kModes) {
        const PorousComplex c = internal::packPorousComplex(dual, seeds, cells, mode);
        for (double k : {0.0, 0.3 * seeds.minSpacing}) {
          INFO((set == 1 ? "sphere" : "grid") << ", " << caseName(cells, mode) << ", k = " << k);
          internal::PorousQueryStats st;
          long mismatch = 0, notExact = 0, clampedInVolume = 0;
          double worst = 0.0, worstGeometric = 0.0;
          for (const Vector3& p : pts) {
            const double q = internal::porousDistance(c, p, k, &st);
            REQUIRE(std::isfinite(q));
            REQUIRE(q >= 0.0);
            double ref;
            if (mode == PorousMode::Struts) {
              ref = Brute::struts(c, p, k);
              // In a cell of a volume seed, the bound never clamps: the bin
              // is at least the largest cell radius.
              if (!seeds.isPad[std::size_t(brute.nearestSeed(p))] && q >= double(c.bound))
                ++clampedInVolume;
            } else if (cells == PorousCells::Voronoi) {
              ref = brute.voronoiWalls(c, p, k, false);
              if (k == 0.0) {
                const double all = brute.voronoiWalls(c, p, 0.0, true);
                worstGeometric = std::max(worstGeometric, std::fabs(all - q));
              }
            } else {
              ref = brute.delaunayWalls(c, p, k);
              if (k == 0.0 && &p - pts.data() < 2000)  // every face: the slow one
                worstGeometric = std::max(worstGeometric, std::fabs(brute.delaunayFaces(p) - q));
            }
            if (q != ref) ++notExact;
            worst = std::max(worst, std::fabs(q - ref));
            // Delaunay walls: the walk on float planes and the exact location
            // may pick the two tets of a face p is within rounding of.
            const double tol = cells == PorousCells::Delaunay && mode == PorousMode::Walls ? 1e-6 : 0.0;
            if (std::fabs(q - ref) > tol) ++mismatch;
          }
          INFO("walk steps " << st.walkSteps << " (max " << st.maxWalkSteps << "), candidates "
                             << st.candidates << ", not bit-equal " << notExact << ", worst "
                             << worst << ", geometric " << worstGeometric);
          CHECK(mismatch == 0);
          CHECK(st.walkCapHits == 0);
          CHECK(worstGeometric < 1e-6);
          CHECK(clampedInVolume == 0);
          if (cells == PorousCells::Voronoi || mode == PorousMode::Struts) CHECK(notExact == 0);
        }
      }
  }
}

TEST_CASE("porous: the complex is deterministic and pinned", "[porous]") {
  const SeedSet seeds = sphereSeeds();
  std::uint64_t got[4];
  int i = 0;
  for (PorousCells cells : kCells)
    for (PorousMode mode : kModes) {
      const PorousComplex a = internal::buildPorousComplex(seeds, cells, mode);
      const PorousComplex b = internal::buildPorousComplex(seeds, cells, mode);
      CHECK(digest(a) == digest(b));
      got[i++] = digest(a);
    }
  // Pinned, so a CI OS whose packed arrays differ fails here.
  CHECK(got[0] == 0x170305c0a0289c47ULL);  // voronoi walls
  CHECK(got[1] == 0xb9d20f69d696dd95ULL);  // voronoi struts
  CHECK(got[2] == 0x894e2d7c2db2acaaULL);  // delaunay walls
  CHECK(got[3] == 0x0dbc0edbb9ab1ec5ULL);  // delaunay struts
}

TEST_CASE("porous: the complex rejects a bad seed set and honours cancel", "[porous]") {
  SeedSet s = sphereSeeds();
  SeedSet noPad = s;
  noPad.isPad.pop_back();
  CHECK_THROWS_AS(internal::buildPorousComplex(noPad, PorousCells::Voronoi, PorousMode::Walls),
                  std::invalid_argument);
  SeedSet noSpacing = s;
  noSpacing.minSpacing = 0.0;
  CHECK_THROWS_AS(internal::buildPorousComplex(noSpacing, PorousCells::Voronoi, PorousMode::Walls),
                  std::invalid_argument);
  const PorousDual d = internal::derivePorousDual(s);
  CancelToken token;
  token.request();
  CHECK_THROWS_AS(internal::derivePorousDual(s, &token), Cancelled);
  for (PorousCells cells : kCells)
    for (PorousMode mode : kModes)
      CHECK_THROWS_AS(internal::packPorousComplex(d, s, cells, mode, &token), Cancelled);
}

TEST_CASE("porous: a capped bin grid gets coarser bins, still exact", "[porous]") {
  // Two clusters at opposite corners of a box 30 wide: 300^3 bins of the
  // spacing, against a cap lowered to 4096 (the library's is 2048 x 8192).
  SeedSet s;
  for (int c = 0; c < 2; ++c)
    for (const Vector3& p : randomCube(150, 40 + std::uint64_t(c)))
      s.points.push_back(p + Vector3{30.0 * c, 30.0 * c, 30.0 * c});
  s.isPad.assign(s.points.size(), 0);
  s.minSpacing = s.maxSpacing = s.pad = 0.1;
  s.box = BBox{{-0.1, -0.1, -0.1}, {31.1, 31.1, 31.1}};
  const PorousDual d = internal::derivePorousDual(s);
  const Brute brute(s, d);
  std::vector<Vector3> pts = randomCube(300, 50);
  for (std::size_t i = 0; i < 150; ++i) pts[i] = pts[i] + Vector3{30.0, 30.0, 30.0};
  for (PorousCells cells : kCells)
    for (PorousMode mode : kModes) {
      INFO(caseName(cells, mode));
      const PorousComplex c = internal::packPorousComplex(d, s, cells, mode, nullptr, 4096.0);
      CHECK(c.bins.count() <= 4096);
      CHECK(c.bins.size > 0.1f);
      if (mode == PorousMode::Struts) CHECK(c.bound == c.bins.size);
      long mismatch = 0;
      for (const Vector3& p : pts) {
        const double q = internal::porousDistance(c, p, 0.02);
        const double ref = mode == PorousMode::Struts ? Brute::struts(c, p, 0.02)
                           : cells == PorousCells::Voronoi ? brute.voronoiWalls(c, p, 0.02, false)
                                                           : brute.delaunayWalls(c, p, 0.02);
        mismatch += std::fabs(q - ref) > 1e-6;
      }
      CHECK(mismatch == 0);
    }
}

TEST_CASE("porous: complex sizes and query rates", "[.][porous-bench]") {
  BoxField box({-0.65, -0.65, -0.65}, {0.65, 0.65, 0.65});
  auto ctl = xControl();
  SeedParams uniform;
  uniform.spacing = 0.0245;
  SeedParams graded;
  graded.spacing = 0.03;
  graded.spacing2 = 0.06;
  graded.d0 = -0.5;
  graded.d1 = 0.5;
  const std::pair<const char*, SeedSet> sets[2] = {
      {"poisson box +-0.65, spacing 0.0245", generatePorousSeeds(box, nullptr, uniform)},
      {"poisson box +-0.65, graded 0.03 -> 0.06 along x",
       generatePorousSeeds(box, ctl.get(), graded)}};
  for (const auto& [name, seeds] : sets) {
    auto t0 = std::chrono::steady_clock::now();
    const PorousDual d = internal::derivePorousDual(seeds);
    auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
    const double deriveMs = ms(t0, std::chrono::steady_clock::now());
    std::ostringstream os;
    os << name << ": " << seeds.points.size() << " seeds, " << d.tets.size() << " tets, "
       << d.faceTets.size() / 2 << " faces, " << d.edges.size() / 2 << " edges, "
       << d.clampedCount << " clamped circumcentres; derive " << deriveMs << " ms";
    const std::vector<Vector3> pts = randomIn(seeds.box, 200000, 77);
    for (PorousCells cells : kCells)
      for (PorousMode mode : kModes) {
        t0 = std::chrono::steady_clock::now();
        const PorousComplex c = internal::packPorousComplex(d, seeds, cells, mode);
        const double packMs = ms(t0, std::chrono::steady_clock::now());
        const auto& st = c.stats;
        os << "\n  " << caseName(cells, mode) << ": pack " << packMs << " ms, "
           << c.packedBytes() << " B = " << double(c.packedBytes()) / seeds.points.size()
           << " B/seed; bins " << c.bins.dims[0] << "x" << c.bins.dims[1] << "x"
           << c.bins.dims[2] << " of " << c.bins.size;
        const double n = double(seeds.points.size());
        os << "\n    per seed: seeds " << c.seeds.size() / n << ", neighbourOffsets "
           << c.neighbourOffsets.size() / n << ", neighbours " << c.neighbours.size() / n
           << ", tetPlanes " << c.tetPlanes.size() / n << ", tetNeighbours "
           << c.tetNeighbours.size() / n << ", binStart " << c.binStart.size() / n
           << ", segments " << c.segments.size() / n << ", binOffsets "
           << c.binOffsets.size() / n << ", binItems " << c.binItems.size() / n
           << " (array elements)";
        if (mode == PorousMode::Struts)
          os << ", bound " << c.bound << " (largest cell radius " << st.largestCellRadius
             << ", max spacing " << seeds.maxSpacing << "), " << st.segments << " segments ("
             << st.candidateSegments << " candidates, dropped " << st.droppedOutside
             << " outside, " << st.droppedZeroLength << " zero-length, " << st.droppedDuplicate
             << " duplicate), items per bin mean " << double(st.binItems) / st.bins << " max "
             << st.maxBinItems;
        for (double k : {0.0, 0.3 * seeds.minSpacing}) {
          internal::PorousQueryStats qs;
          double sum = 0.0;
          t0 = std::chrono::steady_clock::now();
          for (const Vector3& p : pts) sum += internal::porousDistance(c, p, k, &qs);
          const double qMs = ms(t0, std::chrono::steady_clock::now());
          os << "\n    k = " << k << ": " << pts.size() / qMs * 1e3 << " queries/s, mean walk "
             << double(qs.walkSteps) / pts.size() << " (max " << qs.maxWalkSteps << ", cap hits "
             << qs.walkCapHits << "), mean candidates " << double(qs.candidates) / pts.size()
             << " (sum " << sum << ")";
        }
      }
    WARN(os.str());
  }
}

// --- The porous field (#65) ---------------------------------------------------

namespace {

// The field cases' volume and seeds: sphereSeeds()'s, so the field's complex
// is the one packPorousComplex makes from them.
PorousParams sphereParams(PorousCells cells, PorousMode mode, double smooth) {
  PorousParams p;
  p.seeds.spacing = 0.2;
  p.seeds.seed = 21;
  p.cells = cells;
  p.mode = mode;
  p.smooth = smooth;
  return p;
}
SphereField sphereVolume() { return SphereField({0.1, -0.2, 0.05}, 1.0); }

// The distances of the primitives the value at p is folded from, each once:
// the nearest seed's bisectors, the containing tet's faces (none outside the
// hull), or every segment within B.
std::vector<double> primitiveDistances(const Brute& br, const PorousComplex& c,
                                       const Vector3& p) {
  std::vector<double> d;
  if (c.mode == PorousMode::Struts) {
    const double B = double(c.bound);
    for (std::size_t s = 0; s < c.segments.size() / 8; ++s) {
      const float* v = &c.segments[8 * s];
      const double e = internal::porousSegmentDistance(p, internal::porousPackedPoint(v),
                                                       internal::porousPackedPoint(v + 4));
      if (e < B) d.push_back(e);
    }
  } else if (c.cells == PorousCells::Voronoi) {
    const std::int32_t s = br.nearestSeed(p);
    const Vector3 ps = internal::porousPackedPoint(&c.seeds[4 * std::size_t(s)]);
    for (std::int32_t j : br.tetNeighbours[std::size_t(s)])
      d.push_back(internal::porousBisectorDistance(
          p, ps, internal::porousPackedPoint(&c.seeds[4 * std::size_t(j)])));
  } else {
    const std::int32_t t = br.locate(p);
    if (t >= 0)
      for (int f = 0; f < 4; ++f)
        d.push_back(-internal::porousPlaneDistance(
            &c.tetPlanes[16 * std::size_t(t) + 4 * std::size_t(f)], p));
  }
  return d;
}

Vector3 randomUnit(Pcg32& rng) {
  for (;;) {
    const Vector3 v{2 * rng.nextDouble() - 1, 2 * rng.nextDouble() - 1, 2 * rng.nextDouble() - 1};
    const double n = std::sqrt(dot3(v, v));
    if (n > 0.1 && n <= 1.0) return v * (1.0 / n);
  }
}

} // namespace

TEST_CASE("porous: the field is 1-Lipschitz on random pairs", "[porous]") {
  const SphereField volume = sphereVolume();
  BBox box = volume.bounds();
  box.min -= Vector3{0.3, 0.3, 0.3};
  box.max += Vector3{0.3, 0.3, 0.3};
  const double lengths[] = {1e-4, 1e-3, 1e-2, 0.05, 0.3};
  for (PorousCells cells : kCells)
    for (PorousMode mode : kModes)
      for (double k : {0.0, 0.06}) {
        const PorousField f(volume, nullptr, sphereParams(cells, mode, k));
        INFO(caseName(cells, mode) << ", smooth " << k);
        Pcg32 rng(5, 11);
        double worst = 0.0;
        const std::vector<Vector3> pts = randomIn(box, 20000, 31);
        for (std::size_t i = 0; i < pts.size(); ++i) {
          const Vector3 p = pts[i];
          const Vector3 q = p + randomUnit(rng) * lengths[i % 5];
          const double fp = f.valueAt(p), fq = f.valueAt(q);
          REQUIRE(fp >= 0.0);
          const double l = std::sqrt(dist2(p, q));
          worst = std::max(worst, std::fabs(fp - fq) / l);
          REQUIRE(std::fabs(fp - fq) <= l * (1.0 + 1e-6) + 1e-9);
        }
        REQUIRE(worst > 0.5);  // the pairs do probe the slope
        if (mode == PorousMode::Struts) {
          // Far from every segment the struts sit at their ceiling, B - k.
          const double B = f.stats().bound;
          REQUIRE(B > 0.0);
          REQUIRE(f.valueAt(Vector3{9.0, 9.0, 9.0}) == B - k);
        }
      }
}

TEST_CASE("porous: smooth only lowers the value by < k and by k/4 at two primitives",
          "[porous]") {
  // The defaults table's bound (23/01): a fold of two primitives within k of
  // the minimum lowers it by at most k/4; any fold by less than k. Struts are
  // held at B - k, so there the bound is read where m + k < B (the exact
  // region); beyond, the ceiling lowers B to B - k, by exactly k.
  const SphereField volume = sphereVolume();
  const SeedSet seeds = sphereSeeds();
  const PorousDual dual = internal::derivePorousDual(seeds);
  const Brute br(seeds, dual);
  const double k = 0.06;
  for (PorousCells cells : kCells)
    for (PorousMode mode : kModes) {
      INFO(caseName(cells, mode));
      const PorousComplex c = internal::packPorousComplex(dual, seeds, cells, mode);
      const PorousField f0(volume, nullptr, sphereParams(cells, mode, 0.0));
      const PorousField fk(volume, nullptr, sphereParams(cells, mode, k));
      REQUIRE(f0.stats().seeds == seeds.points.size());
      REQUIRE(f0.stats().packedBytes == c.packedBytes());
      const double B = double(c.bound);
      std::size_t two = 0, exact = 0;
      for (const Vector3& p : randomIn(volume.bounds(), 3000, 41)) {
        const double v0 = f0.valueAt(p), vk = fk.valueAt(p);
        // The field is the packed query (struts: under their ceiling).
        if (mode == PorousMode::Walls) {
          REQUIRE(v0 == internal::porousDistance(c, p, 0.0));
          REQUIRE(vk == internal::porousDistance(c, p, k));
        } else {
          REQUIRE(vk == std::min(internal::porousDistance(c, p, k), B - k));
        }
        REQUIRE(vk <= v0);
        std::vector<double> d = primitiveDistances(br, c, p);
        if (d.empty()) continue;
        const double m = *std::min_element(d.begin(), d.end());
        if (mode == PorousMode::Struts && !(m + k < B)) {
          REQUIRE(v0 - vk <= k);
          continue;
        }
        ++exact;
        REQUIRE(v0 - vk < k);
        const auto within = std::count_if(d.begin(), d.end(), [&](double e) { return e < m + k; });
        if (within == 2) {
          ++two;
          REQUIRE(v0 - vk <= 0.25 * k * (1.0 + 1e-9));
        }
      }
      REQUIRE(exact > 1000);
      REQUIRE(two > 50);
    }
}

TEST_CASE("porous: the analytic gradient matches a central difference", "[porous]") {
  const SphereField volume = sphereVolume();
  for (PorousCells cells : kCells)
    for (PorousMode mode : kModes)
      for (double k : {0.0, 0.06}) {
        INFO(caseName(cells, mode) << ", smooth " << k);
        const PorousField f(volume, nullptr, sphereParams(cells, mode, k));
        std::size_t agree = 0, n = 0;
        for (const Vector3& p : randomIn(volume.bounds(), 2000, 51)) {
          const Vector3 g = f.trueGradientAt(p);
          REQUIRE(g.norm() <= 1.0 + 1e-6);
          const Vector3 u = f.gradientAt(p);
          const double un = u.norm();
          REQUIRE((std::fabs(un - 1.0) < 1e-9 || un == 0.0));
          if (g.norm() > 1e-12) REQUIRE(dot3(u, g) > 0.0);
          else REQUIRE(u == Vector3{0.0, 0.0, 1.0});
          const Vector3 fd = internal::fdTrueGradient(f, p);
          ++n;
          if ((g - fd).norm() < 1e-3) ++agree;
        }
        // Only a point within the step of a kink (a wall, a cell edge, a
        // clamp) disagrees.
        REQUIRE(agree >= n * 97 / 100);
      }
}

TEST_CASE("porous: a mesh volume is read at build time only", "[porous]") {
  auto [mesh, geom] = dce::makeTorus(1.0, 0.35, 48, 24);
  std::unique_ptr<PorousField> f;
  BBox torusBox;
  std::vector<Vector3> pts;
  std::vector<char> inside;
  {
    MeshSource torus(*mesh, *geom);
    torusBox = torus.bounds();
    pts = randomIn(torusBox, 3000, 61);
    for (const Vector3& p : pts) inside.push_back(torus.valueAt(p) < 0.0);
    PorousParams p;
    p.seeds.spacing = 0.12;
    p.cells = PorousCells::Voronoi;
    p.mode = PorousMode::Walls;
    f = std::make_unique<PorousField>(torus, nullptr, p);
  }  // the volume is gone; the field keeps what it packed
  mesh.reset();
  geom.reset();
  REQUIRE(f->bounds().min == torusBox.min);
  REQUIRE(f->bounds().max == torusBox.max);
  const PorousField::Stats& st = f->stats();
  REQUIRE(st.seeds > 500);
  REQUIRE(st.padSeeds > 0);
  REQUIRE(st.padSeeds < st.seeds);
  REQUIRE(st.tets > st.seeds);
  Pcg32 rng(3, 4);
  std::size_t zeroish = 0;
  for (std::size_t i = 0; i < pts.size(); ++i) {
    const Vector3 p = pts[i];
    const Vector3 q = p + randomUnit(rng) * 0.01;
    const double fp = f->valueAt(p);
    REQUIRE(fp >= 0.0);
    // Inside, a wall is nearer than the nearest seed, which blue noise keeps
    // within 1.16 spacings (the study's largest empty sphere).
    if (inside[i]) REQUIRE(fp < 1.2 * 0.12);
    REQUIRE(std::fabs(fp - f->valueAt(q)) <= 0.01 * (1.0 + 1e-6) + 1e-9);
    if (fp < 0.01) ++zeroish;
  }
  REQUIRE(zeroish > 100);
}

TEST_CASE("porous: the field is deterministic and rejects a bad smooth or a cancel",
          "[porous]") {
  const SphereField volume = sphereVolume();
  const PorousField a(volume, nullptr, sphereParams(PorousCells::Delaunay, PorousMode::Struts, 0.03));
  const PorousField b(volume, nullptr, sphereParams(PorousCells::Delaunay, PorousMode::Struts, 0.03));
  for (const Vector3& p : randomIn(volume.bounds(), 500, 71)) REQUIRE(a.valueAt(p) == b.valueAt(p));

  REQUIRE_THROWS_AS(PorousField(volume, nullptr, sphereParams(PorousCells::Voronoi, PorousMode::Walls, -0.1)),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      PorousField(volume, nullptr,
                  sphereParams(PorousCells::Voronoi, PorousMode::Walls,
                               std::numeric_limits<double>::infinity())),
      std::invalid_argument);
  // Struts: a smooth at B (about 1.2 x the spacing) would leave 0 everywhere.
  try {
    PorousField(volume, nullptr, sphereParams(PorousCells::Voronoi, PorousMode::Struts, 0.5));
    FAIL("expected std::invalid_argument");
  } catch (const std::invalid_argument& e) {
    REQUIRE(std::string(e.what()).find("bound B") != std::string::npos);
  }
  // Walls take any smooth.
  REQUIRE_NOTHROW(PorousField(volume, nullptr, sphereParams(PorousCells::Voronoi, PorousMode::Walls, 0.5)));
  // What the seeds reject comes through.
  PorousParams bad = sphereParams(PorousCells::Voronoi, PorousMode::Walls, 0.0);
  bad.seeds.spacing = 0.0;
  REQUIRE_THROWS_AS(PorousField(volume, nullptr, bad), std::invalid_argument);

  CancelToken cancel;
  cancel.request();
  REQUIRE_THROWS_AS(
      PorousField(volume, nullptr, sphereParams(PorousCells::Voronoi, PorousMode::Walls, 0.0), &cancel),
      Cancelled);
}

// Why the field holds the struts at B - k and not at porousDistance's B
// (src/implicit/porous.cpp): the largest slope of each, on close pairs; then
// the field's build time and query rates (roadmap 23/02 #65).
TEST_CASE("porous: the struts' ceiling and the raw query's slope", "[.][porous-bench]") {
  const SeedSet seeds = sphereSeeds();
  const SphereField volume = sphereVolume();
  std::ostringstream os;
  for (PorousCells cells : kCells) {
    const double k = 0.06;
    const PorousComplex c = internal::buildPorousComplex(seeds, cells, PorousMode::Struts);
    const PorousField f(volume, nullptr, sphereParams(cells, PorousMode::Struts, k));
    Pcg32 rng(9, 9);
    double raw = 0.0, field = 0.0;
    for (const Vector3& p : randomIn(volume.bounds(), 400000, 81)) {
      const Vector3 q = p + randomUnit(rng) * 1e-5;
      raw = std::max(raw, std::fabs(internal::porousDistance(c, p, k) -
                                    internal::porousDistance(c, q, k)) / 1e-5);
      field = std::max(field, std::fabs(f.valueAt(p) - f.valueAt(q)) / 1e-5);
    }
    os << "\n" << caseName(cells, PorousMode::Struts) << ", k = " << k << ", B = " << c.bound
       << ": largest slope, raw query " << raw << ", field " << field;
  }
  // The field's build and its rates: value, analytic gradient, central
  // difference, on the analytic unit sphere at spacing 0.05.
  const SphereField unit({0.0, 0.0, 0.0}, 1.0);
  auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
  const std::vector<Vector3> pts = randomIn(unit.bounds(), 200000, 91);
  for (PorousCells cells : kCells)
    for (PorousMode mode : kModes) {
      PorousParams pp;
      pp.seeds.spacing = 0.05;
      pp.cells = cells;
      pp.mode = mode;
      pp.smooth = 0.015;
      auto t0 = std::chrono::steady_clock::now();
      const PorousField f(unit, nullptr, pp);
      const double build = ms(t0, std::chrono::steady_clock::now());
      double sum = 0.0;
      t0 = std::chrono::steady_clock::now();
      for (const Vector3& p : pts) sum += f.valueAt(p);
      const double tv = ms(t0, std::chrono::steady_clock::now());
      t0 = std::chrono::steady_clock::now();
      for (const Vector3& p : pts) sum += f.trueGradientAt(p).x;
      const double tg = ms(t0, std::chrono::steady_clock::now());
      t0 = std::chrono::steady_clock::now();
      for (const Vector3& p : pts) sum += internal::fdTrueGradient(f, p).x;
      const double tf = ms(t0, std::chrono::steady_clock::now());
      const double n = double(pts.size());
      os << "\n" << caseName(cells, mode) << ", spacing 0.05, smooth 0.015: " << f.stats().seeds
         << " seeds, " << f.stats().tets << " tets, build " << build << " ms; per second: value "
         << n / tv * 1e3 << ", analytic gradient " << n / tg * 1e3 << ", central difference "
         << n / tf * 1e3 << " (sum " << sum << ")";
    }
  WARN(os.str());
}
