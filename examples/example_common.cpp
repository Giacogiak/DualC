#include "example_common.h"

#include "geometrycentral/surface/halfedge_element_types.h"
#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include "miniz.h"          // vendored (examples/third_party): 3MF ZIP container
#include "meshoptimizer.h"  // vendored (examples/third_party): QEM decimation

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>

#ifdef _WIN32
#include <share.h>   // _SH_DENYNO for _fsopen
#endif

namespace dce {

namespace {

// --- The writers' error line and the output handles (roadmap 17 #52) -------
//
// Every `[dualc] error:` the writer path prints is also kept, per thread, so a
// caller with no console (the C ABI, a host plugin) can read it back through
// dce::lastError(); writeField* clear it on entry. Per thread because several
// exports may run at once in one process.
thread_local std::string g_lastError;

void reportError(const std::string& text) {
  g_lastError = text;
  std::cerr << "[dualc] error: " << text << "\n";
}

std::string errnoText() {
  return std::generic_category().message(errno);
}

// Output files are opened NON-INHERITABLE. The MSVC CRT opens files inheritable
// by default, so a child process the host starts with handle inheritance (what
// .NET's Process.Start does whenever a standard stream is redirected) while a
// `.part` is open holds a duplicate of that handle -- same share mode, no
// FILE_SHARE_DELETE -- for as long as it lives, and the rename over `<path>`
// fails with ERROR_SHARING_VIOLATION. Measured on windows-2022: every tiled
// export open while such a child started failed at the rename; no bounded
// retry would have helped (the hold lasts the child's lifetime). The flag `N`
// (`_O_NOINHERIT`) closes that door; on POSIX `e` (O_CLOEXEC) is the same
// hygiene, though a rename there is never blocked by an open handle.
FILE* openOutputFile(const std::string& path, bool binary) {
#ifdef _WIN32
  return _fsopen(path.c_str(), binary ? "wbN" : "wN", _SH_DENYNO);
#else
  return std::fopen(path.c_str(), binary ? "wbe" : "we");
#endif
}

// std::ofstream has no portable way to set the inherit flag, so on Windows the
// stream is built over the FILE* from openOutputFile through the MSVC
// filebuf(FILE*) extension -- which takes NO ownership ("extension, no
// ownership taking" in <fstream>): the stream's close() flushes but never
// fcloses, so an OutputFile owns the FILE and closes it after the stream.
// Declare the owner BEFORE its stream (destroyed after it). Measured the hard
// way: without the owner the process held its own `.part` and the rename
// failed with the very sharing violation this code exists to prevent.
class OutputFile {
 public:
  OutputFile() = default;
  OutputFile(const OutputFile&) = delete;
  OutputFile& operator=(const OutputFile&) = delete;
  ~OutputFile() {
    if (f_) std::fclose(f_);
  }
  // Open `path` for writing, non-inheritable; false with errno set.
  bool open(std::ofstream& os, const std::string& path, bool binary = true) {
#ifdef _WIN32
    f_ = openOutputFile(path, binary);
    if (!f_) return false;
    os = std::ofstream(f_);
    return static_cast<bool>(os);
#else
    os.open(path, binary ? (std::ios::out | std::ios::binary) : std::ios::out);
    return static_cast<bool>(os);
#endif
  }
  // Flush and close the stream, then the FILE; false if either failed.
  // Safe to call more than once.
  bool close(std::ofstream& os) {
    bool ok = true;
    if (os.is_open()) {
      os.flush();
      ok = static_cast<bool>(os);
      os.close();
      ok = ok && static_cast<bool>(os);
    }
    if (f_) {
      if (std::fclose(f_) != 0) ok = false;
      f_ = nullptr;
    }
    return ok;
  }

