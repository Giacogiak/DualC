#include "example_common.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"
#include "geometrycentral/utilities/vector3.h"

#include "gpu_preference.h"
#include "raymarch_gl.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// dualc_raymarch -- a standalone GPU raymarch viewer for a TPMS-lattice-in-mesh
// implicit field.
//
// Where dualc_lattice CONTOURS the whole field into a (potentially enormous)
// triangle mesh -- which OOMs the halfedge mesh on a dense lattice in a large
// volume -- dualc_raymarch never materialises a mesh at all. It sphere-traces
// the *analytic* TPMS field once per screen pixel, so the cost is O(pixels) and
// is **independent of lattice density**: shrink --wavelength toward zero and the
// frame rate barely moves, where the mesher would have run out of RAM long ago.
//
// The lattice (TPMS + optional |F|-T offset + optional 1/|grad F| normalisation)
// is evaluated directly in the fragment shader -- it is cheap and needs no grid.
// The only thing baked is the *mesh clip*: a low-frequency narrow-band SDF of
// the input mesh, uploaded as a small GL_R32F 3D texture and combined in the
// shader as max(latticeField, meshSDF). With no input mesh, the bare lattice is
// shown inside --bounds.
//
// Controls (printed on start): orbit with the left mouse button, dolly with the
// scroll wheel; [ / ] change wavelength, - / = change offset, t cycles the TPMS
// family, n toggles thickness-normalisation, r resets the view, Esc quits.
//
// This is an opt-in build (DUALC_BUILD_RAYMARCH_VIEWER): it reuses the GLFW +
// glad that the sibling polyscope checkout already vendors, and adds no
// dependency to libdualc. Like dualc_view it needs a GL window, so it has no
// CTest.

using dualc::Vector3;

