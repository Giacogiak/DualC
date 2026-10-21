// The porous cell complex (roadmap 23 #64): the dual of the seeds' Delaunay
// tetrahedralization, packed for the CPU field and the GLSL node, and the
// distance query over it. See src/internal/porous_complex.h for the contract
// and docs/roadmap/23-porous-shell/04-complex-and-bins.md for the rules and
// the numbers.
//
// Bit-identical on every OS: only IEEE arithmetic (+ - * / sqrt), compiled
// without FMA contraction (CMakeLists.txt), exact predicates for every sign
// that matters, and every order fixed by indices alone.

#include "internal/porous_complex.h"

#include "internal/exact_predicates.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>

namespace dualc {
namespace internal {

namespace {

constexpr std::size_t kPollEvery = 4096;
constexpr double kInfinity = std::numeric_limits<double>::infinity();
// A parameter along a Voronoi line past this is "at infinity" (then clipped).
constexpr double kFar = 1e30;

[[noreturn]] void fail(const std::string& what) {
  throw std::invalid_argument("porous complex: " + what);
}

void poll(const CancelToken* cancel, std::size_t i) {
  if (cancel && i % kPollEvery == 0) cancel->throwIfRequested("porous complex");
}

double dot3(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector3 cross3(const Vector3& a, const Vector3& b) {
  return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double dist2(const Vector3& a, const Vector3& b) {
  const Vector3 e = a - b;
  return e.x * e.x + e.y * e.y + e.z * e.z;
}
double comp(const Vector3& v, int axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }
Vector3 toFloat(const Vector3& p) {
  return Vector3{double(float(p.x)), double(float(p.y)), double(float(p.z))};
}

BBox grown(const BBox& b, double r) {
  return BBox{b.min - Vector3{r, r, r}, b.max + Vector3{r, r, r}};
}
bool inside(const BBox& b, const Vector3& p) {
  return p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y && p.y <= b.max.y &&
         p.z >= b.min.z && p.z <= b.max.z;
}
Vector3 clampTo(const BBox& b, const Vector3& p) {
  return Vector3{std::min(std::max(p.x, b.min.x), b.max.x),
                 std::min(std::max(p.y, b.min.y), b.max.y),
                 std::min(std::max(p.z, b.min.z), b.max.z)};
}

const Vector3& vert(const PorousDual& d, std::size_t t, int k) {
  return d.points[static_cast<std::size_t>(d.tets.verts[4 * t + std::size_t(k)])];
}

// The circumcentre of tet t, and whether it is trustworthy: the double
// determinant has the sign the exact orient3d gives (u.(v x w) = -orient3d).
Vector3 circumcentre(const PorousDual& d, std::size_t t, bool* ok) {
  const Vector3& a = vert(d, t, 0);
  const Vector3 u = vert(d, t, 1) - a, v = vert(d, t, 2) - a, w = vert(d, t, 3) - a;
  const double den = 2.0 * dot3(u, cross3(v, w));
  const double o = orient3d(&a.x, &vert(d, t, 1).x, &vert(d, t, 2).x, &vert(d, t, 3).x);
  const Vector3 num = dot3(u, u) * cross3(v, w) + dot3(v, v) * cross3(w, u) + dot3(w, w) * cross3(u, v);
  const Vector3 c = a + num / den;
  *ok = den != 0.0 && ((den > 0.0) == (o < 0.0)) && std::isfinite(c.x) &&
        std::isfinite(c.y) && std::isfinite(c.z);
  return c;
}

// The bin grid over `box` at `size` or more; the origin and size are the
// floats the GPU reads. A grid past maxBins (a sparse volume in a large box)
// gets coarser bins, by 1.25x steps: the own-cell results do not depend on
// the bin size, and a larger struts bound B only moves the clamp further out.
PorousBins makeBins(const BBox& box, double size, double maxBins) {
  PorousBins b;
  b.origin[0] = float(box.min.x);
  b.origin[1] = float(box.min.y);
  b.origin[2] = float(box.min.z);
  for (;; size *= 1.25) {
    b.size = float(size);
    double total = 1.0;
    for (int a = 0; a < 3; ++a) {
      const double span = comp(box.max, a) - double(b.origin[a]);
      const double n = std::max(1.0, std::ceil(span / double(b.size)));
      total *= n;
      if (total <= maxBins) b.dims[a] = static_cast<int>(n);
    }
    if (total <= maxBins) return b;
  }
}

Vector3 binCentre(const PorousBins& b, int x, int y, int z) {
  const double h = double(b.size);
  return Vector3{double(b.origin[0]) + (x + 0.5) * h, double(b.origin[1]) + (y + 0.5) * h,
                 double(b.origin[2]) + (z + 0.5) * h};
}

// The squared distance from p to bin (x, y, z)'s box.
double binDist2(const PorousBins& b, const Vector3& p, int x, int y, int z) {
  const double h = double(b.size);
  const int c[3] = {x, y, z};
  double s = 0.0;
  for (int a = 0; a < 3; ++a) {
    const double lo = double(b.origin[a]) + c[a] * h, hi = lo + h, v = comp(p, a);
    const double g = v < lo ? lo - v : v > hi ? v - hi : 0.0;
    s += g * g;
  }
  return s;
}

// A small candidate buffer on the stack, spilling to the heap.
class Candidates {
public:
  void push(double d, std::int32_t i) {
    if (n_ < fixed_.size()) {
      fixed_[n_++] = {d, i};
      return;
    }
    if (spill_.empty()) spill_.assign(fixed_.begin(), fixed_.end());
    spill_.push_back({d, i});
    ++n_;
  }
  double fold(double k) { return porousSmoothFoldRaw(data(), n_, k); }
  // The fold, and in *g its gradient: gradOf(index) is each primitive's. Each
  // step smin(acc, d) with acc <= d has the gradient (1 - h/2) grad acc +
  // (h/2) grad d, h as porousSmin's.
  template <class GradOf>
  double foldGradient(double k, const GradOf& gradOf, Vector3* g) {
    PorousCandidate* c = data();
    std::size_t w = 0;
    const double v = porousSmoothFoldRaw(c, n_, k, &w);
    if (w == 0) {
      *g = Vector3{0.0, 0.0, 0.0};
      return v;
    }
    double acc = c[0].d;
    Vector3 grad = gradOf(c[0].index);
    for (std::size_t i = 1; i < w; ++i) {
      if (c[i].index == c[i - 1].index) continue;
      const double h = std::max(k - std::fabs(acc - c[i].d), 0.0) / k;
      grad = (1.0 - 0.5 * h) * grad + (0.5 * h) * gradOf(c[i].index);
      acc = porousSmin(acc, c[i].d, k);
    }
    *g = grad;
    return v;
  }
  // Sets *kept to the number of candidates the fold used (sorted at the front
  // of c), 0 when it returned the bare minimum.
  static double porousSmoothFoldRaw(PorousCandidate* c, std::size_t n, double k,
                                    std::size_t* kept = nullptr);

private:
  PorousCandidate* data() { return spill_.empty() ? fixed_.data() : spill_.data(); }

  std::array<PorousCandidate, 64> fixed_;  // the first n_ are set
  std::vector<PorousCandidate> spill_;
  std::size_t n_ = 0;
};

double Candidates::porousSmoothFoldRaw(PorousCandidate* c, std::size_t n, double k,
                                       std::size_t* kept) {
  if (kept) *kept = 0;
  if (n == 0) return kInfinity;
  double m = kInfinity;
  for (std::size_t i = 0; i < n; ++i) m = std::min(m, c[i].d);
  if (!(k > 0.0)) return m;
  // Keep d < m + k, in ascending (d, index) order, each index once.
  std::size_t w = 0;
  for (std::size_t i = 0; i < n; ++i)
    if (c[i].d < m + k) c[w++] = c[i];
  for (std::size_t i = 1; i < w; ++i) {
    const PorousCandidate x = c[i];
    std::size_t j = i;
    for (; j > 0 && (c[j - 1].d > x.d || (c[j - 1].d == x.d && c[j - 1].index > x.index)); --j)
      c[j] = c[j - 1];
    c[j] = x;
  }
  if (kept) *kept = w;
  double acc = c[0].d;
  for (std::size_t i = 1; i < w; ++i)
    if (c[i].index != c[i - 1].index) acc = porousSmin(acc, c[i].d, k);
  return acc;
}

// Liang-Barsky: clips the parameter range [*lo, *hi] of the line o + s e to
// the box; false when nothing is left.
bool clipLine(const Vector3& o, const Vector3& e, const BBox& box, double* lo, double* hi) {
  for (int a = 0; a < 3; ++a) {
    const double oa = comp(o, a), ea = comp(e, a);
    const double bmin = comp(box.min, a), bmax = comp(box.max, a);
    if (ea == 0.0) {
      if (oa < bmin || oa > bmax) return false;
      continue;
    }
    double s0 = (bmin - oa) / ea, s1 = (bmax - oa) / ea;
    if (s0 > s1) std::swap(s0, s1);
    *lo = std::max(*lo, s0);
    *hi = std::min(*hi, s1);
  }
  return *lo <= *hi;
}

struct Segment {
  float v[6];
};

// The walk to the tet holding p, from tet t, over the packed planes: across
// the face p is farthest beyond (the first on a tie) until p is beyond none.
// Returns the tet and sets *outside when it would leave by a hull face.
std::int32_t walkTets(const PorousComplex& c, const Vector3& p, std::int32_t t,
                      bool* outside, long* steps, bool* capped) {
  *outside = false;
  *capped = false;
  for (int step = 0; step < kPorousMaxWalk; ++step) {
    const float* pl = &c.tetPlanes[16 * std::size_t(t)];
    int far = -1;
    double best = 0.0;
    for (int k = 0; k < 4; ++k) {
      const double s = porousPlaneDistance(pl + 4 * k, p);
      if (s > best) {
        best = s;
        far = k;
      }
    }
    if (far < 0) return t;
    const std::int32_t nb = c.tetNeighbours[4 * std::size_t(t) + std::size_t(far)];
    if (nb < 0) {
      *outside = true;
      return t;
    }
    t = nb;
    ++*steps;
  }
  *capped = true;
  return t;
}

void validate(const SeedSet& s) {
  if (s.points.size() != s.isPad.size()) fail("isPad does not match the points");
  if (!(s.minSpacing > 0.0) || !(s.maxSpacing >= s.minSpacing) ||
      !std::isfinite(s.maxSpacing))
    fail("the spacings must be finite and 0 < minSpacing <= maxSpacing");
  if (!s.box.isValid() || s.box.isInfinite()) fail("the seed box must be finite");
  if (!(s.pad >= 0.0)) fail("the pad must be >= 0");
}

} // namespace

double porousSmoothFold(std::vector<PorousCandidate>& c, double k) {
  return Candidates::porousSmoothFoldRaw(c.data(), c.size(), k);
}

// --- The dual ----------------------------------------------------------------

PorousDual derivePorousDual(const SeedSet& seeds, const CancelToken* cancel) {
  validate(seeds);
  PorousDual d;
  d.points.resize(seeds.points.size());
  for (std::size_t i = 0; i < d.points.size(); ++i) d.points[i] = toFloat(seeds.points[i]);
  d.tets = tetrahedralize(d.points, cancel);
  const std::size_t nt = d.tets.size();
  const std::vector<std::int32_t>& V = d.tets.verts;
  const std::vector<std::int32_t>& N = d.tets.neighbours;

  // Faces, each once, in (t, k) order.
  d.faces.reserve(3 * (2 * nt + 64));
  d.faceTets.reserve(2 * (2 * nt + 64));
  for (std::size_t t = 0; t < nt; ++t) {
    poll(cancel, t);
    for (int k = 0; k < 4; ++k) {
      const std::int32_t nb = N[4 * t + std::size_t(k)];
      if (nb >= 0 && static_cast<std::size_t>(nb) < t) continue;
      for (int j = 0; j < 4; ++j)
        if (j != k) d.faces.push_back(V[4 * t + std::size_t(j)]);
      d.faceTets.push_back(static_cast<std::int32_t>(t));
      d.faceTets.push_back(nb);
    }
  }

  // Edges, each once, sorted.
  std::vector<std::uint64_t> keys;
  keys.reserve(6 * nt);
  for (std::size_t t = 0; t < nt; ++t)
    for (int a = 0; a < 4; ++a)
      for (int b = a + 1; b < 4; ++b)
        keys.push_back(std::uint64_t(std::uint32_t(V[4 * t + std::size_t(a)])) << 32 |
                       std::uint32_t(V[4 * t + std::size_t(b)]));
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  d.edges.resize(2 * keys.size());
  for (std::size_t e = 0; e < keys.size(); ++e) {
    d.edges[2 * e]     = std::int32_t(keys[e] >> 32);
    d.edges[2 * e + 1] = std::int32_t(keys[e] & 0xffffffffu);
  }

  // The neighbour CSR. Edges come sorted by their first vertex, so seed s
  // receives its lower neighbours (edges (a, s), a < s, ascending) before its
  // higher ones (edges (s, b), ascending): every list is ascending.
  const std::size_t np = d.points.size();
  d.neighbourOffsets.assign(np + 1, 0);
  for (std::int32_t v : d.edges) ++d.neighbourOffsets[std::size_t(v) + 1];
  for (std::size_t i = 0; i < np; ++i) d.neighbourOffsets[i + 1] += d.neighbourOffsets[i];
  d.neighbours.resize(d.edges.size());
  std::vector<std::int32_t> fill(d.neighbourOffsets.begin(), d.neighbourOffsets.end() - 1);
  for (std::size_t e = 0; e < keys.size(); ++e) {
    const std::int32_t a = d.edges[2 * e], b = d.edges[2 * e + 1];
    d.neighbours[std::size_t(fill[std::size_t(a)]++)] = b;
    d.neighbours[std::size_t(fill[std::size_t(b)]++)] = a;
  }

  // The Voronoi vertices.
  d.circumcentres.resize(nt);
  d.clamped.assign(nt, 0);
  for (std::size_t t = 0; t < nt; ++t) {
    poll(cancel, t);
    bool ok = false;
    Vector3 c = circumcentre(d, t, &ok);
    if (!ok) {
      // Too flat to trust in doubles: at infinity, so clamped; its centroid
      // stands in, clamped too.
      c = 0.25 * (vert(d, t, 0) + vert(d, t, 1) + vert(d, t, 2) + vert(d, t, 3));
      d.clamped[t] = 1;
    }
    if (!inside(seeds.box, c)) d.clamped[t] = 1;
    if (d.clamped[t]) {
      c = clampTo(seeds.box, c);
      ++d.clampedCount;
    }
    d.circumcentres[t] = c;
  }
  return d;
}

// --- Packing -----------------------------------------------------------------

std::size_t PorousComplex::packedBytes() const {
  return 4 * (seeds.size() + neighbourOffsets.size() + neighbours.size() +
              tetPlanes.size() + tetNeighbours.size() + binStart.size() +
              segments.size() + binOffsets.size() + binItems.size());
}

namespace {

// The largest circumradius over the tets with a non-pad vertex whose
// circumcentre lies in the volume's box.
double largestCellRadius(const PorousDual& d, const SeedSet& s, const BBox& volumeBox) {
  double r = 0.0;
  for (std::size_t t = 0; t < d.tets.size(); ++t) {
    bool interior = false;
    for (int k = 0; k < 4; ++k)
      interior = interior || !s.isPad[std::size_t(d.tets.verts[4 * t + std::size_t(k)])];
    if (!interior) continue;
    bool ok = false;
    const Vector3 c = circumcentre(d, t, &ok);
    if (!ok || !inside(volumeBox, c)) continue;
    r = std::max(r, norm(c - vert(d, t, 0)));
  }
  return r;
}

// The Voronoi edge dual to face f, clipped to `box`: false when nothing is
// left. An end inside the box is its tet's circumcentre, shared exactly by
// the tet's four edges; an end beyond it, or a hull face's ray, comes from
// the edge's line: the face's circumcentre plus s times its unit normal.
bool voronoiEdge(const PorousDual& d, std::size_t f, const BBox& box, Vector3* a, Vector3* b) {
  const Vector3& p0 = d.points[std::size_t(d.faces[3 * f])];
  const Vector3& p1 = d.points[std::size_t(d.faces[3 * f + 1])];
  const Vector3& p2 = d.points[std::size_t(d.faces[3 * f + 2])];
  const std::int32_t tets[2] = {d.faceTets[2 * f], d.faceTets[2 * f + 1]};

  Vector3 ends[2];
  bool inBox[2] = {false, false};
  for (int i = 0; i < 2; ++i) {
    if (tets[i] < 0) continue;
    bool ok = false;
    ends[i] = circumcentre(d, std::size_t(tets[i]), &ok);
    inBox[i] = ok && inside(box, ends[i]);
  }
  if (inBox[0] && tets[1] >= 0 && inBox[1]) {
    *a = ends[0];
    *b = ends[1];
    return true;
  }

  const Vector3 u = p1 - p0, v = p2 - p0, w = cross3(u, v);
  const double w2 = dot3(w, w);
  const Vector3 cc = p0 + (dot3(u, u) * cross3(v, w) + dot3(v, v) * cross3(w, u)) / (2.0 * w2);
  const Vector3 n = w / std::sqrt(w2);
  const Vector3 r0 = p0 - cc;
  const double rf2 = dot3(r0, r0);
  // s of each tet's circumcentre on the line, and the side its opposite
  // vertex lies on (+1 along n), the sign from orient3d: > 0 puts the vertex
  // below the counter-clockwise (p0, p1, p2), against n.
  double s[2] = {0.0, 0.0};
  int side[2] = {0, 0};
  for (int i = 0; i < 2; ++i) {
    if (tets[i] < 0) continue;
    const std::size_t t = std::size_t(tets[i]);
    const std::int32_t* tv = &d.tets.verts[4 * t];
    std::int32_t opp = tv[0];
    for (int k = 0; k < 4; ++k)
      if (tv[k] != d.faces[3 * f] && tv[k] != d.faces[3 * f + 1] && tv[k] != d.faces[3 * f + 2])
        opp = tv[k];
    const Vector3& q = d.points[std::size_t(opp)];
    side[i] = orient3d(&p0.x, &p1.x, &p2.x, &q.x) > 0 ? -1 : 1;
    double h = dot3(q - cc, n);
    if (!((h > 0.0) == (side[i] > 0)) || h == 0.0) h = side[i] * 1e-300;
    const Vector3 rq = q - cc;
    s[i] = (dot3(rq, rq) - rf2) / (2.0 * h);
    s[i] = std::min(std::max(s[i], -kFar), kFar);
  }
  // The range of s, and which tet's circumcentre each end is (-1: at
  // infinity, the hull face's ray).
  double lo, hi;
  int loTet, hiTet;
  if (tets[1] >= 0) {
    loTet = s[0] <= s[1] ? 0 : 1;
    hiTet = 1 - loTet;
    lo = s[loTet];
    hi = s[hiTet];
  } else if (side[0] > 0) {  // the ray leaves the hull away from the tet
    loTet = -1;
    hiTet = 0;
    lo = -kInfinity;
    hi = s[0];
  } else {
    loTet = 0;
    hiTet = -1;
    lo = s[0];
    hi = kInfinity;
  }
  const double lo0 = lo, hi0 = hi;
  if (!clipLine(cc, n, box, &lo, &hi)) return false;
  // An end the clip did not move, and whose circumcentre is in the box, is
  // that circumcentre exactly.
  *a = (loTet >= 0 && lo == lo0 && inBox[loTet]) ? ends[loTet] : cc + lo * n;
  *b = (hiTet >= 0 && hi == hi0 && inBox[hiTet]) ? ends[hiTet] : cc + hi * n;
  return true;
}

void packStruts(const PorousDual& d, const SeedSet& seeds, PorousCells cells,
                PorousComplex& c, double maxBins, const CancelToken* cancel) {
  const BBox volumeBox = grown(seeds.box, -seeds.pad);
  double B = std::max(seeds.maxSpacing, c.stats.largestCellRadius);
  BBox visible;  // what a volume point within B can see
  for (;;) {
    visible = grown(volumeBox, B);
    c.bins = makeBins(visible, B, maxBins);
    if (c.bins.size == float(B)) break;
    B = double(c.bins.size);  // coarsened: see the larger box with it
  }
  c.bound = c.bins.size;

  std::vector<Segment> segs;
  auto add = [&](const Vector3& a, const Vector3& b) {
    ++c.stats.candidateSegments;
    Segment s{{float(a.x), float(a.y), float(a.z), float(b.x), float(b.y), float(b.z)}};
    if (std::memcmp(s.v, s.v + 3, 3 * sizeof(float)) == 0) {
      ++c.stats.droppedZeroLength;
      return;
    }
    segs.push_back(s);
  };
  if (cells == PorousCells::Voronoi) {
    const std::size_t nf = d.faceTets.size() / 2;
    for (std::size_t f = 0; f < nf; ++f) {
      poll(cancel, f);
      Vector3 a, b;
      if (voronoiEdge(d, f, visible, &a, &b))
        add(a, b);
      else {
        ++c.stats.candidateSegments;
        ++c.stats.droppedOutside;
      }
    }
  } else {
    for (std::size_t e = 0; e < d.edges.size() / 2; ++e) {
      const Vector3& a = d.points[std::size_t(d.edges[2 * e])];
      const Vector3& b = d.points[std::size_t(d.edges[2 * e + 1])];
      // Seeds lie in the padded box, inside `visible`: nothing to clip.
      add(a, b);
    }
  }

  // The same float segment twice (cospherical seeds: two triangles of one
  // grid square give one Voronoi edge) would be folded twice: keep the first.
  std::vector<std::uint32_t> order(segs.size());
  std::iota(order.begin(), order.end(), 0u);
  auto key = [&](std::uint32_t i) {
    std::array<float, 6> k;
    const float* v = segs[i].v;
    const bool swap = std::lexicographical_compare(v + 3, v + 6, v, v + 3);
    for (int j = 0; j < 3; ++j) {
      k[std::size_t(j)]     = swap ? v[3 + j] : v[j];
      k[std::size_t(3 + j)] = swap ? v[j] : v[3 + j];
    }
    return k;
  };
  std::sort(order.begin(), order.end(), [&](std::uint32_t x, std::uint32_t y) {
    const auto kx = key(x), ky = key(y);
    return kx != ky ? kx < ky : x < y;
  });
  std::vector<std::uint8_t> drop(segs.size(), 0);
  for (std::size_t i = 1; i < order.size(); ++i)
    if (key(order[i]) == key(order[i - 1])) drop[order[i]] = 1;

  for (std::size_t i = 0; i < segs.size(); ++i) {
    if (drop[i]) {
      ++c.stats.droppedDuplicate;
      continue;
    }
    const float* v = segs[i].v;
    c.segments.insert(c.segments.end(), {v[0], v[1], v[2], 0.0f, v[3], v[4], v[5], 0.0f});
  }
  const std::size_t ns = c.segments.size() / 8;
  c.stats.segments = ns;

  // The bins' CSR: every bin the segment's box touches, in segment order.
  const PorousBins& g = c.bins;
  auto range = [&](std::size_t s, int lo[3], int hi[3]) {
    const float* v = &c.segments[8 * s];
    for (int a = 0; a < 3; ++a) {
      lo[a] = g.coord(double(std::min(v[a], v[4 + a])), a);
      hi[a] = g.coord(double(std::max(v[a], v[4 + a])), a);
    }
  };
  c.binOffsets.assign(g.count() + 1, 0);
  for (int pass = 0; pass < 2; ++pass) {
    std::vector<std::int32_t> fill;
    if (pass == 1) {
      for (std::size_t b = 0; b < g.count(); ++b) c.binOffsets[b + 1] += c.binOffsets[b];
      c.binItems.resize(std::size_t(c.binOffsets.back()));
      fill.assign(c.binOffsets.begin(), c.binOffsets.end() - 1);
    }
    for (std::size_t s = 0; s < ns; ++s) {
      poll(cancel, s);
      int lo[3], hi[3];
      range(s, lo, hi);
      for (int z = lo[2]; z <= hi[2]; ++z)
        for (int y = lo[1]; y <= hi[1]; ++y)
          for (int x = lo[0]; x <= hi[0]; ++x) {
            const std::size_t b = g.flat(x, y, z);
            if (pass == 0)
              ++c.binOffsets[b + 1];
            else
              c.binItems[std::size_t(fill[b]++)] = std::int32_t(s);
          }
    }
  }
  c.stats.bins = g.count();
  c.stats.binItems = c.binItems.size();
  for (std::size_t b = 0; b < g.count(); ++b)
    c.stats.maxBinItems = std::max(c.stats.maxBinItems,
                                   std::size_t(c.binOffsets[b + 1] - c.binOffsets[b]));
}

// The start seed of every own-cell bin: the seed nearest the bin's centre
// among those in the bin, or, for an empty bin, the nearest to its centre
// among its 26 neighbours' starts, propagated by a forward and a backward
// sweep (a chamfer transform: every bin is reached, since a seeded bin's
// forward cone holds the last bin and the backward sweep then reaches every
// bin). Only a walk's start: near the nearest, O(bins), deterministic.
std::vector<std::int32_t> startSeeds(const PorousDual& d, const PorousBins& g,
                                     const CancelToken* cancel) {
  const std::size_t nb = g.count();
  std::vector<std::int32_t> start(nb, -1);
  std::vector<double> best(nb, kInfinity);
  auto centre = [&](std::size_t b) {
    const std::size_t nx = std::size_t(g.dims[0]), ny = std::size_t(g.dims[1]);
    return binCentre(g, int(b % nx), int(b / nx % ny), int(b / (nx * ny)));
  };
  auto offer = [&](std::size_t b, std::int32_t i) {
    const double e = dist2(d.points[std::size_t(i)], centre(b));
    if (e < best[b] || (e == best[b] && i < start[b])) {
      best[b] = e;
      start[b] = i;
    }
  };
  std::vector<std::uint8_t> dup(d.points.size(), 0);
  for (std::int32_t i : d.tets.duplicates) dup[std::size_t(i)] = 1;
  for (std::size_t i = 0; i < d.points.size(); ++i) {
    if (dup[i]) continue;
    const Vector3& p = d.points[i];
    offer(g.flat(g.coord(p.x, 0), g.coord(p.y, 1), g.coord(p.z, 2)), std::int32_t(i));
  }
  for (int pass = 0; pass < 2; ++pass)
    for (std::size_t n = 0; n < nb; ++n) {
      poll(cancel, n);
      const std::size_t b = pass == 0 ? n : nb - 1 - n;
      const int x = int(b % std::size_t(g.dims[0]));
      const int y = int(b / std::size_t(g.dims[0]) % std::size_t(g.dims[1]));
      const int z = int(b / (std::size_t(g.dims[0]) * std::size_t(g.dims[1])));
      for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            const int X = x + dx, Y = y + dy, Z = z + dz;
            if (X < 0 || Y < 0 || Z < 0 || X >= g.dims[0] || Y >= g.dims[1] || Z >= g.dims[2])
              continue;
            const std::int32_t s = start[g.flat(X, Y, Z)];
            if (s >= 0 && s != start[b]) offer(b, s);
          }
    }
  for (std::int32_t& s : start)  // unreachable: a 1-bin-thick grid is still swept
    if (s < 0) s = d.tets.verts.empty() ? 0 : d.tets.verts[0];
  return start;
}

void packVoronoiWalls(const PorousDual& d, const SeedSet& seeds, PorousComplex& c,
                      double maxBins, const CancelToken* cancel) {
  c.seeds.resize(4 * d.points.size());
  for (std::size_t i = 0; i < d.points.size(); ++i) {
    c.seeds[4 * i]     = float(d.points[i].x);
    c.seeds[4 * i + 1] = float(d.points[i].y);
    c.seeds[4 * i + 2] = float(d.points[i].z);
    c.seeds[4 * i + 3] = seeds.isPad[i] ? 1.0f : 0.0f;
  }
  c.neighbourOffsets = d.neighbourOffsets;
  c.neighbours = d.neighbours;
  c.bins = makeBins(seeds.box, seeds.minSpacing, maxBins);
  c.binStart = startSeeds(d, c.bins, cancel);
}

void packDelaunayWalls(const PorousDual& d, const SeedSet& seeds, PorousComplex& c,
                       double maxBins, const CancelToken* cancel) {
  const std::size_t nt = d.tets.size();
  c.tetNeighbours = d.tets.neighbours;
  c.tetPlanes.assign(16 * nt, 0.0f);
  const std::size_t nf = d.faceTets.size() / 2;
  for (std::size_t f = 0; f < nf; ++f) {
    poll(cancel, f);
    const std::int32_t* fv = &d.faces[3 * f];
    const Vector3& p0 = d.points[std::size_t(fv[0])];
    const Vector3& p1 = d.points[std::size_t(fv[1])];
    const Vector3& p2 = d.points[std::size_t(fv[2])];
    const Vector3 w = cross3(p1 - p0, p2 - p0);
    const Vector3 n = w / std::sqrt(dot3(w, w));
    const Vector3 g = (p0 + p1 + p2) / 3.0;
    // The tet on the first side: n points out of it when its opposite vertex
    // is below the counter-clockwise face (orient3d > 0).
    const std::size_t t0 = std::size_t(d.faceTets[2 * f]);
    int k0 = 0;
    for (int k = 0; k < 4; ++k)
      if (d.tets.verts[4 * t0 + std::size_t(k)] != fv[0] &&
          d.tets.verts[4 * t0 + std::size_t(k)] != fv[1] &&
          d.tets.verts[4 * t0 + std::size_t(k)] != fv[2])
        k0 = k;
    const Vector3& q = vert(d, t0, k0);
    const double sign = orient3d(&p0.x, &p1.x, &p2.x, &q.x) > 0 ? 1.0 : -1.0;
    const float plane[4] = {float(sign * n.x), float(sign * n.y), float(sign * n.z),
                            float(-sign * dot3(n, g))};
    std::memcpy(&c.tetPlanes[16 * t0 + 4 * std::size_t(k0)], plane, sizeof plane);
    const std::int32_t t1 = d.faceTets[2 * f + 1];
    if (t1 < 0) continue;
    for (int k = 0; k < 4; ++k)
      if (d.tets.neighbours[4 * std::size_t(t1) + std::size_t(k)] == std::int32_t(t0))
        for (int j = 0; j < 4; ++j)
          c.tetPlanes[16 * std::size_t(t1) + 4 * std::size_t(k) + std::size_t(j)] = -plane[j];
  }

  // Start tets: from a tet at the bin's start seed, walked to the tet
  // holding the centre (or the hull tet the walk would leave by).
  c.bins = makeBins(seeds.box, seeds.minSpacing, maxBins);
  const std::vector<std::int32_t> seed = startSeeds(d, c.bins, cancel);
  std::vector<std::int32_t> incident(d.points.size(), -1);
  for (std::size_t t = nt; t-- > 0;)
    for (int k = 0; k < 4; ++k) incident[std::size_t(d.tets.verts[4 * t + std::size_t(k)])] =
        std::int32_t(t);
  c.binStart.resize(c.bins.count());
  std::size_t b = 0;
  for (int z = 0; z < c.bins.dims[2]; ++z)
    for (int y = 0; y < c.bins.dims[1]; ++y)
      for (int x = 0; x < c.bins.dims[0]; ++x, ++b) {
        poll(cancel, b);
        bool outside = false, capped = false;
        long steps = 0;
        c.binStart[b] = walkTets(c, binCentre(c.bins, x, y, z),
                                 incident[std::size_t(seed[b])], &outside, &steps, &capped);
      }
}

} // namespace

PorousComplex packPorousComplex(const PorousDual& d, const SeedSet& seeds, PorousCells cells,
                                PorousMode mode, const CancelToken* cancel, double maxBins) {
  validate(seeds);
  if (d.points.size() != seeds.points.size()) fail("the dual is not of these seeds");
  PorousComplex c;
  c.cells = cells;
  c.mode = mode;
  PorousComplexStats& st = c.stats;
  st.seeds = d.points.size();
  st.tets = d.tets.size();
  st.duplicates = d.tets.duplicates.size();
  st.delaunayFaces = d.faceTets.size() / 2;
  for (std::size_t f = 0; f < st.delaunayFaces; ++f) st.hullFaces += d.faceTets[2 * f + 1] < 0;
  st.delaunayEdges = d.edges.size() / 2;
  st.clampedCircumcentres = d.clampedCount;
  st.largestCellRadius = largestCellRadius(d, seeds, grown(seeds.box, -seeds.pad));

  if (!(maxBins >= 1.0)) fail("maxBins must be >= 1");
  if (mode == PorousMode::Struts)
    packStruts(d, seeds, cells, c, maxBins, cancel);
  else if (cells == PorousCells::Voronoi)
    packVoronoiWalls(d, seeds, c, maxBins, cancel);
  else
    packDelaunayWalls(d, seeds, c, maxBins, cancel);
  return c;
}

PorousComplex buildPorousComplex(const SeedSet& seeds, PorousCells cells, PorousMode mode,
                                 const CancelToken* cancel) {
  return packPorousComplex(derivePorousDual(seeds, cancel), seeds, cells, mode, cancel);
}

// --- The query ---------------------------------------------------------------

namespace {

const Vector3 kZero{0.0, 0.0, 0.0};

// Each query below fills *grad (when not null) with the gradient of the value
// it returns: the fold's over the primitives' own (a bisector's -e/|e|, a
// face plane's -n, a segment's unit offset), zero where a clamp holds the
// value or the walk gave up.
double voronoiWalls(const PorousComplex& c, const Vector3& p, double k, PorousQueryStats* st,
                    Vector3* grad) {
  const PorousBins& g = c.bins;
  std::int32_t s = c.binStart[g.flat(g.coord(p.x, 0), g.coord(p.y, 1), g.coord(p.z, 2))];
  auto seed = [&](std::int32_t i) { return porousPackedPoint(&c.seeds[4 * std::size_t(i)]); };
  auto d2 = [&](std::int32_t i) {
    const Vector3 e = seed(i) - p;
    return dot3(e, e);
  };
  // Greedy: to the neighbour nearest p while it is nearer, by (d^2, index).
  double best = d2(s);
  long steps = 0;
  bool capped = true;
  for (int step = 0; step < kPorousMaxWalk; ++step) {
    std::int32_t next = s;
    double nd = best;
    for (std::int32_t j = c.neighbourOffsets[std::size_t(s)];
         j < c.neighbourOffsets[std::size_t(s) + 1]; ++j) {
      const std::int32_t q = c.neighbours[std::size_t(j)];
      const double e = d2(q);
      if (e < nd || (e == nd && q < next)) {
        nd = e;
        next = q;
      }
    }
    if (next == s) {
      capped = false;
      break;
    }
    s = next;
    best = nd;
    ++steps;
  }
  if (st) {
    st->walkSteps += steps;
    st->maxWalkSteps = std::max(st->maxWalkSteps, steps);
    st->walkCapHits += capped;
  }
  if (grad) *grad = kZero;
  if (capped) return 0.0;
  const Vector3 ps = seed(s);
  Candidates cand;
  double m = kInfinity;
  std::int32_t arg = -1;
  for (std::int32_t j = c.neighbourOffsets[std::size_t(s)];
       j < c.neighbourOffsets[std::size_t(s) + 1]; ++j) {
    const std::int32_t q = c.neighbours[std::size_t(j)];
    const double v = porousBisectorDistance(p, ps, seed(q));
    if (v < m) arg = q;
    m = std::min(m, v);
    if (k > 0.0) cand.push(v, q);
  }
  if (!grad) {
    const double v = k > 0.0 ? cand.fold(k) : m;
    return std::max(0.0, v);
  }
  auto gradOf = [&](std::int32_t q) {
    const Vector3 e = seed(q) - ps;
    return e * (-1.0 / std::sqrt(dot3(e, e)));
  };
  Vector3 gd = kZero;
  double v = m;
  if (k > 0.0) v = cand.foldGradient(k, gradOf, &gd);
  else if (arg >= 0) gd = gradOf(arg);
  if (v > 0.0) *grad = gd;
  return std::max(0.0, v);
}

double delaunayWalls(const PorousComplex& c, const Vector3& p, double k, PorousQueryStats* st,
                     Vector3* grad) {
  const PorousBins& g = c.bins;
  const std::int32_t start =
      c.binStart[g.flat(g.coord(p.x, 0), g.coord(p.y, 1), g.coord(p.z, 2))];
  bool outside = false, capped = false;
  long steps = 0;
  const std::int32_t t = walkTets(c, p, start, &outside, &steps, &capped);
  if (st) {
    st->walkSteps += steps;
    st->maxWalkSteps = std::max(st->maxWalkSteps, steps);
    st->walkCapHits += capped;
  }
  if (grad) *grad = kZero;
  if (outside || capped) return 0.0;
  Candidates cand;
  double m = kInfinity;
  std::int32_t arg = -1;
  for (int f = 0; f < 4; ++f) {
    const double v = -porousPlaneDistance(&c.tetPlanes[16 * std::size_t(t) + 4 * std::size_t(f)], p);
    if (v < m) arg = 4 * t + f;
    m = std::min(m, v);
    if (k > 0.0) cand.push(v, 4 * t + f);
  }
  if (!grad) {
    const double v = k > 0.0 ? cand.fold(k) : m;
    return std::max(0.0, v);
  }
  auto gradOf = [&](std::int32_t i) {
    const float* n = &c.tetPlanes[4 * std::size_t(i)];
    return Vector3{-double(n[0]), -double(n[1]), -double(n[2])};
  };
  Vector3 gd = kZero;
  double v = m;
  if (k > 0.0) v = cand.foldGradient(k, gradOf, &gd);
  else if (arg >= 0) gd = gradOf(arg);
  if (v > 0.0) *grad = gd;
  return std::max(0.0, v);
}

double struts(const PorousComplex& c, const Vector3& p, double k, PorousQueryStats* st,
              Vector3* grad) {
  const PorousBins& g = c.bins;
  const double B = double(c.bound);
  const int cx = g.coord(p.x, 0), cy = g.coord(p.y, 1), cz = g.coord(p.z, 2);
  double m = kInfinity;
  std::int32_t arg = -1;
  Candidates cand;
  long visited = 0;
  auto visit = [&](int x, int y, int z) {
    const std::size_t b = g.flat(x, y, z);
    for (std::int32_t i = c.binOffsets[b]; i < c.binOffsets[b + 1]; ++i) {
      const std::int32_t s = c.binItems[std::size_t(i)];
      const float* v = &c.segments[8 * std::size_t(s)];
      const double d = porousSegmentDistance(p, porousPackedPoint(v), porousPackedPoint(v + 4));
      ++visited;
      if (!(d < B)) continue;
      if (d < m || (d == m && s < arg)) arg = s;
      m = std::min(m, d);
      if (k > 0.0 && d < m + k) cand.push(d, s);
    }
  };
  visit(cx, cy, cz);
  for (int z = std::max(cz - 1, 0); z <= std::min(cz + 1, g.dims[2] - 1); ++z)
    for (int y = std::max(cy - 1, 0); y <= std::min(cy + 1, g.dims[1] - 1); ++y)
      for (int x = std::max(cx - 1, 0); x <= std::min(cx + 1, g.dims[0] - 1); ++x) {
        if (x == cx && y == cy && z == cz) continue;
        // Only a bin that can hold a primitive the result depends on: one
        // nearer than min(m + k, B) (with a margin for the rounding of the
        // box distance; extra visits change nothing).
        const double r = std::min(m + k, B) * (1.0 + 1e-9) + 1e-12;
        if (binDist2(g, p, x, y, z) >= r * r) continue;
        visit(x, y, z);
      }
  if (st) st->candidates += visited;
  if (grad) *grad = kZero;
  if (m == kInfinity) return B;
  if (!grad) {
    const double v = k > 0.0 ? cand.fold(k) : m;
    return std::max(0.0, std::min(v, B));
  }
  auto gradOf = [&](std::int32_t s) {
    const float* v = &c.segments[8 * std::size_t(s)];
    const Vector3 a = porousPackedPoint(v), e = porousPackedPoint(v + 4) - a, q = p - a;
    const double l2 = dot3(e, e);
    double t = l2 > 0.0 ? dot3(q, e) / l2 : 0.0;
    t = std::min(std::max(t, 0.0), 1.0);
    const Vector3 r = q - t * e;
    const double n = std::sqrt(dot3(r, r));
    return n > 0.0 ? r * (1.0 / n) : kZero;
  };
  Vector3 gd = kZero;
  double v = m;
  if (k > 0.0) v = cand.foldGradient(k, gradOf, &gd);
  else gd = gradOf(arg);
  if (v > 0.0 && v < B) *grad = gd;
  return std::max(0.0, std::min(v, B));
}

} // namespace

double porousDistance(const PorousComplex& c, const Vector3& p, double smooth,
                      PorousQueryStats* stats) {
  const double k = smooth > 0.0 ? smooth : 0.0;
  if (c.mode == PorousMode::Struts) return struts(c, p, k, stats, nullptr);
  if (c.cells == PorousCells::Voronoi) return voronoiWalls(c, p, k, stats, nullptr);
  return delaunayWalls(c, p, k, stats, nullptr);
}

double porousDistanceGradient(const PorousComplex& c, const Vector3& p, double smooth,
                              Vector3* gradient, PorousQueryStats* stats) {
  const double k = smooth > 0.0 ? smooth : 0.0;
  if (c.mode == PorousMode::Struts) return struts(c, p, k, stats, gradient);
  if (c.cells == PorousCells::Voronoi) return voronoiWalls(c, p, k, stats, gradient);
  return delaunayWalls(c, p, k, stats, gradient);
}

} // namespace internal
} // namespace dualc