 private:
  FILE* f_ = nullptr;
};

// A failed rename or remove that another handle caused is transient when that
// handle is short-lived (an on-access scanner on a user's machine); permanent
// when it is not (an inherited handle). Retry a bounded time, then give up.
// Error 32 (sharing violation) and 5 (access denied) both map to
// permission_denied under MSVC, measured; on POSIX EACCES/EPERM never clear
// by themselves, so there is no loop.
bool transientFileError(const std::error_code& ec) {
#ifdef _WIN32
  return ec == std::errc::permission_denied || ec.value() == 32 || ec.value() == 5 ||
         ec.value() == 33;
#else
  (void)ec;
  return false;
#endif
}

// Up to `budgetMs` of retries with a short back-off: 1, 2, 4, ... capped at
// 100 ms. Returns the ms spent; `attempts` counts the failed ones.
template <class Op>
bool retryFileOp(Op op, int budgetMs, std::error_code& ec, int& attempts, long long& spentMs) {
  const auto t0 = std::chrono::steady_clock::now();
  int delay = 1;
  attempts = 0;
  for (;;) {
    ec.clear();
    if (op(ec)) { spentMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count(); return true; }
    ++attempts;
    spentMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    if (!transientFileError(ec) || spentMs >= budgetMs) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    delay = std::min(delay * 2, 100);
  }
}

std::vector<std::string> splitComma(const std::string& s) {
  std::vector<std::string> out;
  std::string item;
  std::istringstream ss(s);
  while (std::getline(ss, item, ',')) out.push_back(item);
  return out;
}

} // namespace

bool parseDoubles(const std::string& s, std::vector<double>& out) {
  out.clear();
  if (s.empty()) return false;
  for (const std::string& tok : splitComma(s)) {
    if (tok.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(tok.c_str(), &end);
    if (end == tok.c_str() || *end != '\0') return false;
    out.push_back(v);
  }
  return !out.empty();
}

bool parseVec3(const std::string& s, dualc::Vector3& out) {
  std::vector<double> v;
  if (!parseDoubles(s, v) || v.size() != 3) return false;
  out = dualc::Vector3{v[0], v[1], v[2]};
  return true;
}

bool parseVec2(const std::string& s, dualc::Vector2& out) {
  std::vector<double> v;
  if (!parseDoubles(s, v) || v.size() != 2) return false;
  out = dualc::Vector2{v[0], v[1]};
  return true;
}

bool parseBounds(const std::string& s, dualc::BBox& out) {
  std::vector<double> v;
  if (!parseDoubles(s, v) || v.size() != 6) return false;
  out.min = dualc::Vector3{v[0], v[1], v[2]};
  out.max = dualc::Vector3{v[3], v[4], v[5]};
  return true;
}

bool isNumberToken(const std::string& token) {
  if (token.empty()) return false;
  std::size_t i = 0;
  if (token[i] == '-' || token[i] == '+') ++i;
  if (i >= token.size()) return false;
  return std::isdigit(static_cast<unsigned char>(token[i])) || token[i] == '.';
}

std::function<double(const dualc::Vector3&)> namedBump(const std::string& spec) {
  const std::vector<std::string> parts = splitComma(spec);
  const std::string name = parts.empty() ? "" : parts[0];
  const double amp  = parts.size() > 1 ? std::atof(parts[1].c_str()) : 0.1;
  const double freq = parts.size() > 2 ? std::atof(parts[2].c_str()) : 4.0;

  // Each raw bump function has a different intrinsic range -- sin*sin*sin
  // peaks at 1, the gyroid sin/cos sum at ~1.4973, the cos+cos+cos sum at 3.
  // We divide by that peak so `amp` is the true peak surface displacement
  // for every function: --displace bumps,0.15 and --displace sine,0.15 then
  // deform the surface by the same +/-0.15, instead of bumps secretly
  // applying 3x as much (which pushes the displaced field into genuine
  // self-intersection that no mesher can close).
  if (name == "sine") {
    return [amp, freq](const dualc::Vector3& p) {
      return amp * std::sin(freq * p.x) * std::sin(freq * p.y) *
             std::sin(freq * p.z);
    };
  }
  if (name == "gyroid") {
    const double scale = amp / 1.4973;
    return [scale, freq](const dualc::Vector3& p) {
      return scale * (std::sin(freq * p.x) * std::cos(freq * p.y) +
                      std::sin(freq * p.y) * std::cos(freq * p.z) +
                      std::sin(freq * p.z) * std::cos(freq * p.x));
    };
  }
  if (name == "bumps") {
    const double scale = amp / 3.0;
    return [scale, freq](const dualc::Vector3& p) {
      return scale * (std::cos(freq * p.x) + std::cos(freq * p.y) +
                      std::cos(freq * p.z));
    };
  }
  std::cerr << "[dualc] warning: unknown bump '" << name
            << "' (use sine|gyroid|bumps); using a zero bump\n";
  return [](const dualc::Vector3&) { return 0.0; };
}

const std::vector<TpmsKind>& tpmsKinds() {
  static const std::vector<TpmsKind> k = {
      {"gyroid",       "Schoen G (default)"},
      {"schwarz-p",    "Schwarz P (primitive cubic)"},
      {"diamond",      "Schwarz D (Diamond)"},
      {"fischer-koch", "Fischer-Koch S"},
      {"lidinoid",     "Lidinoid (Lidin's surface)"},
      {"neovius",      "Neovius (Im-3m symmetry)"},
  };
  return k;
}

dualc::FieldPtr makeTpmsField(const std::string& kind,
                              const dualc::Vector3& center,
                              double wavelength) {
  if (kind == "gyroid")
    return std::make_shared<dualc::GyroidField>(center, wavelength);
  if (kind == "schwarz-p")
    return std::make_shared<dualc::SchwarzPField>(center, wavelength);
  if (kind == "diamond")
    return std::make_shared<dualc::DiamondField>(center, wavelength);
  if (kind == "fischer-koch")
    return std::make_shared<dualc::FischerKochSField>(center, wavelength);
  if (kind == "lidinoid")
    return std::make_shared<dualc::LidinoidField>(center, wavelength);
  if (kind == "neovius")
    return std::make_shared<dualc::NeoviusField>(center, wavelength);
  return nullptr;
}

// ---------------------------------------------------------------------------
// Strut-based (wireframe crystal) lattices.
// ---------------------------------------------------------------------------

const std::vector<StrutKind>& strutKinds() {
  static const std::vector<StrutKind> k = {
      {"sc",    "simple cubic (axis struts)"},
      {"bcc",   "body-centered cubic (center-to-corner)"},
      {"fcc",   "face-centered cubic (face diagonals)"},
      {"octet", "octet truss (FCC + octahedral edges)"},
  };
  return k;
}

std::vector<std::pair<dualc::Vector3, dualc::Vector3>> strutCellSegments(
    const std::string& kind, double wavelength) {
  using dualc::Vector3;
  using Seg = std::pair<Vector3, Vector3>;
  const double h = 0.5 * wavelength;  // half-cell: node at origin, corners +-h
  std::vector<Seg> segs;

  // Cell nodes (origin-centered): O = tile center, the 8 corners, the 6 face
  // centers. Struts connect them per crystal. Segments that lie on a cell face
  // (fcc/octet) are naturally emitted on both opposite faces by enumerating all
  // corners, so infinite round-fold (`repeated`) sees them from either side.
  const Vector3 corners[8] = {
      {-h, -h, -h}, {h, -h, -h}, {-h, h, -h}, {h, h, -h},
      {-h, -h, h},  {h, -h, h},  {-h, h, h},  {h, h, h}};
  const Vector3 faces[6] = {{h, 0, 0}, {-h, 0, 0}, {0, h, 0},
                            {0, -h, 0}, {0, 0, h}, {0, 0, -h}};

  if (kind == "sc") {
    // Three axis rods through the tile center; under tiling they join into the
    // orthogonal cubic grid.
    segs.push_back({{-h, 0, 0}, {h, 0, 0}});
    segs.push_back({{0, -h, 0}, {0, h, 0}});
    segs.push_back({{0, 0, -h}, {0, 0, h}});
  } else if (kind == "bcc") {
    // Body diagonals: tile center to each of the 8 corners (corners are shared
    // with the 8 neighbour tiles, giving coordination 8).
    for (const Vector3& c : corners) segs.push_back({{0, 0, 0}, c});
  } else if (kind == "fcc" || kind == "octet") {
    // Face "X"s: each face center bonds to the 4 corners of its face -> the
    // 12 face-diagonal half-struts (the FCC nearest-neighbour graph).
    for (const Vector3& fc : faces)
      for (const Vector3& c : corners) {
        // A corner belongs to a face iff it shares that face's non-zero coord.
        const bool onFace = (fc.x != 0.0 && c.x == fc.x) ||
                            (fc.y != 0.0 && c.y == fc.y) ||
                            (fc.z != 0.0 && c.z == fc.z);
        if (onFace) segs.push_back({fc, c});
      }
    if (kind == "octet") {
      // Octahedral edges: connect face centers on adjacent (non-opposite)
      // faces -> the 12 edges of the central octahedron, adding the interior
      // tetra/octa truss. Emit each unordered pair once.
      for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = i + 1; j < 6; ++j) {
          const Vector3& a = faces[i];
          const Vector3& b = faces[j];
          // Opposite faces (a == -b) are collinear through the center: skip.
          const bool opposite = (a.x == -b.x && a.y == -b.y && a.z == -b.z);
          if (!opposite) segs.push_back({a, b});
        }
    }
  }
  return segs;
}

dualc::FieldPtr makeStrutLattice(const std::string& kind,
                                 const dualc::Vector3& center,
                                 double wavelength, double radius,
                                 double nodeRadius) {
  const auto segs = strutCellSegments(kind, wavelength);
  if (segs.empty()) return nullptr;  // unknown crystal
  if (nodeRadius < 0.0) nodeRadius = radius;  // sentinel: no taper
  const bool tapered = (nodeRadius != radius);

  // Union the unit-cell struts. Untapered: one exact capsule per segment.
  // Tapered: a strut has a node at BOTH ends, so split each segment at its
  // midpoint into two round cones -- RoundCone(a,mid,nodeRadius,radius) U
  // RoundCone(mid,b,radius,nodeRadius) -- both reaching `radius` at mid, giving
  // the symmetric fat-ends / thin-middle profile. Round cones are exact SDFs, so
  // the union stays Lipschitz-1 and the single round-fold tiling stays exact.
  dualc::FieldPtr cell;
  auto add = [&cell](dualc::FieldPtr f) {
    cell = cell ? dualc::unionOf(cell, f) : f;
  };
  for (const auto& s : segs) {
    if (!tapered) {
      add(std::make_shared<dualc::CapsuleField>(s.first, s.second, radius));
    } else {
      const dualc::Vector3 mid = (s.first + s.second) * 0.5;
      add(std::make_shared<dualc::RoundConeField>(s.first, mid, nodeRadius,
                                                  radius));
      add(std::make_shared<dualc::RoundConeField>(mid, s.second, radius,
                                                  nodeRadius));
    }
  }

  // Tile it infinitely, then translate so a cell sits at `center`.
  dualc::FieldPtr tiled = dualc::repeated(
      cell, dualc::Vector3{wavelength, wavelength, wavelength});
  if (center.x != 0.0 || center.y != 0.0 || center.z != 0.0)
    tiled = dualc::transformed(tiled, dualc::Mat4::translation(center));
  return tiled;
}

// ===========================================================================
// Shared builder registries.
// ===========================================================================

namespace {
dualc::Vector3 v3at(const std::vector<double>& p, int i) {
  return dualc::Vector3{p[i], p[i + 1], p[i + 2]};
}
}  // namespace

const std::vector<PrimEntry>& primitiveCatalogue() {
  static const std::vector<PrimEntry> cat = [] {
    using namespace dualc;
    std::vector<PrimEntry> c = {
        // ---- Tier A ----
        {"sphere", "cx cy cz radius", {0, 0, 0, 1},
         [](const std::vector<double>& p) {
           return std::make_shared<SphereField>(v3at(p, 0), p[3]);
         }},
        {"box", "minx miny minz maxx maxy maxz", {-1, -1, -1, 1, 1, 1},
         [](const std::vector<double>& p) {
           return std::make_shared<BoxField>(v3at(p, 0), v3at(p, 3));
         }},
        {"roundbox", "minx miny minz maxx maxy maxz radius",
         {-1, -1, -1, 1, 1, 1, 0.3},
         [](const std::vector<double>& p) {
           return std::make_shared<RoundBoxField>(v3at(p, 0), v3at(p, 3), p[6]);
         }},
        {"plane", "nx ny nz offset  [infinite -- needs --bounds]", {0, 1, 0, 0},
         [](const std::vector<double>& p) {
           return std::make_shared<PlaneField>(v3at(p, 0), p[3]);
         }},
        {"capsule", "ax ay az bx by bz radius", {-1, 0, 0, 1, 0, 0, 0.5},
         [](const std::vector<double>& p) {
           return std::make_shared<CapsuleField>(v3at(p, 0), v3at(p, 3), p[6]);
         }},
        {"cappedcylinder", "ax ay az bx by bz radius", {0, -1, 0, 0, 1, 0, 0.5},
         [](const std::vector<double>& p) {
           return std::make_shared<CappedCylinderField>(v3at(p, 0), v3at(p, 3),
                                                        p[6]);
         }},
        {"torus", "cx cy cz major minor", {0, 0, 0, 1, 0.3},
         [](const std::vector<double>& p) {
           return std::make_shared<TorusField>(v3at(p, 0), p[3], p[4]);
         }},
        {"ellipsoid", "cx cy cz rx ry rz", {0, 0, 0, 1, 0.6, 0.4},
         [](const std::vector<double>& p) {
           return std::make_shared<EllipsoidField>(v3at(p, 0), v3at(p, 3));
         }},
        // ---- Tier B ----
        {"boxframe", "minx miny minz maxx maxy maxz edge",
         {-1, -1, -1, 1, 1, 1, 0.1},
         [](const std::vector<double>& p) {
           return std::make_shared<BoxFrameField>(v3at(p, 0), v3at(p, 3), p[6]);
         }},
        {"cone", "cx cy cz angleRad height", {0, 1, 0, 0.5, 2},
         [](const std::vector<double>& p) {
           return std::make_shared<ConeField>(v3at(p, 0), p[3], p[4]);
         }},
        {"cappedcone", "cx cy cz height radiusLow radiusHigh",
         {0, 0, 0, 1, 1, 0.5},
         [](const std::vector<double>& p) {
           return std::make_shared<CappedConeField>(v3at(p, 0), p[3], p[4],
                                                    p[5]);
         }},
        {"roundcone", "ax ay az bx by bz radiusA radiusB",
         {0, -1, 0, 0, 1, 0, 0.6, 0.3},
         [](const std::vector<double>& p) {
           return std::make_shared<RoundConeField>(v3at(p, 0), v3at(p, 3), p[6],
                                                   p[7]);
         }},
        {"infinitecylinder",
         "px py pz dx dy dz radius  [infinite -- needs --bounds]",
         {0, 0, 0, 0, 1, 0, 0.5},
         [](const std::vector<double>& p) {
           return std::make_shared<InfiniteCylinderField>(v3at(p, 0),
                                                          v3at(p, 3), p[6]);
         }},
        {"hexprism", "cx cy cz radius halfLength", {0, 0, 0, 1, 1},
         [](const std::vector<double>& p) {
           return std::make_shared<HexPrismField>(v3at(p, 0), p[3], p[4]);
         }},
        {"triprism", "cx cy cz radius halfLength", {0, 0, 0, 1, 1},
         [](const std::vector<double>& p) {
           return std::make_shared<TriPrismField>(v3at(p, 0), p[3], p[4]);
         }},
        {"octahedron", "cx cy cz size", {0, 0, 0, 1},
         [](const std::vector<double>& p) {
           return std::make_shared<OctahedronField>(v3at(p, 0), p[3]);
         }},
        {"pyramid", "cx cy cz height", {0, 0, 0, 1.5},
         [](const std::vector<double>& p) {
           return std::make_shared<PyramidField>(v3at(p, 0), p[3]);
         }},
        {"solidangle", "cx cy cz angleRad radius", {0, 0, 0, 0.7, 1.5},
         [](const std::vector<double>& p) {
           return std::make_shared<SolidAngleField>(v3at(p, 0), p[3], p[4]);
         }},
        // ---- Tier C ----
        {"cappedtorus", "cx cy cz angleRad major minor", {0, 0, 0, 1.0, 1, 0.3},
         [](const std::vector<double>& p) {
           return std::make_shared<CappedTorusField>(v3at(p, 0), p[3], p[4],
                                                     p[5]);
         }},
        {"link", "cx cy cz halfLength major minor", {0, 0, 0, 0.5, 1, 0.3},
         [](const std::vector<double>& p) {
           return std::make_shared<LinkField>(v3at(p, 0), p[3], p[4], p[5]);
         }},
        {"cutsphere", "cx cy cz radius cutHeight", {0, 0, 0, 1, 0.3},
         [](const std::vector<double>& p) {
           return std::make_shared<CutSphereField>(v3at(p, 0), p[3], p[4]);
         }},
        {"cuthollowsphere", "cx cy cz radius cutHeight thickness",
         {0, 0, 0, 1, -0.2, 0.1},
         [](const std::vector<double>& p) {
           return std::make_shared<CutHollowSphereField>(v3at(p, 0), p[3], p[4],
                                                         p[5]);
         }},
        {"deathstar", "cx cy cz radiusMain radiusBite distance",
         {0, 0, 0, 1, 0.7, 0.9},
         [](const std::vector<double>& p) {
           return std::make_shared<DeathStarField>(v3at(p, 0), p[3], p[4], p[5]);
         }},
        {"vesica", "ax ay az bx by bz width", {0, -1, 0, 0, 1, 0, 0.6},
         [](const std::vector<double>& p) {
           return std::make_shared<VesicaSegmentField>(v3at(p, 0), v3at(p, 3),
                                                       p[6]);
         }},
        {"rhombus", "cx cy cz la lb height cornerRadius",
         {0, 0, 0, 1, 0.6, 0.3, 0.0},
         [](const std::vector<double>& p) {
           return std::make_shared<RhombusField>(v3at(p, 0), p[3], p[4], p[5],
                                                 p[6]);
         }},
        {"verticalcapsule", "cx cy cz height radius", {0, 0, 0, 1.5, 0.4},
         [](const std::vector<double>& p) {
           return std::make_shared<VerticalCapsuleField>(v3at(p, 0), p[3],
                                                         p[4]);
         }},
        {"roundedcylinder", "cx cy cz radius roundRadius halfHeight",
         {0, 0, 0, 1, 0.2, 1},
         [](const std::vector<double>& p) {
           return std::make_shared<RoundedCylinderField>(v3at(p, 0), p[3], p[4],
                                                         p[5]);
         }},
        {"triangle",
         "ax ay az bx by bz cx cy cz  [open surface -- use --onion]",
         {0, 0, 0, 1, 0, 0, 0, 1, 0},
         [](const std::vector<double>& p) {
           return std::make_shared<TriangleField>(v3at(p, 0), v3at(p, 3),
                                                  v3at(p, 6));
         }},
        {"quad",
         "ax ay az bx by bz cx cy cz dx dy dz  [open surface -- use --onion]",
         {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0},
         [](const std::vector<double>& p) {
           return std::make_shared<QuadField>(v3at(p, 0), v3at(p, 3),
                                              v3at(p, 6), v3at(p, 9));
         }},
        {"infinitecone", "cx cy cz angleRad  [infinite -- needs --bounds]",
         {0, 0, 0, 0.5},
         [](const std::vector<double>& p) {
           return std::make_shared<InfiniteConeField>(v3at(p, 0), p[3]);
         }},
    };
    for (PrimEntry& e : c)
      e.infinite = e.signature.find("infinite") != std::string::npos;
    return c;
  }();
  return cat;
}

const PrimEntry* findPrimitive(const std::string& name) {
  for (const PrimEntry& e : primitiveCatalogue())
    if (e.name == name) return &e;
  return nullptr;
}

dualc::FieldPtr buildPrimitive(const std::string& name,
                               const std::vector<double>& params) {
  const PrimEntry* e = findPrimitive(name);
  if (e == nullptr) return nullptr;
  std::vector<double> p = e->defaults;
  for (std::size_t i = 0; i < params.size() && i < p.size(); ++i) p[i] = params[i];
  return e->build(p);
}

std::vector<std::string> paramNames(const std::string& signature) {
  std::vector<std::string> names;
  std::istringstream ss(signature);
  std::string tok;
  while (ss >> tok) {
    if (!tok.empty() && tok[0] == '[') break;  // start of a "[...]" note
    names.push_back(tok);
  }
  return names;
}

// --- Post-operators ---------------------------------------------------------

bool isPostOpFlag(const std::string& f) {
  return f == "--offset" || f == "--round" || f == "--onion" ||
         f == "--scale" || f == "--elongate" || f == "--translate" ||
         f == "--rotate" || f == "--twist" || f == "--bend" ||
         f == "--mirror" || f == "--repeat" || f == "--repeat-limited" ||
         f == "--displace";
}

bool parsePostOp(const std::string& flag, const std::string& val, PostOp& out,
                 std::string& err) {
  std::vector<double> n;
  auto bad = [&](const char* what) {
    err = "bad value for " + flag + ": '" + val + "' (" + what + ")";
    return false;
  };
  auto nums = [&](std::size_t want, const char* what, PostOpKind kind) {
    if (!parseDoubles(val, n) || n.size() != want) return bad(what);
    out = PostOp{kind, n, {}};
    return true;
  };
  if (flag == "--offset") return nums(1, "expect R", PostOpKind::Offset);
  if (flag == "--round") return nums(1, "expect R", PostOpKind::Round);
  if (flag == "--onion") return nums(1, "expect T", PostOpKind::Onion);
  if (flag == "--scale") return nums(1, "expect S", PostOpKind::Scale);
  if (flag == "--elongate") return nums(3, "expect hx,hy,hz", PostOpKind::Elongate);
  if (flag == "--translate") return nums(3, "expect dx,dy,dz", PostOpKind::Translate);
  if (flag == "--rotate") return nums(4, "expect ax,ay,az,deg", PostOpKind::Rotate);
  if (flag == "--twist") return nums(2, "expect k,axis", PostOpKind::Twist);
  if (flag == "--bend") return nums(2, "expect k,axis", PostOpKind::Bend);
  if (flag == "--mirror") return nums(3, "expect nx,ny,nz", PostOpKind::Mirror);
  if (flag == "--repeat") return nums(3, "expect px,py,pz", PostOpKind::Repeat);
  if (flag == "--repeat-limited")
    return nums(6, "expect px,py,pz,nx,ny,nz", PostOpKind::RepeatLimited);
  if (flag == "--displace") {
    out = PostOp{PostOpKind::Displace, {}, val};
    return true;
  }
  err = "unknown post-op flag: " + flag;
  return false;
}

dualc::FieldPtr applyPostOps(dualc::FieldPtr f, const std::vector<PostOp>& ops) {
  using namespace dualc;
  for (const PostOp& op : ops) {
    const std::vector<double>& p = op.params;
    switch (op.kind) {
      case PostOpKind::Offset:    if (p.size() == 1) f = offsetOf(f, p[0]); break;
      case PostOpKind::Round:     if (p.size() == 1) f = roundedOf(f, p[0]); break;
      case PostOpKind::Onion:     if (p.size() == 1) f = onionOf(f, p[0]); break;
      case PostOpKind::Scale:     if (p.size() == 1) f = scaled(f, p[0]); break;
      case PostOpKind::Elongate:
        if (p.size() == 3) f = elongated(f, Vector3{p[0], p[1], p[2]});
        break;
      case PostOpKind::Translate:
        if (p.size() == 3) f = transformed(f, Mat4::translation(Vector3{p[0], p[1], p[2]}));
        break;
      case PostOpKind::Rotate:
        if (p.size() == 4)
          f = transformed(f, Mat4::rotation(Vector3{p[0], p[1], p[2]},
                                            p[3] * 3.14159265358979323846 / 180.0));
        break;
      case PostOpKind::Twist:
        if (p.size() == 2) f = twisted(f, p[0], static_cast<int>(p[1]));
        break;
      case PostOpKind::Bend:
        if (p.size() == 2) f = bent(f, p[0], static_cast<int>(p[1]));
        break;
      case PostOpKind::Mirror:
        if (p.size() == 3) f = mirrored(f, Vector3{p[0], p[1], p[2]});
        break;
      case PostOpKind::Repeat:
        if (p.size() == 3) f = repeated(f, Vector3{p[0], p[1], p[2]});
        break;
      case PostOpKind::RepeatLimited:
        if (p.size() == 6)
          f = repeatedLimited(f, Vector3{p[0], p[1], p[2]},
                              Vector3i{static_cast<int>(p[3]),
                                       static_cast<int>(p[4]),
                                       static_cast<int>(p[5])});
        break;
      case PostOpKind::Displace:
        f = displaced(f, namedBump(op.bumpSpec));
        break;
    }
  }
  return f;
}

// --- Boolean operators ------------------------------------------------------

const char* boolOpName(BoolOp op) {
  switch (op) {
    case BoolOp::Union:              return "union";
    case BoolOp::Intersection:       return "intersection";
    case BoolOp::Difference:         return "difference";
    case BoolOp::Xor:                return "xor";
    case BoolOp::SmoothUnion:        return "smooth-union";
    case BoolOp::SmoothIntersection: return "smooth-intersection";
    case BoolOp::SmoothDifference:   return "smooth-difference";
  }
  return "union";
}

bool parseBoolOp(const std::string& s, BoolOp& out) {
  if (s == "union") { out = BoolOp::Union; return true; }
  if (s == "intersection") { out = BoolOp::Intersection; return true; }
  if (s == "difference") { out = BoolOp::Difference; return true; }
  if (s == "xor") { out = BoolOp::Xor; return true; }
  if (s == "smooth-union") { out = BoolOp::SmoothUnion; return true; }
  if (s == "smooth-intersection") { out = BoolOp::SmoothIntersection; return true; }
  if (s == "smooth-difference") { out = BoolOp::SmoothDifference; return true; }
  return false;
}

bool boolOpIsSmooth(BoolOp op) {
  return op == BoolOp::SmoothUnion || op == BoolOp::SmoothIntersection ||
         op == BoolOp::SmoothDifference;
}

dualc::FieldPtr applyBoolOp(BoolOp op, dualc::FieldPtr a, dualc::FieldPtr b,
                            double k) {
  using namespace dualc;
  switch (op) {
    case BoolOp::Union:              return unionOf(a, b);
    case BoolOp::Intersection:       return intersectionOf(a, b);
    case BoolOp::Difference:         return differenceOf(a, b);
    case BoolOp::Xor:                return xorOf(a, b);
    case BoolOp::SmoothUnion:        return smoothUnionOf(a, b, k);
    case BoolOp::SmoothIntersection: return smoothIntersectionOf(a, b, k);
    case BoolOp::SmoothDifference:   return smoothDifferenceOf(a, b, k);
  }
  return unionOf(a, b);
}

// --- CSG recipes ------------------------------------------------------------

const std::vector<RecipeEntry>& csgRecipes() {
  static const std::vector<RecipeEntry> r = {
      {"cube-minus-sphere", "hard difference of a box and a sphere", false,
       "boxHalf sphereCx sphereCy sphereCz sphereR", {1, 1, 1, 1, 1.2}},
      {"smooth-blend", "smooth union of two spheres (visible fillet)", false,
       "separation radius blendK", {0.6, 0.8, 0.5}},
      {"displaced-sphere", "a sphere perturbed by a sinusoidal bump", false,
       "radius amplitude frequency", {1.0, 0.08, 8.0}},
      {"mesh-shell", "onionOf(mesh): a hollow shell of the input", true,
       "thickness", {0.05}},
      {"mesh-minus-sphere", "differenceOf(mesh, sphere): carve the input", true,
       "sphereCx sphereCy sphereCz sphereR", {0.5, 0.5, 0.5, 0.5}},
      {"twisted-mesh", "twisted(mesh): a domain warp on the input", true,
       "radPerUnit axis", {2.0, 1}},
  };
  return r;
}

const RecipeEntry* findRecipe(const std::string& name) {
  for (const RecipeEntry& r : csgRecipes())
    if (r.name == name) return &r;
  return nullptr;
}

dualc::FieldPtr buildRecipe(
    const std::string& name, const std::vector<double>& params,
    geometrycentral::surface::SurfaceMesh* mesh,
    geometrycentral::surface::VertexPositionGeometry* geom) {
  using namespace dualc;
  const RecipeEntry* e = findRecipe(name);
  if (e == nullptr) return nullptr;
  if (e->needsMesh && (mesh == nullptr || geom == nullptr)) return nullptr;
  std::vector<double> p = e->defaults;
  for (std::size_t i = 0; i < params.size() && i < p.size(); ++i) p[i] = params[i];

  if (name == "cube-minus-sphere") {
    auto box = std::make_shared<BoxField>(Vector3{-p[0], -p[0], -p[0]},
                                          Vector3{p[0], p[0], p[0]});
    auto sph = std::make_shared<SphereField>(Vector3{p[1], p[2], p[3]}, p[4]);
    return differenceOf(box, sph);
  }
  if (name == "smooth-blend") {
    auto a = std::make_shared<SphereField>(Vector3{-p[0], 0, 0}, p[1]);
    auto b = std::make_shared<SphereField>(Vector3{p[0], 0, 0}, p[1]);
    return smoothUnionOf(a, b, p[2]);
  }
  if (name == "displaced-sphere") {
    auto sph = std::make_shared<SphereField>(Vector3{0, 0, 0}, p[0]);
    const double amp = p[1], freq = p[2];
    return displaced(sph, [amp, freq](const Vector3& q) {
      return amp * std::sin(freq * q.x) * std::sin(freq * q.y) *
             std::sin(freq * q.z);
    });
  }
  if (name == "mesh-shell") {
    auto src = std::make_shared<MeshSource>(*mesh, *geom);
    return onionOf(src, p[0]);
  }
  if (name == "mesh-minus-sphere") {
    auto src = std::make_shared<MeshSource>(*mesh, *geom);
    auto sph = std::make_shared<SphereField>(Vector3{p[0], p[1], p[2]}, p[3]);
    return differenceOf(src, sph);
  }
  if (name == "twisted-mesh") {
    auto src = std::make_shared<MeshSource>(*mesh, *geom);
    return twisted(src, p[0], static_cast<int>(p[1]));
  }
  return nullptr;
}

namespace {

// The result of one sample+contour pass, shared by every output format so a
// single field is never contoured twice.
struct Contoured {
  std::unique_ptr<geometrycentral::surface::SurfaceMesh> mesh;
  std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> geom;
  std::vector<dualc::Vector3> normals;
  dualc::Diagnostics diag;
};

// Flat triangle soup over a shared 0-based vertex pool, ready for STL/3MF.
// Dual contouring may emit quads, so every face is fan-triangulated here.
struct TriMesh {
  std::vector<dualc::Vector3> pos;                  // indexed by getIndex()
  std::vector<std::array<std::size_t, 3>> tris;
};

// Run the pipeline, translating the sampler's unbounded-field exception into
// the same actionable hint the OBJ writer used to print. Returns false (after
// printing) on that path so callers can return exit code 1.
// `collectDiag` is opt-in because Diagnostics costs an O(E) pass over the
// emitted triangles. The monolithic export takes it (once, to warn the user);
// the tiled/streaming driver and the --mem probe do not -- per-tile overhead
// on the one path whose whole purpose is bounded cost would be a poor trade
// for a warning the streamer already handles per tile.
// `cancel` / `progress` (dualc/progress.h) are forwarded to the engine; a
// dualc::Cancelled thrown there passes through untouched -- the drivers below
// catch it after cleaning up and turn it into rc 3.
bool contourOrHint(const dualc::ImplicitField& field,
                   const dualc::SamplerParams& sp,
                   const dualc::ContourerParams& cp, Contoured& out,
                   bool collectDiag = false,
                   const dualc::CancelToken* cancel = nullptr,
                   dualc::ProgressSink* progress = nullptr) {
  try {
    auto [mesh, geom, normals] = dualc::dualContourField(
        field, sp, cp, collectDiag ? &out.diag : nullptr, cancel, progress);
    out.mesh = std::move(mesh);
    out.geom = std::move(geom);
    out.normals = std::move(normals);
    return true;
  } catch (const std::invalid_argument& e) {
    std::cerr << "[dualc] error: " << e.what() << "\n"
              << "[dualc] hint: this field is unbounded -- pass "
                 "--bounds x0,y0,z0,x1,y1,z1 to give the sampler an explicit "
                 "finite region.\n";
    return false;
  }
}

// Lowercased file extension including the dot ("" if none).
std::string lowerExt(const std::string& path) {
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string::npos) return "";
  std::string ext = path.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return ext;
}

TriMesh toTriMesh(const Contoured& c) {
  TriMesh t;
  t.pos.resize(c.mesh->nVertices());
  for (auto v : c.mesh->vertices())
    t.pos[v.getIndex()] = c.geom->inputVertexPositions[v];
  for (const std::vector<std::size_t>& f : c.mesh->getFaceVertexList()) {
    if (f.size() < 3) continue;  // degenerate
    for (std::size_t i = 1; i + 1 < f.size(); ++i)
      t.tris.push_back({f[0], f[i], f[i + 1]});
  }
  return t;
}

// QEM-decimate a triangle mesh via meshoptimizer (Approach A -- monolithic).
// Input must be a shared-vertex indexed mesh: toTriMesh() produces exactly that
// (faces reference vertex getIndex()), which is what lets edge collapses happen
// at all -- an unwelded triangle soup would decimate to nothing. Returns the
// simplified mesh with a compacted vertex pool (unused vertices dropped, so no
// dangling verts reach the .obj/.3mf writers). `outError` receives the achieved
// geometric error in world units (mm). A no-op copy for Mode::None / empty mesh.
TriMesh decimate(const TriMesh& in, const DecimateOpts& dec, float& outError) {
  outError = 0.0f;
  if (dec.mode == DecimateOpts::Mode::None || in.tris.empty()) return in;

  const std::size_t vcount = in.pos.size();
  const std::size_t indexCount = in.tris.size() * 3;

  // Flatten to meshopt's buffers: float3 positions + uint indices. The
  // double->float cast is harmless here (~7 sig digits => ~2 um at 25 mm scale,
  // far below the ~0.05 mm decimation-error target).
  std::vector<float> positions(vcount * 3);
  for (std::size_t i = 0; i < vcount; ++i) {
    positions[3 * i + 0] = static_cast<float>(in.pos[i].x);
    positions[3 * i + 1] = static_cast<float>(in.pos[i].y);
    positions[3 * i + 2] = static_cast<float>(in.pos[i].z);
  }
  std::vector<unsigned int> indices(indexCount);
  for (std::size_t t = 0; t < in.tris.size(); ++t)
    for (int k = 0; k < 3; ++k)
      indices[3 * t + k] = static_cast<unsigned int>(in.tris[t][k]);

  // Ratio: bound the triangle count, no error ceiling. Error: bound the
  // absolute geometric error (world units, via SimplifyErrorAbsolute) and let
  // the count fall as far as that allows.
  std::size_t targetIndexCount;
  float targetError;
  unsigned int options = 0;
  if (dec.mode == DecimateOpts::Mode::Ratio) {
    targetIndexCount =
        static_cast<std::size_t>(std::llround(indexCount * dec.value)) / 3 * 3;
    targetError = std::numeric_limits<float>::max();
  } else {  // Error
    targetIndexCount = 0;
    targetError = static_cast<float>(dec.value);
    options = meshopt_SimplifyErrorAbsolute;
  }

  // destination needs room for index_count (worst case), per the meshopt docs.
  std::vector<unsigned int> simplified(indexCount);
  const std::size_t newIndexCount = meshopt_simplify(
      simplified.data(), indices.data(), indexCount, positions.data(), vcount,
      sizeof(float) * 3, targetIndexCount, targetError, options, &outError);
  simplified.resize(newIndexCount);

  // Compact: meshopt_simplify's output still references the original (full)
  // vertex array; optimizeVertexFetch reorders + drops the unused vertices,
  // rewriting `simplified` in place to index the compacted pool.
  std::vector<float> outPositions(vcount * 3);
  const std::size_t newVCount = meshopt_optimizeVertexFetch(
      outPositions.data(), simplified.data(), newIndexCount, positions.data(),
      vcount, sizeof(float) * 3);

  TriMesh out;
  out.pos.resize(newVCount);
  for (std::size_t i = 0; i < newVCount; ++i)
    out.pos[i] = {static_cast<double>(outPositions[3 * i + 0]),
                  static_cast<double>(outPositions[3 * i + 1]),
                  static_cast<double>(outPositions[3 * i + 2])};
  out.tris.reserve(newIndexCount / 3);
  for (std::size_t t = 0; t + 2 < newIndexCount; t += 3)
    out.tris.push_back({simplified[t], simplified[t + 1], simplified[t + 2]});
  return out;
}

// OBJ writer for a decimated TriMesh (no SurfaceMesh available anymore): emits
// positions, recomputed angle-weighted per-vertex normals, and `v//vn` faces.
// Angle weighting stays stable on the irregular triangles a QEM pass produces.
int writeObjFromTriMesh(const TriMesh& m, const std::string& path) {
  OutputFile own;
  std::ofstream os;
  if (!own.open(os, path, /*binary=*/false)) {
    reportError("cannot open '" + path + "' for writing: " + errnoText());
    return 2;
  }
  auto sub = [](const dualc::Vector3& a, const dualc::Vector3& b) {
    return dualc::Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
  };
  auto len = [](const dualc::Vector3& a) {
    return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
  };
  auto angle = [&](const dualc::Vector3& u, const dualc::Vector3& v) {
    const double lu = len(u), lv = len(v);
    if (lu <= 0.0 || lv <= 0.0) return 0.0;
    double c = (u.x * v.x + u.y * v.y + u.z * v.z) / (lu * lv);
    c = std::max(-1.0, std::min(1.0, c));
    return std::acos(c);
  };
  std::vector<dualc::Vector3> normals(m.pos.size(), dualc::Vector3{0.0, 0.0, 0.0});
  for (const auto& tri : m.tris) {
    const dualc::Vector3& p0 = m.pos[tri[0]];
    const dualc::Vector3& p1 = m.pos[tri[1]];
    const dualc::Vector3& p2 = m.pos[tri[2]];
    const dualc::Vector3 e01 = sub(p1, p0), e02 = sub(p2, p0);
    dualc::Vector3 fn{e01.y * e02.z - e01.z * e02.y,
                      e01.z * e02.x - e01.x * e02.z,
                      e01.x * e02.y - e01.y * e02.x};
    const double fnLen = len(fn);
    if (fnLen <= 0.0) continue;  // degenerate triangle contributes nothing
    fn = {fn.x / fnLen, fn.y / fnLen, fn.z / fnLen};
    const double w[3] = {angle(e01, e02), angle(sub(p2, p1), sub(p0, p1)),
                         angle(sub(p0, p2), sub(p1, p2))};
    for (int k = 0; k < 3; ++k) {
      dualc::Vector3& n = normals[tri[k]];
      n.x += fn.x * w[k];
      n.y += fn.y * w[k];
      n.z += fn.z * w[k];
    }
  }
  for (auto& n : normals) {
    const double l = len(n);
    if (l > 0.0) { n.x /= l; n.y /= l; n.z /= l; }
    else n = dualc::Vector3{0.0, 0.0, 1.0};
  }
  os << "# dualc decimated mesh\n";
  for (const auto& p : m.pos)
    os << "v " << p.x << ' ' << p.y << ' ' << p.z << '\n';
  for (const auto& n : normals)
    os << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
  for (const auto& tri : m.tris)
    os << "f " << (tri[0] + 1) << "//" << (tri[0] + 1) << ' ' << (tri[1] + 1)
       << "//" << (tri[1] + 1) << ' ' << (tri[2] + 1) << "//" << (tri[2] + 1)
       << '\n';
  if (!os || !own.close(os)) {
    reportError("failed writing OBJ '" + path + "'");
    return 2;
  }
  return 0;
}

// The `.part` convention: every export writes `path + ".part"` and renames it
// over `path` only after a successful finish, so a destination file only ever
// appears complete. A cancelled run, a writer failure, an unbounded-field
// error or a killed process leaves at most a `.part` file, never a truncated
// `<path>` -- and a pre-existing `<path>` survives a failed re-export instead
// of being truncated at open. One RAII object per driver: commit() renames,
// the destructor removes the temp if commit() was never reached. Sinks that
// hold the temp open must close it (abort()) before this destructor runs.
class AtomicOutput {
 public:
  explicit AtomicOutput(std::string path)
      : path_(std::move(path)), tmp_(path_ + ".part") {}
  AtomicOutput(const AtomicOutput&) = delete;
  AtomicOutput& operator=(const AtomicOutput&) = delete;
  ~AtomicOutput() {
    if (committed_) return;
    // Bounded like commit() but shorter: a destructor must not block long,
    // and a stray `.part` is the lesser evil next to a hang.
    std::error_code ec;
    int attempts = 0;
    long long spent = 0;
    retryFileOp([&](std::error_code& e) {
      std::filesystem::remove(tmp_, e);
      return !e;
    }, 150, ec, attempts, spent);
  }
  const std::string& tmp() const { return tmp_; }
  // Rename the temp over the destination. std::filesystem::rename replaces an
  // existing file on every platform (std::rename does not on Windows). On
  // Windows a sharing violation is retried for up to ~0.5 s (a scanner's
  // hold); a longer hold -- another process holding the handle -- fails.
  bool commit() {
    std::error_code ec;
    int attempts = 0;
    long long spent = 0;
    const bool ok = retryFileOp([&](std::error_code& e) {
      std::filesystem::rename(tmp_, path_, e);
      return !e;
    }, 500, ec, attempts, spent);
    if (!ok) {
      std::ostringstream os;
      os << "cannot move '" << tmp_ << "' to '" << path_ << "': " << ec.message();
      if (attempts > 1) os << " (after " << attempts << " attempts over " << spent << " ms)";
      reportError(os.str());
      return false;
    }
    if (attempts > 0)
      std::cerr << "[dualc] warning: moved '" << tmp_ << "' to '" << path_ << "' after "
                << attempts << " retries (" << spent << " ms): " << ec.message() << "\n";
    committed_ = true;
    return true;
  }

