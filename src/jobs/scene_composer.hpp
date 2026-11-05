#pragma once

#include "character/character_generator.hpp"
#include "character/pose_library.hpp"
#include "prompt/scene_spec.hpp"
#include "render/camera.hpp"
#include "render/scene.hpp"

#include <vector>

namespace gen {

struct ComposedFrame {
  Scene scene;
  Camera camera;
};

struct PreparedScene {
  CharacterAppearance appearance{};
  HairStyle hair{HairStyle::Straight};
  OutfitSpec outfit{};
  std::vector<Mesh> environment;
  Color background{};
  Light light{};
  Camera camera{};
};

class SceneComposer {
 public:
  PreparedScene prepare(const SceneSpec& spec) const;
  ComposedFrame compose(const PreparedScene& prepared, const Pose& pose) const;
  ComposedFrame compose(const SceneSpec& spec) const;
};

}  // namespace gen
