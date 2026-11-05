#pragma once

#include "character/mesh.hpp"
#include "character/rigid_frame.hpp"

#include <string>

namespace gen {

class MeshPrimitives {
 public:
  MeshPrimitives() = delete;

  static void addBox(Mesh& mesh, std::string name, float x0, float y0, float z0, float x1, float y1, float z1,
                     Color color);
  static void addOrientedBox(Mesh& mesh, std::string name, const RigidFrame& frame, float x0, float y0, float z0,
                             float x1, float y1, float z1, Color color);
  static void addPyramid(Mesh& mesh, std::string name, Vec3 baseCenter, float radius, float height, Color color);
};

}  // namespace gen
