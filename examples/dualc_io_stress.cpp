// dualc_io_stress -- concurrent exports to the temp directory, every rename
// must land (roadmap 17 #52).
//
// The question this answers: does the `.part` -> `<path>` rename in
// AtomicOutput::commit() (example_common.cpp) fail on a Windows runner while
// an on-access scanner still holds the file it just saw closed? Boletus's CI
// saw `DUALC_ERR_IO` from one of six concurrent exports on windows-2022 and
// inferred exactly that; the stderr line that names the OS error was never
// captured. This harness reproduces Boletus's scenario in-process (the same
// field, depth and tile depth; even workers tiled STL, odd monolithic), keeps
// every `[dualc] error:` line per worker, classifies the failing site from it,
// and -- the direct evidence a bounded retry would have helped -- retries the
// rename itself after a failure, printing each error_code it meets.
//
// Opt-in, nondeterministic by design, never a CTest case: `check.py --io-stress`
// is its one runner (an experiment job, roadmap 17 #52). Linux is the control
// (must be zero). `--force-hold` (Windows only) makes the failure deterministic
// by holding the `.part` open without FILE_SHARE_DELETE from another thread.
#include "example_common.h"
#include "field_graph.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <streambuf>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {

// --- stderr capture per thread -------------------------------------------
// example_common.cpp writes every `[dualc] error:` line to std::cerr on the
// thread that runs the export, so a streambuf that appends to a thread_local
// string gives each worker its own transcript with no interleaving and no
// change to the library under test.
thread_local std::string g_tlsText;

class TlsBuf : public std::streambuf {
 protected:
  int overflow(int c) override {
    if (c != EOF) g_tlsText.push_back(static_cast<char>(c));
    return c;
  }
  std::streamsize xsputn(const char* s, std::streamsize n) override {
    g_tlsText.append(s, static_cast<std::size_t>(n));
    return n;
  }
};

class NullBuf : public std::streambuf {
 protected:
  int overflow(int c) override { return c; }
  std::streamsize xsputn(const char*, std::streamsize n) override { return n; }
};

std::string oneLine(std::string s) {
  for (char& c : s) if (c == '\n' || c == '\r') c = ' ';
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}

std::string describe(const std::error_code& ec) {
  std::ostringstream os;
  os << "ec{value=" << ec.value() << " category=" << ec.category().name()
     << " condition=" << ec.default_error_condition().value()
     << " message='" << oneLine(ec.message()) << "'}";
  return os.str();
}

long long msSince(Clock::time_point t) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

// --- options ----------------------------------------------------------------
struct Options {
  int concurrency = 6;
  int iterations = 30;
  int depth = 6;
  int tileDepth = 4;
  std::string dir;
  bool small = false;
  bool mix = false;
  bool verbose = false;
  // force-hold: -1 = off, 0 = "through" (hold until the export returned), > 0 = ms
  long long forceHold = -1;
};

// Boletus's ConcurrencyTests field, verbatim, so the file size (and hence a
// scanner's hold time) matches the observed failure.
const char* kBoletusExpr =
    "intersection(onion(gyroid(wavelength=0.5),thickness=0.12),"
    "box(min=[-1,-1,-1],max=[1,1,1]))";
const char* kSmallExpr = "sphere(radius=0.8)";

void usage() {
  std::cout <<
      "dualc_io_stress [--concurrency N] [--iterations K] [--dir DIR] [--small]\n"
      "                [--mix] [--verbose] [--force-hold through|MS]\n"
      "  Runs K rounds of N concurrent exports of Boletus's gyroid-in-box field\n"
      "  (depth 6, tile depth 4) to fresh files under DIR (default: the temp dir):\n"
      "  even workers writeFieldTiledStl, odd workers writeField. Every non-zero\n"
      "  rc is printed with the [dualc] error line it produced and a rename probe.\n"
      "  --small       a sphere at depth 5 (smoke runs)\n"
      "  --mix         odd workers rotate .stl / .3mf / .obj\n"
      "  --force-hold  (Windows) hold each .part open without FILE_SHARE_DELETE\n"
      "                until the export returned (through) or for MS ms\n";
}

