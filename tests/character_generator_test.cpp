#include "character/body_metrics.hpp"
#include "character/character_generator.hpp"
#include "character/palette.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

namespace {

gen::CharacterSpec heightSpec(gen::Height height) {
  gen::CharacterSpec spec;
  spec.height = {height, true};
  return spec;
}

void expectWithinSpread(float value, float base) {
  EXPECT_LE(std::fabs(value - base), base * 0.05f + 1.0e-5f);
}

}  // namespace

TEST(CharacterGeneratorTest, SameSeedIsIdentical) {
  const gen::CharacterGenerator generator;
  const auto first = generator.generate(gen::CharacterSpec{}, 42);
  const auto second = generator.generate(gen::CharacterSpec{}, 42);
  EXPECT_EQ(first, second);
}

TEST(CharacterGeneratorTest, DrawsShouldersThenArmsThenLegs) {
  const auto body = gen::CharacterGenerator{}.generate(gen::CharacterSpec{}, 42).body;
  EXPECT_FLOAT_EQ(body.shoulderWidth, 0.454228848f);
  EXPECT_FLOAT_EQ(body.armLength, 0.494234025f);
  EXPECT_FLOAT_EQ(body.legLength, 0.815155745f);
  EXPECT_FLOAT_EQ(body.headRadius, gen::BodyMetrics::headRadius);
  EXPECT_FLOAT_EQ(body.torsoLength, gen::BodyMetrics::torsoLength);
}

TEST(CharacterGeneratorTest, ExplicitTallNeverUsesTheShortScale) {
  const gen::CharacterGenerator generator;
  const std::uint32_t seeds[] = {0U, 1U, 42U, 10000U, 4294967295U};
  for (const std::uint32_t seed : seeds) {
    const auto tall = generator.generate(heightSpec(gen::Height::Tall), seed).body;
    const auto shortBody = generator.generate(heightSpec(gen::Height::Short), seed).body;
    EXPECT_FLOAT_EQ(tall.torsoLength, gen::BodyMetrics::torsoLength * 1.15f);
    EXPECT_FLOAT_EQ(tall.headRadius, gen::BodyMetrics::headRadius * 1.15f);
    EXPECT_FLOAT_EQ(shortBody.torsoLength, gen::BodyMetrics::torsoLength * 0.85f);
    EXPECT_GT(tall.torsoLength, shortBody.torsoLength);
    expectWithinSpread(tall.shoulderWidth, gen::BodyMetrics::shoulderWidth * 1.15f);
    expectWithinSpread(tall.armLength, gen::BodyMetrics::armLength * 1.15f);
    expectWithinSpread(tall.legLength, gen::BodyMetrics::legLength * 1.15f);
  }
}

TEST(CharacterGeneratorTest, SeedChangesOnlyImplicitProportionsAndSkin) {
  gen::CharacterSpec spec;
  spec.hair.color = {gen::HairColor::Red, true};
  spec.outfit.outerwear = {gen::Outerwear::Coat, true};
  spec.outfit.primaryColor = {gen::PrimaryColor::Blue, true};

  const gen::CharacterGenerator generator;
  const auto first = generator.generate(spec, 0);
  const auto second = generator.generate(spec, 1);
  const auto sameSkin = generator.generate(spec, 4);

  EXPECT_EQ(first.colors.hair, gen::ColorPalette::hair(gen::HairColor::Red));
  EXPECT_EQ(second.colors.hair, first.colors.hair);
  EXPECT_EQ(first.colors.outerwear, gen::ColorPalette::clothing(gen::PrimaryColor::Blue));
  EXPECT_EQ(second.colors.outerwear, first.colors.outerwear);
  EXPECT_EQ(first.colors.shirt, gen::ColorPalette::undershirt());
  EXPECT_NE(first.colors.skin, second.colors.skin);
  EXPECT_EQ(first.colors.skin, gen::ColorPalette::skin(0));
  EXPECT_EQ(sameSkin.colors.skin, first.colors.skin);
  EXPECT_TRUE(sameSkin.body.shoulderWidth != first.body.shoulderWidth || sameSkin.body.armLength != first.body.armLength ||
              sameSkin.body.legLength != first.body.legLength);
}

TEST(ColorPaletteTest, SkinUsesTheLowBitsAndKeepsTheOriginalTone) {
  EXPECT_EQ(gen::ColorPalette::skin(0), (gen::Color{224, 172, 140, 255}));
  EXPECT_NE(gen::ColorPalette::skin(0), gen::ColorPalette::skin(1));
  EXPECT_NE(gen::ColorPalette::skin(1), gen::ColorPalette::skin(2));
  EXPECT_NE(gen::ColorPalette::skin(2), gen::ColorPalette::skin(3));
  EXPECT_EQ(gen::ColorPalette::skin(4), gen::ColorPalette::skin(0));
}

TEST(CharacterGeneratorTest, ShirtWithoutOuterwearUsesThePrimaryColor) {
  gen::CharacterSpec spec;
  spec.outfit.primaryColor = {gen::PrimaryColor::Yellow, true};
  const auto appearance = gen::CharacterGenerator{}.generate(spec, 9);
  EXPECT_EQ(appearance.colors.shirt, gen::ColorPalette::clothing(gen::PrimaryColor::Yellow));
}
