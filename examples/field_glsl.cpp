#include "field_glsl.h"

#include "example_common.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Field-graph -> GLSL codegen. See field_glsl.h for the contract. The walk
// mirrors FieldGraph::build (field_graph.cpp) op-for-op; where the builder
// constructs a C++ field, this emits a `float fN(vec3 p)` calling a prelude
// helper that is bit-faithful (within float32 + GPU-trig tolerance) to the C++
// formula in src/implicit/. A preview==export parity test per node (the
// dualc_glsl_parity harness) is the acceptance gate.

namespace dce {
namespace fieldgraph {

namespace {

// ===========================================================================
// Fixed GLSL prelude: the primitive / TPMS / boolean / decorator library that
// the generated per-node functions call. Bit-faithful to src/implicit/*.cpp.
// Authored to the WebGL2 (#version 300 es) subset; the version/precision header
// is prepended per target (see header()).
// ===========================================================================

const char* kLibrary = R"GLSL(
const float DC_PI = 3.14159265358979;

float dcSgn(float x) { return x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0); }
float dcDot2(vec3 v) { return dot(v, v); }

// ---- primitives (src/implicit/primitives_tierA/B/C.cpp) -------------------
float sdSphere(vec3 p, vec3 c, float r) { return length(p - c) - r; }

float sdBox(vec3 p, vec3 mn, vec3 mx) {
  vec3 c = (mn + mx) * 0.5, h = (mx - mn) * 0.5;
  vec3 q = abs(p - c) - h;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundBox(vec3 p, vec3 mn, vec3 mx, float r) {
  vec3 c = (mn + mx) * 0.5, h = (mx - mn) * 0.5;
  vec3 q = abs(p - c) - h + vec3(r);
  return min(max(q.x, max(q.y, q.z)), 0.0) + length(max(q, 0.0)) - r;
}

float sdTorus(vec3 p, vec3 c, float R, float r) {
  vec3 d = p - c;
  float qx = sqrt(d.x * d.x + d.z * d.z) - R;
  return sqrt(qx * qx + d.y * d.y) - r;
}

float sdCone(vec3 p, vec3 c, float angle, float h) {
  vec3 pl = p - c;
  float si = sin(angle), co = cos(angle);
  float qx = h * (si / co);
  float qy = -h;
  float wx = sqrt(pl.x * pl.x + pl.z * pl.z);
  float wy = pl.y;
  float t1 = clamp((wx * qx + wy * qy) / (qx * qx + qy * qy), 0.0, 1.0);
  float ax = wx - qx * t1, ay = wy - qy * t1;
  float t2 = clamp(wx / qx, 0.0, 1.0);
  float bx = wx - qx * t2, by = wy - qy;
  float k = dcSgn(qy);
  float d = min(ax * ax + ay * ay, bx * bx + by * by);
  float s = max(k * (wx * qy - wy * qx), k * (wy - qy));
  return sqrt(d) * dcSgn(s);
}

float dcEdgeDist2(vec3 e, vec3 pe) {
  vec3 v = e * clamp(dot(e, pe) / dcDot2(e), 0.0, 1.0) - pe;
  return dot(v, v);
}
// Unsigned (open surface) -- pair with an onion to give it an interior.
float sdTriangle(vec3 p, vec3 a, vec3 b, vec3 c) {
  vec3 ba = b - a, pa = p - a;
  vec3 cb = c - b, pb = p - b;
  vec3 ac = a - c, pc = p - c;
  vec3 nor = cross(ba, ac);
  float facing = dcSgn(dot(cross(ba, nor), pa)) +
                 dcSgn(dot(cross(cb, nor), pb)) +
                 dcSgn(dot(cross(ac, nor), pc));
  float d2;
  if (facing < 2.0) {
    d2 = min(min(dcEdgeDist2(ba, pa), dcEdgeDist2(cb, pb)),
             dcEdgeDist2(ac, pc));
  } else {
    float dn = dot(nor, pa);
    d2 = dn * dn / dcDot2(nor);
  }
  return sqrt(d2);
}

// ---- primitive long tail (src/implicit/primitives_tierA/B/C.cpp) ----------
// Bit-faithful ports of the C++ valueAt formulas. length(vec2(a,b)) stands in
// for std::hypot, and 2D length(max(d,0)) for len2max0. Trig primitives take the
// raw angle and compute sin/cos in-shader (the C++ ctor precomputes them; the
// value is identical up to GPU-trig tolerance, like sdCone).
float sdPlane(vec3 p, vec3 n, float o) { return dot(p, normalize(n)) + o; }

float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
  vec3 pa = p - a, ba = b - a;
  float bb = dot(ba, ba);
  float h = bb > 1e-30 ? clamp(dot(pa, ba) / bb, 0.0, 1.0) : 0.0;
  return length(pa - ba * h) - r;
}

float sdCappedCylinder(vec3 p, vec3 a, vec3 b, float r) {
  vec3 ba = b - a, pa = p - a;
  float baba = dot(ba, ba);
  if (baba < 1e-30) return length(p - a) - r;
  float paba = dot(pa, ba);
  float x = length(pa * baba - ba * paba) - r * baba;
  float y = abs(paba - baba * 0.5) - baba * 0.5;
  float x2 = x * x;
  float y2 = y * y * baba;
  float d = (max(x, y) < 0.0) ? -min(x2, y2)
                              : ((x > 0.0 ? x2 : 0.0) + (y > 0.0 ? y2 : 0.0));
  return dcSgn(d) * sqrt(abs(d)) / baba;
}

float sdEllipsoid(vec3 p, vec3 c, vec3 r) {
  vec3 d = p - c;
  float k0 = length(vec3(d.x/r.x, d.y/r.y, d.z/r.z));
  float k1 = length(vec3(d.x/(r.x*r.x), d.y/(r.y*r.y), d.z/(r.z*r.z)));
  if (k1 < 1e-30) return -min(r.x, min(r.y, r.z));
  return k0 * (k0 - 1.0) / k1;
}

float sdBoxFrame(vec3 p, vec3 mn, vec3 mx, float e) {
  vec3 c = (mn + mx) * 0.5, h = (mx - mn) * 0.5;
  vec3 pp = abs(p - c) - h;
  vec3 q = abs(pp + e) - e;
  float s1 = length(max(vec3(pp.x, q.y, q.z), 0.0)) + min(max(pp.x, max(q.y, q.z)), 0.0);
  float s2 = length(max(vec3(q.x, pp.y, q.z), 0.0)) + min(max(q.x, max(pp.y, q.z)), 0.0);
  float s3 = length(max(vec3(q.x, q.y, pp.z), 0.0)) + min(max(q.x, max(q.y, pp.z)), 0.0);
  return min(s1, min(s2, s3));
}

float sdCappedCone(vec3 p, vec3 c, float h, float r1, float r2) {
  vec3 pl = p - c;
  float qx = length(vec2(pl.x, pl.z)), qy = pl.y;
  float k1x = r2, k1y = h;
  float k2x = r2 - r1, k2y = 2.0 * h;
  float cax = qx - min(qx, (qy < 0.0) ? r1 : r2);
  float cay = abs(qy) - h;
  float t = clamp(((k1x - qx)*k2x + (k1y - qy)*k2y) / (k2x*k2x + k2y*k2y), 0.0, 1.0);
  float cbx = qx - k1x + k2x * t;
  float cby = qy - k1y + k2y * t;
  float s = (cbx < 0.0 && cay < 0.0) ? -1.0 : 1.0;
  return s * sqrt(min(cax*cax + cay*cay, cbx*cbx + cby*cby));
}

float sdRoundCone(vec3 p, vec3 a, vec3 b, float r1, float r2) {
  vec3 ba = b - a;
  float l2 = dot(ba, ba);
  if (l2 < 1e-30) return length(p - a) - r1;
  float rr = r1 - r2;
  float a2 = l2 - rr * rr;
  float il2 = 1.0 / l2;
  vec3 pa = p - a;
  float y = dot(pa, ba);
  float z = y - l2;
  vec3 xv = pa * l2 - ba * y;
  float x2 = dot(xv, xv);
  float y2 = y * y * l2;
  float z2 = z * z * l2;
  float k = dcSgn(rr) * rr * rr * x2;
  if (dcSgn(z) * a2 * z2 > k) return sqrt(x2 + z2) * il2 - r2;
  if (dcSgn(y) * a2 * y2 < k) return sqrt(x2 + y2) * il2 - r1;
  return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

float sdInfiniteCylinder(vec3 p, vec3 p0, vec3 dir, float r) {
  vec3 dn = normalize(dir);
  vec3 d = p - p0;
  return length(d - dn * dot(d, dn)) - r;
}

float sdHexPrism(vec3 p, vec3 c, float r, float hl) {
  float kx = -0.8660254, ky = 0.5, kz = 0.57735;
  vec3 pl = abs(p - c);
  float m = 2.0 * min(kx * pl.x + ky * pl.y, 0.0);
  float px = pl.x - m * kx, py = pl.y - m * ky;
  float cx = clamp(px, -kz * r, kz * r);
  float dx = sqrt((px - cx)*(px - cx) + (py - r)*(py - r)) * dcSgn(py - r);
  float dy = pl.z - hl;
  return min(max(dx, dy), 0.0) + length(max(vec2(dx, dy), 0.0));
}

float sdTriPrism(vec3 p, vec3 c, float r, float hl) {
  vec3 pl = p - c;
  vec3 q = abs(pl);
  return max(q.z - hl, max(q.x * 0.866025 + pl.y * 0.5, -pl.y) - r * 0.5);
}

float sdOctahedron(vec3 p, vec3 c, float s) {  // exact (registry default)
  vec3 pl = abs(p - c);
  float m = pl.x + pl.y + pl.z - s;
  vec3 q;
  if (3.0 * pl.x < m) q = pl;
  else if (3.0 * pl.y < m) q = vec3(pl.y, pl.z, pl.x);
  else if (3.0 * pl.z < m) q = vec3(pl.z, pl.x, pl.y);
  else return m * 0.57735027;
  float k = clamp(0.5 * (q.z - q.y + s), 0.0, s);
  return length(vec3(q.x, q.y - s + k, q.z - k));
}

float sdPyramid(vec3 p, vec3 c, float h) {
  vec3 pl = p - c;
  float m2 = h * h + 0.25;
  float ax = abs(pl.x), az = abs(pl.z);
  if (az > ax) { float tmp = ax; ax = az; az = tmp; }
  ax -= 0.5; az -= 0.5;
  float qx = az;
  float qy = h * pl.y - 0.5 * ax;
  float qz = h * ax + 0.5 * pl.y;
  float s = max(-qx, 0.0);
  float t = clamp((qy - 0.5 * az) / (m2 + 0.25), 0.0, 1.0);
  float a = m2 * (qx + s)*(qx + s) + qy * qy;
  float b = m2 * (qx + 0.5*t)*(qx + 0.5*t) + (qy - m2*t)*(qy - m2*t);
  float d2 = (min(qy, -qx*m2 - qy*0.5) > 0.0) ? 0.0 : min(a, b);
  return sqrt((d2 + qz*qz) / m2) * dcSgn(max(qz, -pl.y));
}

float sdSolidAngle(vec3 p, vec3 c, float ang, float ra) {
  float si = sin(ang), co = cos(ang);
  vec3 pl = p - c;
  float qx = length(vec2(pl.x, pl.z)), qy = pl.y;
  float l = length(vec2(qx, qy)) - ra;
  float dt = clamp(qx * si + qy * co, 0.0, ra);
  float m = length(vec2(qx - si * dt, qy - co * dt));
  return max(l, m * dcSgn(co * qx - si * qy));
}

float sdCappedTorus(vec3 p, vec3 c, float ang, float ra, float rb) {
  float si = sin(ang), co = cos(ang);
  vec3 pl = p - c;
  float px = abs(pl.x);
  float k = (co * px > si * pl.y) ? (px * si + pl.y * co) : length(vec2(px, pl.y));
  float dd = px*px + pl.y*pl.y + pl.z*pl.z;
  return sqrt(max(dd + ra*ra - 2.0*ra*k, 0.0)) - rb;
}

float sdLink(vec3 p, vec3 c, float le, float r1, float r2) {
  vec3 pl = p - c;
  float qy = max(abs(pl.y) - le, 0.0);
  float a = length(vec2(pl.x, qy)) - r1;
  return length(vec2(a, pl.z)) - r2;
}

float sdCutSphere(vec3 p, vec3 c, float r, float h) {
  vec3 pl = p - c;
  float w = sqrt(max(r*r - h*h, 0.0));
  float qx = length(vec2(pl.x, pl.z)), qy = pl.y;
  float s = max((h - r)*qx*qx + w*w*(h + r - 2.0*qy), h*qx - w*qy);
  if (s < 0.0) return length(vec2(qx, qy)) - r;
  if (qx < w) return h - qy;
  return length(vec2(qx - w, qy - h));
}

float sdCutHollowSphere(vec3 p, vec3 c, float r, float h, float t) {
  vec3 pl = p - c;
  float w = sqrt(max(r*r - h*h, 0.0));
  float qx = length(vec2(pl.x, pl.z)), qy = pl.y;
  float d = (h*qx < w*qy) ? length(vec2(qx - w, qy - h))
                          : abs(length(vec2(qx, qy)) - r);
  return d - t;
}

float sdDeathStar(vec3 p, vec3 c, float ra, float rb, float dd) {
  vec3 pl = p - c;
  float a = (ra*ra - rb*rb + dd*dd) / (2.0*dd);
  float b = sqrt(max(ra*ra - a*a, 0.0));
  float px = pl.x;
  float py = length(vec2(pl.y, pl.z));
  if (px*b - py*a > dd*max(b - py, 0.0)) return length(vec2(px - a, py - b));
  return max(length(vec2(px, py)) - ra, -(length(vec2(px - dd, py)) - rb));
}

float sdVesica(vec3 p, vec3 a, vec3 b, float w) {
  vec3 mid = (a + b) * 0.5;
  float l = length(b - a);
  if (l < 1e-30) return length(p - a) - w;
  vec3 v = (b - a) / l;
  float y = dot(p - mid, v);
  float qx = length(p - mid - v * y);
  float qy = abs(y);
  float r = 0.5 * l;
  float d = 0.5 * (r*r - w*w) / w;
  float hx, hy, hz;
  if (r * qx < d * (qy - r)) { hx = 0.0; hy = r; hz = 0.0; }
  else { hx = -d; hy = 0.0; hz = d + w; }
  return length(vec2(qx - hx, qy - hy)) - hz;
}

float sdRhombus(vec3 p, vec3 c, float la, float lb, float h, float ra) {
  vec3 pl = abs(p - c);
  float f = clamp((la*pl.x - lb*pl.z + lb*lb) / (la*la + lb*lb), 0.0, 1.0);
  float wx = pl.x - la * f;
  float wz = pl.z - lb * (1.0 - f);
  float qx = length(vec2(wx, wz)) * dcSgn(wx) - ra;
  float qy = pl.y - h;
  return min(max(qx, qy), 0.0) + length(max(vec2(qx, qy), 0.0));
}

float sdVerticalCapsule(vec3 p, vec3 c, float h, float r) {
  vec3 pl = p - c;
  pl.y -= clamp(pl.y, 0.0, h);
  return length(pl) - r;
}

float sdRoundedCylinder(vec3 p, vec3 c, float ra, float rb, float h) {
  vec3 pl = p - c;
  float dx = length(vec2(pl.x, pl.z)) - ra + rb;
  float dy = abs(pl.y) - h + rb;
  return min(max(dx, dy), 0.0) + length(max(vec2(dx, dy), 0.0)) - rb;
}

// Unsigned (open surface) -- pair with an onion to give it an interior.
float sdQuad(vec3 p, vec3 a, vec3 b, vec3 c, vec3 d) {
  vec3 ba = b - a, pa = p - a;
  vec3 cb = c - b, pb = p - b;
  vec3 dc = d - c, pc = p - c;
  vec3 ad = a - d, pd = p - d;
  vec3 nor = cross(ba, ad);
  float facing = dcSgn(dot(cross(ba, nor), pa)) +
                 dcSgn(dot(cross(cb, nor), pb)) +
                 dcSgn(dot(cross(dc, nor), pc)) +
                 dcSgn(dot(cross(ad, nor), pd));
  float d2;
  if (facing < 3.0) {
    d2 = min(min(dcEdgeDist2(ba, pa), dcEdgeDist2(cb, pb)),
             min(dcEdgeDist2(dc, pc), dcEdgeDist2(ad, pd)));
  } else {
    float dn = dot(nor, pa);
    d2 = dn * dn / dcDot2(nor);
  }
  return sqrt(d2);
}

float sdInfiniteCone(vec3 p, vec3 c, float ang) {
  float si = sin(ang), co = cos(ang);
  vec3 pl = p - c;
  float qx = length(vec2(pl.x, pl.z)), qy = -pl.y;
  float m = max(qx * si + qy * co, 0.0);
  float d = length(vec2(qx - si * m, qy - co * m));
  return d * ((qx * co - qy * si < 0.0) ? -1.0 : 1.0);
}

// ---- TPMS (src/implicit/primitives_tpms.cpp); k = 2*pi/wavelength ---------
vec3 dcTpmsQ(vec3 p, vec3 c, float wl) { return (p - c) * (2.0 * DC_PI / wl); }

float sdGyroid(vec3 p, vec3 c, float wl) {
  vec3 q = dcTpmsQ(p, c, wl);
  return sin(q.x) * cos(q.y) + sin(q.y) * cos(q.z) + sin(q.z) * cos(q.x);
}
float sdSchwarzP(vec3 p, vec3 c, float wl) {
  vec3 q = dcTpmsQ(p, c, wl);
  return cos(q.x) + cos(q.y) + cos(q.z);
}
float sdDiamond(vec3 p, vec3 c, float wl) {
  vec3 q = dcTpmsQ(p, c, wl);
  float sx = sin(q.x), cx = cos(q.x);
  float sy = sin(q.y), cy = cos(q.y);
  float sz = sin(q.z), cz = cos(q.z);
  return sx * sy * sz + sx * cy * cz + cx * sy * cz + cx * cy * sz;
}
float sdFischerKoch(vec3 p, vec3 c, float wl) {
  vec3 q = dcTpmsQ(p, c, wl);
  return cos(2.0 * q.x) * sin(q.y) * cos(q.z) +
         cos(q.x) * cos(2.0 * q.y) * sin(q.z) +
         sin(q.x) * cos(q.y) * cos(2.0 * q.z);
}
float sdLidinoid(vec3 p, vec3 c, float wl) {
  vec3 q = dcTpmsQ(p, c, wl);
  float sx = sin(q.x), cx = cos(q.x);
  float sy = sin(q.y), cy = cos(q.y);
  float sz = sin(q.z), cz = cos(q.z);
  float s2x = sin(2.0 * q.x), c2x = cos(2.0 * q.x);
  float s2y = sin(2.0 * q.y), c2y = cos(2.0 * q.y);
  float s2z = sin(2.0 * q.z), c2z = cos(2.0 * q.z);
  float t1 = 0.5 * (s2x * cy * sz + s2y * cz * sx + s2z * cx * sy);
  float t2 = 0.5 * (c2x * c2y + c2y * c2z + c2z * c2x);
  return t1 - t2 + 0.15;
}
float sdNeovius(vec3 p, vec3 c, float wl) {
  vec3 q = dcTpmsQ(p, c, wl);
  float cx = cos(q.x), cy = cos(q.y), cz = cos(q.z);
  return 3.0 * (cx + cy + cz) + 4.0 * cx * cy * cz;
}

// ---- booleans (src/implicit/combinators.cpp) ------------------------------
float opUnion(float a, float b) { return min(a, b); }
float opInter(float a, float b) { return max(a, b); }
float opDiff(float a, float b)  { return max(a, -b); }
float opXor(float a, float b)   { return max(min(a, b), -max(a, b)); }
float opSmoothUnion(float a, float b, float k) {
  float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
  return mix(b, a, h) - k * h * (1.0 - h);
}
float opSmoothInter(float a, float b, float k) {
  float h = clamp(0.5 - 0.5 * (b - a) / k, 0.0, 1.0);
  return mix(b, a, h) + k * h * (1.0 - h);
}
float opSmoothDiff(float a, float b, float k) {  // smax(a, -b, k)
  float h = clamp(0.5 - 0.5 * ((-b) - a) / k, 0.0, 1.0);
  return mix(-b, a, h) + k * h * (1.0 - h);
}

// ---- transform helper (src/implicit/decorators.cpp TransformField) --------
// The localFromWorld matrix is uploaded row-major (transpose=GL_TRUE), so a
// plain m*vec4(p,1) reproduces Mat4::transformPoint.
vec3 dcXform(mat4 m, vec3 p) { return (m * vec4(p, 1.0)).xyz; }

// ---- domain ops (src/implicit/domain_ops.cpp) -----------------------------
// Round ties AWAY from zero, matching C++ std::round (GLSL round() is ties-to-
// even) -- used by repeat / repeat-limited so the tile index matches the field.
float dcRoundTA(float x) { return sign(x) * floor(abs(x) + 0.5); }
// Displace bump patterns (example_common.cpp namedBump); q = frequency * p. The
// per-pattern amplitude divisor is applied at the call site (sine 1, gyroid
// 1.4973, bumps 3) so `amp` is the true peak displacement.
float dcDispSine(vec3 q)   { return sin(q.x) * sin(q.y) * sin(q.z); }
float dcDispGyroid(vec3 q) { return sin(q.x)*cos(q.y) + sin(q.y)*cos(q.z) + sin(q.z)*cos(q.x); }
float dcDispBumps(vec3 q)  { return cos(q.x) + cos(q.y) + cos(q.z); }
)GLSL";

// The full-screen-triangle vertex body (version-independent).
const char* kVertexBody = R"GLSL(
out vec2 vUV;
void main() {
  vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                (gl_VertexID == 2) ? 3.0 : -1.0);
  vUV = p;
  gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

// The fixed sphere-trace framework (camera, FD normals, shading). Calls the
// generated sceneSDF(); the step is scaled by the graph-level uStepScale. Kept
// in lock-step with dualc_raymarch.cpp's hand-written loop.
const char* kTraceFramework = R"GLSL(
in  vec2 vUV;
out vec4 fragColor;

uniform vec3  uCamPos, uCamForward, uCamRight, uCamUp;
uniform float uTanHalfFov, uAspect;
uniform vec3  uBoundsMin, uBoundsMax;
uniform vec3  uLightDir;
uniform float uStepMin, uStepMax, uHmem, uStepScale;
// 1.0 when the whole graph is a true (Lipschitz-1) SDF -- no TPMS / smooth
// boolean / warp -- so the trace can step by the distance directly (plain
// sphere tracing) and skip the per-step gradient sampling. 0.0 keeps the
// gradient-normalized conservative step for non-metric fields.
uniform float uMetricSDF;

// Viewport-only section planes: up to 3 axis-aligned half-spaces. Each uClip[i]
// is (n.xyz, d); kept region is h(p)=dot(n,p)+d <= 0. Intersecting the body with
// the half-spaces is max(sceneSDF, h_i) -- a true (conservative) SDF, so the
// sphere-trace stays safe. uClipMask==0 makes clippedSDF == sceneSDF exactly, so
// with no plane active the image is identical to the un-clipped scene. Pure
// visualization: the codegen, the parity value-shader and the C++ export path
// never see this.
uniform vec4 uClip[3];
uniform int  uClipMask;

float clippedSDF(vec3 p) {
  float d = sceneSDF(p);
  for (int i = 0; i < 3; ++i)
    if ((uClipMask & (1 << i)) != 0)
      d = max(d, dot(uClip[i].xyz, p) + uClip[i].w);
  return d;
}

vec3 sceneGrad(vec3 p) {
  float h = uHmem;
  return vec3(
    clippedSDF(p + vec3(h,0,0)) - clippedSDF(p - vec3(h,0,0)),
    clippedSDF(p + vec3(0,h,0)) - clippedSDF(p - vec3(0,h,0)),
    clippedSDF(p + vec3(0,0,h)) - clippedSDF(p - vec3(0,0,h)));
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

  float t = max(t0, 0.0) + uStepMin;
  float f = clippedSDF(ro + t * rd);
  bool  hit = false;
  vec3  hp  = ro + t * rd;
  if (f <= 0.0) {
    hit = true;
  } else {
    const int MAX_STEPS = 384;
    for (int i = 0; i < MAX_STEPS; ++i) {
      // A true SDF (uMetricSDF==1) sphere-traces by the distance directly; only a
      // non-metric field pays the 6-sample per-step gradient to normalize the
      // step. This is the dominant cost on dense strut/analytic scenes.
      float gm = (uMetricSDF > 0.5)
                   ? 1.0
                   : max(length(sceneGrad(ro + t * rd)) / (2.0 * uHmem), 1e-5);
      float tN = t + clamp(uStepScale * 0.8 * f / gm, uStepMin, uStepMax);
      if (tN > t1) break;
      float fN = clippedSDF(ro + tN * rd);
      if (fN <= 0.0) {
        float ta = t, tb = tN;
        for (int b = 0; b < 20; ++b) {
          float tm = 0.5 * (ta + tb);
          if (clippedSDF(ro + tm * rd) > 0.0) ta = tm; else tb = tm;
        }
        hp = ro + tb * rd; hit = true; break;
      }
      t = tN; f = fN;
    }
  }

  if (!hit) { fragColor = vec4(background(vUV), 1.0); return; }

  // A baked mesh grid is constant (no band voxels) when the bake region lies
  // strictly inside the solid, so the gradient can be identically zero. Face
  // the camera rather than normalize(vec3(0)).
  vec3 g = sceneGrad(hp);
  vec3 n = (dot(g, g) < 1e-30) ? -rd : normalize(g);
  if (dot(n, rd) > 0.0) n = -n;
  float key  = clamp(dot(n,  uLightDir), 0.0, 1.0);
  float fill = clamp(dot(n, -uLightDir), 0.0, 1.0);
  vec3  base = vec3(0.80, 0.82, 0.86);
  // Tint the section cap: the hit lies on an active plane (|h_i| small) AND it is
  // interior body (sceneSDF < 0), not real geometry that merely grazes the plane.
  float capEps = 1.5 * uStepMin;
  for (int i = 0; i < 3; ++i)
    if ((uClipMask & (1 << i)) != 0 &&
        abs(dot(uClip[i].xyz, hp) + uClip[i].w) < capEps &&
        sceneSDF(hp) < -capEps)
      base = vec3(0.86, 0.74, 0.58);  // warm desaturated section face
  vec3  col  = base * (0.16 + 0.84 * key) + vec3(0.10) * fill;
  fragColor  = vec4(pow(col, vec3(1.0/2.2)), 1.0);
}
)GLSL";

// Parity main: evaluate sceneSDF over an N^3 lattice packed into the render
// target, one sample per fragment. The CPU mirrors this index->point mapping.
const char* kValueFramework = R"GLSL(
out float outVal;
uniform vec3 uPMin, uPExt;
uniform int  uGrid, uTexW;
void main() {
  int px = int(gl_FragCoord.x);
  int py = int(gl_FragCoord.y);
  int idx = py * uTexW + px;
  int N = uGrid;
  int ix = idx - (idx / N) * N;
  int iy = (idx / N) - (idx / (N * N)) * N;
  int iz = idx / (N * N);
  vec3 t = (vec3(float(ix), float(iy), float(iz)) + 0.5) / float(N);
  vec3 p = uPMin + t * uPExt;
  outVal = sceneSDF(p);
}
)GLSL";

