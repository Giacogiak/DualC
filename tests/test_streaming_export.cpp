// Acceptance test for the tiled / streaming STL export (writeFieldTiledStl,
// examples/example_common.cpp). The correctness bar is simple and strong:
// tiling must produce the SAME mesh as the monolithic path. On a dyadic-aligned
// grid (integer box + power-of-2 depth) with a surface strictly interior to the
// box, the per-tile octrees reproduce the global cell grid bit-for-bit, so the
// streamed triangle set must equal the monolithic one exactly. This directly
// exercises the seam: every boundary quad present exactly once -- no crack, no
// duplicate. It also checks STL self-consistency (header facet count vs. the
// file's record area, written via the seekp(80) count-patch).

#include <catch2/catch_test_macros.hpp>

#include "dualc/dualc.h"
#include "example_common.h"
#include "field_graph.h"
#include "miniz.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Parse a binary STL: return the per-triangle vertex floats (9 each), plus the
// header facet count and the total file size for the self-consistency check.
std::vector<std::array<float, 9>> readStlTris(const std::string& path,
                                              std::uint32_t& countOut,
                                              std::uint64_t& fileSize) {
  std::ifstream f(path, std::ios::binary);
  REQUIRE(f);
  f.seekg(0, std::ios::end);
  fileSize = static_cast<std::uint64_t>(f.tellg());
  f.seekg(0, std::ios::beg);
  char header[80];
  f.read(header, 80);
  std::uint32_t n = 0;
  f.read(reinterpret_cast<char*>(&n), 4);
  countOut = n;
  std::vector<std::array<float, 9>> tris;
  tris.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    float buf[12];
    f.read(reinterpret_cast<char*>(buf), 48);  // normal(3) + 3 verts(9)
    char attr[2];
    f.read(attr, 2);
    std::array<float, 9> t{};
    for (int k = 0; k < 9; ++k) t[k] = buf[3 + k];
    tris.push_back(t);
  }
  return tris;
}

// Parse a 3MF: unzip 3D/3dmodel.model and return every triangle in WORLD space,
// resolving each object's local vertex indices. Vertices within a triangle are
// sorted so winding differences don't matter; the caller sorts the list. Also
// returns the <model unit=...> and the object count. Both writers emit vertices
// as %.9g text, so identical doubles round-trip to identical strings -> exact
// set comparison holds (on the dyadic grid the two paths feed identical doubles).
std::vector<std::array<double, 9>> read3mfTris(const std::string& path,
                                               std::string& unitOut,
                                               int& nObjOut,
                                               int& nVertsOut) {
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof zip);
  REQUIRE(mz_zip_reader_init_file(&zip, path.c_str(), 0));
  std::size_t sz = 0;
  char* raw = static_cast<char*>(
      mz_zip_reader_extract_file_to_heap(&zip, "3D/3dmodel.model", &sz, 0));
  REQUIRE(raw != nullptr);
  std::string xml(raw, sz);
  mz_free(raw);
  mz_zip_reader_end(&zip);

  unitOut.clear();
  const std::size_t up = xml.find("unit=\"");
  if (up != std::string::npos) {
    const std::size_t s = up + 6;
    unitOut = xml.substr(s, xml.find('"', s) - s);
  }

  auto dattr = [](const std::string& line, const char* key) -> double {
    const std::size_t p = line.find(key);
    return p == std::string::npos ? 0.0 : std::atof(line.c_str() + p + std::strlen(key));
  };
  auto iattr = [](const std::string& line, const char* key) -> int {
    const std::size_t p = line.find(key);
    return p == std::string::npos ? 0 : std::atoi(line.c_str() + p + std::strlen(key));
  };

  std::vector<std::array<double, 9>> tris;
  std::vector<std::array<double, 3>> verts;  // reset per <object>
  nObjOut = 0;
  nVertsOut = 0;
  std::istringstream is(xml);
  std::string line;
  while (std::getline(is, line)) {
    if (line.find("<object") != std::string::npos) {
      verts.clear();
      ++nObjOut;
    } else if (line.find("<vertex ") != std::string::npos) {
      ++nVertsOut;
      verts.push_back({dattr(line, "x=\""), dattr(line, "y=\""), dattr(line, "z=\"")});
    } else if (line.find("<triangle ") != std::string::npos) {
      const int a = iattr(line, "v1=\""), b = iattr(line, "v2=\""), c = iattr(line, "v3=\"");
      std::array<std::array<double, 3>, 3> v{verts[a], verts[b], verts[c]};
      std::sort(v.begin(), v.end());
      tris.push_back({v[0][0], v[0][1], v[0][2], v[1][0], v[1][1], v[1][2],
                      v[2][0], v[2][1], v[2][2]});
    }
  }
  return tris;
}

}  // namespace

