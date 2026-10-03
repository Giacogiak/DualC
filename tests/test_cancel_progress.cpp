// Tests for dualc/progress.h -- cooperative cancellation and coarse progress
// through the pipeline (roadmap 14 #48).
//
// Pinned here:
//   1. a requested CancelToken makes every driver throw dualc::Cancelled at
//      its next checkpoint, from inside the parallel sampler as well as from
//      the serial contour traversal, and a token requested before the call
//      cancels before the field is queried at all;
//   2. the hooks are inert when unused: a never-requested token plus a
//      recording sink yields output bit-identical to a call without them, at
//      every thread count;
//   3. the report shape: per stage `done` is monotonic, `total` constant, the
//      last report is (total, total), and Sample precedes Contour.
// The writers' half (rc 3, the `.part` guarantee) is in the second part of
// this file.

#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/progress.h"

#include "example_common.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace dualc;

namespace {

// Records every report verbatim.
struct RecordingSink final : ProgressSink {
  struct Entry {
    Stage stage;
    std::size_t done, total;
  };
  std::vector<Entry> entries;
  void report(Stage stage, std::size_t done,
              std::size_t total) noexcept override {
    entries.push_back({stage, done, total});
  }
  std::size_t count(Stage s) const {
    std::size_t n = 0;
    for (const auto& e : entries) n += (e.stage == s);
    return n;
  }
};

// Requests its token on the `nth` report of `stage` (1-based). The request
// is legal from inside a report -- the sink runs on the calling thread -- and
// deterministic, which is what makes "cancel mid-stage" testable.
struct CancelAtStage final : ProgressSink {
  CancelToken* token;
  Stage stage;
  std::size_t nth;
  std::size_t seen = 0;
  CancelAtStage(CancelToken* t, Stage s, std::size_t n)
      : token(t), stage(s), nth(n) {}
  void report(Stage s, std::size_t, std::size_t) noexcept override {
    if (s == stage && ++seen == nth) token->request();
  }
};

// A sphere that counts how often it is asked anything, so "cancelled before
// any work" is an observable fact rather than a timing claim.
class CountingSphere final : public ImplicitField {
 public:
  mutable std::atomic<std::size_t> queries{0};
  double valueAt(const Vector3& p) const override {
    queries.fetch_add(1, std::memory_order_relaxed);
    return p.norm() - 0.4;
  }
  Vector3 gradientAt(const Vector3& p) const override {
    queries.fetch_add(1, std::memory_order_relaxed);
    const double n = p.norm();
    return (n > 1e-12) ? p * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
  }
  BBox bounds() const override {
    return BBox{Vector3{-0.4, -0.4, -0.4}, Vector3{0.4, 0.4, 0.4}};
  }
  bool cellOverlaps(const BBox& cell) const override {
    queries.fetch_add(1, std::memory_order_relaxed);
    return ImplicitField::cellOverlaps(cell);
  }
};

const BBox kBox{Vector3{-1.0, -1.0, -1.0}, Vector3{1.0, 1.0, 1.0}};

SamplerParams samplerAt(int maxDepth, unsigned threads = 0) {
  SamplerParams sp;
  sp.maxDepth   = maxDepth;
  sp.minDepth   = 2;
  sp.rootBounds = kBox;
  sp.numThreads = threads;
  return sp;
}

FieldPtr makeBusyField() {
  auto torus  = std::make_shared<TorusField>(Vector3{0.0, 0.0, 0.0}, 0.7, 0.25);
  auto sphere = std::make_shared<SphereField>(Vector3{0.4, 0.3, 0.2}, 0.45);
  return unionOf(torus, sphere);
}

struct Snapshot {
  std::size_t nFaces = 0;
  std::vector<Vector3> positions, normals;
};

Snapshot contour(const ImplicitField& field, const SamplerParams& sp,
                 const ContourerParams& cp, const CancelToken* cancel,
                 ProgressSink* progress) {
  auto [mesh, geom, normals] =
      dualContourField(field, sp, cp, nullptr, cancel, progress);
  REQUIRE(mesh != nullptr);
  Snapshot s;
  s.nFaces  = mesh->nFaces();
  s.normals = normals;
  for (auto v : mesh->vertices())
    s.positions.push_back(geom->inputVertexPositions[v]);
  return s;
}

void requireIdentical(const Snapshot& a, const Snapshot& b) {
  REQUIRE(a.nFaces == b.nFaces);
  REQUIRE(a.positions.size() == b.positions.size());
  REQUIRE(a.normals.size() == b.normals.size());
  for (std::size_t i = 0; i < a.positions.size(); ++i) {
    INFO("vertex " << i);
    REQUIRE(a.positions[i].x == b.positions[i].x);
    REQUIRE(a.positions[i].y == b.positions[i].y);
    REQUIRE(a.positions[i].z == b.positions[i].z);
    REQUIRE(a.normals[i].x == b.normals[i].x);
    REQUIRE(a.normals[i].y == b.normals[i].y);
    REQUIRE(a.normals[i].z == b.normals[i].z);
  }
}

} // namespace

