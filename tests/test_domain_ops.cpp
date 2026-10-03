#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

using namespace dualc;
using Catch::Approx;

TEST_CASE("mirrored reflects the field across a plane", "[domain]") {
  // Sphere at (2,0,0) mirrored across the x = 0 plane.
  auto sphere = std::make_shared<SphereField>(Vector3{2.0, 0.0, 0.0}, 1.0);
  auto m = mirrored(sphere, Vector3{1.0, 0.0, 0.0});

  REQUIRE(m->valueAt(Vector3{2.0, 0.0, 0.0}) == Approx(-1.0));   // original
  REQUIRE(m->valueAt(Vector3{-2.0, 0.0, 0.0}) == Approx(-1.0));  // reflected
  REQUIRE(m->valueAt(Vector3{0.0, 0.0, 0.0}) > 0.0);             // gap between

  // The field is symmetric about the plane.
  REQUIRE(m->valueAt(Vector3{1.5, 0.3, 0.0}) ==
          Approx(m->valueAt(Vector3{-1.5, 0.3, 0.0})));

  const BBox b = m->bounds();
  REQUIRE(b.min.x == Approx(-3.0));
  REQUIRE(b.max.x == Approx(3.0));
}

TEST_CASE("repeated tiles the field infinitely", "[domain]") {
  auto sphere = std::make_shared<SphereField>(Vector3{0.0, 0.0, 0.0}, 1.0);
  auto r = repeated(sphere, Vector3{4.0, 0.0, 0.0});

  REQUIRE(r->valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(-1.0));   // tile 0
  REQUIRE(r->valueAt(Vector3{4.0, 0.0, 0.0}) == Approx(-1.0));   // tile +1
  REQUIRE(r->valueAt(Vector3{-8.0, 0.0, 0.0}) == Approx(-1.0));  // tile -2
  REQUIRE(r->valueAt(Vector3{2.0, 0.0, 0.0}) == Approx(1.0));    // between tiles

  REQUIRE(r->bounds().isInfinite());
}

// Roadmap 17 #34: `repeated` folds p into the nearest tile and reads the child
// once. That is only the true infinite union when no copy reaches across a tile
// boundary. The existing case above uses a sphere of radius 1 in a period of 4,
// which never exercises it; this one uses a child that is BOTH off-centre and
// wider than half the period, where the fold reads the wrong copy.
TEST_CASE("repeated agrees with the true infinite union across a tile boundary",
          "[domain]") {
  const double period = 1.0;
  const Vector3 centre{0.4, 0.0, 0.0};
  const double radius = 0.3;   // spans x in [0.1, 0.7] -- past the +0.5 seam

  auto sphere = std::make_shared<SphereField>(centre, radius);
  auto r = repeated(sphere, Vector3{period, 0.0, 0.0});

  // Ground truth, independent of the fold: the exact min over a wide range of
  // copies, the same oracle idiom tests/test_strut_lattice.cpp uses.
  auto oracle = [&](const Vector3& p) {
    double best = std::numeric_limits<double>::infinity();
    for (int k = -3; k <= 3; ++k) {
      const Vector3 q{p.x - k * period, p.y, p.z};
      best = std::min(best, (q - centre).norm() - radius);
    }
    return best;
  };

  // The decisive point: x = 0.55 lies inside the home copy (which reaches to
  // 0.7), but round(0.55) == 1 so the fold reads the copy one tile over and
  // reports it as outside. A flipped sign changes a corner sign, which decides
  // whether an octree edge carries Hermite data at all -- changed topology,
  // not a nudged vertex.
  const Vector3 seam{0.55, 0.0, 0.0};
  REQUIRE(oracle(seam) < 0.0);                       // truly inside
  CHECK(r->valueAt(seam) == Approx(oracle(seam)));

  // Swept along the axis, the fold and the truth must agree everywhere.
  for (int i = 0; i <= 40; ++i) {
    const Vector3 p{-1.0 + 0.05 * i, 0.0, 0.0};
    CHECK(r->valueAt(p) == Approx(oracle(p)).margin(1e-12));
  }
}

