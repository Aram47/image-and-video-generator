#include "animation/action_library.hpp"

#include <cmath>
#include <stdexcept>

namespace gen {
namespace {

constexpr float kCycleSeconds = 1.0f;
constexpr float kWalkSpeed = 0.45f;

float wrapCycle(float seconds) {
  float wrapped = std::fmod(seconds, kCycleSeconds);
  if (wrapped < 0.0f) {
    wrapped += kCycleSeconds;
  }
  return wrapped;
}

float blend(float from, float to, float weight) { return from + (to - from) * weight; }

Pose blendPose(const Pose& from, const Pose& to, float weight) {
  Pose pose;
  pose.root = {blend(from.root.x, to.root.x, weight), blend(from.root.y, to.root.y, weight),
               blend(from.root.z, to.root.z, weight)};
  pose.spine = blend(from.spine, to.spine, weight);
  pose.shoulderLeft = blend(from.shoulderLeft, to.shoulderLeft, weight);
  pose.elbowLeft = blend(from.elbowLeft, to.elbowLeft, weight);
  pose.shoulderRight = blend(from.shoulderRight, to.shoulderRight, weight);
  pose.elbowRight = blend(from.elbowRight, to.elbowRight, weight);
  pose.hipLeft = blend(from.hipLeft, to.hipLeft, weight);
  pose.kneeLeft = blend(from.kneeLeft, to.kneeLeft, weight);
  pose.hipRight = blend(from.hipRight, to.hipRight, weight);
  pose.kneeRight = blend(from.kneeRight, to.kneeRight, weight);
  return pose;
}

Pose cycle(const Pose& start, const Pose& opposite, float seconds) {
  const float phase = wrapCycle(seconds);
  if (phase < 0.5f) {
    return blendPose(start, opposite, phase / 0.5f);
  }
  return blendPose(opposite, start, (phase - 0.5f) / 0.5f);
}

Pose walked(float seconds) {
  Pose pose = cycle(PoseLibrary::walkContact(), PoseLibrary::walkPassing(), seconds);
  pose.root.x = 0.0f;
  pose.root.y = 0.0f;
  pose.root.z = kWalkSpeed * seconds;
  return pose;
}

}  // namespace

std::string_view toString(ActionId action) {
  switch (action) {
    case ActionId::Walk:
      return "walk";
    case ActionId::Wave:
      return "wave";
    case ActionId::WalkAndWave:
      return "walk_and_wave";
  }
  return "walk";
}

Pose ActionLibrary::poseAt(ActionId action, float seconds) const {
  if (!std::isfinite(seconds)) {
    throw std::invalid_argument("Animation time must be finite.");
  }
  switch (action) {
    case ActionId::Walk:
      return walked(seconds);
    case ActionId::Wave:
      return cycle(PoseLibrary::waveLowered(), PoseLibrary::waveRaised(), seconds);
    case ActionId::WalkAndWave: {
      Pose pose = walked(seconds);
      const Pose wave = cycle(PoseLibrary::waveLowered(), PoseLibrary::waveRaised(), seconds);
      pose.shoulderRight = wave.shoulderRight;
      pose.elbowRight = wave.elbowRight;
      return pose;
    }
  }
  throw std::invalid_argument("Unknown action.");
}

float AnimationController::timeAt(int frameIndex, int fps) const {
  if (fps <= 0 || frameIndex < 0) {
    throw std::invalid_argument("Frame time is not valid.");
  }
  return static_cast<float>(frameIndex) / static_cast<float>(fps);
}

Pose AnimationController::poseAt(ActionId action, int frameIndex, int fps) const {
  return actions_.poseAt(action, timeAt(frameIndex, fps));
}

Pose PoseResolver::forImage(const SceneSpec& spec) const {
  switch (spec.pose.value) {
    case PoseId::Standing:
      return PoseLibrary::stand();
    case PoseId::Sitting:
      return PoseLibrary::sit();
    case PoseId::Walking:
      return actions_.poseAt(spec.waveRequested ? ActionId::WalkAndWave : ActionId::Walk, 0.0f);
    case PoseId::Waving:
      return actions_.poseAt(ActionId::Wave, 0.0f);
  }
  throw std::invalid_argument("Unknown pose.");
}

Pose PoseResolver::forAction(ActionId action, float seconds) const { return actions_.poseAt(action, seconds); }

}  // namespace gen
