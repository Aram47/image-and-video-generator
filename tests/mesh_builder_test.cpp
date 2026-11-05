#include "character/body_metrics.hpp"
#include "character/character_generator.hpp"
#include "character/hair_mesh_builder.hpp"
#include "character/mesh_builder.hpp"
#include "character/outfit_mesh_builder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace {

gen::CharacterAppearance appearanceFor(std::uint32_t seed) {
  return gen::CharacterGenerator{}.generate(gen::CharacterSpec{}, seed);
}

const gen::MeshPart* findPart(const gen::Mesh& mesh, const std::string& name) {
  for (const gen::MeshPart& part : mesh.parts) {
    if (part.name == name) {
      return &part;
    }
  }
  return nullptr;
}

float maxAbsX(const gen::Mesh& mesh) {
  float value = 0.0f;
  for (const gen::Vec3& vertex : mesh.vertices) {
    value = std::max(value, std::fabs(vertex.x));
  }
  return value;
}

float minY(const gen::Mesh& mesh) {
  float value = mesh.vertices.at(0).y;
  for (const gen::Vec3& vertex : mesh.vertices) {
    value = std::min(value, vertex.y);
  }
  return value;
}

}  // namespace

TEST(MeshBuilderTest, SameSeedBuildsIdenticalVertices) {
  gen::MeshBuilder builder;
  const auto pose = gen::PoseLibrary::stand();
  const auto firstAppearance = appearanceFor(42);
  const auto secondAppearance = appearanceFor(42);
  const auto first = builder.build(firstAppearance.body, pose, firstAppearance.colors.skin);
  const auto second = builder.build(secondAppearance.body, pose, secondAppearance.colors.skin);

  ASSERT_EQ(first.parts.size(), 8U);
  EXPECT_EQ(first.parts[0].name, "head");
  EXPECT_EQ(first.parts[1].name, "torso");
  EXPECT_EQ(first.parts[2].name, "arm_left");
  EXPECT_EQ(first.parts[3].name, "arm_right");
  EXPECT_EQ(first.parts[4].name, "leg_left_thigh");
  EXPECT_EQ(first.parts[5].name, "leg_left_shin");
  EXPECT_EQ(first.parts[6].name, "leg_right_thigh");
  EXPECT_EQ(first.parts[7].name, "leg_right_shin");
  EXPECT_EQ(first.parts[0].color, firstAppearance.colors.skin);
  EXPECT_EQ(first.vertices, second.vertices);
  EXPECT_EQ(first.triangles.size(), second.triangles.size());
}

TEST(MeshBuilderTest, DifferentSeedsStayWithinTheLimbTolerance) {
  const auto narrow = appearanceFor(0).body;
  const auto wide = appearanceFor(10000).body;
  EXPECT_FLOAT_EQ(narrow.torsoLength, gen::BodyMetrics::torsoLength);
  EXPECT_FLOAT_EQ(narrow.headRadius, gen::BodyMetrics::headRadius);
  EXPECT_FLOAT_EQ(wide.torsoLength, narrow.torsoLength);
  EXPECT_FLOAT_EQ(wide.headRadius, narrow.headRadius);
  EXPECT_LE(std::fabs(narrow.shoulderWidth - gen::BodyMetrics::shoulderWidth),
            gen::BodyMetrics::shoulderWidth * 0.05f + 1.0e-4f);
  EXPECT_LE(std::fabs(wide.armLength - gen::BodyMetrics::armLength), gen::BodyMetrics::armLength * 0.05f + 1.0e-4f);
  EXPECT_LE(std::fabs(wide.legLength - gen::BodyMetrics::legLength), gen::BodyMetrics::legLength * 0.05f + 1.0e-4f);
  EXPECT_TRUE(narrow.shoulderWidth != wide.shoulderWidth || narrow.armLength != wide.armLength ||
              narrow.legLength != wide.legLength);
}

float minZ(const gen::Mesh& mesh, const gen::MeshPart& part) {
  float value = mesh.vertices[mesh.triangles[part.firstTriangle].indices[0]].z;
  for (std::uint32_t index = 0; index < part.triangleCount; ++index) {
    const gen::Triangle& triangle = mesh.triangles[part.firstTriangle + index];
    for (std::uint32_t corner : triangle.indices) {
      value = std::min(value, mesh.vertices[corner].z);
    }
  }
  return value;
}

