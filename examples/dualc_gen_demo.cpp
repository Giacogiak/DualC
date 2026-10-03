#include "demo_meshes.h"

#include "geometrycentral/surface/meshio.h"

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// dualc_gen_demo -- write the procedural demo meshes (Tier 3 #12) to OBJ.
//
// Each shape exercises a distinct contouring regime (smooth curvature, genus,
// sharp CAD edges). The meshes are committed under data/ so consumers need not
// run this tool; it exists as the regeneration / provenance artifact.

namespace {

using dce::MeshAndGeom;

struct Shape {
  std::string name;
  std::string defaultOut;
  std::string blurb;
  std::function<MeshAndGeom(int)> build; // arg = depth (only genus2 uses it)
};

const std::vector<Shape>& shapes() {
  static const std::vector<Shape> s = {
      {"cube", "cube.obj", "unit cube, 12 triangles, the recipes' input, chi=2",
       [](int) { return dce::makeCube(); }},
      {"sphere", "sphere.obj", "icosphere, smooth curvature, chi=2",
       [](int) { return dce::makeIcosphere(); }},
      {"uvsphere", "uvsphere.obj", "lat/long sphere with poles, chi=2",
       [](int) { return dce::makeUvSphere(); }},
      {"torus", "torus.obj", "doubly-periodic torus, genus 1, chi=0",
       [](int) { return dce::makeTorus(); }},
      {"knot", "knot.obj", "trefoil-knot tube, genus 1, chi=0",
       [](int) { return dce::makeTrefoilKnot(); }},
      {"genus2", "genus2.obj", "double torus, genus 2, chi=-2",
       [](int d) { return dce::makeGenus2(d > 0 ? d : 7); }},
      {"cylinder", "cylinder.obj", "cylinder, sharp circular rims, chi=2",
       [](int) { return dce::makeCylinder(); }},
      {"bracket", "bracket.obj", "L-bracket, convex+concave sharp edges, chi=2",
       [](int) { return dce::makeLBracket(); }},
      {"hexbore", "hexbore.obj", "hex prism with bore, genus 1, chi=0",
       [](int) { return dce::makeHexPrismBore(); }},
  };
  return s;
}

void printUsage() {
  std::cerr <<
      "dualc_gen_demo -- generate procedural demo meshes\n"
      "\n"
      "USAGE\n"
      "  dualc_gen_demo <shape> [-o out.obj] [--depth N]\n"
      "  dualc_gen_demo all [--dir DIR] [--depth N]\n"
      "\n"
      "  --depth N   octree depth for the genus2 mesh (it is contoured by\n"
      "              DualC; ignored by the purely parametric shapes).\n"
      "  --dir DIR   output directory prefix for `all` (default: current).\n"
      "  -o PATH     output path for a single shape (default: <shape>.obj).\n"
      "  --help,-h   this message.\n"
      "\n"
      "SHAPES\n";
  for (const auto& sh : shapes())
    std::cerr << "  " << sh.name << std::string(10 - sh.name.size(), ' ')
              << sh.blurb << "\n";
}

int writeMesh(const Shape& sh, const std::string& outPath, int depth) {
  auto [mesh, geom] = sh.build(depth);
  geometrycentral::surface::writeSurfaceMesh(*mesh, *geom, outPath);
  std::cout << "[dualc_gen_demo] wrote " << outPath << " ("
            << mesh->nVertices() << " verts, " << mesh->nFaces()
            << " faces)\n";
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    printUsage();
    return 2;
  }
  const std::string cmd = argv[1];
  if (cmd == "--help" || cmd == "-h") {
    printUsage();
    return 0;
  }

  std::string outPath, dir;
  int depth = -1;
  for (int i = 2; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-o" && i + 1 < argc) {
      outPath = argv[++i];
    } else if (a == "--dir" && i + 1 < argc) {
      dir = argv[++i];
      if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '/';
    } else if (a == "--depth" && i + 1 < argc) {
      depth = std::atoi(argv[++i]);
    } else {
      std::cerr << "[dualc_gen_demo] unknown argument: " << a << "\n";
      printUsage();
      return 2;
    }
  }

  if (cmd == "all") {
    for (const auto& sh : shapes()) writeMesh(sh, dir + sh.defaultOut, depth);
    return 0;
  }

  for (const auto& sh : shapes()) {
    if (sh.name == cmd)
      return writeMesh(sh, outPath.empty() ? sh.defaultOut : outPath, depth);
  }
  std::cerr << "[dualc_gen_demo] unknown shape: " << cmd << "\n";
  printUsage();
  return 2;
}
