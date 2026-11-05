#pragma once

#include <cstdint>

namespace gen {

struct Vec3 {
  float x{0};
  float y{0};
  float z{0};

  bool operator==(const Vec3&) const = default;
};

struct Color {
  std::uint8_t red{0};
  std::uint8_t green{0};
  std::uint8_t blue{0};
  std::uint8_t alpha{255};

  bool operator==(const Color&) const = default;
};

}  // namespace gen
