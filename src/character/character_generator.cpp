#include "character/character_generator.hpp"

#include "character/body_metrics.hpp"
#include "character/palette.hpp"
#include "character/proportions.hpp"

#include <random>

namespace gen {
namespace {

constexpr float kProportionSpread = 0.05f;
constexpr float kUnitDenominator = 16777216.0f;

float proportionSpread(std::mt19937& rng) {
  const float unit = static_cast<float>(rng() >> 8) / kUnitDenominator;
  return (unit * 2.0f - 1.0f) * kProportionSpread;
}

}  // namespace

CharacterAppearance CharacterGenerator::generate(const CharacterSpec& spec, std::uint32_t seed) const {
  const float scale = heightScale(spec.height.value);

  // Draw order is part of the seed contract. New implicit parameters are appended after leg length.
  std::mt19937 rng{seed};
  const float shoulderSpread = proportionSpread(rng);
  const float armSpread = proportionSpread(rng);
  const float legSpread = proportionSpread(rng);

  CharacterAppearance appearance;
  appearance.body.headRadius = BodyMetrics::headRadius * scale;
  appearance.body.torsoLength = BodyMetrics::torsoLength * scale;
  appearance.body.shoulderWidth = BodyMetrics::shoulderWidth * scale * (1.0f + shoulderSpread);
  appearance.body.armLength = BodyMetrics::armLength * scale * (1.0f + armSpread);
  appearance.body.legLength = BodyMetrics::legLength * scale * (1.0f + legSpread);
  appearance.body.height =
      appearance.body.legLength + appearance.body.torsoLength + 2.0f * appearance.body.headRadius;

  const Color primary = ColorPalette::clothing(spec.outfit.primaryColor.value);
  appearance.colors.skin = ColorPalette::skin(seed);
  appearance.colors.hair = ColorPalette::hair(spec.hair.color.value);
  if (spec.outfit.outerwear.value == Outerwear::None) {
    appearance.colors.shirt = primary;
    appearance.colors.outerwear = primary;
  } else {
    appearance.colors.shirt = ColorPalette::undershirt();
    appearance.colors.outerwear = primary;
  }
  return appearance;
}

}  // namespace gen
