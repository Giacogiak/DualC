#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include "example_common.h"  // dce::strutCellSegments

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

// Roadmap 17 #35: both repeats skip a neighbour copy that provably cannot win
// the strict `<` minimum -- a copy read twice, or (when the child claims
// boundsBoundValue) a copy whose box is farther than the best so far. The
// claim is that this changes nothing, bit for bit. Two witnesses per case:
// the same repeat over a pass-through child that withholds the claim (the box
// skip off), and a hand-written copy of the 8-neighbour loop that predates the
// skips (no skip at all). Values and gradients must match with ==.

namespace {

// Forwards everything to its child; claims boundsBoundValue only when told
// to, and counts child reads so the test can see the skip actually fires.
// The mutable counter is test-only: the test evaluates on one thread.
class ClaimProbe : public ImplicitField {
public:
  ClaimProbe(FieldPtr child, bool forwardClaim)
      : child_(std::move(child)), forwardClaim_(forwardClaim) {}

  double valueAt(const Vector3& p) const override {
    ++reads;
    return child_->valueAt(p);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    return child_->gradientAt(p);
  }
  Vector3 trueGradientAt(const Vector3& p) const override {
    return child_->trueGradientAt(p);
  }
  BBox bounds() const override { return child_->bounds(); }
  bool boundsBoundValue() const override {
    return forwardClaim_ && child_->boundsBoundValue();
  }
  bool cellOverlaps(const BBox& c) const override {
    return child_->cellOverlaps(c);
  }

  mutable std::size_t reads = 0;

private:
  FieldPtr child_;
  bool     forwardClaim_;
};

double sgnOf(double x) { return x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0); }

// The 8-neighbour minimum as domain_ops.cpp computed it at db50b2e, before
// any copy was skipped: home tile plus the +/-1 neighbour per axis, ids
// clamped into [0, count-1] when `count` is given, first strict minimum kept.
double eightCopyMin(const ImplicitField& child, const Vector3& s,
                    const Vector3i* count, const Vector3& p,
                    Vector3& bestLocal) {
  auto idOf = [](double v, double t) {
    return (t > 0.0) ? std::round(v / t) : 0.0;
  };
  auto clampId = [](double v, int n) {
    const double hi = static_cast<double>(std::max(n - 1, 0));
    return v < 0.0 ? 0.0 : (v > hi ? hi : v);
  };
  const Vector3 id{idOf(p.x, s.x), idOf(p.y, s.y), idOf(p.z, s.z)};
  const Vector3 o{sgnOf(p.x - s.x * id.x), sgnOf(p.y - s.y * id.y),
                  sgnOf(p.z - s.z * id.z)};
  double best = std::numeric_limits<double>::infinity();
  for (int k = 0; k < 2; ++k) {
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 2; ++i) {
        Vector3 rid{id.x + i * o.x, id.y + j * o.y, id.z + k * o.z};
        if (count) {
          rid.x = clampId(rid.x, count->x);
          rid.y = clampId(rid.y, count->y);
          rid.z = clampId(rid.z, count->z);
        }
        const Vector3 r{p.x - s.x * rid.x, p.y - s.y * rid.y,
                        p.z - s.z * rid.z};
        const double d = child.valueAt(r);
        if (d < best) {
          best = d;
          bestLocal = r;
        }
      }
    }
  }
  return best;
}

