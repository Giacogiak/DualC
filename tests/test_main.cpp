// Reserved translation unit for shared fixtures. main() is provided by
// Catch2::Catch2WithMain.

// DELIBERATE BREAK (roadmap 17 #33) -- proves the new CI gates go red;
// reverted in the next commit.
#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <vector>

TEST_CASE("deliberate break: a warning -Werror must reject", "[break]") {
  int unused = 0;  // -Wunused-variable: the build jobs (DUALC_WERROR=ON) fail here
}

TEST_CASE("deliberate break: a heap read ASan must catch", "[break]") {
  std::vector<int> v(3, 1);
  volatile int i = 3;
  const int* p = v.data();
  CHECK(p[i] == 1);  // heap-buffer-overflow, one past the end
}

TEST_CASE("deliberate break: signed overflow UBSan must catch", "[break]") {
  volatile int big = INT_MAX;
  CHECK(big + 1 != 0);  // signed integer overflow
}
