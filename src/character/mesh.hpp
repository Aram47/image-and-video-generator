#pragma once

#include "common/geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gen {

struct Triangle {
  std::uint32_t indices[3]{};
};

struct MeshPart {
  std::string name;
  Color color{};
  std::uint32_t firstTriangle{0};
  std::uint32_t triangleCount{0};
};

struct Mesh {
  std::vector<Vec3> vertices;
  std::vector<Triangle> triangles;
  std::vector<MeshPart> parts;
};

}  // namespace gen