 private:
  std::string path_, tmp_;
  bool committed_ = false;
};

// Existing OBJ path: per-vertex normals projected onto corners so the writer
// emits smooth-shading `vn` lines.
int writeObj(const Contoured& c, const std::string& path) {
  geometrycentral::surface::CornerData<dualc::Vector3> cornerNormals(*c.mesh);
  for (auto v : c.mesh->vertices()) {
    const std::size_t i = v.getIndex();
    const dualc::Vector3 n =
        (i < c.normals.size()) ? c.normals[i] : dualc::Vector3{0.0, 0.0, 1.0};
    for (auto corner : v.adjacentCorners()) cornerNormals[corner] = n;
  }
  geometrycentral::surface::WavefrontOBJ::write(path, *c.geom, cornerNormals);
  return 0;
}

// Pack one binary-STL facet record (50 bytes: float[12] = normal + 3 verts,
// then a uint16 attribute) into `rec`. The facet normal is computed from the
// (CCW, as emitted by getFaceVertexList) winding. memcpy keeps struct padding
// out of the stream. Shared by the buffered (writeStl) and streaming
// (StreamingStl) writers so the on-disk format can never drift between them.
void packStlTriangle(const dualc::Vector3& a, const dualc::Vector3& b,
                     const dualc::Vector3& c, char rec[50]) {
  const dualc::Vector3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
  const dualc::Vector3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
  dualc::Vector3 n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z,
                   e1.x * e2.y - e1.y * e2.x};
  const double len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
  if (len > 0.0) { n.x /= len; n.y /= len; n.z /= len; }
  const float f[12] = {
      static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z),
      static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z),
      static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z),
      static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z)};
  std::memcpy(rec, f, 48);
  const std::uint16_t attr = 0;
  std::memcpy(rec + 48, &attr, 2);
}