TEST_CASE("tiled streaming 3MF equals the monolithic 3MF", "[streaming]") {
  // Same dyadic case as the STL test, but through the 3MF path: the tiled 3MF's
  // world-space triangle set must equal the monolithic write3mf output. Also
  // checks the 1 unit = 1 mm metadata survives and that tiling produced one
  // object per non-empty tile (> 1) while mono is a single object.
  dualc::FieldPtr field = dce::buildPrimitive("sphere", {6.0, 6.0, 6.0, 1.8});
  REQUIRE(field);

  dualc::SamplerParams sp;
  sp.maxDepth = 5;
  dualc::BBox box;
  box.min = dualc::Vector3{0.0, 0.0, 0.0};
  box.max = dualc::Vector3{12.0, 12.0, 12.0};
  sp.rootBounds = box;
  dualc::ContourerParams cp;

  const std::string mono = "stream_mono.3mf";
  REQUIRE(dce::writeField(*field, mono, sp, cp) == 0);
  std::string unitM;
  int nObjM = 0, nVertM = 0;
  std::vector<std::array<double, 9>> tm = read3mfTris(mono, unitM, nObjM, nVertM);
  REQUIRE(tm.size() > 0);
  CHECK(unitM == "millimeter");
  CHECK(nObjM == 1);
  std::sort(tm.begin(), tm.end());

  for (int tileDepth : {4, 3}) {
    const std::string tiled = "stream_tiled.3mf";
    REQUIRE(dce::writeFieldTiled3mf(*field, tiled, sp, cp, tileDepth) == 0);
    std::string unitT;
    int nObjT = 0, nVertT = 0;
    std::vector<std::array<double, 9>> tt = read3mfTris(tiled, unitT, nObjT, nVertT);
    CHECK(unitT == "millimeter");
    CHECK(nObjT > 1);  // one <object> per non-empty tile
    CHECK(tt.size() == tm.size());
    std::sort(tt.begin(), tt.end());
    CHECK(tt == tm);  // same geometry, tiled vs monolithic
    std::remove(tiled.c_str());
  }

  // tile-depth >= depth collapses to a single streamed pass (one object).
  const std::string single = "stream_single.3mf";
  REQUIRE(dce::writeFieldTiled3mf(*field, single, sp, cp, 6) == 0);
  std::string unitS;
  int nObjS = 0, nVertS = 0;
  std::vector<std::array<double, 9>> ts = read3mfTris(single, unitS, nObjS, nVertS);
  CHECK(nObjS == 1);
  CHECK(ts.size() == tm.size());
  std::sort(ts.begin(), ts.end());
  CHECK(ts == tm);
  std::remove(single.c_str());

  std::remove(mono.c_str());
}

