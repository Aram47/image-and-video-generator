#include "character/mesh_primitives.hpp"

#include <cstdint>
#include <stdexcept>

namespace gen {
namespace {

void addPart(Mesh& mesh, std::string name, Color color, std::uint32_t firstTriangle, std::uint32_t triangleCount) {
  MeshPart part;
  part.name = std::move(name);
  part.color = color;
  part.firstTriangle = firstTriangle;
  part.triangleCount = triangleCount;
  mesh.parts.push_back(std::move(part));
}

void addTriangle(Mesh& mesh, std::uint32_t origin, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
  Triangle triangle;
  triangle.indices[0] = origin + a;
  triangle.indices[1] = origin + b;
  triangle.indices[2] = origin + c;
  mesh.triangles.push_back(triangle);
}

}  // namespace

void MeshPrimitives::addBox(Mesh& mesh, std::string name, float x0, float y0, float z0, float x1, float y1, float z1,
                            Color color) {
  if (!(x0 < x1) || !(y0 < y1) || !(z0 < z1)) {
    throw std::invalid_argument("Box corners must be ordered.");
  }

  const auto origin = static_cast<std::uint32_t>(mesh.vertices.size());
  const auto firstTriangle = static_cast<std::uint32_t>(mesh.triangles.size());
  mesh.vertices.push_back(Vec3{x0, y0, z0});
  mesh.vertices.push_back(Vec3{x1, y0, z0});
  mesh.vertices.push_back(Vec3{x1, y1, z0});
  mesh.vertices.push_back(Vec3{x0, y1, z0});
  mesh.vertices.push_back(Vec3{x0, y0, z1});
  mesh.vertices.push_back(Vec3{x1, y0, z1});
  mesh.vertices.push_back(Vec3{x1, y1, z1});
  mesh.vertices.push_back(Vec3{x0, y1, z1});

  const std::uint32_t faces[12][3] = {
      {4, 5, 6}, {4, 6, 7}, {0, 3, 2}, {0, 2, 1}, {0, 4, 7}, {0, 7, 3},
      {1, 2, 6}, {1, 6, 5}, {0, 1, 5}, {0, 5, 4}, {3, 7, 6}, {3, 6, 2},
  };
  for (const auto& face : faces) {
    addTriangle(mesh, origin, face[0], face[1], face[2]);
  }
  addPart(mesh, std::move(name), color, firstTriangle, 12);
}

void MeshPrimitives::addOrientedBox(Mesh& mesh, std::string name, const RigidFrame& frame, float x0, float y0, float z0,
                                   float x1, float y1, float z1, Color color) {
  if (!(x0 < x1) || !(y0 < y1) || !(z0 < z1)) {
    throw std::invalid_argument("Box corners must be ordered.");
  }

  const auto origin = static_cast<std::uint32_t>(mesh.vertices.size());
  const auto firstTriangle = static_cast<std::uint32_t>(mesh.triangles.size());
  mesh.vertices.push_back(frame.apply(x0, y0, z0));
  mesh.vertices.push_back(frame.apply(x1, y0, z0));
  mesh.vertices.push_back(frame.apply(x1, y1, z0));
  mesh.vertices.push_back(frame.apply(x0, y1, z0));
  mesh.vertices.push_back(frame.apply(x0, y0, z1));
  mesh.vertices.push_back(frame.apply(x1, y0, z1));
  mesh.vertices.push_back(frame.apply(x1, y1, z1));
  mesh.vertices.push_back(frame.apply(x0, y1, z1));

  const std::uint32_t faces[12][3] = {
      {4, 5, 6}, {4, 6, 7}, {0, 3, 2}, {0, 2, 1}, {0, 4, 7}, {0, 7, 3},
      {1, 2, 6}, {1, 6, 5}, {0, 1, 5}, {0, 5, 4}, {3, 7, 6}, {3, 6, 2},
  };
  for (const auto& face : faces) {
    addTriangle(mesh, origin, face[0], face[1], face[2]);
  }

  if (!mesh.parts.empty() && mesh.parts.back().name == name && mesh.parts.back().color == color &&
      mesh.parts.back().firstTriangle + mesh.parts.back().triangleCount == firstTriangle) {
    mesh.parts.back().triangleCount += 12;
    return;
  }
  addPart(mesh, std::move(name), color, firstTriangle, 12);
}

void MeshPrimitives::addPyramid(Mesh& mesh, std::string name, Vec3 baseCenter, float radius, float height,
                                Color color) {
  if (!(radius > 0.0f) || !(height > 0.0f)) {
    throw std::invalid_argument("Pyramid size must be positive.");
  }

  const auto origin = static_cast<std::uint32_t>(mesh.vertices.size());
  const auto firstTriangle = static_cast<std::uint32_t>(mesh.triangles.size());
  const float x = baseCenter.x;
  const float y = baseCenter.y;
  const float z = baseCenter.z;
  mesh.vertices.push_back(Vec3{x, y + height, z});
  mesh.vertices.push_back(Vec3{x - radius, y, z - radius});
  mesh.vertices.push_back(Vec3{x + radius, y, z - radius});
  mesh.vertices.push_back(Vec3{x + radius, y, z + radius});
  mesh.vertices.push_back(Vec3{x - radius, y, z + radius});

  addTriangle(mesh, origin, 0, 2, 1);
  addTriangle(mesh, origin, 0, 3, 2);
  addTriangle(mesh, origin, 0, 4, 3);
  addTriangle(mesh, origin, 0, 1, 4);
  addTriangle(mesh, origin, 1, 2, 3);
  addTriangle(mesh, origin, 1, 3, 4);
  addPart(mesh, std::move(name), color, firstTriangle, 6);
}

}  // namespace gen
