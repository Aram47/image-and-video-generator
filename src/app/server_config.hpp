#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace gen {

struct ServerConfig {
  std::string bindAddress{"127.0.0.1"};
  std::uint16_t port{7070};
  std::size_t maxMessageBytes{64 * 1024};
  std::size_t queueCapacity{8};
  int workerCount{1};
  std::filesystem::path outputDirectory{"outputs"};
  bool keepFramesOnEncodingFailure{false};
  bool authRequired{false};
  std::filesystem::path authTokenFile;
  std::filesystem::path tlsCert;
  std::filesystem::path tlsKey;
  std::uint64_t maxOutputBytes{2ull * 1024 * 1024 * 1024};
  std::filesystem::path jobDirectory{"data/jobs"};
  bool logPrompts{false};
  int frameRetentionSeconds{0};

  void validate() const;
  void ensureTlsMaterial() const;
  std::string readAuthToken() const;
  bool tlsEnabled() const;
  std::filesystem::path dataDirectory() const;

  static ServerConfig load(const std::filesystem::path& path);
};

}  // namespace gen
