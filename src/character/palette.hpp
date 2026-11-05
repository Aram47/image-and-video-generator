#pragma once

#include "common/geometry.hpp"
#include "prompt/scene_spec.hpp"

#include <cstdint>

namespace gen {

class ColorPalette {
 public:
  ColorPalette() = delete;

  static Color skin(std::uint32_t seed);
  static Color hair(HairColor color);
  static Color clothing(PrimaryColor color);
  static Color undershirt();
};

}  // namespace gen
