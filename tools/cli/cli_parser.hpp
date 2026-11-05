#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace gen {

struct CliOptions {
  std::string command;
  std::string host{"127.0.0.1"};
  std::uint16_t port{7070};
  std::string token;
  double pollIntervalSeconds{1};
  double timeoutSeconds{120};
  std::filesystem::path caFile;
  std::string prompt;
  int width{0};
  int height{0};
  int fps{0};
  int durationSeconds{0};
  std::string action;
  std::optional<std::uint32_t> seed;
  std::string jobId;
};

class CliParser {
 public:
  CliParser(int argc, char** argv);

  CliOptions parse() const;

 private:
  int argc_;
  char** argv_;
};

}  // namespace gen
