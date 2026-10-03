#include "dualc/primitives.h"

#include <cmath>
#include <stdexcept>
#include <string>

// Triply Periodic Minimal Surface (TPMS) primitives. Each field is an analytic
// trigonometric scalar function whose 0-isosurface is a periodic surface on
// all of R^3; bounds() is therefore BBox::infinite() and the sampler requires
// an explicit SamplerParams::rootBounds (same contract as PlaneField).
//
// Formulas follow Schoen (1970, NASA TN D-5541) and Gandy et al. (2001);
// conventions cross-checked against PicoGK's TPMS module (Apache-2.0,
// LEAP 71 -- reference only, no code copied).

namespace dualc {

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

// Local origin shift + k = 2π/λ baked once. Returns (kx, ky, kz) for p.
inline void tpmsArgs(const Vector3& p, const Vector3& c, double k,
                     double& kx, double& ky, double& kz) {
  kx = k * (p.x - c.x);
  ky = k * (p.y - c.y);
  kz = k * (p.z - c.z);
}

// TPMS surfaces are periodic and densely fill space -- the 0-isosurface
// almost always passes through any cell whose side is on the order of the
// wavelength or larger. The default cellOverlaps (Lipschitz-1 conservative
// test on the field value) under-refines, missing cells where the surface
// passes through without dragging any corner to the opposite sign. Refining
// every cell to maxDepth is safe and matches the dense-surface reality.
constexpr bool kAlwaysOverlap = true;

} // namespace

// ---- Gyroid (Schoen G) --------------------------------------------------
//   F = sin(kx)cos(ky) + sin(ky)cos(kz) + sin(kz)cos(kx)

namespace {

// Every TPMS primitive turns a wavelength into an angular frequency k = 2*pi/L.
// A wavelength of 0 makes k infinite and every sin/cos NaN; a negative one
// mirrors the surface, which is not what the parameter means. Rejected at
// construction rather than left to produce a field of NaNs -- the same
// fail-fast contract GridField and bakeToGrid already follow.
void requirePositiveWavelength(const char* what, double wavelength) {
  if (!(wavelength > 0.0))
    throw std::invalid_argument(
        std::string(what) + ": wavelength must be > 0 (got " +
        std::to_string(wavelength) + ")");
}

}  // namespace

GyroidField::GyroidField(const Vector3& center, double wavelength)
    : c_(center), k_(kTwoPi / wavelength) {
  requirePositiveWavelength("GyroidField", wavelength);
}

double GyroidField::valueAt(const Vector3& p) const {
  double kx, ky, kz;
  tpmsArgs(p, c_, k_, kx, ky, kz);
  return std::sin(kx) * std::cos(ky)
       + std::sin(ky) * std::cos(kz)
       + std::sin(kz) * std::cos(kx);
}

BBox GyroidField::bounds() const { return BBox::infinite(); }

bool GyroidField::cellOverlaps(const BBox&) const { return kAlwaysOverlap; }

// ---- Schwarz P ----------------------------------------------------------
//   F = cos(kx) + cos(ky) + cos(kz)

SchwarzPField::SchwarzPField(const Vector3& center, double wavelength)
    : c_(center), k_(kTwoPi / wavelength) {
  requirePositiveWavelength("SchwarzPField", wavelength);
}

double SchwarzPField::valueAt(const Vector3& p) const {
  double kx, ky, kz;
  tpmsArgs(p, c_, k_, kx, ky, kz);
  return std::cos(kx) + std::cos(ky) + std::cos(kz);
}

BBox SchwarzPField::bounds() const { return BBox::infinite(); }

bool SchwarzPField::cellOverlaps(const BBox&) const { return kAlwaysOverlap; }

// ---- Schwarz D (Diamond) ------------------------------------------------
//   F = sin(kx)sin(ky)sin(kz) + sin(kx)cos(ky)cos(kz)
//     + cos(kx)sin(ky)cos(kz) + cos(kx)cos(ky)sin(kz)

DiamondField::DiamondField(const Vector3& center, double wavelength)
    : c_(center), k_(kTwoPi / wavelength) {
  requirePositiveWavelength("DiamondField", wavelength);
}