std::string header(bool es) {
  if (es)
    return "#version 300 es\nprecision highp float;\nprecision highp int;\n"
           "precision highp sampler3D;\n";
  return "#version 330 core\n";
}

// ===========================================================================
// Param readers (mirror the file-local helpers in field_graph.cpp so the GLSL
// path reads a graph identically to the contour path).
// ===========================================================================

// ---------------------------------------------------------------------------
// Unknown-parameter rejection. Every key a node carries must be READ by the op
// that consumes it; anything left over is a typo or a key this op does not
// take, and until 2026-09-11 it was absorbed silently -- `plane(normal=[1,0,0])`
// kept the registry default and morphed along Y while the recipe said X, on the
// CPU and the GPU identically, so the parity gate could not see it. findParam is
// the single read point, so the set of read keys is exact by construction and no
// per-op table has to be kept in sync with the dispatcher. Keyed by node
// address, scoped by an RAII guard so a throw does not leak an entry; recursion
// into children is safe because each node has its own entry.
// ---------------------------------------------------------------------------
thread_local std::map<const GraphNode*, std::set<std::string>> g_readKeys;

struct ReadScope {
  const GraphNode& n;
  ~ReadScope() { g_readKeys.erase(&n); }
};

const ParamValue* findParam(const GraphNode& n, const std::string& key) {
  g_readKeys[&n].insert(key);
  auto it = n.params.find(key);
  return it == n.params.end() ? nullptr : &it->second;
}

