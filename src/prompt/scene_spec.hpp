#pragma once

#include "prompt/explicit_attribute.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gen {

enum class GenderPresentation { Woman, Man };
enum class Height { Short, Average, Tall };
enum class HairColor { Black, Brown, Blonde, Red };
enum class HairStyle { Straight, Curly, ShortCut };
enum class Outerwear { None, Coat, Jacket };
enum class Shirt { None, Shirt };
enum class PrimaryColor { Blue, Green, Yellow, Red, Black, White };
enum class EnvironmentKind { CityStreet, Park, Plain };
enum class TimeOfDay { Day, Sunset, Night };
enum class CameraDistance { Medium };
enum class CameraYaw { Front };
enum class PoseId { Standing, Sitting, Walking, Waving };
enum class ActionId { Walk, Wave, WalkAndWave };

struct HairSpec {
  ExplicitAttribute<HairColor> color{HairColor::Brown, false};
  ExplicitAttribute<HairStyle> style{HairStyle::Straight, false};

  bool operator==(const HairSpec&) const = default;
};

struct OutfitSpec {
  ExplicitAttribute<Outerwear> outerwear{Outerwear::None, false};
  ExplicitAttribute<Shirt> shirt{Shirt::Shirt, false};
  ExplicitAttribute<PrimaryColor> primaryColor{PrimaryColor::Blue, false};

  bool operator==(const OutfitSpec&) const = default;
};

struct CharacterSpec {
  ExplicitAttribute<GenderPresentation> gender{GenderPresentation::Woman, false};
  ExplicitAttribute<Height> height{Height::Average, false};
  HairSpec hair{};
  OutfitSpec outfit{};

  bool operator==(const CharacterSpec&) const = default;
};

struct EnvironmentSpec {
  ExplicitAttribute<EnvironmentKind> kind{EnvironmentKind::Plain, false};

  bool operator==(const EnvironmentSpec&) const = default;
};

struct CameraSpec {
  ExplicitAttribute<CameraDistance> distance{CameraDistance::Medium, false};
  ExplicitAttribute<CameraYaw> yaw{CameraYaw::Front, false};

  bool operator==(const CameraSpec&) const = default;
};

struct LightingSpec {
  ExplicitAttribute<TimeOfDay> timeOfDay{TimeOfDay::Day, false};

  bool operator==(const LightingSpec&) const = default;
};

struct SceneSpec {
  CharacterSpec character{};
  EnvironmentSpec environment{};
  CameraSpec camera{};
  LightingSpec lighting{};
  ExplicitAttribute<PoseId> pose{PoseId::Standing, false};
  bool waveRequested{false};
  std::optional<ExplicitAttribute<ActionId>> action;
  std::vector<std::string> unsupportedTerms;
  std::uint32_t seed{0};

  bool operator==(const SceneSpec&) const = default;
};

}  // namespace gen
