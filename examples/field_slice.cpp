#include "field_slice.h"

#include "stb_image_write.h"  // declarations only; impl in third_party/stb_impl.cpp

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// The slice core, moved here from dualc_slice.cpp unchanged in what it writes
// (roadmap 21 #59): dualc_slice and dualc_field --slice both call
// writeFieldSlice, so their PNG and SVG agree byte for byte on the same field.

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
                    const std::string& path, bool burnContour,
                    const std::string& tag) {
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
    std::cerr << "[" << tag << "] error: failed to write PNG '" << path << "'\n";
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
                    double vMin, double vMax, const std::string& path,
                    const std::string& tag) {
  std::ofstream os(path);
  if (!os) {
    std::cerr << "[" << tag << "] error: cannot open '" << path << "'\n";
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
    std::cerr << "[" << tag << "] error: failed writing SVG '" << path << "'\n";
    return 2;
  }
  return 0;
}

}  // namespace

namespace dce {

int clampSliceRes(int res) {
  return std::min(std::max(res, kSliceMinRes), kSliceMaxRes);
}

bool parseSlicePlane(const std::string& s, int& axis, double& value) {
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

std::string withExt(const std::string& base, const std::string& ext) {
  const std::size_t slash = base.find_last_of("/\\");
  const std::size_t dot = base.find_last_of('.');
  const bool hasExt = dot != std::string::npos &&
                      (slash == std::string::npos || dot > slash);
  return (hasExt ? base.substr(0, dot) : base) + ext;
}

std::string stripSliceExt(const std::string& path) {
  if (path.size() < 4) return path;
  std::string ext = path.substr(path.size() - 4);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return (ext == ".png" || ext == ".svg") ? path.substr(0, path.size() - 4)
                                          : path;
}

int writeFieldSlice(const dualc::ImplicitField& field, const dualc::BBox& region,
                    int axis, double pos, int res, const std::string& pngPath,
                    const std::string& svgPath, const std::string& tag) {
  // In-plane axes (u, v): z-plane -> (x, y), x-plane -> (y, z), y-plane -> (x, z).
  const int uAxis = (axis == 0) ? 1 : 0;
  const int vAxis = (axis == 2) ? 1 : 2;
  const double uMin = comp(region.min, uAxis), uMax = comp(region.max, uAxis);
  const double vMin = comp(region.min, vAxis), vMax = comp(region.max, vAxis);
  const double aMin = comp(region.min, axis);
  const double aMax = comp(region.max, axis);
  const char axisName[3] = {'x', 'y', 'z'};

  if (pos < aMin || pos > aMax) {
    std::cerr << "[" << tag << "] warning: plane " << axisName[axis] << "="
              << pos << " is outside the sampling bounds [" << aMin << ", "
              << aMax << "]\n";
  }
  std::cout << "[" << tag << "] plane " << axisName[axis] << "=" << pos
            << ", res = " << res << "x" << res << ", pixel = "
            << (uMax - uMin) / (res - 1) << " x " << (vMax - vMin) / (res - 1)
            << " world units\n";

  const std::vector<float> values =
      sampleSlice(field, axis, pos, uAxis, vAxis, uMin, uMax, vMin, vMax, res);

  const std::vector<Seg2> segs =
      marchingSquaresZero(values, res, uMin, uMax, vMin, vMax);
  if (segs.empty()) {
    std::cerr << "[" << tag << "] warning: the field has a single sign on this "
                 "plane -- empty cross-section.\n";
  }

  int rc = writeHeatmapPng(values, res, pngPath, /*burnContour=*/true, tag);
  if (rc != 0) return rc;
  rc = writeContourSvg(segs, uMin, uMax, vMin, vMax, svgPath, tag);
  if (rc != 0) return rc;

  std::cout << "[" << tag << "] wrote " << pngPath << " and " << svgPath << " ("
            << segs.size() << " contour segments)\n";
  return 0;
}

}  // namespace dce