void rejectUnreadParams(const GraphNode& n) {
  auto it = g_readKeys.find(&n);
  static const std::set<std::string> none;
  const std::set<std::string>& read = it == g_readKeys.end() ? none : it->second;
  for (const auto& kv : n.params) {
    if (read.count(kv.first)) continue;
    std::string accepted;
    for (const std::string& k : read) accepted += (accepted.empty() ? "" : ", ") + k;
    std::string hint = accepted.empty() ? " (this op takes no named parameters)"
                                        : " (accepted: " + accepted + ")";
    if (findPrimitive(n.op))
      hint = " (a primitive takes its values positionally, in registry order --"
             " see --list -- or as params=[...]" +
             (accepted == "params" ? std::string() : "; named keys: " + accepted) +
             ")";
    throw GraphError("unknown parameter '" + kv.first + "' for '" + n.op + "'" + hint,
                     n.pointer);
  }
}

double numParam(const GraphNode& n, const std::string& key, double def) {
  const ParamValue* p = findParam(n, key);
  if (!p) return def;
  if (p->kind == ParamValue::Kind::Number) return p->num;
  if (p->kind == ParamValue::Kind::Vector && p->vec.size() == 1) return p->vec[0];
  throw GraphError("parameter '" + key + "' must be a number", n.pointer);
}

