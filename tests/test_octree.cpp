#include "dualc/hermite_octree.h"
#include "internal/octree.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

using namespace dualc;

TEST_CASE("Default-constructed HermiteOctree has no root", "[octree]") {
  HermiteOctree o;
  REQUIRE(o.root() == nullptr);
  REQUIRE(o.nodeCount() == 0);
  REQUIRE(o.leafCount() == 0);
}

TEST_CASE("Root-only HermiteOctree counts as a single leaf", "[octree]") {
  HermiteOctree o(BBox::unit());
  REQUIRE(o.root() != nullptr);
  REQUIRE(o.root()->isLeaf);
  REQUIRE(o.nodeCount() == 1);
  REQUIRE(o.leafCount() == 1);
}

TEST_CASE("childBounds partitions the parent into 8 octants", "[octree]") {
  BBox parent;
  parent.min = Vector3{-1.0, -1.0, -1.0};
  parent.max = Vector3{ 1.0,  1.0,  1.0};

  for (int i = 0; i < 8; ++i) {
    const BBox c = internal::childBounds(parent, i);
    REQUIRE(c.isValid());

    // Each child has half the parent's extent on every axis.
    REQUIRE(c.extent().x == Approx(1.0));
    REQUIRE(c.extent().y == Approx(1.0));
    REQUIRE(c.extent().z == Approx(1.0));

    const bool wantHighX = (i & 1) != 0;
    const bool wantHighY = (i & 2) != 0;
    const bool wantHighZ = (i & 4) != 0;

    REQUIRE(c.min.x == (wantHighX ? Approx(0.0).margin(1e-12) : Approx(-1.0)));
    REQUIRE(c.min.y == (wantHighY ? Approx(0.0).margin(1e-12) : Approx(-1.0)));
    REQUIRE(c.min.z == (wantHighZ ? Approx(0.0).margin(1e-12) : Approx(-1.0)));
  }
}

TEST_CASE("Manually subdivided root counts internals + leaves", "[octree]") {
  HermiteOctree o(BBox::unit());
  HermiteNode* root = o.root();
  root->isLeaf = false;
  root->leaf.reset();
  for (int i = 0; i < 8; ++i) {
    auto child = std::make_unique<HermiteNode>();
    child->bounds = internal::childBounds(root->bounds, i);
    child->depth  = 1;
    child->isLeaf = true;
    root->children[i] = std::move(child);
  }
  REQUIRE(o.nodeCount() == 9); // 1 internal + 8 leaves
  REQUIRE(o.leafCount() == 8);
}
