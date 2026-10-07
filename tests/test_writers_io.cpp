// The writers' failure surface (roadmap 17 #52): what a failed export reports
// and what it leaves behind, on every OS; and on Windows the three holds that
// Boletus's CI taught us -- a short hold is retried through, a long hold fails
// with the OS text, and a child process started with handle inheritance while
// an export has its `.part` open must not inherit it.
#include <catch2/catch_test_macros.hpp>

#include "dualc/implicit.h"
#include "dualc/primitives.h"
#include "dualc/progress.h"

#include "example_common.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using dualc::BBox;
using dualc::ContourerParams;
using dualc::SamplerParams;
using dualc::Vector3;

namespace {

namespace fs = std::filesystem;

struct SphereCase {
  dualc::FieldPtr field = dce::buildPrimitive("sphere", {6.0, 6.0, 6.0, 1.8});
  SamplerParams sp;
  ContourerParams cp;
  SphereCase() {
    sp.maxDepth = 5;
    sp.rootBounds = BBox{Vector3{0.0, 0.0, 0.0}, Vector3{12.0, 12.0, 12.0}};
  }
};

std::string tempName(const char* tag, const char* ext) {
  static std::atomic<int> n{0};
  return (fs::temp_directory_path() /
          ("dualc_writers_io_" + std::string(tag) + "_" + std::to_string(++n) + ext))
      .string();
}

void removeResidue(const std::string& path) {
  for (const std::string& p : {path, path + ".part", path + ".part.model.tmp",
                               path + ".part.verts.tmp", path + ".part.tris.tmp"})
    std::remove(p.c_str());
}

}  // namespace

TEST_CASE("a failed open names the OS error and leaves no file",
          "[writers][io]") {
  SphereCase c;
  const std::string dir =
      (fs::temp_directory_path() / ("dualc_writers_io_missing_dir_" +
                                    std::to_string(static_cast<long long>(
                                        std::chrono::steady_clock::now().time_since_epoch().count()))))
          .string();
  REQUIRE_FALSE(fs::exists(dir));
  const std::string path = dir + "/out.stl";

  SECTION("monolithic") {
    REQUIRE(dce::writeField(*c.field, path, c.sp, c.cp) == 2);
  }
  SECTION("tiled STL") {
    REQUIRE(dce::writeFieldTiledStl(*c.field, path, c.sp, c.cp, 3) == 2);
  }
  SECTION("tiled 3MF") {
    const std::string p3 = dir + "/out.3mf";
    REQUIRE(dce::writeFieldTiled3mf(*c.field, p3, c.sp, c.cp, 3) == 2);
  }
  const std::string& why = dce::lastError();
  INFO(why);
  REQUIRE(why.rfind("cannot open '", 0) == 0);
  REQUIRE(why.find("' for writing: ") != std::string::npos);
  REQUIRE(why.size() > why.find("' for writing: ") + 15);  // the OS text follows
  REQUIRE_FALSE(fs::exists(dir));
}

TEST_CASE("lastError is empty after a successful export and after an unknown extension it names it",
          "[writers][io]") {
  SphereCase c;
  const std::string ok = tempName("ok", ".stl");
  REQUIRE(dce::writeField(*c.field, ok, c.sp, c.cp) == 0);
  REQUIRE(dce::lastError().empty());
  removeResidue(ok);

  const std::string bad = tempName("bad", ".xyz");
  REQUIRE(dce::writeField(*c.field, bad, c.sp, c.cp) == 2);
  REQUIRE(dce::lastError().find("unknown output extension '.xyz'") != std::string::npos);
  REQUIRE_FALSE(fs::exists(bad));
  REQUIRE_FALSE(fs::exists(bad + ".part"));
}

#ifdef _WIN32

namespace {

// Open `path` for reading WITHOUT FILE_SHARE_DELETE as soon as it exists and
// keep it open for `holdMs` from that moment: the shape of any foreign handle
// on a `.part` (an on-access scanner's, an inherited one).
class Holder {
 public:
  Holder(std::string path, int holdMs) : path_(std::move(path)), holdMs_(holdMs) {
    th_ = std::thread([this] { run(); });
  }
  ~Holder() { stop_ = true; if (th_.joinable()) th_.join(); }
  bool acquired() const { return acquired_; }

