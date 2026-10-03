#pragma once

#include "dualc/hermite_octree.h"

#include <cstddef>

namespace dualc {
namespace internal {

// Counters used by HermiteOctree::leafCount() / nodeCount().
std::size_t countLeaves(const HermiteNode* node);
std::size_t countNodes(const HermiteNode* node);

// Iterates the standard child indexing convention. Given a parent BBox and a
// child slot c in 0..7, returns the child's BBox.
BBox childBounds(const BBox& parent, int childIndex);

} // namespace internal
} // namespace dualc
