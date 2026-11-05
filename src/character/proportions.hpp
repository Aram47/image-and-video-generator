#pragma once

#include "prompt/scene_spec.hpp"

#include <stdexcept>

namespace gen {

inline float heightScale(Height height) {
  switch (height) {
    case Height::Short:
      return 0.85f;
    case Height::Average:
      return 1.0f;
    case Height::Tall:
      return 1.15f;
  }
  throw std::invalid_argument("Unknown height.");
}

}  // namespace gen
