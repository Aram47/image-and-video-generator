#include "render/scene.hpp"

#include <cmath>
#include <stdexcept>

namespace gen {
namespace {

Vec3 normalize(Vec3 value) {
  const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
  if (!(lengthSquared > 0.0f) || !std::isfinite(lengthSquared)) {
    throw std::invalid_argument("Light direction must be non-zero.");
  }
  const float inverse = 1.0f / std::sqrt(lengthSquared);
  return {value.x * inverse, value.y * inverse, value.z * inverse};
}

Light makeLight(Vec3 direction, Color color, float ambient, float diffuse) {
  Light light;
  light.direction = normalize(direction);
  light.color = color;
  light.ambient = ambient;
  light.diffuse = diffuse;
  return light;
}

}  // namespace

Light Light::day() { return makeLight({0.15f, -0.45f, -0.88f}, {255, 255, 255, 255}, 0.45f, 0.55f); }

Light Light::sunset() { return makeLight({0.9f, -0.25f, -0.35f}, {255, 176, 96, 255}, 0.42f, 0.58f); }

Light Light::night() { return makeLight({0.05f, -0.55f, -0.4f}, {210, 220, 235, 255}, 0.55f, 0.35f); }

Light Light::forTime(TimeOfDay time) {
  switch (time) {
    case TimeOfDay::Day:
      return day();
    case TimeOfDay::Sunset:
      return sunset();
    case TimeOfDay::Night:
      return night();
  }
  throw std::invalid_argument("Unknown time of day.");
}

Color backgroundFor(TimeOfDay time) {
  switch (time) {
    case TimeOfDay::Day:
      return {186, 214, 232, 255};
    case TimeOfDay::Sunset:
      return {232, 140, 74, 255};
    case TimeOfDay::Night:
      return {12, 16, 28, 255};
  }
  throw std::invalid_argument("Unknown time of day.");
}

}  // namespace gen