namespace {

// ===========================================================================
// Embedded GLSL (GL 3.3 core). The fragment shader's TPMS formulas must stay
// bit-faithful to src/implicit/primitives_tpms.cpp (k = 2*pi/wavelength, phase
// shifted by uCenter), so the raymarched surface matches what dualc_lattice
// would manufacture and what dualc_slice cross-sections.
// ===========================================================================

const char* kVertSrc = R"GLSL(
#version 330 core
out vec2 vUV;
void main() {
  // Full-screen triangle (covers NDC with overscan); vUV in [-1,1] visible.
  vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                (gl_VertexID == 2) ? 3.0 : -1.0);
  vUV = p;
  gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

const char* kFragSrc = R"GLSL(
#version 330 core
in  vec2 vUV;
out vec4 fragColor;

uniform vec3  uCamPos, uCamForward, uCamRight, uCamUp;
uniform float uTanHalfFov, uAspect;

uniform vec3  uBoundsMin, uBoundsMax;

uniform int   uType;        // 0 gyroid .. 5 neovius (matches tpmsKinds())
uniform float uWavelength;  // world units
uniform vec3  uCenter;      // lattice phase shift
uniform float uOffset;      // |F|-T shell when > 0
uniform int   uNormalize;   // 1 -> divide F by |grad F| before the offset

uniform int       uHasMesh;
uniform sampler3D uMeshTex; // narrow-band mesh SDF (R32F)
uniform vec3      uTexMin, uTexScale, uTexDim;

uniform vec3  uLightDir;
uniform float uStepMin, uStepMax, uHmem;

// Viewport-only section planes (up to 3 axis-aligned half-spaces). Each uClip[i]
// is (n.xyz, d); kept region is dot(n,p)+d <= 0. Intersect via max(field, h_i).
// uClipMask==0 => clippedField == sceneField (no visible change). Preview only.
uniform vec4 uClip[3];
uniform int  uClipMask;

const float PI = 3.14159265358979;

float tpms(vec3 p) {
  vec3 q = (p - uCenter) * (2.0 * PI / uWavelength);
  float kx = q.x, ky = q.y, kz = q.z;
  float sx = sin(kx), cx = cos(kx);
  float sy = sin(ky), cy = cos(ky);
  float sz = sin(kz), cz = cos(kz);
  if (uType == 0) {                       // gyroid (Schoen G)
    return sx*cy + sy*cz + sz*cx;
  } else if (uType == 1) {                // schwarz-p
    return cx + cy + cz;
  } else if (uType == 2) {                // diamond (Schwarz D)
    return sx*sy*sz + sx*cy*cz + cx*sy*cz + cx*cy*sz;
  } else if (uType == 3) {                // fischer-koch S
    return cos(2.0*kx)*sy*cz + cx*cos(2.0*ky)*sz + sx*cy*cos(2.0*kz);
  } else if (uType == 4) {                // lidinoid
    float s2x = sin(2.0*kx), c2x = cos(2.0*kx);
    float s2y = sin(2.0*ky), c2y = cos(2.0*ky);
    float s2z = sin(2.0*kz), c2z = cos(2.0*kz);
    float t1 = 0.5 * (s2x*cy*sz + s2y*cz*sx + s2z*cx*sy);
    float t2 = 0.5 * (c2x*c2y + c2y*c2z + c2z*c2x);
    return t1 - t2 + 0.15;
  } else {                                // neovius
    return 3.0*(cx + cy + cz) + 4.0*cx*cy*cz;
  }
}

float tpmsGradMag(vec3 p) {
  float h = uHmem;
  float dx = tpms(p + vec3(h,0,0)) - tpms(p - vec3(h,0,0));
  float dy = tpms(p + vec3(0,h,0)) - tpms(p - vec3(0,h,0));
  float dz = tpms(p + vec3(0,0,h)) - tpms(p - vec3(0,0,h));
  return length(vec3(dx,dy,dz)) / (2.0*h);
}

float latticeField(vec3 p) {
  float f = tpms(p);
  if (uNormalize == 1) f = f / max(tpmsGradMag(p), 1e-6);  // ~ normalizedOf
  if (uOffset > 0.0)   f = abs(f) - uOffset;               // ~ onionOf
  return f;
}

float meshField(vec3 p) {
  vec3 g  = (p - uTexMin) * uTexScale;   // -> [0, res-1]
  vec3 tc = (g + 0.5) / uTexDim;         // texel-centre aligned to the bake
  return texture(uMeshTex, tc).r;
}

float sceneField(vec3 p) {
  float f = latticeField(p);
  if (uHasMesh == 1) f = max(f, meshField(p));  // intersect with mesh interior
  return f;
}

// sceneField intersected with the active section half-spaces (visualization).
float clippedField(vec3 p) {
  float d = sceneField(p);
  for (int i = 0; i < 3; ++i)
    if ((uClipMask & (1 << i)) != 0)
      d = max(d, dot(uClip[i].xyz, p) + uClip[i].w);
  return d;
}

// Central-difference gradient of the clipped field (un-normalised; /(2h) folded
// out where only the direction or a ratio is used).
vec3 sceneGrad(vec3 p) {
  float h = uHmem;
  return vec3(
    clippedField(p + vec3(h,0,0)) - clippedField(p - vec3(h,0,0)),
    clippedField(p + vec3(0,h,0)) - clippedField(p - vec3(0,h,0)),
    clippedField(p + vec3(0,0,h)) - clippedField(p - vec3(0,0,h)));
}

bool intersectBox(vec3 ro, vec3 rd, out float t0, out float t1) {
  vec3 inv  = 1.0 / rd;
  vec3 a    = (uBoundsMin - ro) * inv;
  vec3 b    = (uBoundsMax - ro) * inv;
  vec3 tmin = min(a, b), tmax = max(a, b);
  t0 = max(max(tmin.x, tmin.y), tmin.z);
  t1 = min(min(tmax.x, tmax.y), tmax.z);
  return t1 >= max(t0, 0.0);
}

vec3 background(vec2 uv) {
  float v = 0.5 * (uv.y + 1.0);
  return mix(vec3(0.10, 0.11, 0.13), vec3(0.17, 0.19, 0.23), v);
}

void main() {
  vec3 ro = uCamPos;
  vec3 rd = normalize(uCamForward
                    + vUV.x * uAspect * uTanHalfFov * uCamRight
                    + vUV.y *           uTanHalfFov * uCamUp);

  float t0, t1;
  if (!intersectBox(ro, rd, t0, t1)) { fragColor = vec4(background(vUV), 1.0); return; }

  // Sphere-trace from box entry, looking for the OUTSIDE->INSIDE transition
  // (sceneField crossing from + to <= 0) -- the visible front surface. The step
  // is the first-order distance estimate |F|/|grad F|, capped so we never skip a
  // wall; on a bracketed crossing we BISECT to the exact zero. Detecting the
  // crossing (not a distance threshold) gives a precise, per-pixel-consistent
  // hit -- without it the variable stopping offset showed up as spiky "snag"
  // artifacts and a haze of spurious specks.
  float t = max(t0, 0.0) + uStepMin;
  float f = clippedField(ro + t * rd);
  bool  hit = false;
  vec3  hp  = ro + t * rd;
  if (f <= 0.0) {
    hit = true;                              // box entry already inside material
  } else {
    const int MAX_STEPS = 384;
    for (int i = 0; i < MAX_STEPS; ++i) {
      float gm = max(length(sceneGrad(ro + t * rd)) / (2.0 * uHmem), 1e-5);
      float tN = t + clamp(0.8 * f / gm, uStepMin, uStepMax);
      if (tN > t1) break;
      float fN = clippedField(ro + tN * rd);
      if (fN <= 0.0) {                        // bracketed [t, tN] -> bisect
        float ta = t, tb = tN;
        for (int b = 0; b < 20; ++b) {
          float tm = 0.5 * (ta + tb);
          if (clippedField(ro + tm * rd) > 0.0) ta = tm; else tb = tm;
        }
        hp = ro + tb * rd; hit = true; break;
      }
      t = tN; f = fN;
    }
  }

  if (!hit) { fragColor = vec4(background(vUV), 1.0); return; }

  vec3 n = normalize(sceneGrad(hp));
  if (dot(n, rd) > 0.0) n = -n;           // face the camera
  float key  = clamp(dot(n,  uLightDir), 0.0, 1.0);
  float fill = clamp(dot(n, -uLightDir), 0.0, 1.0);
  vec3  base = vec3(0.80, 0.82, 0.86);
  // Tint the section cap (hit on an active plane AND interior body).
  float capEps = 1.5 * uStepMin;
  for (int i = 0; i < 3; ++i)
    if ((uClipMask & (1 << i)) != 0 &&
        abs(dot(uClip[i].xyz, hp) + uClip[i].w) < capEps &&
        sceneField(hp) < -capEps)
      base = vec3(0.86, 0.74, 0.58);
  vec3  col  = base * (0.16 + 0.84 * key) + vec3(0.10) * fill;
  fragColor  = vec4(pow(col, vec3(1.0/2.2)), 1.0);   // gamma
}
)GLSL";