// ---------------------------------------------------------------------------
// Cancellation
// ---------------------------------------------------------------------------

TEST_CASE("a token requested before the call cancels before any field query",
          "[cancel]") {
  CountingSphere field;
  CancelToken token;
  token.request();
  REQUIRE(token.requested());
  ContourerParams cp;
  REQUIRE_THROWS_AS(dualContourField(field, samplerAt(5), cp, nullptr, &token),
                    Cancelled);
  REQUIRE(field.queries.load() == 0u);
}

TEST_CASE("Cancelled names the stage that noticed", "[cancel]") {
  CancelToken token;
  token.request();
  ContourerParams cp;
  CountingSphere field;
  try {
    dualContourField(field, samplerAt(5), cp, nullptr, &token);
    FAIL("expected Cancelled");
  } catch (const Cancelled& e) {
    REQUIRE(std::string(e.what()) == "cancelled (sampling)");
  }
}

TEST_CASE("a request during the parallel sampler unwinds through parallelFor",
          "[cancel][parallel]") {
  auto field = makeBusyField();
  ContourerParams cp;
  for (unsigned threads : {1u, 2u, 0u}) {
    DYNAMIC_SECTION("threads=" << threads) {
      CancelToken token;
      // The first Sample report is (0, n), before the parallel build; a
      // request there is seen by the first internal-node checkpoint.
      CancelAtStage sink(&token, Stage::Sample, 1);
      try {
        dualContourField(*field, samplerAt(7, threads), cp, nullptr, &token,
                         &sink);
        FAIL("expected Cancelled");
      } catch (const Cancelled& e) {
        REQUIRE(std::string(e.what()) == "cancelled (sampling)");
      }
    }
  }
}

TEST_CASE("a request during the contourer unwinds from the contour stage",
          "[cancel]") {
  auto field = makeBusyField();
  ContourerParams cp;
  CancelToken token;
  // The first Contour report is (0, 2N), before the QEF pre-pass.
  CancelAtStage sink(&token, Stage::Contour, 1);
  try {
    dualContourField(*field, samplerAt(6), cp, nullptr, &token, &sink);
    FAIL("expected Cancelled");
  } catch (const Cancelled& e) {
    REQUIRE(std::string(e.what()) == "cancelled (contouring)");
  }
}

TEST_CASE("a request during the collapse pass unwinds from it", "[cancel]") {
  auto field = makeBusyField();
  ContourerParams cp;
  cp.simplificationError = 1e-4;
  CancelToken token;
  // The last Sample report is (n, n), after the build and before the
  // collapse pass runs; the collapse checkpoint is the next one.
  RecordingSink probe;
  contour(*field, samplerAt(6), cp, nullptr, &probe);
  const std::size_t nSample = probe.count(Stage::Sample);
  REQUIRE(nSample >= 2);
  CancelAtStage sink(&token, Stage::Sample, nSample);
  try {
    dualContourField(*field, samplerAt(6), cp, nullptr, &token, &sink);
    FAIL("expected Cancelled");
  } catch (const Cancelled& e) {
    REQUIRE(std::string(e.what()) == "cancelled (collapsing)");
  }
}

TEST_CASE("the hand-rolled drivers honour the token too", "[cancel]") {
  auto field = makeBusyField();
  CancelToken token;
  HermiteOctree octree = sampleFieldToHermiteOctree(*field, samplerAt(6));
  token.request();
  ContourerParams cp;
  REQUIRE_THROWS_AS(contourHermiteOctree(octree, cp, nullptr, &token),
                    Cancelled);
  cp.simplificationError = 1e-4;
  REQUIRE_THROWS_AS(simplifyHermiteOctree(octree, cp, &token), Cancelled);
  REQUIRE_THROWS_AS(sampleFieldToHermiteOctree(*field, samplerAt(6), nullptr,
                                               &token),
                    Cancelled);
}

