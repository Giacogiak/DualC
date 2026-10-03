#include "dualc/hermite_octree.h"
#include "dualc/types.h"

#include "internal/octree.h"

#include <cmath>
#include <limits>

namespace dualc {

// --- BBox -------------------------------------------------------------------

BBox BBox::unit() {
  BBox b;
  b.min = Vector3{-0.5, -0.5, -0.5};
  b.max = Vector3{ 0.5,  0.5,  0.5};
  return b;
}

Vector3 BBox::center() const {
  return (min + max) * 0.5;
}

Vector3 BBox::extent() const {
  return max - min;
}

bool BBox::isValid() const {
  auto finite = [](double v) { return std::isfinite(v); };
  return finite(min.x) && finite(min.y) && finite(min.z) &&
         finite(max.x) && finite(max.y) && finite(max.z) &&
         max.x >= min.x && max.y >= min.y && max.z >= min.z;
}

BBox BBox::empty() {
  const double inf = std::numeric_limits<double>::infinity();
  BBox b;
  b.min = Vector3{ inf,  inf,  inf};   // inverted on purpose: max < min, so
  b.max = Vector3{-inf, -inf, -inf};   // isValid() is false and stays false
  return b;
}

BBox BBox::infinite() {
  const double inf = std::numeric_limits<double>::infinity();
  BBox b;
  b.min = Vector3{-inf, -inf, -inf};
  b.max = Vector3{ inf,  inf,  inf};
  return b;
}

bool BBox::isInfinite() const {
  const double inf = std::numeric_limits<double>::infinity();
  return min.x == -inf && min.y == -inf && min.z == -inf &&
         max.x == inf && max.y == inf && max.z == inf;
}

// --- HermiteOctree ----------------------------------------------------------

HermiteOctree::HermiteOctree() = default;

HermiteOctree::HermiteOctree(BBox rootBounds) {
  auto r = std::make_unique<HermiteNode>();
  r->bounds = rootBounds;
  r->depth  = 0;
  r->isLeaf = true;
  root_ = std::move(r);
}

HermiteOctree::~HermiteOctree() = default;

HermiteOctree::HermiteOctree(HermiteOctree&&) noexcept            = default;
HermiteOctree& HermiteOctree::operator=(HermiteOctree&&) noexcept = default;

std::size_t HermiteOctree::leafCount() const {
  return internal::countLeaves(root_.get());
}

std::size_t HermiteOctree::nodeCount() const {
  return internal::countNodes(root_.get());
}

} // namespace dualc
