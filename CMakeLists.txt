cmake_minimum_required(VERSION 3.14)
project(dualc VERSION 0.5.0 LANGUAGES CXX)

# Is DualC the top-level project, or is it being add_subdirectory'd into one?
# PROJECT_IS_TOP_LEVEL would say so directly but needs CMake 3.21; this project
# supports 3.14, so fall back to comparing source dirs.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
  set(DUALC_IS_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL})
else()
  string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
         DUALC_IS_TOP_LEVEL)
endif()

# Tests and examples default ON when DualC is the project being built, and OFF
# when it is a subdirectory of someone else's build -- a consumer should not
# silently inherit a Catch2 network fetch and 21 extra test translation units.
option(DUALC_BUILD_TESTS    "Build DualC unit tests"       ${DUALC_IS_TOP_LEVEL})
option(DUALC_BUILD_EXAMPLES "Build DualC example programs" ${DUALC_IS_TOP_LEVEL})
option(DUALC_BUILD_POLYSCOPE_VIEWER
       "Build the interactive Polyscope viewer (dualc_view). Requires a sibling polyscope checkout."
       OFF)
option(DUALC_BUILD_RAYMARCH_VIEWER
       "Build the standalone GPU raymarch viewer (dualc_raymarch). Reuses GLFW+glad from the sibling polyscope checkout."
       OFF)
option(DUALC_BUILD_FIELD_VIEW
       "Build the standalone field-graph GPU viewer (dualc_field_view): compiles any field-graph to GLSL and raymarches it. Reuses GLFW+glad from the sibling polyscope checkout."
       OFF)
option(DUALC_BUILD_GLSL_PARITY
       "Build the field->GLSL parity harness (dualc_glsl_parity): GPU vs C++ per-node acceptance gate. Needs a GL context."
       OFF)
