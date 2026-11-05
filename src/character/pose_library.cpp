#include "character/pose_library.hpp"

namespace gen {
namespace {

constexpr float kHalfPi = 1.5707963267948966f;

}  // namespace

Pose PoseLibrary::stand() { return {}; }

Pose PoseLibrary::sit() {
  Pose pose;
  pose.hipLeft = -kHalfPi;
  pose.hipRight = -kHalfPi;
  pose.kneeLeft = kHalfPi;
  pose.kneeRight = kHalfPi;
  return pose;
}

Pose PoseLibrary::walkContact() {
  Pose pose;
  pose.hipLeft = -0.55f;
  pose.kneeLeft = 0.25f;
  pose.hipRight = 0.35f;
  pose.kneeRight = 0.95f;
  pose.shoulderLeft = 0.35f;
  pose.shoulderRight = -0.35f;
  return pose;
}

Pose PoseLibrary::walkPassing() {
  Pose pose;
  pose.hipLeft = 0.35f;
  pose.kneeLeft = 0.95f;
  pose.hipRight = -0.55f;
  pose.kneeRight = 0.25f;
  pose.shoulderLeft = -0.35f;
  pose.shoulderRight = 0.35f;
  return pose;
}

Pose PoseLibrary::waveLowered() {
  Pose pose;
  pose.shoulderRight = -0.25f;
  pose.elbowRight = 0.15f;
  return pose;
}

Pose PoseLibrary::waveRaised() {
  Pose pose;
  pose.shoulderRight = -2.2f;
  pose.elbowRight = 1.15f;
  return pose;
}

}  // namespace gen
