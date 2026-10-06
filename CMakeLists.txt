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
option(DUALC_BUILD_RAYMARCH_VIEWER
       "Build the standalone GPU raymarch viewer (dualc_raymarch). Needs GLFW (fetched at its pinned tag unless DUALC_GLFW_DIR names a tree) + the vendored glad."
       OFF)
option(DUALC_BUILD_FIELD_VIEW
       "Build the standalone field-graph GPU viewer (dualc_field_view): compiles any field-graph to GLSL and raymarches it. Needs GLFW (fetched at its pinned tag unless DUALC_GLFW_DIR names a tree) + the vendored glad."
       OFF)
option(DUALC_BUILD_GLSL_PARITY
       "Build the field->GLSL parity harness (dualc_glsl_parity): GPU vs C++ per-node acceptance gate. Needs a GL context."
       OFF)
option(DUALC_BUILD_C_ABI
       "Build the C ABI shared library (dualc_capi) + C demo: a flat C boundary over the field-graph proxy+export path. Requires DUALC_BUILD_EXAMPLES (it links the host-side field-graph + export libs)."
       OFF)
option(DUALC_WERROR
       "Treat warnings in DualC's own targets as errors (-Werror / /WX). OFF locally, where scripts/check.py filters the build log instead; ON in CI's build matrix."
       OFF)
# DUALC_SANITIZE (a cache string, e.g. address,undefined) is declared with the
# helper that applies it, in cmake/DualCWarnings.cmake.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(DualCWarnings)

# The library is C++ only; examples/ and tests/ also compile the vendored
# miniz.c. The Visual Studio generator compiles a .c file without C being
# enabled, Ninja and Makefiles do not (CMAKE_C_COMPILE_OBJECT unset), so C is
# enabled here, at top level so both sibling directories see it, and only when
# they are built -- a consumer of the library alone still needs no C compiler.
if(DUALC_BUILD_EXAMPLES OR DUALC_BUILD_TESTS)
  enable_language(C)
endif()

# No CMAKE_CXX_STANDARD here: a global set before the fetches below leaked
# -std=c++17 into geometry-central and Catch2 (roadmap 17 #33). The dialect is
# a property of each DualC target, set by dualc_target_options(). This guard
# fails a configure that reintroduces the global as a normal variable; a
# -DCMAKE_CXX_STANDARD on the command line is a cache entry and passes.
if(DUALC_IS_TOP_LEVEL AND DEFINED CMAKE_CXX_STANDARD
   AND NOT DEFINED CACHE{CMAKE_CXX_STANDARD})
  message(FATAL_ERROR
    "CMAKE_CXX_STANDARD is set as a normal variable before the third-party "
    "fetches, so it would leak into geometry-central and Catch2. Set the dialect "
    "per target with dualc_target_options() (cmake/DualCWarnings.cmake).")