// Binary STL: 80-byte header + uint32 facet count + 50 bytes/triangle. We
// target little-endian (win32/x64).
int writeStl(const TriMesh& m, const std::string& path) {
  OutputFile own;
  std::ofstream os;
  if (!own.open(os, path)) {
    reportError("cannot open '" + path + "' for writing: " + errnoText());
    return 2;
  }
  char header[80] = {0};  // must NOT begin with "solid" (would read as ASCII)
  os.write(header, sizeof header);
  const std::uint32_t count = static_cast<std::uint32_t>(m.tris.size());
  os.write(reinterpret_cast<const char*>(&count), 4);

  char rec[50];
  for (const auto& tri : m.tris) {
    packStlTriangle(m.pos[tri[0]], m.pos[tri[1]], m.pos[tri[2]], rec);
    os.write(rec, 50);
  }
  if (!os || !own.close(os)) {
    reportError("failed writing STL '" + path + "'");
    return 2;
  }
  return 0;
}

// Incremental binary-STL writer for the tiled/streaming path: writes the header
// + a placeholder facet count up front, appends 50-byte records as tiles are
// contoured, and patches the real count into bytes 80-83 on finish(). Peak RAM
// is one tile's mesh, never the whole part.
class StreamingStl {
 public:
  bool open(const std::string& path) {
    if (!own_.open(os_, path)) return false;
    char header[80] = {0};  // must NOT begin with "solid"
    os_.write(header, sizeof header);
    const std::uint32_t placeholder = 0;
    os_.write(reinterpret_cast<const char*>(&placeholder), 4);
    return static_cast<bool>(os_);
  }
  void addTriangle(const dualc::Vector3& a, const dualc::Vector3& b,
                   const dualc::Vector3& c) {
    char rec[50];
    packStlTriangle(a, b, c, rec);
    os_.write(rec, 50);
    ++count_;
  }
  // Seek back and patch the facet count. Returns false on any stream error.
  bool finish() {
    os_.seekp(80, std::ios::beg);
    os_.write(reinterpret_cast<const char*>(&count_), 4);
    const bool ok = static_cast<bool>(os_);
    return own_.close(os_) && ok;
  }
  // Give up: close the stream so the driver can remove the file (Windows
  // cannot unlink an open file). Safe to call more than once.
  void abort() {
    own_.close(os_);
  }
  std::uint32_t count() const { return count_; }

