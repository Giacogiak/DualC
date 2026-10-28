// dualc_mesh_deviation -- how far a contoured mesh sits from the field that
// produced it, and how creased it is (roadmap 17 #34 / #11).
//
// The measurement every "before / after" record of a Hermite-normal or a bake
// change has used (21/04, 17/20), built as a tool instead of a scratch
// evaluator: for every vertex of an OBJ, |valueAt| of a field graph -- the
// caller passes the EXACT graph, the same expression with no `bake=` -- as
// max and mean; the dihedral angle over every interior edge, binned the way
// the record tabulates it; and, against a reference OBJ of the same vertex
// count, whether the face lists are identical and how far each vertex moved.
//
// Opt-in (-DDUALC_BUILD_BENCH=ON), no CTest: scripts/bench_e12.py runs it on
// the record's cases and prints the record's tables.
#include "example_common.h"
#include "field_graph.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace {

namespace gcs = geometrycentral::surface;
constexpr double kPi = 3.14159265358979323846;

void printUsage() {
  std::cout <<
      "dualc_mesh_deviation -- deviation of a contoured OBJ from its exact field,\n"
      "and its dihedral-angle histogram (roadmap 17 #34, #11).\n\n"
      "Usage:\n"
      "  dualc_mesh_deviation <mesh.obj> (--expr \"<shorthand>\" | --graph FILE) [options]\n\n"
      "Options:\n"
      "  --expr S       The field as a text shorthand (dualc_field's --expr grammar).\n"
      "                 Pass the EXACT graph: the contoured expression without bake=.\n"
      "  --graph FILE   The field from a file: JSON ('{' first) or shorthand.\n"
      "  --old PATH     A reference OBJ with the same vertex count: reports whether\n"
      "                 the face lists are identical and the per-vertex shift.\n"
      "  --threads N    Worker threads for the per-vertex evaluation (0 = all).\n"
      "  --json         One JSON object instead of `key value` lines.\n"
      "  --help, -h     This text.\n\n"
      "Output (one `key value` line each): faces, vertices, dev_max, dev_mean,\n"
      "dihedral_lt1 _1_5 _5_10 _10_20 _20_45 _45_90 _ge90 (interior edges, degrees),\n"
      "dihedral_boundary (edges skipped); with --old: old_faces, old_vertices,\n"
      "faces_identical, shift_max, shift_mean.\n";
}

bool readAll(std::istream& in, std::string& out) {
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return static_cast<bool>(in) || in.eof();
}

// dualc_field's rule: .fld is shorthand, else the first non-whitespace char
// decides ('{' => JSON, else shorthand).
bool looksLikeShorthand(const std::string& path, const std::string& text) {
  if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".fld") == 0) {
    return true;
  }
  for (char c : text) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
    return c != '{';
  }
  return true;
}

struct Loaded {
  std::unique_ptr<gcs::SurfaceMesh> mesh;
  std::unique_ptr<gcs::VertexPositionGeometry> geom;
};

Loaded load(const std::string& path) {
  Loaded l;
  std::tie(l.mesh, l.geom) = gcs::readSurfaceMesh(path);
  return l;
}

// The dihedral bins the record (17/20) tabulates, in degrees.
constexpr int kBins = 7;
const char* const kBinKeys[kBins] = {"dihedral_lt1",   "dihedral_1_5",
                                     "dihedral_5_10",  "dihedral_10_20",
                                     "dihedral_20_45", "dihedral_45_90",
                                     "dihedral_ge90"};