option(DUALC_BUILD_C_ABI
       "Build the C ABI shared library (dualc_capi) + C demo: a flat C boundary over the field-graph proxy+export path. Requires DUALC_BUILD_EXAMPLES (it links the host-side field-graph + export libs)."
       OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json for clangd / clang-tidy. Only when top-level: a
# parent project owns this setting when DualC is one of its subdirectories.
if(DUALC_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# geometry-central, pinned. Three rungs, first match wins:
#   1. a `geometry-central` target already defined by an enclosing project;
#   2. -DDUALC_GC_DIR=/path: a local source tree (a sibling checkout for
#      co-development, any version you want to test against);
#   3. FetchContent of upstream nmwsharp/geometry-central at the pinned commit
#      (v1.1.0) into the build tree -- the default, so a bare clone configures
#      with no flags. Pinned by SHA, not tag, and deliberately not GIT_SHALLOW:
#      shallow needs a branch/tag name, and it fights the submodule recursion
#      that brings deps/happly in.
# geometry-central fetches Eigen 3.3.9 itself at configure unless an
# Eigen3::Eigen target or system Eigen is found first.
set(DUALC_GC_DIR "" CACHE PATH
    "Path to a local geometry-central source tree; empty = fetch the pinned upstream commit")
set(DUALC_GC_GIT_TAG 1e8e43d2d50b18c98ea0b4a53ffaf5848dbaaa26)  # v1.1.0
if(TARGET geometry-central)
  message(STATUS "DualC: using the geometry-central target defined by the enclosing project")
elseif(DUALC_GC_DIR)
  if(NOT EXISTS "${DUALC_GC_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
      "DUALC_GC_DIR is set but no geometry-central source tree was found at ${DUALC_GC_DIR}. "
      "Point it at a checkout (git clone --recurse-submodules "
      "https://github.com/nmwsharp/geometry-central) or leave it empty to fetch the pinned commit.")
  endif()
  message(STATUS "DualC: using geometry-central from DUALC_GC_DIR=${DUALC_GC_DIR}")
  add_subdirectory("${DUALC_GC_DIR}" geometry-central-build)
else()
  message(STATUS "DualC: fetching geometry-central ${DUALC_GC_GIT_TAG} (v1.1.0)")
  include(FetchContent)
  FetchContent_Declare(geometry-central
    GIT_REPOSITORY https://github.com/nmwsharp/geometry-central.git
    GIT_TAG        ${DUALC_GC_GIT_TAG})
  FetchContent_MakeAvailable(geometry-central)
endif()

# Polyscope as a sibling checkout. Pulled in for either GL viewer: dualc_view
# uses polyscope itself; dualc_raymarch reuses only its vendored GLFW + glad
# targets. Added exactly once even if both flags are on.
set(DUALC_POLYSCOPE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../polyscope" CACHE PATH
    "Path to polyscope source tree (used by DUALC_BUILD_POLYSCOPE_VIEWER and DUALC_BUILD_RAYMARCH_VIEWER)")
if(DUALC_BUILD_POLYSCOPE_VIEWER OR DUALC_BUILD_RAYMARCH_VIEWER OR
   DUALC_BUILD_FIELD_VIEW OR DUALC_BUILD_GLSL_PARITY)
  if(NOT EXISTS "${DUALC_POLYSCOPE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
      "A GL viewer/harness was requested (DUALC_BUILD_POLYSCOPE_VIEWER, "
      "DUALC_BUILD_RAYMARCH_VIEWER, DUALC_BUILD_FIELD_VIEW or "
      "DUALC_BUILD_GLSL_PARITY) but could not find polyscope at "
      "${DUALC_POLYSCOPE_DIR}. Clone it as a sibling directory:\n"
      "  git clone --recurse-submodules https://github.com/nmwsharp/polyscope ${DUALC_POLYSCOPE_DIR}\n"
      "or set -DDUALC_POLYSCOPE_DIR=/path/to/polyscope.")
  endif()
  add_subdirectory("${DUALC_POLYSCOPE_DIR}" polyscope-build EXCLUDE_FROM_ALL)
endif()

add_library(dualc STATIC
  src/hermite_octree.cpp
  src/sampler.cpp
  src/contourer.cpp
  src/pipeline.cpp
  # Implicit-field layer (v2)
  src/implicit/implicit_field.cpp
  src/implicit/mesh_source.cpp
  src/implicit/winding_field.cpp
  src/implicit/grid_field.cpp
  src/implicit/combinators.cpp
  src/implicit/decorators.cpp
  src/implicit/domain_ops.cpp
  src/implicit/field2d.cpp
  src/implicit/lift.cpp
  src/implicit/primitives_tierA.cpp
  src/implicit/primitives_tierB.cpp
  src/implicit/primitives_tierC.cpp
  src/implicit/primitives_tpms.cpp
  # Vendored (Unlicense) -- see THIRD_PARTY.md
  src/internal/qef.cpp
  src/internal/svd.cpp
  # DualC-original
  src/internal/octree.cpp
  src/internal/mesh_bvh.cpp
  src/internal/sign_oracle.cpp
  src/internal/dc_tables.cpp
  src/internal/cube_components.cpp
)
add_library(dualc::dualc ALIAS dualc)

target_include_directories(dualc
  PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
          $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# The vendored nanort header (see THIRD_PARTY.md) is third-party code: SYSTEM
# keeps its own deprecation warnings out of the /W4 scan of DualC's sources
# (CMake >= 3.22 maps it to /external:I on MSVC; older CMake ignores it and
# scripts/check.py excludes the folder from the warning scan either way).
target_include_directories(dualc SYSTEM PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/third_party)

# geometry-central's PUBLIC interface brings Eigen and happly (nanort and
# nanoflann are PRIVATE to it since v1.1.0). The nanort DualC's BVH is written
# against is its own vendored copy under src/internal/third_party (see
# THIRD_PARTY.md), so nothing here depends on what geometry-central bundles.
target_link_libraries(dualc PUBLIC geometry-central)
target_compile_features(dualc PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(dualc PRIVATE /W4 /permissive-)
else()
  target_compile_options(dualc PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Vendored Unlicense code uses style choices that trip strict warnings; quiet
# them on a per-source basis so /W4 stays useful for our own code.
if(MSVC)
  set_source_files_properties(
    src/internal/qef.cpp
    src/internal/svd.cpp
    PROPERTIES COMPILE_OPTIONS "/W3;/wd4458;/wd4505")
else()
  set_source_files_properties(
    src/internal/qef.cpp
    src/internal/svd.cpp
    PROPERTIES COMPILE_OPTIONS "-w")
endif()

if(DUALC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
  # The local gate's own test (roadmap 19 Phase 2): every docs check in
  # scripts/check.py is run against its pass/ and fail/ fixture trees under
  # scripts/check_fixtures/. Registered here, not in tests/, because it tests
  # the gate rather than the library; skipped when no Python 3 is found.
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME check_selftest
             COMMAND ${Python3_EXECUTABLE} scripts/check.py --selftest
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
endif()
if(DUALC_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(DUALC_BUILD_C_ABI)
  if(NOT DUALC_BUILD_EXAMPLES)
    message(FATAL_ERROR
      "DUALC_BUILD_C_ABI requires DUALC_BUILD_EXAMPLES: the C ABI links the "
      "host-side dualc_examples_fieldgraph + dualc_examples_common libraries.")
  endif()
  add_subdirectory(capi)
endif()