 private:
  void run() {
    const std::wstring w(path_.begin(), path_.end());
    HANDLE h = INVALID_HANDLE_VALUE;
    while (h == INVALID_HANDLE_VALUE && !stop_) {
      h = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (h == INVALID_HANDLE_VALUE) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (h == INVALID_HANDLE_VALUE) return;
    acquired_ = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(holdMs_));
    CloseHandle(h);
  }
  std::string path_;
  int holdMs_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> acquired_{false};
  std::thread th_;
};

// A progress sink that, at the first Tile event (the tiled driver has opened
// its `.part` by then), starts a child process with bInheritHandles = TRUE that
// lives ~2 s -- what a host's Process.Start with a redirected stream does.
class SpawnAtFirstTile : public dualc::ProgressSink {
 public:
  void report(dualc::Stage stage, std::size_t, std::size_t) noexcept override {
    if (stage != dualc::Stage::Tile || spawned_) return;
    spawned_ = true;
    wchar_t cmd[] = L"cmd.exe /c ping -n 3 127.0.0.1 >nul";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                       nullptr, &si, &pi)) {
      CloseHandle(pi.hThread);
      child_ = pi.hProcess;
    }
  }
  bool spawned() const { return spawned_ && child_ != nullptr; }
  ~SpawnAtFirstTile() override {
    if (child_) { WaitForSingleObject(child_, 10000); CloseHandle(child_); }
  }

 private:
  bool spawned_ = false;
  HANDLE child_ = nullptr;
};

}  // namespace

TEST_CASE("Windows: a short hold on the .part is retried through, a long one fails with the OS text",
          "[writers][io][windows]") {
  SphereCase c;

  SECTION("held 300 ms: the rename lands after retries") {
    const std::string path = tempName("hold_short", ".stl");
    {
      Holder hold(path + ".part", 300);
      REQUIRE(dce::writeField(*c.field, path, c.sp, c.cp) == 0);
      REQUIRE(hold.acquired());
    }
    REQUIRE(fs::exists(path));
    REQUIRE_FALSE(fs::exists(path + ".part"));
    REQUIRE(dce::lastError().empty());
    removeResidue(path);
  }

  SECTION("held 3 s: rc 2, the line names the sharing violation, the .part is left") {
    const std::string path = tempName("hold_long", ".stl");
    {
      Holder hold(path + ".part", 3000);
      const auto t0 = std::chrono::steady_clock::now();
      REQUIRE(dce::writeField(*c.field, path, c.sp, c.cp) == 2);
      const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0).count();
      REQUIRE(hold.acquired());
      REQUIRE(ms < 2500);  // the retry budget is bounded, well under the hold
      const std::string& why = dce::lastError();
      INFO(why);
      REQUIRE(why.rfind("cannot move '", 0) == 0);
      REQUIRE(why.find("attempts over") != std::string::npos);
      REQUIRE_FALSE(fs::exists(path));
    }
    removeResidue(path);
  }
}

TEST_CASE("Windows: a child process started mid-export inherits no output handle",
          "[writers][io][windows]") {
  SphereCase c;
  const std::string path = tempName("inherit", ".stl");
  {
    SpawnAtFirstTile spawn;
    // Tile depth 3 on a depth-5 grid: several tiles, so the .part is open
    // for the whole loop and the child is alive when the rename runs.
    REQUIRE(dce::writeFieldTiledStl(*c.field, path, c.sp, c.cp, 3, nullptr, &spawn) == 0);
    REQUIRE(spawn.spawned());
  }
  REQUIRE(fs::exists(path));
  REQUIRE_FALSE(fs::exists(path + ".part"));
  REQUIRE(dce::lastError().empty());
  removeResidue(path);
}

#endif  // _WIN32
