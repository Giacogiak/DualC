# Content check for one CLI smoke-test output (roadmap 17 #32, audit C52).
# The smoke tests asserted the exit code only, so a tool that wrote a corrupt
# PNG or a truncated STL passed. Each output now has a `<test>_content` test
# that runs this script on it:
#
#   cmake -DFILE=<path> -DKIND=<png|stl|3mf|obj|svg|json> [options]
#         -P check_cli_output.cmake
#
# Options:
#   WIDTH, HEIGHT   png: the IHDR dimensions it must declare.
#   SAME_AS_OBJ     stl, 3mf: an OBJ from the same field and depth whose face
#                   count the triangle count must equal exactly.
#   MIN_FACES       obj, stl, 3mf: the least triangle count accepted
#                   (default 1).
#   EXACT_VERTICES, EXACT_FACES   obj: exact counts, for deterministic outputs.
#   CONTAINS        svg: a string the file must contain.
#   JSON_ROOT_OP    json: the `op` the document's `root` must name.
#   RUN             a command to run first, its arguments separated by `|`
#                   (a `;` would not survive add_test), its stdout written to
#                   FILE; a non-zero exit fails the check. For tools that
#                   print their output, such as `dualc_field --dump-json`.
#
# Any failed check ends in FATAL_ERROR, which fails the test.

cmake_minimum_required(VERSION 3.14)

function(fail what)
  message(FATAL_ERROR "${FILE}: ${what}")
endfunction()

if(NOT DEFINED FILE OR NOT DEFINED KIND)
  message(FATAL_ERROR "usage: cmake -DFILE=... -DKIND=... -P check_cli_output.cmake")
