#include "prompt/prompt_parser.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

std::variant<gen::SceneSpec, gen::Error> parse(std::string_view prompt,
                                               gen::PromptTarget target = gen::PromptTarget::Image,
                                               std::optional<std::string_view> action = std::nullopt,
                                               std::uint32_t seed = 1) {
  const gen::PromptParser parser;
  return parser.parse(prompt, target, action, seed);
}

std::optional<gen::SceneSpec> maybeSpec(std::string_view prompt, gen::PromptTarget target = gen::PromptTarget::Image,
                                        std::optional<std::string_view> action = std::nullopt,
                                        std::uint32_t seed = 1) {
  auto result = parse(prompt, target, action, seed);
  if (const auto* spec = std::get_if<gen::SceneSpec>(&result)) {
    return *spec;
  }
  if (const auto* error = std::get_if<gen::Error>(&result)) {
    ADD_FAILURE() << prompt << " -> " << gen::toString(error->code) << ": " << error->message;
  }
  return std::nullopt;
}

void expectCamera(const gen::SceneSpec& spec) {
  EXPECT_EQ(spec.camera.distance.value, gen::CameraDistance::Medium);
  EXPECT_FALSE(spec.camera.distance.explicitValue);
  EXPECT_EQ(spec.camera.yaw.value, gen::CameraYaw::Front);
  EXPECT_FALSE(spec.camera.yaw.explicitValue);
}

}  // namespace

TEST(PromptParserTest, ParsesTheImageExampleFromSection5) {
  const auto parsed = maybeSpec("A tall woman with curly red hair wearing a blue coat, standing on a city street at sunset",
                                gen::PromptTarget::Image, std::nullopt, 12345);
  ASSERT_TRUE(parsed.has_value());
  const auto& spec = *parsed;

  EXPECT_EQ(spec.seed, 12345U);
  EXPECT_EQ(spec.character.gender.value, gen::GenderPresentation::Woman);
  EXPECT_TRUE(spec.character.gender.explicitValue);
  EXPECT_EQ(spec.character.height.value, gen::Height::Tall);
  EXPECT_TRUE(spec.character.height.explicitValue);
  EXPECT_EQ(spec.character.hair.color.value, gen::HairColor::Red);
  EXPECT_TRUE(spec.character.hair.color.explicitValue);
  EXPECT_EQ(spec.character.hair.style.value, gen::HairStyle::Curly);
  EXPECT_TRUE(spec.character.hair.style.explicitValue);
  EXPECT_EQ(spec.character.outfit.outerwear.value, gen::Outerwear::Coat);
  EXPECT_TRUE(spec.character.outfit.outerwear.explicitValue);
  EXPECT_EQ(spec.character.outfit.shirt.value, gen::Shirt::Shirt);
  EXPECT_FALSE(spec.character.outfit.shirt.explicitValue);
  EXPECT_EQ(spec.character.outfit.primaryColor.value, gen::PrimaryColor::Blue);
  EXPECT_TRUE(spec.character.outfit.primaryColor.explicitValue);
  EXPECT_EQ(spec.environment.kind.value, gen::EnvironmentKind::CityStreet);
  EXPECT_TRUE(spec.environment.kind.explicitValue);
  EXPECT_EQ(spec.lighting.timeOfDay.value, gen::TimeOfDay::Sunset);
  EXPECT_TRUE(spec.lighting.timeOfDay.explicitValue);
  EXPECT_EQ(spec.pose.value, gen::PoseId::Standing);
  EXPECT_TRUE(spec.pose.explicitValue);
  EXPECT_FALSE(spec.action.has_value());
  EXPECT_TRUE(spec.unsupportedTerms.empty());
  expectCamera(spec);
}

TEST(PromptParserTest, ParsesTheImageExampleFromSection15) {
  const auto parsed = maybeSpec("A short man with black hair wearing a yellow shirt, standing");
  ASSERT_TRUE(parsed.has_value());
  const auto& spec = *parsed;

  EXPECT_EQ(spec.character.gender.value, gen::GenderPresentation::Man);
  EXPECT_TRUE(spec.character.gender.explicitValue);
  EXPECT_EQ(spec.character.height.value, gen::Height::Short);
  EXPECT_TRUE(spec.character.height.explicitValue);
  EXPECT_EQ(spec.character.hair.color.value, gen::HairColor::Black);
  EXPECT_TRUE(spec.character.hair.color.explicitValue);
  EXPECT_EQ(spec.character.hair.style.value, gen::HairStyle::Straight);
  EXPECT_FALSE(spec.character.hair.style.explicitValue);
  EXPECT_EQ(spec.character.outfit.outerwear.value, gen::Outerwear::None);
  EXPECT_FALSE(spec.character.outfit.outerwear.explicitValue);
  EXPECT_EQ(spec.character.outfit.shirt.value, gen::Shirt::Shirt);
  EXPECT_TRUE(spec.character.outfit.shirt.explicitValue);
  EXPECT_EQ(spec.character.outfit.primaryColor.value, gen::PrimaryColor::Yellow);
  EXPECT_TRUE(spec.character.outfit.primaryColor.explicitValue);
  EXPECT_EQ(spec.environment.kind.value, gen::EnvironmentKind::Plain);
  EXPECT_FALSE(spec.environment.kind.explicitValue);
  EXPECT_EQ(spec.pose.value, gen::PoseId::Standing);
  EXPECT_TRUE(spec.pose.explicitValue);
  EXPECT_FALSE(spec.action.has_value());
  EXPECT_TRUE(spec.unsupportedTerms.empty());
}

