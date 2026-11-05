#pragma once

#include "common/geometry.hpp"

#include <cstdint>
#include <vector>

namespace gen {

class Framebuffer {
 public:
  Framebuffer(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }
  const std::vector<std::uint8_t>& rgba() const { return rgba_; }

  void clear(Color color);
  void setPixel(int x, int y, float depth, Color color);
  Color pixel(int x, int y) const;

 private:
  std::size_t index(int x, int y) const;

  int width_;
  int height_;
  std::vector<std::uint8_t> rgba_;
  std::vector<float> depth_;
};

}  // namespace gen
