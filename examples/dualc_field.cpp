#include "example_common.h"
#include "field_graph.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// dualc_field -- general field-graph CLI: parse a composable field description
// (the canonical JSON form), build the dualc ImplicitField it denotes, dual-
// contour it once, and export. This is what makes a boolean over a TPMS lattice
// ONE field-level contour (no giant intermediate mesh, no mesh round-trip) --
// e.g. difference(intersection(box, normalize(onion(gyroid))), sphere).
//
// The parser/builder lives in field_graph.{h,cpp} (shared with the future
// raymarch side-car and C ABI). The op vocabulary is the example_common
// registry vocabulary; the schema is docs/roadmap/12-field-graph-and-app/01-field-graph.md.

namespace {

void printUsage() {
  std::cerr <<
      "dualc_field -- build and mesh a composable field-graph.\n"
      "\n"
      "USAGE\n"
      "  dualc_field <graph.json> [options]\n"
      "  dualc_field -            [options]   (read the graph from stdin)\n"
      "\n"
      "  The graph is the canonical JSON form: a document\n"
      "  { \"version\":1, \"units\":\"mm\", \"root\": <node> } or a bare node.\n"
      "  A node is { \"op\":<token>, <params...>, \"in\":[<children>] }; op tokens\n"
      "  are the same registry tokens the other CLIs use (gyroid, union,\n"
      "  difference, onion, normalize, mesh, winding, sphere, box, ...).\n"
      "\n"
      "OPTIONS\n"
      "  -o PATH        Output mesh; format follows the extension: .obj\n"
      "                 (default), .stl (binary), or .3mf (1 unit = 1 mm).\n"
      "                 Default field.obj.\n"
      "  --depth N      Octree max depth (default 7).\n"
      "  --collapse E   Adaptive cell-collapse threshold (default 0).\n"
      "  --bounds B     Explicit root box x0,y0,z0,x1,y1,z1. Required when the\n"
      "                 graph is unbounded (a bare plane / infinite source /\n"
      "                 infinite repeat); otherwise auto-fit to the field.\n"
      "  --tile-depth D Stream the output in bounded-memory tiles instead of\n"
      "                 building the whole mesh at once -- for dense parts that\n"
      "                 would OOM. Each tile is a 2^D-cell octree (D < --depth);\n"
      "                 peak RAM is one tile. STL or 3MF output (.3mf is ~3x\n"
      "                 smaller on disk, deflated + within-tile shared vertices,\n"
      "                 slicer-native 1 unit = 1 mm). Tile count is derived. Seam\n"
      "                 vertices are duplicated across tiles (geometrically\n"
      "                 watertight for slicers, not topologically welded).\n"
      "  --weld         With --tile-depth and .3mf output, weld across-tile seam\n"
      "                 vertices into a single globally-manifold, shared-vertex\n"
      "                 object (for re-booleans / FEA / decimation). Bounded RAM\n"
      "                 (only seam-plane vertices are hashed). Default off.\n"
      "  --mem BUDGET   Auto-pick --tile-depth from a RAM budget (e.g. 4G, 512M)\n"
      "                 instead of choosing D by hand: a cheap coarse probe picks\n"
      "                 the largest D whose estimated peak per-tile RAM fits. .stl\n"
      "                 or .3mf output. Conservative (errs to a smaller D); an\n"
      "                 explicit --tile-depth overrides it.\n"
      "  --decimate R   Post-contour QEM decimation: keep fraction R in (0,1)\n"
      "                 of the triangles (e.g. 0.1 = ~10x lighter). Global,\n"
      "                 quality-optimal; watertight in/out. Monolithic only --\n"
      "                 cannot combine with --tile-depth.\n"
      "  --simplify E   Like --decimate but targets an absolute geometric error\n"
      "                 of E world units (mm) instead of a ratio. Mutually\n"
      "                 exclusive with --decimate; monolithic only.\n"
      "  --no-manifold  Disable Manifold Dual Contouring (one vertex per cell).\n"
      "  --expr S       Build from a terse text shorthand string instead of a\n"
      "                 file/stdin: op(child,...,key=value). Underscore aliases\n"
      "                 map the hyphenated tokens (schwarz_p, smooth_union, ...).\n"
      "  --dump-json    Print the canonical JSON for the parsed graph and exit\n"
      "                 (does not build or contour) -- a round-trip / GH aid.\n"
      "  --list         Print the op vocabulary and exit.\n"
      "  --help, -h     Show this message.\n"
      "\n"
      "Input is JSON or shorthand: a .fld file is shorthand; otherwise the first\n"
      "non-whitespace char decides ('{' => JSON, else shorthand).\n"
      "\n"
      "EXAMPLE (JSON: sphere carved out of a box-bounded gyroid lattice)\n"
      "  {\"version\":1,\"units\":\"mm\",\"root\":\n"
      "    {\"op\":\"difference\",\"in\":[\n"
      "      {\"op\":\"intersection\",\"in\":[\n"
      "        {\"op\":\"box\",\"min\":[-40,-40,-40],\"max\":[40,40,40]},\n"
      "        {\"op\":\"onion\",\"thickness\":2,\"in\":[\n"
      "          {\"op\":\"normalize\",\"in\":[\n"
      "            {\"op\":\"gyroid\",\"wavelength\":5}]}]}]},\n"
      "      {\"op\":\"sphere\",\"center\":[0,0,0],\"radius\":20}]}}\n"
      "\n"
      "EXAMPLE (shorthand, equivalent shape)\n"
      "  dualc_field --expr \"difference(intersection(box(min=[-2,-2,-2],\"\n"
      "    \"max=[2,2,2]),onion(normalize(gyroid(wavelength=1)),thickness=0.3)),\"\n"
      "    \"sphere(radius=1))\" -o part.obj\n";
}

// Print the op vocabulary, grouped, to stdout (the --list helper). Sourced from
// the example_common registries so it never drifts from what the builder accepts.
void printVocabulary() {
  std::cout << "dualc_field op vocabulary (registry tokens):\n\n";

  std::cout << "TPMS sources (params: center=[x,y,z], wavelength):\n";
  for (const dce::TpmsKind& k : dce::tpmsKinds())
    std::cout << "  " << k.name << "  -- " << k.blurb << "\n";

  std::cout << "\nStrut-lattice sources (params: center=[x,y,z], wavelength, "
               "radius, nodeRadius=radius for tapered struts):\n";
  for (const dce::StrutKind& k : dce::strutKinds())
    std::cout << "  " << k.name << "  -- " << k.blurb << "\n";

  std::cout << "\nMesh sources:\n"
               "  mesh     -- signed distance to a watertight mesh "
               "(path, sign, normals)\n"
               "  winding  -- generalized-winding-number field for soup / open "
               "shells (path)\n";

  std::cout << "\nBooleans (2 children; smooth-* take k=0.25):\n";
  struct BoolHint { dce::BoolOp op; const char* hint; };
  const BoolHint bops[] = {
      {dce::BoolOp::Union, "A or B"},
      {dce::BoolOp::Intersection, "A and B"},
      {dce::BoolOp::Difference, "A minus B"},
      {dce::BoolOp::Xor, "symmetric difference"},
      {dce::BoolOp::SmoothUnion, "blended union (metric inputs)"},
      {dce::BoolOp::SmoothIntersection, "blended intersection (metric inputs)"},
      {dce::BoolOp::SmoothDifference, "blended difference (metric inputs)"}};
  for (const BoolHint& b : bops)
    std::cout << "  " << dce::boolOpName(b.op) << "  -- " << b.hint << "\n";

  std::cout << "\nDecorators / domain ops (1 child):\n"
               "  normalize       -- rescale a non-metric field toward unit "
               "gradient\n"
               "  transform       -- raw 4x4 matrix (matrix=[16 floats])\n"
               "  graded-onion    -- shell with control-driven thickness; 2 "
               "children\n"
               "                     (base, control), params t1 t2 d0 d1 "
               "(control e.g.\n"
               "                     sphere(radius=0)=dist-from-point, "
               "plane=linear, mesh=surf)\n"
               "  graded-offset   -- inflate a solid by control-driven amount; "
               "same\n"
               "                     2 children + params as graded-onion "
               "(base - t)\n"
               "  mix             -- smooth spatial morph A->B; 3 children "
               "(A, B,\n"
               "                     control), params lo hi -- non-metric; "
               "e.g. crystal\n"
               "                     transitions (unaligned struts stub at the "
               "seam)\n";
  // The post-op tokens are the parsePostOp/tryMakePostOp set minus the leading
  // "--"; this is a hand-maintained mirror of that registry (kept in sync with
  // isPostOpFlag in example_common.cpp).
  struct OpHint { const char* op; const char* hint; };
  const OpHint postOps[] = {
      {"offset", "grow/shrink (r)"},
      {"round", "round corners (r)"},
      {"onion", "hollow shell (thickness)"},
      {"scale", "uniform scale (s)"},
      {"elongate", "stretch (h=[x,y,z])"},
      {"translate", "move (by=[x,y,z])"},
      {"rotate", "rotate (axis=[x,y,z], degrees)"},
      {"twist", "twist about an axis (radiansPerUnit, axis)"},
      {"bend", "bend about an axis (curvature, axis)"},
      {"mirror", "mirror across a plane (normal=[x,y,z])"},
      {"repeat", "infinite tiling (period=[x,y,z]) -- needs --bounds"},
      {"repeat-limited", "finite tiling (period, count)"},
      {"displace", "surface bump (fn, amplitude, frequency)"}};
  for (const OpHint& o : postOps)
    std::cout << "  " << o.op << "  -- " << o.hint << "\n";

  std::cout << "\nAnalytic primitives (grouped keys or flat params=[...]):\n";
  for (const dce::PrimEntry& e : dce::primitiveCatalogue())
    std::cout << "  " << e.name << "  (" << e.signature << ")\n";
}

// Choose JSON vs shorthand for file/stdin input: a .fld path is shorthand;
// otherwise sniff the first non-whitespace char ('{' => JSON, else shorthand).
bool looksLikeShorthand(const std::string& path, const std::string& text) {
  if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".fld") == 0)
    return true;
  for (char c : text) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
    return c != '{';
  }
  return false;  // empty input: let the JSON parser report it
}

