#pragma once

#include "common/error.hpp"
#include "prompt/prompt_normalizer.hpp"
#include "prompt/scene_spec.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace gen {

enum class PromptTarget { Image, Video };

class PromptParser {
 public:
  std::variant<SceneSpec, Error> parse(std::string_view prompt, PromptTarget target,
                                       std::optional<std::string_view> action, std::uint32_t seed) const;

 private:
  Vocabulary vocabulary_;
  PromptNormalizer normalizer_;
};

}  // namespace gen
