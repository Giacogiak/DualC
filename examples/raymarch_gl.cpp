#include "raymarch_gl.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_image_write.h"  // declarations only; impl in third_party/stb_impl.cpp

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using dualc::Vector3;

namespace dce {
namespace gl {

GLFWwindow* initWindow(const char* title, int width, int height, bool visible) {
  if (!glfwInit()) {
    std::cerr << "[dualc_gl] glfwInit failed\n";
    return nullptr;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
  glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
  GLFWwindow* win = glfwCreateWindow(width, height, title, nullptr, nullptr);
  if (!win) {
    std::cerr << "[dualc_gl] window creation failed\n";
    glfwTerminate();
    return nullptr;
  }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    std::cerr << "[dualc_gl] failed to load GL via glad\n";
    glfwDestroyWindow(win);
    glfwTerminate();
    return nullptr;
  }
  if (const GLubyte* r = glGetString(GL_RENDERER))
    std::cout << "[dualc_gl] GL renderer: " << r << "\n";
  return win;
}

unsigned int compileShader(unsigned int kind, const std::string& src,
                           const char* label) {
  GLuint s = glCreateShader(kind);
  const char* csrc = src.c_str();
  glShaderSource(s, 1, &csrc, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    std::cerr << "[dualc_gl] " << label << " shader compile error:\n"
              << log << "\n";
    glDeleteShader(s);
    return 0;
  }
  return s;
}

unsigned int linkProgram(const std::string& vertexSrc,
                         const std::string& fragmentSrc, const char* label) {
  GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc, "vertex");
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc, "fragment");
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return 0;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetProgramInfoLog(p, sizeof(log), nullptr, log);
    std::cerr << "[dualc_gl] " << label << " program link error:\n" << log << "\n";
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

void setU1f(unsigned int prog, const char* name, float v) {
  glUniform1f(glGetUniformLocation(prog, name), v);
}
void setU1i(unsigned int prog, const char* name, int v) {
  glUniform1i(glGetUniformLocation(prog, name), v);
}
void setU3(unsigned int prog, const char* name, const Vector3& v) {
  glUniform3f(glGetUniformLocation(prog, name), float(v.x), float(v.y), float(v.z));
}
void setU3f(unsigned int prog, const char* name, float x, float y, float z) {
  glUniform3f(glGetUniformLocation(prog, name), x, y, z);
}
void setU4v(unsigned int prog, const char* name, int count, const float* v) {
  glUniform4fv(glGetUniformLocation(prog, name), count, v);
}

unsigned int uploadGrid3D(unsigned int prog, const std::string& samplerName,
                          const dualc::BBox& region,
                          const dualc::Vector3i& res, const float* values,
                          int unit) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_3D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, res.x, res.y, res.z, 0, GL_RED,
               GL_FLOAT, values);

  // "u_n5_tex" -> "u_n5"; the codegen names the transform uniforms off that.
  const std::string base = samplerName.substr(0, samplerName.size() - 4);
  const Vector3 ext = region.extent();
  setU1i(prog, samplerName.c_str(), unit);
  setU3(prog, (base + "_texMin").c_str(), region.min);
  setU3f(prog, (base + "_texScale").c_str(),
         float((res.x - 1) / std::max(ext.x, 1e-12)),
         float((res.y - 1) / std::max(ext.y, 1e-12)),
         float((res.z - 1) / std::max(ext.z, 1e-12)));
  setU3f(prog, (base + "_texDim").c_str(), float(res.x), float(res.y),
         float(res.z));
  return tex;
}

void frameBounds(OrbitState& s, const dualc::BBox& b) {
  const Vector3 e = b.extent();
  const double diag = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);
  s.target = b.center();
  s.dist = s.dist0 = std::max(diag * 1.3, 1e-3);
}

void orbitDragBegin(OrbitState& s, double x, double y, bool pressed) {
  s.dragging = pressed;
  s.lastX = x;
  s.lastY = y;
}
void orbitDragTo(OrbitState& s, double x, double y) {
  if (!s.dragging) return;
  const double dx = x - s.lastX, dy = y - s.lastY;
  s.lastX = x;
  s.lastY = y;
  s.yaw -= dx * 0.008;
  s.pitch += dy * 0.008;
  s.pitch = std::clamp(s.pitch, -1.5, 1.5);
}
void orbitDolly(OrbitState& s, double scrollY) {
  s.dist *= std::pow(0.9, scrollY);
  s.dist = std::max(s.dist, 1e-4);
}

Camera orbitCamera(const OrbitState& s) {
  const double cp = std::cos(s.pitch), sp = std::sin(s.pitch);
  const Vector3 dir{cp * std::sin(s.yaw), sp, cp * std::cos(s.yaw)};
  Camera cam;
  cam.pos = s.target + dir * s.dist;
  cam.forward = (s.target - cam.pos).normalize();
  Vector3 right = cross(cam.forward, Vector3{0, 1, 0});
  if (right.norm() < 1e-9) right = Vector3{1, 0, 0};
  cam.right = right.normalize();
  cam.up = cross(cam.right, cam.forward).normalize();
  return cam;
}

bool snapshotPng(const std::string& path, int width, int height,
                 const std::function<void(int, int)>& drawFrame) {
  drawFrame(width, height);
  glFinish();
  std::vector<unsigned char> px(static_cast<std::size_t>(width) * height * 3);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, px.data());
  std::vector<unsigned char> flipped(px.size());  // GL bottom-up -> PNG top-down
  for (int r = 0; r < height; ++r)
    std::copy_n(&px[static_cast<std::size_t>(height - 1 - r) * width * 3],
                width * 3, &flipped[static_cast<std::size_t>(r) * width * 3]);
  return stbi_write_png(path.c_str(), width, height, 3, flipped.data(),
                        width * 3) != 0;
}

}  // namespace gl
}  // namespace dce
