#pragma once

#include "dualc/types.h"

namespace dualc {
namespace internal {

class MeshBVH;

class SignOracle {
public:
  SignOracle(const MeshBVH& bvh, SignMethod method);

  bool isInside(const Vector3& p) const;

private:
  const MeshBVH& bvh_;
  SignMethod     method_;
};

} // namespace internal
} // namespace dualc
