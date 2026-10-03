#include "demo_meshes.h"

#include "dualc/pipeline.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <tuple>

using namespace dualc;

namespace {

// Contour a demo mesh and report (boundary edges, Euler characteristic) of the
// output. A watertight closed manifold has 0 boundary edges; chi = V - E + F
// equals 2 - 2*genus, so a depth that resolves every thin neck/gap preserves
// the input's genus.
struct Topology {
  int boundaryEdges;
  int chi;
  int faces;
};

Topology topologyOf(geometrycentral::surface::SurfaceMesh& m) {
  int boundary = 0;
  for (auto e : m.edges())
    if (e.isBoundary()) ++boundary;
  const int chi = static_cast<int>(m.nVertices()) -
                  static_cast<int>(m.nEdges()) +
                  static_cast<int>(m.nFaces());
  return {boundary, chi, static_cast<int>(m.nFaces())};
}

// `collapseError` drives the optional adaptive cell-collapse pass; 0 (the
// default) disables it, which is what every non-collapse case below wants.
Topology contourTopology(dce::MeshAndGeom mg, int maxDepth,
                         double collapseError = 0.0) {
  auto& [mesh, geom] = mg;
  SamplerParams sp;
  sp.maxDepth = maxDepth;
  ContourerParams cp;
  cp.simplificationError = collapseError;
  auto [outMesh, outGeom, outNormals] = dualContourMesh(*mesh, *geom, sp, cp);
  REQUIRE(outMesh != nullptr);
  REQUIRE(outMesh->nVertices() > 0);
  return topologyOf(*outMesh);
}

} // namespace

// The depth per mesh is chosen so the octree resolves every thin neck/gap; a
// coarser depth would merge near-touching walls (dropping genus) or split thin
// features (opening boundary edges). These mirror the thin-wall depth notes in
// test_tpms.cpp -- if a future change breaks topology, the failing chi/edge
// count points straight at it.

TEST_CASE("Icosphere contours to a watertight sphere (chi=2)", "[demo]") {
  const auto t = contourTopology(dce::makeIcosphere(), 5);
  REQUIRE(t.boundaryEdges == 0);
  REQUIRE(t.chi == 2);
}

TEST_CASE("UV-sphere contours to a watertight sphere (chi=2)", "[demo]") {
  const auto t = contourTopology(dce::makeUvSphere(), 5);
  REQUIRE(t.boundaryEdges == 0);
  REQUIRE(t.chi == 2);
}

TEST_CASE("Torus contours preserving genus 1 (chi=0)", "[demo]") {
  const auto t = contourTopology(dce::makeTorus(), 6);
  REQUIRE(t.boundaryEdges == 0);
  REQUIRE(t.chi == 0);
}

TEST_CASE("Trefoil knot contours preserving genus 1 (chi=0)", "[demo]") {
  // tubeR 0.35 on the (2,3) knot: depth 7 separates the near-approaching
  // strands. A coarser octree merges them and changes the genus.
  const auto t = contourTopology(dce::makeTrefoilKnot(), 7);
  REQUIRE(t.boundaryEdges == 0);
  REQUIRE(t.chi == 0);
}

TEST_CASE("Genus-2 double torus contours preserving genus 2 (chi=-2)",
          "[demo]") {
  const auto t = contourTopology(dce::makeGenus2(7), 6);
  REQUIRE(t.boundaryEdges == 0);
  REQUIRE(t.chi == -2);
}

TEST_CASE("Cylinder contours to a watertight solid (chi=2)", "[demo]") {
  const auto t = contourTopology(dce::makeCylinder(), 6);
  REQUIRE(t.boundaryEdges == 0);
  REQUIRE(t.chi == 2);
}

TEST_CASE("L-bracket contours to a watertight solid (chi=2)", "[demo]") {
  const auto t = contourTopology(dce::makeLBracket(), 5);
  REQUIRE(t.boundaryEdges == 0);
  REQUIRE(t.chi == 2);
}

TEST_CASE("Hex prism + bore contours preserving genus 1 (chi=0)", "[demo]") {
  const auto t = contourTopology(dce::makeHexPrismBore(), 6);
  REQUIRE(t.boundaryEdges == 0);
  REQUIRE(t.chi == 0);
}

// ===========================================================================
// Adaptive cell collapse (simplifyHermiteOctree)
// ===========================================================================
//
// The collapse pass had no test at all before this. It is worth pinning
// because its acceptance test is a QEF energy: if the threshold's meaning ever
// changes again, these are the cases that notice. The contract asserted here
// is the one `contourer.h` states -- collapse may remove triangles, but the
// three topology gates mean it must never open a boundary or change the genus.

namespace {

// Sweep a range of thresholds rather than hard-coding one "big enough" value:
// the QEF energy is a summed squared distance whose scale depends on the mesh
// and the depth, so a magic constant here would be a maintenance trap.
void checkCollapseSweep(dce::MeshAndGeom (*make)(), int maxDepth, int expectChi) {
  const Topology base = contourTopology(make(), maxDepth, 0.0);
  REQUIRE(base.boundaryEdges == 0);
  REQUIRE(base.chi == expectChi);

  bool dropped = false;
  for (const double e : {1e-6, 1e-4, 1e-2, 1.0}) {
    const Topology t = contourTopology(make(), maxDepth, e);
    INFO("collapse threshold " << e << ": " << t.faces << " faces vs "
                               << base.faces << " uncollapsed");
    CHECK(t.boundaryEdges == 0);
    CHECK(t.chi == expectChi);
    CHECK(t.faces <= base.faces);
    if (t.faces < base.faces) dropped = true;
  }
  INFO("no threshold in the sweep collapsed anything");
  CHECK(dropped);
}

dce::MeshAndGeom icosphere() { return dce::makeIcosphere(); }
dce::MeshAndGeom lbracket()  { return dce::makeLBracket(); }

} // namespace

