#include "png_image.hpp"

#include <stdexcept>
#include <string>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

struct DecodedPixels {
  std::uint8_t* data{nullptr};
  ~DecodedPixels() { stbi_image_free(data); }
};

}  // namespace

PngImage readPng(const std::filesystem::path& path) {
  DecodedPixels decoded;
  PngImage image;
  int channels = 0;
  decoded.data = stbi_load(path.string().c_str(), &image.width, &image.height, &channels, 4);
  if (decoded.data == nullptr || image.width <= 0 || image.height <= 0) {
    throw std::runtime_error("Failed to open PNG " + path.string());
  }
  const auto count = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4;
  image.rgba.assign(decoded.data, decoded.data + count);
  return image;
}