 private:
  OutputFile own_;   // before os_: destroyed after it
  std::ofstream os_;
  std::uint32_t count_ = 0;
};

// True if `c` is the empty-output placeholder the contourer emits for a region
// with no surface: a single triangle at (0,0,0)/(1,0,0)/(0,1,0) (see the
// empty-output fallback in contourHermiteOctree, src/contourer.cpp). Empty tiles
// are common, so the streamer must drop these or they leak as stray triangles.
bool isEmptyPlaceholder(const Contoured& c) {
  if (c.mesh->nFaces() != 1 || c.mesh->nVertices() != 3) return false;
  int hits = 0;
  for (auto v : c.mesh->vertices()) {
    const dualc::Vector3 p = c.geom->inputVertexPositions[v];
    const bool isCorner =
        (p.x == 0.0 && p.y == 0.0 && p.z == 0.0) ||
        (p.x == 1.0 && p.y == 0.0 && p.z == 0.0) ||
        (p.x == 0.0 && p.y == 1.0 && p.z == 0.0);
    if (isCorner) ++hits;
  }
  return hits == 3;
}

// Fan-triangulate and stream the faces of `c` for which keep(centroid) is true,
// to `sink` (any type with addTriangle(a,b,c)). Returns true if at least one
// triangle was emitted. Shared by the single-pass and per-tile streaming paths.
template <class Sink, class KeepFn>
bool streamFaces(Sink& sink, const Contoured& c, KeepFn keep) {
  std::vector<dualc::Vector3> pos(c.mesh->nVertices());
  for (auto v : c.mesh->vertices())
    pos[v.getIndex()] = c.geom->inputVertexPositions[v];
  bool any = false;
  for (const std::vector<std::size_t>& f : c.mesh->getFaceVertexList()) {
    if (f.size() < 3) continue;
    dualc::Vector3 ctr{0.0, 0.0, 0.0};
    for (std::size_t vi : f) {
      ctr.x += pos[vi].x; ctr.y += pos[vi].y; ctr.z += pos[vi].z;
    }
    const double inv = 1.0 / static_cast<double>(f.size());
    ctr.x *= inv; ctr.y *= inv; ctr.z *= inv;
    if (!keep(ctr)) continue;
    for (std::size_t i = 1; i + 1 < f.size(); ++i) {
      sink.addTriangle(pos[f[0]], pos[f[i]], pos[f[i + 1]]);
      any = true;
    }
  }
  return any;
}

// The two fixed OPC parts every 3MF needs, shared by the buffered (write3mf) and
// streaming (Tiled3mfSink) writers so the container can never drift between them.
const char* const kContentTypes3mf =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
    " <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
    " <Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n"
    "</Types>\n";
const char* const kRels3mf =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
    " <Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
    "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>\n"
    "</Relationships>\n";
// The <model> header: 1 world unit == 1 mm (declared via unit="millimeter").
const char* const kModelHeader3mf =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<model unit=\"millimeter\" xml:lang=\"en-US\" "
    "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n";

// 3MF: a minimal OPC ZIP (deflate via miniz) holding the three required parts.
// Geometry is one mesh object; 1 world unit == 1 mm (declared on <model>).
int write3mf(const TriMesh& m, const std::string& path) {
  const char* kContentTypes = kContentTypes3mf;
  const char* kRels = kRels3mf;

  std::string model;
  model.reserve(m.pos.size() * 56 + m.tris.size() * 44 + 512);
  model += kModelHeader3mf;
  model +=
      " <resources>\n  <object id=\"1\" type=\"model\">\n   <mesh>\n"
      "    <vertices>\n";
  char buf[160];
  for (const dualc::Vector3& p : m.pos) {
    std::snprintf(buf, sizeof buf,
                  "     <vertex x=\"%.9g\" y=\"%.9g\" z=\"%.9g\"/>\n", p.x, p.y,
                  p.z);
    model += buf;
  }
  model += "    </vertices>\n    <triangles>\n";
  for (const auto& t : m.tris) {
    std::snprintf(buf, sizeof buf,
                  "     <triangle v1=\"%zu\" v2=\"%zu\" v3=\"%zu\"/>\n", t[0],
                  t[1], t[2]);
    model += buf;
  }
  model +=
      "    </triangles>\n   </mesh>\n  </object>\n </resources>\n"
      " <build>\n  <item objectid=\"1\"/>\n </build>\n</model>\n";

  // The ZIP goes through a FILE* we open (non-inheritable) and close; miniz's
  // cfile init never closes a caller's FILE.
  FILE* zf = openOutputFile(path, true);
  if (!zf) {
    reportError("cannot open '" + path + "' for writing: " + errnoText());
    return 2;
  }
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof zip);
  if (!mz_zip_writer_init_cfile(&zip, zf, 0)) {
    std::fclose(zf);
    reportError("cannot open '" + path + "' for writing (zip init)");
    return 2;
  }
  auto add = [&](const char* name, const std::string& data) {
    return mz_zip_writer_add_mem(&zip, name, data.data(), data.size(),
                                 MZ_DEFAULT_LEVEL) != MZ_FALSE;
  };
  bool ok = add("[Content_Types].xml", kContentTypes) &&
            add("_rels/.rels", kRels) && add("3D/3dmodel.model", model);
  if (ok) ok = mz_zip_writer_finalize_archive(&zip) != MZ_FALSE;
  mz_zip_writer_end(&zip);
  if (std::fclose(zf) != 0) ok = false;
  if (!ok) {
    reportError("failed writing 3MF '" + path + "'");
    return 2;
  }
  return 0;
}

// Report every degradation the library flagged. This used to test
// `nFaces() == 0`, which could never be true: the contourer synthesizes a
// placeholder triangle for an empty result, so the warning was dead code and
// an empty field wrote a one-triangle file in silence.
// Diagnostics::emptyContour is the signal that actually distinguishes the two.
void warnDiagnostics(const Contoured& c) {
  const dualc::Diagnostics& d = c.diag;
  if (d.emptyContour)
    std::cerr << "[dualc] warning: no surface crossed the sampled region -- "
                 "the output is a single placeholder triangle, not geometry. "
                 "Check --bounds and the field's sign convention.\n";
  if (d.boundsFallback)
    std::cerr << "[dualc] warning: the field reported unusable bounds, so the "
                 "sampled region fell back to the unit cube [-0.5,0.5]^3. "
                 "Pass --bounds to sample the region you meant.\n";
  if (d.gridBoundsExceeded)
    std::cerr << "[dualc] warning: the sampled region reaches outside the "
                 "baked grid, where the field reads clamped edge values "
                 "instead of real samples.\n";
  if (!d.outputWatertight && !d.emptyContour)
    std::cerr << "[dualc] warning: the output mesh is not closed ("
              << d.outputBoundaryEdges << " boundary edge(s), "
              << d.outputNonManifoldEdges
              << " non-manifold edge(s)) -- it may be rejected by a slicer.\n";
}

// Dispatch to the per-format writer chosen by the extension of `path` (the
// destination the user named), writing to `target` -- the AtomicOutput temp
// the caller commits and then logs.
int dispatchWrite(const Contoured& c, const std::string& path,
                  const std::string& target) {
  const std::string ext = lowerExt(path);
  if (ext == ".obj") return writeObj(c, target);
  if (ext == ".stl") return writeStl(toTriMesh(c), target);
  if (ext == ".3mf") return write3mf(toTriMesh(c), target);
  reportError("unknown output extension '" + ext + "' (use .obj, .stl or .3mf)");
  return 2;
}

// ---------------------------------------------------------------------------
// Tiled/streaming export: a format-agnostic driver + per-format sinks.
//
// forEachOwnedTile() owns the tiling: it resolves the global sampling box + cell
// grid, contours the field in uniform grid-aligned cubic tiles (one-cell ghost
// ring + a centroid-ownership rule so every boundary quad is emitted exactly
// once), and hands each non-empty tile's owned faces to a `Sink`. Peak RAM is one
// tile. The Sink chooses the on-disk format:
//   - TiledStlSink  -> independent-triangle binary STL soup (seams duplicated).
//   - Tiled3mfSink  -> one deflated 3MF object per tile (within-tile shared
//                      vertices), streamed to disk so peak RAM stays one tile.
// A Sink must provide: void setGrid(const TileGrid&); bool open(path);
// onTile(const Contoured&, KeepFn); bool finish(); void abort();
// std::uint32_t count() (triangles emitted, for progress/report).  setGrid is
// called once, after the grid is resolved and before the tile loop, so a sink
// can classify vertices relative to the tile seams; most sinks ignore it.
// abort() is the driver's exit on every non-success path (a cancel, a failed
// tile, a stray exception): close every stream and remove the sink's own
// temp files, so the driver's AtomicOutput can then remove the `.part`. The
// sinks also call it from their destructors, so an unwinding stack never
// leaks a temp.

// Global tile-grid geometry handed to a sink before the tile loop.
struct TileGrid {
  dualc::BBox box;      // global sampling box
  dualc::Vector3 cs;    // global cell size (extent / 2^depth per axis)
  int ownedCells = 0;   // owned (non-ghost) cells per axis in one tile
  int nt = 1;           // tiles per axis
};

// Append the raw bytes of `path` to the open stream `out` in bounded-RAM chunks.
bool appendFileTo(std::ofstream& out, const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  char buf[1 << 16];
  while (in) {
    in.read(buf, sizeof buf);
    out.write(buf, in.gcount());
  }
  return static_cast<bool>(out);
}

// STL sink: fan-triangulate owned faces straight into the incremental STL writer.
class TiledStlSink {
 public:
  void setGrid(const TileGrid&) {}
  bool open(const std::string& path) { return stl_.open(path); }
  template <class KeepFn>
  bool onTile(const Contoured& c, KeepFn keep) {
    return streamFaces(stl_, c, keep);
  }
  bool finish() { return stl_.finish(); }
  void abort() { stl_.abort(); }
  ~TiledStlSink() { abort(); }
  std::uint32_t count() const { return stl_.count(); }

 private:
  StreamingStl stl_;
};

// 3MF sink: each tile becomes one <object> whose <mesh> shares vertices within
// the tile (dedup by original vertex index over the kept faces). The model XML
// is streamed to a temp file as tiles arrive; on finish() the temp file is
// packed into the .3mf ZIP via miniz's disk-streaming add_file (incremental
// deflate), so neither the whole model string nor the whole compressed buffer is
// ever resident. Across-tile seam vertices stay duplicated (as in the STL soup);
// welding them is a separate feature.
class Tiled3mfSink {
 public:
  void setGrid(const TileGrid&) {}
  bool open(const std::string& path) {
    path_ = path;
    tmpPath_ = path + ".model.tmp";
    if (!own_.open(model_, tmpPath_)) return false;
    model_ << kModelHeader3mf << " <resources>\n";
    return static_cast<bool>(model_);
  }

  template <class KeepFn>
  bool onTile(const Contoured& c, KeepFn keep) {
    std::vector<dualc::Vector3> pos(c.mesh->nVertices());
    for (auto v : c.mesh->vertices())
      pos[v.getIndex()] = c.geom->inputVertexPositions[v];

    // Within-tile vertex remap over the kept faces only: assign compact local
    // indices in first-seen order, collect the fan-triangulated index triples.
    std::vector<int> remap(c.mesh->nVertices(), -1);
    std::vector<std::size_t> localVerts;             // original -> emission order
    std::vector<std::array<std::size_t, 3>> localTris;
    auto mapv = [&](std::size_t vi) -> std::size_t {
      if (remap[vi] < 0) {
        remap[vi] = static_cast<int>(localVerts.size());
        localVerts.push_back(vi);
      }
      return static_cast<std::size_t>(remap[vi]);
    };
    for (const std::vector<std::size_t>& f : c.mesh->getFaceVertexList()) {
      if (f.size() < 3) continue;
      dualc::Vector3 ctr{0.0, 0.0, 0.0};
      for (std::size_t vi : f) { ctr.x += pos[vi].x; ctr.y += pos[vi].y; ctr.z += pos[vi].z; }
      const double inv = 1.0 / static_cast<double>(f.size());
      ctr.x *= inv; ctr.y *= inv; ctr.z *= inv;
      if (!keep(ctr)) continue;
      for (std::size_t i = 1; i + 1 < f.size(); ++i)
        localTris.push_back({mapv(f[0]), mapv(f[i]), mapv(f[i + 1])});
    }
    if (localTris.empty()) return false;

    ++objId_;
    char buf[192];
    model_ << "  <object id=\"" << objId_ << "\" type=\"model\">\n"
              "   <mesh>\n    <vertices>\n";
    for (std::size_t vi : localVerts) {
      std::snprintf(buf, sizeof buf,
                    "     <vertex x=\"%.9g\" y=\"%.9g\" z=\"%.9g\"/>\n",
                    pos[vi].x, pos[vi].y, pos[vi].z);
      model_ << buf;
    }
    model_ << "    </vertices>\n    <triangles>\n";
    for (const std::array<std::size_t, 3>& t : localTris) {
      std::snprintf(buf, sizeof buf,
                    "     <triangle v1=\"%zu\" v2=\"%zu\" v3=\"%zu\"/>\n",
                    t[0], t[1], t[2]);
      model_ << buf;
    }
    model_ << "    </triangles>\n   </mesh>\n  </object>\n";
    buildIds_.push_back(objId_);
    triCount_ += static_cast<std::uint32_t>(localTris.size());
    return static_cast<bool>(model_);
  }

