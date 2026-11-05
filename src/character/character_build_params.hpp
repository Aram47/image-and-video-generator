#pragma once

namespace gen {

struct CharacterBuildParams {
  float height{0};
  float shoulderWidth{0};
  float torsoLength{0};
  float armLength{0};
  float legLength{0};
  float headRadius{0};

  bool operator==(const CharacterBuildParams&) const = default;
};

}  // namespace gen