double requireNum(const GraphNode& n, const std::string& key) {
  if (!findParam(n, key))
    throw GraphError("missing required parameter '" + key + "'", n.pointer);
  return numParam(n, key, 0.0);
}

std::vector<double> vecParam(const GraphNode& n, const std::string& key,
                             std::size_t arity, bool required,
                             const std::vector<double>& def) {
  const ParamValue* p = findParam(n, key);
  if (!p) {
    if (required)
      throw GraphError("missing required parameter '" + key + "'", n.pointer);
    return def;
  }
  if (p->kind != ParamValue::Kind::Vector || p->vec.size() != arity)
    throw GraphError("parameter '" + key + "' must be an array of " +
                         std::to_string(arity) + " numbers",
                     n.pointer);
  return p->vec;
}

dualc::Vector3 vec3Param(const GraphNode& n, const std::string& key,
                         const dualc::Vector3& def) {
  const ParamValue* p = findParam(n, key);
  if (!p) return def;
  std::vector<double> v = vecParam(n, key, 3, false, {def.x, def.y, def.z});
  return dualc::Vector3{v[0], v[1], v[2]};
}

std::string strParam(const GraphNode& n, const std::string& key,
                     const std::string& def) {
  const ParamValue* p = findParam(n, key);
  if (!p) return def;
  if (p->kind != ParamValue::Kind::String)
    throw GraphError("parameter '" + key + "' must be a string", n.pointer);
  return p->str;
}

int axisIndex(const GraphNode& n, const std::string& s) {
  if (s == "x" || s == "0") return 0;
  if (s == "y" || s == "1") return 1;
  if (s == "z" || s == "2") return 2;
  throw GraphError("axis must be \"x\", \"y\" or \"z\"", n.pointer);
}

void requireChildren(const GraphNode& n, std::size_t k) {
  if (n.in.size() != k)
    throw GraphError("'" + n.op + "' expects " + std::to_string(k) +
                         " child node(s), got " + std::to_string(n.in.size()),
                     n.pointer);
}

bool isTpmsOp(const std::string& op) {
  for (const TpmsKind& k : tpmsKinds())
    if (k.name == op) return true;
  return false;
}

bool isStrutOp(const std::string& op) {
  for (const StrutKind& k : strutKinds())
    if (k.name == op) return true;
  return false;
}

// Resolve a primitive's flat parameter vector exactly as field_graph.cpp does:
// registry defaults, grouped semantic keys (the slice subset), then the flat
// `params` override.
struct Slot { std::string key; std::size_t offset; std::size_t arity; };

std::vector<double> primitiveParams(const GraphNode& n) {
  const PrimEntry* e = findPrimitive(n.op);
  std::vector<double> p = e->defaults;
  static const std::map<std::string, std::vector<Slot>> layouts = {
      {"sphere", {{"center", 0, 3}, {"radius", 3, 1}}},
      {"box", {{"min", 0, 3}, {"max", 3, 3}}},
      {"roundbox", {{"min", 0, 3}, {"max", 3, 3}, {"radius", 6, 1}}},
      {"capsule", {{"a", 0, 3}, {"b", 3, 3}, {"radius", 6, 1}}},
      {"cappedcylinder", {{"a", 0, 3}, {"b", 3, 3}, {"radius", 6, 1}}},
      {"torus", {{"center", 0, 3}, {"major", 3, 1}, {"minor", 4, 1}}},
      {"ellipsoid", {{"center", 0, 3}, {"radii", 3, 3}}},
  };
  auto it = layouts.find(n.op);
  if (it != layouts.end()) {
    for (const Slot& s : it->second) {
      if (!findParam(n, s.key)) continue;
      std::vector<double> vals = (s.arity == 1)
          ? std::vector<double>{numParam(n, s.key, 0.0)}
          : vecParam(n, s.key, s.arity, false, {});
      for (std::size_t i = 0; i < s.arity && s.offset + i < p.size(); ++i)
        p[s.offset + i] = vals[i];
    }
  }
  const ParamValue* flat = findParam(n, "params");
  if (flat) {
    if (flat->kind != ParamValue::Kind::Vector)
      throw GraphError("'params' must be a number array", n.pointer);
    for (std::size_t i = 0; i < flat->vec.size() && i < p.size(); ++i)
      p[i] = flat->vec[i];
  }
  return p;
}

// ===========================================================================
// Codegen context + emit walk.
// ===========================================================================

struct Ctx {
  std::ostringstream decls;   // uniform declarations
  std::ostringstream funcs;   // per-node function bodies, children first
  std::vector<UniformBinding> bindings;
  std::vector<MeshTexture> meshes;
  MeshResolver* resolver = nullptr;
  dualc::BBox bounds;
  int gridRes = kDefaultMeshGridRes;
  int counter = 0;
  std::map<std::string, int> memo;  // structural key -> emitted node index, so a
                                    // repeated subtree (DAG dedup) reuses its fN.
};

std::string fname(int idx) { return "f" + std::to_string(idx); }

// Declare an editable uniform and register it in the binding table. Returns the
// uniform name to splice into the emitted call.
std::string declScalar(Ctx& c, int idx, const std::string& key, double v,
                       const GraphNode& n) {
  const std::string name = "u_n" + std::to_string(idx) + "_" + key;
  c.decls << "uniform float " << name << ";\n";
  c.bindings.push_back({name, "float", {float(v)}, n.pointer, key});
  return name;
}

std::string declVec3(Ctx& c, int idx, const std::string& key,
                     const dualc::Vector3& v, const GraphNode& n) {
  const std::string name = "u_n" + std::to_string(idx) + "_" + key;
  c.decls << "uniform vec3 " << name << ";\n";
  c.bindings.push_back(
      {name, "vec3", {float(v.x), float(v.y), float(v.z)}, n.pointer, key});
  return name;
}

std::string declMat4(Ctx& c, int idx, const std::string& key,
                     const dualc::Mat4& m, const GraphNode& n) {
  const std::string name = "u_n" + std::to_string(idx) + "_" + key;
  c.decls << "uniform mat4 " << name << ";\n";
  std::vector<float> v(16);
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) v[i * 4 + j] = float(m.m[i][j]);  // row-major
  c.bindings.push_back({name, "mat4", v, n.pointer, key});
  return name;
}

int emitNode(const GraphNode& n, Ctx& c);      // memoizing wrapper (DAG dedup)
int emitNodeImpl(const GraphNode& n, Ctx& c);  // the per-op emit dispatcher
float nodeFeatureScale(const GraphNode& n);  // defined below; used by normalize

// A structural key for DAG dedup: two nodes that would emit an identical fN
// (same op, same params, structurally-equal children) share one key. The
// codegen depends only on (op, params, children) -- the Ctx-level bounds/gridRes
// are constant per compile -- so the key need not include them. Doubles are
// written at full (round-trip) precision so distinct values never collide and
// equal values never split. Works for today's tree-only wire-format (a
// duplicated subtree) and, unchanged, for a future literal-DAG wire-format.
std::string nodeKey(const GraphNode& n) {
  std::ostringstream k;
  k.precision(17);  // round-trip precision for IEEE-754 double
  k << n.op << '{';
  for (const auto& kv : n.params) {  // params is a std::map: already sorted
    k << kv.first << '=';
    const ParamValue& p = kv.second;
    switch (p.kind) {
      case ParamValue::Kind::Number: k << 'N' << p.num; break;
      case ParamValue::Kind::String: k << 'S' << p.str; break;
      case ParamValue::Kind::Vector:
        k << 'V';
        for (double d : p.vec) k << d << ',';
        break;
    }
    k << ';';
  }
  k << "}(";
  for (const GraphNode& ch : n.in) k << nodeKey(ch) << ',';
  k << ')';
  return k.str();
}

