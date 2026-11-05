#pragma once

#include "common/geometry.hpp"

#include <cmath>

namespace gen {

struct RigidFrame {
  Vec3 origin{};
  float pitch{0};

  Vec3 apply(float x, float y, float z) const {
    const float cosine = std::cos(pitch);
    const float sine = std::sin(pitch);
    return {origin.x + x, origin.y + (y * cosine - z * sine), origin.z + (y * sine + z * cosine)};
  }
};

}  // namespace gen
