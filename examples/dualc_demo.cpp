#include "dualc/dualc.h"

#include "geometrycentral/surface/halfedge_element_types.h"
#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace {

void printUsage() {
  std::cerr <<
    "dualc_demo -- dual contouring on a triangle mesh\n"
    "\n"
    "USAGE\n"
    "  dualc_demo [options] [input.obj] [output.obj]\n"
    "\n"
    "  Options can be repeated and combined. Order doesn't matter.\n"
    "\n"
    "  input.obj   path to input mesh (default: cube.obj)\n"
    "  output.obj  path for the dual-contoured output (default: cube_dc.obj)\n"
    "\n"
    "  Both formats follow geometry-central's OBJ reader/writer. The output is\n"
    "  written with `vn` lines (smooth-shaded) via WavefrontOBJ::write.\n"
    "\n"
    "OPTIONS\n"
    "  --depth N           Octree maximum depth (default 7).\n"
    "                      Cell side length = (root extent) / 2^N.\n"
    "                      Each +1 quadruples the surface cell count and the\n"
    "                      output triangle count; runtime scales similarly.\n"
    "                      Practical range on a 16 GB box: 4-10.\n"
    "                      Examples: --depth 5  (coarse, ~few seconds)\n"
    "                                --depth 7  (default, balanced)\n"
    "                                --depth 9  (fine, ~minutes, hundreds of MB)\n"
    "\n"
    "  --depth=N           Equivalent form with `=`.\n"
    "\n"
    "  --collapse E        Adaptive cell-collapse threshold (default 0, disabled).\n"
    "                      QEF residual threshold for the bottom-up\n"
    "                      Ju/Schaefer/Warren simplification pass. Higher values\n"
    "                      collapse more cells but never violate the topology-\n"
    "                      safe test (no holes, no non-manifold edges introduced).\n"
    "                      Useful range depends on mesh scale; try 0.1, 1, 10, 100.\n"
    "                      Typical reductions on organic meshes: ~5-10% triangles\n"
    "                      with no visible quality change.\n"
    "                      Examples: --collapse 1     (mild simplification)\n"
    "                                --collapse 100   (aggressive)\n"
    "\n"
    "  --collapse=E        Equivalent form with `=`.\n"
    "\n"
    "  --sharp             Use the hit triangle's geometric face normal at\n"
    "                      each Hermite-edge crossing instead of the\n"
    "                      barycentric blend of the input mesh's per-vertex\n"
    "                      normals. Default is smooth (interpolation ON).\n"
    "                      Use this for CAD-style inputs where you want 90deg\n"
    "                      corners preserved exactly. On the cube, with this\n"
    "                      flag the output corner vertices land at exactly\n"
    "                      (+/-0.5, +/-0.5, +/-0.5); without it they're rounded\n"
    "                      slightly inward by the smooth-normal QEF placement.\n"
    "                      Trade-off: sharp inputs come out sharp; smooth\n"
    "                      organic inputs (molde) come out faceted at the cell\n"
    "                      scale -- not what you want for those.\n"
    "\n"
    "  --no-manifold       Disable Manifold Dual Contouring -- place exactly\n"
    "                      one QEF vertex per leaf cell, the way DualC behaved\n"
    "                      before MDC. With MDC on (default), cells whose sign\n"
    "                      configuration encodes multiple disjoint surface\n"
    "                      components get one vertex per component, which\n"
    "                      removes the small number of intrinsic non-manifold\n"
    "                      edges that single-vertex DC leaves on pinch-cells.\n"
    "                      Use this flag only to reproduce pre-MDC reference\n"
    "                      output for regression comparisons.\n"
    "\n"
    "  --pseudonormal      Use the Baerentzen-Aanaes angle-weighted\n"
    "                      pseudonormal as the inside/outside oracle: one\n"
    "                      closest-point query per corner sign, no rays at\n"
    "                      all. Faster than the default 3-ray parity, but\n"
    "                      assumes the input mesh is WATERTIGHT and CONSISTENTLY\n"
    "                      ORIENTED -- non-watertight inputs (open shells,\n"
    "                      soup, self-intersections) will misclassify near\n"
    "                      boundaries and non-manifold features. The default\n"
    "                      (3-ray majority parity) tolerates those cases.\n"
    "\n"
    "  --gwn               Use the generalized winding number (Jacobson) as\n"
    "                      the inside/outside oracle, hierarchically\n"
    "                      accelerated. Robust on non-watertight input (open\n"
    "                      shells, soup, self-intersections) where ray parity\n"
    "                      fails. Edge crossings still come from the mesh\n"
    "                      triangles, so the input should still be close to\n"
    "                      watertight; for genuinely open input use --gwn-field.\n"
    "\n"
    "  --gwn-field         Contour the 0.5-isosurface of the generalized\n"
    "                      winding number field instead of the mesh's signed\n"
    "                      distance. Sign, value AND edge crossings all derive\n"
    "                      from the GWN scalar, so an open input mesh is sealed\n"
    "                      into a watertight solid (the hole gets a smooth\n"
    "                      cap). Overrides --pseudonormal / --gwn.\n"
    "\n"
    "  --help, -h          Show this message.\n"
    "\n"
    "INPUT REQUIREMENTS\n"
    "  - By default the input mesh must be CLOSED and consistently ORIENTED:\n"
    "    the default sign oracle uses 3-ray-stabbing parity, correct only for\n"
    "    watertight oriented meshes. For non-watertight input (open shells,\n"
    "    soup, self-intersections) use --gwn-field, which seals it into a\n"
    "    watertight solid. Triangle orientation must still be consistent.\n"
    "  - Triangle and polygon faces are both accepted (polygons are\n"
    "    fan-triangulated internally).\n"
    "  - Per-vertex normals from the input's `vn` lines are NOT used directly\n"
    "    (geometry-central's loader discards them); angle-weighted vertex\n"
    "    normals are computed from positions+topology and used for smooth\n"
    "    Hermite-edge sampling.\n"
    "\n"
    "EXAMPLES\n"
    "  # Defaults: read cube.obj, write cube_dc.obj at depth 7, no collapse.\n"
    "  dualc_demo\n"
    "\n"
    "  # Coarse output for fast iteration:\n"
    "  dualc_demo --depth 5 cube.obj cube_dc.obj\n"
    "\n"
    "  # Default depth, mild simplification:\n"
    "  dualc_demo --collapse 1 molde.obj molde_dc.obj\n"
    "\n"
    "  # High depth + aggressive collapse to keep file size bounded:\n"
    "  dualc_demo --depth 9 --collapse 100 molde.obj molde_dc_d9_e100.obj\n"
    "\n"
    "  # CAD-style sharp-corner output for a cube (no smooth normals):\n"
    "  dualc_demo --sharp cube.obj cube_sharp.obj\n"
    "\n"
    "  # Just the input (writes default output path):\n"
    "  dualc_demo molde.obj\n";
}

} // namespace

