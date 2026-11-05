#include "prompt/vocabulary.hpp"

#include <iterator>

namespace gen {
namespace {

struct Synonym {
  std::string_view surface;
  std::string_view canonical;
};

struct Phrase {
  std::string_view first;
  std::string_view second;
  std::string_view canonical;
};

struct Entry {
  std::string_view token;
  TokenMeaning meaning;
};

struct ActionEntry {
  std::string_view name;
  ActionId id;
};

constexpr std::string_view kStopWords[] = {"a", "an", "the", "with", "wearing", "on", "in", "at", "and", "of"};

constexpr std::string_view kForbidden[] = {"wings", "tail",  "horns", "beard",      "mustache", "hat",
                                           "glasses", "wheelchair", "horse", "dog", "car"};

constexpr Synonym kSynonyms[] = {
    {"female", "woman"}, {"lady", "woman"},     {"male", "man"},     {"guy", "man"},
    {"street", "city_street"}, {"dusk", "sunset"}, {"overcoat", "coat"}, {"stands", "standing"},
    {"walk", "walking"}, {"walks", "walking"}, {"wave", "waving"},   {"waves", "waving"},
    {"sits", "sitting"},
};

constexpr Phrase kPhrases[] = {
    {"city", "street", "city_street"},
    {"short", "cut", "short_cut"},
};

constexpr Entry kEntries[] = {
    {"woman", {TokenKind::Gender, GenderPresentation::Woman}},
    {"man", {TokenKind::Gender, GenderPresentation::Man}},
    {"short", {TokenKind::Height, Height::Short}},
    {"average", {TokenKind::Height, Height::Average}},
    {"tall", {TokenKind::Height, Height::Tall}},
    {"brown", {TokenKind::HairColor, HairColor::Brown}},
    {"blonde", {TokenKind::HairColor, HairColor::Blonde}},
    {"red", {TokenKind::AmbiguousColor, HairColor::Red}},
    {"black", {TokenKind::AmbiguousColor, HairColor::Black}},
    {"blue", {TokenKind::PrimaryColor, PrimaryColor::Blue}},
    {"green", {TokenKind::PrimaryColor, PrimaryColor::Green}},
    {"yellow", {TokenKind::PrimaryColor, PrimaryColor::Yellow}},
    {"white", {TokenKind::PrimaryColor, PrimaryColor::White}},
    {"straight", {TokenKind::HairStyle, HairStyle::Straight}},
    {"curly", {TokenKind::HairStyle, HairStyle::Curly}},
    {"short_cut", {TokenKind::HairStyle, HairStyle::ShortCut}},
    {"hair", {TokenKind::HairAnchor, {}}},
    {"coat", {TokenKind::Outerwear, Outerwear::Coat}},
    {"jacket", {TokenKind::Outerwear, Outerwear::Jacket}},
    {"shirt", {TokenKind::Shirt, Shirt::Shirt}},
    {"city_street", {TokenKind::Environment, EnvironmentKind::CityStreet}},
    {"park", {TokenKind::Environment, EnvironmentKind::Park}},
    {"plain", {TokenKind::Environment, EnvironmentKind::Plain}},
    {"day", {TokenKind::TimeOfDay, TimeOfDay::Day}},
    {"sunset", {TokenKind::TimeOfDay, TimeOfDay::Sunset}},
    {"night", {TokenKind::TimeOfDay, TimeOfDay::Night}},
    {"standing", {TokenKind::BodyPose, PoseId::Standing}},
    {"sitting", {TokenKind::BodyPose, PoseId::Sitting}},
    {"walking", {TokenKind::BodyPose, PoseId::Walking}},
    {"waving", {TokenKind::Wave, {}}},
};

constexpr ActionEntry kActions[] = {
    {"walk", ActionId::Walk},
    {"wave", ActionId::Wave},
    {"walk_and_wave", ActionId::WalkAndWave},
};

template <typename EntryT, typename Predicate>
const EntryT* findIf(const EntryT* first, const EntryT* last, Predicate predicate) {
  for (const EntryT* entry = first; entry != last; ++entry) {
    if (predicate(*entry)) {
      return entry;
    }
  }
  return nullptr;
}

}  // namespace

bool Vocabulary::isStopWord(std::string_view token) const {
  return findIf(std::begin(kStopWords), std::end(kStopWords),
                [&](std::string_view stop) { return stop == token; }) != nullptr;
}

bool Vocabulary::isForbidden(std::string_view token) const {
  return findIf(std::begin(kForbidden), std::end(kForbidden),
                [&](std::string_view word) { return word == token; }) != nullptr;
}

std::optional<std::string_view> Vocabulary::phrase(std::string_view first, std::string_view second) const {
  const auto* entry = findIf(std::begin(kPhrases), std::end(kPhrases), [&](const Phrase& phrase) {
    return phrase.first == first && phrase.second == second;
  });
  if (entry == nullptr) {
    return std::nullopt;
  }
  return entry->canonical;
}

std::string_view Vocabulary::canonicalize(std::string_view token) const {
  const auto* entry = findIf(std::begin(kSynonyms), std::end(kSynonyms),
                             [&](const Synonym& synonym) { return synonym.surface == token; });
  if (entry == nullptr) {
    return token;
  }
  return entry->canonical;
}

std::optional<TokenMeaning> Vocabulary::meaning(std::string_view token) const {
  const auto* entry =
      findIf(std::begin(kEntries), std::end(kEntries), [&](const Entry& item) { return item.token == token; });
  if (entry == nullptr) {
    return std::nullopt;
  }
  return entry->meaning;
}

std::optional<ActionId> Vocabulary::action(std::string_view name) const {
  const auto* entry = findIf(std::begin(kActions), std::end(kActions),
                             [&](const ActionEntry& item) { return item.name == name; });
  if (entry == nullptr) {
    return std::nullopt;
  }
  return entry->id;
}

}  // namespace gen
