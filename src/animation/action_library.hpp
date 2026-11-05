#pragma once

#include "character/pose_library.hpp"
#include "prompt/scene_spec.hpp"

#include <string_view>

namespace gen {

std::string_view toString(ActionId action);

class ActionLibrary {
 public:
  Pose poseAt(ActionId action, float seconds) const;
};

class AnimationController {
 public:
  float timeAt(int frameIndex, int fps) const;
  Pose poseAt(ActionId action, int frameIndex, int fps) const;

 private:
  ActionLibrary actions_;
};

class PoseResolver {
 public:
  Pose forImage(const SceneSpec& spec) const;
  Pose forAction(ActionId action, float seconds) const;

 private:
  ActionLibrary actions_;
};

}  // namespace gen
