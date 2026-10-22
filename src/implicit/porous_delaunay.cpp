// The Delaunay tetrahedralization of the porous seeds (roadmap 23 #64): own
// Bowyer-Watson on Shewchuk's exact predicates (D-74), sequential, Morton
// insertion order, strict-conflict cavity, an infinite vertex for an exact
// hull, canonical output. See src/internal/porous_delaunay.h for the contract
// and docs/roadmap/23-porous-shell/01-study.md (a) for why each choice.
//
// Bit-identical on every OS: the predicates are exact, the only floating
// point outside them is the Morton key (one subtraction, one division, one
// product per axis, compiled without FMA contraction: CMakeLists.txt), and
// every order below is fixed by indices alone.

#include "internal/porous_delaunay.h"

#include "dualc/porous.h"
#include "internal/exact_predicates.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>

namespace dualc {
namespace internal {

namespace {

// The infinite vertex. A tet that names it is a hull tet: its other three
// vertices are a hull face, ordered so that putting a point q in place of the
// infinite vertex gives orient3d > 0 exactly when q lies strictly beyond that
// face, as if the infinite vertex were a point far out on that side.
constexpr int kInf = -1;

// The cancel token is polled every this many insertions.
constexpr std::size_t kPollEvery = 4096;

std::once_flag gPredicatesInit;

[[noreturn]] void fail(const std::string& what) {
  throw std::invalid_argument("porous delaunay: " + what);
}

// Spreads the low 21 bits of v three apart, for a 63-bit Morton key.
std::uint64_t spread21(std::uint64_t v) {
  v &= 0x1fffffull;
  v = (v | v << 32) & 0x1f00000000ffffull;
  v = (v | v << 16) & 0x1f0000ff0000ffull;
  v = (v | v << 8) & 0x100f00f00f00f00full;
  v = (v | v << 4) & 0x10c30c30c30c30c3ull;
  v = (v | v << 2) & 0x1249249249249249ull;
  return v;
}

class Builder {
public:
  explicit Builder(const std::vector<Vector3>& points) : n_(points.size()) {
    P_.resize(3 * n_);
    for (std::size_t i = 0; i < n_; ++i) {
      P_[3 * i]     = points[i].x;
      P_[3 * i + 1] = points[i].y;
      P_[3 * i + 2] = points[i].z;
    }
    // About 6.7 finite tets per point in a random cloud, plus the hull.
    T_.reserve(7 * n_ + 64);
  }

  DelaunayTets run(const CancelToken* cancel) {
    const std::vector<int> order = mortonOrder();
    std::array<int, 4> first = firstTet(order);
    start(first);
    int cur = 0;
    std::size_t inserted = 0;
    for (int i : order) {
      if (i == first[0] || i == first[1] || i == first[2] || i == first[3])
        continue;
      cur = insert(cur, i);
      if (++inserted % kPollEvery == 0 && cancel)
        cancel->throwIfRequested("porous delaunay");
    }
    return output();
  }

private:
  struct Tet {
    std::array<int, 4> v;   // vertices; kInf on a hull tet
    std::array<int, 4> nb;  // the tet across the face opposite v[k]
  };
  struct Face { int tet, slot; };
  struct EdgeKey { std::uint64_t key; int tet, slot; };

  const double* p(int i) const { return &P_[3 * static_cast<std::size_t>(i)]; }

  static int infSlot(const Tet& t) {
    for (int k = 0; k < 4; ++k)
      if (t.v[k] == kInf) return k;
    return -1;
  }

  // orient3d of t's vertices with v[slot] replaced by q. Every other vertex
  // of t must be finite.
  double orientWith(const Tet& t, int slot, const double* q) const {
    const double* a[4];
    for (int k = 0; k < 4; ++k) a[k] = (k == slot) ? q : p(t.v[k]);
    return orient3d(a[0], a[1], a[2], a[3]);
  }