bool readAll(std::istream& in, std::string& out) {
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return static_cast<bool>(in) || in.eof();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" ||
      std::string(argv[1]) == "-h") {
    printUsage();
    return argc < 2 ? 2 : 0;
  }

  std::string inPath;
  std::string outPath;
  std::string exprText;
  bool haveExpr = false;
  bool dumpJson = false;
  int tileDepth = 0;  // 0 = monolithic (no tiling)
  bool weld = false;  // --weld: single globally-manifold 3MF object
  std::uint64_t memBudget = 0;  // --mem: RAM budget (bytes) -> auto tile-depth
  bool haveMem = false;
  dce::DecimateOpts dec;  // --decimate / --simplify: post-contour QEM pass
  dualc::SamplerParams sp;
  dualc::ContourerParams cp;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_field] " << f << " requires a value\n";
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
      dualc::BBox b;
      if (!dce::parseBounds(v, b)) {
        std::cerr << "[dualc_field] --bounds expects six comma-separated "
                     "doubles\n";
        return 2;
      }
      sp.rootBounds = b;
    } else if (a == "--tile-depth") {
      const char* v = needValue("--tile-depth");
      if (!v) return 2;
      tileDepth = std::atoi(v);
    } else if (a == "--weld") {
      weld = true;
    } else if (a == "--mem") {
      const char* v = needValue("--mem");
      if (!v) return 2;
      if (!dce::parseMemBudget(v, memBudget)) {
        std::cerr << "[dualc_field] --mem expects a positive size like 4G, "
                     "512M, or a byte count\n";
        return 2;
      }
      haveMem = true;
    } else if (a == "--decimate") {
      const char* v = needValue("--decimate");
      if (!v) return 2;
      if (dec.mode != dce::DecimateOpts::Mode::None) {
        std::cerr << "[dualc_field] --decimate and --simplify are mutually "
                     "exclusive\n";
        return 2;
      }
      const double r = std::atof(v);
      if (!(r > 0.0 && r < 1.0)) {
        std::cerr << "[dualc_field] --decimate expects a keep-fraction in "
                     "(0,1) (e.g. 0.1 for 10x lighter)\n";
        return 2;
      }
      dec = {dce::DecimateOpts::Mode::Ratio, r};
    } else if (a == "--simplify") {
      const char* v = needValue("--simplify");
      if (!v) return 2;
      if (dec.mode != dce::DecimateOpts::Mode::None) {
        std::cerr << "[dualc_field] --decimate and --simplify are mutually "
                     "exclusive\n";
        return 2;
      }
      const double e = std::atof(v);
      if (!(e > 0.0)) {
        std::cerr << "[dualc_field] --simplify expects a positive error in "
                     "world units (mm)\n";
        return 2;
      }
      dec = {dce::DecimateOpts::Mode::Error, e};
    } else if (a == "--no-manifold") {
      cp.manifoldDC = false;
    } else if (a == "--expr") {
      const char* v = needValue("--expr");
      if (!v) return 2;
      exprText = v;
      haveExpr = true;
    } else if (a == "--dump-json") {
      dumpJson = true;
    } else if (a == "--list") {
      printVocabulary();
      return 0;
    } else if (a == "-") {
      inPath = "-";
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_field] unknown flag: " << a << "\n";
      return 2;
    } else if (inPath.empty()) {
      inPath = a;
    } else {
      std::cerr << "[dualc_field] unexpected positional arg: " << a << "\n";
      return 2;
    }
  }

  if (haveExpr && !inPath.empty()) {
    std::cerr << "[dualc_field] give either --expr or an input graph, not both\n";
    return 2;
  }
  if (!haveExpr && inPath.empty()) {
    std::cerr << "[dualc_field] no input graph given\n";
    printUsage();
    return 2;
  }

  // Obtain the graph text (from --expr, stdin, or a file) and decide JSON vs
  // shorthand. --expr is always shorthand; a file/stdin chooses by .fld
  // extension else first-non-whitespace-char content sniff.
  std::string text;
  bool shorthand = false;
  if (haveExpr) {
    text = exprText;
    shorthand = true;
  } else if (inPath == "-") {
    if (!readAll(std::cin, text)) {
      std::cerr << "[dualc_field] failed reading graph from stdin\n";
      return 2;
    }
    shorthand = looksLikeShorthand("", text);
  } else {
    std::ifstream f(inPath, std::ios::binary);
    if (!f) {
      std::cerr << "[dualc_field] cannot open '" << inPath << "'\n";
      return 2;
    }
    readAll(f, text);
    shorthand = looksLikeShorthand(inPath, text);
  }

  if (outPath.empty()) outPath = "field.obj";

  // Parse -> (dump | build -> contour -> export). The mesh resolver owns every
  // loaded mesh and must outlive the field, so keep it in scope through
  // writeField.
  dce::fieldgraph::FileMeshResolver meshes;
  try {
    dce::fieldgraph::GraphNode root =
        shorthand ? dce::fieldgraph::parseShorthand(text)
                  : dce::fieldgraph::parseJson(text);

    // --dump-json canonicalises and exits before any build/contour.
    if (dumpJson) {
      std::cout << dce::fieldgraph::dumpJson(root) << "\n";
      return 0;
    }

    dce::fieldgraph::FieldGraph graph =
        dce::fieldgraph::FieldGraph::build(root, meshes);
    std::cout << "[dualc_field] depth = " << sp.maxDepth
              << ", manifold = " << (cp.manifoldDC ? "on" : "off") << "\n";

    // --mem auto-budget: pick the tile-depth from a RAM budget, then fall through
    // to the same tiled-export path an explicit --tile-depth uses. An explicit
    // --tile-depth wins. Fail fast (before the probe) on the same combinations
    // the tiling block rejects.
    if (haveMem) {
      if (tileDepth > 0) {
        std::cerr << "[dualc_field] note: --mem ignored -- explicit --tile-depth "
                  << tileDepth << " takes precedence\n";
      } else {
        std::string ext = outPath.size() >= 4
                              ? outPath.substr(outPath.size() - 4)
                              : std::string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char ch) { return std::tolower(ch); });
        if (dec.mode != dce::DecimateOpts::Mode::None) {
          std::cerr << "[dualc_field] --mem (streaming) cannot be combined with "
                       "--decimate/--simplify\n";
          return 2;
        }
        if (ext != ".stl" && ext != ".3mf") {
          std::cerr << "[dualc_field] --mem requires .stl or .3mf output (got '"
                    << outPath << "')\n";
          return 2;
        }
        if (cp.simplificationError > 0.0) {
          std::cerr << "[dualc_field] --mem (streaming) cannot be combined with "
                       "--collapse (per-tile collapse would crack seams)\n";
          return 2;
        }
        const int d =
            dce::chooseTileDepthForBudget(graph.field(), sp, cp, memBudget);
        if (d < 0) return 2;  // unbounded field (hint already printed)
        tileDepth = d;
      }
    }

    if (tileDepth > 0) {
      // Streaming / tiled export: STL or 3MF. Post-contour decimation is
      // Approach A (monolithic) only -- a per-tile QEM pass needs locked seam
      // vertices to stay crack-free (Approach B), which is not implemented.
      if (dec.mode != dce::DecimateOpts::Mode::None) {
        std::cerr << "[dualc_field] --decimate/--simplify cannot be combined "
                     "with --tile-depth (per-tile locked-border decimation is "
                     "not yet implemented)\n";
        return 2;
      }
      // Streaming / tiled export: STL or 3MF, and incompatible with per-cell
      // collapse (per-tile simplification would crack the seams).
      std::string ext = outPath.size() >= 4 ? outPath.substr(outPath.size() - 4)
                                            : std::string();
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char ch) { return std::tolower(ch); });
      if (ext != ".stl" && ext != ".3mf") {
        std::cerr << "[dualc_field] --tile-depth requires .stl or .3mf output "
                     "(got '" << outPath << "')\n";
        return 2;
      }
      if (cp.simplificationError > 0.0) {
        std::cerr << "[dualc_field] --tile-depth cannot be combined with "
                     "--collapse (per-tile collapse would crack seams)\n";
        return 2;
      }
      if (weld && ext != ".3mf") {
        std::cerr << "[dualc_field] --weld requires .3mf output (got '" << outPath
                  << "')\n";
        return 2;
      }
      return ext == ".3mf"
                 ? dce::writeFieldTiled3mf(graph.field(), outPath, sp, cp,
                                           tileDepth, weld)
                 : dce::writeFieldTiledStl(graph.field(), outPath, sp, cp, tileDepth);
    }
    return dce::writeField(graph.field(), outPath, sp, cp, dec);
  } catch (const dce::fieldgraph::GraphError& e) {
    std::cerr << "[dualc_field] field-graph error";
    if (!e.pointer().empty()) std::cerr << " at " << e.pointer();
    std::cerr << ": " << e.what() << "\n";
    return 2;
  }
}
