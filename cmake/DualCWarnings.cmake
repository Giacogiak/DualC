# Compile options for DualC's own targets (roadmap 17 #33). Applied per target,
# never through CMAKE_CXX_* globals or add_compile_options, so nothing leaks
# into the third-party subtrees (geometry-central, Eigen, Catch2, GLFW) that
# the root CMakeLists and tests/ fetch.
#
# dualc_target_options(<target> [VENDORED])
#   dialect     C++17, required, no compiler extensions -- on every target.
#   warnings    /W4 /permissive- (MSVC) or -Wall -Wextra -Wpedantic, plus /WX
#               or -Werror when DUALC_WERROR is ON. Not on a VENDORED target:
#               its sources are third-party and built warning-silenced.
#   sanitizers  -fsanitize=<DUALC_SANITIZE> at compile and link, on every
#               target, so each executable and shared library links the
#               runtime its objects need. GCC/Clang only. The one exception to
#               "never on third-party code" is below: dualc_target_sanitize().

set(DUALC_SANITIZE "" CACHE STRING
    "Comma-separated -fsanitize= list for DualC's own targets, e.g. address,undefined (GCC/Clang only); empty = none")

if(DUALC_SANITIZE AND MSVC)
  message(FATAL_ERROR
    "DUALC_SANITIZE=${DUALC_SANITIZE} is GCC/Clang only; leave it empty with MSVC.")
endif()

function(dualc_target_options target)
  cmake_parse_arguments(ARG "VENDORED" "" "" ${ARGN})
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
  if(NOT ARG_VENDORED)
    if(MSVC)
      target_compile_options(${target} PRIVATE
        /W4 $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
      if(DUALC_WERROR)
        target_compile_options(${target} PRIVATE /WX)
      endif()
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
      if(DUALC_WERROR)
        target_compile_options(${target} PRIVATE -Werror)
      endif()
    endif()
  endif()
  dualc_target_sanitize(${target})
endfunction()

# dualc_target_sanitize(<target>): the sanitizer half alone, for a third-party
# target that must be instrumented like DualC's own. geometry-central is one:
# Eigen picks its aligned allocator by whether __SANITIZE_ADDRESS__ is defined
# (Eigen/src/Core/util/Memory.h), so an Eigen matrix allocated in an
# uninstrumented geometry-central TU and freed in an instrumented DualC TU is a
# heap-buffer-overflow report on the very first run (roadmap 17 #33).
function(dualc_target_sanitize target)
  if(DUALC_SANITIZE)
    # UBSan only reports and carries on by default; -fno-sanitize-recover
    # makes a finding fail the test that hit it, as ASan already does.
    target_compile_options(${target} PRIVATE
      -fsanitize=${DUALC_SANITIZE} -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=${DUALC_SANITIZE})
  endif()
endfunction()