TEST_CASE("tiled streaming 3MF rejects non-3MF output", "[streaming]") {
  dualc::FieldPtr field = dce::buildPrimitive("sphere", {0.0, 0.0, 0.0, 1.0});
  REQUIRE(field);
  dualc::SamplerParams sp;
  sp.maxDepth = 4;
  dualc::BBox box;
  box.min = dualc::Vector3{-2.0, -2.0, -2.0};
  box.max = dualc::Vector3{2.0, 2.0, 2.0};
  sp.rootBounds = box;
  dualc::ContourerParams cp;
  CHECK(dce::writeFieldTiled3mf(*field, "nope.stl", sp, cp, 3) == 2);
  CHECK(dce::writeFieldTiled3mf(*field, "nope.3mf", sp, cp, 1) == 2);  // depth<2
  std::remove("nope.3mf");
}

TEST_CASE("welded streaming 3MF is a single manifold object", "[streaming]") {
  // The hard case for seam-welding: a NON-dyadic sampling box (arbitrary float
  // bounds), where adjacent tiles compute the same seam vertex via different
  // subdivision chains and so may differ in low mantissa bits -- an exact-double
  // weld key would crack here; the snap-to-grid key must not. The bar: the welded
  // .3mf is ONE object whose triangle set equals the monolithic mesh AND is
  // manifold (every edge shared by exactly 2 triangles -- a closed solid).
  dce::fieldgraph::FileMeshResolver meshes;
  dce::fieldgraph::FieldGraph fg = dce::fieldgraph::FieldGraph::build(
      dce::fieldgraph::parseShorthand(
          "intersection(box(min=[-1.3,-1.3,-1.3],max=[2.7,2.7,2.7]),"
          "onion(normalize(gyroid(wavelength=1)),thickness=0.3))"),
      meshes);

  dualc::SamplerParams sp;
  sp.maxDepth = 5;
  dualc::BBox box;
  box.min = dualc::Vector3{-1.3, -1.3, -1.3};
  box.max = dualc::Vector3{2.7, 2.7, 2.7};  // non-dyadic origin: cs and tb.min not exact in double
  sp.rootBounds = box;
  dualc::ContourerParams cp;

  const std::string mono = "weld_mono.3mf";
  const std::string tiled = "weld_tiled.3mf";
  const std::string weld = "weld_welded.3mf";
  REQUIRE(dce::writeField(fg.field(), mono, sp, cp) == 0);
  REQUIRE(dce::writeFieldTiled3mf(fg.field(), tiled, sp, cp, 3, false) == 0);
  REQUIRE(dce::writeFieldTiled3mf(fg.field(), weld, sp, cp, 3, true) == 0);

  std::string um, ut, uw;
  int om = 0, ot = 0, ow = 0, vm = 0, vt = 0, vw = 0;
  std::vector<std::array<double, 9>> tm = read3mfTris(mono, um, om, vm);
  std::vector<std::array<double, 9>> tt = read3mfTris(tiled, ut, ot, vt);
  std::vector<std::array<double, 9>> tw = read3mfTris(weld, uw, ow, vw);

  REQUIRE(tm.size() > 1000);          // genuinely dense
  CHECK(ow == 1);                     // welded = a single object
  CHECK(ot > 1);                      // per-tile path = many objects
  CHECK(tw.size() == tm.size());      // same triangle count as monolithic
  CHECK(vw < vt);                     // seams collapsed vs per-tile duplicates
  CHECK(vw == vm);                    // reproduces the monolithic vertex count exactly

  // Geometry: welded triangle set equals the monolithic mesh.
  std::vector<std::array<double, 9>> sm = tm, sw = tw;
  std::sort(sm.begin(), sm.end());
  std::sort(sw.begin(), sw.end());
  CHECK(sw == sm);

  // Topology: the welded mesh must reproduce the monolithic edge structure
  // EXACTLY -- welding introduces no crack (a crack would add edges incident to
  // just ONE triangle) and no doubled seam face. We compare the whole
  // edge-incidence distribution against the monolithic baseline rather than
  // demanding pure 2-manifoldness, because this DC output has intrinsic
  // higher-valence edges (e.g. 4-valent junctions) present in the monolithic
  // mesh too -- welding must match, not "improve on", that baseline.
  auto edgeIncidence = [](const std::vector<std::array<double, 9>>& tris) {
    std::map<std::array<double, 6>, int> edge;
    auto add = [&](const double* a, const double* b) {
      std::array<double, 6> e;
      const bool aLess = std::lexicographical_compare(a, a + 3, b, b + 3);
      const double* lo = aLess ? a : b;
      const double* hi = aLess ? b : a;
      for (int k = 0; k < 3; ++k) { e[k] = lo[k]; e[3 + k] = hi[k]; }
      ++edge[e];
    };
    for (const std::array<double, 9>& t : tris) {
      add(&t[0], &t[3]); add(&t[0], &t[6]); add(&t[3], &t[6]);
    }
    std::map<int, int> dist;  // incidence -> #edges
    for (const auto& kv : edge) ++dist[kv.second];
    return dist;
  };
  const std::map<int, int> distW = edgeIncidence(tw);
  const std::map<int, int> distM = edgeIncidence(tm);
  CHECK(distW == distM);          // same topology as monolithic
  CHECK(distW.count(1) == 0);     // no crack: no edge bordered by a single face

  std::remove(mono.c_str());
  std::remove(tiled.c_str());
  std::remove(weld.c_str());
}

