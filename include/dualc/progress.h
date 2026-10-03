#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace dualc {

// Coarse pipeline stages a ProgressSink is told about. Sample and Contour are
// emitted by the engine (sampler, contourer); Write and Tile by the host-side
// writers in examples/example_common.cpp. The values are stable: the C ABI
// mirrors them as DUALC_STAGE_* and static_asserts the match.
enum class Stage : int { Sample = 0, Contour = 1, Write = 2, Tile = 3 };

// Thrown at a cancellation checkpoint once CancelToken::request() has been
// observed. Derives from std::runtime_error so existing catch(std::exception)
// sites still see it; the writers and the C ABI catch it FIRST and map it to
// their own "cancelled" status. `where` names the stage that noticed.
class Cancelled : public std::runtime_error {
 public:
  explicit Cancelled(const char* where)
      : std::runtime_error(std::string("cancelled (") + where + ")") {}
};

// Cooperative cancellation flag.
//
// request() may be called from ANY thread at any time -- the whole point is
// that a host UI thread flips it while the pipeline runs on a worker. The
// pipeline polls it at checkpoints (one relaxed load per internal octree
// node, per leaf QEF solve, per tile) and unwinds by throwing Cancelled,
// which internal::parallelFor propagates to the calling thread after every
// worker has been joined.
//
// Sticky: there is no reset -- use one token per job. It must outlive every
// call it was passed to. Passing a token that is never requested changes
// nothing about the output (the checkpoints are pure loads).
class CancelToken {
 public:
  void request() noexcept { flag_.store(true, std::memory_order_relaxed); }
  bool requested() const noexcept {
    return flag_.load(std::memory_order_relaxed);
  }
  void throwIfRequested(const char* where) const {
    if (requested()) throw Cancelled(where);
  }

 private:
  std::atomic<bool> flag_{false};
};

// Progress receiver.
//
// report() is invoked ONLY on the thread that called the pipeline entry point
// -- never from a worker -- so a sink may touch host state without locking
// (the same invariant Diagnostics documents). Within one stage `done` is
// monotonic non-decreasing, `total` is constant, and the last report is
// (total, total). Reports arrive at stage boundaries and, inside a parallel
// region, at most every ~100 ms.
//
// It must not throw: `noexcept` here is what guarantees Cancelled is the only
// exception that ever unwinds a parallel region on the pipeline's behalf. A
// sink that wants to stop the job requests its CancelToken instead.
class ProgressSink {
 public:
  virtual ~ProgressSink() = default;
  virtual void report(Stage stage, std::size_t done,
                      std::size_t total) noexcept = 0;
};

} // namespace dualc