TEST(MeshBuilderTest, BendingTheElbowMovesTheForearm) {
  const auto appearance = appearanceFor(1);
  gen::MeshBuilder builder;
  const auto standing = builder.build(appearance.body, gen::PoseLibrary::stand(), appearance.colors.skin);
  gen::Pose bent = gen::PoseLibrary::stand();
  bent.elbowRight = 1.0f;
  const auto posed = builder.build(appearance.body, bent, appearance.colors.skin);
  const gen::MeshPart* standingArm = findPart(standing, "arm_right");
  const gen::MeshPart* bentArm = findPart(posed, "arm_right");
  ASSERT_NE(standingArm, nullptr);
  ASSERT_NE(bentArm, nullptr);
  EXPECT_LT(minZ(posed, *bentArm), minZ(standing, *standingArm));
}

TEST(MeshBuilderTest, SittingMovesTheShinsForwardOfTheStandingLegs) {
  const auto appearance = appearanceFor(7);
  gen::MeshBuilder builder;
  const auto standing = builder.build(appearance.body, gen::PoseLibrary::stand(), appearance.colors.skin);
  const auto sitting = builder.build(appearance.body, gen::PoseLibrary::sit(), appearance.colors.skin);
  const gen::MeshPart* standingShin = findPart(standing, "leg_left_shin");
  const gen::MeshPart* sittingShin = findPart(sitting, "leg_left_shin");
  ASSERT_NE(standingShin, nullptr);
  ASSERT_NE(sittingShin, nullptr);

  float standingMaxZ = standing.vertices[standing.triangles[standingShin->firstTriangle].indices[0]].z;
  float sittingMaxZ = sitting.vertices[sitting.triangles[sittingShin->firstTriangle].indices[0]].z;
  for (std::uint32_t index = 0; index < standingShin->triangleCount; ++index) {
    const gen::Triangle& triangle = standing.triangles[standingShin->firstTriangle + index];
    for (std::uint32_t corner : triangle.indices) {
      standingMaxZ = std::max(standingMaxZ, standing.vertices[corner].z);
    }
  }
  for (std::uint32_t index = 0; index < sittingShin->triangleCount; ++index) {
    const gen::Triangle& triangle = sitting.triangles[sittingShin->firstTriangle + index];
    for (std::uint32_t corner : triangle.indices) {
      sittingMaxZ = std::max(sittingMaxZ, sitting.vertices[corner].z);
    }
  }
  EXPECT_GT(sittingMaxZ, standingMaxZ);
}

TEST(MeshBuilderTest, HairIsASeparateMesh) {
  const auto appearance = appearanceFor(3);
  const auto pose = gen::PoseLibrary::stand();
  const gen::Mesh body = gen::MeshBuilder{}.build(appearance.body, pose, appearance.colors.skin);
  const auto bodyVertices = body.vertices;
  const gen::Mesh straight =
      gen::HairMeshBuilder{}.build(appearance.body, pose, gen::HairStyle::Straight, appearance.colors.hair);
  const gen::Mesh curly =
      gen::HairMeshBuilder{}.build(appearance.body, pose, gen::HairStyle::Curly, appearance.colors.hair);
  EXPECT_EQ(body.vertices, bodyVertices);
  ASSERT_EQ(straight.parts.size(), 1U);
  EXPECT_EQ(straight.parts[0].name, "hair");
  EXPECT_EQ(straight.parts[0].color, appearance.colors.hair);
  EXPECT_GT(maxAbsX(curly), maxAbsX(straight));
}

TEST(OutfitMeshBuilderTest, CoatAddsGeometryBelowTheShirt) {
  const auto appearance = appearanceFor(5);
  const auto pose = gen::PoseLibrary::stand();
  gen::OutfitSpec shirtOnly;
  shirtOnly.outerwear.value = gen::Outerwear::None;
  const auto shirt = gen::OutfitMeshBuilder{}.build(appearance.body, pose, shirtOnly, appearance.colors.shirt,
                                                    appearance.colors.outerwear);
  gen::OutfitSpec coated = shirtOnly;
  coated.outerwear = {gen::Outerwear::Coat, true};
  const auto coat = gen::OutfitMeshBuilder{}.build(appearance.body, pose, coated, appearance.colors.shirt,
                                                   appearance.colors.outerwear);

  ASSERT_EQ(shirt.size(), 1U);
  ASSERT_EQ(coat.size(), 2U);
  EXPECT_EQ(shirt[0].parts.at(0).name, "shirt");
  EXPECT_EQ(coat[1].parts.at(0).name, "coat");
  EXPECT_LT(minY(coat[1]), minY(shirt[0]));
  EXPECT_EQ(coat[1].parts[0].color, appearance.colors.outerwear);
}
