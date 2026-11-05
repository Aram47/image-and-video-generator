#pragma once

#include "character/character_build_params.hpp"
#include "character/mesh.hpp"
#include "character/pose_library.hpp"
#include "prompt/scene_spec.hpp"

#include <vector>

namespace gen {

class OutfitMeshBuilder {
 public:
  std::vector<Mesh> build(const CharacterBuildParams& params, const Pose& pose, const OutfitSpec& outfit,
                          Color shirtColor, Color outerwearColor) const;
};

}  // namespace gen