TEST(PromptParserTest, ParsesTheVideoExampleFromSection4) {
  const auto parsed = maybeSpec("A young man in a green jacket walks through a park and waves at the camera",
                                gen::PromptTarget::Video, std::nullopt, 12345);
  ASSERT_TRUE(parsed.has_value());
  const auto& spec = *parsed;

  EXPECT_EQ(spec.seed, 12345U);
  EXPECT_EQ(spec.character.gender.value, gen::GenderPresentation::Man);
  EXPECT_TRUE(spec.character.gender.explicitValue);
  EXPECT_EQ(spec.character.height.value, gen::Height::Average);
  EXPECT_FALSE(spec.character.height.explicitValue);
  EXPECT_EQ(spec.character.outfit.outerwear.value, gen::Outerwear::Jacket);
  EXPECT_TRUE(spec.character.outfit.outerwear.explicitValue);
  EXPECT_EQ(spec.character.outfit.primaryColor.value, gen::PrimaryColor::Green);
  EXPECT_TRUE(spec.character.outfit.primaryColor.explicitValue);
  EXPECT_EQ(spec.environment.kind.value, gen::EnvironmentKind::Park);
  EXPECT_TRUE(spec.environment.kind.explicitValue);
  EXPECT_EQ(spec.lighting.timeOfDay.value, gen::TimeOfDay::Day);
  EXPECT_FALSE(spec.lighting.timeOfDay.explicitValue);
  EXPECT_EQ(spec.pose.value, gen::PoseId::Walking);
  EXPECT_TRUE(spec.pose.explicitValue);
  ASSERT_TRUE(spec.action.has_value());
  EXPECT_EQ(spec.action->value, gen::ActionId::WalkAndWave);
  EXPECT_TRUE(spec.action->explicitValue);
  EXPECT_EQ(spec.unsupportedTerms, (std::vector<std::string>{"young", "through", "camera"}));
}

TEST(PromptParserTest, HairWordOrderDoesNotChangeColorOrStyle) {
  const auto adjectiveFirst = maybeSpec("red curly hair", gen::PromptTarget::Image, std::nullopt, 5);
  const auto styleFirst = maybeSpec("curly red hair", gen::PromptTarget::Image, std::nullopt, 5);
  ASSERT_TRUE(adjectiveFirst.has_value());
  ASSERT_TRUE(styleFirst.has_value());
  EXPECT_EQ(*adjectiveFirst, *styleFirst);
  EXPECT_EQ(adjectiveFirst->character.hair.color.value, gen::HairColor::Red);
  EXPECT_EQ(adjectiveFirst->character.hair.style.value, gen::HairStyle::Curly);
}

TEST(PromptParserTest, SynonymsResolveToTheSameSpec) {
  const auto spoken = maybeSpec("A lady stands in an overcoat on a street at dusk", gen::PromptTarget::Image,
                                std::nullopt, 4);
  const auto canonical = maybeSpec("A woman standing in a coat on a city street at sunset", gen::PromptTarget::Image,
                                   std::nullopt, 4);
  ASSERT_TRUE(spoken.has_value());
  ASSERT_TRUE(canonical.has_value());
  EXPECT_EQ(*spoken, *canonical);
  EXPECT_EQ(spoken->character.gender.value, gen::GenderPresentation::Woman);
  EXPECT_EQ(spoken->character.outfit.outerwear.value, gen::Outerwear::Coat);
  EXPECT_EQ(spoken->environment.kind.value, gen::EnvironmentKind::CityStreet);
  EXPECT_EQ(spoken->lighting.timeOfDay.value, gen::TimeOfDay::Sunset);
  EXPECT_EQ(spoken->pose.value, gen::PoseId::Standing);
}