  // Whether q is strictly inside t's circumsphere. A hull tet's "sphere" is
  // the open half-space beyond its face, plus, on the face's plane, the open
  // disc of the face's circumcircle -- which, for q on that plane, is where q
  // is strictly inside the sphere of the finite tet across the face. That
  // makes the two agree on a coplanar q, so the cavity never puts a flat tet
  // on a hull face (the CGAL rule, with Shewchuk's predicates only).
  bool conflicts(int t, const double* q) const {
    const Tet& tt = T_[t];
    const int s = infSlot(tt);
    if (s < 0)
      return insphere(p(tt.v[0]), p(tt.v[1]), p(tt.v[2]), p(tt.v[3]), q) > 0;
    const double o = orientWith(tt, s, q);
    if (o != 0) return o > 0;
    return conflicts(tt.nb[s], q);
  }

  int alloc(const Tet& t) {
    if (!free_.empty()) {
      const int i = free_.back();
      free_.pop_back();
      T_[i]     = t;
      alive_[i] = 1;
      return i;
    }
    T_.push_back(t);
    alive_.push_back(1);
    inCavity_.push_back(0);
    tested_.push_back(0);
    return static_cast<int>(T_.size()) - 1;
  }

  std::vector<int> mortonOrder() const {
    double lo[3], hi[3];
    for (int a = 0; a < 3; ++a) lo[a] = hi[a] = P_[a];
    for (std::size_t i = 0; i < n_; ++i)
      for (int a = 0; a < 3; ++a) {
        lo[a] = std::min(lo[a], P_[3 * i + a]);
        hi[a] = std::max(hi[a], P_[3 * i + a]);
      }
    const double ext = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
    std::vector<std::uint64_t> key(n_, 0);
    if (ext > 0)
      for (std::size_t i = 0; i < n_; ++i) {
        std::uint64_t k = 0;
        for (int a = 0; a < 3; ++a) {
          const double u = (P_[3 * i + a] - lo[a]) / ext * 2097151.0;
          k |= spread21(static_cast<std::uint64_t>(u)) << a;
        }
        key[i] = k;
      }
    std::vector<int> order(n_);
    for (std::size_t i = 0; i < n_; ++i) order[i] = static_cast<int>(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
      return key[a] != key[b] ? key[a] < key[b] : a < b;
    });
    return order;
  }

  bool samePoint(int i, int j) const {
    return P_[3 * i] == P_[3 * j] && P_[3 * i + 1] == P_[3 * j + 1] &&
           P_[3 * i + 2] == P_[3 * j + 2];
  }

  // Exact: three points are collinear iff all three axis projections are.
  bool collinear(int i, int j, int k) const {
    const double* a = p(i);
    const double* b = p(j);
    const double* c = p(k);
    if (orient2d(a, b, c) != 0) return false;          // (x, y)
    if (orient2d(a + 1, b + 1, c + 1) != 0) return false;  // (y, z)
    const double az[2] = {a[2], a[0]}, bz[2] = {b[2], b[0]}, cz[2] = {c[2], c[0]};
    return orient2d(az, bz, cz) == 0;                  // (z, x)
  }

  // The first four points in insertion order that span 3D.
  std::array<int, 4> firstTet(const std::vector<int>& order) const {
    const int a = order[0];
    int b = -1, c = -1, d = -1;
    std::size_t i = 1;
    for (; i < n_ && b < 0; ++i)
      if (!samePoint(order[i], a)) b = order[i];
    for (; i < n_ && c < 0; ++i)
      if (!collinear(a, b, order[i])) c = order[i];
    for (; i < n_ && d < 0; ++i)
      if (orient3d(p(a), p(b), p(c), p(order[i])) != 0) d = order[i];
    if (d < 0) fail("the seeds do not span 3D (fewer than four distinct, or all coplanar)");
    return {a, b, c, d};
  }

  // The first tet, positively oriented, and its four hull tets.
  void start(std::array<int, 4> q) {
    if (orient3d(p(q[0]), p(q[1]), p(q[2]), p(q[3])) < 0) std::swap(q[0], q[1]);
    Tet t0{{q[0], q[1], q[2], q[3]}, {-1, -1, -1, -1}};
    alloc(t0);
    for (int i = 0; i < 4; ++i) {
      Tet u = t0;
      u.v[i] = kInf;  // and an odd swap flips the orientation outwards
      std::swap(u.v[(i + 1) % 4], u.v[(i + 2) % 4]);
      alloc(u);
    }
    // Link the five by their shared faces.
    for (int s = 0; s < 5; ++s)
      for (int t = s + 1; t < 5; ++t)
        for (int ks = 0; ks < 4; ++ks)
          for (int kt = 0; kt < 4; ++kt)
            if (faceKey(T_[s], ks) == faceKey(T_[t], kt)) {
              T_[s].nb[ks] = t;
              T_[t].nb[kt] = s;
            }
  }

