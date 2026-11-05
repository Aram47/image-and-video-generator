#pragma once

#include "character/character_build_params.hpp"
#include "character/pose_library.hpp"
#include "character/rigid_frame.hpp"

namespace gen {

struct SolvedPose {
  RigidFrame root{};
  RigidFrame spine{};
  RigidFrame head{};
  RigidFrame shoulderLeft{};
  RigidFrame elbowLeft{};
  RigidFrame shoulderRight{};
  RigidFrame elbowRight{};
  RigidFrame hipLeft{};
  RigidFrame kneeLeft{};
  RigidFrame hipRight{};
  RigidFrame kneeRight{};

  float scale{1};
  float halfShoulder{0};
  float torsoLength{0};
  float halfTorsoDepth{0};
  float armThickness{0};
  float halfArm{0};
  float upperArmLength{0};
  float forearmLength{0};
  float halfLeg{0};
  float thighLength{0};
  float shinLength{0};
  float headRadius{0};
};

class Skeleton {
 public:
  SolvedPose solve(const CharacterBuildParams& params, const Pose& pose) const;
};

}  // namespace gen
