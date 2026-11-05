#include "character/hair_mesh_builder.hpp"

#include "character/mesh_primitives.hpp"
#include "character/skeleton.hpp"

#include <stdexcept>

namespace gen {

Mesh HairMeshBuilder::build(const CharacterBuildParams& params, const Pose& pose, HairStyle style, Color color) const {
  const SolvedPose solved = Skeleton{}.solve(params, pose);
  const float radius = solved.headRadius;

  float halfWidth = radius * 1.25f;
  float y0 = 0.0f;
  float y1 = 2.0f * radius + radius * 0.08f;
  float z0 = -radius - radius * 0.45f;
  float z1 = -radius + radius * 0.15f;
  if (style == HairStyle::Curly) {
    halfWidth = radius * 1.75f;
    z0 = -radius - radius * 0.7f;
  } else if (style == HairStyle::ShortCut) {
    halfWidth = radius * 1.15f;
    y0 = radius * 0.75f;
    y1 = 2.0f * radius + radius * 0.12f;
    z0 = -radius - radius * 0.2f;
    z1 = -radius + radius * 0.55f;
  } else if (style != HairStyle::Straight) {
    throw std::invalid_argument("Unknown hair style.");
  }

  Mesh mesh;
  MeshPrimitives::addOrientedBox(mesh, "hair", solved.head, -halfWidth, y0, z0, halfWidth, y1, z1, color);
  return mesh;
}

}  // namespace gen
