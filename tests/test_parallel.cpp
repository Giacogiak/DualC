// Tests for src/internal/parallel.h and the guarantees built on top of it.
//
// Two things are pinned here, both of which the library previously only
// promised in a comment:
//   1. Output is bit-identical regardless of numThreads (SamplerParams docs
//      the guarantee; nothing tested it -- `numThreads` did not appear
//      anywhere under tests/). Since #34 the same holds for
//      ContourerParams::numThreads and the two bakes' thread count, and a
//      contourer at numThreads = 1 is shown to really run its solves serially.
//   2. An exception thrown by fn propagates out of parallelFor instead of
//      reaching std::terminate.

#include "dualc/contourer.h"
#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/progress.h"
#include "dualc/sampler.h"

#include "demo_meshes.h"
#include "internal/parallel.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace dualc;

namespace {

// A field with enough surface area to spread real work across the sampler's
// frontier: a torus unioned with an offset sphere, so the octree is neither
// trivially small nor symmetric about the split planes.
FieldPtr makeBusyField() {
  auto torus  = std::make_shared<TorusField>(Vector3{0.0, 0.0, 0.0}, 0.7, 0.25);
  auto sphere = std::make_shared<SphereField>(Vector3{0.4, 0.3, 0.2}, 0.45);
  return unionOf(torus, sphere);
}

struct ContourResult {
  std::size_t          nVertices = 0;
  std::size_t          nFaces    = 0;
  std::vector<Vector3> positions;
  std::vector<Vector3> normals;
};

ContourResult contourWithThreads(const ImplicitField& field, unsigned threads) {
  SamplerParams sp;
  sp.maxDepth   = 6;
  sp.numThreads = threads;
  ContourerParams cp;

  auto [mesh, geom, normals] = dualContourField(field, sp, cp);
  REQUIRE(mesh != nullptr);
  REQUIRE(geom != nullptr);

  ContourResult r;
  r.nVertices = mesh->nVertices();
  r.nFaces    = mesh->nFaces();
  r.normals   = normals;
  r.positions.reserve(r.nVertices);
  for (auto v : mesh->vertices()) r.positions.push_back(geom->inputVertexPositions[v]);
  return r;
}

// Bit-identical, not approximately equal: the guarantee is that no reduction
// and no scheduling-order-dependent floating-point arithmetic exists on this
// path, so anything short of exact equality would mean the guarantee is false.
void requireIdentical(const ContourResult& a, const ContourResult& b) {
  REQUIRE(a.nVertices == b.nVertices);
  REQUIRE(a.nFaces == b.nFaces);
  REQUIRE(a.positions.size() == b.positions.size());
  REQUIRE(a.normals.size() == b.normals.size());
  for (std::size_t i = 0; i < a.positions.size(); ++i) {
    INFO("vertex " << i);
    REQUIRE(a.positions[i].x == b.positions[i].x);
    REQUIRE(a.positions[i].y == b.positions[i].y);
    REQUIRE(a.positions[i].z == b.positions[i].z);
  }
  for (std::size_t i = 0; i < a.normals.size(); ++i) {
    INFO("normal " << i);
    REQUIRE(a.normals[i].x == b.normals[i].x);
    REQUIRE(a.normals[i].y == b.normals[i].y);
    REQUIRE(a.normals[i].z == b.normals[i].z);
  }
}

} // namespace

TEST_CASE("Contouring is bit-identical regardless of numThreads",
          "[parallel][determinism]") {
  auto field = makeBusyField();

  const ContourResult serial = contourWithThreads(*field, 1);
  REQUIRE(serial.nVertices > 0);
  REQUIRE(serial.nFaces > 0);

  // Two threads: the frontier is split, so any order dependence shows up here.
  requireIdentical(serial, contourWithThreads(*field, 2));

  // 0 == "all hardware threads", the default every caller actually gets.
  requireIdentical(serial, contourWithThreads(*field, 0));

  // A thread count above the work item count exercises the clamp in
  // parallelFor (threads > n) rather than a fresh code path in the sampler.
  requireIdentical(serial, contourWithThreads(*field, 64));
}

namespace {

ContourResult contourOctreeWithThreads(const HermiteOctree& octree,
                                       unsigned threads) {
  ContourerParams cp;
  cp.numThreads = threads;
  auto [mesh, geom, normals] = contourHermiteOctree(octree, cp);
  REQUIRE(mesh != nullptr);

  ContourResult r;
  r.nVertices = mesh->nVertices();
  r.nFaces    = mesh->nFaces();
  r.normals   = normals;
  for (auto v : mesh->vertices()) r.positions.push_back(geom->inputVertexPositions[v]);
  return r;
}

void requireSameGrid(const FieldPtr& a, const FieldPtr& b) {
  const auto* ga = dynamic_cast<const GridField*>(a.get());
  const auto* gb = dynamic_cast<const GridField*>(b.get());
  REQUIRE(ga != nullptr);
  REQUIRE(gb != nullptr);
  REQUIRE(ga->values() == gb->values());  // float ==, element by element
}

// Records the Contour stage's reports only.
struct ContourReports final : ProgressSink {
  std::vector<std::size_t> done;
  std::size_t total = 0;
  void report(Stage stage, std::size_t d, std::size_t t) noexcept override {
    if (stage != Stage::Contour) return;
    done.push_back(d);
    total = t;
  }
};

} // namespace

