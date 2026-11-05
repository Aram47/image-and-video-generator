#include "prompt/prompt_normalizer.hpp"

namespace gen {
namespace {

std::vector<std::string> split(std::string_view text) {
  std::vector<std::string> tokens;
  std::string current;
  const auto flush = [&] {
    if (!current.empty()) {
      tokens.push_back(std::move(current));
      current.clear();
    }
  };

  for (const unsigned char character : text) {
    if (character >= 'A' && character <= 'Z') {
      current.push_back(static_cast<char>(character - 'A' + 'a'));
      continue;
    }
    if ((character >= 'a' && character <= 'z') || character >= 128) {
      current.push_back(static_cast<char>(character));
      continue;
    }
    flush();
  }
  flush();
  return tokens;
}

}  // namespace

std::vector<std::string> PromptNormalizer::tokenize(std::string_view text, const Vocabulary& vocabulary) const {
  std::vector<std::string> content;
  for (auto& token : split(text)) {
    if (!vocabulary.isStopWord(token)) {
      content.push_back(std::move(token));
    }
  }

  std::vector<std::string> canonical;
  canonical.reserve(content.size());
  for (std::size_t index = 0; index < content.size();) {
    if (index + 1 < content.size()) {
      if (const auto phrase = vocabulary.phrase(content[index], content[index + 1])) {
        canonical.emplace_back(*phrase);
        index += 2;
        continue;
      }
    }
    canonical.emplace_back(vocabulary.canonicalize(content[index]));
    ++index;
  }
  return canonical;
}

}  // namespace gen
