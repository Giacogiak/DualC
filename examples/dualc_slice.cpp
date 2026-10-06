#include "example_common.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include "stb_image_write.h"  // declarations only; impl in third_party/stb_impl.cpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// dualc_slice -- sample a TPMS-lattice-in-mesh implicit field on a 2D plane.
//
// Where dualc_lattice contours the whole field into a (potentially enormous)
// mesh, dualc_slice never materialises the mesh: it evaluates field.valueAt on
// a regular grid over an axis-aligned cutting plane. Cost is O(res^2) and
// independent of lattice density, so it stays cheap on parts that would OOM the
// mesher. Two outputs:
//   * a PNG heatmap (diverging blue=inside / white=surface / red=outside) with
//     the zero-isocontour burned in, for a quick visual read;
//   * an SVG of that zero-isocontour as line segments (marching squares) -- the
//     authoritative cross-section geometry, and the seed of future per-layer
//     direct print-slicing.
//
// The field is composed exactly as in dualc_lattice (same flags) so the slice
// shows the very field that would be manufactured.

using dualc::FieldPtr;
using dualc::Vector3;

namespace {

// ---- small geometry helpers -------------------------------------------------

double comp(const Vector3& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }
void setComp(Vector3& v, int a, double val) {
  if (a == 0) v.x = val;
  else if (a == 1) v.y = val;
  else v.z = val;
}

// A 2D line segment in world (u, v) plane coordinates.
struct Seg2 {
  double x0, y0, x1, y1;
};

// ---- CLI plumbing (mirrors dualc_lattice) -----------------------------------

void printUsage() {
  std::cerr <<
      "dualc_slice -- sample a TPMS-lattice-in-mesh field on a cutting plane.\n"
      "\n"
      "USAGE\n"
      "  dualc_slice <input.obj> [options]\n"
      "\n"
      "OPTIONS\n"
      "  --type NAME            TPMS family. Default: gyroid.\n"
      "                         One of:\n";
  for (const dce::TpmsKind& k : dce::tpmsKinds()) {
    std::cerr << "                           " << k.name << " -- " << k.blurb
              << "\n";
  }
  std::cerr <<
      "  --wavelength W         Unit-cell side (world units). Default: mesh\n"
      "                         AABB diagonal / 10.\n"
      "  --offset T             Thick-wall offset (>=0). 0 (default) slices the\n"
      "                         solid phase; T>0 slices the |F|-T shell.\n"
      "  --normalize-thickness  Normalise the TPMS by 1/|grad F| before offset.\n"
      "  --plane AXIS=VALUE     Axis-aligned cutting plane, e.g. z=0 or x=1.5.\n"
      "                         AXIS is x, y or z. Default: z at the bounds\n"
      "                         centre.\n"
      "  --res N                Grid resolution per side (default 512, max\n"
      "                         4096).\n"
      "  --bounds x0,y0,z0,x1,y1,z1\n"
      "                         Sampling region. Default: input AABB padded 5%%.\n"
      "  -o PATH                Output base; .png + .svg are written. Default:\n"
      "                         <type>_slice.png / .svg.\n"
      "  --help, -h             Show this message.\n"
      "\n"
      "EXAMPLES\n"
      "  dualc_slice cube.obj --type gyroid --wavelength 0.5 --plane z=0\n"
      "  dualc_slice bunny.obj --offset 1.0 --normalize-thickness\n"
      "              --plane y=0 --res 1024 -o bunny_y.png\n";
}

dualc::BBox meshAABB(geometrycentral::surface::SurfaceMesh& mesh,
                     geometrycentral::surface::VertexPositionGeometry& geom) {
  dualc::BBox b;
  bool first = true;
  for (auto v : mesh.vertices()) {
    const Vector3 p = geom.inputVertexPositions[v];
    if (first) {
      b.min = b.max = p;
      first = false;
    } else {
      b.min = Vector3{std::min(b.min.x, p.x), std::min(b.min.y, p.y),
                      std::min(b.min.z, p.z)};
      b.max = Vector3{std::max(b.max.x, p.x), std::max(b.max.y, p.y),
                      std::max(b.max.z, p.z)};
    }
  }
  return b;
}

dualc::BBox padBBox(const dualc::BBox& b, double frac) {
  const Vector3 e = b.extent() * frac;
  dualc::BBox r;
  r.min = b.min - e;
  r.max = b.max + e;
  return r;
}

// Parse "z=1.5" into (axis 0/1/2, value). False on malformed input.
bool parsePlane(const std::string& s, int& axis, double& value) {
  if (s.size() < 3 || s[1] != '=') return false;
  switch (s[0]) {
    case 'x': case 'X': axis = 0; break;
    case 'y': case 'Y': axis = 1; break;
    case 'z': case 'Z': axis = 2; break;
    default: return false;
  }
  char* end = nullptr;
  value = std::strtod(s.c_str() + 2, &end);
  return end != s.c_str() + 2 && *end == '\0';
}

// Replace (or append) the extension on `base` with `ext` (which includes the
// dot). "a/b.png" + ".svg" -> "a/b.svg"; "a/b" + ".png" -> "a/b.png".
std::string withExt(const std::string& base, const std::string& ext) {
  const std::size_t slash = base.find_last_of("/\\");
  const std::size_t dot = base.find_last_of('.');
  const bool hasExt = dot != std::string::npos &&
                      (slash == std::string::npos || dot > slash);
  return (hasExt ? base.substr(0, dot) : base) + ext;
}

// ---- the slice pipeline -----------------------------------------------------

// Evaluate the field on a res x res corner grid spanning [uMin,uMax] x
// [vMin,vMax] on the plane axis==pos. Row-major: values[j*res + i], i indexes u
// (column), j indexes v (row), both ascending from the min corner.
std::vector<float> sampleSlice(const dualc::ImplicitField& field, int axis,
                               double pos, int uAxis, int vAxis, double uMin,
                               double uMax, double vMin, double vMax, int res) {
  std::vector<float> values(static_cast<std::size_t>(res) * res);
  const double du = (res > 1) ? (uMax - uMin) / (res - 1) : 0.0;
  const double dv = (res > 1) ? (vMax - vMin) / (res - 1) : 0.0;
  for (int j = 0; j < res; ++j) {
    const double v = vMin + dv * j;
    for (int i = 0; i < res; ++i) {
      Vector3 p{0, 0, 0};
      setComp(p, axis, pos);
      setComp(p, uAxis, uMin + du * i);
      setComp(p, vAxis, v);
      values[static_cast<std::size_t>(j) * res + i] =
          static_cast<float>(field.valueAt(p));
    }
  }
  return values;
}

// Symmetric colour range: ~99th percentile of |value|, so one huge
// MeshSource-outside value can't flatten the interesting near-surface band.
float symRange(const std::vector<float>& vals) {
  std::vector<float> a;
  a.reserve(vals.size());
  for (float v : vals)
    if (std::isfinite(v)) a.push_back(std::fabs(v));
  if (a.empty()) return 1.0f;
  const std::size_t k =
      static_cast<std::size_t>(std::floor(0.99 * (a.size() - 1)));
  std::nth_element(a.begin(), a.begin() + k, a.end());
  const float s = a[k];
  return s > 0.0f ? s : 1.0f;
}

// Diverging blue(inside) -> white(surface) -> red(outside) for normalised
// t in [-1, 1].
void colormap(float t, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
  if (t < -1.0f) t = -1.0f;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.0f) {           // inside: blue -> white
    const float s = 1.0f + t;  // 0 at -1, 1 at 0
    r = static_cast<std::uint8_t>(255.0f * s);
    g = static_cast<std::uint8_t>(255.0f * s);
    b = 255;
  } else {                  // outside: white -> red
    r = 255;
    g = static_cast<std::uint8_t>(255.0f * (1.0f - t));
    b = static_cast<std::uint8_t>(255.0f * (1.0f - t));
  }
}

