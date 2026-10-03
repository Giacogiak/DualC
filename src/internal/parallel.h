#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace dualc {
namespace internal {

// Resolve a requested thread count: 0 means "use all hardware threads".
inline unsigned resolveThreadCount(unsigned requested) {
  if (requested != 0) return requested;
  const unsigned hc = std::thread::hardware_concurrency();
  return hc == 0 ? 1u : hc;
}

// Run fn(i) for every i in [0, n) across `threads` worker threads and block
// until all are done. Work is handed out one index at a time via an atomic
// counter (dynamic scheduling) so imbalanced tasks -- e.g. octree subtrees of
// wildly different surface complexity -- still load-balance well.
//
// Determinism: fn must touch only data private to index i (or otherwise be
// order-independent). Output order never depends on thread scheduling.
//
// Exceptions: if fn throws, the first exception raised by any worker propagates
// out of parallelFor and the rest are discarded -- the same observable
// behaviour as the serial path above. Workers stop claiming new indices as soon
// as one fails, every thread is still joined, and nothing escapes a worker's
// std::thread entry point, so a throwing fn no longer means std::terminate.
// The library itself throws from several constructors and a user-supplied
// ImplicitField may throw from valueAt/gradientAt, so this path is reachable.
template <typename Fn>
void parallelFor(std::size_t n, unsigned threads, Fn fn) {
  threads = resolveThreadCount(threads);
  if (threads <= 1 || n <= 1) {
    for (std::size_t i = 0; i < n; ++i) fn(i);
    return;
  }
  if (static_cast<std::size_t>(threads) > n) threads = static_cast<unsigned>(n);

  std::atomic<std::size_t> next{0};
  std::atomic<bool>        failed{false};
  std::mutex               errorMutex;
  std::exception_ptr       firstError;

  auto worker = [&]() {
    for (;;) {
      if (failed.load(std::memory_order_relaxed)) break;
      const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= n) break;
      try {
        fn(i);
      } catch (...) {
        std::lock_guard<std::mutex> lock(errorMutex);
        if (!firstError) firstError = std::current_exception();
        failed.store(true, std::memory_order_relaxed);
        break;
      }
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(threads - 1);
  for (unsigned t = 1; t < threads; ++t) pool.emplace_back(worker);
  worker();                         // cannot throw: worker() captures into firstError
  for (auto& th : pool) th.join();  // always reached, so no joinable thread is destroyed
  if (firstError) std::rethrow_exception(firstError);
}

// parallelFor plus a caller-side poll. The calling thread does NOT claim
// indices: it spawns `threads` workers (not threads-1) and, until they are
// done, wakes at most every `interval` -- or as soon as the last index
// completes or a worker fails -- and calls poll(done, n) on itself. So the
// poll runs on the calling thread only, with the exact completed count, and
// the workers' dynamic scheduling is untouched. Output, exception behaviour
// and joining are exactly parallelFor's; the only cost is one mostly-sleeping
// thread. The serial path (threads <= 1 || n <= 1) runs fn on the caller and
// polls every max(1, n/64) items. `poll` must not throw.
//
// This is what lets a ProgressSink (dualc/progress.h) see the inside of a
// parallel region without ever being invoked from a worker.
template <typename Fn, typename Poll>
void parallelForPolled(std::size_t n, unsigned threads, Fn fn, Poll poll,
                       std::chrono::milliseconds interval =
                           std::chrono::milliseconds(100)) {
  threads = resolveThreadCount(threads);
  if (threads <= 1 || n <= 1) {
    const std::size_t every = n / 64 == 0 ? 1 : n / 64;
    for (std::size_t i = 0; i < n; ++i) {
      fn(i);
      if ((i + 1) % every == 0) poll(i + 1, n);
    }
    poll(n, n);
    return;
  }
  if (static_cast<std::size_t>(threads) > n) threads = static_cast<unsigned>(n);

  std::atomic<std::size_t>  next{0};
  std::atomic<std::size_t>  done{0};
  std::atomic<bool>         failed{false};
  std::mutex                mutex;      // guards firstError and the wait
  std::condition_variable   cv;
  std::exception_ptr        firstError;

  auto finished = [&]() {
    return done.load(std::memory_order_relaxed) >= n ||
           failed.load(std::memory_order_relaxed);
  };
  auto worker = [&]() {
    for (;;) {
      if (failed.load(std::memory_order_relaxed)) break;
      const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= n) break;
      try {
        fn(i);
      } catch (...) {
        {
          std::lock_guard<std::mutex> lock(mutex);
          if (!firstError) firstError = std::current_exception();
          failed.store(true, std::memory_order_relaxed);
        }
        cv.notify_one();
        break;
      }
      if (done.fetch_add(1, std::memory_order_relaxed) + 1 >= n) {
        // The empty lock/unlock is load-bearing: the caller evaluates
        // `finished` under `mutex` and then sleeps; taking the mutex here,
        // AFTER the store to `done`, orders that store before the caller's
        // next predicate check, so the final completion can never fall into
        // the gap between the caller's check and its wait. (A missed notify
        // would only cost one `interval`, since the wait is timed -- but the
        // final (n, n) poll must not be late.)
        { std::lock_guard<std::mutex> lock(mutex); }
        cv.notify_one();
      }
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(threads);
  for (unsigned t = 0; t < threads; ++t) pool.emplace_back(worker);
  for (;;) {
    bool stop;
    {
      std::unique_lock<std::mutex> lock(mutex);
      stop = cv.wait_for(lock, interval, finished);
    }
    poll(done.load(std::memory_order_relaxed), n);
    if (stop) break;
  }
  for (auto& th : pool) th.join();  // always reached
  if (firstError) std::rethrow_exception(firstError);
}

} // namespace internal
} // namespace dualc