TEST_CASE("repeatedLimited tiles a finite number of copies", "[domain]") {
  auto sphere = std::make_shared<SphereField>(Vector3{0.0, 0.0, 0.0}, 1.0);
  auto r = repeatedLimited(sphere, Vector3{4.0, 0.0, 0.0}, Vector3i{3, 1, 1});

  REQUIRE(r->valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(-1.0));   // tile 0
  REQUIRE(r->valueAt(Vector3{4.0, 0.0, 0.0}) == Approx(-1.0));   // tile 1
  REQUIRE(r->valueAt(Vector3{8.0, 0.0, 0.0}) == Approx(-1.0));   // tile 2
  REQUIRE(r->valueAt(Vector3{12.0, 0.0, 0.0}) > 0.0);            // no tile 3
  REQUIRE(r->valueAt(Vector3{-4.0, 0.0, 0.0}) > 0.0);            // no tile -1

  // bounds() is finite -- it spans exactly the three tiles.
  const BBox b = r->bounds();
  REQUIRE(b.isValid());
  REQUIRE_FALSE(b.isInfinite());
  REQUIRE(b.min.x == Approx(-1.0));
  REQUIRE(b.max.x == Approx(9.0));
}

TEST_CASE("twisted leaves the twist axis unchanged", "[domain]") {
  auto box = std::make_shared<BoxField>(Vector3{-1.0, -1.0, -1.0},
                                        Vector3{1.0, 1.0, 1.0});
  auto t = twisted(box, 1.0, /*axis=*/1);  // twist about y

  // On the twist axis the rotation is the identity.
  REQUIRE(t->valueAt(Vector3{0.0, 0.5, 0.0}) ==
          Approx(box->valueAt(Vector3{0.0, 0.5, 0.0})));
  REQUIRE(t->valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(-1.0));

  // The perpendicular extent grows to the corner radius hypot(1,1).
  const BBox b = t->bounds();
  REQUIRE(b.max.x == Approx(std::sqrt(2.0)));
  REQUIRE(b.max.y == Approx(1.0));
}

TEST_CASE("bent warps the field and keeps a finite bound", "[domain]") {
  auto box = std::make_shared<BoxField>(Vector3{-1.0, -1.0, -1.0},
                                        Vector3{1.0, 1.0, 1.0});
  auto b = bent(box, 0.5, /*axis=*/0);  // bend driven by x

  // At the origin the bend angle is zero -> identity.
  REQUIRE(b->valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(-1.0));

  const BBox bb = b->bounds();
  REQUIRE(bb.isValid());
  REQUIRE_FALSE(bb.isInfinite());
  REQUIRE(bb.max.x == Approx(std::sqrt(2.0)));
}

TEST_CASE("displaced adds a scalar bump to the field", "[domain]") {
  auto sphere = std::make_shared<SphereField>(Vector3{0.0, 0.0, 0.0}, 1.0);
  auto d = displaced(sphere, [](const Vector3&) { return 0.3; });

  for (const Vector3 p : {Vector3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0},
                          Vector3{2.0, 0.0, 0.0}}) {
    REQUIRE(d->valueAt(p) == Approx(sphere->valueAt(p) + 0.3));
  }
}

TEST_CASE("dualContourField meshes domain operators end-to-end", "[domain]") {
  SamplerParams sp;
  sp.maxDepth = 5;
  ContourerParams cp;

  SECTION("finite repetition") {
    auto sphere = std::make_shared<SphereField>(Vector3{0.0, 0.0, 0.0}, 1.0);
    auto r = repeatedLimited(sphere, Vector3{4.0, 0.0, 0.0}, Vector3i{3, 1, 1});
    auto [mesh, geom, normals] = dualContourField(*r, sp, cp);
    REQUIRE(mesh != nullptr);
    REQUIRE(mesh->nFaces() > 0);
  }

  SECTION("twisted box") {
    auto box = std::make_shared<BoxField>(Vector3{-1.0, -1.0, -1.0},
                                          Vector3{1.0, 1.0, 1.0});
    auto t = twisted(box, 1.5, 1);
    auto [mesh, geom, normals] = dualContourField(*t, sp, cp);
    REQUIRE(mesh != nullptr);
    REQUIRE(mesh->nFaces() > 0);
  }
}

