#include "internal/dc_tables.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>

using namespace dualc::tables;

TEST_CASE("kCornerOffset uses the canonical (z<<2 | y<<1 | x) encoding",
          "[dc_tables]") {
  for (std::uint8_t i = 0; i < 8; ++i) {
    const auto& off = kCornerOffset[i];
    REQUIRE(off[0] == ((i >> 0) & 1u));
    REQUIRE(off[1] == ((i >> 1) & 1u));
    REQUIRE(off[2] == ((i >> 2) & 1u));
  }
}

TEST_CASE("Each cube edge connects two corners that differ in exactly one bit",
          "[dc_tables]") {
  for (std::uint8_t e = 0; e < 12; ++e) {
    const auto a = kEdgeEndpoints[e][0];
    const auto b = kEdgeEndpoints[e][1];
    REQUIRE(a < 8);
    REQUIRE(b < 8);
    REQUIRE(a != b);
    const std::uint8_t diff = a ^ b;
    REQUIRE((diff == 1 || diff == 2 || diff == 4));
    // The varying bit must agree with the declared axis.
    const std::uint8_t expectedAxis = (diff == 1) ? 0u : (diff == 2) ? 1u : 2u;
    REQUIRE(kEdgeAxis[e] == expectedAxis);
    // Endpoint convention: low coord first.
    REQUIRE(a < b);
  }
}

TEST_CASE("Edges cover all 12 cube edges with no duplicates", "[dc_tables]") {
  std::set<std::pair<std::uint8_t, std::uint8_t>> edgeSet;
  for (std::uint8_t e = 0; e < 12; ++e) {
    edgeSet.emplace(kEdgeEndpoints[e][0], kEdgeEndpoints[e][1]);
  }
  REQUIRE(edgeSet.size() == 12);
}

TEST_CASE("Each axis owns exactly 4 edges", "[dc_tables]") {
  std::array<int, 3> count{0, 0, 0};
  for (auto a : kEdgeAxis) ++count[a];
  REQUIRE(count[0] == 4);
  REQUIRE(count[1] == 4);
  REQUIRE(count[2] == 4);
}

TEST_CASE("Each face is a 4-cycle and lies on a constant-axis plane",
          "[dc_tables]") {
  for (std::uint8_t f = 0; f < 6; ++f) {
    const auto axis = static_cast<std::uint8_t>(f / 2u);
    const std::uint8_t bit = static_cast<std::uint8_t>(f % 2u);
    std::set<std::uint8_t> seen;
    for (std::uint8_t i = 0; i < 4; ++i) {
      const std::uint8_t corner = kFaceCorners[f][i];
      REQUIRE(corner < 8);
      seen.insert(corner);
      const std::uint8_t cBit = static_cast<std::uint8_t>((corner >> axis) & 1u);
      REQUIRE(cBit == bit);
    }
    REQUIRE(seen.size() == 4);
  }
}

// ===========================================================================
// Descent tables (cellProc / faceProc / edgeProc)
// ===========================================================================
//
// These six tables are hand-transcribed cube combinatorics and were previously
// covered only end-to-end, through the Euler-characteristic assertions in
// test_demo_meshes.cpp -- real evidence, but it localises terribly. Every
// check below RE-DERIVES the expected entry from the four basic tables above
// rather than restating the table, so a transcription slip fails here with the
// offending index named.
//
// Shared convention, stated once. For an edge or face whose axis is `a`, the
// two perpendicular axes are taken in ASCENDING index order:
//   a = X -> (P1, P2) = (Y, Z);  a = Y -> (X, Z);  a = Z -> (X, Y).
// edgeProc visits the 4 cells around an edge in the CCW order
//   (P1 hi, P2 hi), (P1 lo, P2 hi), (P1 lo, P2 lo), (P1 hi, P2 lo).

