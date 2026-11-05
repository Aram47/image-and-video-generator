#include "media/image_writer.hpp"

#include "stb/stb_image_write.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace gen {

void ImageWriter::writePng(const Framebuffer& framebuffer, const std::filesystem::path& path) const {
  const int width = framebuffer.width();
  const int height = framebuffer.height();
  const auto expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  if (width <= 0 || height <= 0 || framebuffer.rgba().size() != expected) {
    throw std::runtime_error("Failed to write the image at " + path.string());
  }

  const int written =
      stbi_write_png(path.string().c_str(), width, height, 4, framebuffer.rgba().data(), width * 4);
  if (written == 0) {
    std::error_code error;
    std::filesystem::remove(path, error);
    throw std::runtime_error("Failed to write the image at " + path.string());
  }
}

}  // namespace gen
