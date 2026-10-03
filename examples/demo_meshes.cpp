#include "demo_meshes.h"

#include "dualc/pipeline.h"
#include "dualc/primitives.h"

#include "geometrycentral/surface/surface_mesh_factories.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace dce {

namespace {

using dualc::Vector3;
using geometrycentral::surface::makeSurfaceMeshAndGeometry;
using Polygons = std::vector<std::vector<std::size_t>>;
using Positions = std::vector<Vector3>;

constexpr double kPi = 3.14159265358979323846;

inline double dot3(const Vector3& a, const Vector3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vector3 cross3(const Vector3& a, const Vector3& b) {
  return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x};
}
inline Vector3 normalized(const Vector3& a) {
  const double n = a.norm();
  return (n > 1e-12) ? a * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
}
// Rotate `v` about unit axis `k` by `ang` (Rodrigues).
inline Vector3 rotateAbout(const Vector3& v, const Vector3& k, double ang) {
  const double c = std::cos(ang), s = std::sin(ang);
  return v * c + cross3(k, v) * s + k * (dot3(k, v) * (1.0 - c));
}

} // namespace

// --- Icosphere -----------------------------------------------------------

MeshAndGeom makeIcosphere(double radius, int subdivisions) {
  const double t = (1.0 + std::sqrt(5.0)) / 2.0;
  Positions pos = {
      {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
      {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
      {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1},
  };
  std::vector<std::array<std::size_t, 3>> faces = {
      {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
      {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
      {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
      {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
  };

  for (int s = 0; s < subdivisions; ++s) {
    std::map<std::pair<std::size_t, std::size_t>, std::size_t> midpoint;
    auto edgeMid = [&](std::size_t a, std::size_t b) -> std::size_t {
      const auto key = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
      auto it = midpoint.find(key);
      if (it != midpoint.end()) return it->second;
      const Vector3 m = (pos[a] + pos[b]) * 0.5;
      const std::size_t idx = pos.size();
      pos.push_back(m);
      midpoint[key] = idx;
      return idx;
    };
    std::vector<std::array<std::size_t, 3>> next;
    next.reserve(faces.size() * 4);
    for (const auto& f : faces) {
      const std::size_t a = edgeMid(f[0], f[1]);
      const std::size_t b = edgeMid(f[1], f[2]);
      const std::size_t c = edgeMid(f[2], f[0]);
      next.push_back({f[0], a, c});
      next.push_back({f[1], b, a});
      next.push_back({f[2], c, b});
      next.push_back({a, b, c});
    }
    faces.swap(next);
  }

  for (auto& p : pos) p = normalized(p) * radius;

  Polygons polys;
  polys.reserve(faces.size());
  for (const auto& f : faces) polys.push_back({f[0], f[1], f[2]});
  return makeSurfaceMeshAndGeometry(polys, pos);
}

// --- UV-sphere ------------------------------------------------------------

MeshAndGeom makeUvSphere(double radius, int nLat, int nLong) {
  Positions pos;
  pos.push_back({0.0, radius, 0.0}); // top pole = index 0
  for (int l = 1; l < nLat; ++l) {
    const double theta = kPi * l / nLat; // 0 at top pole
    const double y = radius * std::cos(theta);
    const double rr = radius * std::sin(theta);
    for (int j = 0; j < nLong; ++j) {
      const double phi = 2.0 * kPi * j / nLong;
      pos.push_back({rr * std::cos(phi), y, rr * std::sin(phi)});
    }
  }
  const std::size_t bottom = pos.size();
  pos.push_back({0.0, -radius, 0.0});

  auto ring = [&](int l, int j) -> std::size_t {
    return 1 + std::size_t(l - 1) * nLong + (j % nLong);
  };

  Polygons polys;
  for (int j = 0; j < nLong; ++j) // top cap
    polys.push_back({0, ring(1, j), ring(1, j + 1)});
  for (int l = 1; l < nLat - 1; ++l) // middle quads
    for (int j = 0; j < nLong; ++j)
      polys.push_back({ring(l, j), ring(l + 1, j), ring(l + 1, j + 1),
                       ring(l, j + 1)});
  for (int j = 0; j < nLong; ++j) // bottom cap
    polys.push_back({bottom, ring(nLat - 1, j + 1), ring(nLat - 1, j)});

  return makeSurfaceMeshAndGeometry(polys, pos);
}

// --- Torus ----------------------------------------------------------------

MeshAndGeom makeTorus(double majorRadius, double minorRadius, int nMajor,
                      int nMinor) {
  Positions pos;
  pos.reserve(std::size_t(nMajor) * nMinor);
  for (int i = 0; i < nMajor; ++i) {
    const double u = 2.0 * kPi * i / nMajor;
    const double cu = std::cos(u), su = std::sin(u);
    for (int j = 0; j < nMinor; ++j) {
      const double v = 2.0 * kPi * j / nMinor;
      const double rr = majorRadius + minorRadius * std::cos(v);
      // ring in the xz-plane (axis = y), matching TorusField's convention.
      pos.push_back({rr * cu, minorRadius * std::sin(v), rr * su});
    }
  }
  auto idx = [&](int i, int j) -> std::size_t {
    return std::size_t(i % nMajor) * nMinor + (j % nMinor);
  };
  Polygons polys;
  polys.reserve(std::size_t(nMajor) * nMinor);
  for (int i = 0; i < nMajor; ++i)
    for (int j = 0; j < nMinor; ++j)
      polys.push_back({idx(i, j), idx(i + 1, j), idx(i + 1, j + 1),
                       idx(i, j + 1)});
  return makeSurfaceMeshAndGeometry(polys, pos);
}

// --- Trefoil knot ---------------------------------------------------------

MeshAndGeom makeTrefoilKnot(double tubeR, int nAlong, int nAround) {
  // (2,3) torus-knot centerline: lies on a torus of major 2, minor +-1.
  auto center = [](double s) {
    const double c3 = std::cos(3 * s), s3 = std::sin(3 * s);
    const double r = 2.0 + c3;
    return Vector3{r * std::cos(2 * s), r * std::sin(2 * s), s3};
  };

  std::vector<Vector3> C(nAlong), T(nAlong), N(nAlong), B(nAlong);
  for (int i = 0; i < nAlong; ++i) {
    const double s = 2.0 * kPi * i / nAlong;
    C[i] = center(s);
    const double h = 1e-4;
    T[i] = normalized(center(s + h) - center(s - h));
  }

  // Rotation-minimizing frame by parallel transport.
  const Vector3 ref =
      (std::abs(T[0].z) < 0.9) ? Vector3{0, 0, 1} : Vector3{0, 1, 0};
  N[0] = normalized(ref - T[0] * dot3(ref, T[0]));
  for (int i = 1; i < nAlong; ++i) {
    const Vector3 a = cross3(T[i - 1], T[i]);
    const double an = a.norm();
    if (an < 1e-9) {
      N[i] = N[i - 1];
    } else {
      const Vector3 k = a * (1.0 / an);
      const double ang = std::atan2(an, dot3(T[i - 1], T[i]));
      N[i] = normalized(rotateAbout(N[i - 1], k, ang));
      N[i] = normalized(N[i] - T[i] * dot3(N[i], T[i]));
    }
  }

  // Holonomy: transport the last normal across the seam back onto station 0
  // and measure the residual twist about T[0], then cancel it uniformly so
  // the tube closes without a visible kink (watertightness is already
  // guaranteed by the modular face indexing below).
  Vector3 nClose = N[nAlong - 1];
  {
    const Vector3 a = cross3(T[nAlong - 1], T[0]);
    const double an = a.norm();
    if (an > 1e-9)
      nClose = rotateAbout(N[nAlong - 1], a * (1.0 / an),
                           std::atan2(an, dot3(T[nAlong - 1], T[0])));
  }
  double theta = std::acos(std::max(-1.0, std::min(1.0, dot3(normalized(nClose),
                                                             N[0]))));
  if (dot3(cross3(nClose, N[0]), T[0]) < 0) theta = -theta;
  for (int i = 0; i < nAlong; ++i) {
    N[i] = normalized(rotateAbout(N[i], T[i], -theta * i / nAlong));
    B[i] = normalized(cross3(T[i], N[i]));
  }

  Positions pos;
  pos.reserve(std::size_t(nAlong) * nAround);
  for (int i = 0; i < nAlong; ++i)
    for (int j = 0; j < nAround; ++j) {
      const double a = 2.0 * kPi * j / nAround;
      pos.push_back(C[i] + (N[i] * std::cos(a) + B[i] * std::sin(a)) * tubeR);
    }
  auto idx = [&](int i, int j) -> std::size_t {
    return std::size_t(i % nAlong) * nAround + (j % nAround);
  };
  Polygons polys;
  polys.reserve(std::size_t(nAlong) * nAround);
  for (int i = 0; i < nAlong; ++i)
    for (int j = 0; j < nAround; ++j)
      polys.push_back({idx(i, j), idx(i + 1, j), idx(i + 1, j + 1),
                       idx(i, j + 1)});
  return makeSurfaceMeshAndGeometry(polys, pos);
}

// --- Genus-2 double torus -------------------------------------------------

MeshAndGeom makeGenus2(int maxDepth) {
  using namespace dualc;
  // Two coplanar tori (rings in the xz-plane) offset along x so their tubes
  // merge in a single neck near the origin: connected sum of two genus-1
  // handlebodies = genus 2. Contoured by DualC into a watertight mesh.
  auto left = std::make_shared<TorusField>(Vector3{-1.2, 0, 0}, 1.0, 0.45);
  auto right = std::make_shared<TorusField>(Vector3{1.2, 0, 0}, 1.0, 0.45);
  FieldPtr f = unionOf(left, right);

  SamplerParams sp;
  sp.maxDepth = maxDepth;
  ContourerParams cp;
  auto res = dualContourField(*f, sp, cp);
  return MeshAndGeom{std::move(std::get<0>(res)), std::move(std::get<1>(res))};
}

// --- Cylinder -------------------------------------------------------------

MeshAndGeom makeCylinder(double radius, double height, int nSeg) {
  const double hy = height * 0.5;
  Positions pos;
  pos.reserve(std::size_t(nSeg) * 2);
  for (int j = 0; j < nSeg; ++j) { // top ring 0..nSeg-1
    const double a = 2.0 * kPi * j / nSeg;
    pos.push_back({radius * std::cos(a), hy, radius * std::sin(a)});
  }
  for (int j = 0; j < nSeg; ++j) { // bottom ring nSeg..2nSeg-1
    const double a = 2.0 * kPi * j / nSeg;
    pos.push_back({radius * std::cos(a), -hy, radius * std::sin(a)});
  }
  auto top = [&](int j) { return std::size_t(j % nSeg); };
  auto bot = [&](int j) { return std::size_t(nSeg + (j % nSeg)); };

  Polygons polys;
  for (int j = 0; j < nSeg; ++j) // side wall (outward winding)
    polys.push_back({bot(j), bot(j + 1), top(j + 1), top(j)});
  std::vector<std::size_t> topCap, botCap; // flat polygon caps -> sharp rims
  for (int j = 0; j < nSeg; ++j) topCap.push_back(top(j));
  for (int j = nSeg - 1; j >= 0; --j) botCap.push_back(bot(j));
  polys.push_back(topCap);
  polys.push_back(botCap);
  return makeSurfaceMeshAndGeometry(polys, pos);
}

// --- Cube -----------------------------------------------------------------

MeshAndGeom makeCube(double side) {
  // Vertex order and triangulation reproduce the original data/cube.obj so the
  // recipes that name it keep producing byte-identical output.
  const double h = side * 0.5;
  Positions pos;
  for (int i = 0; i < 8; ++i)
    pos.push_back({(i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h});
  const std::size_t tris[12][3] = {
      {0, 2, 3}, {0, 3, 1}, {4, 5, 7}, {4, 7, 6}, {0, 1, 5}, {0, 5, 4},
      {2, 6, 7}, {2, 7, 3}, {0, 4, 6}, {0, 6, 2}, {1, 3, 7}, {1, 7, 5}};
  Polygons polys;
  for (const auto& t : tris) polys.push_back({t[0], t[1], t[2]});
  return makeSurfaceMeshAndGeometry(polys, pos);
}

// --- L-bracket ------------------------------------------------------------

MeshAndGeom makeLBracket(double arm, double thick, double depth) {
  // L profile in xy (CCW); reentrant (concave) corner at (thick, thick).
  std::vector<std::pair<double, double>> profile = {
      {0, 0}, {arm, 0}, {arm, thick}, {thick, thick}, {thick, arm}, {0, arm}};
  const double cx = arm * 0.5, cy = arm * 0.5, hz = depth * 0.5;
  const int n = static_cast<int>(profile.size());

  Positions pos;
  for (const auto& pr : profile) // front (z = +hz), indices 0..n-1
    pos.push_back({pr.first - cx, pr.second - cy, hz});
  for (const auto& pr : profile) // back (z = -hz), indices n..2n-1
    pos.push_back({pr.first - cx, pr.second - cy, -hz});

  Polygons polys;
  // The L profile is non-convex (reflex corner at p3), so a polygon-face fan
  // from an arbitrary vertex can cross the notch. Triangulate both caps
  // explicitly from p0 (the origin corner), which sees the whole star-shaped
  // L, guaranteeing valid in-profile triangles.
  for (int i = 1; i < n - 1; ++i) // front cap, faces +z
    polys.push_back({0u, std::size_t(i), std::size_t(i + 1)});
  for (int i = 1; i < n - 1; ++i) // back cap, faces -z (reversed)
    polys.push_back({std::size_t(n), std::size_t(n + i + 1), std::size_t(n + i)});
  for (int i = 0; i < n; ++i) { // side walls
    const int ni = (i + 1) % n;
    polys.push_back({std::size_t(i), std::size_t(ni), std::size_t(n + ni),
                     std::size_t(n + i)});
  }
  return makeSurfaceMeshAndGeometry(polys, pos);
}

// --- Hex prism + cylindrical bore -----------------------------------------

MeshAndGeom makeHexPrismBore(double outerR, double height, double boreR,
                             int nSeg) {
  if (nSeg % 6 != 0) nSeg += 6 - (nSeg % 6); // keep the 6 hex corners on verts
  const double hy = height * 0.5;
  const double inradius = outerR * std::cos(kPi / 6.0);

  Positions pos; // rings: outer-top, outer-bot, bore-top, bore-bot
  auto hexR = [&](double a) {
    const double al = std::fmod(a, kPi / 3.0) - kPi / 6.0;
    return inradius / std::cos(al);
  };
  for (int j = 0; j < nSeg; ++j) {
    const double a = 2.0 * kPi * j / nSeg;
    const double r = hexR(a);
    pos.push_back({r * std::cos(a), hy, r * std::sin(a)});
  }
  for (int j = 0; j < nSeg; ++j) {
    const double a = 2.0 * kPi * j / nSeg;
    const double r = hexR(a);
    pos.push_back({r * std::cos(a), -hy, r * std::sin(a)});
  }
  for (int j = 0; j < nSeg; ++j) {
    const double a = 2.0 * kPi * j / nSeg;
    pos.push_back({boreR * std::cos(a), hy, boreR * std::sin(a)});
  }
  for (int j = 0; j < nSeg; ++j) {
    const double a = 2.0 * kPi * j / nSeg;
    pos.push_back({boreR * std::cos(a), -hy, boreR * std::sin(a)});
  }
  auto oT = [&](int j) { return std::size_t(j % nSeg); };
  auto oB = [&](int j) { return std::size_t(nSeg + (j % nSeg)); };
  auto iT = [&](int j) { return std::size_t(2 * nSeg + (j % nSeg)); };
  auto iB = [&](int j) { return std::size_t(3 * nSeg + (j % nSeg)); };

  Polygons polys;
  for (int j = 0; j < nSeg; ++j) { // outer wall (faces out)
    polys.push_back({oB(j), oB(j + 1), oT(j + 1), oT(j)});
    // bore wall (faces in -> reversed)
    polys.push_back({iT(j), iT(j + 1), iB(j + 1), iB(j)});
    // top annulus cap (faces +y)
    polys.push_back({oT(j), oT(j + 1), iT(j + 1), iT(j)});
    // bottom annulus cap (faces -y)
    polys.push_back({iB(j), iB(j + 1), oB(j + 1), oB(j)});
  }
  return makeSurfaceMeshAndGeometry(polys, pos);
}

} // namespace dce