endif()

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
set(dualc_gc_from_parent OFF)
if(TARGET geometry-central)
  message(STATUS "DualC: using the geometry-central target defined by the enclosing project")
  set(dualc_gc_from_parent ON)
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
# geometry-central's headers (and happly's) trip -Wunused-parameter and friends
# in every DualC TU that includes them -- 16 warnings per TU on GCC 15. When
# DualC added the tree itself, their include directories reach DualC's targets
# as SYSTEM, like the vendored nanort below, so DUALC_WERROR and the warnings
# scan judge DualC's code only. (Its Eigen is SYSTEM already.) A target owned
# by an enclosing project is left as that project declared it.
if(NOT dualc_gc_from_parent)
  foreach(dep geometry-central happly)
    if(TARGET ${dep})
      get_target_property(dep_includes ${dep} INTERFACE_INCLUDE_DIRECTORIES)
      if(dep_includes)
        set_target_properties(${dep} PROPERTIES
          INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${dep_includes}")
      endif()
    endif()
  endforeach()
  # Instrumented alongside DualC under DUALC_SANITIZE, or Eigen's allocator
  # differs across the two (see dualc_target_sanitize). An enclosing project
  # that owns the target and sets DUALC_SANITIZE must instrument it itself.
  dualc_target_sanitize(geometry-central)
endif()

# GLFW, for the opt-in GL targets only (dualc_raymarch, dualc_field_view,
# dualc_glsl_parity). Resolved like geometry-central, three rungs, first match
# wins: an existing `glfw` target from an enclosing project; a local tree via
# -DDUALC_GLFW_DIR; else FetchContent of upstream glfw/glfw at the pinned 3.4
# release commit. The GL loader those targets pair it with is the vendored
# glad under examples/third_party/glad (see THIRD_PARTY.md); nothing here
# touches libdualc. On Linux GLFW builds its X11 backend only unless
# -DGLFW_BUILD_WAYLAND=ON is passed: X11 needs the Xlib/Xrandr/Xinerama/Xcursor/Xi
# dev headers, Wayland would add libwayland, xkbcommon and wayland-scanner, and
# an X11-only GLFW still runs under a Wayland session through XWayland.
set(DUALC_GLFW_DIR "" CACHE PATH
    "Path to a local GLFW source tree; empty = fetch the pinned upstream release")
set(DUALC_GLFW_GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579)  # 3.4
if(DUALC_BUILD_RAYMARCH_VIEWER OR DUALC_BUILD_FIELD_VIEW OR DUALC_BUILD_GLSL_PARITY)
  set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
  if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build GLFW's Wayland backend (needs libwayland, xkbcommon, wayland-scanner)")
  endif()
  if(TARGET glfw)
    message(STATUS "DualC: using the glfw target defined by the enclosing project")
  elseif(DUALC_GLFW_DIR)
    if(NOT EXISTS "${DUALC_GLFW_DIR}/CMakeLists.txt")
      message(FATAL_ERROR
        "DUALC_GLFW_DIR is set but no GLFW source tree was found at ${DUALC_GLFW_DIR}. "
        "Point it at a checkout (git clone https://github.com/glfw/glfw) or leave it "
        "empty to fetch the pinned release.")
    endif()
    message(STATUS "DualC: using GLFW from DUALC_GLFW_DIR=${DUALC_GLFW_DIR}")
    add_subdirectory("${DUALC_GLFW_DIR}" glfw-build EXCLUDE_FROM_ALL)
  else()
    message(STATUS "DualC: fetching GLFW ${DUALC_GLFW_GIT_TAG} (3.4)")
    include(FetchContent)
    FetchContent_Declare(glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG        ${DUALC_GLFW_GIT_TAG})
    FetchContent_MakeAvailable(glfw)
  endif()
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
target_link_libraries(dualc PUBLIC geometry-central glfw)  # DELIBERATE BREAK (#33)
target_compile_features(dualc PUBLIC cxx_std_17)
dualc_target_options(dualc)

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

# The layering rule THIRD_PARTY.md states (host-only dependencies never link
# into libdualc), checked rather than trusted: after every add_subdirectory,
# dualc may link geometry-central and the threads library, nothing else.
foreach(prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
  get_target_property(dualc_links dualc ${prop})
  if(NOT dualc_links)
    continue()
  endif()
  foreach(lib IN LISTS dualc_links)
    # A static library lists its PRIVATE deps as $<LINK_ONLY:...> here.
    string(REGEX REPLACE "^\\$<LINK_ONLY:(.*)>$" "\\1" lib "${lib}")
    if(NOT lib MATCHES "^(geometry-central|Threads::Threads)$")
      message(FATAL_ERROR
        "libdualc's ${prop} names '${lib}': only geometry-central and "
        "Threads::Threads may link into the library (THIRD_PARTY.md, the "
        "layering rule). Link host-only dependencies into examples/ targets.")
    endif()
  endforeach()
endforeach()
