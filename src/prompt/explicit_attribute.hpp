#pragma once

namespace gen {

template <typename T>
struct ExplicitAttribute {
  T value{};
  bool explicitValue{false};

  bool operator==(const ExplicitAttribute&) const = default;
};

}  // namespace gen