// Write the heatmap PNG. Row 0 (top) is world v = vMax, so the image shares the
// SVG's world-v-up orientation. With burnContour, pixels straddling a sign
// change (vs their right/upper neighbour in the grid) are painted black.
int writeHeatmapPng(const std::vector<float>& values, int res,
                    const std::string& path, bool burnContour) {
  const float s = symRange(values);
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(res) * res * 3);
  for (int r = 0; r < res; ++r) {
    const int j = res - 1 - r;  // flip: top row = max v
    for (int i = 0; i < res; ++i) {
      const float val = values[static_cast<std::size_t>(j) * res + i];
      std::uint8_t cr, cg, cb;
      colormap(val / s, cr, cg, cb);
      bool edge = false;
      if (burnContour) {
        const float v0 = val;
        if (i + 1 < res) {
          const float v1 = values[static_cast<std::size_t>(j) * res + i + 1];
          if ((v0 < 0.0f) != (v1 < 0.0f)) edge = true;
        }
        if (!edge && j + 1 < res) {
          const float v1 = values[static_cast<std::size_t>(j + 1) * res + i];
          if ((v0 < 0.0f) != (v1 < 0.0f)) edge = true;
        }
      }
      const std::size_t o = (static_cast<std::size_t>(r) * res + i) * 3;
      if (edge) { rgb[o] = rgb[o + 1] = rgb[o + 2] = 0; }
      else { rgb[o] = cr; rgb[o + 1] = cg; rgb[o + 2] = cb; }
    }
  }
  if (stbi_write_png(path.c_str(), res, res, 3, rgb.data(), res * 3) == 0) {
    std::cerr << "[dualc_slice] error: failed to write PNG '" << path << "'\n";
    return 2;
  }
  return 0;
}

