#include "character/outfit_mesh_builder.hpp"

#include "character/mesh_primitives.hpp"
#include "character/skeleton.hpp"

#include <stdexcept>

namespace gen {

std::vector<Mesh> OutfitMeshBuilder::build(const CharacterBuildParams& params, const Pose& pose, const OutfitSpec& outfit,
                                           Color shirtColor, Color outerwearColor) const {
  const SolvedPose solved = Skeleton{}.solve(params, pose);
  std::vector<Mesh> meshes;
  if (outfit.shirt.value == Shirt::Shirt) {
    const float pad = 0.02f * solved.scale;
    Mesh shirt;
    MeshPrimitives::addOrientedBox(shirt, "shirt", solved.spine, -solved.halfShoulder - pad, 0.0f,
                                   -solved.halfTorsoDepth - pad, solved.halfShoulder + pad, solved.torsoLength,
                                   solved.halfTorsoDepth + pad, shirtColor);
    meshes.push_back(std::move(shirt));
  } else if (outfit.shirt.value != Shirt::None) {
    throw std::invalid_argument("Unknown shirt.");
  }

  if (outfit.outerwear.value == Outerwear::None) {
    return meshes;
  }

  const bool coat = outfit.outerwear.value == Outerwear::Coat;
  if (!coat && outfit.outerwear.value != Outerwear::Jacket) {
    throw std::invalid_argument("Unknown outerwear.");
  }
  const float pad = (coat ? 0.05f : 0.04f) * solved.scale;
  const float drop = (coat ? 0.22f : 0.06f) * solved.scale;
  Mesh layer;
  MeshPrimitives::addOrientedBox(layer, coat ? "coat" : "jacket", solved.spine, -solved.halfShoulder - pad, -drop,
                                 -solved.halfTorsoDepth - pad, solved.halfShoulder + pad, solved.torsoLength,
                                 solved.halfTorsoDepth + pad, outerwearColor);
  meshes.push_back(std::move(layer));
  return meshes;
}

}  // namespace gen
