#include "example_common.h"
#include "field_glsl.h"
#include "field_graph.h"
#include "gpu_preference.h"
#include "raymarch_gl.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// dualc_field_view -- a standalone GPU raymarch viewer for an arbitrary
// composed field-graph (roadmap docs/roadmap/12 §D).
//
// Where dualc_raymarch hand-codes one TPMS-in-mesh field, this compiles ANY
// field-graph (the same .fld/.json the dualc_field exporter consumes) to GLSL
// via field_glsl.h and sphere-traces it. So the field you contour to STL/3MF is
// the field you preview -- one source of truth, no mesh round-trip. RAM is
// bounded by the window, not the lattice, so it renders densities that OOM the
// contourer.
//
// Two edit tiers (the field_glsl binding table):
//   * structural reload ('l', or AUTOMATICALLY when the input file changes on
//     disk) re-parses + re-compiles the shader;
//   * a parameter nudge ('[' / ']') on the selected scalar binding only updates
//     a uniform -- no recompile, instant.
//
// The disk file-watch is what lets the Boletus Rhino side-car push live edits: it
// rewrites the graph file on every Grasshopper solve and the viewer reloads on its
// own (roadmap 12 §D + 15-boletus-handoff.md, Boletus Phase 5b). Only for a real
// file input (not --expr / stdin); 'l' still forces a manual reload.
//
// Opt-in build (DUALC_BUILD_FIELD_VIEW); reuses the GLFW + glad the sibling
// polyscope checkout vendors. Needs a GL window, so no CTest -- a hidden
// --snapshot PNG path renders one frame headless for scripted verification.

using dce::fieldgraph::GlslScene;
using dce::fieldgraph::GraphNode;