TEST(PromptParserTest, CaseAndPunctuationDoNotChangeTheSpec) {
  const auto clean = maybeSpec("A tall woman with curly red hair wearing a blue coat, standing on a city street at sunset",
                               gen::PromptTarget::Image, std::nullopt, 3);
  const auto noisy =
      maybeSpec("A TALL WOMAN, WITH CURLY RED HAIR, WEARING A BLUE COAT, STANDING ON A CITY STREET AT SUNSET.",
                gen::PromptTarget::Image, std::nullopt, 3);
  ASSERT_TRUE(clean.has_value());
  ASSERT_TRUE(noisy.has_value());
  EXPECT_EQ(*clean, *noisy);
}

TEST(PromptParserTest, MissingAttributesUseDefaults) {
  const auto image = maybeSpec("hello", gen::PromptTarget::Image, std::nullopt, 7);
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->seed, 7U);
  EXPECT_EQ(image->unsupportedTerms, (std::vector<std::string>{"hello"}));
  EXPECT_EQ(image->character.gender.value, gen::GenderPresentation::Woman);
  EXPECT_FALSE(image->character.gender.explicitValue);
  EXPECT_EQ(image->character.height.value, gen::Height::Average);
  EXPECT_FALSE(image->character.height.explicitValue);
  EXPECT_EQ(image->character.hair.color.value, gen::HairColor::Brown);
  EXPECT_FALSE(image->character.hair.color.explicitValue);
  EXPECT_EQ(image->character.hair.style.value, gen::HairStyle::Straight);
  EXPECT_FALSE(image->character.hair.style.explicitValue);
  EXPECT_EQ(image->character.outfit.outerwear.value, gen::Outerwear::None);
  EXPECT_FALSE(image->character.outfit.outerwear.explicitValue);
  EXPECT_EQ(image->character.outfit.shirt.value, gen::Shirt::Shirt);
  EXPECT_FALSE(image->character.outfit.shirt.explicitValue);
  EXPECT_EQ(image->character.outfit.primaryColor.value, gen::PrimaryColor::Blue);
  EXPECT_FALSE(image->character.outfit.primaryColor.explicitValue);
  EXPECT_EQ(image->environment.kind.value, gen::EnvironmentKind::Plain);
  EXPECT_FALSE(image->environment.kind.explicitValue);
  EXPECT_EQ(image->lighting.timeOfDay.value, gen::TimeOfDay::Day);
  EXPECT_FALSE(image->lighting.timeOfDay.explicitValue);
  EXPECT_EQ(image->pose.value, gen::PoseId::Standing);
  EXPECT_FALSE(image->pose.explicitValue);
  EXPECT_FALSE(image->action.has_value());
  expectCamera(*image);

  const auto video = maybeSpec("hello", gen::PromptTarget::Video, std::nullopt, 7);
  ASSERT_TRUE(video.has_value());
  ASSERT_TRUE(video->action.has_value());
  EXPECT_EQ(video->action->value, gen::ActionId::Walk);
  EXPECT_FALSE(video->action->explicitValue);
}

TEST(PromptParserTest, ForbiddenFeatureNamesTheWord) {
  const char* words[] = {"wings", "tail",  "horns", "beard", "mustache", "hat",
                         "glasses", "wheelchair", "horse", "dog", "car"};
  for (const char* word : words) {
    const auto result = parse(std::string("a woman with ") + word);
    const auto* error = std::get_if<gen::Error>(&result);
    ASSERT_NE(error, nullptr) << word;
    EXPECT_EQ(error->code, gen::ErrorCode::UnsupportedPromptFeature) << word;
    EXPECT_EQ(error->message, "The requested feature '" + std::string(word) + "' is not supported yet.") << word;
  }
}

TEST(PromptParserTest, UnknownWordIsKeptAndTheSpecIsBuilt) {
  const auto spec = maybeSpec("A tall woman wearing velvet");
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->unsupportedTerms, (std::vector<std::string>{"velvet"}));
  EXPECT_EQ(spec->character.gender.value, gen::GenderPresentation::Woman);
  EXPECT_TRUE(spec->character.gender.explicitValue);
  EXPECT_EQ(spec->character.height.value, gen::Height::Tall);
  EXPECT_TRUE(spec->character.height.explicitValue);
}

TEST(PromptParserTest, ConflictingValuesNameTheField) {
  const auto result = parse("A tall short woman");
  const auto* error = std::get_if<gen::Error>(&result);
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, gen::ErrorCode::InvalidFieldValue);
  EXPECT_EQ(error->message, "Field 'height' has conflicting values.");
}

TEST(PromptParserTest, RepeatingTheSameValueIsNotAConflict) {
  const auto spec = maybeSpec("tall tall woman");
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->character.height.value, gen::Height::Tall);
  EXPECT_TRUE(spec->character.height.explicitValue);
}