TEST_CASE("Adaptive collapse keeps a sphere watertight (chi=2) and cuts faces",
          "[demo][collapse]") {
  checkCollapseSweep(&icosphere, 5, 2);
}

TEST_CASE("Adaptive collapse keeps the L-bracket watertight (chi=2)",
          "[demo][collapse]") {
  // Flat-walled CAD geometry is the case collapse is for: large planar
  // regions merge with near-zero QEF energy.
  checkCollapseSweep(&lbracket, 5, 2);
}

TEST_CASE("simplifyHermiteOctree with threshold 0 is a no-op", "[demo][collapse]") {
  // Near-vacuous through the pipeline (which guards on > 0), so drive the
  // octree by hand: this is what catches the guard being deleted.
  auto mg = dce::makeLBracket();
  auto& [mesh, geom] = mg;
  SamplerParams sp;
  sp.maxDepth = 5;
  ContourerParams cp;

  HermiteOctree plain = sampleMeshToHermiteOctree(*mesh, *geom, sp);
  const std::size_t leavesBefore = plain.leafCount();
  auto [meshA, geomA, normalsA] = contourHermiteOctree(plain, cp);

  HermiteOctree zeroed = sampleMeshToHermiteOctree(*mesh, *geom, sp);
  simplifyHermiteOctree(zeroed, 0.0);
  REQUIRE(zeroed.leafCount() == leavesBefore);
  simplifyHermiteOctree(zeroed, cp);  // the ContourerParams overload too
  REQUIRE(zeroed.leafCount() == leavesBefore);
  auto [meshB, geomB, normalsB] = contourHermiteOctree(zeroed, cp);

  REQUIRE(meshB->nVertices() == meshA->nVertices());
  REQUIRE(meshB->nEdges()    == meshA->nEdges());
  REQUIRE(meshB->nFaces()    == meshA->nFaces());
}

// ===========================================================================
// Positional golden digest (hidden)
// ===========================================================================
//
// Every assertion above is a topology invariant -- boundary edges, chi, face
// count -- and all three survive a vertex moving. That is exactly the blind
// spot when a change to the sign oracle or the QEF is under review: the suite
// goes green while the geometry drifts.
//
// This case prints a positional digest instead of asserting one. Run it either
// side of such a change and diff the two dumps; there is no checked-in golden,
// because the numbers are not a contract -- "unchanged across this edit" is.
// Hidden ([.]) so it never joins the default run.
//
//   dualc_tests "[golden]" --success | tee before.txt
//
// Determinism across runs is pinned separately by the threading-determinism
// test (audit item #30).

namespace {

// FNV-1a over the raw bytes of every vertex coordinate and face index. Raw
// bytes, not a rounded print: the point is to notice a 1-ULP drift.
std::uint64_t fnv1a(std::uint64_t h, const void* data, std::size_t n) {
  const auto* p = static_cast<const unsigned char*>(data);
  for (std::size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}

void dumpDigest(const std::string& name, dce::MeshAndGeom mg, int maxDepth) {
  auto& [mesh, geom] = mg;
  SamplerParams sp;
  sp.maxDepth = maxDepth;
  auto [outMesh, outGeom, outNormals] = dualContourMesh(*mesh, *geom, sp, {});
  REQUIRE(outMesh != nullptr);

  std::uint64_t h = 14695981039346656037ull;
  for (auto v : outMesh->vertices()) {
    const geometrycentral::Vector3 p = outGeom->vertexPositions[v];
    const double xyz[3] = {p.x, p.y, p.z};
    h = fnv1a(h, xyz, sizeof(xyz));
  }
  for (auto f : outMesh->faces())
    for (auto v : f.adjacentVertices()) {
      const std::size_t idx = v.getIndex();
      h = fnv1a(h, &idx, sizeof(idx));
    }

  std::ostringstream os;
  os << "GOLDEN " << name << " depth=" << maxDepth
     << " nV=" << outMesh->nVertices() << " nF=" << outMesh->nFaces()
     << " digest=" << std::hex << std::setw(16) << std::setfill('0') << h;
  WARN(os.str());
}

} // namespace

TEST_CASE("Positional digest of every demo mesh", "[.][golden]") {
  dumpDigest("icosphere",    dce::makeIcosphere(),    5);
  dumpDigest("uvsphere",     dce::makeUvSphere(),     5);
  dumpDigest("torus",        dce::makeTorus(),        6);
  dumpDigest("trefoil",      dce::makeTrefoilKnot(),  7);
  dumpDigest("genus2",       dce::makeGenus2(7),      6);
  dumpDigest("cylinder",     dce::makeCylinder(),     6);
  dumpDigest("lbracket",     dce::makeLBracket(),     5);
  dumpDigest("hexprismbore", dce::makeHexPrismBore(), 6);
}