  static std::array<int, 3> faceKey(const Tet& t, int k) {
    std::array<int, 3> f{};
    int m = 0;
    for (int j = 0; j < 4; ++j)
      if (j != k) f[m++] = t.v[j];
    std::sort(f.begin(), f.end());
    return f;
  }

  // The visibility walk: a finite tet containing q (closed), or a hull tet q
  // lies strictly beyond. It ends on a Delaunay triangulation whatever face
  // it leaves by; the step guard only turns a broken invariant into an error.
  int locate(int t, const double* q) const {
    for (std::size_t steps = 0; steps <= T_.size(); ++steps) {
      const Tet& tt = T_[t];
      int next = -1;
      for (int i = 0; i < 4 && next < 0; ++i)
        if (orientWith(tt, i, q) < 0) next = tt.nb[i];
      if (next < 0) return t;
      t = next;
      if (infSlot(T_[t]) >= 0) return t;
    }
    throw std::logic_error("porous delaunay: the point location walk did not end");
  }

  // Inserts point pi from the finite tet `from`; returns a finite tet to start
  // the next walk from.
  int insert(int from, int pi) {
    const double* q = p(pi);
    const int t0 = locate(from, q);
    if (infSlot(T_[t0]) < 0)
      for (int k = 0; k < 4; ++k)
        if (samePoint(T_[t0].v[k], pi)) {
          duplicates_.push_back(pi);
          return t0;
        }

    // The cavity. t0 conflicts: a point in a closed tet, not a vertex, is
    // strictly inside its circumsphere; a point strictly beyond a hull face
    // is in that hull tet's half-space.
    ++stamp_;
    cavity_.assign(1, t0);
    stack_.assign(1, t0);
    inCavity_[t0] = stamp_;
    boundary_.clear();
    while (!stack_.empty()) {
      const int t = stack_.back();
      stack_.pop_back();
      for (int i = 0; i < 4; ++i) {
        const int nb = T_[t].nb[i];
        if (inCavity_[nb] == stamp_) continue;
        if (tested_[nb] != stamp_) {
          if (conflicts(nb, q)) {
            inCavity_[nb] = stamp_;
            cavity_.push_back(nb);
            stack_.push_back(nb);
            continue;
          }
          tested_[nb] = stamp_;
        }
        boundary_.push_back({t, i});
      }
    }

    // One new tet per boundary face: the cavity tet with q in place of the
    // vertex opposite the face. The outer tet's back slot is read before the
    // cavity's slots are freed and reused.
    newTets_.clear();
    backSlot_.clear();
    for (const Face& b : boundary_) {
      Tet nt = T_[b.tet];
      const int outer = nt.nb[b.slot];
      nt.v[b.slot] = pi;
      nt.nb = {-1, -1, -1, -1};
      nt.nb[b.slot] = outer;
      newTets_.push_back(nt);
      int back = -1;
      for (int k = 0; k < 4; ++k)
        if (T_[outer].nb[k] == b.tet) back = k;
      backSlot_.push_back(back);
    }
    for (int c : cavity_) {
      alive_[c] = 0;
      free_.push_back(c);
    }
    ids_.clear();
    for (const Tet& nt : newTets_) ids_.push_back(alloc(nt));

    // Link: each new tet to its outer tet, and the new tets to each other by
    // the edge their shared face has besides q.
    edges_.clear();
    for (std::size_t j = 0; j < ids_.size(); ++j) {
      const int id = ids_[j];
      const int pk = boundary_[j].slot;
      T_[T_[id].nb[pk]].nb[backSlot_[j]] = id;
      for (int k = 0; k < 4; ++k) {
        if (k == pk) continue;
        int a = 0, b = 0, m = 0;
        for (int s = 0; s < 4; ++s)
          if (s != k && s != pk) (m++ == 0 ? a : b) = T_[id].v[s];
        const std::uint64_t key =
            (static_cast<std::uint64_t>(std::min(a, b) + 1) << 32) |
            static_cast<std::uint32_t>(std::max(a, b) + 1);
        std::size_t e = 0;
        while (e < edges_.size() && edges_[e].key != key) ++e;
        if (e == edges_.size()) {
          edges_.push_back({key, id, k});
        } else {
          T_[id].nb[k] = edges_[e].tet;
          T_[edges_[e].tet].nb[edges_[e].slot] = id;
          edges_[e] = edges_.back();
          edges_.pop_back();
        }
      }
    }
    for (std::size_t j = ids_.size(); j-- > 0;)
      if (infSlot(T_[ids_[j]]) < 0) return ids_[j];
    throw std::logic_error("porous delaunay: an insertion made no finite tet");
  }