// ===========================================================================
// cellOverlaps conservativeness (the whole wrapper family)
// ===========================================================================
//
// `cellOverlaps` is the sampler's only pruning gate, so a wrapper that warps
// the domain must ask its child about a region CONTAINING the child's image of
// the cell -- any superset is valid, but a subset silently drops geometry.
//
// The check below needs no knowledge of any particular warp. A recording child
// notes (a) every point it is READ at while the wrapper evaluates a cloud of
// samples drawn from the cell, and (b) every box it is ASKED about when the
// wrapper's `cellOverlaps` runs on that same cell. Every read must fall inside
// some asked box. The recorder answers `false` so that wrappers which test
// several candidate boxes (repeat-limited) enumerate all of them instead of
// short-circuiting on the first.

namespace {

struct ProbeLog {
  std::vector<Vector3> reads;
  std::vector<BBox>    asked;
  bool recordReads = false;
  bool recordAsked = false;
};

class ProbeField : public ImplicitField {
public:
  explicit ProbeField(std::shared_ptr<ProbeLog> log) : log_(std::move(log)) {}

  double valueAt(const Vector3& p) const override {
    if (log_->recordReads) log_->reads.push_back(p);
    return 1.0;  // never inside: keeps the base corner test from short-cutting
  }
  Vector3 gradientAt(const Vector3&) const override {
    return Vector3{0.0, 0.0, 1.0};
  }
  BBox bounds() const override {
    BBox b;
    b.min = Vector3{-1e3, -1e3, -1e3};
    b.max = Vector3{ 1e3,  1e3,  1e3};
    return b;
  }
  bool cellOverlaps(const BBox& c) const override {
    if (log_->recordAsked) log_->asked.push_back(c);
    return false;
  }

private:
  std::shared_ptr<ProbeLog> log_;
};

bool boxContains(const BBox& b, const Vector3& p, double eps) {
  for (int a = 0; a < 3; ++a) {
    if (p[a] < b.min[a] - eps || p[a] > b.max[a] + eps) return false;
  }
  return true;
}

// Deterministic low-discrepancy-ish sweep of the cell: a 7^3 grid plus the
// corners, which is enough to expose a bound that is tight in the middle and
// short at an extreme.
std::vector<Vector3> cellSamples(const BBox& cell) {
  std::vector<Vector3> pts;
  const int n = 7;
  for (int i = 0; i <= n; ++i) {
    for (int j = 0; j <= n; ++j) {
      for (int k = 0; k <= n; ++k) {
        const double fx = static_cast<double>(i) / n;
        const double fy = static_cast<double>(j) / n;
        const double fz = static_cast<double>(k) / n;
        pts.push_back(Vector3{cell.min.x + fx * (cell.max.x - cell.min.x),
                              cell.min.y + fy * (cell.max.y - cell.min.y),
                              cell.min.z + fz * (cell.max.z - cell.min.z)});
      }
    }
  }
  return pts;
}

BBox cellAt(const Vector3& lo, const Vector3& hi) {
  BBox b;
  b.min = lo;
  b.max = hi;
  return b;
}

using Wrap = FieldPtr (*)(FieldPtr);

void checkConservative(const char* name, Wrap wrap, const BBox& cell) {
  auto log = std::make_shared<ProbeLog>();
  FieldPtr w = wrap(std::make_shared<ProbeField>(log));

  log->recordReads = true;
  for (const Vector3& p : cellSamples(cell)) w->valueAt(p);
  log->recordReads = false;

  log->recordAsked = true;
  w->cellOverlaps(cell);
  log->recordAsked = false;

  INFO("wrapper: " << name);
  REQUIRE_FALSE(log->reads.empty());
  REQUIRE_FALSE(log->asked.empty());  // it must forward, not answer on its own

  for (const Vector3& q : log->reads) {
    bool covered = false;
    for (const BBox& b : log->asked) {
      if (boxContains(b, q, 1e-9)) { covered = true; break; }
    }
    INFO("read at (" << q.x << ", " << q.y << ", " << q.z
                     << ") is outside every box the child was asked about");
    REQUIRE(covered);
  }
}

FieldPtr wrapMirror(FieldPtr f) { return mirrored(std::move(f), Vector3{1.0, 0.5, 0.0}); }
FieldPtr wrapRepeat(FieldPtr f) { return repeated(std::move(f), Vector3{1.0, 1.3, 0.7}); }
FieldPtr wrapRepeatLimited(FieldPtr f) {
  return repeatedLimited(std::move(f), Vector3{1.0, 1.3, 0.7}, Vector3i{3, 2, 4});
}
FieldPtr wrapTwistSmall(FieldPtr f) { return twisted(std::move(f), 0.6, 1); }
FieldPtr wrapTwistLarge(FieldPtr f) { return twisted(std::move(f), 3.0, 2); }
FieldPtr wrapBendSmall(FieldPtr f)  { return bent(std::move(f), 0.4, 0); }
FieldPtr wrapBendLarge(FieldPtr f)  { return bent(std::move(f), 2.5, 1); }
FieldPtr wrapDisplace(FieldPtr f) {
  return displaced(std::move(f), [](const Vector3& p) { return 0.1 * std::sin(4.0 * p.x); });
}
FieldPtr wrapTransform(FieldPtr f) {
  return transformed(std::move(f),
                     Mat4::translation(Vector3{0.3, -0.7, 1.1}) *
                         Mat4::rotation(Vector3{0.3, 1.0, -0.2}, 0.9));
}
FieldPtr wrapScalePos(FieldPtr f) { return scaled(std::move(f), 2.5); }
FieldPtr wrapScaleNeg(FieldPtr f) { return scaled(std::move(f), -1.7); }
FieldPtr wrapElongate(FieldPtr f) { return elongated(std::move(f), Vector3{0.3, 0.2, 0.6}); }
FieldPtr wrapOffsetOut(FieldPtr f) { return offsetOf(std::move(f), 0.25); }
FieldPtr wrapOffsetIn(FieldPtr f)  { return offsetOf(std::move(f), -0.25); }

} // namespace