// ---------------------------------------------------------------------------
// Inertness and the report shape
// ---------------------------------------------------------------------------

TEST_CASE("an unrequested token and a sink change nothing about the output",
          "[cancel][progress][determinism]") {
  auto field = makeBusyField();
  ContourerParams cp;
  const Snapshot plain = contour(*field, samplerAt(6, 1), cp, nullptr, nullptr);
  REQUIRE(plain.nFaces > 0);
  for (unsigned threads : {1u, 2u, 0u}) {
    DYNAMIC_SECTION("threads=" << threads) {
      CancelToken token;
      RecordingSink sink;
      requireIdentical(plain,
                       contour(*field, samplerAt(6, threads), cp, &token, &sink));
      REQUIRE_FALSE(token.requested());
      REQUIRE(!sink.entries.empty());
    }
  }
}

TEST_CASE("reports are monotonic per stage, end at (total, total), and Sample "
          "precedes Contour",
          "[progress]") {
  auto field = makeBusyField();
  ContourerParams cp;
  for (unsigned threads : {1u, 0u}) {
    DYNAMIC_SECTION("threads=" << threads) {
      RecordingSink sink;
      contour(*field, samplerAt(6, threads), cp, nullptr, &sink);

      REQUIRE(sink.count(Stage::Sample) >= 2);
      REQUIRE(sink.count(Stage::Contour) >= 2);
      REQUIRE(sink.count(Stage::Write) == 0);
      REQUIRE(sink.count(Stage::Tile) == 0);

      // Stage order: every Sample report comes before every Contour report.
      std::size_t lastSample = 0, firstContour = sink.entries.size();
      for (std::size_t i = 0; i < sink.entries.size(); ++i) {
        if (sink.entries[i].stage == Stage::Sample) lastSample = i;
        if (sink.entries[i].stage == Stage::Contour && i < firstContour)
          firstContour = i;
      }
      REQUIRE(lastSample < firstContour);

      for (Stage stage : {Stage::Sample, Stage::Contour}) {
        std::size_t total = 0, prev = 0;
        bool first = true;
        const RecordingSink::Entry* last = nullptr;
        for (const auto& e : sink.entries) {
          if (e.stage != stage) continue;
          if (first) {
            total = e.total;
            first = false;
            REQUIRE(e.done == 0u);
          }
          REQUIRE(e.total == total);
          REQUIRE(e.done >= prev);
          REQUIRE(e.done <= total);
          prev = e.done;
          last = &e;
        }
        REQUIRE(last != nullptr);
        REQUIRE(last->done == total);
        REQUIRE(total > 0u);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// The writers: rc 3 and the `.part` guarantee (examples/example_common.cpp)
// ---------------------------------------------------------------------------

namespace {

bool exists(const std::string& p) { return std::filesystem::exists(p); }

// Every file a cancelled or failed export could have left behind for `path`.
std::vector<std::string> residue(const std::string& path) {
  return {path, path + ".part", path + ".part.model.tmp",
          path + ".part.verts.tmp", path + ".part.tris.tmp"};
}

void requireNoResidue(const std::string& path) {
  for (const auto& p : residue(path)) {
    INFO(p);
    REQUIRE_FALSE(exists(p));
  }
}

void removeResidue(const std::string& path) {
  for (const auto& p : residue(path)) std::remove(p.c_str());
}

// A dyadic case the streaming tests use: a sphere well inside a 12^3 box.
struct WriterCase {
  FieldPtr field = dce::buildPrimitive("sphere", {6.0, 6.0, 6.0, 1.8});
  SamplerParams sp;
  ContourerParams cp;
  WriterCase() {
    sp.maxDepth   = 5;
    sp.rootBounds = BBox{Vector3{0.0, 0.0, 0.0}, Vector3{12.0, 12.0, 12.0}};
  }
};

} // namespace

TEST_CASE("a cancelled monolithic export returns 3 and leaves nothing",
          "[cancel][writers]") {
  WriterCase w;
  for (const char* ext : {".stl", ".3mf", ".obj"}) {
    DYNAMIC_SECTION("format " << ext) {
      const std::string path = std::string("cancel_mono") + ext;
      removeResidue(path);
      CancelToken token;
      CancelAtStage sink(&token, Stage::Contour, 1);
      REQUIRE(dce::writeField(*w.field, path, w.sp, w.cp, {}, nullptr, &token,
                              &sink) == 3);
      requireNoResidue(path);
    }
  }
}

TEST_CASE("a token requested before a monolithic export cancels it before "
          "the field is sampled",
          "[cancel][writers]") {
  CountingSphere field;
  const std::string path = "cancel_pre.stl";
  removeResidue(path);
  CancelToken token;
  token.request();
  REQUIRE(dce::writeField(field, path, samplerAt(5), ContourerParams{}, {},
                          nullptr, &token) == 3);
  REQUIRE(field.queries.load() == 0u);
  requireNoResidue(path);
}

TEST_CASE("a pre-existing destination survives a cancelled re-export and is "
          "replaced by a successful one",
          "[cancel][writers]") {
  WriterCase w;
  const std::string path = "cancel_keep.stl";
  removeResidue(path);
  {
    std::ofstream marker(path, std::ios::binary);
    marker << "marker";
  }
  CancelToken token;
  CancelAtStage sink(&token, Stage::Sample, 1);
  REQUIRE(dce::writeField(*w.field, path, w.sp, w.cp, {}, nullptr, &token,
                          &sink) == 3);
  REQUIRE(exists(path));
  {
    std::ifstream in(path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    REQUIRE(content == "marker");
  }
  REQUIRE_FALSE(exists(path + ".part"));

  // A successful export replaces it (std::filesystem::rename overwrites).
  REQUIRE(dce::writeField(*w.field, path, w.sp, w.cp) == 0);
  REQUIRE(exists(path));
  REQUIRE(std::filesystem::file_size(path) > 84u);
  REQUIRE_FALSE(exists(path + ".part"));
  removeResidue(path);
}

TEST_CASE("a monolithic export reports Sample, Contour, then Write (0,1) (1,1)",
          "[progress][writers]") {
  WriterCase w;
  const std::string path = "progress_mono.stl";
  removeResidue(path);
  RecordingSink sink;
  REQUIRE(dce::writeField(*w.field, path, w.sp, w.cp, {}, nullptr, nullptr,
                          &sink) == 0);
  REQUIRE(sink.count(Stage::Sample) >= 2);
  REQUIRE(sink.count(Stage::Contour) >= 2);
  REQUIRE(sink.count(Stage::Write) == 2);
  REQUIRE(sink.count(Stage::Tile) == 0);
  const auto& e = sink.entries;
  REQUIRE(e[e.size() - 2].stage == Stage::Write);
  REQUIRE(e[e.size() - 2].done == 0u);
  REQUIRE(e[e.size() - 2].total == 1u);
  REQUIRE(e.back().stage == Stage::Write);
  REQUIRE(e.back().done == 1u);
  REQUIRE(e.back().total == 1u);
  REQUIRE(exists(path));
  REQUIRE_FALSE(exists(path + ".part"));
  removeResidue(path);
}

TEST_CASE("a cancelled tiled export returns 3 and leaves nothing, for every "
          "sink",
          "[cancel][writers][streaming]") {
  WriterCase w;
  const int tileDepth = 3;  // 6^3 tiles at depth 5

  SECTION("tiled STL, cancelled at the third tile") {
    const std::string path = "cancel_tiled.stl";
    removeResidue(path);
    CancelToken token;
    CancelAtStage sink(&token, Stage::Tile, 3);
    REQUIRE(dce::writeFieldTiledStl(*w.field, path, w.sp, w.cp, tileDepth,
                                    &token, &sink) == 3);
    requireNoResidue(path);
  }
  SECTION("tiled STL, cancelled inside a tile's contour") {
    // The token is requested during tile 1's Sample stage -- the sink is not
    // forwarded per tile, so request it from a second sink instead: here the
    // token is pre-requested after the first Tile report through a wrapper.
    const std::string path = "cancel_tiled_inner.stl";
    removeResidue(path);
    CancelToken token;
    struct FirstTile final : ProgressSink {
      CancelToken* t;
      explicit FirstTile(CancelToken* tok) : t(tok) {}
      void report(Stage s, std::size_t done, std::size_t) noexcept override {
        if (s == Stage::Tile && done == 1) t->request();
      }
    } sink(&token);
    REQUIRE(dce::writeFieldTiledStl(*w.field, path, w.sp, w.cp, tileDepth,
                                    &token, &sink) == 3);
    requireNoResidue(path);
  }
  SECTION("tiled 3MF, per-tile objects") {
    const std::string path = "cancel_tiled.3mf";
    removeResidue(path);
    CancelToken token;
    CancelAtStage sink(&token, Stage::Tile, 3);
    REQUIRE(dce::writeFieldTiled3mf(*w.field, path, w.sp, w.cp, tileDepth,
                                    false, &token, &sink) == 3);
    requireNoResidue(path);
  }
  SECTION("tiled 3MF, welded") {
    const std::string path = "cancel_welded.3mf";
    removeResidue(path);
    CancelToken token;
    CancelAtStage sink(&token, Stage::Tile, 3);
    REQUIRE(dce::writeFieldTiled3mf(*w.field, path, w.sp, w.cp, tileDepth,
                                    true, &token, &sink) == 3);
    requireNoResidue(path);
  }
  SECTION("single streamed pass (tileDepth >= depth), cancelled in the "
          "contour") {
    const std::string path = "cancel_single.stl";
    removeResidue(path);
    CancelToken token;
    CancelAtStage sink(&token, Stage::Contour, 1);
    REQUIRE(dce::writeFieldTiledStl(*w.field, path, w.sp, w.cp, 6, &token,
                                    &sink) == 3);
    requireNoResidue(path);
  }
  SECTION("pre-requested token: nothing is opened") {
    const std::string path = "cancel_tiled_pre.stl";
    removeResidue(path);
    CancelToken token;
    token.request();
    REQUIRE(dce::writeFieldTiledStl(*w.field, path, w.sp, w.cp, tileDepth,
                                    &token) == 3);
    requireNoResidue(path);
  }
}

TEST_CASE("a tiled export reports Tile only, (0,T) first and (T,T) last",
          "[progress][writers][streaming]") {
  WriterCase w;
  const std::string path = "progress_tiled.stl";
  removeResidue(path);
  RecordingSink sink;
  REQUIRE(dce::writeFieldTiledStl(*w.field, path, w.sp, w.cp, 3, nullptr,
                                  &sink) == 0);
  REQUIRE(sink.count(Stage::Sample) == 0);
  REQUIRE(sink.count(Stage::Contour) == 0);
  REQUIRE(sink.count(Stage::Write) == 0);
  const std::size_t T = 6 * 6 * 6;
  REQUIRE(sink.count(Stage::Tile) == T + 1);
  REQUIRE(sink.entries.front().done == 0u);
  REQUIRE(sink.entries.front().total == T);
  REQUIRE(sink.entries.back().done == T);
  REQUIRE(sink.entries.back().total == T);
  std::size_t prev = 0;
  for (const auto& e : sink.entries) {
    REQUIRE(e.total == T);
    REQUIRE(e.done >= prev);
    prev = e.done;
  }
  REQUIRE(exists(path));
  REQUIRE_FALSE(exists(path + ".part"));
  removeResidue(path);
}

TEST_CASE("an unrequested token and a sink leave a tiled export byte-identical",
          "[cancel][progress][writers][streaming][determinism]") {
  WriterCase w;
  const std::string plain = "det_plain.stl", hooked = "det_hooked.stl";
  removeResidue(plain);
  removeResidue(hooked);
  REQUIRE(dce::writeFieldTiledStl(*w.field, plain, w.sp, w.cp, 3) == 0);
  CancelToken token;
  RecordingSink sink;
  REQUIRE(dce::writeFieldTiledStl(*w.field, hooked, w.sp, w.cp, 3, &token,
                                  &sink) == 0);
  std::string sa, sb;
  {
    std::ifstream a(plain, std::ios::binary), b(hooked, std::ios::binary);
    sa.assign((std::istreambuf_iterator<char>(a)),
              std::istreambuf_iterator<char>());
    sb.assign((std::istreambuf_iterator<char>(b)),
              std::istreambuf_iterator<char>());
  }
  removeResidue(plain);
  removeResidue(hooked);
  REQUIRE(sa.size() > 84u);
  REQUIRE(sa == sb);
}