endif()
if(DEFINED RUN)
  string(REPLACE "|" ";" run "${RUN}")
  execute_process(COMMAND ${run} OUTPUT_FILE "${FILE}" RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${RUN} exited with ${rc}")
  endif()
endif()
if(NOT EXISTS "${FILE}")
  fail("missing")
endif()
file(SIZE "${FILE}" size)
if(size EQUAL 0)
  fail("empty")
endif()
if(NOT DEFINED MIN_FACES)
  set(MIN_FACES 1)
endif()

# Unsigned integer from `bytes` hex-encoded bytes at `offset` of a hex string.
function(read_uint hex offset bytes endian out)
  set(value 0)
  math(EXPR last "${bytes} - 1")
  foreach(i RANGE 0 ${last})
    if(endian STREQUAL "big")
      math(EXPR pos "2 * (${offset} + ${i})")
    else()
      math(EXPR pos "2 * (${offset} + ${bytes} - 1 - ${i})")
    endif()
    string(SUBSTRING "${hex}" ${pos} 2 byte)
    math(EXPR value "${value} * 256 + 0x${byte}")
  endforeach()
  set(${out} ${value} PARENT_SCOPE)
endfunction()

# Vertex and face counts of an OBJ, in one pass over the file; every face
# must be a triangle.
function(obj_counts path out_vertices out_faces)
  file(STRINGS "${path}" lines REGEX "^[vf] ")
  list(LENGTH lines total)
  list(FILTER lines INCLUDE REGEX "^f")
  list(LENGTH lines faces)
  list(FILTER lines EXCLUDE REGEX "^f +[0-9/]+ +[0-9/]+ +[0-9/]+ *$")
  list(LENGTH lines bad)
  if(NOT bad EQUAL 0)
    message(FATAL_ERROR "${path}: ${bad} of its ${faces} faces are not triangles")
  endif()
  math(EXPR vertices "${total} - ${faces}")
  set(${out_vertices} ${vertices} PARENT_SCOPE)
  set(${out_faces} ${faces} PARENT_SCOPE)
endfunction()

function(check_triangles n)
  if(n LESS MIN_FACES)
    fail("${n} triangles, expected at least ${MIN_FACES}")
  endif()
  if(DEFINED SAME_AS_OBJ)
    obj_counts("${SAME_AS_OBJ}" unused want)
    if(NOT n EQUAL want)
      fail("${n} triangles, but ${SAME_AS_OBJ} has ${want} faces")
    endif()
  endif()
endfunction()

if(KIND STREQUAL "png")
  file(READ "${FILE}" hex LIMIT 24 HEX)
  string(SUBSTRING "${hex}" 0 16 signature)
  string(SUBSTRING "${hex}" 24 8 chunk)
  if(NOT signature STREQUAL "89504e470d0a1a0a")
    fail("no PNG signature (${signature})")
  endif()
  if(NOT chunk STREQUAL "49484452")
    fail("first chunk is not IHDR")
  endif()
  read_uint("${hex}" 16 4 big width)
  read_uint("${hex}" 20 4 big height)
  if(DEFINED WIDTH AND NOT width EQUAL WIDTH)
    fail("width ${width}, expected ${WIDTH}")
  endif()
  if(DEFINED HEIGHT AND NOT height EQUAL HEIGHT)
    fail("height ${height}, expected ${HEIGHT}")
  endif()
  message(STATUS "${FILE}: PNG ${width} x ${height}")

elseif(KIND STREQUAL "stl")
  # Binary STL: 80-byte header, uint32 count, 50 bytes per triangle.
  file(READ "${FILE}" hex LIMIT 84 HEX)
  read_uint("${hex}" 80 4 little n)
  math(EXPR want "84 + 50 * ${n}")
  if(NOT size EQUAL want)
    fail("${size} bytes, but its header declares ${n} triangles (${want} bytes)")
  endif()
  check_triangles(${n})
  message(STATUS "${FILE}: binary STL, ${n} triangles")

elseif(KIND STREQUAL "3mf")
  file(READ "${FILE}" hex LIMIT 4 HEX)
  if(NOT hex STREQUAL "504b0304")
    fail("not a zip archive (${hex})")
  endif()
  if(CMAKE_VERSION VERSION_LESS 3.18)
    message(STATUS "${FILE}: zip signature only; unpacking needs CMake 3.18")
    return()
  endif()
  get_filename_component(name "${FILE}" NAME)
  set(dir "${CMAKE_CURRENT_BINARY_DIR}/${name}.unpacked")
  file(REMOVE_RECURSE "${dir}")
  file(ARCHIVE_EXTRACT INPUT "${FILE}" DESTINATION "${dir}")
  foreach(part "[Content_Types].xml" "_rels/.rels" "3D/3dmodel.model")
    if(NOT EXISTS "${dir}/${part}")
      fail("no ${part} in the package")
    endif()
  endforeach()
  file(STRINGS "${dir}/3D/3dmodel.model" tris REGEX "<triangle ")
  list(LENGTH tris n)
  file(STRINGS "${dir}/3D/3dmodel.model" verts REGEX "<vertex ")
  list(LENGTH verts nv)
  file(REMOVE_RECURSE "${dir}")
  if(nv EQUAL 0)
    fail("the model has no vertices")
  endif()
  check_triangles(${n})
  message(STATUS "${FILE}: 3MF package, ${nv} vertices, ${n} triangles")

elseif(KIND STREQUAL "obj")
  obj_counts("${FILE}" nv n)
  if(nv EQUAL 0)
    fail("no vertices")
  endif()
  check_triangles(${n})
  if(DEFINED EXACT_VERTICES AND NOT nv EQUAL EXACT_VERTICES)
    fail("${nv} vertices, expected ${EXACT_VERTICES}")
  endif()
  if(DEFINED EXACT_FACES AND NOT n EQUAL EXACT_FACES)
    fail("${n} faces, expected ${EXACT_FACES}")
  endif()
  message(STATUS "${FILE}: OBJ, ${nv} vertices, ${n} triangles")

elseif(KIND STREQUAL "svg")
  file(READ "${FILE}" text)
  if(NOT text MATCHES "<svg[ >]" OR NOT text MATCHES "</svg>[ \r\n]*$")
    fail("not a complete <svg> document")
  endif()
  if(DEFINED CONTAINS)
    string(FIND "${text}" "${CONTAINS}" at)
    if(at EQUAL -1)
      fail("does not contain '${CONTAINS}'")
    endif()
  endif()
  message(STATUS "${FILE}: SVG, ${size} bytes")

elseif(KIND STREQUAL "json")
  file(READ "${FILE}" text)
  if(CMAKE_VERSION VERSION_LESS 3.19)
    message(STATUS "${FILE}: ${size} bytes; parsing JSON needs CMake 3.19")
    return()
  endif()
  string(JSON op ERROR_VARIABLE err GET "${text}" root op)
  if(err)
    fail("not a JSON field graph: ${err}")
  endif()
  if(DEFINED JSON_ROOT_OP AND NOT op STREQUAL JSON_ROOT_OP)
    fail("root op '${op}', expected '${JSON_ROOT_OP}'")
  endif()
  message(STATUS "${FILE}: JSON field graph, root op '${op}'")

else()
  message(FATAL_ERROR "unknown KIND '${KIND}'")
endif()