int emitPrimitive(const GraphNode& n, Ctx& c) {
  const std::vector<double> p = primitiveParams(n);
  const int idx = c.counter;  // reserve our index before declaring uniforms
  c.counter++;
  std::string call;
  if (n.op == "sphere") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string r = declScalar(c, idx, "radius", p[3], n);
    call = "sdSphere(p, " + ctr + ", " + r + ")";
  } else if (n.op == "box") {
    std::string mn = declVec3(c, idx, "min", {p[0], p[1], p[2]}, n);
    std::string mx = declVec3(c, idx, "max", {p[3], p[4], p[5]}, n);
    call = "sdBox(p, " + mn + ", " + mx + ")";
  } else if (n.op == "roundbox") {
    std::string mn = declVec3(c, idx, "min", {p[0], p[1], p[2]}, n);
    std::string mx = declVec3(c, idx, "max", {p[3], p[4], p[5]}, n);
    std::string r = declScalar(c, idx, "radius", p[6], n);
    call = "sdRoundBox(p, " + mn + ", " + mx + ", " + r + ")";
  } else if (n.op == "torus") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string maj = declScalar(c, idx, "major", p[3], n);
    std::string min_ = declScalar(c, idx, "minor", p[4], n);
    call = "sdTorus(p, " + ctr + ", " + maj + ", " + min_ + ")";
  } else if (n.op == "cone") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string ang = declScalar(c, idx, "angleRad", p[3], n);
    std::string h = declScalar(c, idx, "height", p[4], n);
    call = "sdCone(p, " + ctr + ", " + ang + ", " + h + ")";
  } else if (n.op == "triangle") {
    std::string a = declVec3(c, idx, "a", {p[0], p[1], p[2]}, n);
    std::string b = declVec3(c, idx, "b", {p[3], p[4], p[5]}, n);
    std::string cc = declVec3(c, idx, "c", {p[6], p[7], p[8]}, n);
    call = "sdTriangle(p, " + a + ", " + b + ", " + cc + ")";
  } else if (n.op == "plane") {
    std::string nrm = declVec3(c, idx, "normal", {p[0], p[1], p[2]}, n);
    std::string o = declScalar(c, idx, "offset", p[3], n);
    call = "sdPlane(p, " + nrm + ", " + o + ")";
  } else if (n.op == "capsule") {
    std::string a = declVec3(c, idx, "a", {p[0], p[1], p[2]}, n);
    std::string b = declVec3(c, idx, "b", {p[3], p[4], p[5]}, n);
    std::string r = declScalar(c, idx, "radius", p[6], n);
    call = "sdCapsule(p, " + a + ", " + b + ", " + r + ")";
  } else if (n.op == "cappedcylinder") {
    std::string a = declVec3(c, idx, "a", {p[0], p[1], p[2]}, n);
    std::string b = declVec3(c, idx, "b", {p[3], p[4], p[5]}, n);
    std::string r = declScalar(c, idx, "radius", p[6], n);
    call = "sdCappedCylinder(p, " + a + ", " + b + ", " + r + ")";
  } else if (n.op == "ellipsoid") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string rad = declVec3(c, idx, "radii", {p[3], p[4], p[5]}, n);
    call = "sdEllipsoid(p, " + ctr + ", " + rad + ")";
  } else if (n.op == "boxframe") {
    std::string mn = declVec3(c, idx, "min", {p[0], p[1], p[2]}, n);
    std::string mx = declVec3(c, idx, "max", {p[3], p[4], p[5]}, n);
    std::string e = declScalar(c, idx, "thickness", p[6], n);
    call = "sdBoxFrame(p, " + mn + ", " + mx + ", " + e + ")";
  } else if (n.op == "cappedcone") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string h = declScalar(c, idx, "height", p[3], n);
    std::string r1 = declScalar(c, idx, "radiusLow", p[4], n);
    std::string r2 = declScalar(c, idx, "radiusHigh", p[5], n);
    call = "sdCappedCone(p, " + ctr + ", " + h + ", " + r1 + ", " + r2 + ")";
  } else if (n.op == "roundcone") {
    std::string a = declVec3(c, idx, "a", {p[0], p[1], p[2]}, n);
    std::string b = declVec3(c, idx, "b", {p[3], p[4], p[5]}, n);
    std::string r1 = declScalar(c, idx, "radiusA", p[6], n);
    std::string r2 = declScalar(c, idx, "radiusB", p[7], n);
    call = "sdRoundCone(p, " + a + ", " + b + ", " + r1 + ", " + r2 + ")";
  } else if (n.op == "infinitecylinder") {
    std::string ap = declVec3(c, idx, "axisPoint", {p[0], p[1], p[2]}, n);
    std::string ad = declVec3(c, idx, "axisDir", {p[3], p[4], p[5]}, n);
    std::string r = declScalar(c, idx, "radius", p[6], n);
    call = "sdInfiniteCylinder(p, " + ap + ", " + ad + ", " + r + ")";
  } else if (n.op == "hexprism") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string r = declScalar(c, idx, "radius", p[3], n);
    std::string hl = declScalar(c, idx, "halfLength", p[4], n);
    call = "sdHexPrism(p, " + ctr + ", " + r + ", " + hl + ")";
  } else if (n.op == "triprism") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string r = declScalar(c, idx, "radius", p[3], n);
    std::string hl = declScalar(c, idx, "halfLength", p[4], n);
    call = "sdTriPrism(p, " + ctr + ", " + r + ", " + hl + ")";
  } else if (n.op == "octahedron") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string s = declScalar(c, idx, "size", p[3], n);
    call = "sdOctahedron(p, " + ctr + ", " + s + ")";
  } else if (n.op == "pyramid") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string h = declScalar(c, idx, "height", p[3], n);
    call = "sdPyramid(p, " + ctr + ", " + h + ")";
  } else if (n.op == "solidangle") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string ang = declScalar(c, idx, "angleRad", p[3], n);
    std::string r = declScalar(c, idx, "radius", p[4], n);
    call = "sdSolidAngle(p, " + ctr + ", " + ang + ", " + r + ")";
  } else if (n.op == "cappedtorus") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string ang = declScalar(c, idx, "angleRad", p[3], n);
    std::string ra = declScalar(c, idx, "major", p[4], n);
    std::string rb = declScalar(c, idx, "minor", p[5], n);
    call = "sdCappedTorus(p, " + ctr + ", " + ang + ", " + ra + ", " + rb + ")";
  } else if (n.op == "link") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string le = declScalar(c, idx, "halfLength", p[3], n);
    std::string ra = declScalar(c, idx, "major", p[4], n);
    std::string rb = declScalar(c, idx, "minor", p[5], n);
    call = "sdLink(p, " + ctr + ", " + le + ", " + ra + ", " + rb + ")";
  } else if (n.op == "cutsphere") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string r = declScalar(c, idx, "radius", p[3], n);
    std::string h = declScalar(c, idx, "cutHeight", p[4], n);
    call = "sdCutSphere(p, " + ctr + ", " + r + ", " + h + ")";
  } else if (n.op == "cuthollowsphere") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string r = declScalar(c, idx, "radius", p[3], n);
    std::string h = declScalar(c, idx, "cutHeight", p[4], n);
    std::string t = declScalar(c, idx, "thickness", p[5], n);
    call = "sdCutHollowSphere(p, " + ctr + ", " + r + ", " + h + ", " + t + ")";
  } else if (n.op == "deathstar") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string ra = declScalar(c, idx, "radiusMain", p[3], n);
    std::string rb = declScalar(c, idx, "radiusBite", p[4], n);
    std::string d = declScalar(c, idx, "distance", p[5], n);
    call = "sdDeathStar(p, " + ctr + ", " + ra + ", " + rb + ", " + d + ")";
  } else if (n.op == "vesica") {
    std::string a = declVec3(c, idx, "a", {p[0], p[1], p[2]}, n);
    std::string b = declVec3(c, idx, "b", {p[3], p[4], p[5]}, n);
    std::string w = declScalar(c, idx, "width", p[6], n);
    call = "sdVesica(p, " + a + ", " + b + ", " + w + ")";
  } else if (n.op == "rhombus") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string la = declScalar(c, idx, "la", p[3], n);
    std::string lb = declScalar(c, idx, "lb", p[4], n);
    std::string h = declScalar(c, idx, "height", p[5], n);
    std::string ra = declScalar(c, idx, "cornerRadius", p[6], n);
    call = "sdRhombus(p, " + ctr + ", " + la + ", " + lb + ", " + h + ", " + ra + ")";
  } else if (n.op == "verticalcapsule") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string h = declScalar(c, idx, "height", p[3], n);
    std::string r = declScalar(c, idx, "radius", p[4], n);
    call = "sdVerticalCapsule(p, " + ctr + ", " + h + ", " + r + ")";
  } else if (n.op == "roundedcylinder") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string ra = declScalar(c, idx, "radius", p[3], n);
    std::string rb = declScalar(c, idx, "roundRadius", p[4], n);
    std::string h = declScalar(c, idx, "halfHeight", p[5], n);
    call = "sdRoundedCylinder(p, " + ctr + ", " + ra + ", " + rb + ", " + h + ")";
  } else if (n.op == "quad") {
    std::string a = declVec3(c, idx, "a", {p[0], p[1], p[2]}, n);
    std::string b = declVec3(c, idx, "b", {p[3], p[4], p[5]}, n);
    std::string cc = declVec3(c, idx, "c", {p[6], p[7], p[8]}, n);
    std::string d = declVec3(c, idx, "d", {p[9], p[10], p[11]}, n);
    call = "sdQuad(p, " + a + ", " + b + ", " + cc + ", " + d + ")";
  } else if (n.op == "infinitecone") {
    std::string ctr = declVec3(c, idx, "center", {p[0], p[1], p[2]}, n);
    std::string ang = declScalar(c, idx, "angleRad", p[3], n);
    call = "sdInfiniteCone(p, " + ctr + ", " + ang + ")";
  } else {
    throw GraphError("GLSL codegen does not support primitive '" + n.op + "'",
                     n.pointer);
  }
  c.funcs << "float " << fname(idx) << "(vec3 p){ return " << call << "; }\n";
  return idx;
}

int emitTpms(const GraphNode& n, Ctx& c) {
  const int idx = c.counter;
  c.counter++;
  std::string ctr =
      declVec3(c, idx, "center", vec3Param(n, "center", {0, 0, 0}), n);
  std::string wl = declScalar(c, idx, "wavelength", numParam(n, "wavelength", 1.0), n);
  static const std::map<std::string, std::string> fn = {
      {"gyroid", "sdGyroid"},     {"schwarz-p", "sdSchwarzP"},
      {"diamond", "sdDiamond"},   {"fischer-koch", "sdFischerKoch"},
      {"lidinoid", "sdLidinoid"}, {"neovius", "sdNeovius"},
  };
  c.funcs << "float " << fname(idx) << "(vec3 p){ return " << fn.at(n.op)
          << "(p, " << ctr << ", " << wl << "); }\n";
  return idx;
}

