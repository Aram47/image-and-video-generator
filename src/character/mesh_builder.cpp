#include "character/mesh_builder.hpp"

#include "character/mesh_primitives.hpp"
#include "character/skeleton.hpp"

namespace gen {
namespace {

void addSegment(Mesh& mesh, const char* name, const RigidFrame& frame, float x0, float x1, float length, float halfDepth,
                Color color) {
  MeshPrimitives::addOrientedBox(mesh, name, frame, x0, -length, -halfDepth, x1, 0.0f, halfDepth, color);
}

}  // namespace

Mesh MeshBuilder::build(const CharacterBuildParams& params, const Pose& pose, Color skin) const {
  const SolvedPose solved = Skeleton{}.solve(params, pose);
  const float radius = solved.headRadius;

  Mesh mesh;
  mesh.vertices.reserve(80);
  mesh.triangles.reserve(144);
  mesh.parts.reserve(8);
  MeshPrimitives::addOrientedBox(mesh, "head", solved.head, -radius, 0.0f, -radius, radius, 2.0f * radius, radius, skin);
  MeshPrimitives::addOrientedBox(mesh, "torso", solved.spine, -solved.halfShoulder, 0.0f, -solved.halfTorsoDepth,
                                 solved.halfShoulder, solved.torsoLength, solved.halfTorsoDepth, skin);
  addSegment(mesh, "arm_left", solved.shoulderLeft, -solved.armThickness, 0.0f, solved.upperArmLength, solved.halfArm,
             skin);
  addSegment(mesh, "arm_left", solved.elbowLeft, -solved.armThickness, 0.0f, solved.forearmLength, solved.halfArm, skin);
  addSegment(mesh, "arm_right", solved.shoulderRight, 0.0f, solved.armThickness, solved.upperArmLength, solved.halfArm,
             skin);
  addSegment(mesh, "arm_right", solved.elbowRight, 0.0f, solved.armThickness, solved.forearmLength, solved.halfArm, skin);
  addSegment(mesh, "leg_left_thigh", solved.hipLeft, -solved.halfLeg, solved.halfLeg, solved.thighLength, solved.halfLeg,
             skin);
  addSegment(mesh, "leg_left_shin", solved.kneeLeft, -solved.halfLeg, solved.halfLeg, solved.shinLength, solved.halfLeg,
             skin);
  addSegment(mesh, "leg_right_thigh", solved.hipRight, -solved.halfLeg, solved.halfLeg, solved.thighLength,
             solved.halfLeg, skin);
  addSegment(mesh, "leg_right_shin", solved.kneeRight, -solved.halfLeg, solved.halfLeg, solved.shinLength, solved.halfLeg,
             skin);
  return mesh;
}

}  // namespace gen
