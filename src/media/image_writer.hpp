#pragma once

#include "render/framebuffer.hpp"

#include <filesystem>

namespace gen {

class ImageWriter {
 public:
  void writePng(const Framebuffer& framebuffer, const std::filesystem::path& path) const;
};

}  // namespace gen
