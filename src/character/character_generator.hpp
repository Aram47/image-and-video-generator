#pragma once

#include "character/character_build_params.hpp"
#include "common/geometry.hpp"
#include "prompt/scene_spec.hpp"

#include <cstdint>

namespace gen {

struct CharacterColors {
  Color skin{};
  Color hair{};
  Color shirt{};
  Color outerwear{};

  bool operator==(const CharacterColors&) const = default;
};

struct CharacterAppearance {
  CharacterBuildParams body{};
  CharacterColors colors{};

  bool operator==(const CharacterAppearance&) const = default;
};

class CharacterGenerator {
 public:
  CharacterAppearance generate(const CharacterSpec& spec, std::uint32_t seed) const;
};

}  // namespace gen