TEST(PromptParserTest, UnknownActionIsRejected) {
  const auto result = parse("A man walks", gen::PromptTarget::Video, "fly");
  const auto* error = std::get_if<gen::Error>(&result);
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, gen::ErrorCode::UnsupportedAction);
  EXPECT_EQ(error->message, "Action 'fly' is not supported.");
}

TEST(PromptParserTest, ExplicitActionOverridesThePrompt) {
  const auto spec = maybeSpec("A man walks", gen::PromptTarget::Video, "wave");
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->pose.value, gen::PoseId::Walking);
  EXPECT_TRUE(spec->pose.explicitValue);
  ASSERT_TRUE(spec->action.has_value());
  EXPECT_EQ(spec->action->value, gen::ActionId::Wave);
  EXPECT_TRUE(spec->action->explicitValue);
}

TEST(PromptParserTest, WalkingTextDoesNotFillAnImageAction) {
  const auto spec = maybeSpec("A man walks");
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->pose.value, gen::PoseId::Walking);
  EXPECT_TRUE(spec->pose.explicitValue);
  EXPECT_FALSE(spec->waveRequested);
  EXPECT_FALSE(spec->action.has_value());
}

TEST(PromptParserTest, WalkingAndWavingOnAnImageKeepsBothSignals) {
  const auto spec = maybeSpec("a man walks and waves");
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->pose.value, gen::PoseId::Walking);
  EXPECT_TRUE(spec->waveRequested);
  EXPECT_FALSE(spec->action.has_value());
}

TEST(PromptParserTest, WalkingTextSetsTheVideoAction) {
  const auto spec = maybeSpec("A man walks", gen::PromptTarget::Video);
  ASSERT_TRUE(spec.has_value());
  ASSERT_TRUE(spec->action.has_value());
  EXPECT_EQ(spec->action->value, gen::ActionId::Walk);
  EXPECT_TRUE(spec->action->explicitValue);
}

TEST(PromptParserTest, WavingAloneSetsThePoseAndAction) {
  const auto spec = maybeSpec("a woman waves", gen::PromptTarget::Video);
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->pose.value, gen::PoseId::Waving);
  EXPECT_TRUE(spec->pose.explicitValue);
  ASSERT_TRUE(spec->action.has_value());
  EXPECT_EQ(spec->action->value, gen::ActionId::Wave);
  EXPECT_TRUE(spec->action->explicitValue);
}

TEST(PromptParserTest, RepeatedParseIsIdenticalIncludingUnknownTermOrder) {
  constexpr std::string_view prompt = "velvet zebra velvet, YOUNG";
  const auto first = maybeSpec(prompt, gen::PromptTarget::Image, std::nullopt, 12345);
  const auto second = maybeSpec(prompt, gen::PromptTarget::Image, std::nullopt, 12345);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(*first, *second);
  EXPECT_EQ(first->unsupportedTerms, (std::vector<std::string>{"velvet", "zebra", "young"}));

  auto otherSeed = maybeSpec(prompt, gen::PromptTarget::Image, std::nullopt, 99);
  ASSERT_TRUE(otherSeed.has_value());
  otherSeed->seed = 12345;
  EXPECT_EQ(*first, *otherSeed);
}

TEST(PromptParserTest, ShortCutIsAHairStylePhrase) {
  const auto spec = maybeSpec("short cut hair");
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->character.hair.style.value, gen::HairStyle::ShortCut);
  EXPECT_TRUE(spec->character.hair.style.explicitValue);
  EXPECT_EQ(spec->character.height.value, gen::Height::Average);
  EXPECT_FALSE(spec->character.height.explicitValue);
}

TEST(PromptParserTest, SharedColorsFollowTheNearestAnchor) {
  const auto hair = maybeSpec("black hair");
  const auto coat = maybeSpec("black coat");
  ASSERT_TRUE(hair.has_value());
  ASSERT_TRUE(coat.has_value());
  EXPECT_EQ(hair->character.hair.color.value, gen::HairColor::Black);
  EXPECT_TRUE(hair->character.hair.color.explicitValue);
  EXPECT_FALSE(hair->character.outfit.primaryColor.explicitValue);
  EXPECT_EQ(coat->character.outfit.primaryColor.value, gen::PrimaryColor::Black);
  EXPECT_TRUE(coat->character.outfit.primaryColor.explicitValue);
  EXPECT_EQ(coat->character.outfit.outerwear.value, gen::Outerwear::Coat);
  EXPECT_FALSE(coat->character.hair.color.explicitValue);
}

TEST(PromptParserTest, SittingInflectionSetsThePose) {
  const auto spec = maybeSpec("a man sits");
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->character.gender.value, gen::GenderPresentation::Man);
  EXPECT_EQ(spec->pose.value, gen::PoseId::Sitting);
  EXPECT_TRUE(spec->pose.explicitValue);
}
