#include "octree.h"

#include "dc_tables.h"

namespace dualc {
namespace internal {

std::size_t countLeaves(const HermiteNode* node) {
  if (node == nullptr) return 0;
  if (node->isLeaf) return 1;
  std::size_t total = 0;
  for (const auto& c : node->children) total += countLeaves(c.get());
  return total;
}

std::size_t countNodes(const HermiteNode* node) {
  if (node == nullptr) return 0;
  std::size_t total = 1;
  if (!node->isLeaf) {
    for (const auto& c : node->children) total += countNodes(c.get());
  }
  return total;
}

BBox childBounds(const BBox& parent, int childIndex) {
  const auto& off = tables::kCornerOffset[static_cast<std::size_t>(childIndex)];
  const Vector3 c = (parent.min + parent.max) * 0.5;
  BBox out;
  out.min = Vector3{
      off[0] ? c.x : parent.min.x,
      off[1] ? c.y : parent.min.y,
      off[2] ? c.z : parent.min.z};
  out.max = Vector3{
      off[0] ? parent.max.x : c.x,
      off[1] ? parent.max.y : c.y,
      off[2] ? parent.max.z : c.z};
  return out;
}

} // namespace internal
} // namespace dualc
