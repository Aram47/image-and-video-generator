#include "render/cpu_rasterizer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace gen {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinDepth = 1.0e-4f;

struct Vec3Math {
  static float dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
  }

  static Vec3 cross(const Vec3& left, const Vec3& right) {
    return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
  }

  static Vec3 subtract(const Vec3& left, const Vec3& right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
  }

  static Vec3 scale(const Vec3& value, float factor) { return {value.x * factor, value.y * factor, value.z * factor}; }

  static Vec3 normalize(const Vec3& value) {
    const float lengthSquared = dot(value, value);
    if (!std::isfinite(lengthSquared) || !(lengthSquared > 0.0f)) {
      throw std::runtime_error("Projection failed.");
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {value.x * inverseLength, value.y * inverseLength, value.z * inverseLength};
  }
};

struct CameraBasis {
  Vec3 position{};
  Vec3 forward{};
  Vec3 right{};
  Vec3 up{};
  float tanHalfX{0};
  float tanHalfY{0};
};

struct ProjectedPoint {
  float x{0};
  float y{0};
  float depth{0};
  bool inFront{false};
};

CameraBasis makeBasis(const Camera& camera, int width, int height) {
  if (!std::isfinite(camera.verticalFovRadians) || !(camera.verticalFovRadians > 0.0f) ||
      !(camera.verticalFovRadians < kPi) || width <= 0 || height <= 0) {
    throw std::runtime_error("Projection failed.");
  }

  CameraBasis basis;
  basis.position = camera.position;
  basis.forward = Vec3Math::normalize(Vec3Math::subtract(camera.target, camera.position));
  const Vec3 worldUp{0.0f, 1.0f, 0.0f};
  basis.right = Vec3Math::normalize(Vec3Math::cross(basis.forward, worldUp));
  basis.up = Vec3Math::cross(basis.right, basis.forward);
  basis.tanHalfY = std::tan(camera.verticalFovRadians * 0.5f);
  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  basis.tanHalfX = basis.tanHalfY * aspect;
  if (!(basis.tanHalfX > 0.0f) || !(basis.tanHalfY > 0.0f)) {
    throw std::runtime_error("Projection failed.");
  }
  return basis;
}

ProjectedPoint projectPoint(const Vec3& world, const CameraBasis& camera, int width, int height) {
  const Vec3 offset = Vec3Math::subtract(world, camera.position);
  ProjectedPoint projected;
  projected.depth = Vec3Math::dot(offset, camera.forward);
  projected.inFront = std::isfinite(projected.depth) && projected.depth > kMinDepth;
  if (!projected.inFront) {
    return projected;
  }

  const float cameraX = Vec3Math::dot(offset, camera.right);
  const float cameraY = Vec3Math::dot(offset, camera.up);
  const float ndcX = (cameraX / projected.depth) / camera.tanHalfX;
  const float ndcY = (cameraY / projected.depth) / camera.tanHalfY;
  projected.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width);
  projected.y = (0.5f - ndcY * 0.5f) * static_cast<float>(height);
  if (!std::isfinite(projected.x) || !std::isfinite(projected.y)) {
    throw std::runtime_error("Projection failed.");
  }
  return projected;
}