  bool finish() {
    // Close the model part: one build item per emitted object.
    model_ << " </resources>\n <build>\n";
    for (std::uint32_t id : buildIds_)
      model_ << "  <item objectid=\"" << id << "\"/>\n";
    model_ << " </build>\n</model>\n";
    const bool wrote = static_cast<bool>(model_);
    if (!own_.close(model_) || !wrote) { std::remove(tmpPath_.c_str()); return false; }

    // Pack the ZIP: the two tiny fixed parts in memory, the model streamed from
    // the temp file (miniz deflates incrementally -> one-tile peak RAM holds).
    FILE* zf = openOutputFile(path_, true);
    if (!zf) {
      std::remove(tmpPath_.c_str());
      return false;
    }
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof zip);
    if (!mz_zip_writer_init_cfile(&zip, zf, 0)) {
      std::fclose(zf);
      std::remove(tmpPath_.c_str());
      return false;
    }
    bool ok =
        mz_zip_writer_add_mem(&zip, "[Content_Types].xml", kContentTypes3mf,
                              std::strlen(kContentTypes3mf),
                              MZ_DEFAULT_LEVEL) != MZ_FALSE &&
        mz_zip_writer_add_mem(&zip, "_rels/.rels", kRels3mf,
                              std::strlen(kRels3mf),
                              MZ_DEFAULT_LEVEL) != MZ_FALSE &&
        mz_zip_writer_add_file(&zip, "3D/3dmodel.model", tmpPath_.c_str(),
                               nullptr, 0, MZ_DEFAULT_LEVEL) != MZ_FALSE;
    if (ok) ok = mz_zip_writer_finalize_archive(&zip) != MZ_FALSE;
    mz_zip_writer_end(&zip);
    if (std::fclose(zf) != 0) ok = false;
    std::remove(tmpPath_.c_str());
    return ok;
  }

  void abort() {
    own_.close(model_);
    if (!tmpPath_.empty()) std::remove(tmpPath_.c_str());
  }
  ~Tiled3mfSink() { abort(); }

  std::uint32_t count() const { return triCount_; }

 private:
  std::string path_;
  std::string tmpPath_;
  OutputFile own_;   // before model_: destroyed after it
  std::ofstream model_;
  std::uint32_t objId_ = 0;
  std::uint32_t triCount_ = 0;
  std::vector<std::uint32_t> buildIds_;
};

// Welded 3MF sink: produces a SINGLE <object> with a global shared-vertex pool,
// welding the coincident vertices that adjacent tiles both emit at their shared
// seam -- a globally manifold, connectivity-carrying mesh (for re-booleans, FEA,
// decimation).
//
// RAM: NOT one-tile (unlike the STL / per-tile-3MF sinks). One tile of geometry
// is live, PLUS a seam-vertex hash that only holds vertices within a few cells of
// an internal tile seam plane -- a 2D subset, O(part seam area) ~ O(nt*nGlobal^2).
// That is far below the full 3D mesh (interior vertices are unique to their tile,
// get a fresh global id deduped only *within* the tile, and never enter the hash)
// but it grows with the part; the hash never shrinks here. A truly O(one-tile)
// bound needs the deferred moving-front eviction (drop a seam plane's entries once
// both its neighbouring tiles are processed) -- see roadmap 11 5a.4.
//
// Two robustness choices (see the plan / roadmap 11 5a.4):
//  * Weld key = position quantised to eps = cs*1e-6 per axis. Coincident seam
//    vertices are bit-identical (dyadic bounds) or differ by a few ULPs
//    (non-dyadic) -- both far below eps; distinct vertices (different DC cells, or
//    different manifold components of one cell) are ~cs apart, far above eps -- so
//    the key merges the coincident pair without false-merging distinct sheets.
//  * Seam classification is by DISTANCE to the seam PLANE (a cs-scale band), not
//    by which integer cell a vertex floors into, so a ULP nudge can never flip a
//    shared vertex between the hashed and un-hashed sets (which would crack it).
//
// A single <object> needs all <vertices> before all <triangles>, but both are
// discovered as tiles stream, so they go to two temp files; finish() concatenates
// header + verts + tris + footer into the model part and packs the .3mf.
class Welded3mfSink {
 public:
  void setGrid(const TileGrid& g) { g_ = g; }

  bool open(const std::string& path) {
    path_ = path;
    vpath_ = path + ".verts.tmp";
    tpath_ = path + ".tris.tmp";
    return vown_.open(verts_, vpath_) && town_.open(tris_, tpath_);
  }

  template <class KeepFn>
  bool onTile(const Contoured& c, KeepFn keep) {
    std::vector<dualc::Vector3> pos(c.mesh->nVertices());
    for (auto v : c.mesh->vertices())
      pos[v.getIndex()] = c.geom->inputVertexPositions[v];

    std::vector<long long> local2global(c.mesh->nVertices(), -1);
    auto gid = [&](std::size_t vi) -> long long {
      if (local2global[vi] < 0) local2global[vi] = globalIdFor(pos[vi]);
      return local2global[vi];
    };

    bool any = false;
    for (const std::vector<std::size_t>& f : c.mesh->getFaceVertexList()) {
      if (f.size() < 3) continue;
      dualc::Vector3 ctr{0.0, 0.0, 0.0};
      for (std::size_t vi : f) { ctr.x += pos[vi].x; ctr.y += pos[vi].y; ctr.z += pos[vi].z; }
      const double inv = 1.0 / static_cast<double>(f.size());
      ctr.x *= inv; ctr.y *= inv; ctr.z *= inv;
      if (!keep(ctr)) continue;
      const long long g0 = gid(f[0]);
      for (std::size_t i = 1; i + 1 < f.size(); ++i) {
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "     <triangle v1=\"%lld\" v2=\"%lld\" v3=\"%lld\"/>\n",
                      g0, gid(f[i]), gid(f[i + 1]));
        tris_ << buf;
        ++triCount_;
        any = true;
      }
    }
    return any;
  }

  void abort() {
    vown_.close(verts_);
    town_.close(tris_);
    cleanup();
  }
  ~Welded3mfSink() { abort(); }

  bool finish() {
    const bool wrote = static_cast<bool>(verts_) && static_cast<bool>(tris_);
    const bool closed = vown_.close(verts_) && town_.close(tris_);
    if (!wrote || !closed) { cleanup(); return false; }

    // Assemble the single-object model part = header + verts + tris + footer.
    const std::string mpath = path_ + ".model.tmp";
    OutputFile mown;
    std::ofstream m;
    if (!mown.open(m, mpath)) { cleanup(); return false; }
    m << kModelHeader3mf
      << " <resources>\n  <object id=\"1\" type=\"model\">\n   <mesh>\n"
         "    <vertices>\n";
    bool ok = appendFileTo(m, vpath_);
    m << "    </vertices>\n    <triangles>\n";
    ok = appendFileTo(m, tpath_) && ok;
    m << "    </triangles>\n   </mesh>\n  </object>\n </resources>\n"
         " <build>\n  <item objectid=\"1\"/>\n </build>\n</model>\n";
    ok = ok && static_cast<bool>(m);
    ok = mown.close(m) && ok;
    if (!ok) { std::remove(mpath.c_str()); cleanup(); return false; }

    FILE* zf = openOutputFile(path_, true);
    if (!zf) {
      std::remove(mpath.c_str());
      cleanup();
      return false;
    }
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof zip);
    if (!mz_zip_writer_init_cfile(&zip, zf, 0)) {
      std::fclose(zf);
      std::remove(mpath.c_str());
      cleanup();
      return false;
    }
    ok = mz_zip_writer_add_mem(&zip, "[Content_Types].xml", kContentTypes3mf,
                               std::strlen(kContentTypes3mf),
                               MZ_DEFAULT_LEVEL) != MZ_FALSE &&
         mz_zip_writer_add_mem(&zip, "_rels/.rels", kRels3mf,
                               std::strlen(kRels3mf),
                               MZ_DEFAULT_LEVEL) != MZ_FALSE &&
         mz_zip_writer_add_file(&zip, "3D/3dmodel.model", mpath.c_str(),
                                nullptr, 0, MZ_DEFAULT_LEVEL) != MZ_FALSE;
    if (ok) ok = mz_zip_writer_finalize_archive(&zip) != MZ_FALSE;
    mz_zip_writer_end(&zip);
    if (std::fclose(zf) != 0) ok = false;
    std::remove(mpath.c_str());
    cleanup();
    return ok;
  }

  std::uint32_t count() const { return triCount_; }

 private:
  // True if p lies within `band` cells of an internal seam plane on any axis
  // (planes at box.min + k*ownedCells*cs, k in [1, nt-1]). Distance-based, so ULP
  // noise cannot flip the classification. Over-inclusion only wastes hash slots.
  bool onSeam(const dualc::Vector3& p) const {
    const int oc = g_.ownedCells;
    if (g_.nt <= 1 || oc <= 0) return false;
    const double band = 2.0;  // cells; >= the 2-cell tile overlap, with margin
    const double d[3] = {p.x - g_.box.min.x, p.y - g_.box.min.y, p.z - g_.box.min.z};
    const double csa[3] = {g_.cs.x, g_.cs.y, g_.cs.z};
    for (int a = 0; a < 3; ++a) {
      if (csa[a] <= 0.0) continue;
      const double inCells = d[a] / csa[a];
      const long long k = std::llround(inCells / oc);
      if (k >= 1 && k <= g_.nt - 1 &&
          std::fabs(inCells - static_cast<double>(k * oc)) < band)
        return true;
    }
    return false;
  }

  long long globalIdFor(const dualc::Vector3& p) {
    if (!onSeam(p)) return writeVertex(p);  // interior: unique to this tile
    const long long kx = keyAxis(p.x - g_.box.min.x, g_.cs.x);
    const long long ky = keyAxis(p.y - g_.box.min.y, g_.cs.y);
    const long long kz = keyAxis(p.z - g_.box.min.z, g_.cs.z);
    // Probe the 3x3x3 key neighbourhood, not just the exact bucket: a coincident
    // pair from two tiles can straddle a bucket boundary (their positions differ
    // by the tile-local rounding divergence and round to ADJACENT keys), which a
    // single-bucket lookup would miss -> crack. Distinct seam vertices are ~1e6
    // buckets apart (eps = cs*1e-6), so any +/-1 neighbour that is present is
    // necessarily the coincident vertex -- welding to it needs no distance check
    // and can never false-merge two genuinely distinct vertices.
    for (int dz = -1; dz <= 1; ++dz)
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          auto it = seam_.find(Key{kx + dx, ky + dy, kz + dz});
          if (it != seam_.end()) return it->second;
        }
    const long long id = writeVertex(p);
    seam_.emplace(Key{kx, ky, kz}, id);
    return id;
  }

  long long writeVertex(const dualc::Vector3& p) {
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "     <vertex x=\"%.9g\" y=\"%.9g\" z=\"%.9g\"/>\n",
                  p.x, p.y, p.z);
    verts_ << buf;
    return nextId_++;
  }

  static long long keyAxis(double d, double cs) {
    const double eps = (cs > 0.0 ? cs : 1.0) * 1e-6;
    return std::llround(d / eps);
  }

  void cleanup() {
    if (vpath_.empty()) return;
    std::remove(vpath_.c_str());
    std::remove(tpath_.c_str());
    std::remove((path_ + ".model.tmp").c_str());
  }

  struct Key {
    long long x, y, z;
    bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z; }
  };
  struct KeyHash {
    std::size_t operator()(const Key& k) const {
      std::size_t h = static_cast<std::size_t>(k.x) * 73856093ULL;
      h ^= static_cast<std::size_t>(k.y) * 19349663ULL;
      h ^= static_cast<std::size_t>(k.z) * 83492791ULL;
      return h;
    }
  };

  TileGrid g_;
  std::string path_, vpath_, tpath_;
  OutputFile vown_, town_;   // before their streams: destroyed after them
  std::ofstream verts_, tris_;
  std::unordered_map<Key, long long, KeyHash> seam_;
  long long nextId_ = 0;
  std::uint32_t triCount_ = 0;
};