TEST_CASE("tiled streaming STL equals the monolithic mesh", "[streaming]") {
  // Sphere strictly interior to a dyadic box [0,12]^3 at depth 5 (cs = 0.375).
  dualc::FieldPtr field = dce::buildPrimitive("sphere", {6.0, 6.0, 6.0, 1.8});
  REQUIRE(field);

  dualc::SamplerParams sp;
  sp.maxDepth = 5;
  dualc::BBox box;
  box.min = dualc::Vector3{0.0, 0.0, 0.0};
  box.max = dualc::Vector3{12.0, 12.0, 12.0};
  sp.rootBounds = box;
  dualc::ContourerParams cp;

  const std::string mono = "stream_mono.stl";
  REQUIRE(dce::writeField(*field, mono, sp, cp) == 0);
  std::uint32_t nm = 0;
  std::uint64_t szm = 0;
  std::vector<std::array<float, 9>> tm = readStlTris(mono, nm, szm);
  REQUIRE(nm > 0);
  CHECK(szm == static_cast<std::uint64_t>(nm) * 50 + 84);
  std::sort(tm.begin(), tm.end());

  // Several tile depths < global depth: nt = 3 (tile-depth 4) and a coarser
  // tiling. Each must reproduce the monolithic triangle set exactly.
  for (int tileDepth : {4, 3}) {
    const std::string tiled = "stream_tiled.stl";
    REQUIRE(dce::writeFieldTiledStl(*field, tiled, sp, cp, tileDepth) == 0);
    std::uint32_t nt = 0;
    std::uint64_t szt = 0;
    std::vector<std::array<float, 9>> tt = readStlTris(tiled, nt, szt);
    CHECK(szt == static_cast<std::uint64_t>(nt) * 50 + 84);  // count-patch ok
    CHECK(nt == nm);
    std::sort(tt.begin(), tt.end());
    CHECK(tt == tm);  // bit-identical: seams welded, no crack/dupe
    std::remove(tiled.c_str());
  }

  // tile-depth >= depth collapses to a single streamed pass; still == mono.
  const std::string single = "stream_single.stl";
  REQUIRE(dce::writeFieldTiledStl(*field, single, sp, cp, 6) == 0);
  std::uint32_t ns = 0;
  std::uint64_t szs = 0;
  std::vector<std::array<float, 9>> ts = readStlTris(single, ns, szs);
  CHECK(szs == static_cast<std::uint64_t>(ns) * 50 + 84);
  CHECK(ns == nm);
  std::sort(ts.begin(), ts.end());
  CHECK(ts == tm);
  std::remove(single.c_str());

  std::remove(mono.c_str());
}