// ===========================================================================
// Tiny geometry helpers (shared shape with dualc_slice; kept local).
// ===========================================================================

dualc::BBox meshAABB(geometrycentral::surface::SurfaceMesh& mesh,
                     geometrycentral::surface::VertexPositionGeometry& geom) {
  dualc::BBox b;
  bool first = true;
  for (auto v : mesh.vertices()) {
    const Vector3 p = geom.inputVertexPositions[v];
    if (first) { b.min = b.max = p; first = false; }
    else {
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
  return dualc::BBox{b.min - e, b.max + e};
}

double comp(const Vector3& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }

int tpmsIndex(const std::string& name) {
  const auto& kinds = dce::tpmsKinds();
  for (std::size_t i = 0; i < kinds.size(); ++i)
    if (kinds[i].name == name) return static_cast<int>(i);
  return -1;
}

// ===========================================================================
// GL plumbing is shared with the other viewers via raymarch_gl.h
// (dce::gl::initWindow / linkProgram / setU* / snapshotPng).
// ===========================================================================

using dce::gl::setU1f;
using dce::gl::setU1i;
using dce::gl::setU3;
using dce::gl::setU3f;
using dce::gl::setU4v;

// ===========================================================================
// Interaction state (orbit camera + live field params), reached from the GLFW
// callbacks via the window user pointer.
// ===========================================================================

struct State {
  // Orbit camera.
  Vector3 target{0, 0, 0};
  double  yaw   = 0.7;     // radians
  double  pitch = 0.5;
  double  dist  = 3.0;
  double  dist0 = 3.0;     // for reset
  // Mouse drag.
  bool    dragging = false;
  double  lastX = 0, lastY = 0;
  // Live field params.
  int     type      = 0;
  double  wavelength = 0.25;
  double  offset     = 0.0;
  bool    normalize  = false;
  // Viewport-only section planes (one per axis). pos01 is the cut position
  // normalized in [0,1] along the bounds; flip swaps the kept side; active is the
  // axis ;/: slide acts on.
  bool    clipOn[3]    = {false, false, false};
  float   clipPos01[3] = {0.5f, 0.5f, 0.5f};
  bool    clipFlip[3]  = {false, false, false};
  int     clipActive   = 0;
};

void onMouseButton(GLFWwindow* w, int button, int action, int /*mods*/) {
  auto* s = static_cast<State*>(glfwGetWindowUserPointer(w));
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    s->dragging = (action == GLFW_PRESS);
    glfwGetCursorPos(w, &s->lastX, &s->lastY);
  }
}

void onCursor(GLFWwindow* w, double x, double y) {
  auto* s = static_cast<State*>(glfwGetWindowUserPointer(w));
  if (!s->dragging) return;
  const double dx = x - s->lastX, dy = y - s->lastY;
  s->lastX = x; s->lastY = y;
  s->yaw   -= dx * 0.008;
  s->pitch += dy * 0.008;
  s->pitch  = std::clamp(s->pitch, -1.5, 1.5);  // avoid the poles
}

void onScroll(GLFWwindow* w, double /*dx*/, double dy) {
  auto* s = static_cast<State*>(glfwGetWindowUserPointer(w));
  s->dist *= std::pow(0.9, dy);
  s->dist  = std::max(s->dist, 1e-4);
}

void onKey(GLFWwindow* w, int key, int scancode, int action, int /*mods*/) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
  auto* s = static_cast<State*>(glfwGetWindowUserPointer(w));
  auto toggleClip = [&](int axis) {
    s->clipOn[axis] = !s->clipOn[axis];
    s->clipActive = axis;
    std::cout << "[dualc_raymarch] section " << "xyz"[axis] << " "
              << (s->clipOn[axis] ? "on" : "off") << "\n";
  };
  auto slideClip = [&](float d) {
    const int a = s->clipActive;
    if (s->clipOn[a])
      s->clipPos01[a] = std::clamp(s->clipPos01[a] + d, 0.0f, 1.0f);
  };
  // GLFW key codes are physical US positions, so the LABELED x/y/z/f keys move on
  // AZERTY/QWERTZ. Match the printed character (glfwGetKeyName); slide on arrows.
  const char* kn = glfwGetKeyName(key, scancode);
  const char ch = (kn && kn[0] && kn[1] == '\0') ? kn[0] : '\0';
  switch (ch) {
    case 'x': toggleClip(0); return;
    case 'y': toggleClip(1); return;
    case 'z': toggleClip(2); return;
    case 'f': s->clipFlip[s->clipActive] = !s->clipFlip[s->clipActive]; return;
    default: break;
  }
  switch (key) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); break;
    // Wavelength: [ / ] are physical-US (AltGr-only on AZERTY); down/up arrows are
    // the layout-independent path, brackets kept as US-keyboard alternates.
    case GLFW_KEY_LEFT_BRACKET:
    case GLFW_KEY_DOWN: s->wavelength *= 0.8;  break;   // finer cells
    case GLFW_KEY_RIGHT_BRACKET:
    case GLFW_KEY_UP:   s->wavelength *= 1.25; break;   // coarser cells
    // Offset (thick-wall shell): - / = (US) or PageDown / PageUp (any layout).
    case GLFW_KEY_MINUS:
    case GLFW_KEY_PAGE_DOWN:
      s->offset = std::max(0.0, s->offset - 0.05 * s->wavelength); break;
    case GLFW_KEY_EQUAL:
    case GLFW_KEY_PAGE_UP:
      s->offset += 0.05 * s->wavelength; break;
    case GLFW_KEY_T: s->type = (s->type + 1) % 6; break;
    case GLFW_KEY_N: s->normalize = !s->normalize; break;
    case GLFW_KEY_LEFT:  slideClip(-0.02f); break;   // slide active section -
    case GLFW_KEY_RIGHT: slideClip(0.02f);  break;   // slide active section +
    case GLFW_KEY_0:
      s->clipOn[0] = s->clipOn[1] = s->clipOn[2] = false; break;
    case GLFW_KEY_R: s->yaw = 0.7; s->pitch = 0.5; s->dist = s->dist0; break;
    default: break;
  }
}