// Linear edge interpolation: where does the zero crossing sit between corner
// values va (at coordA) and vb (at coordB)?
double interp(double va, double vb, double coordA, double coordB) {
  const double d = va - vb;
  if (d == 0.0) return 0.5 * (coordA + coordB);
  return coordA + (va / d) * (coordB - coordA);
}

// Marching squares over the (res-1)^2 cells; pure function of the grid + world
// mapping (no PNG/SVG knowledge) so it doubles as the print-slicing kernel.
// Saddle cells (4 crossings) are resolved by the 4-corner average sign.
std::vector<Seg2> marchingSquaresZero(const std::vector<float>& values, int res,
                                      double uMin, double uMax, double vMin,
                                      double vMax) {
  std::vector<Seg2> segs;
  const double du = (res > 1) ? (uMax - uMin) / (res - 1) : 0.0;
  const double dv = (res > 1) ? (vMax - vMin) / (res - 1) : 0.0;
  auto at = [&](int i, int j) {
    return static_cast<double>(values[static_cast<std::size_t>(j) * res + i]);
  };
  for (int j = 0; j + 1 < res; ++j) {
    for (int i = 0; i + 1 < res; ++i) {
      const double c00 = at(i, j),       c10 = at(i + 1, j);
      const double c01 = at(i, j + 1),   c11 = at(i + 1, j + 1);
      const double ui = uMin + du * i,   ui1 = uMin + du * (i + 1);
      const double vj = vMin + dv * j,   vj1 = vMin + dv * (j + 1);

      // Crossings on the four edges (A bottom, B right, C top, D left).
      bool hA = (c00 < 0) != (c10 < 0);
      bool hB = (c10 < 0) != (c11 < 0);
      bool hC = (c11 < 0) != (c01 < 0);
      bool hD = (c01 < 0) != (c00 < 0);
      const double ax = interp(c00, c10, ui, ui1), ay = vj;      // bottom
      const double bx = ui1, by = interp(c10, c11, vj, vj1);     // right
      const double cx = interp(c11, c01, ui1, ui), cy = vj1;     // top
      const double dx = ui, dy = interp(c01, c00, vj1, vj);      // left

      const int count = (hA ? 1 : 0) + (hB ? 1 : 0) + (hC ? 1 : 0) + (hD ? 1 : 0);
      if (count == 2) {
        // count == 2 fills both slots; the zero-init is for GCC 13's
        // -Wmaybe-uninitialized, which cannot see that (roadmap 17 #33).
        double px[2] = {0.0, 0.0}, py[2] = {0.0, 0.0};
        int n = 0;
        if (hA) { px[n] = ax; py[n] = ay; ++n; }
        if (hB) { px[n] = bx; py[n] = by; ++n; }
        if (hC) { px[n] = cx; py[n] = cy; ++n; }
        if (hD) { px[n] = dx; py[n] = dy; ++n; }
        segs.push_back({px[0], py[0], px[1], py[1]});
      } else if (count == 4) {
        const double center = 0.25 * (c00 + c10 + c11 + c01);
        if ((center < 0) == (c00 < 0)) {  // c00/c11 grouped with centre
          segs.push_back({ax, ay, dx, dy});   // isolate c00 corner (A-D)
          segs.push_back({bx, by, cx, cy});   // isolate c11 corner (B-C)
        } else {
          segs.push_back({ax, ay, bx, by});   // isolate c10 corner (A-B)
          segs.push_back({cx, cy, dx, dy});   // isolate c01 corner (C-D)
        }
      }
    }
  }
  return segs;
}

