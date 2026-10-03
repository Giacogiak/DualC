#pragma once

#include "dualc/dualc.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <memory>
#include <tuple>

// Procedural demo-mesh generators (Tier 3 #12).
//
// Each helper builds an explicit vertex/face list and returns a
// geometry-central (SurfaceMesh, VertexPositionGeometry) pair, so a caller can
// either write it to OBJ (`writeSurfaceMesh`) or feed it straight into
// `dualContourMesh`. The shapes deliberately span topology / curvature /
// feature regimes for contouring QA:
//
//   icosphere / uv-sphere  smooth curvature, Euler chi = 2 (uv exposes pole
//                          tessellation slivers; ico is uniform).
//   torus                  Euler chi = 0, genus 1 -- the non-zero-genus check.
//   trefoil knot           chi = 0, genus 1, strong varying curvature and
//                          near-approaching strands (organic-like stress case).
//   genus-2 double torus   chi = -2 -- higher genus.
//   cylinder               chi = 2, mixed sharp circular rims + smooth wall.
//   L-bracket              chi = 2, CAD part with BOTH convex (90 deg) and
//                          concave (270 deg, reentrant) sharp edges -- the
//                          sharp-feature toggle validator.
//   hex prism + bore       chi = 0, genus 1, many sharp facets + curved bore.
//
// These live in examples/ (not the library): mesh generation is a host-side
// concern, like mesh I/O.

namespace dce {

using MeshAndGeom =
    std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
               std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>;

// Icosphere: subdivided icosahedron projected to `radius`. `subdivisions`
// quadruples the triangle count each step (n=3 -> 1280 tris). Uniform triangle
// area -> the cleanest smooth-curvature baseline.
MeshAndGeom makeIcosphere(double radius = 1.0, int subdivisions = 3);

// UV-sphere: latitude/longitude grid with two poles. Concentrates sliver
// triangles at the poles -- kept precisely to contrast with the icosphere.
MeshAndGeom makeUvSphere(double radius = 1.0, int nLat = 24, int nLong = 48);

// Torus: doubly-periodic u/v grid, ring in the xz-plane (axis = y). Genus 1.
MeshAndGeom makeTorus(double majorRadius = 1.0, double minorRadius = 0.35,
                      int nMajor = 64, int nMinor = 32);

// Trefoil knot: a tube of radius `tubeR` swept along the (2,3) torus-knot
// centerline with a closed rotation-minimizing frame. Genus 1.
MeshAndGeom makeTrefoilKnot(double tubeR = 0.35, int nAlong = 256,
                            int nAround = 24);

// Genus-2 double torus: the union of two coplanar tori that merge in a single
// neck, contoured by DualC itself into a watertight mesh. chi = -2.
MeshAndGeom makeGenus2(int maxDepth = 7);

// Unit cube centred at the origin, 12 triangles: the input of most command-
// reference recipes. Identical (vertex order, triangulation) to the hand-
// authored data/cube.obj that predates this generator, so `all` now yields
// every recipe input and nothing depends on a file that is not tracked.
MeshAndGeom makeCube(double side = 1.0);

// Cylinder: `nSeg` side quads + two flat polygon caps. Sharp circular rims.
MeshAndGeom makeCylinder(double radius = 1.0, double height = 2.0,
                         int nSeg = 48);

// L-bracket: an L-shaped profile extruded along z. Convex + concave sharp
// edges. `arm` is the outer square side, `thick` the arm width, `depth` the
// extrusion.
MeshAndGeom makeLBracket(double arm = 1.0, double thick = 0.4,
                         double depth = 0.6);

// Hexagonal prism with a cylindrical through-bore. `nSeg` must be a multiple
// of 6 so the outer silhouette keeps its 6 sharp corners while the annular
// caps stay quad-only. Genus 1.
MeshAndGeom makeHexPrismBore(double outerR = 1.0, double height = 1.2,
                             double boreR = 0.45, int nSeg = 24);

} // namespace dce
