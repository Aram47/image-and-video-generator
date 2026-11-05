#include "render/framebuffer.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace gen {

Framebuffer::Framebuffer(int width, int height) : width_(width), height_(height) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("Framebuffer size must be positive.");
  }
  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  rgba_.assign(count * 4, 0);
  depth_.assign(count, std::numeric_limits<float>::infinity());
}

void Framebuffer::clear(Color color) {
  for (std::size_t offset = 0; offset < rgba_.size(); offset += 4) {
    rgba_[offset] = color.red;
    rgba_[offset + 1] = color.green;
    rgba_[offset + 2] = color.blue;
    rgba_[offset + 3] = color.alpha;
  }
  std::fill(depth_.begin(), depth_.end(), std::numeric_limits<float>::infinity());
}

void Framebuffer::setPixel(int x, int y, float depth, Color color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return;
  }
  const std::size_t pixel = index(x, y);
  if (!(depth < depth_[pixel])) {
    return;
  }
  depth_[pixel] = depth;
  const std::size_t offset = pixel * 4;
  rgba_[offset] = color.red;
  rgba_[offset + 1] = color.green;
  rgba_[offset + 2] = color.blue;
  rgba_[offset + 3] = color.alpha;
}

Color Framebuffer::pixel(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    throw std::out_of_range("Pixel is outside the framebuffer.");
  }
  const std::size_t offset = index(x, y) * 4;
  return Color{rgba_[offset], rgba_[offset + 1], rgba_[offset + 2], rgba_[offset + 3]};
}

std::size_t Framebuffer::index(int x, int y) const {
  return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}

}  // namespace gen