// Resolve the finite global sampling box the tiler (and the --mem probe) works
// over. With explicit --bounds it is used verbatim; otherwise replicate the
// sampler's auto-fit padding (padBBox in src/sampler.cpp) so the tiled cell grid
// matches exactly what the monolithic path would build. Returns false (message
// printed) when the field is unbounded and no --bounds was given.
bool resolveGlobalSamplingBox(const dualc::ImplicitField& field,
                              const dualc::SamplerParams& sp, dualc::BBox& box) {
  if (sp.rootBounds.has_value()) {
    box = *sp.rootBounds;
    return true;
  }
  const dualc::BBox raw = field.bounds();
  if (raw.isInfinite() || !raw.isValid()) {
    std::cerr << "[dualc] error: this field is unbounded -- pass "
                 "--bounds x0,y0,z0,x1,y1,z1 to give the tiler an explicit "
                 "finite region.\n";
    return false;
  }
  const dualc::Vector3 ext = raw.extent();
  const double maxDim = std::max({ext.x, ext.y, ext.z, 1e-12});
  const double pad = maxDim * sp.padFraction;
  box = raw;
  box.min = box.min - dualc::Vector3{pad, pad, pad};
  box.max = box.max + dualc::Vector3{pad, pad, pad};
  return true;
}

// Format-agnostic tiling driver shared by writeFieldTiledStl / writeFieldTiled3mf.
// `label` names the format in progress/success lines. Returns a process rc:
// 0 ok, 1 unbounded field, 2 validation / writer failure, 3 cancelled. On
// every non-zero rc nothing is left at `path` (the `.part` convention above).
//
// Hooks: `cancel` is polled at the top of every tile and inside every tile's
// contour; `progress` receives Stage::Tile (tileIdx, totalTiles) on every
// tile, (0, T) before the loop and (T, T) after the file is committed. The
// per-tile contours do not forward the sink -- thousands of Sample/Contour
// restarts would be noise, not a bar. The single-pass fallback (one whole-box
// contour) forwards it, since there the engine stages are the only progress.
template <class Sink>
int forEachOwnedTileImpl(const dualc::ImplicitField& field,
                         const std::string& path,
                         const dualc::SamplerParams& sp,
                         const dualc::ContourerParams& cp, int tileDepth,
                         const char* label, Sink& sink,
                         const dualc::CancelToken* cancel,
                         dualc::ProgressSink* progress) {
  if (tileDepth < 2) {
    reportError("--tile-depth must be >= 2");
    return 2;
  }
  if (cancel) cancel->throwIfRequested("tiling");
  const int globalDepth = sp.maxDepth;
  const int tileD = tileDepth;

  dualc::BBox box;
  if (!resolveGlobalSamplingBox(field, sp, box)) return 1;

  const dualc::Vector3 ext = box.extent();
  const int nGlobal = 1 << globalDepth;     // global cells per axis
  const int tileCells = 1 << tileD;         // cells in one tile octree per axis
  const int ownedCells = tileCells - 2;     // owned (non-ghost) cells per axis
  const dualc::Vector3 cs{ext.x / nGlobal, ext.y / nGlobal, ext.z / nGlobal};
  // nt tiles per axis (1 when the tile spans the whole grid -> single pass).
  const int nt =
      (ownedCells >= nGlobal) ? 1 : (nGlobal + ownedCells - 1) / ownedCells;

  AtomicOutput out(path);
  if (!sink.open(out.tmp())) {
    reportError("cannot open '" + out.tmp() + "' for writing: " + errnoText());
    return 2;
  }
  // Declared AFTER `out` so it is destroyed FIRST: on any exit that is not a
  // committed success the sink closes its streams before AtomicOutput tries
  // to remove the `.part` (Windows cannot unlink an open file). Disarmed on
  // the success path.
  struct SinkAbortGuard {
    Sink& s;
    bool armed = true;
    ~SinkAbortGuard() { if (armed) s.abort(); }
  } guard{sink};
  sink.setGrid({box, cs, ownedCells, nt});

  // When the tile is as big as (or bigger than) the whole grid there is no RAM
  // benefit and the overlap scheme would contour ~the full part several times
  // over. Fall back to a single streamed pass: one contour over the whole box,
  // every face kept.
  if (ownedCells >= nGlobal) {
    std::cout << "[dualc] tile-depth >= depth: no tiling, single streamed pass\n";
    dualc::SamplerParams sp2 = sp;
    sp2.rootBounds = box;
    Contoured c;
    // abort(), not finish(): a failed contour must not finalize a file.
    if (!contourOrHint(field, sp2, cp, c, false, cancel, progress)) {
      sink.abort();
      return 1;
    }
    if (!isEmptyPlaceholder(c))
      sink.onTile(c, [](const dualc::Vector3&) { return true; });
    if (!sink.finish()) {
      reportError(std::string("failed finalizing ") + label + " '" + path + "'");
      return 2;
    }
    if (!out.commit()) return 2;
    guard.armed = false;
    if (sink.count() == 0)
      std::cerr << "[dualc] warning: the field produced an empty mesh -- no "
                   "surface crossed the sampled region.\n";
    std::cout << "[dualc] wrote " << path << " (" << sink.count()
              << " triangles)\n";
    return 0;
  }

  // Tiles per axis (nt, computed above) cover all nGlobal owned cells; the last
  // tile overhangs into empty space (the octree prunes it). ownedCells >= 2.
  const long long totalTiles = static_cast<long long>(nt) * nt * nt;
  std::cout << "[dualc] tiled streaming " << label << ": depth " << globalDepth
            << ", tile-depth " << tileD << " => " << nt << "^3 = " << totalTiles
            << " tiles (" << ownedCells << " owned cells/axis + 1 ghost ring)\n";

  const std::size_t totalT = static_cast<std::size_t>(totalTiles);
  if (progress) progress->report(dualc::Stage::Tile, 0, totalT);
  long long tileIdx = 0, nonEmpty = 0;
  for (int iz = 0; iz < nt; ++iz)
    for (int iy = 0; iy < nt; ++iy)
      for (int ix = 0; ix < nt; ++ix) {
        ++tileIdx;
        if (cancel) cancel->throwIfRequested("tiling");
        // Report BEFORE the tile is contoured: the progress the host sees is
        // "tiles started", so the count moves even while the field is
        // sparse and most tiles are skipped below. (tileIdx, T) is reported
        // once more after the last tile completes.
        if (progress && tileIdx < totalTiles)
          progress->report(dualc::Stage::Tile,
                           static_cast<std::size_t>(tileIdx - 1), totalT);
        const int sx = ix * ownedCells, sy = iy * ownedCells, sz = iz * ownedCells;
        // Tile octree box = owned cell range grown by 1 ghost cell each side, so
        // it is exactly tileCells == 2^tileD cells of size cs and stays aligned
        // to the global grid.
        dualc::BBox tb;
        tb.min = dualc::Vector3{box.min.x + (sx - 1) * cs.x,
                               box.min.y + (sy - 1) * cs.y,
                               box.min.z + (sz - 1) * cs.z};
        tb.max = dualc::Vector3{tb.min.x + tileCells * cs.x,
                               tb.min.y + tileCells * cs.y,
                               tb.min.z + tileCells * cs.z};

        // Skip tiles the surface cannot touch -- both a big speed-up on sparse
        // parts and the cheap way to avoid contouring empty regions.
        if (!field.cellOverlaps(tb)) continue;

        dualc::SamplerParams sp2 = sp;
        sp2.rootBounds = tb;
        sp2.maxDepth = tileD;

        Contoured c;
        if (!contourOrHint(field, sp2, cp, c, false, cancel, nullptr)) {
          sink.abort();  // not finish(): a failed tile must not finalize a file
          return 1;      // unexpected: box is finite, but fail cleanly
        }
        if (isEmptyPlaceholder(c)) continue;  // conservatively-true but empty

        // Owned-face filter. Quantize each face centroid to the GLOBAL cell grid
        // and keep it iff that cell is in this tile's owned range. Integer
        // compare is robust to ULP noise at the seam, so every boundary quad is
        // emitted by exactly one tile (the union of owned ranges partitions
        // [0, nGlobal) with no gaps or overlaps -- first/last tiles clamp to the
        // global domain).
        const bool tileHadFaces = sink.onTile(
            c, [&](const dualc::Vector3& ctr) {
              const int cx =
                  static_cast<int>(std::floor((ctr.x - box.min.x) / cs.x));
              const int cy =
                  static_cast<int>(std::floor((ctr.y - box.min.y) / cs.y));
              const int cz =
                  static_cast<int>(std::floor((ctr.z - box.min.z) / cs.z));
              const bool okx = cx >= (ix == 0 ? 0 : sx) &&
                               cx < (ix == nt - 1 ? nGlobal : sx + ownedCells);
              const bool oky = cy >= (iy == 0 ? 0 : sy) &&
                               cy < (iy == nt - 1 ? nGlobal : sy + ownedCells);
              const bool okz = cz >= (iz == 0 ? 0 : sz) &&
                               cz < (iz == nt - 1 ? nGlobal : sz + ownedCells);
              return okx && oky && okz;
            });
        if (tileHadFaces) ++nonEmpty;
        if (totalTiles <= 64 || tileIdx % 16 == 0 || tileIdx == totalTiles)
          std::cout << "[dualc]   tile " << tileIdx << "/" << totalTiles << " ("
                    << sink.count() << " tris so far)\n";
        // c is freed here -> peak RAM is bounded by one tile.
      }

  if (!sink.finish()) {
    reportError(std::string("failed finalizing ") + label + " '" + path + "'");
    return 2;
  }
  if (!out.commit()) return 2;
  guard.armed = false;
  if (progress) progress->report(dualc::Stage::Tile, totalT, totalT);
  if (sink.count() == 0)
    std::cerr << "[dualc] warning: the field produced an empty mesh -- no "
                 "surface crossed the sampled region.\n";
  std::cout << "[dualc] wrote " << path << " (" << sink.count() << " triangles, "
            << nonEmpty << "/" << totalTiles << " tiles non-empty)\n";
  return 0;
}

// The public shape of the driver: every non-success exit -- a cancel, a
// failed tile, a stray exception -- goes through sink.abort() before the
// AtomicOutput inside the Impl has removed the `.part`, so nothing is left
// on disk. Returns 3 on a cancel.
template <class Sink>
int forEachOwnedTile(const dualc::ImplicitField& field, const std::string& path,
                     const dualc::SamplerParams& sp,
                     const dualc::ContourerParams& cp, int tileDepth,
                     const char* label, Sink& sink,
                     const dualc::CancelToken* cancel,
                     dualc::ProgressSink* progress) {
  try {
    return forEachOwnedTileImpl(field, path, sp, cp, tileDepth, label, sink,
                                cancel, progress);
  } catch (const dualc::Cancelled& e) {
    sink.abort();
    std::cerr << "[dualc] " << e.what() << ": nothing written to '" << path
              << "'\n";
    return 3;
  } catch (...) {
    sink.abort();
    throw;
  }
}

// --- `--mem BUDGET` tile-depth picker ---------------------------------------
//
// Peak RAM of a tiled export is bounded by the single BUSIEST tile, so the
// estimate cannot be an average -- the dense interior tiles of a boolean-carved
// lattice run well above the mean while empty tiles are skipped entirely. We get
// the spatial distribution directly from one cheap coarse whole-field contour:
// bucket its face centroids into each candidate tile grid, take the max bucket,
// and extrapolate to the target depth. The estimate is deliberately CONSERVATIVE
// (over-estimating RAM only picks a smaller D -> more tiles, slower, but safe;
// under-estimating would OOM, the one failure --mem exists to prevent), and it
// is always reported so the user can override with an explicit --tile-depth.

