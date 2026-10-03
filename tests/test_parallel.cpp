// Tests for src/internal/parallel.h and the guarantees built on top of it.
//
// Two things are pinned here, both of which the library previously only
// promised in a comment:
//   1. Output is bit-identical regardless of numThreads (SamplerParams docs
//      the guarantee; nothing tested it -- `numThreads` did not appear
//      anywhere under tests/).
//   2. An exception thrown by fn propagates out of parallelFor instead of
//      reaching std::terminate.

#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"

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
    // Not a timing assertion: with 1e6 indices, a run that ignored the failure
    // flag would execute essentially all of them. Asserting "fewer than all"
    // is enough to show the early-out works, and cannot flake the other way.
    constexpr std::size_t kN = 1000000;
    std::atomic<std::size_t> ran{0};
    REQUIRE_THROWS_AS(parallelFor(kN, 4,
                                  [&](std::size_t i) {
                                    ran.fetch_add(1, std::memory_order_relaxed);
                                    if (i == 0) throw std::runtime_error("early");
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
    std::atomic<std::size_t> ran{0};
    REQUIRE_THROWS_AS(parallelForPolled(
                          100000, 4,
                          [&](std::size_t i) {
                            ran.fetch_add(1, std::memory_order_relaxed);
                            if (i == 10) throw std::runtime_error("early");
                          },
                          [](std::size_t, std::size_t) {}),
                      std::runtime_error);
    REQUIRE(ran.load() < 100000u);
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
