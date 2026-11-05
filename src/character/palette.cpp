#include "character/palette.hpp"

#include <stdexcept>

namespace gen {
namespace {

constexpr Color kSkin[] = {
    {224, 172, 140, 255},
    {198, 146, 114, 255},
    {154, 108, 80, 255},
    {96, 66, 50, 255},
};

}  // namespace

Color ColorPalette::skin(std::uint32_t seed) { return kSkin[seed & 3U]; }

Color ColorPalette::hair(HairColor color) {
  switch (color) {
    case HairColor::Black:
      return {30, 30, 32, 255};
    case HairColor::Brown:
      return {92, 58, 34, 255};
    case HairColor::Blonde:
      return {214, 176, 92, 255};
    case HairColor::Red:
      return {166, 54, 32, 255};
  }
  throw std::invalid_argument("Unknown hair color.");
}

Color ColorPalette::clothing(PrimaryColor color) {
  switch (color) {
    case PrimaryColor::Blue:
      return {40, 78, 168, 255};
    case PrimaryColor::Green:
      return {46, 130, 72, 255};
    case PrimaryColor::Yellow:
      return {214, 176, 42, 255};
    case PrimaryColor::Red:
      return {168, 48, 48, 255};
    case PrimaryColor::Black:
      return {32, 32, 36, 255};
    case PrimaryColor::White:
      return {236, 236, 232, 255};
  }
  throw std::invalid_argument("Unknown clothing color.");
}

Color ColorPalette::undershirt() { return {214, 210, 204, 255}; }

}  // namespace gen
