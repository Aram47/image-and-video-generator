#pragma once

#include "character/character_build_params.hpp"
#include "character/mesh.hpp"
#include "character/pose_library.hpp"
#include "prompt/scene_spec.hpp"

namespace gen {

class HairMeshBuilder {
 public:
  Mesh build(const CharacterBuildParams& params, const Pose& pose, HairStyle style, Color color) const;
};

}  // namespace gen
