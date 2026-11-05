#pragma once

namespace gen {

// Average-height body. CharacterGenerator scales it and jitters the implicit limb lengths.
struct BodyMetrics {
  static constexpr float headRadius = 0.16f;
  static constexpr float torsoLength = 0.52f;
  static constexpr float torsoDepth = 0.22f;
  static constexpr float armLength = 0.48f;
  static constexpr float armThickness = 0.10f;
  static constexpr float legLength = 0.78f;
  static constexpr float legThickness = 0.12f;
  static constexpr float legGap = 0.04f;
  static constexpr float shoulderWidth = 0.46f;
  static constexpr float height = legLength + torsoLength + 2.0f * headRadius;
  static constexpr float torsoCenterY = legLength + torsoLength * 0.5f;
};

}  // namespace gen
