#pragma once

#include <iostream>
#include <mutex>
#include <ostream>
#include <string_view>

namespace gen {

class Logger {
 public:
  explicit Logger(std::ostream& output = std::cerr);

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void info(std::string_view event, std::string_view requestId, std::string_view jobId,
            std::string_view detail);
  void error(std::string_view event, std::string_view requestId, std::string_view jobId,
             std::string_view detail);

 private:
  void write(std::string_view level, std::string_view event, std::string_view requestId,
             std::string_view jobId, std::string_view detail);

  std::ostream& output_;
  std::mutex mutex_;
};

}  // namespace gen