// Strut-lattice sources: the min over the unit-cell capsules (sdCapsule) after
// the same single round-fold RepeatField uses, translated by `center`. The unit
// cell coords come from strutCellSegments at wavelength 1 (h = 0.5) and scale
// linearly with the wavelength uniform, so all three params stay live-editable.
// Both this and the C++ path evaluate ONLY the nearest tile (single fold), so
// they match to float precision -- dualc_glsl_parity gates it -- and the
// tiled==explicit-union oracle (test_strut_lattice.cpp) certifies the fold.
int emitStrut(const GraphNode& n, Ctx& c) {
  const int idx = c.counter++;
  std::string ctr =
      declVec3(c, idx, "center", vec3Param(n, "center", {0, 0, 0}), n);
  std::string wl =
      declScalar(c, idx, "wavelength", numParam(n, "wavelength", 1.0), n);
  const double radius = numParam(n, "radius", 0.1);
  const double nodeRadius = numParam(n, "nodeRadius", radius);
  const bool tapered = (nodeRadius != radius);
  std::string r = declScalar(c, idx, "radius", radius, n);
  // The nodeRadius uniform is declared only on the tapered path, so an untapered
  // graph emits the exact same GLSL as before (capsule loop, radius uniform).
  std::string nr = tapered ? declScalar(c, idx, "nodeRadius", nodeRadius, n) : "";
  const auto segs = strutCellSegments(n.op, 1.0);
  // Format a coord as a GLSL float literal (append ".0" so vec3(0,..) is float).
  auto g = [](double v) {
    std::ostringstream o;
    o << v;
    std::string s = o.str();
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
      s += ".0";
    return s;
  };
  auto v3 = [&](const dualc::Vector3& p) {
    std::ostringstream o;
    o << wl << "*vec3(" << g(p.x) << "," << g(p.y) << "," << g(p.z) << ")";
    return o.str();
  };
  std::ostringstream b;
  b << "float " << fname(idx) << "(vec3 p){\n";
  b << "  vec3 pl = p - " << ctr << ";\n";
  b << "  vec3 q = pl - " << wl << " * vec3(dcRoundTA(pl.x/" << wl
    << "), dcRoundTA(pl.y/" << wl << "), dcRoundTA(pl.z/" << wl << "));\n";
  b << "  float d = 1e30;\n";
  for (const auto& s : segs) {
    if (!tapered) {
      b << "  d = min(d, sdCapsule(q, " << v3(s.first) << ", " << v3(s.second)
        << ", " << r << "));\n";
    } else {
      // Split the segment at its midpoint into two round cones (fat nodeRadius at
      // each end-node, thin radius at mid) -- the GPU counterpart of the tapered
      // makeStrutLattice cell.
      const dualc::Vector3 mid = (s.first + s.second) * 0.5;
      b << "  d = min(d, sdRoundCone(q, " << v3(s.first) << ", " << v3(mid)
        << ", " << nr << ", " << r << "));\n";
      b << "  d = min(d, sdRoundCone(q, " << v3(mid) << ", " << v3(s.second)
        << ", " << r << ", " << nr << "));\n";
    }
  }
  b << "  return d;\n}\n";
  c.funcs << b.str();
  return idx;
}

// Shared tail for the mesh/winding sources: a baked GridField -> one sampler3D
// (a MeshTexture the app uploads) + the texture()-as-SDF lookup function. Both
// emitMesh and emitWinding feed their bake through here so the dedup memo, the
// binding/texture plumbing, and the fN shape are identical.
int emitBakedGrid(const dualc::GridField* grid, Ctx& c) {
  const int idx = c.counter++;
  const std::string base = "u_n" + std::to_string(idx);
  MeshTexture mt;
  mt.samplerName = base + "_tex";
  mt.resolution = grid->resolution();
  mt.region = grid->bounds();
  mt.values = grid->values();
  c.meshes.push_back(std::move(mt));

  c.decls << "uniform sampler3D " << base << "_tex;\n"
          << "uniform vec3 " << base << "_texMin;\n"
          << "uniform vec3 " << base << "_texScale;\n"
          << "uniform vec3 " << base << "_texDim;\n";
  c.funcs << "float " << fname(idx) << "(vec3 p){\n"
          << "  vec3 g = (p - " << base << "_texMin) * " << base << "_texScale;\n"
          << "  vec3 tc = (g + 0.5) / " << base << "_texDim;\n"
          << "  return texture(" << base << "_tex, tc).r;\n}\n";
  return idx;
}

int emitMesh(const GraphNode& n, Ctx& c) {
  std::string path = strParam(n, "path", "");
  if (path.empty()) throw GraphError("'mesh' requires a 'path'", n.pointer);
  std::string sign = strParam(n, "sign", "parity");
  dualc::SignMethod sm;
  if (sign == "parity") sm = dualc::SignMethod::WINDING_NUMBER;
  else if (sign == "pseudonormal") sm = dualc::SignMethod::PSEUDONORMAL;
  else if (sign == "gwn")
    throw GraphError("mesh sign 'gwn' is not supported by 'mesh'; a 'winding' "
                     "node is not yet supported by the GLSL codegen",
                     n.pointer);
  else throw GraphError("mesh sign must be 'parity' or 'pseudonormal'", n.pointer);
  std::string normals = strParam(n, "normals", "smooth");
  bool interp;
  if (normals == "smooth") interp = true;
  else if (normals == "sharp") interp = false;
  else throw GraphError("mesh normals must be 'smooth' or 'sharp'", n.pointer);

  MeshHandle h = c.resolver->resolve(path);
  dualc::MeshSource src(*h.mesh, *h.geom, interp, sm);
  const dualc::Vector3 e = c.bounds.extent();
  const double maxCell =
      std::max({e.x, e.y, e.z}) / std::max(c.gridRes - 1, 1);
  dualc::FieldPtr gridPtr = src.bakeToGrid(
      c.bounds, dualc::Vector3i{c.gridRes, c.gridRes, c.gridRes}, 3.0 * maxCell);
  auto* grid = dynamic_cast<const dualc::GridField*>(gridPtr.get());
  if (!grid)
    throw GraphError("internal: mesh bake did not return a GridField", n.pointer);
  return emitBakedGrid(grid, c);
}

// winding (generalized winding number): a mesh-soup source. Unlike `mesh` it is
// NOT a metric SDF and has no narrow band, so it bakes via the generic
// full-lattice bakeToGrid(field, ...) rather than MeshSource's banded one. The
// field's sign convention (negative inside) matches MeshSource, so the baked grid
// feeds the same texture()-as-SDF emission with no sign flip.
int emitWinding(const GraphNode& n, Ctx& c) {
  std::string path = strParam(n, "path", "");
  if (path.empty())
    throw GraphError("'winding' requires a 'path'; id-based in-memory sources are "
                     "not supported by the GLSL preview (use a file path)",
                     n.pointer);
  MeshHandle h = c.resolver->resolve(path);
  dualc::FieldPtr wf = dualc::windingNumberField(*h.mesh, *h.geom);
  dualc::FieldPtr gridPtr = dualc::bakeToGrid(
      *wf, c.bounds, dualc::Vector3i{c.gridRes, c.gridRes, c.gridRes});
  auto* grid = dynamic_cast<const dualc::GridField*>(gridPtr.get());
  if (!grid)
    throw GraphError("internal: winding bake did not return a GridField",
                     n.pointer);
  return emitBakedGrid(grid, c);
}

// translate/rotate/transform all lower to a TransformField; emit the
// localFromWorld matrix (= worldFromLocal.inverseRigid()) and warp the point.
int emitTransform(const GraphNode& n, Ctx& c, const dualc::Mat4& worldFromLocal) {
  requireChildren(n, 1);
  const int child = emitNode(n.in[0], c);
  const int idx = c.counter++;
  std::string mat = declMat4(c, idx, "mat", worldFromLocal.inverseRigid(), n);
  c.funcs << "float " << fname(idx) << "(vec3 p){ return " << fname(child)
          << "(dcXform(" << mat << ", p)); }\n";
  return idx;
}

// twist/bend warp the point with the axis baked in (structural) and the rate as
// an editable uniform -- avoids dynamic vector indexing in GLSL.
int emitWarp(const GraphNode& n, Ctx& c, bool bend) {
  requireChildren(n, 1);
  const int child = emitNode(n.in[0], c);
  const double rate =
      bend ? requireNum(n, "curvature") : requireNum(n, "radiansPerUnit");
  const int axis = axisIndex(n, strParam(n, "axis", "x"));
  const char comp[3] = {'x', 'y', 'z'};
  const char A = comp[axis], U = comp[(axis + 1) % 3], V = comp[(axis + 2) % 3];
  const int idx = c.counter++;
  std::string k =
      declScalar(c, idx, bend ? "curvature" : "radiansPerUnit", rate, n);
  std::ostringstream b;
  b << "float " << fname(idx) << "(vec3 p){\n";
  b << "  float ang = " << k << " * p." << A << ";\n";
  b << "  float cc = cos(ang), ss = sin(ang);\n";
  b << "  vec3 q = p;\n";
  if (!bend) {
    // Twist: rotate the (U,V) plane; the axis coord is unchanged.
    b << "  q." << U << " = cc * p." << U << " - ss * p." << V << ";\n";
    b << "  q." << V << " = ss * p." << U << " + cc * p." << V << ";\n";
  } else {
    // Bend: rotate the (axis,U) plane; the V coord is unchanged.
    b << "  q." << A << " = cc * p." << A << " - ss * p." << U << ";\n";
    b << "  q." << U << " = ss * p." << A << " + cc * p." << U << ";\n";
  }
  b << "  return " << fname(child) << "(q);\n}\n";
  c.funcs << b.str();
  return idx;
}

