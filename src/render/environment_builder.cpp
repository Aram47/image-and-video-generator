#include "render/environment_builder.hpp"

#include "character/mesh_primitives.hpp"

#include <stdexcept>
#include <utility>

namespace gen {
namespace {

constexpr Color kSidewalk{158, 156, 152, 255};
constexpr Color kPlainFloor{176, 172, 164, 255};
constexpr Color kRoad{64, 66, 70, 255};
constexpr Color kBuildingLeft{118, 124, 136, 255};
constexpr Color kBuildingRight{96, 108, 122, 255};
constexpr Color kGrass{78, 138, 70, 255};
constexpr Color kTrunk{92, 64, 40, 255};
constexpr Color kFoliage{42, 112, 56, 255};
constexpr Color kFoliageLight{56, 130, 64, 255};

void addTree(Mesh& mesh, const char* trunkName, const char* crownName, Vec3 base, float trunkHeight, float crownRadius,
             float crownHeight, Color foliage) {
  const float trunkRadius = 0.06f;
  MeshPrimitives::addBox(mesh, trunkName, base.x - trunkRadius, base.y, base.z - trunkRadius, base.x + trunkRadius,
                         base.y + trunkHeight, base.z + trunkRadius, kTrunk);
  MeshPrimitives::addPyramid(mesh, crownName, Vec3{base.x, base.y + trunkHeight, base.z}, crownRadius, crownHeight,
                             foliage);
}

class PlainEnvironment final : public IEnvironmentBuilder {
 public:
  std::vector<Mesh> build() const override {
    Mesh mesh;
    MeshPrimitives::addBox(mesh, "floor", -1.8f, -0.06f, -1.6f, 1.8f, -0.01f, 1.6f, kPlainFloor);
    return {std::move(mesh)};
  }
};

class CityStreetEnvironment final : public IEnvironmentBuilder {
 public:
  std::vector<Mesh> build() const override {
    Mesh mesh;
    MeshPrimitives::addBox(mesh, "sidewalk", -2.0f, -0.06f, -1.8f, 2.0f, -0.01f, 0.45f, kSidewalk);
    MeshPrimitives::addBox(mesh, "road", -2.0f, -0.08f, 0.45f, 2.0f, -0.02f, 2.2f, kRoad);
    MeshPrimitives::addBox(mesh, "building_left", -1.7f, 0.0f, -1.7f, -0.85f, 1.7f, -0.55f, kBuildingLeft);
    MeshPrimitives::addBox(mesh, "building_right", 0.85f, 0.0f, -1.7f, 1.7f, 1.45f, -0.55f, kBuildingRight);
    return {std::move(mesh)};
  }
};

class ParkEnvironment final : public IEnvironmentBuilder {
 public:
  std::vector<Mesh> build() const override {
    Mesh mesh;
    MeshPrimitives::addBox(mesh, "floor", -2.0f, -0.06f, -2.0f, 2.0f, -0.01f, 1.8f, kGrass);
    addTree(mesh, "trunk_left", "crown_left", {-0.95f, 0.0f, -1.35f}, 0.45f, 0.28f, 0.75f, kFoliage);
    addTree(mesh, "trunk_right", "crown_right", {1.05f, 0.0f, -1.25f}, 0.4f, 0.26f, 0.7f, kFoliageLight);
    addTree(mesh, "trunk_back", "crown_back", {0.05f, 0.0f, -1.75f}, 0.55f, 0.32f, 0.85f, kFoliage);
    return {std::move(mesh)};
  }
};

}  // namespace

std::unique_ptr<IEnvironmentBuilder> EnvironmentBuilderFactory::create(EnvironmentKind kind) {
  switch (kind) {
    case EnvironmentKind::Plain:
      return std::make_unique<PlainEnvironment>();
    case EnvironmentKind::CityStreet:
      return std::make_unique<CityStreetEnvironment>();
    case EnvironmentKind::Park:
      return std::make_unique<ParkEnvironment>();
  }
  throw std::invalid_argument("Unknown environment.");
}

}  // namespace gen