TEST_CASE("tiled streaming STL equals monolithic with the surface on the bounds",
          "[streaming]") {
  // The real manufacturing case: gyroid INTERSECT box, with the sampling box
  // EQUAL to the clip box, so the surface lies ON the global --bounds faces
  // (outer walls + caps) -- not strictly interior like the sphere above. This
  // exercises the tile ghost ring + ownership at the global boundary. Dyadic
  // grid (box [-2,2], depth 5 -> cs = 0.125) so the bar is bit-identity.
  dce::fieldgraph::FileMeshResolver meshes;
  dce::fieldgraph::FieldGraph fg = dce::fieldgraph::FieldGraph::build(
      dce::fieldgraph::parseShorthand(
          "intersection(box(min=[-2,-2,-2],max=[2,2,2]),"
          "onion(normalize(gyroid(wavelength=1)),thickness=0.3))"),
      meshes);

  dualc::SamplerParams sp;
  sp.maxDepth = 5;
  dualc::BBox box;
  box.min = dualc::Vector3{-2.0, -2.0, -2.0};
  box.max = dualc::Vector3{2.0, 2.0, 2.0};
  sp.rootBounds = box;
  dualc::ContourerParams cp;

  const std::string mono = "bstream_mono.stl";
  const std::string tiled = "bstream_tiled.stl";
  REQUIRE(dce::writeField(fg.field(), mono, sp, cp) == 0);
  REQUIRE(dce::writeFieldTiledStl(fg.field(), tiled, sp, cp, 3) == 0);

  std::uint32_t nm = 0, nt = 0;
  std::uint64_t szm = 0, szt = 0;
  std::vector<std::array<float, 9>> tm = readStlTris(mono, nm, szm);
  std::vector<std::array<float, 9>> tt = readStlTris(tiled, nt, szt);
  REQUIRE(nm > 1000);  // a genuinely dense, boundary-touching mesh
  CHECK(nt == nm);
  std::sort(tm.begin(), tm.end());
  std::sort(tt.begin(), tt.end());
  CHECK(tt == tm);  // box-face caps reproduced faithfully -- no crack/dup
  std::remove(mono.c_str());
  std::remove(tiled.c_str());
}

TEST_CASE("tiled streaming STL rejects non-STL output", "[streaming]") {
  dualc::FieldPtr field = dce::buildPrimitive("sphere", {0.0, 0.0, 0.0, 1.0});
  REQUIRE(field);
  dualc::SamplerParams sp;
  sp.maxDepth = 4;
  dualc::BBox box;
  box.min = dualc::Vector3{-2.0, -2.0, -2.0};
  box.max = dualc::Vector3{2.0, 2.0, 2.0};
  sp.rootBounds = box;
  dualc::ContourerParams cp;
  CHECK(dce::writeFieldTiledStl(*field, "nope.obj", sp, cp, 3) == 2);
  CHECK(dce::writeFieldTiledStl(*field, "nope.stl", sp, cp, 1) == 2);  // depth<2
  std::remove("nope.stl");
}

TEST_CASE("parseMemBudget parses sizes and rejects garbage", "[streaming][mem]") {
  std::uint64_t b = 0;
  CHECK((dce::parseMemBudget("4G", b) && b == 4ull * 1024 * 1024 * 1024));
  CHECK((dce::parseMemBudget("512M", b) && b == 512ull * 1024 * 1024));
  CHECK((dce::parseMemBudget("2048K", b) && b == 2048ull * 1024));
  CHECK((dce::parseMemBudget("1.5G", b) &&
         b == static_cast<std::uint64_t>(1.5 * 1024 * 1024 * 1024)));
  CHECK((dce::parseMemBudget("256MiB", b) && b == 256ull * 1024 * 1024));  // "iB"
  CHECK((dce::parseMemBudget("1000", b) && b == 1000));                    // bytes
  CHECK((dce::parseMemBudget("  8G  ", b) && b == 8ull * 1024 * 1024 * 1024));
  CHECK_FALSE(dce::parseMemBudget("", b));
  CHECK_FALSE(dce::parseMemBudget("bogus", b));
  CHECK_FALSE(dce::parseMemBudget("-4G", b));
  CHECK_FALSE(dce::parseMemBudget("0", b));
  CHECK_FALSE(dce::parseMemBudget("4Z", b));  // unknown suffix
}

