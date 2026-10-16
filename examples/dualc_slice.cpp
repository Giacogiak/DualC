#include "example_common.h"
#include "field_slice.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

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
// shows the very field that would be manufactured. The sampling, colormap and
// marching squares live in field_slice.{h,cpp}, shared with dualc_field --slice.

using dualc::FieldPtr;
using dualc::Vector3;

namespace {

double comp(const Vector3& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }

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
      "                         Sampling region. Default: input AABB padded 5%.\n"
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
  int res = dce::kSliceDefaultRes;

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
      if (!dce::parseSlicePlane(v, planeAxis, planePos)) {
        std::cerr << "[dualc_slice] --plane expects AXIS=VALUE (e.g. z=0)\n";
        return 2;
      }
      havePlane = true;
    } else if (a == "--res") {
      const char* v = needValue("--res"); if (!v) return 2;
      res = dce::clampSliceRes(std::atoi(v));
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
  const std::string pngPath = dce::withExt(outPath, ".png");
  const std::string svgPath = dce::withExt(outPath, ".svg");

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

  std::cout << "[dualc_slice] type = " << type << ", wavelength = " << wavelength
            << ", offset = " << offset
            << (normalizeThickness ? " (normalized)" : "") << "\n";
  return dce::writeFieldSlice(*clipped, userBounds, planeAxis, planePos, res,
                              pngPath, svgPath, "dualc_slice");
}
