// The porous shell's seeds (roadmap 23 #63): the six properties of the raw
// plan's P02 list, on a sphere volume and on a box. Its Delaunay
// tetrahedralization (#64): the four properties of P03 T1. Every case is named
// `porous: ...` so `ctest -R porous` finds it.

#include "dualc/implicit.h"
#include "dualc/porous.h"
#include "dualc/primitives.h"

#include "internal/exact_predicates.h"
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