TEST_CASE("Domain wrappers ask their child about a superset of what they read",
          "[domain][celloverlaps]") {
  const std::vector<std::pair<const char*, Wrap>> wrappers = {
      {"mirrored", &wrapMirror},
      {"repeated", &wrapRepeat},
      {"repeatedLimited", &wrapRepeatLimited},
      {"twisted (small sweep)", &wrapTwistSmall},
      {"twisted (large sweep)", &wrapTwistLarge},
      {"bent (small sweep)", &wrapBendSmall},
      {"bent (large sweep)", &wrapBendLarge},
      {"displaced", &wrapDisplace},
      {"transformed", &wrapTransform},
      {"scaled (positive)", &wrapScalePos},
      {"scaled (negative)", &wrapScaleNeg},
      {"elongated", &wrapElongate},
      {"offset (outward)", &wrapOffsetOut},
      {"offset (inward)", &wrapOffsetIn},
  };

  // Cells chosen to hit the branches: wholly on one side of the mirror plane
  // and straddling it; inside one repeat tile and spanning a tile seam; small
  // enough for the arc bound and wide enough to force the full-disc fallback;
  // and one far from the origin.
  const std::vector<BBox> cells = {
      cellAt(Vector3{0.10, 0.10, 0.10}, Vector3{0.35, 0.30, 0.28}),
      cellAt(Vector3{-0.20, -0.15, -0.30}, Vector3{0.25, 0.40, 0.10}),
      cellAt(Vector3{0.40, 0.55, 0.20}, Vector3{1.60, 1.90, 1.05}),
      cellAt(Vector3{-3.10, 2.05, -1.90}, Vector3{-2.55, 2.60, -1.20}),
      cellAt(Vector3{-2.00, -2.00, -2.00}, Vector3{2.00, 2.00, 2.00}),
  };

  for (const auto& w : wrappers) {
    for (const BBox& c : cells) {
      checkConservative(w.first, w.second, c);
    }
  }
}

