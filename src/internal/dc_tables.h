#pragma once

#include <array>
#include <cstdint>

// Cube topology constants used throughout DualC.
//
// Corner index encoding: c = (z << 2) | (y << 1) | x, where each axis bit is
// 0 or 1. So corner 0 is (0,0,0), corner 7 is (1,1,1).
//
// Edge indices are axis-major: edges 0..3 are x-aligned, 4..7 are y-aligned,
// 8..11 are z-aligned.
//
// Below the basic constants, this header also defines the descent tables
// used by the adaptive dual-contouring recursion (cellProc / faceProc /
// edgeProc). All tables are derived from cube combinatorics.
//
// COVERAGE: tests/test_dc_tables.cpp validates the four basic shape tables
// (kCornerOffset, kEdgeEndpoints, kEdgeAxis, kFaceCorners) AND all six descent
// tables below (kCellProcFaceMask, kCellProcEdgeMask, kFaceProcFaceMask,
// kFaceProcEdgeMask, kEdgeProcEdgeMask, kProcessEdgeMask). The descent-table
// cases re-derive each expected entry from the basic tables and the shared
// (P1, P2) node-ordering convention rather than restating the data, so a
// transcription slip fails with the offending index named. That matters
// because the fallback evidence -- the end-to-end Euler-characteristic and
// boundary-edge assertions in tests/test_demo_meshes.cpp -- is real but
// localises poorly: a wrong entry surfaces there as a crack somewhere in a
// mesh.

namespace dualc {
namespace tables {

// (x, y, z) offset for each corner index 0..7. Each component is 0 or 1.
extern const std::array<std::array<std::uint8_t, 3>, 8> kCornerOffset;

// For each edge 0..11, the pair of corner indices it connects (low, high).
extern const std::array<std::array<std::uint8_t, 2>, 12> kEdgeEndpoints;

// Axis (0=X, 1=Y, 2=Z) along which each edge runs.
extern const std::array<std::uint8_t, 12> kEdgeAxis;

// CCW corners on each axis-aligned face (viewed from outside the cube).
//   0: x=0 plane, 1: x=1 plane, 2: y=0, 3: y=1, 4: z=0, 5: z=1.
extern const std::array<std::array<std::uint8_t, 4>, 6> kFaceCorners;

// === Descent tables for cellProc / faceProc / edgeProc recursion ===========

// cellProc: for an internal node, the 12 internal faces between sibling
// children. Each entry: (childA, childB, axis). childA is on the lo side of
// `axis`, childB on the hi side.
extern const std::array<std::array<std::uint8_t, 3>, 12> kCellProcFaceMask;

// cellProc: for an internal node, the 6 internal edges shared by 4 sibling
// children. Each entry: (c0, c1, c2, c3, axis). The 4 children are listed in
// the order edgeProc expects:
//   axis=X: (P1=Y=hi, P2=Z=hi), (Y=lo, Z=hi), (Y=lo, Z=lo), (Y=hi, Z=lo)
//   axis=Y: (P1=X=hi, P2=Z=hi), (X=lo, Z=hi), (X=lo, Z=lo), (X=hi, Z=lo)
//   axis=Z: (P1=X=hi, P2=Y=hi), (X=lo, Y=hi), (X=lo, Y=lo), (X=hi, Y=lo)
extern const std::array<std::array<std::uint8_t, 5>, 6> kCellProcEdgeMask;

// faceProc: for a face between two cells (faceProc(A, B, axis)) that needs
// to descend, the 4 sub-faces of the shared face. Each entry: (childA-of-A,
// childB-of-B). When A is a leaf we treat it as its own children for these
// indices (caller substitutes A for the entry).
extern const std::array<std::array<std::array<std::uint8_t, 2>, 4>, 3> kFaceProcFaceMask;

// faceProc: 4 internal edges of the shared face (between A and B along
// `axis`). Each entry describes 4 cells around the edge:
//   childOf[k] : 0 = take child of A,  1 = take child of B
//   childIdx[k]: which 0..7 child within that node
//   edgeAxis   : the edge's axis (perpendicular to face axis)
struct FaceProcEdgeCall {
  std::array<std::uint8_t, 4> childOf;
  std::array<std::uint8_t, 4> childIdx;
  std::uint8_t                edgeAxis;
};
extern const std::array<std::array<FaceProcEdgeCall, 4>, 3> kFaceProcEdgeMask;

// edgeProc: for an edge surrounded by 4 cells (along `axis`) that needs to
// descend, split into 2 sub-edges along the edge axis. Each entry holds the
// child of each of the 4 cells contributing to that sub-edge.
//   axis=X: lo-x sub-edge, then hi-x.
//   axis=Y: lo-y, then hi-y.
//   axis=Z: lo-z, then hi-z.
extern const std::array<std::array<std::array<std::uint8_t, 4>, 2>, 3> kEdgeProcEdgeMask;

// edgeProc terminal: when all 4 cells are leaves, this gives the local
// edge-index 0..11 in each of the 4 cells that corresponds to the absolute
// edge being processed.
//   axis=X: {0, 1, 3, 2}  (edges 0..3 are x-aligned)
//   axis=Y: {4, 5, 7, 6}
//   axis=Z: {8, 9, 11, 10}
extern const std::array<std::array<std::uint8_t, 4>, 3> kProcessEdgeMask;

} // namespace tables
} // namespace dualc