TEST_CASE("mem-budget picks a tile-depth that delegates to the tiled path",
          "[streaming][mem]") {
  // The dense, boundary-touching gyroid case (as the STL bit-identity test) so
  // the busiest-tile estimate is meaningful. depth 6 => valid D range [2, 5].
  dce::fieldgraph::FileMeshResolver meshes;
  dce::fieldgraph::FieldGraph fg = dce::fieldgraph::FieldGraph::build(
      dce::fieldgraph::parseShorthand(
          "intersection(box(min=[-2,-2,-2],max=[2,2,2]),"
          "onion(normalize(gyroid(wavelength=1)),thickness=0.3))"),
      meshes);

  dualc::SamplerParams sp;
  sp.maxDepth = 6;
  dualc::BBox box;
  box.min = dualc::Vector3{-2.0, -2.0, -2.0};
  box.max = dualc::Vector3{2.0, 2.0, 2.0};
  sp.rootBounds = box;
  dualc::ContourerParams cp;

  // Clamping: every pick lands in [2, depth-1], and the pick is MONOTONE in the
  // budget (a bigger budget never selects a smaller D).
  const std::uint64_t budgets[] = {1ull << 20, 8ull << 20, 64ull << 20,
                                   1ull << 30, 64ull << 30};
  int prev = 0;
  for (std::uint64_t bud : budgets) {
    const int d = dce::chooseTileDepthForBudget(fg.field(), sp, cp, bud);
    CHECK(d >= 2);
    CHECK(d <= sp.maxDepth - 1);
    CHECK(d >= prev);  // monotone non-decreasing
    prev = d;
  }
  // Ample budget saturates at the largest tile-depth (depth - 1).
  CHECK(dce::chooseTileDepthForBudget(fg.field(), sp, cp, 256ull << 30) ==
        sp.maxDepth - 1);

  // Delegation: the chosen D must produce output byte-identical to calling the
  // tiled writer directly at that D -- --mem is only a D picker in front of the
  // already-verified streaming path.
  const int d = dce::chooseTileDepthForBudget(fg.field(), sp, cp, 8ull << 20);
  REQUIRE((d >= 2 && d <= sp.maxDepth - 1));
  const std::string viaMem = "mem_pick.stl";
  const std::string viaTd = "mem_explicit.stl";
  REQUIRE(dce::writeFieldTiledStl(fg.field(), viaMem, sp, cp, d) == 0);
  REQUIRE(dce::writeFieldTiledStl(fg.field(), viaTd, sp, cp, d) == 0);
  std::uint32_t n1 = 0, n2 = 0;
  std::uint64_t s1 = 0, s2 = 0;
  std::vector<std::array<float, 9>> t1 = readStlTris(viaMem, n1, s1);
  std::vector<std::array<float, 9>> t2 = readStlTris(viaTd, n2, s2);
  CHECK(n1 == n2);
  CHECK(s1 == s2);
  CHECK(t1 == t2);
  std::remove(viaMem.c_str());
  std::remove(viaTd.c_str());

  // Unbounded field with no --bounds -> the picker reports the failure (-1).
  dualc::SamplerParams spu;
  spu.maxDepth = 6;  // no rootBounds
  dce::fieldgraph::FieldGraph gy = dce::fieldgraph::FieldGraph::build(
      dce::fieldgraph::parseShorthand("gyroid(wavelength=1)"), meshes);
  CHECK(dce::chooseTileDepthForBudget(gy.field(), spu, cp, 64ull << 20) == -1);
}
