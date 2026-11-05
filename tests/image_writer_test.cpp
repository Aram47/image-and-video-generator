#include "character/character_generator.hpp"
#include "character/mesh_builder.hpp"
#include "media/image_writer.hpp"
#include "png_image.hpp"
#include "render/camera.hpp"
#include "render/cpu_rasterizer.hpp"
#include "render/framebuffer.hpp"
#include "render/scene.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>

namespace {

class TempDirectory {
 public:
  TempDirectory() {
    path_ = std::filesystem::temp_directory_path() /
            ("gen-png-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path_);
  }

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  std::filesystem::path file(const std::string& name) const { return path_ / name; }

 private:
  std::filesystem::path path_;
};

gen::Color pixelAt(const PngImage& image, int x, int y) {
  const auto offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4;
  return gen::Color{image.rgba[offset], image.rgba[offset + 1], image.rgba[offset + 2], image.rgba[offset + 3]};
}

}  // namespace

TEST(ImageWriterTest, PngMatchesTheFramebufferSizeAndShowsTheFigure) {
  constexpr int kSize = 64;
  const auto appearance = gen::CharacterGenerator{}.generate(gen::CharacterSpec{}, 42);
  gen::MeshBuilder builder;
  const gen::Mesh mesh = builder.build(appearance.body, gen::PoseLibrary::stand(), appearance.colors.skin);

  gen::Scene scene;
  scene.meshes.push_back(mesh);
  scene.background = gen::dayBackground();
  scene.light = gen::Light::day();

  gen::Framebuffer framebuffer(kSize, kSize);
  framebuffer.clear(scene.background);
  gen::CpuRasterizer rasterizer;
  rasterizer.render(scene, gen::Camera::defaultPortrait(), framebuffer);

  TempDirectory directory;
  const auto path = directory.file("image.png");
  gen::ImageWriter writer;
  writer.writePng(framebuffer, path);

  const PngImage image = readPng(path);
  EXPECT_EQ(image.width, kSize);
  EXPECT_EQ(image.height, kSize);
  const gen::Color background = gen::dayBackground();
  EXPECT_EQ(pixelAt(image, 0, 0), background);
  EXPECT_EQ(pixelAt(image, kSize - 1, 0), background);
  EXPECT_EQ(pixelAt(image, 0, kSize - 1), background);
  EXPECT_EQ(pixelAt(image, kSize - 1, kSize - 1), background);
  EXPECT_NE(pixelAt(image, kSize / 2, kSize / 2), background);
}