void printControls() {
  std::cout <<
    "[dualc_raymarch] controls:\n"
    "  left-drag    orbit\n"
    "  scroll       dolly (zoom)\n"
    "  down / up    wavelength x0.8 / x1.25 (finer / coarser cells); or [ / ]\n"
    "  PgDn / PgUp  offset (thick-wall |F|-T) down / up; or - / =\n"
    "  t            cycle TPMS family\n"
    "  n            toggle thickness normalisation\n"
    "  x / y / z    toggle section plane on that axis (viewport only)\n"
    "  <- / ->      slide the active section plane - / + (arrow keys)\n"
    "  f            flip the active section plane's kept side\n"
    "  0            clear all section planes\n"
    "  r            reset view\n"
    "  Esc          quit\n";
}

void printUsage() {
  std::cerr <<
    "dualc_raymarch -- GPU raymarch viewer for a TPMS-lattice-in-mesh field.\n"
    "\n"
    "USAGE\n"
    "  dualc_raymarch [input.obj] [options]\n"
    "\n"
    "OPTIONS\n"
    "  --type NAME            TPMS family (gyroid|schwarz-p|diamond|\n"
    "                         fischer-koch|lidinoid|neovius). Default: gyroid.\n"
    "  --wavelength W         Unit-cell side. Default: mesh AABB diag / 10, or\n"
    "                         0.25 with no input mesh.\n"
    "  --offset T             Thick-wall offset (|F|-T), T >= 0. Default 0.\n"
    "  --normalize-thickness  Normalise the TPMS by 1/|grad F| before offset.\n"
    "  --bounds x0,y0,z0,x1,y1,z1\n"
    "                         Sampling/raymarch box. Default: input AABB + 5%,\n"
    "                         or the unit cube with no input mesh.\n"
    "  --grid-res N           Mesh-SDF bake resolution per axis (default 96,\n"
    "                         max 256). Only the low-frequency mesh clip is\n"
    "                         baked; the lattice itself stays analytic.\n"
    "  --snapshot PATH        Render one frame headless to a PNG and exit (no\n"
    "                         window). For scripted/visual verification.\n"
    "  --help, -h             Show this message.\n"
    "\n"
    "EXAMPLES\n"
    "  dualc_raymarch --type gyroid --wavelength 0.2\n"
    "  dualc_raymarch --type gyroid --wavelength 0.02   # would OOM the mesher\n"
    "  dualc_raymarch bunny.obj --type gyroid --offset 1.0 --normalize-thickness\n";
}

} // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && (std::string(argv[1]) == "--help" ||
                    std::string(argv[1]) == "-h")) {
    printUsage();
    return 0;
  }

  std::string inPath, type = "gyroid", snapshotPath;
  double wavelength = -1.0, offset = 0.0;
  bool   normalizeThickness = false, haveBounds = false;
  dualc::BBox userBounds;
  int    gridRes = 96;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "[dualc_raymarch] " << f << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") { printUsage(); return 0; }
    else if (a == "--type") { const char* v = needValue("--type"); if (!v) return 2; type = v; }
    else if (a == "--wavelength") { const char* v = needValue("--wavelength"); if (!v) return 2; wavelength = std::atof(v); }
    else if (a == "--offset") { const char* v = needValue("--offset"); if (!v) return 2; offset = std::atof(v); }
    else if (a == "--normalize-thickness") { normalizeThickness = true; }
    else if (a == "--grid-res") { const char* v = needValue("--grid-res"); if (!v) return 2; gridRes = std::atoi(v); }
    else if (a == "--snapshot") { const char* v = needValue("--snapshot"); if (!v) return 2; snapshotPath = v; }
    else if (a == "--bounds") {
      const char* v = needValue("--bounds"); if (!v) return 2;
      if (!dce::parseBounds(v, userBounds)) {
        std::cerr << "[dualc_raymarch] --bounds expects six comma-separated doubles\n";
        return 2;
      }
      haveBounds = true;
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "[dualc_raymarch] unknown flag: " << a << "\n";
      return 2;
    } else if (inPath.empty()) {
      inPath = a;
    } else {
      std::cerr << "[dualc_raymarch] unexpected positional arg: " << a << "\n";
      return 2;
    }
  }

  const int typeIdx = tpmsIndex(type);
  if (typeIdx < 0) { std::cerr << "[dualc_raymarch] unknown --type '" << type << "'\n"; return 2; }
  if (offset < 0.0) { std::cerr << "[dualc_raymarch] --offset must be >= 0\n"; return 2; }
  if (gridRes < 2) gridRes = 2;
  if (gridRes > 256) gridRes = 256;

  // ---- compose the geometry: optional mesh clip + lattice phase center ----
  std::unique_ptr<geometrycentral::surface::SurfaceMesh> mesh;
  std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> geom;
  std::shared_ptr<dualc::MeshSource> src;
  Vector3 center{0, 0, 0};

  if (!inPath.empty()) {
    // Resolve relative to the current working directory (NOT the exe). A bare
    // "foot.obj" only works when it sits in the cwd -- catch the common case of
    // a missing/unreadable file with a clear message instead of an uncaught
    // geometry-central exception (which aborts before any window opens).
    if (!std::ifstream(inPath).good()) {
      std::cerr << "[dualc_raymarch] cannot open mesh '" << inPath
                << "': no such file (paths are relative to the current "
                   "directory, not the executable)\n";
      return 2;
    }
    std::cout << "[dualc_raymarch] reading " << inPath << "\n";
    try {
      std::tie(mesh, geom) = geometrycentral::surface::readSurfaceMesh(inPath);
    } catch (const std::exception& e) {
      std::cerr << "[dualc_raymarch] failed to read mesh '" << inPath
                << "': " << e.what() << "\n";
      return 2;
    }
    std::cout << "[dualc_raymarch] input: " << mesh->nVertices() << " verts, "
              << mesh->nFaces() << " faces\n";
    const dualc::BBox aabb = meshAABB(*mesh, *geom);
    center = aabb.center();
    if (wavelength <= 0.0) {
      const Vector3 e = aabb.extent();
      wavelength = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z) / 10.0;
    }
    if (!haveBounds) userBounds = padBBox(aabb, 0.05);
  } else {
    if (wavelength <= 0.0) wavelength = 0.25;
    if (!haveBounds) userBounds = dualc::BBox{Vector3{-0.5, -0.5, -0.5},
                                              Vector3{0.5, 0.5, 0.5}};
    center = userBounds.center();
  }

  // ---- bake the mesh clip into a narrow-band SDF grid (low-frequency) -----
  // Read the baked grid back for upload. The lattice is NOT baked: it is
  // analytic and stays exact in the shader.
  std::vector<float> texData;
  dualc::Vector3i texRes{1, 1, 1};
  dualc::BBox texRegion = userBounds;
  if (mesh) {
    src = std::make_shared<dualc::MeshSource>(*mesh, *geom);
    const Vector3 e = userBounds.extent();
    const double maxCell = std::max({e.x, e.y, e.z}) / std::max(gridRes - 1, 1);
    const double bandWidth = 3.0 * maxCell;
    std::cout << "[dualc_raymarch] baking mesh SDF at " << gridRes << "^3 ..."
              << std::endl;  // flush: the bake can take a few seconds
    dualc::FieldPtr gridPtr =
        src->bakeToGrid(userBounds, dualc::Vector3i{gridRes, gridRes, gridRes},
                        bandWidth);
    auto* grid = dynamic_cast<const dualc::GridField*>(gridPtr.get());
    if (!grid) { std::cerr << "[dualc_raymarch] internal: bake did not return a GridField\n"; return 1; }
    texData   = grid->values();        // copy; freed with gridPtr otherwise
    texRes    = grid->resolution();
    texRegion = grid->bounds();
  } else {
    texData.assign(1, 1.0f);           // dummy "outside" texel; never sampled
  }

  // ---- GL window ----------------------------------------------------------
  // A software rasteriser (llvmpipe, "GDI Generic", Mesa softpipe) raymarches at
  // seconds-per-frame, where a real GPU is interactive -- per-pixel cost grows
  // as the lattice gets finer.
  const bool snapshot = !snapshotPath.empty();
  GLFWwindow* win = dce::gl::initWindow("dualc_raymarch", 1100, 800, !snapshot);
  if (!win) return 1;

  GLuint prog = dce::gl::linkProgram(kVertSrc, kFragSrc, "dualc_raymarch");
  if (!prog) { glfwTerminate(); return 1; }

  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  // Upload the (real or dummy) mesh-SDF 3D texture; always bound so the
  // sampler is valid on every driver even when uHasMesh == 0.
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, texRes.x, texRes.y, texRes.z, 0,
               GL_RED, GL_FLOAT, texData.data());

  // ---- interaction state --------------------------------------------------
  State st;
  const Vector3 ext = userBounds.extent();
  const double diag = std::sqrt(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z);
  st.target     = userBounds.center();
  st.dist       = st.dist0 = std::max(diag * 1.3, 1e-3);
  st.type       = typeIdx;
  st.wavelength = wavelength;
  st.offset     = offset;
  st.normalize  = normalizeThickness;
  glfwSetWindowUserPointer(win, &st);
  glfwSetMouseButtonCallback(win, onMouseButton);
  glfwSetCursorPosCallback(win, onCursor);
  glfwSetScrollCallback(win, onScroll);
  glfwSetKeyCallback(win, onKey);

  std::cout << "[dualc_raymarch] type = " << dce::tpmsKinds()[typeIdx].name
            << ", wavelength = " << wavelength << ", offset = " << offset
            << (normalizeThickness ? " (normalized)" : "")
            << (mesh ? "" : ", bare lattice (no input mesh)") << "\n";
  if (!snapshot) printControls();

  const Vector3 texMin   = texRegion.min;
  const Vector3 texExt   = texRegion.extent();
  const Vector3 texScale{(texRes.x - 1) / std::max(texExt.x, 1e-12),
                         (texRes.y - 1) / std::max(texExt.y, 1e-12),
                         (texRes.z - 1) / std::max(texExt.z, 1e-12)};
  const Vector3 light = Vector3{0.5, 0.85, 0.6}.normalize();

  // Set all per-frame uniforms (orbit camera + field params) and draw the
  // full-screen triangle into a viewport of fbw x fbh. Shared by the live loop
  // and the headless --snapshot path.
  auto drawFrame = [&](int fbw, int fbh) {
    glViewport(0, 0, fbw, fbh);
    const double cp = std::cos(st.pitch), sp = std::sin(st.pitch);
    const Vector3 dir{cp * std::sin(st.yaw), sp, cp * std::cos(st.yaw)};
    const Vector3 camPos = st.target + dir * st.dist;
    const Vector3 fwd    = (st.target - camPos).normalize();
    Vector3 right        = cross(fwd, Vector3{0, 1, 0});
    if (right.norm() < 1e-9) right = Vector3{1, 0, 0};
    right = right.normalize();
    const Vector3 up = cross(right, fwd).normalize();

    const float lambda  = float(st.wavelength);
    const float tanHalf = 0.41421356f;  // tan(22.5 deg), half of a 45 deg fov

    setU3 (prog, "uCamPos",     camPos);
    setU3 (prog, "uCamForward", fwd);
    setU3 (prog, "uCamRight",   right);
    setU3 (prog, "uCamUp",      up);
    setU1f(prog, "uTanHalfFov", tanHalf);
    setU1f(prog, "uAspect",     float(fbw) / float(fbh));
    setU3 (prog, "uBoundsMin",  userBounds.min);
    setU3 (prog, "uBoundsMax",  userBounds.max);
    setU1i(prog, "uType",       st.type);
    setU1f(prog, "uWavelength", lambda);
    setU3 (prog, "uCenter",     center);
    setU1f(prog, "uOffset",     float(st.offset));
    setU1i(prog, "uNormalize",  st.normalize ? 1 : 0);
    setU1i(prog, "uHasMesh",    mesh ? 1 : 0);
    setU1i(prog, "uMeshTex",    0);
    setU3 (prog, "uTexMin",     texMin);
    setU3 (prog, "uTexScale",   texScale);
    setU3f(prog, "uTexDim",     float(texRes.x), float(texRes.y), float(texRes.z));
    setU3 (prog, "uLightDir",   light);
    // wavelength-relative march constants -> behaviour is scale-invariant.
    // uStepMax is capped well below the cell so the sphere-trace cannot skip a
    // wall between two same-sign samples (sign-change detection needs that).
    setU1f(prog, "uStepMin", lambda * 0.003f);
    setU1f(prog, "uStepMax", lambda * 0.350f);
    setU1f(prog, "uHmem",    lambda * 0.003f);

    // Section planes -> uClip[3]/uClipMask (built from bounds each frame).
    auto axisComp = [](const Vector3& v, int a) {
      return a == 0 ? v.x : (a == 1 ? v.y : v.z);
    };
    float clipv[12] = {0};
    int clipMask = 0;
    for (int a = 0; a < 3; ++a) {
      if (!st.clipOn[a]) continue;
      clipMask |= (1 << a);
      const double lo = axisComp(userBounds.min, a);
      const double hi = axisComp(userBounds.max, a);
      const double c = lo + double(st.clipPos01[a]) * (hi - lo);
      const float sgn = st.clipFlip[a] ? -1.0f : 1.0f;
      clipv[a * 4 + a] = sgn;               // n on this axis (others 0)
      clipv[a * 4 + 3] = -sgn * float(c);   // d = -dot(n, cutPoint)
    }
    setU4v(prog, "uClip", 3, clipv);
    setU1i(prog, "uClipMask", clipMask);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  };

  glUseProgram(prog);

  if (snapshot) {
    // ---- headless one-frame render to PNG (scripted verification) ---------
    const bool ok = dce::gl::snapshotPng(snapshotPath, 800, 600, drawFrame);
    std::cout << "[dualc_raymarch] snapshot -> " << snapshotPath
              << (ok ? " (ok)" : " (FAILED)") << std::endl;  // flush for scripts
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glfwDestroyWindow(win);
    glfwTerminate();
    return ok ? 0 : 2;
  }

  // ---- interactive render loop --------------------------------------------
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    if (fbw == 0 || fbh == 0) { glfwWaitEvents(); continue; }
    drawFrame(fbw, fbh);
    glfwSwapBuffers(win);
  }

  glDeleteTextures(1, &tex);
  glDeleteVertexArrays(1, &vao);
  glDeleteProgram(prog);
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
