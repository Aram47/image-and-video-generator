#include "common/logger.hpp"

#include <iostream>
#include <string>

namespace gen {
namespace {

std::string sanitize(std::string_view value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (const char character : value) {
    if (character == '\n' || character == '\r') {
      sanitized.push_back(' ');
    } else {
      sanitized.push_back(character);
    }
  }
  return sanitized;
}

}  // namespace

Logger::Logger(std::ostream& output) : output_(output) {}

void Logger::info(std::string_view event, std::string_view requestId, std::string_view jobId,
                  std::string_view detail) {
  write("info", event, requestId, jobId, detail);
}

void Logger::error(std::string_view event, std::string_view requestId, std::string_view jobId,
                   std::string_view detail) {
  write("error", event, requestId, jobId, detail);
}

void Logger::write(std::string_view level, std::string_view event, std::string_view requestId,
                   std::string_view jobId, std::string_view detail) {
  std::lock_guard lock(mutex_);
  output_ << "level=" << level << " event=" << sanitize(event) << " request_id=" << sanitize(requestId)
          << " job_id=" << sanitize(jobId) << " detail=" << sanitize(detail) << '\n';
}

}  // namespace gen