namespace {

void printUsage() {
  std::cerr <<
    "dualc_field_view -- GPU raymarch viewer for a composed field-graph.\n"
    "\n"
    "USAGE\n"
    "  dualc_field_view <graph.fld|graph.json|-> [options]\n"
    "  dualc_field_view --expr \"onion(gyroid(wavelength=0.3),thickness=0.05)\"\n"
    "\n"
    "OPTIONS\n"
    "  --expr STR        Inline text shorthand instead of a file.\n"
    "  --bounds x0,y0,z0,x1,y1,z1\n"
    "                    Raymarch box. Default: the field's own bounds (an\n"
    "                    infinite field -- plane/repeat -- requires this).\n"
    "  --grid-res N      Mesh-source bake resolution per axis (default 96,\n"
    "                    max 256). The bake covers the raymarch box, so it\n"
    "                    composes with --bounds.\n"
    "  --preview-scale S Resolution factor while orbiting / editing a param\n"
    "                    (default 0.5). The raymarch renders at S x the window\n"
    "                    while the view moves, then snaps to full-res when it\n"
    "                    settles -- keeps a heavy lattice smooth to navigate.\n"
    "                    1.0 disables it (always full-res). Range [0.1, 1.0].\n"
    "  --es              Emit a #version 300 es shader (WebGL2 subset) instead\n"
    "                    of desktop 330 core. For portability checks.\n"
    "  --snapshot PATH   Render one frame headless to a PNG and exit.\n"
    "  --help, -h        Show this message.\n"
    "\n"
    "CONTROLS\n"
    "  left-drag orbit | scroll dolly | tab select param | [ / ] nudge it\n"
    "  l reload file (structural; also auto-reloads on file change) | r reset view | Esc quit\n";
}

std::string readAll(std::istream& in) {
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Parse text to a GraphNode, sniffing JSON vs shorthand (mirrors dualc_field):
// a leading '{' (or a .json hint) is JSON, otherwise the terse shorthand.
GraphNode parseText(const std::string& text, bool jsonHint) {
  std::size_t i = 0;
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
  const bool isJson = jsonHint || (i < text.size() && text[i] == '{');
  return isJson ? dce::fieldgraph::parseJson(text)
                : dce::fieldgraph::parseShorthand(text);
}

// Upload one baked mesh grid to a GL_R32F 3D texture on unit `unit` and feed its
// sampler + transform uniforms. Returns the texture id (the caller owns it: a
// structural reload re-bakes and must release the previous one).
GLuint uploadMesh(GLuint prog, const dce::fieldgraph::MeshTexture& mt, int unit) {
  // Shared with dualc_glsl_parity so the gate exercises this exact upload path.
  return dce::gl::uploadGrid3D(prog, mt.samplerName, mt.region, mt.resolution,
                               mt.values.data(), unit);
}

// Set a binding's current value into its uniform (a parameter edit -- no
// recompile). mat4 bindings are uploaded row-major (transpose) to match
// Mat4::transformPoint.
void setBinding(GLuint prog, const dce::fieldgraph::UniformBinding& b) {
  const GLint loc = glGetUniformLocation(prog, b.name.c_str());
  if (loc < 0) return;
  if (b.glslType == "float") glUniform1f(loc, b.value[0]);
  else if (b.glslType == "vec3") glUniform3f(loc, b.value[0], b.value[1], b.value[2]);
  else if (b.glslType == "mat4") glUniformMatrix4fv(loc, 1, GL_TRUE, b.value.data());
}

}  // namespace

int main(int argc, char** argv) {
  // Some GL drivers tear the process down in glfwTerminate() without flushing C
  // stdio; keep stdout unbuffered so scripted --snapshot messages survive.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::string inPath, expr, snapshotPath;
  bool haveBounds = false, emitES = false, useExpr = false;
  dualc::BBox userBounds;
  int gridRes = dce::fieldgraph::kDefaultMeshGridRes;
  float previewScale = 0.5f;  // resolution factor while the view is moving

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto needValue = [&](const char* f) -> const char* {
      if (i + 1 >= argc) { std::cerr << "[dualc_field_view] " << f << " requires a value\n"; return nullptr; }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") { printUsage(); return 0; }
    else if (a == "--expr") { const char* v = needValue("--expr"); if (!v) return 2; expr = v; useExpr = true; }
    else if (a == "--grid-res") { const char* v = needValue("--grid-res"); if (!v) return 2; gridRes = std::atoi(v); }
    else if (a == "--preview-scale") { const char* v = needValue("--preview-scale"); if (!v) return 2; previewScale = float(std::atof(v)); }
    else if (a == "--es") { emitES = true; }
    else if (a == "--snapshot") { const char* v = needValue("--snapshot"); if (!v) return 2; snapshotPath = v; }
    else if (a == "--bounds") {
      const char* v = needValue("--bounds"); if (!v) return 2;
      if (!dce::parseBounds(v, userBounds)) {
        std::cerr << "[dualc_field_view] --bounds expects six comma-separated doubles\n";
        return 2;
      }
      haveBounds = true;
    } else if (!a.empty() && a[0] == '-' && a != "-") {
      std::cerr << "[dualc_field_view] unknown flag: " << a << "\n";
      return 2;
    } else if (inPath.empty()) {
      inPath = a;
    } else {
      std::cerr << "[dualc_field_view] unexpected positional arg: " << a << "\n";
      return 2;
    }
  }
  gridRes = std::clamp(gridRes, 2, dce::fieldgraph::kMaxMeshGridRes);
  previewScale = std::clamp(previewScale, 0.1f, 1.0f);
  if (!useExpr && inPath.empty()) { printUsage(); return 2; }

  // ---- read + parse the graph ---------------------------------------------
  auto loadGraph = [&](GraphNode& out) -> bool {
    try {
      if (useExpr) { out = dce::fieldgraph::parseShorthand(expr); return true; }
      if (inPath == "-") { out = parseText(readAll(std::cin), false); return true; }
      std::ifstream f(inPath);
      if (!f.good()) {
        std::cerr << "[dualc_field_view] cannot open '" << inPath << "'\n";
        return false;
      }
      const bool jsonHint = inPath.size() > 5 &&
                            inPath.substr(inPath.size() - 5) == ".json";
      out = parseText(readAll(f), jsonHint);
      return true;
    } catch (const dce::fieldgraph::GraphError& e) {
      std::cerr << "[dualc_field_view] field-graph error"
                << (e.pointer().empty() ? "" : " at " + e.pointer()) << ": "
                << e.what() << "\n";
      return false;
    }
  };

  GraphNode root;
  if (!loadGraph(root)) return 2;

  dce::fieldgraph::FileMeshResolver resolver;

  // Bounds: explicit flag, else the field's own bounds (built once via the
  // exporter path, which also validates the graph + shares the mesh cache).
  dualc::BBox bounds = userBounds;
  if (!haveBounds) {
    try {
      auto fg = dce::fieldgraph::FieldGraph::build(root, resolver);
      bounds = fg.field().bounds();
    } catch (const dce::fieldgraph::GraphError& e) {
      std::cerr << "[dualc_field_view] field-graph error"
                << (e.pointer().empty() ? "" : " at " + e.pointer()) << ": "
                << e.what() << "\n";
      return 2;
    }
    if (!bounds.isValid()) {
      std::cerr << "[dualc_field_view] the field has no finite bounds "
                   "(infinite source / repeat) -- pass --bounds\n";
      return 2;
    }
    const dualc::Vector3 e = bounds.extent() * 0.05;  // 5% pad
    bounds = dualc::BBox{bounds.min - e, bounds.max + e};
  }

  // ---- compile the graph to GLSL ------------------------------------------
  auto compile = [&](const GraphNode& g, GlslScene& scene) -> bool {
    try {
      scene = dce::fieldgraph::compileToGlsl(g, resolver, bounds, gridRes);
      return true;
    } catch (const dce::fieldgraph::GraphError& e) {
      std::cerr << "[dualc_field_view] codegen error"
                << (e.pointer().empty() ? "" : " at " + e.pointer()) << ": "
                << e.what() << "\n";
      return false;
    }
  };
  GlslScene scene;
  if (!compile(root, scene)) return 2;
  std::cout << "[dualc_field_view] compiled " << scene.nodeCount << " nodes, "
            << scene.bindings.size() << " params, " << scene.meshes.size()
            << " mesh texture(s)";
  // Report the bake resolution AND its world cell size once (every mesh node
  // bakes at the same resolution over the same box). --grid-res went unnoticed
  // as a no-op precisely because the render never changed with it.
  if (!scene.meshes.empty()) {
    const auto& mt = scene.meshes.front();
    const dualc::Vector3 e = mt.region.extent();
    const double cell = std::max({e.x, e.y, e.z}) /
                        std::max(mt.resolution.x - 1, 1);
    std::cout << " @ " << mt.resolution.x << "^3 (cell " << cell << ")";
  }
  std::cout << ", stepScale " << scene.stepScale << "\n";

  // ---- GL window + program ------------------------------------------------
  const bool snapshot = !snapshotPath.empty();
  GLFWwindow* win = dce::gl::initWindow("dualc_field_view", 1100, 800, !snapshot);
  if (!win) return 1;

  auto buildProgram = [&](const GlslScene& s) -> GLuint {
    return dce::gl::linkProgram(dce::fieldgraph::vertexShaderSource(emitES),
                                dce::fieldgraph::assembleTraceShader(s, emitES),
                                "dualc_field_view");
  };
  GLuint prog = buildProgram(scene);
  if (!prog) { glfwTerminate(); return 1; }

  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  // Upload mesh textures + set all initial uniforms. The previous scene's
  // textures are released first: a structural reload re-bakes, and the
  // file-watch path fires on every write (it is what the Boletus side-car
  // drives), so holding them would leak a full bake per solve -- 64 MB each at
  // --grid-res 256.
  std::vector<GLuint> meshTextures;
  auto applyScene = [&](const GlslScene& s) {
    glUseProgram(prog);
    if (!meshTextures.empty()) {
      glDeleteTextures(static_cast<GLsizei>(meshTextures.size()),
                       meshTextures.data());
      meshTextures.clear();
    }
    for (std::size_t i = 0; i < s.meshes.size(); ++i)
      meshTextures.push_back(uploadMesh(prog, s.meshes[i], static_cast<int>(i)));
    for (const auto& b : s.bindings) setBinding(prog, b);
  };
  applyScene(scene);

  // ---- interaction state --------------------------------------------------
  dce::gl::OrbitState cam;
  dce::gl::frameBounds(cam, bounds);
  int selected = 0;  // index into scene.bindings (scalars are nudgeable)

  // Viewport-only section planes (one per axis). pos01 is the cut position
  // normalized in [0,1] along the bounds of that axis; flip swaps the kept side;
  // active is the axis the ;/: slide acts on. See clippedSDF() in kTraceFramework.
  struct ClipState { bool on[3]{}; float pos01[3]{0.5f, 0.5f, 0.5f};
                     bool flip[3]{}; int active = 0; } clip;

  // lastInteract stamps the time of the most recent camera / param / section
  // change; the render loop renders at previewScale for a short settle window
  // after it (and while a drag is live), then snaps back to full resolution.
  struct UserData { dce::gl::OrbitState* cam; int* selected; GlslScene* scene;
                    GLuint* prog; ClipState* clip; bool reload = false;
                    double lastInteract = 0.0; }
      ud{&cam, &selected, &scene, &prog, &clip};
  glfwSetWindowUserPointer(win, &ud);

  glfwSetMouseButtonCallback(win, [](GLFWwindow* w, int button, int action, int) {
    auto* u = static_cast<UserData*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      double x, y; glfwGetCursorPos(w, &x, &y);
      dce::gl::orbitDragBegin(*u->cam, x, y, action == GLFW_PRESS);
      u->lastInteract = glfwGetTime();  // press begins / release ends a settle
    }
  });
  glfwSetCursorPosCallback(win, [](GLFWwindow* w, double x, double y) {
    auto* u = static_cast<UserData*>(glfwGetWindowUserPointer(w));
    dce::gl::orbitDragTo(*u->cam, x, y);
    if (u->cam->dragging) u->lastInteract = glfwGetTime();  // orbiting, not hover
  });
  glfwSetScrollCallback(win, [](GLFWwindow* w, double, double dy) {
    auto* u = static_cast<UserData*>(glfwGetWindowUserPointer(w));
    dce::gl::orbitDolly(*u->cam, dy);
    u->lastInteract = glfwGetTime();
  });
  glfwSetKeyCallback(win, [](GLFWwindow* w, int key, int scancode, int action,
                             int /*mods*/) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    auto* u = static_cast<UserData*>(glfwGetWindowUserPointer(w));
    u->lastInteract = glfwGetTime();  // any key that reaches here changes the view
    GlslScene& s = *u->scene;
    ClipState& clip = *u->clip;
    auto toggleClip = [&](int axis) {
      clip.on[axis] = !clip.on[axis];
      clip.active = axis;
      std::cout << "[dualc_field_view] section " << "xyz"[axis] << " "
                << (clip.on[axis] ? "on" : "off") << "\n";
    };
    auto slideClip = [&](float d) {
      const int a = clip.active;
      if (!clip.on[a]) return;
      clip.pos01[a] = std::clamp(clip.pos01[a] + d, 0.0f, 1.0f);
      std::cout << "[dualc_field_view] section " << "xyz"[a] << " pos "
                << clip.pos01[a] << "\n";
    };
    // GLFW key codes are physical US-layout positions, so the LABELED x/y/z/f keys
    // sit elsewhere on AZERTY/QWERTZ. Match the character the key actually prints
    // (glfwGetKeyName) so the labels work on any layout; slide is on the arrows
    // (layout-independent). See command_reference/12 -> Section planes.
    const char* kn = glfwGetKeyName(key, scancode);
    const char ch = (kn && kn[0] && kn[1] == '\0') ? kn[0] : '\0';
    switch (ch) {
      case 'x': toggleClip(0); return;
      case 'y': toggleClip(1); return;
      case 'z': toggleClip(2); return;
      case 'f':
        clip.flip[clip.active] = !clip.flip[clip.active];
        std::cout << "[dualc_field_view] section " << "xyz"[clip.active]
                  << " flip " << (clip.flip[clip.active] ? "on" : "off") << "\n";
        return;
      default: break;
    }
    auto nudge = [&](double f) {
      // Find the selected SCALAR binding and scale it (parameter edit).
      if (s.bindings.empty()) return;
      int n = static_cast<int>(s.bindings.size());
      *u->selected = ((*u->selected % n) + n) % n;
      auto& b = s.bindings[*u->selected];
      if (b.glslType != "float") return;
      b.value[0] *= float(f);  // multiplicative nudge (scale-free)
      glUseProgram(*u->prog);
      setBinding(*u->prog, b);
      std::cout << "[dualc_field_view] " << b.paramKey << " = " << b.value[0]
                << "\n";
    };
    switch (key) {
      case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); break;
      case GLFW_KEY_TAB:
        if (!s.bindings.empty()) {
          *u->selected = (*u->selected + 1) % static_cast<int>(s.bindings.size());
          std::cout << "[dualc_field_view] selected param: "
                    << s.bindings[*u->selected].paramKey << "\n";
        }
        break;
      // Nudge the selected scalar. [ / ] are physical-US positions (AltGr-only on
      // AZERTY, where glfwGetKeyName can't see them), so down/up arrows are the
      // layout-independent path; the brackets stay as US-keyboard alternates.
      case GLFW_KEY_LEFT_BRACKET:
      case GLFW_KEY_DOWN: nudge(0.8);  break;
      case GLFW_KEY_RIGHT_BRACKET:
      case GLFW_KEY_UP:   nudge(1.25); break;
      // Slide the active section plane (left/right: same on every layout, auto-repeat).
      case GLFW_KEY_LEFT:  slideClip(-0.02f); break;
      case GLFW_KEY_RIGHT: slideClip(0.02f);  break;
      case GLFW_KEY_0:
        clip.on[0] = clip.on[1] = clip.on[2] = false;
        std::cout << "[dualc_field_view] section cleared\n";
        break;
      case GLFW_KEY_L: u->reload = true; break;
      case GLFW_KEY_R:
        u->cam->yaw = 0.7; u->cam->pitch = 0.5; u->cam->dist = u->cam->dist0;
        break;
      default: break;
    }
  });

  if (!snapshot) {
    std::cout <<
      "[dualc_field_view] controls: left-drag orbit | scroll dolly | "
      "tab select | up/down (or [ / ]) nudge | x/y/z section | "
      "left/right slide | f flip | 0 clear | l reload (auto on file change) | "
      "r reset | Esc quit\n";
    if (previewScale < 0.999f)
      std::cout << "[dualc_field_view] adaptive preview: "
                << int(previewScale * 100.0f + 0.5f)
                << "% resolution while moving, full-res when still "
                   "(--preview-scale 1.0 to disable)\n";
  }

  // Non-const: recomputed on reload so a size-changing edit keeps the raymarch box
  // + section-plane extents correct (see the reload block below).
  dualc::Vector3 bext = bounds.extent();
  double diag = std::sqrt(bext.x * bext.x + bext.y * bext.y + bext.z * bext.z);
  const dualc::Vector3 light = dualc::Vector3{0.5, 0.85, 0.6}.normalize();

  // ---- adaptive-resolution preview target ---------------------------------
  // While the view is moving, render the raymarch into a low-res offscreen FBO
  // (previewScale x window) and glBlitFramebuffer it up with GL_LINEAR; when it
  // settles, render full-res straight to the window. A heavy lattice (a tapered
  // octet is 72 SDF primitives/step) then stays smooth to orbit/edit while the
  // still frame is crisp. Attachment is a plain RGBA8 renderbuffer -- never
  // sampled, so no texture/precision plumbing and --es stays unaffected.
  GLuint lowFbo = 0, lowRbo = 0;
  int lowW = 0, lowH = 0;
  auto ensureLowTarget = [&](int w, int h) {
    if (w == lowW && h == lowH && lowFbo) return;
    if (!lowFbo) glGenFramebuffers(1, &lowFbo);
    if (!lowRbo) glGenRenderbuffers(1, &lowRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, lowRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, lowFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, lowRbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    lowW = w; lowH = h;
  };

  // Render the scene at (fbw x fbh) into the currently bound framebuffer. All
  // glUniform calls target the active program, so bind prog first (it may have
  // been swapped by a reload).
  auto drawFrame = [&](int fbw, int fbh) {
    glUseProgram(prog);
    glViewport(0, 0, fbw, fbh);
    const dce::gl::Camera c = dce::gl::orbitCamera(cam);
    dce::gl::setU3(prog, "uCamPos", c.pos);
    dce::gl::setU3(prog, "uCamForward", c.forward);
    dce::gl::setU3(prog, "uCamRight", c.right);
    dce::gl::setU3(prog, "uCamUp", c.up);
    dce::gl::setU1f(prog, "uTanHalfFov", 0.41421356f);  // tan(22.5 deg)
    dce::gl::setU1f(prog, "uAspect", float(fbw) / float(fbh));
    dce::gl::setU3(prog, "uBoundsMin", bounds.min);
    dce::gl::setU3(prog, "uBoundsMax", bounds.max);
    dce::gl::setU3(prog, "uLightDir", light);
    // March constants scale to the finest feature present (recomputed each frame
    // so a reload tracks it): a dense lattice in large bounds needs a
    // wavelength-relative FD step + step cap (the factors dualc_raymarch uses),
    // or diag*0.001 would be too coarse to resolve a wall and the trace would
    // step over it. Fall back to the diagonal for an all-analytic graph.
    const double feat = scene.featureScale > 0.0
                            ? std::min(double(scene.featureScale), diag)
                            : diag;
    const float stepMin = float(std::min(diag * 0.002, feat * 0.003));
    dce::gl::setU1f(prog, "uStepScale", scene.stepScale);
    // A stepScale of 1.0 means nodeStepScale found no TPMS / smooth boolean /
    // warp -- the graph is a true SDF, so the trace can sphere-step by distance
    // and skip the per-step gradient (a large speedup on dense strut lattices).
    dce::gl::setU1f(prog, "uMetricSDF", scene.stepScale >= 0.999f ? 1.0f : 0.0f);
    dce::gl::setU1f(prog, "uStepMin", stepMin);
    dce::gl::setU1f(prog, "uStepMax", float(std::min(diag * 0.100, feat * 0.350)));
    dce::gl::setU1f(prog, "uHmem", stepMin);  // FD normals use the smallest step
    // Section planes -> uClip[3]/uClipMask. Built each frame from the bounds so a
    // reload (new program) keeps the active section. n is +axis (keep low side),
    // negated on flip; d = -dot(n, cutPoint). The cut point sits at pos01 along
    // the axis extent.
    auto axisComp = [](const dualc::Vector3& v, int a) {
      return a == 0 ? v.x : (a == 1 ? v.y : v.z);
    };
    float clipv[12] = {0};
    int clipMask = 0;
    for (int a = 0; a < 3; ++a) {
      if (!clip.on[a]) continue;
      clipMask |= (1 << a);
      const double lo = axisComp(bounds.min, a), hi = axisComp(bounds.max, a);
      const double c = lo + double(clip.pos01[a]) * (hi - lo);
      const float s = clip.flip[a] ? -1.0f : 1.0f;
      clipv[a * 4 + a] = s;               // n on this axis (others 0)
      clipv[a * 4 + 3] = -s * float(c);   // d = -dot(n, cutPoint)
    }
    dce::gl::setU4v(prog, "uClip", 3, clipv);
    dce::gl::setU1i(prog, "uClipMask", clipMask);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  };

  glUseProgram(prog);

  if (snapshot) {
    const bool ok = dce::gl::snapshotPng(snapshotPath, 800, 600, drawFrame);
    std::cout << "[dualc_field_view] snapshot -> " << snapshotPath
              << (ok ? " (ok)" : " (FAILED)") << std::endl;
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glfwDestroyWindow(win);
    glfwTerminate();
    return ok ? 0 : 2;
  }

  // Live file-watch state: auto-reload when the graph file's mtime changes. Only a
  // real file has an mtime to poll (--expr / stdin are one-shot). This is the hook
  // the Boletus side-car drives (Phase 5b) -- it rewrites the file each GH solve.
  const bool watch = !useExpr && inPath != "-";
  std::filesystem::file_time_type lastWrite{};
  if (watch) { std::error_code ec; lastWrite = std::filesystem::last_write_time(inPath, ec); }
  double lastWatchCheck = glfwGetTime();

  const double kSettle = 0.18;  // seconds of full-res render after motion stops
  bool interacting = true;      // last frame's state -> this frame's wait policy
  // Open in the settle window so the very first frames render at previewScale:
  // the window shows an image immediately instead of blocking on a heavy full-res
  // frame before it is even interactive.
  ud.lastInteract = glfwGetTime();
  while (!glfwWindowShouldClose(win)) {
    // Spin at vsync while the view is moving so orbit/nudge stays responsive;
    // when settled, block up to 0.1 s (still wakes on input and for the ~7 Hz
    // file-watch poll below) so an idle window isn't re-raymarched at 60 Hz.
    if (interacting) glfwPollEvents(); else glfwWaitEventsTimeout(0.1);
    // Poll the file mtime (throttled, ~7 Hz) and request a reload on any change.
    if (watch) {
      const double now = glfwGetTime();
      if (now - lastWatchCheck > 0.15) {
        lastWatchCheck = now;
        std::error_code ec;
        const auto t = std::filesystem::last_write_time(inPath, ec);
        if (!ec && t != lastWrite) { lastWrite = t; ud.reload = true; }
      }
    }
    if (ud.reload) {
      ud.reload = false;
      GraphNode g;
      GlslScene s;
      if (loadGraph(g)) {
        // Track auto-bounds across a size-changing edit so the raymarch box + the
        // section planes stay correct (bounds/bext/diag were captured at startup).
        // Skip when the user pinned --bounds. The camera is intentionally NOT
        // re-framed -- keep the user's orbit; 'r' resets it.
        if (!haveBounds) {
          try {
            auto fg = dce::fieldgraph::FieldGraph::build(g, resolver);
            const dualc::BBox nb = fg.field().bounds();
            if (nb.isValid()) {
              const dualc::Vector3 e = nb.extent() * 0.05;  // 5% pad, as at startup
              bounds = dualc::BBox{nb.min - e, nb.max + e};
              bext = bounds.extent();
              diag = std::sqrt(bext.x * bext.x + bext.y * bext.y + bext.z * bext.z);
            }
          } catch (const dce::fieldgraph::GraphError&) { /* keep old bounds; compile() reports */ }
        }
        if (compile(g, s)) {
          GLuint np = buildProgram(s);
          if (np) {
            glDeleteProgram(prog);
            prog = np;
            scene = std::move(s);
            selected = 0;
            applyScene(scene);
            std::cout << "[dualc_field_view] reloaded\n";
          }
        }
      }
    }
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    if (fbw == 0 || fbh == 0) { glfwWaitEvents(); continue; }

    // A left-drag or a recent camera/param/section change keeps us "moving".
    const double now = glfwGetTime();
    interacting = cam.dragging || (now - ud.lastInteract) < kSettle;

    if (interacting && previewScale < 0.999f) {
      // Low-res pass into the offscreen FBO, then linear-upscale to the window.
      const int sw = std::max(1, int(float(fbw) * previewScale));
      const int sh = std::max(1, int(float(fbh) * previewScale));
      ensureLowTarget(sw, sh);
      glBindFramebuffer(GL_FRAMEBUFFER, lowFbo);
      drawFrame(sw, sh);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, lowFbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBlitFramebuffer(0, 0, sw, sh, 0, 0, fbw, fbh, GL_COLOR_BUFFER_BIT,
                        GL_LINEAR);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
      // Settled (or --preview-scale 1.0): full-res straight to the window.
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      drawFrame(fbw, fbh);
    }
    glfwSwapBuffers(win);
  }

  if (lowRbo) glDeleteRenderbuffers(1, &lowRbo);
  if (lowFbo) glDeleteFramebuffers(1, &lowFbo);
  if (!meshTextures.empty())
    glDeleteTextures(static_cast<GLsizei>(meshTextures.size()),
                     meshTextures.data());
  glDeleteVertexArrays(1, &vao);
  glDeleteProgram(prog);
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
