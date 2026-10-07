// Shared fixtures for the whole suite. main() is provided by
// Catch2::Catch2WithMain.
//
// Every test case runs in a working directory of its own (roadmap 17 #32,
// finding C43). catch_discover_tests registers each case as its own CTest
// test, all with the same WORKING_DIRECTORY, and several cases write files by
// bare name (the streaming and cancel exports, "nope.stl" in three of them).
// Serial, that is safe by construction; under `ctest -j` it was safe only
// because no two concurrent cases happened to pick the same name. A listener
// that moves each case into <start>/dualc_test_work/<pid>-<n> makes it safe
// for any name a future test picks.
//
// The directory is removed when the case passes and kept when it fails, so a
// failing export can be inspected; it is under the build tree, not the
// system temp directory, so two build trees on one machine never meet.

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#define DUALC_TEST_GETPID _getpid
#else
#include <unistd.h>
#define DUALC_TEST_GETPID getpid
#endif

namespace {

namespace fs = std::filesystem;

class PerCaseWorkingDirectory : public Catch::EventListenerBase {
public:
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const&) override {
    home_ = fs::current_path();
    dir_ = home_ / "dualc_test_work" /
           (std::to_string(DUALC_TEST_GETPID()) + "-" + std::to_string(next_++));
    fs::create_directories(dir_);
    fs::current_path(dir_);
  }

  void testCaseEnded(Catch::TestCaseStats const& stats) override {
    fs::current_path(home_);
    if (stats.totals.assertions.allOk()) {
      std::error_code ec;   // best effort: a leftover only costs disk
      fs::remove_all(dir_, ec);
    } else {
      std::cerr << "test files kept in " << dir_.string() << "\n";
    }
  }

private:
  fs::path home_, dir_;
  unsigned next_ = 0;
};

} // namespace

CATCH_REGISTER_LISTENER(PerCaseWorkingDirectory)
