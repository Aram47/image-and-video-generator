#include "character/skeleton.hpp"

#include "character/body_metrics.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace gen {
namespace {

constexpr float kSitHip = -1.5707963267948966f;

void requirePositive(float value, const char* name) {
  if (!std::isfinite(value) || !(value > 0.0f)) {
    throw std::invalid_argument(std::string(name) + " must be positive.");
  }
}

RigidFrame rotated(Vec3 origin, float pitch) {
  RigidFrame frame;
  frame.origin = origin;
  frame.pitch = pitch;
  return frame;
}

bool isSit(const Pose& pose) {
  return std::fabs(pose.hipLeft - kSitHip) < 1.0e-4f && std::fabs(pose.hipRight - kSitHip) < 1.0e-4f;
}

}  // namespace

SolvedPose Skeleton::solve(const CharacterBuildParams& params, const Pose& pose) const {
  requirePositive(params.height, "height");
  requirePositive(params.shoulderWidth, "shoulderWidth");
  requirePositive(params.torsoLength, "torsoLength");
  requirePositive(params.armLength, "armLength");
  requirePositive(params.legLength, "legLength");
  requirePositive(params.headRadius, "headRadius");

  const float scale = params.torsoLength / BodyMetrics::torsoLength;
  const float pelvisY = isSit(pose) ? params.legLength * 0.5f : params.legLength;
  const Vec3 pelvis{pose.root.x, pelvisY, pose.root.z};
  const float halfShoulder = params.shoulderWidth * 0.5f;
  const float legThickness = BodyMetrics::legThickness * scale;
  const float hipOffset = BodyMetrics::legGap * scale * 0.5f + legThickness * 0.5f;
  const float thighLength = params.legLength * 0.5f;

  SolvedPose solved;
  solved.scale = scale;
  solved.halfShoulder = halfShoulder;
  solved.torsoLength = params.torsoLength;
  solved.halfTorsoDepth = BodyMetrics::torsoDepth * scale * 0.5f;
  solved.armThickness = BodyMetrics::armThickness * scale;
  solved.halfArm = solved.armThickness * 0.5f;
  solved.upperArmLength = params.armLength * 0.5f;
  solved.forearmLength = params.armLength * 0.5f;
  solved.halfLeg = legThickness * 0.5f;
  solved.thighLength = thighLength;
  solved.shinLength = params.legLength * 0.5f;
  solved.headRadius = params.headRadius;
  solved.root = rotated(Vec3{pose.root.x, 0.0f, pose.root.z}, 0.0f);
  solved.spine = rotated(pelvis, pose.spine);
  solved.head = rotated(solved.spine.apply(0.0f, params.torsoLength, 0.0f), pose.spine);

  solved.shoulderLeft =
      rotated(solved.spine.apply(-halfShoulder, params.torsoLength, 0.0f), pose.spine + pose.shoulderLeft);
  solved.elbowLeft =
      rotated(solved.shoulderLeft.apply(0.0f, -solved.upperArmLength, 0.0f), solved.shoulderLeft.pitch + pose.elbowLeft);
  solved.shoulderRight =
      rotated(solved.spine.apply(halfShoulder, params.torsoLength, 0.0f), pose.spine + pose.shoulderRight);
  solved.elbowRight = rotated(solved.shoulderRight.apply(0.0f, -solved.upperArmLength, 0.0f),
                              solved.shoulderRight.pitch + pose.elbowRight);

  solved.hipLeft = rotated(Vec3{pelvis.x - hipOffset, pelvis.y, pelvis.z}, pose.hipLeft);
  solved.kneeLeft =
      rotated(solved.hipLeft.apply(0.0f, -thighLength, 0.0f), solved.hipLeft.pitch + pose.kneeLeft);
  solved.hipRight = rotated(Vec3{pelvis.x + hipOffset, pelvis.y, pelvis.z}, pose.hipRight);
  solved.kneeRight =
      rotated(solved.hipRight.apply(0.0f, -thighLength, 0.0f), solved.hipRight.pitch + pose.kneeRight);
  return solved;
}

}  // namespace gen
