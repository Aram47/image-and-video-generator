#pragma once

#include "prompt/vocabulary.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gen {

class PromptNormalizer {
 public:
  std::vector<std::string> tokenize(std::string_view text, const Vocabulary& vocabulary) const;
};

}  // namespace gen
