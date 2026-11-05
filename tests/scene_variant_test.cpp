#include "character/character_generator.hpp"
#include "character/hair_mesh_builder.hpp"
#include "character/mesh_builder.hpp"
#include "character/outfit_mesh_builder.hpp"
#include "jobs/scene_composer.hpp"
#include "media/image_writer.hpp"
#include "prompt/prompt_parser.hpp"
#include "render/cpu_rasterizer.hpp"
#include "render/environment_builder.hpp"
#include "render/framebuffer.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// Identical frames are compared as PNG bytes. stb_image_write emits a deterministic
// file for the same pixels, so a byte match is the equality check for this suite.
// Region and framing checks read pixels from the framebuffer that was written.

class TempDirectory {
 public:
  TempDirectory() {
    path_ = std::filesystem::temp_directory_path() /
            ("gen-variant-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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

gen::SceneSpec requireSpec(std::string_view prompt, std::uint32_t seed) {
  const auto parsed = gen::PromptParser{}.parse(prompt, gen::PromptTarget::Image, std::nullopt, seed);
  const auto* spec = std::get_if<gen::SceneSpec>(&parsed);
  if (spec == nullptr) {
    throw std::runtime_error("Prompt was rejected.");
  }
  return *spec;
}

gen::Framebuffer draw(const gen::ComposedFrame& frame, int size) {
  gen::Framebuffer framebuffer(size, size);
  framebuffer.clear(frame.scene.background);
  gen::CpuRasterizer{}.render(frame.scene, frame.camera, framebuffer);
  return framebuffer;
}

gen::Framebuffer renderPrompt(std::string_view prompt, std::uint32_t seed, int size) {
  return draw(gen::SceneComposer{}.compose(requireSpec(prompt, seed)), size);
}

std::string pngBytes(const gen::Framebuffer& framebuffer) {
  TempDirectory directory;
  const auto path = directory.file("image.png");
  gen::ImageWriter{}.writePng(framebuffer, path);
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool regionDiffers(const gen::Framebuffer& left, const gen::Framebuffer& right, int yBegin, int yEnd) {
  for (int y = yBegin; y < yEnd; ++y) {
    for (int x = 0; x < left.width(); ++x) {
      if (left.pixel(x, y) != right.pixel(x, y)) {
        return true;
      }
    }
  }
  return false;
}

std::optional<gen::Color> partColor(const gen::Scene& scene, const std::string& name) {
  for (const gen::Mesh& mesh : scene.meshes) {
    for (const gen::MeshPart& part : mesh.parts) {
      if (part.name == name) {
        return part.color;
      }
    }
  }
  return std::nullopt;
}

std::vector<std::string> environmentParts(gen::EnvironmentKind kind) {
  const auto meshes = gen::EnvironmentBuilderFactory::create(kind)->build();
  std::vector<std::string> names;
  for (const gen::Mesh& mesh : meshes) {
    for (const gen::MeshPart& part : mesh.parts) {
      names.push_back(part.name);
    }
  }
  return names;
}

gen::Framebuffer renderFigure(gen::Height height, std::uint32_t seed, int size) {
  gen::CharacterSpec spec;
  spec.height = {height, true};
  const auto appearance = gen::CharacterGenerator{}.generate(spec, seed);
  const auto pose = gen::PoseLibrary::stand();
  gen::Scene scene;
  scene.background = {255, 0, 255, 255};
  scene.light = gen::Light::day();
  scene.meshes.push_back(gen::MeshBuilder{}.build(appearance.body, pose, appearance.colors.skin));
  scene.meshes.push_back(
      gen::HairMeshBuilder{}.build(appearance.body, pose, spec.hair.style.value, appearance.colors.hair));
  auto clothing = gen::OutfitMeshBuilder{}.build(appearance.body, pose, spec.outfit, appearance.colors.shirt,
                                                 appearance.colors.outerwear);
  for (gen::Mesh& mesh : clothing) {
    scene.meshes.push_back(std::move(mesh));
  }
  gen::Framebuffer framebuffer(size, size);
  framebuffer.clear(scene.background);
  gen::CpuRasterizer{}.render(scene, gen::Camera::frameCharacter(height), framebuffer);
  return framebuffer;
}

bool figureIsInset(const gen::Framebuffer& framebuffer, gen::Color background) {
  const int width = framebuffer.width();
  const int height = framebuffer.height();
  for (int x = 0; x < width; ++x) {
    if (framebuffer.pixel(x, 0) != background || framebuffer.pixel(x, height - 1) != background) {
      return false;
    }
  }
  for (int y = 0; y < height; ++y) {
    if (framebuffer.pixel(0, y) != background || framebuffer.pixel(width - 1, y) != background) {
      return false;
    }
  }
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      if (framebuffer.pixel(x, y) != background) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

TEST(SceneVariantTest, SamePromptAndSeedWriteIdenticalPngBytes) {
  const auto first = renderPrompt("a woman with curly red hair wearing a blue coat", 42, 48);
  const auto second = renderPrompt("a woman with curly red hair wearing a blue coat", 42, 48);
  EXPECT_EQ(pngBytes(first), pngBytes(second));
}

TEST(SceneVariantTest, AnotherSeedChangesImplicitPixelsButNotExplicitClothing) {
  const auto first = gen::SceneComposer{}.compose(requireSpec("a woman wearing a blue coat", 0));
  const auto second = gen::SceneComposer{}.compose(requireSpec("a woman wearing a blue coat", 1));
  EXPECT_EQ(partColor(first.scene, "coat"), partColor(second.scene, "coat"));
  EXPECT_EQ(partColor(first.scene, "hair"), partColor(second.scene, "hair"));
  EXPECT_NE(partColor(first.scene, "head"), partColor(second.scene, "head"));
  EXPECT_NE(pngBytes(draw(first, 48)), pngBytes(draw(second, 48)));
}

TEST(SceneVariantTest, CurlyAndStraightDifferInTheUpperHalf) {
  const auto straight = renderPrompt("a woman with straight hair standing", 11, 128);
  const auto curly = renderPrompt("a woman with curly hair standing", 11, 128);
  EXPECT_TRUE(regionDiffers(straight, curly, 0, straight.height() / 2));
}

TEST(SceneVariantTest, EnvironmentsAreConstantAndVisuallyDistinct) {
  const auto plain = environmentParts(gen::EnvironmentKind::Plain);
  const auto street = environmentParts(gen::EnvironmentKind::CityStreet);
  const auto park = environmentParts(gen::EnvironmentKind::Park);
  EXPECT_EQ(plain, (std::vector<std::string>{"floor"}));
  EXPECT_NE(plain, street);
  EXPECT_NE(street, park);
  EXPECT_NE(std::find(street.begin(), street.end(), "building_left"), street.end());
  EXPECT_NE(std::find(street.begin(), street.end(), "road"), street.end());
  EXPECT_NE(std::find(park.begin(), park.end(), "crown_back"), park.end());

  const auto first = gen::EnvironmentBuilderFactory::create(gen::EnvironmentKind::Park)->build();
  const auto second = gen::EnvironmentBuilderFactory::create(gen::EnvironmentKind::Park)->build();
  ASSERT_EQ(first.size(), second.size());
  EXPECT_EQ(first[0].vertices, second[0].vertices);

  const auto plainFrame = renderPrompt("a woman standing", 8, 96);
  const auto streetFrame = renderPrompt("a woman standing on a city street", 8, 96);
  const auto parkFrame = renderPrompt("a woman standing in a park", 8, 96);
  EXPECT_NE(pngBytes(plainFrame), pngBytes(streetFrame));
  EXPECT_NE(pngBytes(streetFrame), pngBytes(parkFrame));
  EXPECT_NE(pngBytes(plainFrame), pngBytes(parkFrame));
}

TEST(SceneVariantTest, SunsetAndDayDifferByTheSkyColor) {
  const auto day = renderPrompt("a woman standing", 8, 64);
  const auto sunset = renderPrompt("a woman standing at sunset", 8, 64);
  EXPECT_EQ(day.pixel(0, 0), gen::backgroundFor(gen::TimeOfDay::Day));
  EXPECT_EQ(sunset.pixel(0, 0), gen::backgroundFor(gen::TimeOfDay::Sunset));
  EXPECT_NE(day.pixel(0, 0), sunset.pixel(0, 0));
}

TEST(SceneVariantTest, SittingDiffersFromStandingInTheLowerHalf) {
  const auto standing = renderPrompt("a woman standing", 11, 128);
  const auto sitting = renderPrompt("a woman sitting", 11, 128);
  EXPECT_TRUE(regionDiffers(standing, sitting, standing.height() / 2, standing.height()));
}

TEST(SceneVariantTest, ShortAverageAndTallStayInsideTheFrame) {
  const gen::Color background{255, 0, 255, 255};
  const gen::Height heights[] = {gen::Height::Short, gen::Height::Average, gen::Height::Tall};
  const int sizes[] = {512, 1024};
  for (const gen::Height height : heights) {
    for (const int size : sizes) {
      const auto framebuffer = renderFigure(height, 42, size);
      EXPECT_TRUE(figureIsInset(framebuffer, background)) << static_cast<int>(height) << " at " << size;
    }
  }
}

TEST(SceneVariantTest, OneFrameAt1024FinishesWithinAFewSeconds) {
  // Measured on the developer machine: one 1024×1024 city-street frame rendered in about 200 ms.
  const auto started = std::chrono::steady_clock::now();
  const auto framebuffer = renderPrompt(
      "A tall woman with curly red hair wearing a blue coat, standing on a city street at sunset", 12345, 1024);
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
  EXPECT_EQ(framebuffer.width(), 1024);
  EXPECT_LT(elapsed.count(), 5000) << "frame_1024_ms=" << elapsed.count();
}
