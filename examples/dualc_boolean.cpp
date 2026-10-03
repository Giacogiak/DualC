#include "example_common.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

// dualc_boolean -- combine two triangle meshes with an SDF boolean (hard or
// smooth) and dual-contour the result.

namespace {

void printUsage() {
  std::cerr <<
      "dualc_boolean -- SDF boolean of two meshes, dual-contoured.\n"
      "\n"
      "USAGE\n"
      "  dualc_boolean <op> a.obj b.obj [options]\n"
      "\n"
      "OPS\n"
      "  union  intersection  difference  xor\n"
      "  smooth-union  smooth-intersection  smooth-difference\n"
      "\n"
      "OPTIONS\n"
      "  -o PATH        Output mesh; format follows the extension: .obj\n"
      "                 (default), .stl (binary), or .3mf (1 unit = 1 mm).\n"
      "                 Default boolean.obj.\n"
      "  --k K          Blend radius for the smooth ops (default 0.25).\n"
      "  --depth N      Octree max depth (default 7).\n"
      "  --collapse E   Adaptive cell-collapse threshold (default 0).\n"
      "  --sharp        Use face normals (no smooth-normal interpolation).\n"
      "  --bake N       Accelerate huge meshes by baking each one into an\n"
      "                 N^3 narrow-band signed-distance grid before combining.\n"
      "                 This is a speed/quality trade -- the grid rounds off\n"
      "                 sharp features. Default: no baking; the meshes are\n"
      "                 contoured directly, which is exact and keeps sharp\n"
      "                 edges crisp.\n"
      "  --help, -h     Show this message.\n"
      "\n"
      "NOTES\n"
      "  --k is in world units: pick it on the order of the model size, not a\n"
      "  fixed small number -- a blend smaller than one contour cell is\n"
      "  invisible.\n"
      "\n"
      "EXAMPLES\n"
      "  dualc_boolean difference a.obj b.obj -o a_minus_b.obj\n"
      "  dualc_boolean smooth-union a.obj b.obj --k 30 -o blend.obj\n";
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" ||
      std::string(argv[1]) == "-h") {
    printUsage();
    return argc < 2 ? 2 : 0;
  }

  const std::string op = argv[1];
  dce::BoolOp boolOp;
  if (!dce::parseBoolOp(op, boolOp)) {
    std::cerr << "[dualc_boolean] unknown op: " << op << "\n";
    printUsage();
    return 2;
  }
  std::string aPath, bPath, outPath = "boolean.obj";
  double k = 0.25;
  bool sharp = false;
  int  bakeRes = -1;   // < 2 => direct contouring; >= 2 => bake at this res
  dualc::SamplerParams sp;
  dualc::ContourerParams cp;

