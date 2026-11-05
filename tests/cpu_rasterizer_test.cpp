#include "character/mesh.hpp"
#include "character/palette.hpp"
#include "render/cpu_rasterizer.hpp"
#include "render/framebuffer.hpp"
#include "render/scene.hpp"

#include <gtest/gtest.h>

namespace {

gen::Camera testCamera() {
  gen::Camera camera;
  camera.position = gen::Vec3{0.0f, 0.0f, 2.0f};
  camera.target = gen::Vec3{0.0f, 0.0f, 0.0f};
  camera.verticalFovRadians = 1.57079632679f;
  return camera;
}

gen::Mesh triangleMesh(gen::Color color, float z, float scale) {
  gen::Mesh mesh;
  mesh.vertices.push_back(gen::Vec3{-1.2f * scale, -1.2f * scale, z});
  mesh.vertices.push_back(gen::Vec3{1.2f * scale, -1.2f * scale, z});
  mesh.vertices.push_back(gen::Vec3{0.0f, 1.2f * scale, z});
  gen::Triangle triangle;
  triangle.indices[0] = 0;
  triangle.indices[1] = 1;
  triangle.indices[2] = 2;
  mesh.triangles.push_back(triangle);
  gen::MeshPart part;
  part.name = "triangle";
  part.color = color;
  part.firstTriangle = 0;
  part.triangleCount = 1;
  mesh.parts.push_back(part);
  return mesh;
}

gen::Light fullyLitFront() {
  gen::Light light;
  light.direction = {0.0f, 0.0f, -1.0f};
  light.color = {255, 255, 255, 255};
  light.ambient = 0.25f;
  light.diffuse = 0.75f;
  return light;
}

int luminance(gen::Color color) { return color.red + color.green + color.blue; }

gen::Color renderPixel(const gen::Scene& scene, int x, int y) {
  gen::Framebuffer framebuffer(8, 8);
  framebuffer.clear(gen::Color{0, 0, 0, 255});
  gen::CpuRasterizer rasterizer;
  rasterizer.render(scene, testCamera(), framebuffer);
  return framebuffer.pixel(x, y);
}

}  // namespace

TEST(CpuRasterizerTest, TriangleCoversTheExpectedPixelAndLeavesTheCorner) {
  gen::Scene scene;
  scene.light = fullyLitFront();
  scene.meshes.push_back(triangleMesh(gen::Color{255, 0, 0, 255}, 0.0f, 1.0f));

  const gen::Color covered = renderPixel(scene, 4, 4);
  const gen::Color corner = renderPixel(scene, 0, 0);

  EXPECT_EQ(covered, (gen::Color{255, 0, 0, 255}));
  EXPECT_EQ(corner, (gen::Color{0, 0, 0, 255}));
}

TEST(CpuRasterizerTest, LightFromBehindDarkensTheFrontFace) {
  gen::Scene scene;
  scene.light = fullyLitFront();
  scene.light.direction = {0.0f, 0.0f, 1.0f};
  scene.light.ambient = 0.0f;
  scene.light.diffuse = 1.0f;
  scene.meshes.push_back(triangleMesh(gen::Color{255, 0, 0, 255}, 0.0f, 1.0f));

  EXPECT_EQ(renderPixel(scene, 4, 4), (gen::Color{0, 0, 0, 255}));
}

TEST(CpuRasterizerTest, NightKeepsSkinBrighterThanTheBackground) {
  gen::Scene scene;
  scene.light = gen::Light::night();
  scene.meshes.push_back(triangleMesh(gen::ColorPalette::skin(0), 0.0f, 1.0f));

  EXPECT_GT(luminance(renderPixel(scene, 4, 4)), luminance(gen::backgroundFor(gen::TimeOfDay::Night)));
}

TEST(CpuRasterizerTest, NearerTriangleWinsTheSharedPixel) {
  gen::Scene scene;
  scene.light = fullyLitFront();
  scene.meshes.push_back(triangleMesh(gen::Color{255, 0, 0, 255}, 0.0f, 1.0f));
  scene.meshes.push_back(triangleMesh(gen::Color{0, 0, 255, 255}, -2.0f, 2.0f));

  EXPECT_EQ(renderPixel(scene, 4, 4), (gen::Color{255, 0, 0, 255}));
}
