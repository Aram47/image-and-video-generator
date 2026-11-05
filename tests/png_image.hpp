#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

struct PngImage {
  int width{0};
  int height{0};
  std::vector<std::uint8_t> rgba;
};

PngImage readPng(const std::filesystem::path& path);