  // The finite tets, each with its vertices ascending (neighbours permuted
  // along), sorted by their quadruples; a neighbour that is a hull tet is -1.
  DelaunayTets output() const {
    std::vector<int> live;
    live.reserve(T_.size());
    for (std::size_t t = 0; t < T_.size(); ++t)
      if (alive_[t] && infSlot(T_[t]) < 0) live.push_back(static_cast<int>(t));
    std::vector<std::array<int, 4>> perm(T_.size());  // sorted slot -> old slot
    std::vector<std::array<int, 4>> quad(T_.size());
    for (int t : live) {
      std::array<int, 4>& s = perm[t];
      s = {0, 1, 2, 3};
      std::sort(s.begin(), s.end(),
                [&](int x, int y) { return T_[t].v[x] < T_[t].v[y]; });
      for (int k = 0; k < 4; ++k) quad[t][k] = T_[t].v[s[k]];
    }
    std::sort(live.begin(), live.end(),
              [&](int x, int y) { return quad[x] < quad[y]; });
    std::vector<std::int32_t> newIndex(T_.size(), -1);
    for (std::size_t i = 0; i < live.size(); ++i)
      newIndex[live[i]] = static_cast<std::int32_t>(i);

    DelaunayTets out;
    out.verts.resize(4 * live.size());
    out.neighbours.resize(4 * live.size());
    for (std::size_t i = 0; i < live.size(); ++i) {
      const int t = live[i];
      for (int k = 0; k < 4; ++k) {
        out.verts[4 * i + k] = quad[t][k];
        out.neighbours[4 * i + k] = newIndex[T_[t].nb[perm[t][k]]];
      }
    }
    out.duplicates = duplicates_;
    std::sort(out.duplicates.begin(), out.duplicates.end());
    return out;
  }

  std::size_t n_;
  std::vector<double> P_;
  std::vector<Tet> T_;
  std::vector<std::uint8_t> alive_;
  std::vector<std::uint32_t> inCavity_, tested_;  // == stamp_: this insertion
  std::uint32_t stamp_ = 0;
  std::vector<int> free_;
  std::vector<std::int32_t> duplicates_;
  // Per-insertion scratch, kept to reuse the allocations.
  std::vector<int> cavity_, stack_, backSlot_, ids_;
  std::vector<Face> boundary_;
  std::vector<Tet> newTets_;
  std::vector<EdgeKey> edges_;
};

} // namespace

DelaunayTets tetrahedralize(const std::vector<Vector3>& points,
                            const CancelToken* cancel) {
  if (points.size() > kPorousMaxSeeds)
    fail(std::to_string(points.size()) + " points, more than the ceiling of " +
         std::to_string(kPorousMaxSeeds));
  for (const Vector3& v : points)
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
      fail("a point has a non-finite coordinate");
  if (points.size() < 4)
    fail("the seeds do not span 3D (fewer than four distinct, or all coplanar)");
  if (cancel) cancel->throwIfRequested("porous delaunay");
  std::call_once(gPredicatesInit, exactPredicatesInit);
  return Builder(points).run(cancel);
}

} // namespace internal
} // namespace dualc
