#pragma once

#include "character/mesh.hpp"
#include "prompt/scene_spec.hpp"

#include <memory>
#include <vector>

namespace gen {

class IEnvironmentBuilder {
 public:
  virtual ~IEnvironmentBuilder() = default;

  IEnvironmentBuilder() = default;
  IEnvironmentBuilder(const IEnvironmentBuilder&) = delete;
  IEnvironmentBuilder& operator=(const IEnvironmentBuilder&) = delete;

  virtual std::vector<Mesh> build() const = 0;
};

class EnvironmentBuilderFactory {
 public:
  static std::unique_ptr<IEnvironmentBuilder> create(EnvironmentKind kind);
};

}  // namespace gen
