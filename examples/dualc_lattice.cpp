#include "example_common.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// dualc_lattice -- TPMS-lattice infill bounded by an input mesh.
//
// Workflow:
//   1. Load the input mesh as a closed volume (MeshSource).
//   2. Build a periodic TPMS implicit field of the requested type.
//   3. Optionally normalise it (`--normalize-thickness`) so subsequent offset
//      thickness is metrically meaningful.
//   4. Optionally take the closed double-sheet shell `|F| - t` via onionOf
//      (the "thick-walled" mode); without --offset the bare 0-isosurface of
//      the TPMS-and-mesh intersection is contoured (the "solid phase" mode).
//   5. Clip to the input volume: intersectionOf(lattice, MeshSource).
//   6. Dual-contour the result and write the OBJ.
//
// Both output modes produce a closed manifold solid bounded by the input
// mesh; mode (a) is a single phase of the lattice clipped to the mesh, mode
// (b) is a thin wall hugging the medial 0-isosurface on both sides.

using dualc::FieldPtr;
using dualc::Vector3;

namespace {

void printUsage() {
  std::cerr <<
      "dualc_lattice -- TPMS-lattice infill bounded by an input mesh.\n"
      "\n"
      "USAGE\n"
      "  dualc_lattice <input.obj> [options]\n"
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
      "                         AABB diagonal / 10, giving ~10 cells across.\n"
      "  --offset T             Thick-wall offset (world units). T == 0\n"
      "                         (default) contours the lattice's solid phase\n"
      "                         clipped to the mesh; T > 0 contours the\n"
      "                         closed double-sheet shell |F|-T clipped to\n"
      "                         the mesh. Both modes produce a closed\n"
      "                         manifold solid.\n"
      "  --normalize-thickness  Normalise the TPMS by 1/|grad F| before\n"
      "                         applying --offset, so T is close to the\n"
      "                         true metric wall thickness. Raw TPMS values\n"
      "                         are NOT signed distances, so without this\n"
      "                         flag T is proportional to thickness but not\n"
      "                         equal to it.\n"
      "  --depth N              Octree max depth (default 7).\n"
      "  --collapse E           Adaptive cell-collapse threshold (default 0).\n"
      "  --bounds x0,y0,z0,x1,y1,z1\n"
      "                         Explicit sampling region. Default: the input\n"
      "                         mesh's AABB padded by 5%%. The intersection\n"
      "                         clips the lattice to the mesh, so the box\n"
      "                         only needs to contain the mesh.\n"
      "  -o PATH                Output mesh; the format follows the extension:\n"
      "                         .obj (default), .stl (binary), or .3mf\n"
      "                         (3MF-mesh, 1 unit = 1 mm). Default:\n"
      "                         <type>_lattice.obj.\n"
      "  --help, -h             Show this message.\n"
      "\n"
      "EXAMPLES\n"
      "  dualc_lattice cube.obj\n"
      "  dualc_lattice cube.obj --type schwarz-p --wavelength 0.5\n"
      "                         --offset 0.05 --depth 7\n"
      "  dualc_lattice molde.obj --type gyroid --offset 1.0\n"
      "                         --normalize-thickness --depth 8 -o gyroid_molde.obj\n";
}

// Compute the input mesh's AABB from its vertex positions.
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

  std::string inPath;
  std::string outPath;
  std::string type = "gyroid";
  double wavelength = -1.0;  // <0 => derive from mesh AABB
  double offset = 0.0;
  bool   normalizeThickness = false;
  bool   haveBounds = false;
  dualc::BBox userBounds;
  dualc::SamplerParams sp;
  dualc::ContourerParams cp;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_lattice] " << f << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    } else if (a == "--type") {
      const char* v = needValue("--type");
      if (!v) return 2;
      type = v;
    } else if (a == "--wavelength") {
      const char* v = needValue("--wavelength");
      if (!v) return 2;
      wavelength = std::atof(v);
    } else if (a == "--offset") {
      const char* v = needValue("--offset");
      if (!v) return 2;
      offset = std::atof(v);
    } else if (a == "--normalize-thickness") {
      normalizeThickness = true;
    } else if (a == "--depth") {
      const char* v = needValue("--depth");
      if (!v) return 2;
      sp.maxDepth = std::atoi(v);
    } else if (a == "--collapse") {
      const char* v = needValue("--collapse");
      if (!v) return 2;
      cp.simplificationError = std::atof(v);
    } else if (a == "--bounds") {
      const char* v = needValue("--bounds");
      if (!v) return 2;
      if (!dce::parseBounds(v, userBounds)) {
        std::cerr << "[dualc_lattice] --bounds expects six "
                     "comma-separated doubles\n";
        return 2;
      }
      haveBounds = true;
    } else if (a == "-o") {
      const char* v = needValue("-o");
      if (!v) return 2;
      outPath = v;
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_lattice] unknown flag: " << a << "\n";
      return 2;
    } else if (inPath.empty()) {
      inPath = a;
    } else {
      std::cerr << "[dualc_lattice] unexpected positional arg: " << a << "\n";
      return 2;
    }
  }

  if (inPath.empty()) {
    std::cerr << "[dualc_lattice] no input mesh given\n";
    printUsage();
    return 2;
  }
  if (dce::makeTpmsField(type, Vector3{0, 0, 0}, 1.0) == nullptr) {
    std::cerr << "[dualc_lattice] unknown --type '" << type << "'\n";
    return 2;
  }
  if (offset < 0.0) {
    std::cerr << "[dualc_lattice] --offset must be >= 0\n";
    return 2;
  }
  if (outPath.empty()) outPath = type + "_lattice.obj";

  // Load the input mesh; keep it alive in this scope so the MeshSource it
  // backs remains valid through dual contouring.
  std::cout << "[dualc_lattice] reading " << inPath << "\n";
  auto [mesh, geom] = geometrycentral::surface::readSurfaceMesh(inPath);
  std::cout << "[dualc_lattice] input: " << mesh->nVertices() << " verts, "
            << mesh->nFaces() << " faces\n";

  const dualc::BBox aabb = meshAABB(*mesh, *geom);
  if (wavelength <= 0.0) {
    const Vector3 e = aabb.extent();
    const double diag = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);
    wavelength = diag / 10.0;
  }
  if (!haveBounds) {
    userBounds = padBBox(aabb, 0.05);
  }
  sp.rootBounds = userBounds;

  // Compose the field tree.
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

  std::cout << "[dualc_lattice] type = " << type
            << ", wavelength = " << wavelength
            << ", offset = " << offset
            << (normalizeThickness ? " (normalized)" : "")
            << ", depth = " << sp.maxDepth << "\n";

  return dce::writeField(*clipped, outPath, sp, cp);
}