// #34: the contourer's QEF pre-solve takes its thread count from
// ContourerParams. One octree, contoured at several counts, gives the same
// mesh -- the sampler is out of the picture.
TEST_CASE("The contourer is bit-identical regardless of its numThreads",
          "[parallel][determinism]") {
  auto field = makeBusyField();
  SamplerParams sp;
  sp.maxDepth = 6;
  const HermiteOctree octree = sampleFieldToHermiteOctree(*field, sp);

  const ContourResult serial = contourOctreeWithThreads(octree, 1);
  REQUIRE(serial.nFaces > 0);
  requireIdentical(serial, contourOctreeWithThreads(octree, 2));
  requireIdentical(serial, contourOctreeWithThreads(octree, 0));
  requireIdentical(serial, contourOctreeWithThreads(octree, 64));
}

// The proof that the count reaches the contourer, not just that the output is
// unchanged: parallelForPolled's serial path (threads <= 1) polls after every
// max(1, n/64) items, so the pre-solve's half of the Contour stage reports
// exactly every multiple of that step below n. The threaded path polls on a
// 100 ms timer instead and cannot produce that sequence for this octree.
// Before #34 the pre-solve always ran on all hardware threads.
TEST_CASE("ContourerParams::numThreads = 1 runs the leaf solves serially",
          "[parallel][progress]") {
  auto field = makeBusyField();
  SamplerParams sp;
  sp.maxDepth = 6;
  const HermiteOctree octree = sampleFieldToHermiteOctree(*field, sp);

  ContourerParams cp;
  cp.numThreads = 1;
  ContourReports sink;
  contourHermiteOctree(octree, cp, nullptr, nullptr, &sink);

  REQUIRE(sink.total % 2 == 0);
  const std::size_t n = sink.total / 2;  // leaves; the stage total is 2n
  REQUIRE(n >= 128);                     // enough for >= 63 serial polls
  const std::size_t every = n / 64 == 0 ? 1 : n / 64;

  std::vector<std::size_t> expected, seen;
  for (std::size_t k = every; k < n; k += every) expected.push_back(k);
  for (std::size_t d : sink.done)
    if (d > 0 && d < n) seen.push_back(d);
  REQUIRE(seen == expected);
}

// #34: both bakes take a thread count; every voxel is written by one worker,
// so the grid is the same at any count.
TEST_CASE("bakeToGrid and MeshSource::bakeToGrid are bit-identical regardless "
          "of their thread count", "[parallel][determinism][grid]") {
  const BBox region{Vector3{-1.2, -1.2, -1.2}, Vector3{1.2, 1.2, 1.2}};

  SECTION("bakeToGrid") {
    auto field = makeBusyField();
    const FieldPtr serial = bakeToGrid(*field, region, 40, 1);
    requireSameGrid(serial, bakeToGrid(*field, region, 40, 2));
    requireSameGrid(serial, bakeToGrid(*field, region, 40, 0));
    requireSameGrid(serial, bakeToGrid(*field, region, 40));
  }

  SECTION("MeshSource::bakeToGrid (narrow band)") {
    auto [mesh, geom] = dce::makeTorus(0.8, 0.3);
    MeshSource src(*mesh, *geom);
    const Vector3i res{36, 36, 36};
    const FieldPtr serial = src.bakeToGrid(region, res, 0.2, 1);
    requireSameGrid(serial, src.bakeToGrid(region, res, 0.2, 3));
    requireSameGrid(serial, src.bakeToGrid(region, res, 0.2, 0));
    requireSameGrid(serial, src.bakeToGrid(region, res, 0.2));
  }
}

