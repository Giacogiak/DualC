#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include "example_common.h"  // dce::strutKinds / strutCellSegments / makeStrutLattice

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <tuple>
#include <vector>

using namespace dualc;
using Catch::Approx;

namespace {

// Ground-truth (oracle) value of a strut lattice at `p`: the true infinite
// union of capsules, evaluated by tiling the unit-cell segments MANUALLY over a
// wide neighbour range and taking the exact min. This is deliberately
// independent of makeStrutLattice's `repeated` tiling: a tiling that drops a
// strut makes the oracle min strictly smaller here, and the comparisons below
// catch it where they sample (see seamPoints()).
double oracleValue(const std::string& kind, double wavelength, double radius,
                   const Vector3& p) {
  const auto segs = dce::strutCellSegments(kind, wavelength);
  double best = std::numeric_limits<double>::infinity();
  // p lies within a few tenths of a cell of its nearest struts; +-3 cells about
  // the tile p falls in is far more than enough.
  const int k0x = static_cast<int>(std::round(p.x / wavelength));
  const int k0y = static_cast<int>(std::round(p.y / wavelength));
  const int k0z = static_cast<int>(std::round(p.z / wavelength));
  for (int kx = k0x - 3; kx <= k0x + 3; ++kx)
    for (int ky = k0y - 3; ky <= k0y + 3; ++ky)
      for (int kz = k0z - 3; kz <= k0z + 3; ++kz) {
        const Vector3 off{kx * wavelength, ky * wavelength, kz * wavelength};
        for (const auto& s : segs) {
          CapsuleField cap(s.first + off, s.second + off, radius);
          best = std::min(best, cap.valueAt(p));
        }
      }
  return best;
}

// Tapered variant of the oracle: the same independent wide-neighbour tiling, but
// each segment is the union of TWO round cones split at its midpoint (fat
// nodeRadius at the end-nodes, thin radius mid-span) -- mirroring the tapered
// makeStrutLattice cell without reusing its `repeated` fold. The fat node caps
// overhang the seams further than plain capsules, so the tiling must stay
// exact there too.
double oracleValueTapered(const std::string& kind, double wavelength,
                          double radius, double nodeRadius, const Vector3& p) {
  const auto segs = dce::strutCellSegments(kind, wavelength);
  double best = std::numeric_limits<double>::infinity();
  const int k0x = static_cast<int>(std::round(p.x / wavelength));
  const int k0y = static_cast<int>(std::round(p.y / wavelength));
  const int k0z = static_cast<int>(std::round(p.z / wavelength));
  for (int kx = k0x - 3; kx <= k0x + 3; ++kx)
    for (int ky = k0y - 3; ky <= k0y + 3; ++ky)
      for (int kz = k0z - 3; kz <= k0z + 3; ++kz) {
        const Vector3 off{kx * wavelength, ky * wavelength, kz * wavelength};
        for (const auto& s : segs) {
          const Vector3 a = s.first + off;
          const Vector3 b = s.second + off;
          const Vector3 mid = (a + b) * 0.5;
          RoundConeField c1(a, mid, nodeRadius, radius);
          RoundConeField c2(mid, b, radius, nodeRadius);
          best = std::min(best, std::min(c1.valueAt(p), c2.valueAt(p)));
        }
      }
  return best;
}

// A small LCG so the test is reproducible without <random> plumbing.
struct Lcg {
  std::uint64_t state = 0x9e3779b97f4a7c15ULL;
  double next() {  // [0, 1)
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<double>((state >> 11) & 0x1FFFFF) / 2097152.0;
  }
};

// Deterministic pseudo-random points in [-1.1, 1.1]^3.
std::vector<Vector3> samplePoints(int n) {
  std::vector<Vector3> pts;
  Lcg rng;
  for (int i = 0; i < n; ++i)
    pts.push_back(Vector3{2.2 * rng.next() - 1.1, 2.2 * rng.next() - 1.1,
                          2.2 * rng.next() - 1.1});
  return pts;
}

// Seam-targeted points. A tiling goes wrong, if anywhere, within a cap's
// reach of a seam -- the planes at half-integer multiples of the period -- and
// the strut nodes sit on the seams: the cell corners on three at once, the
// face centres on one. Uniform points rarely land in those slivers. For every node of the cell, in the tile at the
// origin and in the one at (+1, -1, +1), `perNode` points within `reach` of
// it (in a ball), plus `perFace` points on each seam plane of the origin tile,
// within `reach` across it.
std::vector<Vector3> seamPoints(double wavelength, double reach, int perNode,
                                int perFace) {
  const double h = 0.5 * wavelength;
  std::vector<Vector3> nodes = {{0, 0, 0}};
  for (int c = 0; c < 8; ++c)
    nodes.push_back({(c & 1) ? h : -h, (c & 2) ? h : -h, (c & 4) ? h : -h});
  for (int a = 0; a < 3; ++a)
    for (double sgn : {-1.0, 1.0}) {
      Vector3 f{0, 0, 0};
      f[a] = sgn * h;
      nodes.push_back(f);
    }
  std::vector<Vector3> pts;
  Lcg rng;
  auto inBall = [&]() {
    for (;;) {
      const Vector3 d{2 * rng.next() - 1, 2 * rng.next() - 1, 2 * rng.next() - 1};
      if (d.norm2() <= 1.0) return d * reach;
    }
  };
  for (const Vector3& tile : {Vector3{0, 0, 0},
                              Vector3{wavelength, -wavelength, wavelength}})
    for (const Vector3& n : nodes)
      for (int i = 0; i < perNode; ++i) pts.push_back(tile + n + inBall());
  for (int a = 0; a < 3; ++a)
    for (double sgn : {-1.0, 1.0})
      for (int i = 0; i < perFace; ++i) {
        Vector3 p{(2 * rng.next() - 1) * h, (2 * rng.next() - 1) * h,
                  (2 * rng.next() - 1) * h};
        p[a] = sgn * h + (2 * rng.next() - 1) * reach;
        pts.push_back(p);
      }
  return pts;
}

std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeUnitCube() {
  std::vector<Vector3> positions = {
      Vector3{-0.5, -0.5, -0.5}, Vector3{ 0.5, -0.5, -0.5},
      Vector3{ 0.5,  0.5, -0.5}, Vector3{-0.5,  0.5, -0.5},
      Vector3{-0.5, -0.5,  0.5}, Vector3{ 0.5, -0.5,  0.5},
      Vector3{ 0.5,  0.5,  0.5}, Vector3{-0.5,  0.5,  0.5},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
      {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              positions);
}

int boundaryEdgeCount(geometrycentral::surface::SurfaceMesh& m) {
  int b = 0;
  for (auto e : m.edges())
    if (e.isBoundary()) ++b;
  return b;
}

} // namespace

TEST_CASE("Strut unit cells have the expected segment counts", "[strut]") {
  // sc: 3 axis rods; bcc: 8 body diagonals; fcc: 6 faces x 4 corners = 24;
  // octet: fcc 24 + 12 octahedral edges = 36.
  REQUIRE(dce::strutCellSegments("sc", 1.0).size() == 3u);
  REQUIRE(dce::strutCellSegments("bcc", 1.0).size() == 8u);
  REQUIRE(dce::strutCellSegments("fcc", 1.0).size() == 24u);
  REQUIRE(dce::strutCellSegments("octet", 1.0).size() == 36u);
  REQUIRE(dce::strutCellSegments("nonsense", 1.0).empty());
}

TEST_CASE("makeStrutLattice returns null for an unknown crystal", "[strut]") {
  REQUIRE(dce::makeStrutLattice("nonsense", Vector3{0, 0, 0}, 1.0, 0.1) ==
          nullptr);
}

// Uniform points plus the seam-targeted ones, `reach` being how far a cap
// overhangs a seam.
std::vector<Vector3> gatePoints(double wavelength, double reach) {
  std::vector<Vector3> pts = samplePoints(400);
  const auto seam = seamPoints(wavelength, reach, 12, 40);
  pts.insert(pts.end(), seam.begin(), seam.end());
  return pts;
}

// The tiling gate: the tiled field must equal the true infinite union at every
// sampled point, for every crystal -- 1,000 points, 600 of them within a cap's
// reach of a seam or a node. What it does NOT certify (roadmap 17, record 15):
// these four cells are mirror-symmetric about their own faces, which makes
// even a single-tile fold exact in value -- with RepeatField forced back to
// one, this gate still passes, and so it should. The 2026-09-10 RepeatField
// defect (an off-centre child reaching across a seam) is pinned by
// test_domain_ops.cpp, not here. The manifold end-to-end test below CANNOT
// catch a dropped strut (each capsule is independently closed, so a sparser
// lattice is still watertight), which is why this value-level oracle exists.
TEST_CASE("Tiled strut lattice equals the explicit infinite union", "[strut]") {
  const double wavelength = 0.5;
  const double radius = 0.06;
  const auto pts = gatePoints(wavelength, 2.0 * radius);

  for (const auto& kind : dce::strutKinds()) {
    FieldPtr lattice =
        dce::makeStrutLattice(kind.name, Vector3{0, 0, 0}, wavelength, radius);
    REQUIRE(lattice != nullptr);
    double worst = 0.0;
    for (const Vector3& p : pts) {
      const double got = lattice->valueAt(p);
      const double want = oracleValue(kind.name, wavelength, radius, p);
      worst = std::max(worst, std::abs(got - want));
    }
    INFO("crystal = " << kind.name << ", worst |tiled - union| = " << worst);
    REQUIRE(worst < 1e-9);
  }
}

// The tapered counterpart of the tiling gate: the two-round-cone split must tile
// exactly too. A dropped tapered boundary strut makes the independent oracle
// strictly smaller near a seam and fails this REQUIRE.
TEST_CASE("Tapered strut lattice equals the explicit two-cone union", "[strut]") {
  const double wavelength = 0.5;
  const double radius = 0.04;
  const double nodeRadius = 0.10;
  const auto pts = gatePoints(wavelength, 2.0 * nodeRadius);

  for (const auto& kind : dce::strutKinds()) {
    FieldPtr lattice = dce::makeStrutLattice(kind.name, Vector3{0, 0, 0},
                                             wavelength, radius, nodeRadius);
    REQUIRE(lattice != nullptr);
    double worst = 0.0;
    for (const Vector3& p : pts) {
      const double got = lattice->valueAt(p);
      const double want =
          oracleValueTapered(kind.name, wavelength, radius, nodeRadius, p);
      worst = std::max(worst, std::abs(got - want));
    }
    INFO("crystal = " << kind.name << ", worst |tiled - union| = " << worst);
    REQUIRE(worst < 1e-9);
  }
}

TEST_CASE("Tapered struts are fatter at the nodes than at mid-span", "[strut]") {
  const double radius = 0.04, nodeRadius = 0.12;
  // bcc body-diagonal strut: node at the cell centre O=(0,0,0), node at the
  // corner (0.5,0.5,0.5), midpoint at (0.25,0.25,0.25). On-axis the signed value
  // is -(local radius): -nodeRadius at a node, -radius at the span midpoint.
  FieldPtr f =
      dce::makeStrutLattice("bcc", Vector3{0, 0, 0}, 1.0, radius, nodeRadius);
  const Vector3 node{0.0, 0.0, 0.0};
  const Vector3 mid{0.25, 0.25, 0.25};
  REQUIRE(f->valueAt(node) == Approx(-nodeRadius).margin(1e-9));
  REQUIRE(f->valueAt(mid) == Approx(-radius).margin(1e-9));
  REQUIRE(f->valueAt(node) < f->valueAt(mid));  // node bulges (more negative)

  // Default (nodeRadius omitted) is the untapered capsule: node == mid == -radius.
  FieldPtr plain = dce::makeStrutLattice("bcc", Vector3{0, 0, 0}, 1.0, radius);
  REQUIRE(plain->valueAt(node) == Approx(-radius).margin(1e-9));
  REQUIRE(plain->valueAt(mid) == Approx(-radius).margin(1e-9));
}

TEST_CASE("End-to-end: strut lattices clipped to a unit cube are manifold",
          "[strut][pipeline]") {
  auto [mesh, geom] = makeUnitCube();
  auto src = std::make_shared<MeshSource>(*mesh, *geom);

  SamplerParams sp;
  sp.maxDepth = 6;
  BBox box;
  box.min = Vector3{-0.6, -0.6, -0.6};
  box.max = Vector3{ 0.6,  0.6,  0.6};
  sp.rootBounds = box;
  ContourerParams cp;

  // Radius 0.07 is well above the depth-6 cell (~0.019), so every strut tube
  // resolves into a clean manifold. Check the two crystals with boundary struts
  // (bcc corners, octet faces) -- the ones most likely to break at cell seams.
  for (const char* kind : {"bcc", "octet"}) {
    FieldPtr lattice =
        dce::makeStrutLattice(kind, Vector3{0, 0, 0}, 0.5, 0.07);
    FieldPtr clipped = intersectionOf(lattice, src);

    auto [outMesh, outGeom, outNormals] = dualContourField(*clipped, sp, cp);
    REQUIRE(outMesh != nullptr);
    REQUIRE(outMesh->nVertices() > 0);
    INFO("crystal = " << kind);
    REQUIRE(boundaryEdgeCount(*outMesh) == 0);
  }
}

TEST_CASE("Strut radius controls thickness; wavelength sets the period",
          "[strut]") {
  // A point exactly on a bcc body-diagonal strut is inside by ~radius.
  FieldPtr thin = dce::makeStrutLattice("bcc", Vector3{0, 0, 0}, 1.0, 0.05);
  FieldPtr thick = dce::makeStrutLattice("bcc", Vector3{0, 0, 0}, 1.0, 0.20);
  const Vector3 onStrut{0.25, 0.25, 0.25};  // midpoint of center->(+,+,+) corner
  REQUIRE(thin->valueAt(onStrut) == Approx(-0.05).margin(1e-9));
  REQUIRE(thick->valueAt(onStrut) == Approx(-0.20).margin(1e-9));

  // Periodicity: stepping one wavelength along an axis recovers the value.
  FieldPtr f = dce::makeStrutLattice("octet", Vector3{0, 0, 0}, 0.5, 0.06);
  const Vector3 p{0.07, 0.13, 0.19};
  REQUIRE(f->valueAt(p + Vector3{0.5, 0, 0}) ==
          Approx(f->valueAt(p)).margin(1e-10));
}