// Surface faces in a fixed region grow ~4x/level for a thin sheet, up to ~8x for
// a space-filling lattice (the measured gyroid was ~7x). We use the space-filling
// bound: it over-estimates a thin surface (-> smaller D -> safe), and thin
// surfaces rarely need --mem anyway.
constexpr double kRamGrowthPerLevel = 8.0;
// Peak bytes charged per surface face at the busiest tile. A geometry-central
// SurfaceMesh face is only ~200-300 B, but the TRANSIENT contour peak -- the
// octree with its internal nodes, the mesh under construction, and the QEF/SVD +
// normal scratch -- is an order of magnitude larger per output face. Calibrated
// by EXECUTION against the Phase-0 anchor (80^3 box, gyroid l=5, depth 7, D=5 tile
// measured at ~62 MB peak): at 3 KiB/face this model reports ~99 MB there, i.e.
// ~1.6x the measured peak -- deliberately conservative so a budget that admits D=5
// is comfortably above the true requirement. (1 KiB under-reported it at ~33 MB,
// which would have silently OOM'd a tight budget -- the one failure to avoid.)
constexpr double kBytesPerSurfaceFace = 3072.0;

// Chosen tile-depth for `budgetBytes`, or -1 if the field is unbounded (message
// printed). Logs the probe, the chosen D, and its estimate. Range: [2, depth-1].
int pickTileDepthForBudget(const dualc::ImplicitField& field,
                           const dualc::SamplerParams& sp,
                           const dualc::ContourerParams& cp,
                           std::uint64_t budgetBytes) {
  const int depth = sp.maxDepth;
  const int hiD = std::max(2, depth - 1);  // largest tile-depth worth trying
  if (depth < 3) {
    std::cout << "[dualc] --mem: --depth " << depth
              << " is too small to tile; using tile-depth 2\n";
    return 2;
  }

  dualc::BBox box;
  if (!resolveGlobalSamplingBox(field, sp, box)) return -1;
  const dualc::Vector3 ext = box.extent();
  const int nGlobal = 1 << depth;

  // Coarse probe: contour the whole field at a cheap depth and cache the face
  // centroids. Bump the probe depth if a genuinely non-empty part shows no
  // crossings at first (a thin wall in a big box can miss every coarse cell).
  std::vector<dualc::Vector3> centroids;
  int probeD = std::min(depth - 1, 5);
  int usedD = probeD;
  for (; probeD <= depth - 1; ++probeD) {
    dualc::SamplerParams spp = sp;
    spp.rootBounds = box;
    spp.maxDepth = probeD;
    Contoured c;
    if (!contourOrHint(field, spp, cp, c)) return -1;  // box is set: unreachable
    if (isEmptyPlaceholder(c)) continue;
    std::vector<dualc::Vector3> pos(c.mesh->nVertices());
    for (auto v : c.mesh->vertices())
      pos[v.getIndex()] = c.geom->inputVertexPositions[v];
    for (const std::vector<std::size_t>& f : c.mesh->getFaceVertexList()) {
      if (f.size() < 3) continue;
      dualc::Vector3 ctr{0.0, 0.0, 0.0};
      for (std::size_t vi : f) {
        ctr.x += pos[vi].x; ctr.y += pos[vi].y; ctr.z += pos[vi].z;
      }
      const double inv = 1.0 / static_cast<double>(f.size());
      centroids.push_back({ctr.x * inv, ctr.y * inv, ctr.z * inv});
    }
    usedD = probeD;
    if (!centroids.empty()) break;
  }
  if (centroids.empty()) {
    std::cout << "[dualc] --mem: probe found no surface; using tile-depth " << hiD
              << " (output will be empty)\n";
    return hiD;
  }

  const double growth = std::pow(kRamGrowthPerLevel, depth - usedD);
  std::cout << "[dualc] --mem: budget " << (budgetBytes / (1024 * 1024))
            << " MiB; probe at depth " << usedD << " -> " << centroids.size()
            << " faces\n";

  // Largest D whose busiest-tile estimate fits. Bucket the cached coarse
  // centroids into this D's owned-tile grid; the peak bucket, grown to the target
  // depth, is the busiest tile's face estimate.
  int chosen = 2;
  double chosenBytes = 0.0;
  for (int D = hiD; D >= 2; --D) {
    const int ownedCells = (1 << D) - 2;
    const int nt =
        (ownedCells >= nGlobal) ? 1 : (nGlobal + ownedCells - 1) / ownedCells;
    const dualc::Vector3 tw{ext.x * ownedCells / nGlobal,
                            ext.y * ownedCells / nGlobal,
                            ext.z * ownedCells / nGlobal};
    std::unordered_map<long long, long long> buckets;
    long long maxBucket = 0;
    for (const dualc::Vector3& ctr : centroids) {
      auto axis = [nt](double c, double lo, double w) {
        if (w <= 0.0) return 0;
        int i = static_cast<int>(std::floor((c - lo) / w));
        return i < 0 ? 0 : (i >= nt ? nt - 1 : i);
      };
      const int tx = axis(ctr.x, box.min.x, tw.x);
      const int ty = axis(ctr.y, box.min.y, tw.y);
      const int tz = axis(ctr.z, box.min.z, tw.z);
      const long long key = (static_cast<long long>(tx) * nt + ty) * nt + tz;
      maxBucket = std::max(maxBucket, ++buckets[key]);
    }
    const double estBytes =
        static_cast<double>(maxBucket) * growth * kBytesPerSurfaceFace;
    // Record the smallest D as the floor; keep walking up while it fits so the
    // loop's first (largest) fit wins.
    if (estBytes <= static_cast<double>(budgetBytes)) {
      chosen = D;
      chosenBytes = estBytes;
      break;
    }
    chosen = D;          // nothing fits yet; D is the current floor candidate
    chosenBytes = estBytes;
  }

  const double mib = chosenBytes / (1024.0 * 1024.0);
  if (chosen == 2 && chosenBytes > static_cast<double>(budgetBytes)) {
    std::cout << "[dualc] --mem: nothing fits the budget; using the smallest "
                 "tile-depth 2 (est. peak ~" << static_cast<long long>(mib)
              << " MiB/tile -- still over budget)\n";
  } else {
    std::cout << "[dualc] --mem: chose --tile-depth " << chosen << " (est. peak ~"
              << static_cast<long long>(mib) << " MiB/tile)\n";
  }
  return chosen;
}

} // namespace

namespace {

// The body of writeField. The contour runs before any file is opened, so a
// cancel during Sample/Contour never touches the disk; the write itself goes
// to the AtomicOutput temp and is committed last. The Write stage is reported
// as (0, 1) before the writer and (1, 1) after the commit -- there is no
// checkpoint inside the monolithic writers (roadmap 14 #48, D-45).
int writeFieldImpl(const dualc::ImplicitField& field, const std::string& path,
                   const dualc::SamplerParams& sp,
                   const dualc::ContourerParams& cp, const DecimateOpts& dec,
                   dualc::Diagnostics* diag, const dualc::CancelToken* cancel,
                   dualc::ProgressSink* progress) {
  if (cancel) cancel->throwIfRequested("sampling");
  Contoured c;
  if (!contourOrHint(field, sp, cp, c, /*collectDiag=*/true, cancel, progress))
    return 1;
  warnDiagnostics(c);
  if (diag) *diag = c.diag;
  if (cancel) cancel->throwIfRequested("writing");
  if (progress) progress->report(dualc::Stage::Write, 0, 1);
  AtomicOutput out(path);
  if (dec.mode == DecimateOpts::Mode::None) {
    const int rc = dispatchWrite(c, path, out.tmp());
    if (rc != 0) return rc;
    if (!out.commit()) return 2;
    if (progress) progress->report(dualc::Stage::Write, 1, 1);
    std::cout << "[dualc] wrote " << path << " (" << c.mesh->nVertices()
              << " verts, " << c.mesh->nFaces() << " faces)\n";
    return 0;
  }

  // Approach A: contour whole -> decimate whole -> write. The decimated result
  // is a plain TriMesh (no SurfaceMesh), so every format goes through the
  // TriMesh writers; .obj gets recomputed angle-weighted normals.
  const std::string ext = lowerExt(path);
  const TriMesh full = toTriMesh(c);
  const std::size_t beforeFaces = full.tris.size();
  float achievedError = 0.0f;
  const TriMesh m = decimate(full, dec, achievedError);
  int rc;
  if (ext == ".obj") {
    rc = writeObjFromTriMesh(m, out.tmp());
  } else if (ext == ".stl") {
    rc = writeStl(m, out.tmp());
  } else if (ext == ".3mf") {
    rc = write3mf(m, out.tmp());
  } else {
    reportError("unknown output extension '" + ext + "' (use .obj, .stl or .3mf)");
    return 2;
  }
  if (rc != 0) return rc;
  if (!out.commit()) return 2;
  if (progress) progress->report(dualc::Stage::Write, 1, 1);
  std::cout << "[dualc] wrote " << path << " (" << m.pos.size() << " verts, "
            << m.tris.size() << " faces; decimated from " << beforeFaces
            << " faces, error " << achievedError << " mm)\n";
  return 0;
}

} // namespace

const std::string& lastError() { return g_lastError; }

int writeField(const dualc::ImplicitField& field, const std::string& path,
               const dualc::SamplerParams& sp, const dualc::ContourerParams& cp,
               const DecimateOpts& dec, dualc::Diagnostics* diag,
               const dualc::CancelToken* cancel,
               dualc::ProgressSink* progress) {
  g_lastError.clear();
  try {
    return writeFieldImpl(field, path, sp, cp, dec, diag, cancel, progress);
  } catch (const dualc::Cancelled& e) {
    std::cerr << "[dualc] " << e.what() << ": nothing written to '" << path
              << "'\n";
    return 3;
  }
}

int writeFieldTiledStl(const dualc::ImplicitField& field, const std::string& path,
                       const dualc::SamplerParams& sp,
                       const dualc::ContourerParams& cp, int tileDepth,
                       const dualc::CancelToken* cancel,
                       dualc::ProgressSink* progress) {
  g_lastError.clear();
  if (lowerExt(path) != ".stl") {
    reportError("tiled/streaming STL export supports only .stl output (got '" +
                lowerExt(path) + "')");
    return 2;
  }
  TiledStlSink sink;
  return forEachOwnedTile(field, path, sp, cp, tileDepth, "STL", sink, cancel,
                          progress);
}

int writeFieldTiled3mf(const dualc::ImplicitField& field, const std::string& path,
                       const dualc::SamplerParams& sp,
                       const dualc::ContourerParams& cp, int tileDepth,
                       bool weld, const dualc::CancelToken* cancel,
                       dualc::ProgressSink* progress) {
  g_lastError.clear();
  if (lowerExt(path) != ".3mf") {
    reportError("tiled/streaming 3MF export supports only .3mf output (got '" +
                lowerExt(path) + "')");
    return 2;
  }
  if (weld) {
    Welded3mfSink sink;
    return forEachOwnedTile(field, path, sp, cp, tileDepth, "3MF(welded)", sink,
                            cancel, progress);
  }
  Tiled3mfSink sink;
  return forEachOwnedTile(field, path, sp, cp, tileDepth, "3MF", sink, cancel,
                          progress);
}

bool parseMemBudget(const std::string& s, std::uint64_t& bytesOut) {
  std::string t = s;
  // Trim surrounding whitespace.
  while (!t.empty() && std::isspace(static_cast<unsigned char>(t.front())))
    t.erase(t.begin());
  while (!t.empty() && std::isspace(static_cast<unsigned char>(t.back())))
    t.pop_back();
  if (t.empty()) return false;

  // Strip an optional trailing 'B'/'b', then an optional 'i'/'I', so both the
  // bare ("4G") and full ("4GiB") binary spellings work. What remains ends in the
  // unit letter (G/M/K) or a digit (plain bytes).
  auto lc = [](char c) { return std::tolower(static_cast<unsigned char>(c)); };
  if (!t.empty() && lc(t.back()) == 'b') t.pop_back();
  if (!t.empty() && lc(t.back()) == 'i') t.pop_back();
  if (t.empty()) return false;

  std::uint64_t mult = 1;
  if (!std::isdigit(static_cast<unsigned char>(t.back()))) {
    switch (lc(t.back())) {
      case 'g': mult = 1024ull * 1024 * 1024; break;
      case 'm': mult = 1024ull * 1024; break;
      case 'k': mult = 1024ull; break;
      default: return false;
    }
    t.pop_back();
  }
  if (t.empty()) return false;

  // Accept a fractional magnitude (e.g. "1.5G").
  char* end = nullptr;
  const double mag = std::strtod(t.c_str(), &end);
  if (end == t.c_str() || *end != '\0' || !(mag > 0.0)) return false;
  bytesOut = static_cast<std::uint64_t>(mag * static_cast<double>(mult));
  return bytesOut > 0;
}

int chooseTileDepthForBudget(const dualc::ImplicitField& field,
                             const dualc::SamplerParams& sp,
                             const dualc::ContourerParams& cp,
                             std::uint64_t budgetBytes) {
  return pickTileDepthForBudget(field, sp, cp, budgetBytes);
}

} // namespace dce
