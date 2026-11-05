#pragma once

#include "character/mesh.hpp"
#include "common/geometry.hpp"
#include "prompt/scene_spec.hpp"

#include <vector>

namespace gen {

struct Light {
  Vec3 direction{0.0f, 0.0f, -1.0f};
  Color color{255, 255, 255, 255};
  float ambient{1.0f};
  float diffuse{0.0f};

  static Light day();
  static Light sunset();
  static Light night();
  static Light forTime(TimeOfDay time);
};

Color backgroundFor(TimeOfDay time);

inline Color dayBackground() { return backgroundFor(TimeOfDay::Day); }

struct Scene {
  std::vector<Mesh> meshes;
  Color background{dayBackground()};
  Light light{Light::day()};
};

}  // namespace gen