bool sameVec(const Vector3& a, const Vector3& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

// The octet unit cell example_common's makeStrutLattice tiles -- capsules,
// or (tapered) two round cones per strut -- built here untiled so it can be
// wrapped in a ClaimProbe.
FieldPtr octetCell(double wavelength, double radius, double nodeRadius) {
  FieldPtr cell;
  auto add = [&cell](FieldPtr f) { cell = cell ? unionOf(cell, f) : f; };
  for (const auto& sg : dce::strutCellSegments("octet", wavelength)) {
    if (nodeRadius == radius) {
      add(std::make_shared<CapsuleField>(sg.first, sg.second, radius));
    } else {
      const Vector3 mid = (sg.first + sg.second) * 0.5;
      add(std::make_shared<RoundConeField>(sg.first, mid, nodeRadius, radius));
      add(std::make_shared<RoundConeField>(mid, sg.second, radius, nodeRadius));
    }
  }
  return cell;
}

struct PruneCase {
  const char* name;
  FieldPtr    child;
  Vector3     period;
  bool        limited;
  Vector3i    count;
  BBox        region;
  bool        claims;   // the child claims boundsBoundValue
};

void checkPrunedEqualsUnpruned(const PruneCase& c) {
  INFO("case: " << c.name);
  REQUIRE(c.child->boundsBoundValue() == c.claims);

  auto withClaim = std::make_shared<ClaimProbe>(c.child, true);
  auto without   = std::make_shared<ClaimProbe>(c.child, false);
  FieldPtr pruned = c.limited
      ? repeatedLimited(withClaim, c.period, c.count)
      : repeated(withClaim, c.period);
  FieldPtr unpruned = c.limited
      ? repeatedLimited(without, c.period, c.count)
      : repeated(without, c.period);
  const Vector3i* count = c.limited ? &c.count : nullptr;

  // A 25^3 grid over the region whose coordinates on a repeated axis are
  // exact multiples of period/8 -- so it lands on tile centres, where the
  // neighbour offset is 0 and the repeat skip fires, and on seams -- plus
  // 4,000 pseudo-random points.
  std::vector<Vector3> pts;
  const int n = 24;
  auto coord = [&](int a, int i) {
    const double s = c.period[a];
    const double lo = c.region.min[a], hi = c.region.max[a];
    if (s > 0.0) {
      const double m0 = std::ceil(lo / (s / 8.0));
      const double m1 = std::floor(hi / (s / 8.0));
      return s * ((m0 + std::round((m1 - m0) * i / n)) / 8.0);
    }
    return lo + (hi - lo) * i / n;
  };
  for (int i = 0; i <= n; ++i)
    for (int j = 0; j <= n; ++j)
      for (int k = 0; k <= n; ++k)
        pts.push_back(Vector3{coord(0, i), coord(1, j), coord(2, k)});
  std::uint64_t state = 0x9E3779B97F4A7C15ull;
  auto unit = [&state]() {  // splitmix64, the same on every platform
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return static_cast<double>(z >> 11) * 0x1.0p-53;
  };
  for (int i = 0; i < 4000; ++i) {
    pts.push_back(Vector3{
        c.region.min.x + (c.region.max.x - c.region.min.x) * unit(),
        c.region.min.y + (c.region.max.y - c.region.min.y) * unit(),
        c.region.min.z + (c.region.max.z - c.region.min.z) * unit()});
  }

  std::size_t mismatches = 0;
  withClaim->reads = without->reads = 0;
  for (const Vector3& p : pts) {
    Vector3 local;
    const double ref = eightCopyMin(*c.child, c.period, count, p, local);
    const Vector3 refGrad = c.child->gradientAt(local);
    const Vector3 refTrue = c.child->trueGradientAt(local);
    const bool ok = pruned->valueAt(p) == ref && unpruned->valueAt(p) == ref &&
                    sameVec(pruned->gradientAt(p), refGrad) &&
                    sameVec(unpruned->gradientAt(p), refGrad) &&
                    sameVec(pruned->trueGradientAt(p), refTrue) &&
                    sameVec(unpruned->trueGradientAt(p), refTrue);
    if (!ok && mismatches++ == 0) {
      UNSCOPED_INFO("first mismatch at (" << p.x << ", " << p.y << ", "
                                          << p.z << ")");
    }
  }
  CHECK(mismatches == 0);

  // The skip must actually fire on a claimant, or the test proves nothing.
  if (c.claims) CHECK(withClaim->reads < without->reads);
  else          CHECK(withClaim->reads == without->reads);
}

BBox regionOf(const Vector3& lo, const Vector3& hi) {
  BBox b;
  b.min = lo;
  b.max = hi;
  return b;
}

} // namespace

TEST_CASE("Pruned repeats equal the 8-neighbour minimum bit for bit",
          "[domain][repeat]") {
  const double w = 0.4;
  const Vector3 lattice{w, w, w};
  const BBox latticeBox = regionOf(Vector3{-0.6, -0.6, -0.6}, Vector3{0.6, 0.6, 0.6});
  const std::vector<PruneCase> cases = {
      {"sphere across a seam", std::make_shared<SphereField>(Vector3{0.4, 0.0, 0.0}, 0.3),
       Vector3{1.0, 0.0, 0.0}, false, Vector3i{1, 1, 1},
       regionOf(Vector3{-1.5, -0.5, -0.5}, Vector3{1.5, 0.5, 0.5}), true},
      {"sphere across two seams", std::make_shared<SphereField>(Vector3{0.4, 0.15, 0.0}, 0.3),
       Vector3{1.0, 1.0, 0.0}, false, Vector3i{1, 1, 1},
       regionOf(Vector3{-1.5, -1.5, -0.5}, Vector3{1.5, 1.5, 0.5}), true},
      {"octet cell (capsules)", octetCell(w, 0.05, 0.05), lattice, false,
       Vector3i{1, 1, 1}, latticeBox, true},
      {"tapered octet cell (round cones)", octetCell(w, 0.05, 0.08), lattice, false,
       Vector3i{1, 1, 1}, latticeBox, true},
      {"round box (no claim)",
       std::make_shared<RoundBoxField>(Vector3{-0.1, -0.3, -0.25}, Vector3{0.6, 0.3, 0.2}, 0.05),
       Vector3{1.0, 0.0, 0.4}, false, Vector3i{1, 1, 1},
       regionOf(Vector3{-1.5, -0.5, -1.0}, Vector3{1.5, 0.5, 1.0}), false},
      {"limited sphere", std::make_shared<SphereField>(Vector3{0.4, 0.0, 0.0}, 0.3),
       Vector3{1.0, 0.0, 0.0}, true, Vector3i{3, 1, 1},
       regionOf(Vector3{-1.0, -0.5, -0.5}, Vector3{3.0, 0.5, 0.5}), true},
      {"limited octet cell", octetCell(w, 0.05, 0.05), lattice, true,
       Vector3i{3, 2, 2}, regionOf(Vector3{-0.6, -0.6, -0.6}, Vector3{1.4, 1.0, 1.0}), true},
  };
  for (const PruneCase& c : cases) checkPrunedEqualsUnpruned(c);
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
