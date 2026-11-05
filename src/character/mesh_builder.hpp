#pragma once

#include "character/character_build_params.hpp"
#include "character/mesh.hpp"
#include "character/pose_library.hpp"

namespace gen {

class MeshBuilder {
 public:
  Mesh build(const CharacterBuildParams& params, const Pose& pose, Color skin) const;
};

}  // namespace gen