TEST_CASE("parallelFor propagates an exception instead of terminating",
          "[parallel][exceptions]") {
  using internal::parallelFor;

  SECTION("a throwing body on the serial path still propagates") {
    REQUIRE_THROWS_AS(parallelFor(4, 1, [](std::size_t) {
                        throw std::runtime_error("boom");
                      }),
                      std::runtime_error);
  }

  SECTION("a throwing body on the threaded path propagates, and joins") {
    // Every index throws, so both the spawned workers and the calling thread's
    // own worker() run hit the catch. Before the fix, the first of those called
    // std::terminate from a std::thread entry point and the second unwound past
    // the join loop, destroying joinable threads -- also std::terminate.
    REQUIRE_THROWS_AS(parallelFor(1024, 8, [](std::size_t) {
                        throw std::runtime_error("boom");
                      }),
                      std::runtime_error);
  }

  SECTION("one failing index out of many still propagates") {
    std::atomic<int> ran{0};
    REQUIRE_THROWS_AS(parallelFor(1024, 8,
                                  [&](std::size_t i) {
                                    ran.fetch_add(1, std::memory_order_relaxed);
                                    if (i == 500) throw std::logic_error("one");
                                  }),
                      std::logic_error);
    REQUIRE(ran.load() > 0);
  }

  SECTION("workers stop claiming work once one has failed") {
    // Index 0 throws; every other index parks its worker for 1 ms before
    // returning, so draining kN indices on the three free workers takes ~30 s
    // and the thrower's catch runs long before that. The bound therefore
    // fails only when a worker ignores the flag, not when the thrower is
    // descheduled for a timeslice -- which is how the bare `ran < kN` of the
    // sibling parallelForPolled case flaked (D-52): a 2-core box let three
    // free workers claim 1e5 indices in the time one throw took to unwind.
    constexpr std::size_t kN = 100000;
    std::atomic<std::size_t> ran{0};
    REQUIRE_THROWS_AS(parallelFor(kN, 4,
                                  [&](std::size_t i) {
                                    ran.fetch_add(1, std::memory_order_relaxed);
                                    if (i == 0) throw std::runtime_error("early");
                                    std::this_thread::sleep_for(
                                        std::chrono::milliseconds(1));
                                  }),
                      std::runtime_error);
    REQUIRE(ran.load() < kN);
  }

  SECTION("a successful run is unaffected") {
    std::atomic<std::size_t> sum{0};
    parallelFor(1000, 8, [&](std::size_t i) {
      sum.fetch_add(i, std::memory_order_relaxed);
    });
    REQUIRE(sum.load() == 999u * 1000u / 2u);
  }
}

// parallelForPolled is parallelFor with a caller-side poll: same work, same
// exception behaviour, plus the guarantee that the poll runs on the calling
// thread only and ends at (n, n). That guarantee is what lets a ProgressSink
// see the inside of a parallel region (dualc/progress.h).
TEST_CASE("parallelForPolled does the same work and polls on the caller only",
          "[parallel][progress]") {
  using internal::parallelForPolled;
  using std::chrono::milliseconds;

  struct Poll {
    std::size_t done, total;
    std::thread::id thread;
  };

  for (unsigned threads : {1u, 2u, 8u}) {
    DYNAMIC_SECTION("threads=" << threads) {
      std::atomic<std::size_t> sum{0};
      std::vector<Poll> polls;
      parallelForPolled(
          1000, threads,
          [&](std::size_t i) { sum.fetch_add(i, std::memory_order_relaxed); },
          [&](std::size_t done, std::size_t total) {
            polls.push_back({done, total, std::this_thread::get_id()});
          },
          milliseconds(1));
      REQUIRE(sum.load() == 999u * 1000u / 2u);
      REQUIRE(!polls.empty());
      REQUIRE(polls.back().done == 1000u);
      REQUIRE(polls.back().total == 1000u);
      for (std::size_t k = 0; k < polls.size(); ++k) {
        INFO("poll " << k);
        REQUIRE(polls[k].total == 1000u);
        REQUIRE(polls[k].thread == std::this_thread::get_id());
        if (k > 0) REQUIRE(polls[k].done >= polls[k - 1].done);
      }
    }
  }

  SECTION("a throwing body propagates and joins, on the threaded path") {
    // Deterministic early-out: index 0 throws, every other index parks its
    // worker until the poll has run. With an interval of seconds the poll runs
    // only once the caller has seen `failed`, and a released worker re-checks
    // that flag before claiming, so at most one index per worker ever runs.
    // The previous bound, `ran < n` with no park, raced the thrower's unwind
    // against three free workers draining n = 1e5 and lost 23 times in 400 on
    // two cores (D-52, the one CI flake). The 10 s deadline on the park only
    // keeps a broken failure->poll wake from hanging the case.
    using std::chrono::seconds;
    using std::chrono::steady_clock;
    constexpr unsigned kThreads = 4;
    std::atomic<bool>        released{false};
    std::atomic<std::size_t> ran{0};
    REQUIRE_THROWS_AS(parallelForPolled(
                          100000, kThreads,
                          [&](std::size_t i) {
                            ran.fetch_add(1, std::memory_order_relaxed);
                            if (i == 0) throw std::runtime_error("early");
                            const auto deadline = steady_clock::now() + seconds(10);
                            while (!released.load() && steady_clock::now() < deadline)
                              std::this_thread::yield();
                          },
                          [&](std::size_t, std::size_t) { released.store(true); },
                          seconds(10)),
                      std::runtime_error);
    REQUIRE(released.load());
    REQUIRE(ran.load() <= kThreads);
  }

  SECTION("a throwing body propagates on the serial path") {
    REQUIRE_THROWS_AS(parallelForPolled(
                          4, 1, [](std::size_t) { throw std::logic_error("x"); },
                          [](std::size_t, std::size_t) {}),
                      std::logic_error);
  }

  SECTION("n == 0 still ends at (0, 0)") {
    std::size_t last = 99;
    parallelForPolled(
        0, 8, [](std::size_t) { FAIL("no work expected"); },
        [&](std::size_t done, std::size_t total) { last = done + total; });
    REQUIRE(last == 0u);
  }
}
