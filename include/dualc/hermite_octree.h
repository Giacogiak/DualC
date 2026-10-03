#pragma once

#include "dualc/types.h"

#include <array>
#include <cstddef>
#include <memory>

namespace dualc {

// Per-cube-edge Hermite sample. Only meaningful when the two endpoint signs
// differ (hasCrossing == true).
struct HermiteEdge {
  Vector3 position{0.0, 0.0, 0.0};
  Vector3 normal{0.0, 0.0, 0.0};
  bool    hasCrossing = false;
};

// Octree leaf cell payload: 8 corner signs and 12 edge crossings.
// Corner index = (z << 2) | (y << 1) | x. Edge index per dc_tables.h.
struct HermiteLeafData {
  std::array<bool, 8>        cornerInside{};
  std::array<HermiteEdge, 12> edges{};
};

struct HermiteNode {
  BBox bounds{};
  int  depth  = 0;
  bool isLeaf = true;
  std::unique_ptr<HermiteLeafData> leaf{};
  std::array<std::unique_ptr<HermiteNode>, 8> children{};
};

class HermiteOctree {
public:
  HermiteOctree();
  explicit HermiteOctree(BBox rootBounds);
  ~HermiteOctree();

  HermiteOctree(HermiteOctree&&) noexcept;
  HermiteOctree& operator=(HermiteOctree&&) noexcept;

  HermiteOctree(const HermiteOctree&)            = delete;
  HermiteOctree& operator=(const HermiteOctree&) = delete;

  const HermiteNode* root() const { return root_.get(); }
  HermiteNode*       root()       { return root_.get(); }

  void setRoot(std::unique_ptr<HermiteNode> r) { root_ = std::move(r); }

  std::size_t leafCount() const;
  std::size_t nodeCount() const;

private:
  std::unique_ptr<HermiteNode> root_{};
};

} // namespace dualc