int emitBoolean(const GraphNode& n, Ctx& c, BoolOp op) {
  requireChildren(n, 2);
  const int a = emitNode(n.in[0], c);
  const int b = emitNode(n.in[1], c);
  const int idx = c.counter++;
  const std::string ca = fname(a) + "(p)", cb = fname(b) + "(p)";
  std::string expr;
  switch (op) {
    case BoolOp::Union:        expr = "opUnion(" + ca + ", " + cb + ")"; break;
    case BoolOp::Intersection: expr = "opInter(" + ca + ", " + cb + ")"; break;
    case BoolOp::Difference:   expr = "opDiff(" + ca + ", " + cb + ")"; break;
    case BoolOp::Xor:          expr = "opXor(" + ca + ", " + cb + ")"; break;
    case BoolOp::SmoothUnion:
    case BoolOp::SmoothIntersection:
    case BoolOp::SmoothDifference: {
      std::string k = declScalar(c, idx, "k", numParam(n, "k", 0.25), n);
      const char* h = op == BoolOp::SmoothUnion ? "opSmoothUnion"
                    : op == BoolOp::SmoothIntersection ? "opSmoothInter"
                                                       : "opSmoothDiff";
      expr = std::string(h) + "(" + ca + ", " + cb + ", " + k + ")";
      break;
    }
  }
  c.funcs << "float " << fname(idx) << "(vec3 p){ return " << expr << "; }\n";
  return idx;
}

// Memoizing wrapper: emit each unique node once, then reuse its fN everywhere it
// recurs. The emit appends the function to c.funcs BEFORE the memo entry is set,
// so any later reuse references a function already defined earlier in the source
// -- children-before-parents topological order (GLSL has no forward decls) is
// preserved automatically. Dedup is value-preserving: sceneSDF is byte-identical
// to the un-deduped walk, it just drops duplicate functions / uniforms (and, for
// meshes, duplicate bakes + sampler3D textures via emitMesh).
int emitNode(const GraphNode& n, Ctx& c) {
  const std::string key = nodeKey(n);
  auto it = c.memo.find(key);
  if (it != c.memo.end()) return it->second;
  // The unread-key check runs only on this, the real emission path: a memo hit
  // above is a value-identical node whose params were already validated.
  ReadScope scope{n};
  const int idx = emitNodeImpl(n, c);
  rejectUnreadParams(n);
  c.memo[key] = idx;
  return idx;
}

int emitNodeImpl(const GraphNode& n, Ctx& c) {
  const std::string& op = n.op;

  // 1. Booleans (two children).
  BoolOp bop;
  if (parseBoolOp(op, bop)) return emitBoolean(n, c, bop);

  // 2. TPMS sources.
  if (isTpmsOp(op)) return emitTpms(n, c);

  // 2b. Strut-lattice sources (wireframe crystals).
  if (isStrutOp(op)) return emitStrut(n, c);

  // 3. Mesh sources.
  if (op == "mesh") return emitMesh(n, c);
  if (op == "winding") return emitWinding(n, c);

  // 4. Decorators not in the PostOp set (one child).
  if (op == "normalize") {
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    const std::string fc = fname(child);
    // FD |grad| step. The C++ NormalizedField (decorators.cpp) uses a fixed 1e-4
    // in *double* precision, but in the shader's *float32* a step that small
    // loses the gradient to catastrophic cancellation -> noisy |grad| -> a
    // "sandy"/speckled GPU surface. Use a feature-relative step instead -- the
    // same quantity the marcher feeds uHmem (min(diag*0.002, feat*0.003)), where
    // feat is the child subtree's characteristic length -- so the normalize
    // gradient is sampled over a float32-safe baseline that scales with the
    // lattice. This matches dualc_raymarch's clean result exactly (its tpmsGradMag
    // uses uHmem too). The eps floor stays 1e-9 to mirror NormalizedField::kEps;
    // the resulting preview-vs-export divergence is FD-tier and is gated by
    // dualc_glsl_parity.
    const double diag = c.bounds.isValid() ? c.bounds.extent().norm() : 1.0;
    const float fs = nodeFeatureScale(n.in[0]);
    const double feat = std::isfinite(fs) ? std::min(double(fs), diag) : diag;
    const double e = std::min(diag * 0.002, feat * 0.003);
    std::ostringstream b;
    b << "  float e = " << e << ";\n"
      << "  float dx = " << fc << "(vec3(p.x+e,p.y,p.z)) - " << fc << "(vec3(p.x-e,p.y,p.z));\n"
      << "  float dy = " << fc << "(vec3(p.x,p.y+e,p.z)) - " << fc << "(vec3(p.x,p.y-e,p.z));\n"
      << "  float dz = " << fc << "(vec3(p.x,p.y,p.z+e)) - " << fc << "(vec3(p.x,p.y,p.z-e));\n"
      << "  float gm = length(vec3(dx,dy,dz)) / (2.0*e);\n"
      << "  return " << fc << "(p) / max(gm, 1e-9);\n";
    c.funcs << "float " << fname(idx) << "(vec3 p){\n" << b.str() << "}\n";
    return idx;
  }
  if (op == "transform") {
    dualc::Mat4 m{};
    std::vector<double> v = vecParam(n, "matrix", 16, true, {});
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) m.m[i][j] = v[std::size_t(i) * 4 + j];
    return emitTransform(n, c, m);
  }
  if (op == "translate") {
    std::vector<double> by = vecParam(n, "by", 3, true, {});
    return emitTransform(n, c,
                         dualc::Mat4::translation({by[0], by[1], by[2]}));
  }
  if (op == "rotate") {
    std::vector<double> ax = vecParam(n, "axis", 3, true, {});
    double deg = requireNum(n, "degrees");
    return emitTransform(
        n, c,
        dualc::Mat4::rotation({ax[0], ax[1], ax[2]},
                              deg * 3.14159265358979323846 / 180.0));
  }

  // 5. PostOp-style decorators / warps (one child).
  if (op == "offset" || op == "round") {  // both lower to offsetOf
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    std::string r = declScalar(c, idx, "r", requireNum(n, "r"), n);
    c.funcs << "float " << fname(idx) << "(vec3 p){ return " << fname(child)
            << "(p) - " << r << "; }\n";
    return idx;
  }
  if (op == "onion") {
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    std::string t = declScalar(c, idx, "thickness", requireNum(n, "thickness"), n);
    c.funcs << "float " << fname(idx) << "(vec3 p){ return abs(" << fname(child)
            << "(p)) - " << t << "; }\n";
    return idx;
  }
  if (op == "graded-onion") {
    requireChildren(n, 2);
    const int base = emitNode(n.in[0], c);
    const int ctrl = emitNode(n.in[1], c);
    const int idx = c.counter++;
    std::string t1 = declScalar(c, idx, "t1", requireNum(n, "t1"), n);
    std::string t2 = declScalar(c, idx, "t2", requireNum(n, "t2"), n);
    std::string d0 = declScalar(c, idx, "d0", numParam(n, "d0", 0.0), n);
    std::string d1 = declScalar(c, idx, "d1", requireNum(n, "d1"), n);
    // t(p) = mix(t1,t2, clamp((control-d0)/(d1-d0),0,1)); guard d1==d0.
    c.funcs << "float " << fname(idx) << "(vec3 p){ float dn = " << d1 << " - "
            << d0 << "; dn = abs(dn) < 1e-9 ? (dn < 0.0 ? -1e-9 : 1e-9) : dn;"
            << " float u = clamp((" << fname(ctrl) << "(p) - " << d0
            << ") / dn, 0.0, 1.0); float t = mix(" << t1 << ", " << t2
            << ", u); return abs(" << fname(base) << "(p)) - t; }\n";
    return idx;
  }
  if (op == "graded-offset") {
    // Same control-ramp as graded-onion, but inflates the solid: base - t
    // (drops the abs), so positive t grows every strut/solid outward.
    requireChildren(n, 2);
    const int base = emitNode(n.in[0], c);
    const int ctrl = emitNode(n.in[1], c);
    const int idx = c.counter++;
    std::string t1 = declScalar(c, idx, "t1", requireNum(n, "t1"), n);
    std::string t2 = declScalar(c, idx, "t2", requireNum(n, "t2"), n);
    std::string d0 = declScalar(c, idx, "d0", numParam(n, "d0", 0.0), n);
    std::string d1 = declScalar(c, idx, "d1", requireNum(n, "d1"), n);
    c.funcs << "float " << fname(idx) << "(vec3 p){ float dn = " << d1 << " - "
            << d0 << "; dn = abs(dn) < 1e-9 ? (dn < 0.0 ? -1e-9 : 1e-9) : dn;"
            << " float u = clamp((" << fname(ctrl) << "(p) - " << d0
            << ") / dn, 0.0, 1.0); float t = mix(" << t1 << ", " << t2
            << ", u); return " << fname(base) << "(p) - t; }\n";
    return idx;
  }
  if (op == "mix") {
    // Three-child morph: value = mix(A(p), B(p), w), w the same control-ramp as
    // the graded ops. Guard hi==lo the same way. Emits both child subtrees.
    requireChildren(n, 3);
    const int a = emitNode(n.in[0], c);
    const int b = emitNode(n.in[1], c);
    const int ctrl = emitNode(n.in[2], c);
    const int idx = c.counter++;
    std::string lo = declScalar(c, idx, "lo", numParam(n, "lo", 0.0), n);
    std::string hi = declScalar(c, idx, "hi", requireNum(n, "hi"), n);
    c.funcs << "float " << fname(idx) << "(vec3 p){ float dn = " << hi << " - "
            << lo << "; dn = abs(dn) < 1e-9 ? (dn < 0.0 ? -1e-9 : 1e-9) : dn;"
            << " float u = clamp((" << fname(ctrl) << "(p) - " << lo
            << ") / dn, 0.0, 1.0); return mix(" << fname(a) << "(p), "
            << fname(b) << "(p), u); }\n";
    return idx;
  }
  if (op == "scale") {
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    std::string s = declScalar(c, idx, "s", numParam(n, "s", 1.0), n);
    c.funcs << "float " << fname(idx) << "(vec3 p){ float s = " << s
            << "; return s * " << fname(child) << "(p / s); }\n";
    return idx;
  }
  if (op == "elongate") {
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    std::vector<double> hv = vecParam(n, "h", 3, true, {});
    std::string h = declVec3(c, idx, "h", {hv[0], hv[1], hv[2]}, n);
    c.funcs << "float " << fname(idx) << "(vec3 p){ return " << fname(child)
            << "(p - clamp(p, -" << h << ", " << h << ")); }\n";
    return idx;
  }
  if (op == "twist") return emitWarp(n, c, false);
  if (op == "bend") return emitWarp(n, c, true);
  if (op == "mirror") {
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    std::vector<double> nv = vecParam(n, "normal", 3, true, {});
    std::string nrm = declVec3(c, idx, "normal", {nv[0], nv[1], nv[2]}, n);
    // C++ MirrorField unit-normalises the plane normal in its ctor, so the GLSL
    // must too; fold = p - 2*n*min(dot(p,n),0).
    c.funcs << "float " << fname(idx) << "(vec3 p){ vec3 nn = normalize(" << nrm
            << "); return " << fname(child)
            << "(p - nn * (2.0 * min(dot(p, nn), 0.0))); }\n";
    return idx;
  }
  if (op == "repeat") {
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    std::vector<double> pv = vecParam(n, "period", 3, true, {});
    std::string per = declVec3(c, idx, "period", {pv[0], pv[1], pv[2]}, n);
    // Per-axis v - s*round(v/s) when s>0, else v (infinite tiling).
    c.funcs << "float " << fname(idx) << "(vec3 p){\n"
            << "  vec3 s = " << per << ", q = p;\n"
            << "  if (s.x > 0.0) q.x = p.x - s.x * dcRoundTA(p.x / s.x);\n"
            << "  if (s.y > 0.0) q.y = p.y - s.y * dcRoundTA(p.y / s.y);\n"
            << "  if (s.z > 0.0) q.z = p.z - s.z * dcRoundTA(p.z / s.z);\n"
            << "  return " << fname(child) << "(q);\n}\n";
    return idx;
  }
  if (op == "repeat-limited") {
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    std::vector<double> pv = vecParam(n, "period", 3, true, {});
    std::vector<double> cv = vecParam(n, "count", 3, true, {});
    std::string per = declVec3(c, idx, "period", {pv[0], pv[1], pv[2]}, n);
    std::string cnt = declVec3(c, idx, "count", {cv[0], cv[1], cv[2]}, n);
    // Quilez finite repetition: home tile + 7 neighbours, ids clamped to
    // [0,count-1], keep the closest copy (min over the 8). Mirrors
    // RepeatLimitedField::evaluate (domain_ops.cpp).
    std::ostringstream b;
    b << "float " << fname(idx) << "(vec3 p){\n";
    b << "  vec3 s = " << per << ", hi = max(" << cnt << " - 1.0, vec3(0.0));\n";
    b << "  vec3 id = vec3(s.x > 0.0 ? dcRoundTA(p.x/s.x) : 0.0,\n";
    b << "                 s.y > 0.0 ? dcRoundTA(p.y/s.y) : 0.0,\n";
    b << "                 s.z > 0.0 ? dcRoundTA(p.z/s.z) : 0.0);\n";
    b << "  vec3 o = sign(p - s * id);\n";
    b << "  float best = 1e30;\n";
    b << "  for (int k = 0; k < 2; ++k)\n";
    b << "  for (int j = 0; j < 2; ++j)\n";
    b << "  for (int i = 0; i < 2; ++i) {\n";
    b << "    vec3 rid = clamp(id + vec3(float(i)*o.x, float(j)*o.y, float(k)*o.z),"
         " vec3(0.0), hi);\n";
    b << "    best = min(best, " << fname(child) << "(p - s * rid));\n";
    b << "  }\n";
    b << "  return best;\n}\n";
    c.funcs << b.str();
    return idx;
  }
  if (op == "displace") {
    requireChildren(n, 1);
    const int child = emitNode(n.in[0], c);
    const int idx = c.counter++;
    const std::string fn = strParam(n, "fn", "sine");
    std::string amp = declScalar(c, idx, "amplitude", numParam(n, "amplitude", 0.1), n);
    std::string freq = declScalar(c, idx, "frequency", numParam(n, "frequency", 6.0), n);
    // child(p) + amp/div * pattern(freq*p); div matches example_common namedBump.
    std::string pat;
    if (fn == "sine")
      pat = amp + " * dcDispSine(" + freq + " * p)";
    else if (fn == "gyroid")
      pat = "(" + amp + " / 1.4973) * dcDispGyroid(" + freq + " * p)";
    else if (fn == "bumps")
      pat = "(" + amp + " / 3.0) * dcDispBumps(" + freq + " * p)";
    else
      throw GraphError("displace fn must be 'sine', 'gyroid' or 'bumps'", n.pointer);
    c.funcs << "float " << fname(idx) << "(vec3 p){ return " << fname(child)
            << "(p) + " << pat << "; }\n";
    return idx;
  }

  // 6. Analytic primitives (leaf).
  if (findPrimitive(op)) return emitPrimitive(n, c);

  // 7. Unknown / unsupported.
  throw GraphError("GLSL codegen does not support op '" + op + "'", n.pointer);
}