TEST_CASE("offsetOf asks its child about the cell grown by |r|",
          "[domain][celloverlaps]") {
  // The superset check above cannot pin the growth: `offsetOf` reads its
  // child at the very point it is asked, so any forwarded box that contains
  // the cell -- the bare cell included -- covers every read. What the growth
  // is for is a Lipschitz child: the level set {child = r} lies |r| off the
  // child's surface, so a cell that straddles it can be clear of {child = 0}
  // and the child's own (tight) test prunes it. Both signs matter: for r < 0
  // the level set sits inside the child, where growing by max(r, 0) = 0 --
  // the rule `bounds()` correctly uses -- would still prune.
  auto sphere = std::make_shared<SphereField>(Vector3{0, 0, 0}, 1.0);
  const double r = 0.2;

  auto straddling = [](double radius) {
    // A thin cell across the sphere of that radius on the +x axis.
    return cellAt(Vector3{radius - 0.05, -0.05, -0.05},
                  Vector3{radius + 0.05,  0.05,  0.05});
  };

  SECTION("outward offset") {
    const BBox cell = straddling(1.0 + r);
    REQUIRE_FALSE(sphere->cellOverlaps(cell));  // the test is sharp
    REQUIRE(offsetOf(sphere, r)->cellOverlaps(cell));
  }
  SECTION("inward offset") {
    const BBox cell = straddling(1.0 - r);
    REQUIRE_FALSE(sphere->cellOverlaps(cell));
    REQUIRE(offsetOf(sphere, -r)->cellOverlaps(cell));
  }
}

TEST_CASE("A domain op over a TPMS shell still contours watertight",
          "[domain][celloverlaps][pipeline]") {
  // The regression this whole family of overrides exists for. A TPMS reports
  // an overlap unconditionally, because the Lipschitz-1 default under-refines
  // it; before the domain wrappers forwarded `cellOverlaps`, that signal
  // stopped at the wrapper and the sampler pruned cells the surface passes
  // through. The symptom is not subtle -- measured on a depth-7 run of a
  // twisted gyroid shell, the output carried 4,196 boundary edges, i.e. holes.
  //
  // This is also the only configuration in the suite whose refinement the fix
  // changes, so it is what exercises the new mixed-depth transitions in
  // `edgeProc`.
  //
  // Note the root box is padded well outside the clip box: the warp pushes the
  // clipped shell outside its own bounds, and geometry that leaves the root
  // box is cut there, which would show up as boundary edges having nothing to
  // do with pruning.
  BBox clip;
  clip.min = Vector3{-0.5, -0.5, -0.5};
  clip.max = Vector3{ 0.5,  0.5,  0.5};
  BBox root;
  root.min = Vector3{-0.8, -0.8, -0.8};
  root.max = Vector3{ 0.8,  0.8,  0.8};

  auto tpms = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 0.5);
  FieldPtr shell   = onionOf(normalizedOf(tpms), 0.12);
  FieldPtr clipped =
      intersectionOf(shell, std::make_shared<BoxField>(clip.min, clip.max));

  SamplerParams sp;
  sp.maxDepth   = 6;
  sp.rootBounds = root;
  ContourerParams cp;

  auto check = [&](const char* what, FieldPtr f) {
    auto [outMesh, outGeom, outNormals] = dualContourField(*f, sp, cp);
    INFO(what);
    REQUIRE(outMesh != nullptr);
    REQUIRE(outMesh->nVertices() > 0);
    int boundaryEdges = 0;
    for (auto e : outMesh->edges()) {
      if (e.isBoundary()) ++boundaryEdges;
    }
    REQUIRE(boundaryEdges == 0);
  };

  check("twisted", twisted(clipped, 0.3, 1));
  check("mirrored", mirrored(clipped, Vector3{1.0, 0.4, 0.0}));

  // The same signal through a level-set shift: `offset` over a TPMS is the
  // field graph's Quilez shift, and the one decorator that had not forwarded
  // (17 #46). Whether the default test prunes depends on how the surface
  // sits in the coarse cells; this wavelength in this root box is one that
  // did (834 boundary edges before the fix), a denser one happened not to.
  auto dense = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 0.25);
  check("offset", intersectionOf(offsetOf(dense, 0.0),
                                 std::make_shared<BoxField>(clip.min, clip.max)));
}