bool parse(int argc, char** argv, Options& o) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](const char* flag) -> const char* {
      if (i + 1 >= argc) { std::cerr << flag << " needs a value\n"; return nullptr; }
      return argv[++i];
    };
    if (a == "--concurrency") { const char* v = next("--concurrency"); if (!v) return false; o.concurrency = std::atoi(v); }
    else if (a == "--iterations") { const char* v = next("--iterations"); if (!v) return false; o.iterations = std::atoi(v); }
    else if (a == "--depth") { const char* v = next("--depth"); if (!v) return false; o.depth = std::atoi(v); }
    else if (a == "--tile-depth") { const char* v = next("--tile-depth"); if (!v) return false; o.tileDepth = std::atoi(v); }
    else if (a == "--dir") { const char* v = next("--dir"); if (!v) return false; o.dir = v; }
    else if (a == "--small") { o.small = true; if (o.depth == 6) o.depth = 5; }
    else if (a == "--mix") o.mix = true;
    else if (a == "--verbose") o.verbose = true;
    else if (a == "--force-hold") {
      const char* v = next("--force-hold"); if (!v) return false;
      o.forceHold = (std::string(v) == "through") ? 0 : std::atoll(v);
      if (o.forceHold < 0) { std::cerr << "--force-hold: through or a positive ms\n"; return false; }
    }
    else if (a == "--help" || a == "-h") { usage(); std::exit(0); }
    else { std::cerr << "unknown argument '" << a << "'\n"; usage(); return false; }
  }
  if (o.concurrency < 1 || o.iterations < 1) { std::cerr << "concurrency and iterations must be >= 1\n"; return false; }
  return true;
}

// --- the per-export record -------------------------------------------------
struct Outcome {
  int iter = 0, worker = 0;
  std::string mode;          // tiled | mono(.ext)
  std::string path;
  int rc = 0;
  long long ms = 0;
  std::string stderrText;    // every line the export wrote to std::cerr
  bool partAfter = false, destAfter = false;
  std::string probe;         // what the rename probe saw after a failure
  std::uint32_t facets = 0;  // binary STL only
  bool countChecked = false;
  std::string deleteNote;    // non-empty when the delete needed retries or failed
  bool deleteFailed = false;
  bool stray = false;        // a .part still on disk when the record closed
  std::string site() const {
    if (rc == 0) return "none";
    if (stderrText.find("cannot open") != std::string::npos) return "open";
    if (stderrText.find("failed finalizing") != std::string::npos) return "finish";
    if (stderrText.find("failed writing") != std::string::npos) return "write";
    if (stderrText.find("cannot move") != std::string::npos) return "commit";
    return "other";
  }
};