// ---- step-scale analysis --------------------------------------------------
float nodeStepScale(const GraphNode& n) {
  const std::string& op = n.op;
  if (op == "normalize") return 1.0f;  // re-normalises its subtree
  if (isTpmsOp(op)) return 0.5f;
  BoolOp bop;
  if (parseBoolOp(op, bop)) {
    float s = 1.0f;
    for (const GraphNode& ch : n.in) s = std::min(s, nodeStepScale(ch));
    if (bop == BoolOp::SmoothUnion || bop == BoolOp::SmoothIntersection ||
        bop == BoolOp::SmoothDifference)
      s = std::min(s, 0.6f);
    return s;
  }
  if (op == "twist" || op == "bend" || op == "displace") {
    float s = 0.5f;
    for (const GraphNode& ch : n.in) s = std::min(s, nodeStepScale(ch));
    return s;
  }
  // Baked sources are NOT Lipschitz-1 distance fields, so they must not take
  // the uMetricSDF fast path (which steps by the raw value, no gradient
  // normalization). Outside the narrow band a `mesh` grid carries chamferGrow's
  // deliberate OVER-estimate (src/internal/distance_grid.h), and `winding`
  // stores a unitless 0.5 - windingNumber (src/implicit/winding_field.cpp).
  // Stepping either directly tunnels through thin features: on foot.obj under a
  // graded gyroid, restoring the conservative step took isolated pinholes from
  // 127 to 16. Making the baked far field conservative would earn the fast path
  // back -- see docs/roadmap/12-field-graph-and-app/02-glsl-codegen.md.
  if (op == "mesh") return 0.6f;
  if (op == "winding") return 0.5f;
  // primitives / metric decorators: pass child through (1.0 at a leaf).
  float s = 1.0f;
  for (const GraphNode& ch : n.in) s = std::min(s, nodeStepScale(ch));
  return s;
}

// Smallest characteristic feature length in the graph: the min TPMS wavelength.
// Infinity (-> reported as 0) when the graph has no periodic source, so the app
// can fall back to the bounds diagonal.
float nodeFeatureScale(const GraphNode& n) {
  float best = std::numeric_limits<float>::infinity();
  if (isTpmsOp(n.op) || isStrutOp(n.op))
    best = std::min(best, float(numParam(n, "wavelength", 1.0)));
  for (const GraphNode& ch : n.in) best = std::min(best, nodeFeatureScale(ch));
  return best;
}

}  // namespace

GlslScene compileToGlsl(const GraphNode& root, MeshResolver& meshes,
                        const dualc::BBox& bounds, int gridRes) {
  Ctx c;
  c.resolver = &meshes;
  c.bounds = bounds;
  // Clamp here too: a bad programmatic value must not reach GridField's throw.
  c.gridRes = std::max(2, std::min(gridRes, kMaxMeshGridRes));
  const int rootIdx = emitNode(root, c);

  std::ostringstream src;
  src << c.decls.str() << "\n" << c.funcs.str() << "\n"
      << "float sceneSDF(vec3 p){ return " << fname(rootIdx) << "(p); }\n";

  GlslScene scene;
  scene.generatedSource = src.str();
  scene.bindings = std::move(c.bindings);
  scene.meshes = std::move(c.meshes);
  scene.bounds = bounds;
  scene.nodeCount = c.counter;
  scene.stepScale = nodeStepScale(root);
  const float feat = nodeFeatureScale(root);
  scene.featureScale = std::isfinite(feat) ? feat : 0.0f;
  return scene;
}

std::string assembleTraceShader(const GlslScene& scene, bool emitES) {
  return header(emitES) + kLibrary + "\n" + scene.generatedSource + "\n" +
         kTraceFramework;
}

std::string assembleValueShader(const GlslScene& scene, bool emitES) {
  return header(emitES) + kLibrary + "\n" + scene.generatedSource + "\n" +
         kValueFramework;
}

std::string vertexShaderSource(bool emitES) {
  return header(emitES) + kVertexBody;
}

}  // namespace fieldgraph
}  // namespace dce
