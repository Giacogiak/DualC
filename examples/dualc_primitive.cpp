#include "example_common.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// dualc_primitive -- build an analytic primitive (optionally decorated and
// warped by a post-op chain) and dual-contour it to an OBJ mesh. The primitive
// catalogue and post-op parsing live in example_common (dce::), shared with the
// interactive viewer (dualc_view).

using dualc::FieldPtr;

namespace {

void printList() {
  std::cout << "PRIMITIVES (parameters are positional; omitted ones use "
               "defaults)\n\n";
  for (const dce::PrimEntry& e : dce::primitiveCatalogue()) {
    std::cout << "  " << e.name << "\n      " << e.signature << "\n";
  }
}

void printUsage() {
  std::cerr <<
      "dualc_primitive -- mesh an analytic primitive, with an optional\n"
      "                   decorator / domain-operator post-op chain.\n"
      "\n"
      "USAGE\n"
      "  dualc_primitive <name> [params...] [post-ops...] [options]\n"
      "\n"
      "  Params are positional doubles (single tokens or comma groups);\n"
      "  omitted trailing params fall back to the primitive's defaults.\n"
      "\n"
      "OPTIONS\n"
      "  -o PATH            Output mesh; format follows the extension: .obj\n"
      "                     (default), .stl (binary), or .3mf (1 unit = 1 mm).\n"
      "                     Default <name>.obj.\n"
      "  --depth N          Octree max depth (default 7).\n"
      "  --collapse E       Adaptive cell-collapse threshold (default 0).\n"
      "  --bounds B         Explicit root box x0,y0,z0,x1,y1,z1. Required for\n"
      "                     infinite primitives and for --repeat.\n"
      "  --list             List every primitive and its parameters.\n"
      "  --help, -h         Show this message.\n"
      "\n"
      "POST-OPS (applied left-to-right, each wraps the running field)\n"
      "  Decorators:\n"
      "    --offset R              grow (+) / shrink (-) the surface\n"
      "    --round R               alias of --offset\n"
      "    --onion T               hollow shell of wall thickness T\n"
      "    --scale S               uniform scale about the origin\n"
      "    --elongate hx,hy,hz     stretch by a slab per axis\n"
      "    --translate dx,dy,dz    rigid translation\n"
      "    --rotate ax,ay,az,deg   rigid rotation about an axis\n"
      "  Domain operators:\n"
      "    --twist k,axis          twist about axis (0=x,1=y,2=z)\n"
      "    --bend k,axis           bend driven by axis\n"
      "    --mirror nx,ny,nz       mirror across a plane through the origin\n"
      "    --repeat px,py,pz       infinite tiling (needs --bounds)\n"
      "    --repeat-limited px,py,pz,nx,ny,nz   finite tiling\n"
      "    --displace name,amp,freq             bump: sine|gyroid|bumps\n"
      "\n"
      "EXAMPLES\n"
      "  dualc_primitive sphere\n"
      "  dualc_primitive torus 0 0 0 2 0.5 --onion 0.1 --twist 1.5,1 -o t.obj\n"
      "  dualc_primitive plane 0 1 0 0 --bounds -2,-2,-2,2,2,2\n"
      "  dualc_primitive triangle --onion 0.05\n";
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" ||
      std::string(argv[1]) == "-h") {
    printUsage();
    return argc < 2 ? 2 : 0;
  }
  if (std::string(argv[1]) == "--list") {
    printList();
    return 0;
  }

  const std::string name = argv[1];
  const dce::PrimEntry* entry = dce::findPrimitive(name);
  if (entry == nullptr) {
    std::cerr << "[dualc_primitive] unknown primitive: " << name
              << "  (try --list)\n";
    return 2;
  }

  std::string outPath = name + ".obj";
  dualc::SamplerParams sp;
  dualc::ContourerParams cp;
  std::vector<double> params;
  std::vector<dce::PostOp> postOps;

  int i = 2;
  // Positional numeric params come first, right after the primitive name.
  for (; i < argc && dce::isNumberToken(argv[i]); ++i) {
    std::vector<double> group;
    if (!dce::parseDoubles(argv[i], group)) {
      std::cerr << "[dualc_primitive] bad parameter: " << argv[i] << "\n";
      return 2;
    }
    params.insert(params.end(), group.begin(), group.end());
  }
  // Everything else is a flag.
  for (; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_primitive] " << f << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    }
    if (a == "-o") {
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
        std::cerr << "[dualc_primitive] bad --bounds (expect "
                     "x0,y0,z0,x1,y1,z1)\n";
        return 2;
      }
      sp.rootBounds = b;
    } else if (dce::isPostOpFlag(a)) {
      const char* v = needValue(a.c_str());
      if (!v) return 2;
      dce::PostOp op;
      std::string err;
      if (!dce::parsePostOp(a, v, op, err)) {
        std::cerr << "[dualc_primitive] " << err << "\n";
        return 2;
      }
      postOps.push_back(std::move(op));
    } else {
      std::cerr << "[dualc_primitive] unknown flag: " << a << "\n";
      printUsage();
      return 2;
    }
  }

  if (params.size() > entry->defaults.size()) {
    std::cerr << "[dualc_primitive] " << name << " takes at most "
              << entry->defaults.size() << " parameters ("
              << entry->signature << ")\n";
    return 2;
  }

  FieldPtr field = dce::buildPrimitive(name, params);
  field = dce::applyPostOps(field, postOps);

  std::cout << "[dualc_primitive] " << name << ", " << postOps.size()
            << " post-op(s), depth " << sp.maxDepth << "\n";
  return dce::writeField(*field, outPath, sp, cp);
}