namespace {

std::array<int, 2> perpAxes(int axis) {
  std::array<int, 2> p{};
  int n = 0;
  for (int a = 0; a < 3; ++a) {
    if (a != axis) p[n++] = a;
  }
  return p;
}

// The 4 (P1, P2) relative positions in edgeProc's CCW node order.
std::array<int, 2> edgeProcNodePos(int k) {
  static const std::array<std::array<int, 2>, 4> kOrder = {{
      {{1, 1}}, {{0, 1}}, {{0, 0}}, {{1, 0}},
  }};
  return kOrder[static_cast<std::size_t>(k)];
}

int cornerFromBits(int bx, int by, int bz) { return bx | (by << 1) | (bz << 2); }

int cellWithBits(int axis, int axisBit, const std::array<int, 2>& perp,
                 int p1Bit, int p2Bit) {
  int b[3] = {0, 0, 0};
  b[axis]    = axisBit;
  b[perp[0]] = p1Bit;
  b[perp[1]] = p2Bit;
  return cornerFromBits(b[0], b[1], b[2]);
}

// The local index of the axis-`axis` edge sitting at perpendicular position
// (p1Bit, p2Bit), found by searching kEdgeEndpoints -- i.e. derived from the
// basic tables, not restated.
int localEdgeAt(int axis, const std::array<int, 2>& perp, int p1Bit, int p2Bit) {
  for (std::uint8_t e = 0; e < 12; ++e) {
    if (kEdgeAxis[e] != axis) continue;
    const int a = kEdgeEndpoints[e][0];
    if (((a >> perp[0]) & 1) == p1Bit && ((a >> perp[1]) & 1) == p2Bit) {
      return static_cast<int>(e);
    }
  }
  return -1;
}

} // namespace

TEST_CASE("kCellProcFaceMask pairs siblings across one axis", "[dc_tables]") {
  std::array<int, 3> perAxis{0, 0, 0};
  std::set<std::pair<int, int>> seen;
  for (std::size_t i = 0; i < kCellProcFaceMask.size(); ++i) {
    const auto& m    = kCellProcFaceMask[i];
    const int   a    = m[2];
    const int   lo   = m[0];
    const int   hi   = m[1];
    INFO("kCellProcFaceMask entry " << i);
    REQUIRE(a >= 0);
    REQUIRE(a < 3);
    // Siblings sharing a face differ in exactly the face-axis bit...
    REQUIRE((lo ^ hi) == (1 << a));
    // ...and the first is the lo-side child.
    REQUIRE(((lo >> a) & 1) == 0);
    REQUIRE(seen.insert({lo, hi}).second);
    ++perAxis[static_cast<std::size_t>(a)];
  }
  for (int a = 0; a < 3; ++a) REQUIRE(perAxis[static_cast<std::size_t>(a)] == 4);
}

TEST_CASE("kCellProcEdgeMask lists the 4 siblings around each internal edge "
          "in edgeProc order", "[dc_tables]") {
  std::array<int, 3> perAxis{0, 0, 0};
  for (std::size_t i = 0; i < kCellProcEdgeMask.size(); ++i) {
    const auto& m    = kCellProcEdgeMask[i];
    const int   axis = m[4];
    INFO("kCellProcEdgeMask entry " << i);
    REQUIRE(axis >= 0);
    REQUIRE(axis < 3);
    const auto perp = perpAxes(axis);
    // All 4 siblings sit on the same side along the edge axis (the entry is
    // the lo- or hi-side sub-edge), and cover the 4 perpendicular quadrants
    // in edgeProc's CCW order.
    const int axisBit = (m[0] >> axis) & 1;
    for (int k = 0; k < 4; ++k) {
      const auto pos = edgeProcNodePos(k);
      REQUIRE(static_cast<int>(m[static_cast<std::size_t>(k)]) ==
              cellWithBits(axis, axisBit, perp, pos[0], pos[1]));
    }
    ++perAxis[static_cast<std::size_t>(axis)];
  }
  for (int a = 0; a < 3; ++a) REQUIRE(perAxis[static_cast<std::size_t>(a)] == 2);
}

TEST_CASE("kProcessEdgeMask names each cell's own copy of the shared edge",
          "[dc_tables]") {
  for (int axis = 0; axis < 3; ++axis) {
    const auto perp = perpAxes(axis);
    std::set<int> seen;
    for (int k = 0; k < 4; ++k) {
      const int e = kProcessEdgeMask[static_cast<std::size_t>(axis)]
                                    [static_cast<std::size_t>(k)];
      INFO("kProcessEdgeMask[" << axis << "][" << k << "] = " << e);
      REQUIRE(e >= 0);
      REQUIRE(e < 12);
      // It must be an edge along the right axis...
      REQUIRE(static_cast<int>(kEdgeAxis[static_cast<std::size_t>(e)]) == axis);
      // ...and specifically the one at the corner of that cell facing the
      // shared edge, i.e. the complement of the cell's own position.
      const auto pos = edgeProcNodePos(k);
      REQUIRE(e == localEdgeAt(axis, perp, 1 - pos[0], 1 - pos[1]));
      REQUIRE(seen.insert(e).second);
    }
  }
}

