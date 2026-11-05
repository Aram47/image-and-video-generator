#pragma once

#include "common/geometry.hpp"
#include "prompt/scene_spec.hpp"

namespace gen {

struct Camera {
  Vec3 position{};
  Vec3 target{};
  float verticalFovRadians{0};

  static Camera frameCharacter(Height height);
  static Camera defaultPortrait();
  static Camera follow(const Camera& base, const Vec3& root);
};

}  // namespace gen