int binOf(double deg) {
  if (deg < 1.0) return 0;
  if (deg < 5.0) return 1;
  if (deg < 10.0) return 2;
  if (deg < 20.0) return 3;
  if (deg < 45.0) return 4;
  if (deg < 90.0) return 5;
  return 6;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" ||
      std::string(argv[1]) == "-h") {
    printUsage();
    return argc < 2 ? 2 : 0;
  }

  std::string meshPath, oldPath, graphPath, exprText;
  bool haveExpr = false, json = false;
  unsigned threads = 0;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_mesh_deviation] " << f << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    } else if (a == "--expr") {
      const char* v = needValue("--expr");
      if (!v) return 2;
      exprText = v;
      haveExpr = true;
    } else if (a == "--graph") {
      const char* v = needValue("--graph");
      if (!v) return 2;
      graphPath = v;
    } else if (a == "--old") {
      const char* v = needValue("--old");
      if (!v) return 2;
      oldPath = v;
    } else if (a == "--threads") {
      const char* v = needValue("--threads");
      if (!v) return 2;
      const long n = std::strtol(v, nullptr, 10);
      if (n < 0 || n > 1024) {
        std::cerr << "[dualc_mesh_deviation] --threads must be 0..1024\n";
        return 2;
      }
      threads = static_cast<unsigned>(n);
    } else if (a == "--json") {
      json = true;
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_mesh_deviation] unknown flag " << a << "\n";
      return 2;
    } else if (meshPath.empty()) {
      meshPath = a;
    } else {
      std::cerr << "[dualc_mesh_deviation] unexpected argument " << a << "\n";
      return 2;
    }
  }
  if (meshPath.empty()) {
    std::cerr << "[dualc_mesh_deviation] a mesh path is required\n";
    return 2;
  }
  if (haveExpr == !graphPath.empty()) {
    std::cerr << "[dualc_mesh_deviation] exactly one of --expr / --graph\n";
    return 2;
  }
  if (threads == 0) {
    threads = std::max(1u, std::thread::hardware_concurrency());
  }

  std::string text;
  bool shorthand = true;
  if (haveExpr) {
    text = exprText;
  } else {
    std::ifstream f(graphPath, std::ios::binary);
    if (!f) {
      std::cerr << "[dualc_mesh_deviation] cannot open '" << graphPath << "'\n";
      return 2;
    }
    readAll(f, text);
    shorthand = looksLikeShorthand(graphPath, text);
  }

  Loaded cur, old;
  try {
    cur = load(meshPath);
    if (!oldPath.empty()) old = load(oldPath);
  } catch (const std::exception& e) {
    std::cerr << "[dualc_mesh_deviation] error: " << e.what() << "\n";
    return 1;
  }

  const std::size_t nV = cur.mesh->nVertices();
  const std::size_t nF = cur.mesh->nFaces();

  // --- deviation: |valueAt| of the exact field at every vertex ------------
  double devMax = 0.0, devMean = 0.0;
  dce::fieldgraph::FileMeshResolver meshes;
  try {
    const dce::fieldgraph::GraphNode root =
        shorthand ? dce::fieldgraph::parseShorthand(text)
                  : dce::fieldgraph::parseJson(text);
    const dce::fieldgraph::FieldGraph graph =
        dce::fieldgraph::FieldGraph::build(root, meshes, threads);
    const dualc::ImplicitField& field = graph.field();

    std::vector<dualc::Vector3> pts(nV);
    for (gcs::Vertex v : cur.mesh->vertices()) {
      const auto p = cur.geom->inputVertexPositions[v];
      pts[v.getIndex()] = dualc::Vector3{p.x, p.y, p.z};
    }
    std::vector<double> dev(nV, 0.0);
    const unsigned nt =
        static_cast<unsigned>(std::min<std::size_t>(threads, std::max<std::size_t>(nV, 1)));
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < nt; ++t) {
      pool.emplace_back([&, t]() {
        const std::size_t lo = nV * t / nt, hi = nV * (t + 1) / nt;
        for (std::size_t i = lo; i < hi; ++i) {
          dev[i] = std::abs(field.valueAt(pts[i]));
        }
      });
    }
    for (std::thread& th : pool) th.join();
    for (double d : dev) {
      devMax = std::max(devMax, d);
      devMean += d;
    }
    if (nV > 0) devMean /= static_cast<double>(nV);
  } catch (const dce::fieldgraph::GraphError& e) {
    std::cerr << "[dualc_mesh_deviation] error: " << e.what();
    if (!e.pointer().empty()) std::cerr << " (at " << e.pointer() << ")";
    std::cerr << "\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "[dualc_mesh_deviation] error: " << e.what() << "\n";
    return 1;
  }

  // --- dihedral histogram over interior edges -------------------------------
  long long bins[kBins] = {0, 0, 0, 0, 0, 0, 0};
  long long boundary = 0;
  cur.geom->requireEdgeDihedralAngles();
  for (gcs::Edge e : cur.mesh->edges()) {
    if (e.isBoundary() || e.degree() != 2) {
      ++boundary;
      continue;
    }
    const double deg = std::abs(cur.geom->edgeDihedralAngles[e]) * 180.0 / kPi;
    ++bins[binOf(deg)];
  }

  // --- against the reference mesh --------------------------------------------
  bool haveOld = old.mesh != nullptr;
  bool facesIdentical = false;
  double shiftMax = 0.0, shiftMean = 0.0;
  std::size_t oldV = 0, oldF = 0;
  if (haveOld) {
    oldV = old.mesh->nVertices();
    oldF = old.mesh->nFaces();
    if (oldV != nV) {
      std::cerr << "[dualc_mesh_deviation] error: --old has " << oldV
                << " vertices, the mesh " << nV
                << "; the shift needs the same vertex set\n";
      return 1;
    }
    facesIdentical = (oldF == nF);
    if (facesIdentical) {
      auto fa = cur.mesh->getFaceVertexList();
      auto fb = old.mesh->getFaceVertexList();
      facesIdentical = (fa == fb);
    }
    for (gcs::Vertex v : cur.mesh->vertices()) {
      const auto a = cur.geom->inputVertexPositions[v];
      const auto b = old.geom->inputVertexPositions[old.mesh->vertex(v.getIndex())];
      const double d = (a - b).norm();
      shiftMax = std::max(shiftMax, d);
      shiftMean += d;
    }
    if (nV > 0) shiftMean /= static_cast<double>(nV);
  }

  // --- report -------------------------------------------------------------------
  if (json) {
    std::printf("{\"faces\": %zu, \"vertices\": %zu, \"dev_max\": %.9g, "
                "\"dev_mean\": %.9g, \"dihedral_boundary\": %lld",
                nF, nV, devMax, devMean, boundary);
    for (int b = 0; b < kBins; ++b) {
      std::printf(", \"%s\": %lld", kBinKeys[b], bins[b]);
    }
    if (haveOld) {
      std::printf(", \"old_faces\": %zu, \"old_vertices\": %zu, "
                  "\"faces_identical\": %s, \"shift_max\": %.9g, "
                  "\"shift_mean\": %.9g",
                  oldF, oldV, facesIdentical ? "true" : "false", shiftMax,
                  shiftMean);
    }
    std::printf("}\n");
  } else {
    std::printf("faces %zu\nvertices %zu\ndev_max %.9g\ndev_mean %.9g\n", nF,
                nV, devMax, devMean);
    for (int b = 0; b < kBins; ++b) {
      std::printf("%s %lld\n", kBinKeys[b], bins[b]);
    }
    std::printf("dihedral_boundary %lld\n", boundary);
    if (haveOld) {
      std::printf("old_faces %zu\nold_vertices %zu\nfaces_identical %s\n"
                  "shift_max %.9g\nshift_mean %.9g\n",
                  oldF, oldV, facesIdentical ? "true" : "false", shiftMax,
                  shiftMean);
    }
  }
  return 0;
}
