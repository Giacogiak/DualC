#pragma once

#include "dualc/implicit.h"
#include "dualc/types.h"

#include <string>

// field_slice -- sample any implicit field on an axis-aligned plane and write
// what it shows (roadmap 21 #59).
//
// Where the contour path materialises the zero set as a mesh, a slice never
// does: it evaluates field.valueAt on a regular res x res grid over the plane's
// footprint in a region. Cost is O(res^2) and independent of the field's
// detail, so it stays cheap on parts that would OOM the mesher. Two outputs:
//   * a PNG heatmap (diverging blue=inside / white=surface / red=outside) with
//     the zero-isocontour burned in, for a quick visual read;
//   * an SVG of that zero-isocontour as line segments (marching squares) -- the
//     authoritative cross-section geometry, and the seed of future per-layer
//     direct print-slicing.
//
// Called by dualc_field --slice (any field graph); the retired dualc_slice
// (a TPMS-in-mesh field from flags) shared it and wrote the same bytes.

namespace dce {

// --res: default and clamp range, the same for every slicing CLI.
constexpr int kSliceDefaultRes = 512;
constexpr int kSliceMinRes = 2;
constexpr int kSliceMaxRes = 4096;

// `res` clamped into [kSliceMinRes, kSliceMaxRes].
int clampSliceRes(int res);

// Parse "z=1.5" into (axis 0/1/2, value). AXIS is x, y or z (either case).
// False on malformed input.
bool parseSlicePlane(const std::string& s, int& axis, double& value);

// Replace (or append) the extension on `base` with `ext` (which includes the
// dot). "a/b.png" + ".svg" -> "a/b.svg"; "a/b" + ".png" -> "a/b.png".
std::string withExt(const std::string& base, const std::string& ext);

// `path` without a trailing .png / .svg (any case); any other path unchanged.
// "a/b.png" -> "a/b"; "a/b.v2" -> "a/b.v2".
std::string stripSliceExt(const std::string& path);

// Sample `field` on the plane axis==pos across `region`'s footprint at res x res
// and write the heatmap to `pngPath` and the contour to `svgPath`. The in-plane
// axes are (u, v): a z-plane is (x, y), an x-plane (y, z), a y-plane (x, z).
// Progress, warnings (a plane outside the region, an empty cross-section) and
// errors go to stdout / stderr under the `[tag]` prefix. Returns a process rc:
// 0 ok, 2 a write failure.
int writeFieldSlice(const dualc::ImplicitField& field, const dualc::BBox& region,
                    int axis, double pos, int res, const std::string& pngPath,
                    const std::string& svgPath, const std::string& tag);

}  // namespace dce