  std::vector<std::string> positional;
  for (int i = 2; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_boolean] " << f << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    } else if (a == "-o") {
      const char* v = needValue("-o");
      if (!v) return 2;
      outPath = v;
    } else if (a == "--k") {
      const char* v = needValue("--k");
      if (!v) return 2;
      k = std::atof(v);
    } else if (a == "--depth") {
      const char* v = needValue("--depth");
      if (!v) return 2;
      sp.maxDepth = std::atoi(v);
    } else if (a == "--collapse") {
      const char* v = needValue("--collapse");
      if (!v) return 2;
      cp.simplificationError = std::atof(v);
    } else if (a == "--sharp") {
      sharp = true;
    } else if (a == "--bake") {
      const char* v = needValue("--bake");
      if (!v) return 2;
      bakeRes = std::atoi(v);
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_boolean] unknown flag: " << a << "\n";
      return 2;
    } else {
      positional.push_back(a);
    }
  }

  if (positional.size() != 2) {
    std::cerr << "[dualc_boolean] expected two input meshes\n";
    printUsage();
    return 2;
  }
  aPath = positional[0];
  bPath = positional[1];

  std::cout << "[dualc_boolean] " << op << "  " << aPath << "  " << bPath
            << "\n";
  auto [meshA, geomA] = geometrycentral::surface::readSurfaceMesh(aPath);
  auto [meshB, geomB] = geometrycentral::surface::readSurfaceMesh(bPath);

  // MeshSource holds references into mesh/geometry -- meshA..geomB above must
  // outlive the field tree, which they do (they live to the end of main).
  auto fa = std::make_shared<dualc::MeshSource>(*meshA, *geomA, !sharp);
  auto fb = std::make_shared<dualc::MeshSource>(*meshB, *geomB, !sharp);

  // Combined input AABB -- drives both the tiny-k hint and the bake region.
  dualc::FieldPtr a = fa;
  dualc::FieldPtr b = fb;
  const dualc::BBox ba = fa->bounds();
  const dualc::BBox bb = fb->bounds();
  dualc::BBox inputAABB;
  inputAABB.min = dualc::Vector3{std::min(ba.min.x, bb.min.x),
                                 std::min(ba.min.y, bb.min.y),
                                 std::min(ba.min.z, bb.min.z)};
  inputAABB.max = dualc::Vector3{std::max(ba.max.x, bb.max.x),
                                 std::max(ba.max.y, bb.max.y),
                                 std::max(ba.max.z, bb.max.z)};
  const dualc::Vector3 inExt = inputAABB.extent();
  const double modelExtent = std::max({inExt.x, inExt.y, inExt.z, 1e-9});
  const bool   isSmoothOp  = dce::boolOpIsSmooth(boolOp);

  // `k` is in world units. A blend much smaller than a contour cell is
  // invisible -- warn rather than let the user think the op is broken.
  if (isSmoothOp && k > 0.0 && k < 0.01 * modelExtent) {
    std::cerr << "[dualc_boolean] warning: --k " << k << " is tiny next to "
                 "the model (extent ~" << modelExtent << "); the blend "
                 "fillet will be far smaller than one contour cell and "
                 "invisible. Pick a k on the order of the model size.\n";
  }

  // Default: contour the two MeshSources directly -- exact distances and
  // normals, sharp features preserved. `--bake N` opts into a narrow-band
  // SDF-grid bake: an O(1)-lookup speed/quality trade for very large meshes.
  if (bakeRes >= 2) {
    dualc::BBox region = inputAABB;
    const double pad = 0.10 * modelExtent + std::max(0.0, k);
    region.min = region.min - dualc::Vector3{pad, pad, pad};
    region.max = region.max + dualc::Vector3{pad, pad, pad};

    // Narrow-band width: exact SDF within k of each mesh (so the smooth
    // blend stays exact) plus 3 cells of contour-sampling margin.
    const dualc::Vector3 cext = region.extent();
    const double maxCell =
        std::max({cext.x, cext.y, cext.z}) / (bakeRes - 1);
    const double bandWidth =
        (isSmoothOp ? std::max(0.0, k) : 0.0) + 3.0 * maxCell;
    const dualc::Vector3i bakeDim{bakeRes, bakeRes, bakeRes};

    const auto t0 = std::chrono::steady_clock::now();
    a = fa->bakeToGrid(region, bakeDim, bandWidth);
    b = fb->bakeToGrid(region, bakeDim, bandWidth);
    const auto t1 = std::chrono::steady_clock::now();
    const double secs = std::chrono::duration<double>(t1 - t0).count();
    const double mb = static_cast<double>(bakeRes) * bakeRes * bakeRes *
                      sizeof(float) / (1024.0 * 1024.0);
    std::cout << "[dualc_boolean] baked 2 x " << bakeRes
              << "^3 narrow-band SDF grids (" << (2.0 * mb) << " MB) in "
              << secs << " s\n";
  }

  dualc::FieldPtr field = dce::applyBoolOp(boolOp, a, b, k);

  return dce::writeField(*field, outPath, sp, cp);
}