TEST_CASE("kFaceProcFaceMask pairs the 4 sub-faces across a shared face",
          "[dc_tables]") {
  for (int axis = 0; axis < 3; ++axis) {
    const auto perp = perpAxes(axis);
    for (int i = 0; i < 4; ++i) {
      const auto& pair = kFaceProcFaceMask[static_cast<std::size_t>(axis)]
                                          [static_cast<std::size_t>(i)];
      const int p1 = i & 1, p2 = (i >> 1) & 1;
      INFO("kFaceProcFaceMask[" << axis << "][" << i << "]");
      // The lo-side cell contributes its hi-axis child and vice versa; both
      // sit in the same perpendicular quadrant.
      REQUIRE(static_cast<int>(pair[0]) == cellWithBits(axis, 1, perp, p1, p2));
      REQUIRE(static_cast<int>(pair[1]) == cellWithBits(axis, 0, perp, p1, p2));
    }
  }
}

TEST_CASE("kEdgeProcEdgeMask picks the child of each cell that touches the "
          "sub-edge", "[dc_tables]") {
  for (int axis = 0; axis < 3; ++axis) {
    const auto perp = perpAxes(axis);
    for (int sub = 0; sub < 2; ++sub) {
      for (int k = 0; k < 4; ++k) {
        const int child = kEdgeProcEdgeMask[static_cast<std::size_t>(axis)]
                                           [static_cast<std::size_t>(sub)]
                                           [static_cast<std::size_t>(k)];
        const auto pos = edgeProcNodePos(k);
        INFO("kEdgeProcEdgeMask[" << axis << "][" << sub << "][" << k << "]");
        // The cell sits at (pos) relative to the edge, so the child touching
        // the edge is at the opposite corner, on sub-edge `sub` along the axis.
        REQUIRE(child ==
                cellWithBits(axis, sub, perp, 1 - pos[0], 1 - pos[1]));
      }
    }
  }
}

TEST_CASE("kFaceProcEdgeMask routes the shared face's 4 internal edges",
          "[dc_tables]") {
  for (int faceAxis = 0; faceAxis < 3; ++faceAxis) {
    const auto facePerp = perpAxes(faceAxis);
    for (int i = 0; i < 4; ++i) {
      // Entries run: first perpendicular axis (lo sub-edge, then hi), then
      // the second.
      const int edgeAxis = facePerp[static_cast<std::size_t>(i / 2)];
      const int sub      = i % 2;
      const auto& call = kFaceProcEdgeMask[static_cast<std::size_t>(faceAxis)]
                                          [static_cast<std::size_t>(i)];
      INFO("kFaceProcEdgeMask[" << faceAxis << "][" << i << "]");
      REQUIRE(static_cast<int>(call.edgeAxis) == edgeAxis);

      const auto perp = perpAxes(edgeAxis);
      for (int k = 0; k < 4; ++k) {
        const auto pos = edgeProcNodePos(k);
        // Which of the two face-sharing cells this node is: the one on the
        // hi side along the face axis is B (childOf == 1).
        const int faceSlot = (perp[0] == faceAxis) ? pos[0] : pos[1];
        INFO("  node " << k);
        REQUIRE(static_cast<int>(call.childOf[static_cast<std::size_t>(k)]) ==
                faceSlot);
        // Unlike cellProc's internal edges, only TWO cells surround this edge:
        // A and B each appear twice, straddling the non-face perpendicular
        // axis. So the child is the complement of the node position along the
        // face axis (that is the cell face touching the edge) but the SAME
        // side along the other perpendicular axis.
        const int p1Bit = (perp[0] == faceAxis) ? (1 - pos[0]) : pos[0];
        const int p2Bit = (perp[1] == faceAxis) ? (1 - pos[1]) : pos[1];
        REQUIRE(static_cast<int>(call.childIdx[static_cast<std::size_t>(k)]) ==
                cellWithBits(edgeAxis, sub, perp, p1Bit, p2Bit));
      }
    }
  }
}
