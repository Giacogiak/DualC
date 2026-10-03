#include "example_common.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// dualc_lift -- build a 2D profile, lift it to 3D by revolution or extrusion,
// and dual-contour the result.

using dualc::Field2DPtr;
using dualc::Vector2;

namespace {

struct ProfileEntry {
  std::string name;
  std::string signature;
  std::vector<double> defaults;
  bool variadic;  // polygon: any even count of coordinates >= 6
  std::function<Field2DPtr(const std::vector<double>&)> build;
};

std::vector<ProfileEntry> profiles() {
  using namespace dualc;
  return {
      {"circle", "cx cy radius", {0, 0, 0.5}, false,
       [](const std::vector<double>& p) {
         return std::make_shared<Circle2D>(Vector2{p[0], p[1]}, p[2]);
       }},
      {"box", "cx cy halfx halfy", {0, 0, 1, 0.5}, false,
       [](const std::vector<double>& p) {
         return std::make_shared<Box2D>(Vector2{p[0], p[1]},
                                        Vector2{p[2], p[3]});
       }},
      {"segment", "ax ay bx by radius", {-1, 0, 1, 0, 0.3}, false,
       [](const std::vector<double>& p) {
         return std::make_shared<Segment2D>(Vector2{p[0], p[1]},
                                            Vector2{p[2], p[3]}, p[4]);
       }},
      {"polygon", "x1 y1 x2 y2 x3 y3 ...  (>= 3 vertices)",
       {0, 0, 1, 0, 0, 1}, true,
       [](const std::vector<double>& p) {
         std::vector<Vector2> verts;
         for (std::size_t i = 0; i + 1 < p.size(); i += 2)
           verts.push_back(Vector2{p[i], p[i + 1]});
         return std::make_shared<Polygon2D>(std::move(verts));
       }},
  };
}

const ProfileEntry* findProfile(const std::string& name) {
  static const std::vector<ProfileEntry> table = profiles();
  for (const ProfileEntry& e : table) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

void printUsage() {
  std::cerr <<
      "dualc_lift -- revolve or extrude a 2D profile into a 3D mesh.\n"
      "\n"
      "USAGE\n"
      "  dualc_lift revolve <profile> [params...] --offset O [options]\n"
      "  dualc_lift extrude <profile> [params...] --height H [options]\n"
      "\n"
      "PROFILES (params positional; omitted ones use defaults)\n"
      "  circle    cx cy radius\n"
      "  box       cx cy halfx halfy\n"
      "  segment   ax ay bx by radius\n"
      "  polygon   x1 y1 x2 y2 x3 y3 ...   (>= 3 vertices)\n"
      "\n"
      "OPTIONS\n"
      "  --offset O     Revolution: profile x -> (radius - O). Default 1.\n"
      "  --height H     Extrusion: half-height along z. Default 0.5.\n"
      "  -o PATH        Output mesh; format follows the extension: .obj\n"
      "                 (default), .stl (binary), or .3mf (1 unit = 1 mm).\n"
      "                 Default revolved.obj / extruded.obj.\n"
      "  --depth N      Octree max depth (default 7).\n"
      "  --collapse E   Adaptive cell-collapse threshold (default 0).\n"
      "  --help, -h     Show this message.\n"
      "\n"
      "EXAMPLES\n"
      "  dualc_lift revolve circle 0 0 0.5 --offset 2 -o torus.obj\n"
      "  dualc_lift extrude box 0 0 1 0.5 --height 0.75 -o slab.obj\n"
      "  dualc_lift extrude polygon 0,0 1,0 1,1 0,1 --height 0.5\n";
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 3 || std::string(argv[1]) == "--help" ||
      std::string(argv[1]) == "-h") {
    printUsage();
    return argc < 3 ? 2 : 0;
  }

  const std::string mode = argv[1];
  if (mode != "revolve" && mode != "extrude") {
    std::cerr << "[dualc_lift] mode must be 'revolve' or 'extrude'\n";
    printUsage();
    return 2;
  }
  const std::string profileName = argv[2];
  const ProfileEntry* entry = findProfile(profileName);
  if (entry == nullptr) {
    std::cerr << "[dualc_lift] unknown profile: " << profileName << "\n";
    printUsage();
    return 2;
  }

  std::string outPath = (mode == "revolve") ? "revolved.obj" : "extruded.obj";
  double offset = 1.0;
  double height = 0.5;
  dualc::SamplerParams sp;
  dualc::ContourerParams cp;
  std::vector<double> params;

  int i = 3;
  for (; i < argc && dce::isNumberToken(argv[i]); ++i) {
    std::vector<double> group;
    if (!dce::parseDoubles(argv[i], group)) {
      std::cerr << "[dualc_lift] bad parameter: " << argv[i] << "\n";
      return 2;
    }
    params.insert(params.end(), group.begin(), group.end());
  }
  for (; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_lift] " << f << " requires a value\n";
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
    } else if (a == "--offset") {
      const char* v = needValue("--offset");
      if (!v) return 2;
      offset = std::atof(v);
    } else if (a == "--height") {
      const char* v = needValue("--height");
      if (!v) return 2;
      height = std::atof(v);
    } else if (a == "--depth") {
      const char* v = needValue("--depth");
      if (!v) return 2;
      sp.maxDepth = std::atoi(v);
    } else if (a == "--collapse") {
      const char* v = needValue("--collapse");
      if (!v) return 2;
      cp.simplificationError = std::atof(v);
    } else {
      std::cerr << "[dualc_lift] unknown flag: " << a << "\n";
      return 2;
    }
  }

  std::vector<double> p;
  if (entry->variadic) {
    p = params.empty() ? entry->defaults : params;
    if (p.size() < 6 || p.size() % 2 != 0) {
      std::cerr << "[dualc_lift] polygon needs an even number of coordinates "
                   "for >= 3 vertices\n";
      return 2;
    }
  } else {
    if (params.size() > entry->defaults.size()) {
      std::cerr << "[dualc_lift] " << profileName << " takes at most "
                << entry->defaults.size() << " parameters ("
                << entry->signature << ")\n";
      return 2;
    }
    p = entry->defaults;
    for (std::size_t k = 0; k < params.size(); ++k) p[k] = params[k];
  }

  Field2DPtr profile = entry->build(p);
  dualc::FieldPtr field;
  if (mode == "revolve") {
    field = std::make_shared<dualc::RevolveField>(profile, offset);
    std::cout << "[dualc_lift] revolve " << profileName << ", offset " << offset
              << "\n";
  } else {
    field = std::make_shared<dualc::ExtrudeField>(profile, height);
    std::cout << "[dualc_lift] extrude " << profileName << ", height " << height
              << "\n";
  }

  return dce::writeField(*field, outPath, sp, cp);
}
