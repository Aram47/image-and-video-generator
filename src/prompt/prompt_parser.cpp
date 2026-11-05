#include "prompt/prompt_parser.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace gen {
namespace {

Error conflicting(std::string_view field) {
  return Error{ErrorCode::InvalidFieldValue, "Field '" + std::string(field) + "' has conflicting values."};
}

Error unsupportedFeature(std::string_view word) {
  return Error{ErrorCode::UnsupportedPromptFeature,
               "The requested feature '" + std::string(word) + "' is not supported yet."};
}

Error unsupportedAction(std::string_view action) {
  return Error{ErrorCode::UnsupportedAction, "Action '" + std::string(action) + "' is not supported."};
}

template <typename T>
std::optional<Error> assign(ExplicitAttribute<T>& attribute, T value, std::string_view field) {
  if (attribute.explicitValue && attribute.value != value) {
    return conflicting(field);
  }
  attribute.value = value;
  attribute.explicitValue = true;
  return std::nullopt;
}

void appendTerm(std::vector<std::string>& terms, const std::string& token) {
  if (std::find(terms.begin(), terms.end(), token) == terms.end()) {
    terms.push_back(token);
  }
}

bool isHairAnchor(const TokenMeaning& meaning) {
  return meaning.kind == TokenKind::HairAnchor || meaning.kind == TokenKind::HairStyle;
}

bool isClothingAnchor(const TokenMeaning& meaning) {
  return meaning.kind == TokenKind::Outerwear || meaning.kind == TokenKind::Shirt;
}

bool colorBindsToHair(const std::vector<std::string>& tokens, std::size_t index, const Vocabulary& vocabulary) {
  constexpr int kNone = std::numeric_limits<int>::max();
  int hairDistance = kNone;
  int clothingDistance = kNone;
  const auto count = static_cast<int>(tokens.size());
  const auto origin = static_cast<int>(index);

  for (int distance = 1; distance < count; ++distance) {
    const auto consider = [&](int candidate) {
      if (candidate < 0 || candidate >= count) {
        return;
      }
      const auto meaning = vocabulary.meaning(tokens[static_cast<std::size_t>(candidate)]);
      if (!meaning) {
        return;
      }
      if (isHairAnchor(*meaning) && hairDistance == kNone) {
        hairDistance = distance;
      }
      if (isClothingAnchor(*meaning) && clothingDistance == kNone) {
        clothingDistance = distance;
      }
    };
    consider(origin - distance);
    consider(origin + distance);
    if (hairDistance != kNone || clothingDistance != kNone) {
      break;
    }
  }

  if (hairDistance == kNone && clothingDistance == kNone) {
    return false;
  }
  if (hairDistance < clothingDistance) {
    return true;
  }
  if (clothingDistance < hairDistance) {
    return false;
  }

  const int right = origin + hairDistance;
  if (right >= 0 && right < count) {
    const auto meaning = vocabulary.meaning(tokens[static_cast<std::size_t>(right)]);
    if (meaning && isHairAnchor(*meaning)) {
      return true;
    }
  }
  return false;
}

PrimaryColor primaryOf(HairColor color) {
  return color == HairColor::Black ? PrimaryColor::Black : PrimaryColor::Red;
}

std::optional<Error> applyToken(const TokenMeaning& meaning, const std::vector<std::string>& tokens, std::size_t index,
                                const Vocabulary& vocabulary, SceneSpec& spec, bool& sawWalk, bool& sawWave) {
  switch (meaning.kind) {
    case TokenKind::Gender:
      return assign(spec.character.gender, std::get<GenderPresentation>(meaning.value), "gender_presentation");
    case TokenKind::Height:
      return assign(spec.character.height, std::get<Height>(meaning.value), "height");
    case TokenKind::HairColor:
      return assign(spec.character.hair.color, std::get<HairColor>(meaning.value), "hair_color");
    case TokenKind::PrimaryColor:
      return assign(spec.character.outfit.primaryColor, std::get<PrimaryColor>(meaning.value), "primary_color");
    case TokenKind::AmbiguousColor: {
      const auto color = std::get<HairColor>(meaning.value);
      if (colorBindsToHair(tokens, index, vocabulary)) {
        return assign(spec.character.hair.color, color, "hair_color");
      }
      return assign(spec.character.outfit.primaryColor, primaryOf(color), "primary_color");
    }
    case TokenKind::HairStyle:
      return assign(spec.character.hair.style, std::get<HairStyle>(meaning.value), "hair_style");
    case TokenKind::HairAnchor:
      return std::nullopt;
    case TokenKind::Outerwear:
      return assign(spec.character.outfit.outerwear, std::get<Outerwear>(meaning.value), "outerwear");
    case TokenKind::Shirt:
      return assign(spec.character.outfit.shirt, std::get<Shirt>(meaning.value), "shirt");
    case TokenKind::Environment:
      return assign(spec.environment.kind, std::get<EnvironmentKind>(meaning.value), "environment");
    case TokenKind::TimeOfDay:
      return assign(spec.lighting.timeOfDay, std::get<TimeOfDay>(meaning.value), "time_of_day");
    case TokenKind::BodyPose: {
      const auto pose = std::get<PoseId>(meaning.value);
      if (const auto error = assign(spec.pose, pose, "pose")) {
        return error;
      }
      if (pose == PoseId::Walking) {
        sawWalk = true;
      }
      return std::nullopt;
    }
    case TokenKind::Wave:
      sawWave = true;
      spec.waveRequested = true;
      return std::nullopt;
  }
  return std::nullopt;
}

std::optional<Error> applyAction(SceneSpec& spec, PromptTarget target, std::optional<std::string_view> action,
                                 bool sawWalk, bool sawWave, const Vocabulary& vocabulary) {
  if (target != PromptTarget::Video) {
    return std::nullopt;
  }
  if (action && !action->empty()) {
    const auto resolved = vocabulary.action(*action);
    if (!resolved) {
      return unsupportedAction(*action);
    }
    spec.action = ExplicitAttribute<ActionId>{*resolved, true};
    return std::nullopt;
  }
  if (sawWalk && sawWave) {
    spec.action = ExplicitAttribute<ActionId>{ActionId::WalkAndWave, true};
  } else if (sawWalk) {
    spec.action = ExplicitAttribute<ActionId>{ActionId::Walk, true};
  } else if (sawWave) {
    spec.action = ExplicitAttribute<ActionId>{ActionId::Wave, true};
  } else {
    spec.action = ExplicitAttribute<ActionId>{ActionId::Walk, false};
  }
  return std::nullopt;
}

}  // namespace

std::variant<SceneSpec, Error> PromptParser::parse(std::string_view prompt, PromptTarget target,
                                                   std::optional<std::string_view> action, std::uint32_t seed) const {
  const auto tokens = normalizer_.tokenize(prompt, vocabulary_);
  SceneSpec spec;
  spec.seed = seed;
  bool sawWalk = false;
  bool sawWave = false;

  for (std::size_t index = 0; index < tokens.size(); ++index) {
    const auto& token = tokens[index];
    if (vocabulary_.isForbidden(token)) {
      return unsupportedFeature(token);
    }
    const auto meaning = vocabulary_.meaning(token);
    if (!meaning) {
      appendTerm(spec.unsupportedTerms, token);
      continue;
    }
    if (const auto error = applyToken(*meaning, tokens, index, vocabulary_, spec, sawWalk, sawWave)) {
      return *error;
    }
  }

  if (!spec.pose.explicitValue && sawWave) {
    spec.pose.value = PoseId::Waving;
    spec.pose.explicitValue = true;
  }

  if (const auto error = applyAction(spec, target, action, sawWalk, sawWave, vocabulary_)) {
    return *error;
  }
  return spec;
}

}  // namespace gen