int main(int argc, char** argv) {
  std::string inPath  = "cube.obj";
  std::string outPath = "cube_dc.obj";
  int    maxDepth   = -1;   // -1 = use SamplerParams default
  double collapseE  = -1.0; // <0 = use ContourerParams default (= 0, off)
  bool   sharp      = false; // when true, disable smooth-normal interpolation
  bool   noManifold = false; // when true, disable manifold dual contouring
  bool   pseudoNormal = false; // when true, use PSEUDONORMAL sign oracle
  bool   gwnSign  = false;  // when true, use GENERALIZED_WINDING_NUMBER sign
  bool   gwnField = false;  // when true, contour a WindingNumberField

  std::vector<std::string> positional;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    }
    if (a == "--depth") {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_demo] --depth requires a value\n";
        return 2;
      }
      maxDepth = std::atoi(argv[++i]);
      continue;
    }
    if (a.rfind("--depth=", 0) == 0) {
      maxDepth = std::atoi(a.c_str() + 8);
      continue;
    }
    if (a == "--collapse") {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_demo] --collapse requires a value\n";
        return 2;
      }
      collapseE = std::atof(argv[++i]);
      continue;
    }
    if (a.rfind("--collapse=", 0) == 0) {
      collapseE = std::atof(a.c_str() + 11);
      continue;
    }
    if (a == "--sharp") {
      sharp = true;
      continue;
    }
    if (a == "--no-manifold") {
      noManifold = true;
      continue;
    }
    if (a == "--pseudonormal") {
      pseudoNormal = true;
      continue;
    }
    if (a == "--gwn") {
      gwnSign = true;
      continue;
    }
    if (a == "--gwn-field") {
      gwnField = true;
      continue;
    }
    if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_demo] unknown flag: " << a << "\n";
      printUsage();
      return 2;
    }
    positional.push_back(a);
  }
  if (positional.size() >= 1) inPath  = positional[0];
  if (positional.size() >= 2) outPath = positional[1];

  std::cout << "[dualc_demo] reading " << inPath << "\n";
  auto [mesh, geom] = geometrycentral::surface::readSurfaceMesh(inPath);
  std::cout << "[dualc_demo] input: " << mesh->nVertices() << " verts, "
            << mesh->nFaces() << " faces\n";

  dualc::SamplerParams   sp;
  dualc::ContourerParams cp;
  if (maxDepth  >= 0)   sp.maxDepth = maxDepth;
  if (collapseE >= 0.0) cp.simplificationError = collapseE;
  if (sharp)            sp.interpolateNormals = false;
  if (noManifold)       cp.manifoldDC = false;
  if (pseudoNormal)     sp.signMethod = dualc::SignMethod::PSEUDONORMAL;
  if (gwnSign)          sp.signMethod = dualc::SignMethod::GENERALIZED_WINDING_NUMBER;
  const char* signName =
      sp.signMethod == dualc::SignMethod::PSEUDONORMAL ? "pseudonormal"
      : sp.signMethod == dualc::SignMethod::GENERALIZED_WINDING_NUMBER ? "gwn"
      : "winding";
  std::cout << "[dualc_demo] octree maxDepth = " << sp.maxDepth
            << ", simplificationError = " << cp.simplificationError
            << ", normals = " << (sp.interpolateNormals ? "smooth" : "sharp")
            << ", manifoldDC = " << (cp.manifoldDC ? "on" : "off")
            << ", field = " << (gwnField ? "winding-number" : "mesh")
            << ", sign = " << (gwnField ? "n/a" : signName)
            << "\n";

  std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
             std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>,
             std::vector<dualc::Vector3>>
      result;
  if (gwnField) {
    dualc::WindingNumberField field(*mesh, *geom);
    result = dualc::dualContourField(field, sp, cp);
  } else {
    result = dualc::dualContourMesh(*mesh, *geom, sp, cp);
  }
  auto& [outMesh, outGeom, outVertNormals] = result;
  std::cout << "[dualc_demo] output: " << outMesh->nVertices() << " verts, "
            << outMesh->nFaces() << " faces\n";

  // Project the per-vertex normals onto a CornerData so geometry-central's
  // OBJ writer can emit `vn` lines and matching `f a/_/n` indices. Every
  // corner of a vertex shares the same normal -> smooth Phong shading in
  // any viewer.
  geometrycentral::surface::CornerData<dualc::Vector3> cornerNormals(*outMesh);
  for (auto v : outMesh->vertices()) {
    const std::size_t i = v.getIndex();
    const dualc::Vector3 n = (i < outVertNormals.size())
        ? outVertNormals[i]
        : dualc::Vector3{0.0, 0.0, 1.0};
    for (auto c : v.adjacentCorners()) {
      cornerNormals[c] = n;
    }
  }
  geometrycentral::surface::WavefrontOBJ::write(outPath, *outGeom, cornerNormals);
  std::cout << "[dualc_demo] wrote " << outPath << " (with vn)\n";
  return 0;
}