float edge(float ax, float ay, float bx, float by, float px, float py) {
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

Color shadeColor(Color albedo, const Vec3& normal, const Light& light, const Vec3& toLight) {
  const float incidence = std::max(0.0f, Vec3Math::dot(normal, toLight));
  const float factor = std::min(1.0f, light.ambient + light.diffuse * incidence);
  if (!std::isfinite(factor)) {
    throw std::runtime_error("Projection failed.");
  }
  const auto channel = [factor](std::uint8_t value, std::uint8_t tint) {
    const float scaled = static_cast<float>(value) * (static_cast<float>(tint) / 255.0f) * factor;
    const long rounded = std::lround(scaled);
    return static_cast<std::uint8_t>(std::clamp(rounded, 0L, 255L));
  };
  return {channel(albedo.red, light.color.red), channel(albedo.green, light.color.green),
          channel(albedo.blue, light.color.blue), albedo.alpha};
}

Color colorForTriangle(const Mesh& mesh, std::uint32_t triangleIndex) {
  for (const MeshPart& part : mesh.parts) {
    const std::uint32_t end = part.firstTriangle + part.triangleCount;
    if (triangleIndex >= part.firstTriangle && triangleIndex < end) {
      return part.color;
    }
  }
  throw std::runtime_error("Triangle has no material.");
}

void rasterizeTriangle(const ProjectedPoint& first, const ProjectedPoint& second, const ProjectedPoint& third,
                       Color color, Framebuffer& framebuffer) {
  if (!first.inFront || !second.inFront || !third.inFront) {
    return;
  }

  int minX = static_cast<int>(std::floor(std::min({first.x, second.x, third.x})));
  int minY = static_cast<int>(std::floor(std::min({first.y, second.y, third.y})));
  int maxX = static_cast<int>(std::floor(std::max({first.x, second.x, third.x})));
  int maxY = static_cast<int>(std::floor(std::max({first.y, second.y, third.y})));
  minX = std::max(0, minX);
  minY = std::max(0, minY);
  maxX = std::min(framebuffer.width() - 1, maxX);
  maxY = std::min(framebuffer.height() - 1, maxY);
  if (minX > maxX || minY > maxY) {
    return;
  }

  for (int y = minY; y <= maxY; ++y) {
    const float pixelY = static_cast<float>(y) + 0.5f;
    for (int x = minX; x <= maxX; ++x) {
      const float pixelX = static_cast<float>(x) + 0.5f;
      const float weight0 = edge(second.x, second.y, third.x, third.y, pixelX, pixelY);
      const float weight1 = edge(third.x, third.y, first.x, first.y, pixelX, pixelY);
      const float weight2 = edge(first.x, first.y, second.x, second.y, pixelX, pixelY);
      const float area = weight0 + weight1 + weight2;
      if (!std::isfinite(area) || area == 0.0f) {
        continue;
      }
      const bool inside = area > 0.0f ? (weight0 >= 0.0f && weight1 >= 0.0f && weight2 >= 0.0f)
                                      : (weight0 <= 0.0f && weight1 <= 0.0f && weight2 <= 0.0f);
      if (!inside) {
        continue;
      }
      const float depth = (weight0 * first.depth + weight1 * second.depth + weight2 * third.depth) / area;
      if (!std::isfinite(depth)) {
        throw std::runtime_error("Projection failed.");
      }
      framebuffer.setPixel(x, y, depth, color);
    }
  }
}

bool validIndex(const Mesh& mesh, std::uint32_t index) { return index < mesh.vertices.size(); }

}  // namespace

void CpuRasterizer::render(const Scene& scene, const Camera& camera, Framebuffer& framebuffer) {
  const CameraBasis view = makeBasis(camera, framebuffer.width(), framebuffer.height());
  const Vec3 toLight = Vec3Math::normalize(Vec3Math::scale(scene.light.direction, -1.0f));
  for (const Mesh& mesh : scene.meshes) {
    for (std::uint32_t index = 0; index < mesh.triangles.size(); ++index) {
      const Triangle& triangle = mesh.triangles[index];
      if (!validIndex(mesh, triangle.indices[0]) || !validIndex(mesh, triangle.indices[1]) ||
          !validIndex(mesh, triangle.indices[2])) {
        throw std::runtime_error("Projection failed.");
      }
      const Vec3& a = mesh.vertices[triangle.indices[0]];
      const Vec3& b = mesh.vertices[triangle.indices[1]];
      const Vec3& c = mesh.vertices[triangle.indices[2]];
      const Vec3 normal = Vec3Math::normalize(Vec3Math::cross(Vec3Math::subtract(b, a), Vec3Math::subtract(c, a)));
      const ProjectedPoint first = projectPoint(a, view, framebuffer.width(), framebuffer.height());
      const ProjectedPoint second = projectPoint(b, view, framebuffer.width(), framebuffer.height());
      const ProjectedPoint third = projectPoint(c, view, framebuffer.width(), framebuffer.height());
      rasterizeTriangle(first, second, third, shadeColor(colorForTriangle(mesh, index), normal, scene.light, toLight),
                        framebuffer);
    }
  }
}

}  // namespace gen