double DiamondField::valueAt(const Vector3& p) const {
  double kx, ky, kz;
  tpmsArgs(p, c_, k_, kx, ky, kz);
  const double sx = std::sin(kx), cx = std::cos(kx);
  const double sy = std::sin(ky), cy = std::cos(ky);
  const double sz = std::sin(kz), cz = std::cos(kz);
  return sx * sy * sz
       + sx * cy * cz
       + cx * sy * cz
       + cx * cy * sz;
}

BBox DiamondField::bounds() const { return BBox::infinite(); }

bool DiamondField::cellOverlaps(const BBox&) const { return kAlwaysOverlap; }

// ---- Fischer-Koch S -----------------------------------------------------
//   F = cos(2kx)sin(ky)cos(kz)
//     + cos(kx)cos(2ky)sin(kz)
//     + sin(kx)cos(ky)cos(2kz)

FischerKochSField::FischerKochSField(const Vector3& center, double wavelength)
    : c_(center), k_(kTwoPi / wavelength) {
  requirePositiveWavelength("FischerKochSField", wavelength);
}

double FischerKochSField::valueAt(const Vector3& p) const {
  double kx, ky, kz;
  tpmsArgs(p, c_, k_, kx, ky, kz);
  return std::cos(2.0 * kx) * std::sin(ky) * std::cos(kz)
       + std::cos(kx) * std::cos(2.0 * ky) * std::sin(kz)
       + std::sin(kx) * std::cos(ky) * std::cos(2.0 * kz);
}

BBox FischerKochSField::bounds() const { return BBox::infinite(); }

bool FischerKochSField::cellOverlaps(const BBox&) const { return kAlwaysOverlap; }

// ---- Lidinoid (Lidin's surface) -----------------------------------------
//   F = ½[sin(2kx)cos(ky)sin(kz) + sin(2ky)cos(kz)sin(kx)
//       + sin(2kz)cos(kx)sin(ky)]
//     − ½[cos(2kx)cos(2ky) + cos(2ky)cos(2kz) + cos(2kz)cos(2kx)]
//     + 0.15
//
// The trailing constant shifts the 0-isosurface to the Lidinoid proper (the
// minimal surface). PicoGK uses the same convention.

LidinoidField::LidinoidField(const Vector3& center, double wavelength)
    : c_(center), k_(kTwoPi / wavelength) {
  requirePositiveWavelength("LidinoidField", wavelength);
}

double LidinoidField::valueAt(const Vector3& p) const {
  double kx, ky, kz;
  tpmsArgs(p, c_, k_, kx, ky, kz);
  const double sx  = std::sin(kx),       cx  = std::cos(kx);
  const double sy  = std::sin(ky),       cy  = std::cos(ky);
  const double sz  = std::sin(kz),       cz  = std::cos(kz);
  const double s2x = std::sin(2.0 * kx), c2x = std::cos(2.0 * kx);
  const double s2y = std::sin(2.0 * ky), c2y = std::cos(2.0 * ky);
  const double s2z = std::sin(2.0 * kz), c2z = std::cos(2.0 * kz);
  const double t1 = 0.5 * (s2x * cy * sz + s2y * cz * sx + s2z * cx * sy);
  const double t2 = 0.5 * (c2x * c2y    + c2y * c2z    + c2z * c2x);
  return t1 - t2 + 0.15;
}

BBox LidinoidField::bounds() const { return BBox::infinite(); }

bool LidinoidField::cellOverlaps(const BBox&) const { return kAlwaysOverlap; }

// ---- Neovius ------------------------------------------------------------
//   F = 3(cos(kx) + cos(ky) + cos(kz)) + 4 cos(kx)cos(ky)cos(kz)

NeoviusField::NeoviusField(const Vector3& center, double wavelength)
    : c_(center), k_(kTwoPi / wavelength) {
  requirePositiveWavelength("NeoviusField", wavelength);
}

double NeoviusField::valueAt(const Vector3& p) const {
  double kx, ky, kz;
  tpmsArgs(p, c_, k_, kx, ky, kz);
  const double cx = std::cos(kx);
  const double cy = std::cos(ky);
  const double cz = std::cos(kz);
  return 3.0 * (cx + cy + cz) + 4.0 * cx * cy * cz;
}

BBox NeoviusField::bounds() const { return BBox::infinite(); }

bool NeoviusField::cellOverlaps(const BBox&) const { return kAlwaysOverlap; }

} // namespace dualc
