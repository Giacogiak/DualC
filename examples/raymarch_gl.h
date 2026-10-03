#pragma once

#include "dualc/dualc.h"

#include <functional>
#include <string>

// Shared GL plumbing for the standalone GPU viewers (dualc_raymarch, the
// field-graph viewer dualc_field_view, and the dualc_glsl_parity harness): GLFW
// + glad window/context bring-up, shader compile/link, scalar/vector uniform
// setters, an orbit camera, and the headless one-frame PNG snapshot. Authored
// once here so the three apps share it instead of each owning a copy. GL types
// are referenced as plain unsigned int so this header pulls in no GL headers.

struct GLFWwindow;

namespace dce {
namespace gl {

// Create a GLFW window with a GL 3.3 core context, load glad, and print the GL
// renderer. `visible == false` makes a hidden window (the --snapshot path).
// Returns nullptr on any failure (after printing a diagnostic); the caller owns
// the window and must glfwTerminate().
GLFWwindow* initWindow(const char* title, int width, int height, bool visible);

// Compile a shader stage / link a program from GLSL source. Return 0 on error
// (after printing the info log prefixed with `label`).
unsigned int compileShader(unsigned int kind, const std::string& src,
                           const char* label);
unsigned int linkProgram(const std::string& vertexSrc,
                         const std::string& fragmentSrc, const char* label);

// Uniform setters by name (no-op when the uniform is not active).
void setU1f(unsigned int prog, const char* name, float v);
void setU1i(unsigned int prog, const char* name, int v);
void setU3(unsigned int prog, const char* name, const dualc::Vector3& v);
void setU3f(unsigned int prog, const char* name, float x, float y, float z);
// Set a `vec4` array uniform: `count` vec4s packed x,y,z,w in `v` (used for the
// viewers' section-plane table uClip[]).
void setU4v(unsigned int prog, const char* name, int count, const float* v);

// Upload one baked scalar grid (a mesh/winding source) to a GL_R32F 3D texture
// on unit `unit` and feed the sampler plus the `<base>_texMin/_texScale/_texDim`
// uniforms the codegen emits, where `<base>` is `samplerName` minus its "_tex"
// suffix. `values` is x-fastest then y then z, `res.x*y*z` long -- the GridField
// / MeshTexture layout. Returns the texture id so the caller can delete it.
//
// Shared so the viewer and the dualc_glsl_parity gate exercise the SAME upload
// path: a texMin/texScale/texDim or filtering bug must not be able to hide in
// the difference between two copies. Takes plain values rather than a
// MeshTexture so this GL layer keeps no dependency on the codegen.
unsigned int uploadGrid3D(unsigned int prog, const std::string& samplerName,
                          const dualc::BBox& region,
                          const dualc::Vector3i& res, const float* values,
                          int unit);

// ---- orbit camera ---------------------------------------------------------
struct OrbitState {
  dualc::Vector3 target{0, 0, 0};
  double yaw = 0.7;     // radians
  double pitch = 0.5;
  double dist = 3.0;
  double dist0 = 3.0;   // reset distance
  bool   dragging = false;
  double lastX = 0, lastY = 0;
};

// Frame a bounding box: target = center, dist = 1.3 * diagonal.
void frameBounds(OrbitState& s, const dualc::BBox& b);

// Feed GLFW input into the orbit state (call from the app's own callbacks).
void orbitDragBegin(OrbitState& s, double x, double y, bool pressed);
void orbitDragTo(OrbitState& s, double x, double y);
void orbitDolly(OrbitState& s, double scrollY);

struct Camera {
  dualc::Vector3 pos, forward, right, up;
};
Camera orbitCamera(const OrbitState& s);

// Render one frame via `drawFrame(W, H)` into a hidden context and write it as a
// PNG (GL bottom-up rows flipped to PNG top-down). Returns true on success.
bool snapshotPng(const std::string& path, int width, int height,
                 const std::function<void(int, int)>& drawFrame);

}  // namespace gl
}  // namespace dce
