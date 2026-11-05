#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace gen {

struct ImageRequest {
  std::string requestId;
  std::string authToken;
  std::string prompt;
  int width{0};
  int height{0};
  std::optional<std::uint32_t> seed;
  std::string style;
};

struct VideoRequest {
  std::string requestId;
  std::string authToken;
  std::string prompt;
  int width{0};
  int height{0};
  int fps{0};
  int durationSeconds{0};
  std::optional<std::uint32_t> seed;
  std::string style;
  std::optional<std::string> action;
};

struct StatusRequest {
  std::string requestId;
  std::string authToken;
  std::string jobId;
};

}  // namespace gen
