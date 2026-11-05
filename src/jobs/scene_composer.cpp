#include "jobs/scene_composer.hpp"

#include "animation/action_library.hpp"
#include "character/hair_mesh_builder.hpp"
#include "character/mesh_builder.hpp"
#include "character/outfit_mesh_builder.hpp"
#include "render/environment_builder.hpp"

#include <utility>

namespace gen {

PreparedScene SceneComposer::prepare(const SceneSpec& spec) const {
  PreparedScene prepared;
  prepared.appearance = CharacterGenerator{}.generate(spec.character, spec.seed);
  prepared.hair = spec.character.hair.style.value;
  prepared.outfit = spec.character.outfit;
  prepared.environment = EnvironmentBuilderFactory::create(spec.environment.kind.value)->build();
  prepared.background = backgroundFor(spec.lighting.timeOfDay.value);
  prepared.light = Light::forTime(spec.lighting.timeOfDay.value);
  prepared.camera = Camera::frameCharacter(spec.character.height.value);
  return prepared;
}

ComposedFrame SceneComposer::compose(const PreparedScene& prepared, const Pose& pose) const {
  ComposedFrame frame;
  frame.scene.meshes.push_back(MeshBuilder{}.build(prepared.appearance.body, pose, prepared.appearance.colors.skin));
  frame.scene.meshes.push_back(
      HairMeshBuilder{}.build(prepared.appearance.body, pose, prepared.hair, prepared.appearance.colors.hair));
  auto clothing = OutfitMeshBuilder{}.build(prepared.appearance.body, pose, prepared.outfit,
                                            prepared.appearance.colors.shirt, prepared.appearance.colors.outerwear);
  for (Mesh& mesh : clothing) {
    frame.scene.meshes.push_back(std::move(mesh));
  }
  for (const Mesh& mesh : prepared.environment) {
    frame.scene.meshes.push_back(mesh);
  }
  frame.scene.background = prepared.background;
  frame.scene.light = prepared.light;
  frame.camera = Camera::follow(prepared.camera, pose.root);
  return frame;
}

ComposedFrame SceneComposer::compose(const SceneSpec& spec) const {
  return compose(prepare(spec), PoseResolver{}.forImage(spec));
}

}  // namespace gen
