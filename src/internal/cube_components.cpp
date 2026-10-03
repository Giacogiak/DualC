#include "internal/cube_components.h"

#include "internal/dc_tables.h"

#include <array>
#include <cassert>

namespace dualc {
namespace internal {

namespace {

// Cube edge (0..11) joining two corners, or -1 if the corners are not
// edge-adjacent (i.e. differ in more than one of (x, y, z)).
std::int8_t edgeBetweenCorners(std::uint8_t cA, std::uint8_t cB) {
  for (std::uint8_t e = 0; e < 12; ++e) {
    const auto& ep = tables::kEdgeEndpoints[e];
    if ((ep[0] == cA && ep[1] == cB) || (ep[0] == cB && ep[1] == cA)) {
      return static_cast<std::int8_t>(e);
    }
  }
  return -1;
}

// Tiny union-find over the 12 cube edges. Path compression; no union-by-rank
// (the tree is at most 12 deep -- trivial).
struct UF {
  std::array<std::int8_t, 12> parent{};
  UF() { for (std::int8_t i = 0; i < 12; ++i) parent[static_cast<std::size_t>(i)] = i; }
  std::int8_t find(std::int8_t x) {
    while (parent[static_cast<std::size_t>(x)] != x) {
      parent[static_cast<std::size_t>(x)] =
          parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])];
      x = parent[static_cast<std::size_t>(x)];
    }
    return x;
  }
  void unite(std::int8_t a, std::int8_t b) {
    const std::int8_t ra = find(a);
    const std::int8_t rb = find(b);
    if (ra != rb) parent[static_cast<std::size_t>(ra)] = rb;
  }
};

} // namespace

EdgeComponents partitionCubeEdges(
    const std::array<bool, 8>&  cornerInside,
    const std::array<bool, 12>& hasCrossing) {
  EdgeComponents out;
  out.componentOfEdge.fill(-1);
  out.numComponents = 0;

  UF uf;

  for (std::size_t f = 0; f < 6; ++f) {
    const auto& fc = tables::kFaceCorners[f];

    // The 4 face-boundary edges, indexed CCW around the face: edge i goes
    // from fc[i] to fc[(i+1)%4]. Map each to its cube-edge index.
    std::array<std::int8_t, 4> faceEdge{};
    for (int i = 0; i < 4; ++i) {
      faceEdge[static_cast<std::size_t>(i)] =
          edgeBetweenCorners(fc[static_cast<std::size_t>(i)],
                             fc[static_cast<std::size_t>((i + 1) % 4)]);
    }

    // Crossings on this face: positions (0..3) where the face-edge has a
    // sign-change crossing on the cube.
    std::array<int, 4> crossingPos{};
    int faceCrossings = 0;
    for (int i = 0; i < 4; ++i) {
      const std::int8_t e = faceEdge[static_cast<std::size_t>(i)];
      if (e >= 0 && hasCrossing[static_cast<std::size_t>(e)]) {
        crossingPos[static_cast<std::size_t>(faceCrossings)] = i;
        ++faceCrossings;
      }
    }

    if (faceCrossings == 0) continue;

    if (faceCrossings == 2) {
      const std::int8_t eA =
          faceEdge[static_cast<std::size_t>(crossingPos[0])];
      const std::int8_t eB =
          faceEdge[static_cast<std::size_t>(crossingPos[1])];
      uf.unite(eA, eB);
      continue;
    }

    // faceCrossings == 4: saddle face. The face's 4 corners alternate in
    // sign (any other parity would give 0, 2, or 4 sign changes; only the
    // strict alternation gives exactly 4). Disambiguate by the inside-pair
    // rule -- pair the two crossings that share an inside corner.
    //
    // Edge i (CCW) connects fc[i] and fc[i+1]. With alternating signs,
    // edges (i-1) and (i) share corner fc[i]; if that corner is inside,
    // edges (i-1, i) form one component on this face.
    //
    // For (s0, s1, s2, s3) = (in, out, in, out): inside corners at fc[0]
    // (joined by edges 3, 0) and fc[2] (joined by edges 1, 2).
    // For (out, in, out, in): inside corners at fc[1] (edges 0, 1) and
    // fc[3] (edges 2, 3).
    if (cornerInside[fc[0]]) {
      uf.unite(faceEdge[3], faceEdge[0]);
      uf.unite(faceEdge[1], faceEdge[2]);
    } else {
      uf.unite(faceEdge[0], faceEdge[1]);
      uf.unite(faceEdge[2], faceEdge[3]);
    }
  }

  // Compact UF roots into 0-indexed component IDs.
  std::array<std::int8_t, 12> rootToId{};
  rootToId.fill(-1);
  for (std::uint8_t e = 0; e < 12; ++e) {
    if (!hasCrossing[e]) {
      out.componentOfEdge[e] = -1;
      continue;
    }
    const std::int8_t root = uf.find(static_cast<std::int8_t>(e));
    if (rootToId[static_cast<std::size_t>(root)] < 0) {
      rootToId[static_cast<std::size_t>(root)] =
          static_cast<std::int8_t>(out.numComponents++);
    }
    out.componentOfEdge[e] = rootToId[static_cast<std::size_t>(root)];
  }

  assert(out.numComponents <= 4 && "cube has at most 4 surface components");
  return out;
}

} // namespace internal
} // namespace dualc
