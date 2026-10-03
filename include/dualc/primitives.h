#pragma once

#include "dualc/implicit.h"

// Analytic primitive fields -- the implicit-field building blocks of v2.
//
// Each primitive is a public ImplicitField subclass with a closed-form
// valueAt() and a closed-form bounds(); construct them directly via
// std::make_shared<XxxField>(...). Unlike the boolean combinators (which are
// hidden behind builder functions), primitives are the leaves of the
// expression tree and the user names them explicitly.
//
// The signed-distance formulas follow Inigo Quilez's distance-functions
// article (https://iquilezles.org/articles/distfunctions/). Primitives that
// have a natural placement (sphere centre, box corners, capsule endpoints)
// take it directly; otherwise a primitive is built in its canonical Quilez
// orientation around a `center` and positioned with the `transformed`
// decorator.

namespace dualc {

// Base for analytic primitives: supplies a central-difference gradient so a
// concrete primitive only has to provide valueAt() and bounds(). Subclasses
// with a cheap exact gradient override gradientAt().
class PrimitiveField : public ImplicitField {
public:
  Vector3 gradientAt(const Vector3& p) const override {
    constexpr double e = 1e-4;
    const double dx = valueAt(Vector3{p.x + e, p.y, p.z}) -
                      valueAt(Vector3{p.x - e, p.y, p.z});
    const double dy = valueAt(Vector3{p.x, p.y + e, p.z}) -
                      valueAt(Vector3{p.x, p.y - e, p.z});
    const double dz = valueAt(Vector3{p.x, p.y, p.z + e}) -
                      valueAt(Vector3{p.x, p.y, p.z - e});
    const Vector3 g{dx, dy, dz};
    const double n = g.norm();
    return (n > 1e-12) ? g * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
  }
};

// ===================================================================
// Tier A -- core 8
// ===================================================================

// Sphere of `radius` centred at `center`.
class SphereField : public PrimitiveField {
public:
  SphereField(const Vector3& center, double radius);
  double  valueAt(const Vector3& p) const override;
  Vector3 gradientAt(const Vector3& p) const override;
  BBox    bounds() const override;

private:
  Vector3 c_;
  double  r_;
};

// Axis-aligned box spanning the corners [min, max].
class BoxField : public PrimitiveField {
public:
  BoxField(const Vector3& min, const Vector3& max);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;   // centre
  Vector3 h_;   // half-extents
};

// Axis-aligned box with corners rounded by `radius`; [min, max] is the outer
// extent (radius is rounded inward from it).
class RoundBoxField : public PrimitiveField {
public:
  RoundBoxField(const Vector3& min, const Vector3& max, double radius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  Vector3 h_;
  double  r_;
};

// Infinite half-space. The surface is the plane { dot(p, normal) + offset = 0 }
// and the field is negative on the side the normal points away from.
// bounds() is infinite -- sampling needs an explicit SamplerParams::rootBounds.
class PlaneField : public PrimitiveField {
public:
  PlaneField(const Vector3& normal, double offset);
  double  valueAt(const Vector3& p) const override;
  Vector3 gradientAt(const Vector3& p) const override;
  BBox    bounds() const override;

private:
  Vector3 n_;   // unit normal
  double  o_;
};

// Capsule: the `radius`-thick swept sphere along the segment [a, b].
class CapsuleField : public PrimitiveField {
public:
  CapsuleField(const Vector3& a, const Vector3& b, double radius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 a_, b_;
  double  r_;
};

// Capped cylinder of `radius` with flat caps, axis running from a to b.
class CappedCylinderField : public PrimitiveField {
public:
  CappedCylinderField(const Vector3& a, const Vector3& b, double radius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 a_, b_;
  double  r_;
};

// Torus centred at `center`, ring in the local xz-plane (axis = y).
// majorRadius is the ring radius, minorRadius the tube radius.
class TorusField : public PrimitiveField {
public:
  TorusField(const Vector3& center, double majorRadius, double minorRadius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  R_, r_;
};

// Ellipsoid with per-axis `radii`, centred at `center`. This is Quilez's
// bounded (not exact) ellipsoid distance: a tight lower bound on the true SDF.
class EllipsoidField : public PrimitiveField {
public:
  EllipsoidField(const Vector3& center, const Vector3& radii);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  Vector3 rad_;
};

// ===================================================================
// Tier B -- common
// ===================================================================

// Hollow box frame: the box [min, max] with its faces removed, leaving 12
// struts of square cross-section `edgeThickness`.
class BoxFrameField : public PrimitiveField {
public:
  BoxFrameField(const Vector3& min, const Vector3& max, double edgeThickness);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  Vector3 h_;
  double  e_;
};

// Exact finite cone, apex at `center`, opening downward (-y) over `height`.
// `angleRad` is the half-angle between the axis and the side.
class ConeField : public PrimitiveField {
public:
  ConeField(const Vector3& center, double angleRad, double height);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  sin_, cos_, h_;
};

// Capped (truncated) cone, axis = y, centred at `center`. The lower cap
// (y = -height) has radius radiusLow, the upper cap (y = +height) radiusHigh.
class CappedConeField : public PrimitiveField {
public:
  CappedConeField(const Vector3& center, double height,
                  double radiusLow, double radiusHigh);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  h_, r1_, r2_;
};

// Round cone: a cone capped by spheres of radius radiusA at a and radiusB
// at b -- a smoothly tapered capsule.
class RoundConeField : public PrimitiveField {
public:
  RoundConeField(const Vector3& a, const Vector3& b,
                 double radiusA, double radiusB);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 a_, b_;
  double  r1_, r2_;
};

// Infinite cylinder of `radius` whose axis passes through `axisPoint` along
// `axisDir`. bounds() is infinite -- sampling needs an explicit rootBounds.
class InfiniteCylinderField : public PrimitiveField {
public:
  InfiniteCylinderField(const Vector3& axisPoint, const Vector3& axisDir,
                        double radius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 p0_;
  Vector3 dir_;   // unit
  double  r_;
};

// Hexagonal prism, axis = z, centred at `center`. `radius` is the apothem
// (centre-to-edge distance) of the hexagon, `halfLength` the half-extent
// along z.
class HexPrismField : public PrimitiveField {
public:
  HexPrismField(const Vector3& center, double radius, double halfLength);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  r_, hl_;
};

// Triangular prism, axis = z, centred at `center`. Quilez's bounded (not
// exact) triangular-prism distance.
class TriPrismField : public PrimitiveField {
public:
  TriPrismField(const Vector3& center, double radius, double halfLength);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  r_, hl_;
};

// Octahedron of `size` (vertex-to-centre distance), centred at `center`.
// `exact` selects the exact distance; false uses Quilez's cheap bound.
class OctahedronField : public PrimitiveField {
public:
  OctahedronField(const Vector3& center, double size, bool exact = true);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  s_;
  bool    exact_;
};

// Square-based pyramid: base of side 1 in the local xz-plane at y = 0,
// apex at y = height. Positioned by `center` (the base centre).
class PyramidField : public PrimitiveField {
public:
  PyramidField(const Vector3& center, double height);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  h_;
};

// Solid angle ("ice-cream cone"): the part of a sphere of `radius` within a
// cone of half-angle `angleRad` opening along +y from `center`.
class SolidAngleField : public PrimitiveField {
public:
  SolidAngleField(const Vector3& center, double angleRad, double radius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  sin_, cos_, ra_;
};

// ===================================================================
// Tier C -- long tail
// ===================================================================

// Partial torus (an arc): a torus clipped to the wedge of half-angle
// `angleRad`. Ring in the local xy-plane, centred at `center`.
class CappedTorusField : public PrimitiveField {
public:
  CappedTorusField(const Vector3& center, double angleRad,
                   double majorRadius, double minorRadius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  sin_, cos_, ra_, rb_;
};

// A chain link: a torus of radii (majorRadius, minorRadius) stretched by
// `halfLength` along y, centred at `center`.
class LinkField : public PrimitiveField {
public:
  LinkField(const Vector3& center, double halfLength,
            double majorRadius, double minorRadius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  le_, r1_, r2_;
};

// A sphere of `radius` sliced flat by the plane y = `cutHeight`; the cap
// with y >= cutHeight is kept (the flat disc faces down). Centred at `center`.
class CutSphereField : public PrimitiveField {
public:
  CutSphereField(const Vector3& center, double radius, double cutHeight);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  r_, h_;
};

// A hollow (shell) sphere of `radius` and wall `thickness`, sliced flat by
// the plane y = `cutHeight`. Centred at `center`.
class CutHollowSphereField : public PrimitiveField {
public:
  CutHollowSphereField(const Vector3& center, double radius,
                       double cutHeight, double thickness);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  r_, h_, t_;
};

// Death-star shape: a sphere of radius `radiusMain` with a spherical bite of
// radius `radiusBite` removed, the bite's centre offset by `distance` along
// +x. Centred at `center`.
class DeathStarField : public PrimitiveField {
public:
  DeathStarField(const Vector3& center, double radiusMain,
                 double radiusBite, double distance);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  ra_, rb_, d_;
};

// Vesica segment: the lens-shaped swept profile along the segment [a, b],
// bulging out to half-width `width` at the midpoint.
class VesicaSegmentField : public PrimitiveField {
public:
  VesicaSegmentField(const Vector3& a, const Vector3& b, double width);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 a_, b_;
  double  w_;
};

// Rhombus (a flat diamond extruded along y): diagonals 2*la (x) and 2*lb (z),
// half-height `height` along y, edges rounded by `cornerRadius`.
class RhombusField : public PrimitiveField {
public:
  RhombusField(const Vector3& center, double la, double lb,
               double height, double cornerRadius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  la_, lb_, h_, ra_;
};

// Vertical capsule: the `radius`-thick swept sphere along the segment from
// `center` (y = 0) up to y = `height`.
class VerticalCapsuleField : public PrimitiveField {
public:
  VerticalCapsuleField(const Vector3& center, double height, double radius);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  h_, r_;
};

// Cylinder of outer `radius` and `halfHeight`, with both rim edges rounded by
// `roundRadius`. Axis = y, centred at `center`.
class RoundedCylinderField : public PrimitiveField {
public:
  RoundedCylinderField(const Vector3& center, double radius,
                       double roundRadius, double halfHeight);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  ra_, rb_, h_;
};

// A single triangle. NOTE: this is an open surface -- valueAt() is an
// UNSIGNED distance (always >= 0), so it has no inside and cannot be
// dual-contoured into a closed mesh on its own. Use it as the child of
// onionOf() (to give it thickness) or another closing operation.
class TriangleField : public PrimitiveField {
public:
  TriangleField(const Vector3& a, const Vector3& b, const Vector3& c);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 a_, b_, c_;
};

// A planar quad (a, b, c, d in order). Like TriangleField this is an open
// surface: valueAt() is an UNSIGNED distance -- see TriangleField.
class QuadField : public PrimitiveField {
public:
  QuadField(const Vector3& a, const Vector3& b,
            const Vector3& c, const Vector3& d);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 a_, b_, c_, d_;
};

// Infinite cone, apex at `center`, opening along -y with half-angle
// `angleRad`. bounds() is infinite -- sampling needs an explicit rootBounds.
class InfiniteConeField : public PrimitiveField {
public:
  InfiniteConeField(const Vector3& center, double angleRad);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;

private:
  Vector3 c_;
  double  sin_, cos_;
};

// ===================================================================
// Tier D -- TPMS (triply periodic minimal surfaces)
// ===================================================================
//
// Trigonometric scalar fields whose 0-isosurface is a periodic minimal
// surface on all of R^3 -- the building blocks of TPMS lattice infill.
// Each TPMS takes a `center` (origin of the trig phase) and `wavelength`
// (period along each axis -- the unit-cell side). bounds() is infinite;
// the sampler requires an explicit SamplerParams::rootBounds. Intended
// to be composed via intersectionOf(...) with a MeshSource volume and
// optionally onionOf(..., t) for a closed thick-walled lattice shell.
//
// Formulas follow Schoen (1970, NASA TN D-5541) and Gandy et al. (2001);
// values are not signed distances -- magnitudes vary with the local
// gradient. Use normalizedOf() (see implicit.h) for a first-order
// SDF-normalized variant when metric-distance offsets matter.

// Gyroid (Schoen G):
//   F = sin(kx)cos(ky) + sin(ky)cos(kz) + sin(kz)cos(kx),   k = 2π/wavelength
class GyroidField : public PrimitiveField {
public:
  GyroidField(const Vector3& center, double wavelength);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;
  bool   cellOverlaps(const BBox& cell) const override;

private:
  Vector3 c_;
  double  k_;
};

// Schwarz P (the "primitive" cubic TPMS):
//   F = cos(kx) + cos(ky) + cos(kz)
class SchwarzPField : public PrimitiveField {
public:
  SchwarzPField(const Vector3& center, double wavelength);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;
  bool   cellOverlaps(const BBox& cell) const override;

private:
  Vector3 c_;
  double  k_;
};

// Schwarz D (Diamond): the "diamond" cubic TPMS.
class DiamondField : public PrimitiveField {
public:
  DiamondField(const Vector3& center, double wavelength);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;
  bool   cellOverlaps(const BBox& cell) const override;

private:
  Vector3 c_;
  double  k_;
};

// Fischer-Koch S: an I-43m TPMS used for bone scaffolds / lattice infill.
class FischerKochSField : public PrimitiveField {
public:
  FischerKochSField(const Vector3& center, double wavelength);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;
  bool   cellOverlaps(const BBox& cell) const override;

private:
  Vector3 c_;
  double  k_;
};

// Lidinoid (Lidin's surface): TPMS in the gyroid family.
class LidinoidField : public PrimitiveField {
public:
  LidinoidField(const Vector3& center, double wavelength);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;
  bool   cellOverlaps(const BBox& cell) const override;

private:
  Vector3 c_;
  double  k_;
};

// Neovius: a cubic TPMS-adjacent surface (Im-3m symmetry).
class NeoviusField : public PrimitiveField {
public:
  NeoviusField(const Vector3& center, double wavelength);
  double valueAt(const Vector3& p) const override;
  BBox   bounds() const override;
  bool   cellOverlaps(const BBox& cell) const override;

private:
  Vector3 c_;
  double  k_;
};

} // namespace dualc