// Retry `op` every 20 ms for up to `budgetMs`, collecting the first three
// distinct error_codes met. Returns the ms at which it succeeded, or -1.
template <class Op>
long long retryLoop(Op op, long long budgetMs, std::string& note) {
  const auto t0 = Clock::now();
  std::vector<std::string> seen;
  int attempts = 0;
  for (;;) {
    std::error_code ec;
    ++attempts;
    if (op(ec)) {
      std::ostringstream os;
      os << "ok@" << msSince(t0) << "ms after " << (attempts - 1) << " failures";
      for (const auto& s : seen) os << " " << s;
      note = os.str();
      return msSince(t0);
    }
    const std::string d = describe(ec);
    if (seen.size() < 3 && std::find(seen.begin(), seen.end(), d) == seen.end()) seen.push_back(d);
    if (msSince(t0) >= budgetMs) {
      std::ostringstream os;
      os << "never (" << attempts << " attempts over " << msSince(t0) << "ms)";
      for (const auto& s : seen) os << " " << s;
      note = os.str();
      return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

bool readStlCount(const std::string& path, std::uint32_t& n) {
  std::ifstream is(path, std::ios::binary);
  if (!is) return false;
  is.seekg(0, std::ios::end);
  const auto size = static_cast<long long>(is.tellg());
  if (size < 84) return false;
  is.seekg(80, std::ios::beg);
  is.read(reinterpret_cast<char*>(&n), 4);
  return static_cast<bool>(is) && size == 84 + 50LL * n;
}

// --- the Windows holder (the forcing variant) --------------------------------
#ifdef _WIN32
// Poll for the `.part` the writer is about to create, open it for reading
// WITHOUT FILE_SHARE_DELETE (what an on-access scanner's read handle is
// believed to do), and keep it open until released. A rename or delete of
// the file by the writer then fails with ERROR_SHARING_VIOLATION.
class Holder {
 public:
  Holder(std::string part, long long holdMs, std::atomic<bool>& exportDone)
      : part_(std::move(part)), holdMs_(holdMs), done_(exportDone) {
    th_ = std::thread([this] { run(); });
  }
  ~Holder() { join(); }
  void join() { if (th_.joinable()) th_.join(); }
  std::string note() const { return note_; }

 private:
  void run() {
    const auto t0 = Clock::now();
    HANDLE h = INVALID_HANDLE_VALUE;
    const std::wstring w(part_.begin(), part_.end());  // temp paths here are ASCII
    while (h == INVALID_HANDLE_VALUE) {
      h = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (h != INVALID_HANDLE_VALUE) break;
      if (done_.load()) { note_ = "holder never saw the .part"; return; }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const long long acquired = msSince(t0);
    if (holdMs_ == 0) {
      // Keep holding for a while after the export returned, so the harness's
      // own rename probe meets the held file at least once and prints the
      // error_code it gets -- the tuple the fix's predicate is built from.
      while (!done_.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(holdMs_));
    }
    CloseHandle(h);
    std::ostringstream os;
    os << "held .part from " << acquired << "ms to " << msSince(t0) << "ms";
    note_ = os.str();
  }
  std::string part_;
  long long holdMs_;
  std::atomic<bool>& done_;
  std::thread th_;
  std::string note_;
};
#endif

// --- one export, start to finish -----------------------------------------------
void runOne(const dualc::ImplicitField& field, const Options& o, Outcome& r) {
  dualc::SamplerParams sp;
  dualc::ContourerParams cp;
  sp.maxDepth = o.depth;
  g_tlsText.clear();
  std::atomic<bool> done{false};
#ifdef _WIN32
  std::unique_ptr<Holder> holder;
  if (o.forceHold >= 0) holder.reset(new Holder(r.path + ".part", o.forceHold, done));
#endif
  const auto t0 = Clock::now();
  if (r.mode == "tiled")
    r.rc = dce::writeFieldTiledStl(field, r.path, sp, cp, o.tileDepth);
  else
    r.rc = dce::writeField(field, r.path, sp, cp);
  r.ms = msSince(t0);
  done.store(true);
  r.stderrText = oneLine(g_tlsText);
  const std::string part = r.path + ".part";
  r.partAfter = fs::exists(part);
  r.destAfter = fs::exists(r.path);

  if (r.rc != 0) {
    // The probe: would a bounded retry of the rename have landed the file?
    // Under --force-hold the holder is still open here, so the first attempts
    // fail and the error_code they get is printed.
    if (r.partAfter) {
      std::string note;
      retryLoop([&](std::error_code& ec) { fs::rename(part, r.path, ec); return !ec; }, 3000, note);
      r.probe += "rename " + note;
    } else {
      r.probe += (r.destAfter ? "no .part, destination present" : "no .part left");
    }
  }
#ifdef _WIN32
  if (holder) { std::unique_ptr<Holder> h = std::move(holder); h->join(); r.probe += " | holder: " + h->note(); }
#endif

  // Read back (H5: the mesh is right) -- binary STL only.
  if (fs::exists(r.path) && r.path.size() > 4 && r.path.compare(r.path.size() - 4, 4, ".stl") == 0) {
    r.countChecked = readStlCount(r.path, r.facets);
  }

  // Delete with the same retry (H1: a scanner holding the renamed file).
  if (fs::exists(r.path)) {
    std::string note;
    const long long t = retryLoop([&](std::error_code& ec) {
      fs::remove(r.path, ec);
      return !ec && !fs::exists(r.path);
    }, 3000, note);
    if (t < 0) { r.deleteFailed = true; r.deleteNote = note; }
    else if (note.find("after 0 failures") == std::string::npos) r.deleteNote = note;
  }
  if (fs::exists(part)) {
    r.stray = true;
    std::string note;
    retryLoop([&](std::error_code& ec) { fs::remove(part, ec); return !ec && !fs::exists(part); }, 3000, note);
    r.probe += " | stray .part removal: " + note;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Options o;
  if (!parse(argc, argv, o)) return 2;
#ifndef _WIN32
  if (o.forceHold >= 0) {
    std::cout << "dualc_io_stress: --force-hold is not supported on this platform\n";
    return 0;
  }
#endif

  // Build the field once; the resolver must outlive it.
  dce::fieldgraph::FileMeshResolver meshes;
  std::unique_ptr<dce::fieldgraph::FieldGraph> graph;
  const char* expr = o.small ? kSmallExpr : kBoletusExpr;
  try {
    graph.reset(new dce::fieldgraph::FieldGraph(
        dce::fieldgraph::FieldGraph::build(dce::fieldgraph::parseShorthand(expr), meshes)));
  } catch (const std::exception& e) {
    std::cerr << "dualc_io_stress: cannot build the field: " << e.what() << "\n";
    return 2;
  }
  const dualc::ImplicitField& field = graph->field();

  fs::path dir = o.dir.empty() ? fs::temp_directory_path() : fs::path(o.dir);
  std::error_code dec;
  fs::create_directories(dir, dec);
  const long long pid =
#ifdef _WIN32
      static_cast<long long>(GetCurrentProcessId());
#else
      static_cast<long long>(::getpid());
#endif

  // From here on the library's console output is captured (stderr per thread)
  // or dropped (stdout, unless --verbose); the harness reports through `out`.
  std::ostream out(std::cout.rdbuf());
  TlsBuf tls;
  NullBuf null;
  std::streambuf* oldErr = std::cerr.rdbuf(&tls);
  std::streambuf* oldOut = o.verbose ? nullptr : std::cout.rdbuf(&null);

  out << "dualc_io_stress: field=" << (o.small ? "sphere" : "boletus-gyroid-box")
      << " depth=" << o.depth << " tile_depth=" << o.tileDepth
      << " concurrency=" << o.concurrency << " iterations=" << o.iterations
      << " dir=" << dir.string() << " hw_threads=" << std::thread::hardware_concurrency()
      << " force_hold=" << (o.forceHold < 0 ? std::string("off") : o.forceHold == 0 ? std::string("through") : std::to_string(o.forceHold) + "ms")
      << "\n";

  auto nameFor = [&](int iter, int w) {
    std::string ext = ".stl";
    if (o.mix && (w % 2 == 1)) { static const char* exts[] = {".stl", ".3mf", ".obj"}; ext = exts[(w / 2) % 3]; }
    std::ostringstream os;
    os << "dualc_io_stress_" << pid << "_" << iter << "_" << w << ext;
    return (dir / os.str()).string();
  };

  // Sequential baseline: one tiled, one monolithic -- the counts every
  // concurrent export is compared to, and the duration --force-hold MS is
  // checked against.
  std::uint32_t baseTiled = 0, baseMono = 0;
  long long baseMs = 0;
  {
    Options seq = o; seq.forceHold = -1;
    Outcome a; a.iter = -1; a.worker = 0; a.mode = "tiled"; a.path = nameFor(-1, 0);
    Outcome b; b.iter = -1; b.worker = 1; b.mode = "mono"; b.path = nameFor(-1, 1);
    runOne(field, seq, a); runOne(field, seq, b);
    if (a.rc != 0 || b.rc != 0 || !a.countChecked || !b.countChecked) {
      std::cerr.rdbuf(oldErr); if (oldOut) std::cout.rdbuf(oldOut);
      std::cerr << "dualc_io_stress: the sequential baseline failed (tiled rc=" << a.rc
                << " '" << a.stderrText << "', mono rc=" << b.rc << " '" << b.stderrText << "')\n";
      return 2;
    }
    baseTiled = a.facets; baseMono = b.facets; baseMs = std::max(a.ms, b.ms);
    out << "dualc_io_stress: baseline tiled=" << baseTiled << " facets (" << a.ms << "ms), mono="
        << baseMono << " facets (" << b.ms << "ms)\n";
    if (o.forceHold > 0 && o.forceHold < baseMs)
      out << "dualc_io_stress: note: --force-hold " << o.forceHold << "ms is shorter than one export ("
          << baseMs << "ms); the hold may end before finish()\n";
  }

  std::vector<Outcome> all;
  all.reserve(static_cast<std::size_t>(o.concurrency) * o.iterations);
  for (int iter = 0; iter < o.iterations; ++iter) {
    std::vector<Outcome> round(static_cast<std::size_t>(o.concurrency));
    std::vector<std::thread> threads;
    for (int w = 0; w < o.concurrency; ++w) {
      Outcome& r = round[static_cast<std::size_t>(w)];
      r.iter = iter; r.worker = w; r.mode = (w % 2 == 0) ? "tiled" : "mono"; r.path = nameFor(iter, w);
      threads.emplace_back([&field, &o, &r] { runOne(field, o, r); });
    }
    for (auto& t : threads) t.join();
    for (auto& r : round) all.push_back(std::move(r));
  }

  std::cerr.rdbuf(oldErr);
  if (oldOut) std::cout.rdbuf(oldOut);

  int failed = 0, nOpen = 0, nFinish = 0, nWrite = 0, nCommit = 0, nOther = 0, nDelete = 0, nStray = 0, nCount = 0;
  for (const auto& r : all) {
    bool bad = r.rc != 0;
    std::string site = r.site();
    if (!bad && r.countChecked) {
      const std::uint32_t expect = (r.mode == "tiled") ? baseTiled : baseMono;
      if (r.facets != expect) { bad = true; site = "count"; ++nCount; }
    }
    if (bad) {
      ++failed;
      if (site == "open") ++nOpen; else if (site == "finish") ++nFinish; else if (site == "write") ++nWrite;
      else if (site == "commit") ++nCommit; else if (site != "count") ++nOther;
      out << "FAIL #" << r.iter << "/" << r.worker << " " << r.mode << " rc=" << r.rc << " site=" << site
          << " ms=" << r.ms << " part_after=" << (r.partAfter ? "y" : "n") << " dest_after=" << (r.destAfter ? "y" : "n");
      if (site == "count") out << " facets=" << r.facets;
      out << " | " << (r.stderrText.empty() ? "(no stderr)" : r.stderrText) << " | probe=" << r.probe << "\n";
    }
    if (r.deleteFailed) { ++nDelete; out << "DELETE-FAIL #" << r.iter << "/" << r.worker << " " << r.path << ": " << r.deleteNote << "\n"; }
    else if (!r.deleteNote.empty()) out << "DELETE-RETRIED #" << r.iter << "/" << r.worker << " " << r.path << ": " << r.deleteNote << "\n";
    if (r.stray) ++nStray;
  }
  out << "dualc_io_stress: " << all.size() << " exports, " << failed << " failed (open " << nOpen
      << ", finish " << nFinish << ", write " << nWrite << ", commit " << nCommit << ", count " << nCount
      << ", other " << nOther << "), " << nDelete << " delete failures, " << nStray << " stray .part\n";
  return (failed > 0 || nStray > 0) ? 1 : 0;
}
