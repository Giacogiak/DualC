#include "dc_tables.h"

namespace dualc {
namespace tables {

const std::array<std::array<std::uint8_t, 3>, 8> kCornerOffset = {{
    {{0, 0, 0}}, // 0
    {{1, 0, 0}}, // 1
    {{0, 1, 0}}, // 2
    {{1, 1, 0}}, // 3
    {{0, 0, 1}}, // 4
    {{1, 0, 1}}, // 5
    {{0, 1, 1}}, // 6
    {{1, 1, 1}}, // 7
}};

const std::array<std::array<std::uint8_t, 2>, 12> kEdgeEndpoints = {{
    // X-aligned edges (low, high)
    {{0, 1}}, {{2, 3}}, {{4, 5}}, {{6, 7}},
    // Y-aligned
    {{0, 2}}, {{1, 3}}, {{4, 6}}, {{5, 7}},
    // Z-aligned
    {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
}};

const std::array<std::uint8_t, 12> kEdgeAxis = {{
    0, 0, 0, 0,
    1, 1, 1, 1,
    2, 2, 2, 2,
}};

const std::array<std::array<std::uint8_t, 4>, 6> kFaceCorners = {{
    {{0, 4, 6, 2}}, // x=0
    {{1, 3, 7, 5}}, // x=1
    {{0, 1, 5, 4}}, // y=0
    {{2, 6, 7, 3}}, // y=1
    {{0, 2, 3, 1}}, // z=0
    {{4, 5, 7, 6}}, // z=1
}};

// --- cellProc face mask ----------------------------------------------------
//
// 12 internal faces between sibling children: 4 along each axis, indexed by
// the (y, z) / (x, z) / (x, y) coords of the 4 cells at the lo side.

const std::array<std::array<std::uint8_t, 3>, 12> kCellProcFaceMask = {{
    // X-axis faces: lo-x child, hi-x child
    {{0, 1, 0}}, {{2, 3, 0}}, {{4, 5, 0}}, {{6, 7, 0}},
    // Y-axis faces: lo-y, hi-y
    {{0, 2, 1}}, {{1, 3, 1}}, {{4, 6, 1}}, {{5, 7, 1}},
    // Z-axis faces: lo-z, hi-z
    {{0, 4, 2}}, {{1, 5, 2}}, {{2, 6, 2}}, {{3, 7, 2}},
}};

// --- cellProc edge mask ----------------------------------------------------
//
// 6 internal edges shared by 4 sibling children. The 4 children are listed
// in the convention edgeProc expects:
//   axis=X: (P1=Y=hi, P2=Z=hi), (Y=lo, Z=hi), (Y=lo, Z=lo), (Y=hi, Z=lo)
//   axis=Y: (P1=X=hi, P2=Z=hi), (X=lo, Z=hi), (X=lo, Z=lo), (X=hi, Z=lo)
//   axis=Z: (P1=X=hi, P2=Y=hi), (X=lo, Y=hi), (X=lo, Y=lo), (X=hi, Y=lo)
const std::array<std::array<std::uint8_t, 5>, 6> kCellProcEdgeMask = {{
    // X-axis: lo-x sub-edge (children at lo-x of parent: c0, c2, c4, c6)
    //   y=hi,z=hi: c6;  y=lo,z=hi: c4;  y=lo,z=lo: c0;  y=hi,z=lo: c2
    {{6, 4, 0, 2, 0}},
    // X-axis: hi-x sub-edge
    {{7, 5, 1, 3, 0}},
    // Y-axis: lo-y sub-edge
    //   x=hi,z=hi: c5;  x=lo,z=hi: c4;  x=lo,z=lo: c0;  x=hi,z=lo: c1
    {{5, 4, 0, 1, 1}},
    // Y-axis: hi-y sub-edge
    {{7, 6, 2, 3, 1}},
    // Z-axis: lo-z sub-edge
    //   x=hi,y=hi: c3;  x=lo,y=hi: c2;  x=lo,y=lo: c0;  x=hi,y=lo: c1
    {{3, 2, 0, 1, 2}},
    // Z-axis: hi-z sub-edge
    {{7, 6, 4, 5, 2}},
}};

// --- faceProc face mask ----------------------------------------------------
//
// For face between A (lo-axis) and B (hi-axis), the 4 sub-faces. Each entry:
// (childA-from-A, childB-from-B). Order: 4 sub-faces in the (P1, P2) plane.

const std::array<std::array<std::array<std::uint8_t, 2>, 4>, 3> kFaceProcFaceMask = {{
    // axis=X: P1=Y, P2=Z
    {{
        {{1, 0}}, // (y=lo, z=lo)
        {{3, 2}}, // (y=hi, z=lo)
        {{5, 4}}, // (y=lo, z=hi)
        {{7, 6}}, // (y=hi, z=hi)
    }},
    // axis=Y: P1=X, P2=Z
    {{
        {{2, 0}}, // (x=lo, z=lo)
        {{3, 1}}, // (x=hi, z=lo)
        {{6, 4}}, // (x=lo, z=hi)
        {{7, 5}}, // (x=hi, z=hi)
    }},
    // axis=Z: P1=X, P2=Y
    {{
        {{4, 0}}, // (x=lo, y=lo)
        {{5, 1}}, // (x=hi, y=lo)
        {{6, 2}}, // (x=lo, y=hi)
        {{7, 3}}, // (x=hi, y=hi)
    }},
}};

// --- faceProc edge mask ----------------------------------------------------
//
// 4 internal edges of the shared face. Each lists 4 cells around the edge in
// edgeProc's CCW order ((P1=hi,P2=hi), (P1=lo,P2=hi), (P1=lo,P2=lo), (P1=hi,P2=lo))
// where the perpendicular axes are determined by the EDGE's axis (not the
// face's). Each cell is described by which of (A, B) it belongs to and its
// child index there.

// MSVC is picky about brace nesting for std::array<Struct, N> where Struct
// has std::array members; using brace-elision form keeps it happy.
const std::array<std::array<FaceProcEdgeCall, 4>, 3> kFaceProcEdgeMask = {
    // ===== axis=X face =====
    std::array<FaceProcEdgeCall, 4>{
        FaceProcEdgeCall{ {1, 0, 0, 1}, {4, 5, 1, 0}, 1 },  // Y-axis edge, lo-y, z=mid
        FaceProcEdgeCall{ {1, 0, 0, 1}, {6, 7, 3, 2}, 1 },  // Y-axis edge, hi-y
        FaceProcEdgeCall{ {1, 0, 0, 1}, {2, 3, 1, 0}, 2 },  // Z-axis edge, lo-z, y=mid
        FaceProcEdgeCall{ {1, 0, 0, 1}, {6, 7, 5, 4}, 2 },  // Z-axis edge, hi-z
    },
    // ===== axis=Y face =====
    std::array<FaceProcEdgeCall, 4>{
        FaceProcEdgeCall{ {1, 0, 0, 1}, {4, 6, 2, 0}, 0 },  // X-axis edge, lo-x, z=mid
        FaceProcEdgeCall{ {1, 0, 0, 1}, {5, 7, 3, 1}, 0 },  // X-axis edge, hi-x
        FaceProcEdgeCall{ {1, 1, 0, 0}, {1, 0, 2, 3}, 2 },  // Z-axis edge, lo-z, x=mid
        FaceProcEdgeCall{ {1, 1, 0, 0}, {5, 4, 6, 7}, 2 },  // Z-axis edge, hi-z
    },
    // ===== axis=Z face =====
    std::array<FaceProcEdgeCall, 4>{
        FaceProcEdgeCall{ {1, 1, 0, 0}, {2, 0, 4, 6}, 0 },  // X-axis edge, lo-x, y=mid
        FaceProcEdgeCall{ {1, 1, 0, 0}, {3, 1, 5, 7}, 0 },  // X-axis edge, hi-x
        FaceProcEdgeCall{ {1, 1, 0, 0}, {1, 0, 4, 5}, 1 },  // Y-axis edge, lo-y, x=mid
        FaceProcEdgeCall{ {1, 1, 0, 0}, {3, 2, 6, 7}, 1 },  // Y-axis edge, hi-y
    },
};

// --- edgeProc edge mask ----------------------------------------------------
//
// edgeProc(node[0..3], axis): if any node is internal, descend into 2
// sub-edges along the edge axis. Each sub-edge has 4 children, one from each
// node[k]. Children are taken from the corresponding child-of-node[k] that
// borders the edge.

const std::array<std::array<std::array<std::uint8_t, 4>, 2>, 3> kEdgeProcEdgeMask = {{
    // axis=X: node[0]=Y+Z+, node[1]=Y-Z+, node[2]=Y-Z-, node[3]=Y+Z-
    //   For each node[k], the edge sits at its (y, z) corner facing the edge.
    //     node[0] at y=hi z=hi from edge => edge at node[0]'s (lo-y, lo-z) corner
    //       => children touching that corner: c0 (any-x, lo-y, lo-z) for lo-x sub, c1 for hi-x
    {{
        {{0, 2, 6, 4}}, // lo-x sub-edge
        {{1, 3, 7, 5}}, // hi-x sub-edge
    }},
    // axis=Y
    {{
        {{0, 1, 5, 4}}, // lo-y sub-edge
        {{2, 3, 7, 6}}, // hi-y sub-edge
    }},
    // axis=Z
    {{
        {{0, 1, 3, 2}}, // lo-z sub-edge
        {{4, 5, 7, 6}}, // hi-z sub-edge
    }},
}};

// --- processEdgeMask -------------------------------------------------------
//
// At edgeProc's terminal case (all 4 nodes are leaves), each of the 4 cells
// "owns" the edge as one of its 12 local edges. This table gives the local
// edge index in each cell.

const std::array<std::array<std::uint8_t, 4>, 3> kProcessEdgeMask = {{
    // axis=X: node[0]=(Y+,Z+) => its X-axis edge at (lo-y, lo-z) = local edge 0
    //         node[1]=(Y-,Z+) => X-axis edge at (hi-y, lo-z) = edge 1
    //         node[2]=(Y-,Z-) => X-axis edge at (hi-y, hi-z) = edge 3
    //         node[3]=(Y+,Z-) => X-axis edge at (lo-y, hi-z) = edge 2
    {{0, 1, 3, 2}},
    // axis=Y: node[0]=(X+,Z+) => Y-axis edge at (lo-x, lo-z) = edge 4
    //         node[1]=(X-,Z+) => Y-axis edge at (hi-x, lo-z) = edge 5
    //         node[2]=(X-,Z-) => Y-axis edge at (hi-x, hi-z) = edge 7
    //         node[3]=(X+,Z-) => Y-axis edge at (lo-x, hi-z) = edge 6
    {{4, 5, 7, 6}},
    // axis=Z: node[0]=(X+,Y+) => Z-axis edge at (lo-x, lo-y) = edge 8
    //         node[1]=(X-,Y+) => Z-axis edge at (hi-x, lo-y) = edge 9
    //         node[2]=(X-,Y-) => Z-axis edge at (hi-x, hi-y) = edge 11
    //         node[3]=(X+,Y-) => Z-axis edge at (lo-x, hi-y) = edge 10
    {{8, 9, 11, 10}},
}};

} // namespace tables
} // namespace dualc
