#pragma once

#include "dualc/implicit.h"
#include "dualc/progress.h"
#include "dualc/types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// The porous shell's build-time half (roadmap 23): seeds placed in any
// volume, deterministically on every OS. The Delaunay complex (#64,
// src/internal/porous_complex.h) and the
// `porous` field (#65, PorousField below) build on SeedSet; the defaults every choice here
// follows are the table in docs/roadmap/23-porous-shell/01-study.md.

namespace dualc {

// PCG32, XSH-RR 64/32 -- O'Neill's pcg32_random_r, seeded the way
// pcg32_srandom_r(initState, initSeq) seeds it. The whole porous build draws
// from it and nothing else: std::*_distribution differs between libstdc++,
// libc++ and MSVC, so the seeds would differ between the three CI OSes.
class Pcg32 {
public:
  explicit Pcg32(std::uint64_t initState, std::uint64_t initSeq = 0);

  std::uint32_t next();

  // A double in [0, 1) with 53 random bits, from two draws:
  // ((a >> 5) * 2^26 + (b >> 6)) * 2^-53. Exact in binary64, so identical
  // everywhere.
  double nextDouble();

private:
  std::uint64_t state_ = 0;
  std::uint64_t inc_   = 0;
};

enum class SeedDistribution { Poisson, Jitter, Random };

// Which complex the porous field measures the distance to (#64): the Voronoi
// cells of the seeds or their Delaunay tets; and whether that is the cells'
// walls (the Voronoi faces / the tet faces) or their edges (the Voronoi edges
// / the tet edges), the struts of an open-cell foam.
enum class PorousCells { Voronoi, Delaunay };
enum class PorousMode { Walls, Struts };
enum class DensityRamp { Linear, Smooth, Gaussian };

// The ramp weight w(u) for u = (control - d0) / (d1 - d0), clamped to
// [0, 1]: linear u; smooth u^2 (3 - 2u); gaussian (1 - 100^(-u^2)) / 0.99,
// which is (1 - exp(-u^2 / 2 sigma^2)) / (1 - exp(-1 / 2 sigma^2)) with
// sigma = 1 / sqrt(2 ln 100), so that x = 1 reaches 99 % before the
// normalisation to 1. The exponential is DualC's own (basic arithmetic only),
// not libm's, whose last bit differs between platforms.
double densityRampWeight(DensityRamp ramp, double u);

// Ceilings: a clear std::invalid_argument past them (the graph node turns it
// into a GraphError).
constexpr std::size_t kPorousMaxSeeds = 2000000;
constexpr int         kPorousMaxRelax = 50;

struct SeedParams {
  SeedDistribution dist = SeedDistribution::Poisson;
  // poisson: the Bridson minimum distance; jitter: the grid pitch; random:
  // density 1 / spacing^3. Where a control is given, `spacing` holds where
  // control <= d0 and `spacing2` where control >= d1, through the ramp.
  double      spacing  = 0.0;  // required, > 0
  double      spacing2 = 0.0;  // <= 0 means "= spacing"
  double      d0       = 0.0;
  double      d1       = 0.0;  // must differ from d0 when the density is graded
  DensityRamp ramp     = DensityRamp::Linear;
  int         relax    = 0;    // Lloyd iterations, 0 ... kPorousMaxRelax
  std::uint64_t seed   = 0;
  double      jitter   = 1.0;  // dist = jitter only; 0 = the regular grid
  unsigned    numThreads = 0;  // Lloyd's sample pass; 0 = all hardware threads.
                               // The output does not depend on it.
};

// The seeds of one porous build. `points[i]` is a seed; `isPad[i]` is 1 when
// the volume's value there is > 0 (the seed sits in the pad outside the
// volume, which keeps the boundary cells well formed) and 0 inside.
struct SeedSet {
  std::vector<Vector3>      points;
  std::vector<std::uint8_t> isPad;
  BBox   box;              // the volume's bounds grown by `pad`: every seed is in it
  double pad        = 0.0; // max(spacing, spacing2); seeds satisfy volume < pad
  double minSpacing = 0.0; // min(spacing, spacing2)
  double maxSpacing = 0.0; // max(spacing, spacing2)
};

// Places seeds in { x : volume(x) < pad } by `params`. `control` (may be null)
// grades the spacing; without it, or with spacing2 == spacing, the density is
// uniform. The volume and the control are evaluated here only. Same inputs ->
// the same SeedSet, bit for bit, on any OS and thread count.
//
// Throws std::invalid_argument on bad parameters (spacing <= 0, relax out of
// range, a graded density with d1 == d0, a volume with no finite bounds) and
// past the ceilings; throws Cancelled when `cancel` is requested (polled every
// 4096 candidates and per Lloyd chunk).
SeedSet generatePorousSeeds(const ImplicitField& volume,
                            const ImplicitField* control,
                            const SeedParams&    params,
                            const CancelToken*   cancel = nullptr);

// What one porous field is built from: its seeds, which complex, and the
// smooth-min radius of the fold (k >= 0; 0 = the exact distance).
struct PorousParams {
  SeedParams  seeds;
  PorousCells cells  = PorousCells::Voronoi;
  PorousMode  mode   = PorousMode::Walls;
  double      smooth = 0.0;
};

// The `porous` source (#65): the unsigned distance to the Voronoi or Delaunay
// complex of seeds placed in a volume -- its walls or its struts -- smoothed
// by PorousParams::smooth. Zero on the complex and positive elsewhere, so a
// contour of the bare field is empty: it is a source to give a thickness to,
// e.g. intersection(V, onion(porous(V, ...), thickness=t)).
//
// The volume and the control are read during construction only (seeds,
// tetrahedralization, packing); the field keeps the packed complex and the
// volume's box, nothing that points back at the caller's fields. bounds() is
// that box: the complex is built for it and is meant to be clipped by the
// volume. Construction costs the seeds plus about 2 s per 100k seeds (one
// thread); evaluation is const and thread-safe.
//
// Throws std::invalid_argument on what generatePorousSeeds and the complex
// reject, on a negative or non-finite smooth, and, for struts, on a smooth at
// or past the struts' bound B (the field would be 0 everywhere); throws
// Cancelled when `cancel` is requested.
class PorousField : public ImplicitField {
public:
  PorousField(const ImplicitField& volume, const ImplicitField* control,
              const PorousParams& params, const CancelToken* cancel = nullptr);
  ~PorousField() override;

  PorousField(const PorousField&)            = delete;
  PorousField& operator=(const PorousField&) = delete;

  double  valueAt(const Vector3& p) const override;
  // Analytic: one query, the fold's gradient over the primitives' own.
  Vector3 gradientAt(const Vector3& p) const override;
  Vector3 trueGradientAt(const Vector3& p) const override;
  BBox    bounds() const override;
  // cellOverlaps: the default, deliberately -- see porous.cpp for why the
  // value is 1-Lipschitz.

  struct Stats {
    std::size_t seeds = 0, padSeeds = 0, tets = 0;
    std::size_t delaunayFaces = 0, delaunayEdges = 0;
    std::size_t segments = 0;     // struts: the packed segments
    std::size_t bins = 0;
    std::size_t packedBytes = 0;  // what the GLSL node uploads
    double bound = 0.0;           // struts: B, where the distance is clamped
  };
  const Stats&        stats() const;
  const PorousParams& params() const;

private:
  struct Impl;
  std::unique_ptr<const Impl> impl_;
};

} // namespace dualc
