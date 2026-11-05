#pragma once

#include "prompt/scene_spec.hpp"

#include <optional>
#include <string_view>
#include <variant>

namespace gen {

enum class TokenKind {
  Gender,
  Height,
  HairColor,
  PrimaryColor,
  AmbiguousColor,
  HairStyle,
  HairAnchor,
  Outerwear,
  Shirt,
  Environment,
  TimeOfDay,
  BodyPose,
  Wave,
};

struct TokenMeaning {
  TokenKind kind{};
  std::variant<std::monostate, GenderPresentation, Height, HairColor, PrimaryColor, HairStyle, Outerwear, Shirt,
               EnvironmentKind, TimeOfDay, PoseId>
      value{};
};

class Vocabulary {
 public:
  bool isStopWord(std::string_view token) const;
  bool isForbidden(std::string_view token) const;
  std::optional<std::string_view> phrase(std::string_view first, std::string_view second) const;
  std::string_view canonicalize(std::string_view token) const;
  std::optional<TokenMeaning> meaning(std::string_view token) const;
  std::optional<ActionId> action(std::string_view name) const;
};

}  // namespace gen
