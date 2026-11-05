#include "cli_parser.hpp"

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>

namespace gen {
namespace {

std::string requireValue(int argc, char** argv, int& index, const char* name) {
  if (index + 1 >= argc) {
    throw std::invalid_argument(std::string("Missing value for ") + name);
  }
  ++index;
  return argv[index];
}

int requireInt(std::string_view text, const char* name) {
  int value = 0;
  const auto* begin = text.data();
  const auto* end = begin + text.size();
  const auto result = std::from_chars(begin, end, value);
  if (result.ec != std::errc{} || result.ptr != end) {
    throw std::invalid_argument(std::string("Invalid value for ") + name);
  }
  return value;
}

double requireDouble(const std::string& text, const char* name) {
  std::size_t consumed = 0;
  double value = 0;
  try {
    value = std::stod(text, &consumed);
  } catch (const std::exception&) {
    throw std::invalid_argument(std::string("Invalid value for ") + name);
  }
  if (consumed != text.size()) {
    throw std::invalid_argument(std::string("Invalid value for ") + name);
  }
  return value;
}

}  // namespace

CliParser::CliParser(int argc, char** argv) : argc_(argc), argv_(argv) {}

CliOptions CliParser::parse() const {
  if (argc_ < 2) {
    throw std::invalid_argument("Usage: generator_cli <image|video|status> [flags]");
  }
  CliOptions options;
  options.command = argv_[1];
  if (options.command != "image" && options.command != "video" && options.command != "status") {
    throw std::invalid_argument("Usage: generator_cli <image|video|status> [flags]");
  }

  for (int index = 2; index < argc_; ++index) {
    const std::string flag = argv_[index];
    if (flag == "--host") {
      options.host = requireValue(argc_, argv_, index, "--host");
    } else if (flag == "--port") {
      const int port = requireInt(requireValue(argc_, argv_, index, "--port"), "--port");
      if (port < 0 || port > 65535) {
        throw std::invalid_argument("Invalid value for --port");
      }
      options.port = static_cast<std::uint16_t>(port);
    } else if (flag == "--token") {
      options.token = requireValue(argc_, argv_, index, "--token");
    } else if (flag == "--poll-interval") {
      options.pollIntervalSeconds = requireDouble(requireValue(argc_, argv_, index, "--poll-interval"), "--poll-interval");
      if (options.pollIntervalSeconds < 0) {
        throw std::invalid_argument("Invalid value for --poll-interval");
      }
    } else if (flag == "--timeout") {
      options.timeoutSeconds = requireDouble(requireValue(argc_, argv_, index, "--timeout"), "--timeout");
      if (options.timeoutSeconds < 0) {
        throw std::invalid_argument("Invalid value for --timeout");
      }
    } else if (flag == "--ca-file") {
      options.caFile = requireValue(argc_, argv_, index, "--ca-file");
    } else if (flag == "--prompt") {
      options.prompt = requireValue(argc_, argv_, index, "--prompt");
    } else if (flag == "--width") {
      options.width = requireInt(requireValue(argc_, argv_, index, "--width"), "--width");
    } else if (flag == "--height") {
      options.height = requireInt(requireValue(argc_, argv_, index, "--height"), "--height");
    } else if (flag == "--fps") {
      options.fps = requireInt(requireValue(argc_, argv_, index, "--fps"), "--fps");
    } else if (flag == "--duration") {
      options.durationSeconds = requireInt(requireValue(argc_, argv_, index, "--duration"), "--duration");
    } else if (flag == "--action") {
      options.action = requireValue(argc_, argv_, index, "--action");
    } else if (flag == "--seed") {
      const auto text = requireValue(argc_, argv_, index, "--seed");
      unsigned long value = 0;
      const auto* begin = text.data();
      const auto* end = begin + text.size();
      const auto result = std::from_chars(begin, end, value);
      if (result.ec != std::errc{} || result.ptr != end || value > 4294967295UL) {
        throw std::invalid_argument("Invalid value for --seed");
      }
      options.seed = static_cast<std::uint32_t>(value);
    } else if (flag == "--job-id") {
      options.jobId = requireValue(argc_, argv_, index, "--job-id");
    } else {
      throw std::invalid_argument("Unknown flag " + flag);
    }
  }

  if (options.command == "status") {
    if (options.jobId.empty()) {
      throw std::invalid_argument("Missing value for --job-id");
    }
  } else {
    if (options.prompt.empty()) {
      throw std::invalid_argument("Missing value for --prompt");
    }
    if (options.width <= 0 || options.height <= 0) {
      throw std::invalid_argument("Missing value for --width or --height");
    }
  }
  if (options.command == "video") {
    if (options.fps <= 0) {
      throw std::invalid_argument("Missing value for --fps");
    }
    if (options.durationSeconds <= 0) {
      throw std::invalid_argument("Missing value for --duration");
    }
    if (options.action.empty()) {
      throw std::invalid_argument("Missing value for --action");
    }
  }
  if (options.host.empty()) {
    throw std::invalid_argument("Missing value for --host");
  }
  return options;
}

}  // namespace gen
