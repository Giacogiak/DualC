#include "example_common.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// dualc_csg_demo -- baked field-tree recipes. These showcase composition,
// mesh + primitive booleans, and the `displaced` operator (whose function
// argument cannot be expressed as a plain CLI flag). The recipe registry lives
// in example_common (dce::), shared with the interactive viewer (dualc_view),
// where each recipe's baked constants become tunable sliders. This CLI builds
// every recipe at its defaults, reproducing the original output.

using dualc::FieldPtr;

namespace {

void printList() {
  std::cout << "RECIPES\n";
  for (const dce::RecipeEntry& r : dce::csgRecipes()) {
    std::cout << "  " << r.name << (r.needsMesh ? "  <input.obj>" : "")
              << "\n      " << r.blurb << "\n";
  }
}

void printUsage() {
  std::cerr <<
      "dualc_csg_demo -- baked implicit-field recipes, dual-contoured.\n"
      "\n"
      "USAGE\n"
      "  dualc_csg_demo --recipe=NAME [input.obj] [-o out.EXT] [--depth N]\n"
      "  dualc_csg_demo --list\n"
      "\n"
      "  Recipes whose name starts with 'mesh-' or 'twisted-mesh' need an\n"
      "  input .obj; the others are self-contained.\n"
      "  -o's format follows the extension: .obj (default), .stl, or .3mf.\n"
      "\n"
      "EXAMPLES\n"
      "  dualc_csg_demo --recipe=smooth-blend\n"
      "  dualc_csg_demo --recipe=mesh-minus-sphere cube.obj -o carved.obj\n";
}

} // namespace

int main(int argc, char** argv) {
  std::string recipeName, inPath, outPath;
  dualc::SamplerParams sp;
  dualc::ContourerParams cp;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_csg_demo] " << f << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    } else if (a == "--list") {
      printList();
      return 0;
    } else if (a.rfind("--recipe=", 0) == 0) {
      recipeName = a.substr(9);
    } else if (a == "--recipe") {
      const char* v = needValue("--recipe");
      if (!v) return 2;
      recipeName = v;
    } else if (a == "-o") {
      const char* v = needValue("-o");
      if (!v) return 2;
      outPath = v;
    } else if (a == "--depth") {
      const char* v = needValue("--depth");
      if (!v) return 2;
      sp.maxDepth = std::atoi(v);
    } else if (a == "--collapse") {
      const char* v = needValue("--collapse");
      if (!v) return 2;
      cp.simplificationError = std::atof(v);
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_csg_demo] unknown flag: " << a << "\n";
      return 2;
    } else {
      inPath = a;
    }
  }

  if (recipeName.empty()) {
    std::cerr << "[dualc_csg_demo] no --recipe given\n";
    printUsage();
    return 2;
  }
  const dce::RecipeEntry* recipe = dce::findRecipe(recipeName);
  if (recipe == nullptr) {
    std::cerr << "[dualc_csg_demo] unknown recipe: " << recipeName
              << "  (try --list)\n";
    return 2;
  }
  if (outPath.empty()) outPath = recipeName + ".obj";

  // Mesh recipes need an input; keep the loaded mesh/geometry alive in this
  // scope so the MeshSource references stay valid through contouring.
  std::unique_ptr<geometrycentral::surface::SurfaceMesh> mesh;
  std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> geom;
  if (recipe->needsMesh) {
    if (inPath.empty()) {
      std::cerr << "[dualc_csg_demo] recipe '" << recipeName
                << "' needs an input .obj\n";
      return 2;
    }
    auto loaded = geometrycentral::surface::readSurfaceMesh(inPath);
    mesh = std::move(std::get<0>(loaded));
    geom = std::move(std::get<1>(loaded));
  }

  // Build at defaults (empty param list) -- reproduces the original recipe.
  FieldPtr field =
      dce::buildRecipe(recipeName, {}, mesh.get(), geom.get());
  if (!field) {
    std::cerr << "[dualc_csg_demo] failed to build recipe: " << recipeName
              << "\n";
    return 2;
  }

  std::cout << "[dualc_csg_demo] recipe '" << recipeName << "'\n";
  return dce::writeField(*field, outPath, sp, cp);
}