// SVG of the contour. World v points up; SVG y points down, so we flip
// y' = vMin + vMax - v to keep PNG and SVG in the same orientation.
int writeContourSvg(const std::vector<Seg2>& segs, double uMin, double uMax,
                    double vMin, double vMax, const std::string& path) {
  std::ofstream os(path);
  if (!os) {
    std::cerr << "[dualc_slice] error: cannot open '" << path << "'\n";
    return 2;
  }
  const double w = uMax - uMin, h = vMax - vMin;
  const double sw = 0.0015 * std::max(w, h);  // stroke width ~ 0.15% of extent
  auto fy = [&](double v) { return vMin + vMax - v; };
  os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  os << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << uMin << " "
     << vMin << " " << w << " " << h << "\">\n";
  os << "<g stroke=\"black\" stroke-width=\"" << sw
     << "\" fill=\"none\" stroke-linecap=\"round\">\n";
  for (const Seg2& s : segs) {
    os << "<line x1=\"" << s.x0 << "\" y1=\"" << fy(s.y0) << "\" x2=\"" << s.x1
       << "\" y2=\"" << fy(s.y1) << "\"/>\n";
  }
  os << "</g>\n</svg>\n";
  if (!os) {
    std::cerr << "[dualc_slice] error: failed writing SVG '" << path << "'\n";
    return 2;
  }
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" ||
      std::string(argv[1]) == "-h") {
    printUsage();
    return argc < 2 ? 2 : 0;
  }

  std::string inPath, outPath, type = "gyroid";
  double wavelength = -1.0;
  double offset = 0.0;
  bool normalizeThickness = false;
  bool haveBounds = false, havePlane = false;
  dualc::BBox userBounds;
  int planeAxis = 2;       // default z
  double planePos = 0.0;
  int res = 512;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_slice] " << f << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    } else if (a == "--type") {
      const char* v = needValue("--type"); if (!v) return 2; type = v;
    } else if (a == "--wavelength") {
      const char* v = needValue("--wavelength"); if (!v) return 2;
      wavelength = std::atof(v);
    } else if (a == "--offset") {
      const char* v = needValue("--offset"); if (!v) return 2;
      offset = std::atof(v);
    } else if (a == "--normalize-thickness") {
      normalizeThickness = true;
    } else if (a == "--plane") {
      const char* v = needValue("--plane"); if (!v) return 2;
      if (!parsePlane(v, planeAxis, planePos)) {
        std::cerr << "[dualc_slice] --plane expects AXIS=VALUE (e.g. z=0)\n";
        return 2;
      }
      havePlane = true;
    } else if (a == "--res") {
      const char* v = needValue("--res"); if (!v) return 2;
      res = std::atoi(v);
      if (res < 2) res = 2;
      if (res > 4096) res = 4096;
    } else if (a == "--bounds") {
      const char* v = needValue("--bounds"); if (!v) return 2;
      if (!dce::parseBounds(v, userBounds)) {
        std::cerr << "[dualc_slice] --bounds expects six comma-separated "
                     "doubles\n";
        return 2;
      }
      haveBounds = true;
    } else if (a == "-o") {
      const char* v = needValue("-o"); if (!v) return 2; outPath = v;
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_slice] unknown flag: " << a << "\n";
      return 2;
    } else if (inPath.empty()) {
      inPath = a;
    } else {
      std::cerr << "[dualc_slice] unexpected positional arg: " << a << "\n";
      return 2;
    }
  }

  if (inPath.empty()) {
    std::cerr << "[dualc_slice] no input mesh given\n";
    printUsage();
    return 2;
  }
  if (dce::makeTpmsField(type, Vector3{0, 0, 0}, 1.0) == nullptr) {
    std::cerr << "[dualc_slice] unknown --type '" << type << "'\n";
    return 2;
  }
  if (offset < 0.0) {
    std::cerr << "[dualc_slice] --offset must be >= 0\n";
    return 2;
  }
  if (outPath.empty()) outPath = type + "_slice.png";
  const std::string pngPath = withExt(outPath, ".png");
  const std::string svgPath = withExt(outPath, ".svg");

  // Load the input mesh; keep it alive so the MeshSource stays valid.
  std::cout << "[dualc_slice] reading " << inPath << "\n";
  auto [mesh, geom] = geometrycentral::surface::readSurfaceMesh(inPath);
  std::cout << "[dualc_slice] input: " << mesh->nVertices() << " verts, "
            << mesh->nFaces() << " faces\n";

  const dualc::BBox aabb = meshAABB(*mesh, *geom);
  if (wavelength <= 0.0) {
    const Vector3 e = aabb.extent();
    wavelength = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z) / 10.0;
  }
  if (!haveBounds) userBounds = padBBox(aabb, 0.05);
  if (!havePlane) planePos = comp(userBounds.center(), planeAxis);

  // Compose the field tree -- identical to dualc_lattice so we slice the very
  // field that would be manufactured.
  FieldPtr tpms = dce::makeTpmsField(type, aabb.center(), wavelength);
  FieldPtr lattice = tpms;
  if (offset > 0.0) {
    if (normalizeThickness) lattice = dualc::normalizedOf(lattice);
    lattice = dualc::onionOf(lattice, offset);
  } else if (normalizeThickness) {
    lattice = dualc::normalizedOf(lattice);
  }
  auto src = std::make_shared<dualc::MeshSource>(*mesh, *geom);
  FieldPtr clipped = dualc::intersectionOf(lattice, src);

  // In-plane axes (u, v): z-plane -> (x, y), x-plane -> (y, z), y-plane -> (x, z).
  const int uAxis = (planeAxis == 0) ? 1 : 0;
  const int vAxis = (planeAxis == 2) ? 1 : 2;
  const double uMin = comp(userBounds.min, uAxis), uMax = comp(userBounds.max, uAxis);
  const double vMin = comp(userBounds.min, vAxis), vMax = comp(userBounds.max, vAxis);
  const double aMin = comp(userBounds.min, planeAxis);
  const double aMax = comp(userBounds.max, planeAxis);
  const char axisName[3] = {'x', 'y', 'z'};

  if (planePos < aMin || planePos > aMax) {
    std::cerr << "[dualc_slice] warning: plane " << axisName[planeAxis] << "="
              << planePos << " is outside the sampling bounds ["
              << aMin << ", " << aMax << "]\n";
  }

  std::cout << "[dualc_slice] type = " << type << ", wavelength = " << wavelength
            << ", offset = " << offset
            << (normalizeThickness ? " (normalized)" : "") << "\n";
  std::cout << "[dualc_slice] plane " << axisName[planeAxis] << "=" << planePos
            << ", res = " << res << "x" << res << ", pixel = "
            << (uMax - uMin) / (res - 1) << " x " << (vMax - vMin) / (res - 1)
            << " world units\n";

  const std::vector<float> values =
      sampleSlice(*clipped, planeAxis, planePos, uAxis, vAxis, uMin, uMax, vMin,
                  vMax, res);

  const std::vector<Seg2> segs =
      marchingSquaresZero(values, res, uMin, uMax, vMin, vMax);
  if (segs.empty()) {
    std::cerr << "[dualc_slice] warning: the field has a single sign on this "
                 "plane -- empty cross-section.\n";
  }

  int rc = writeHeatmapPng(values, res, pngPath, /*burnContour=*/true);
  if (rc != 0) return rc;
  rc = writeContourSvg(segs, uMin, uMax, vMin, vMax, svgPath);
  if (rc != 0) return rc;

  std::cout << "[dualc_slice] wrote " << pngPath << " and " << svgPath << " ("
            << segs.size() << " contour segments)\n";
  return 0;
}
