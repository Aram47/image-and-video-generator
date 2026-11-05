#pragma once

#include "common/logger.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace gen {

class Metrics {
 public:
  Metrics(Logger& logger, std::filesystem::path snapshotPath);

  Metrics(const Metrics&) = delete;
  Metrics& operator=(const Metrics&) = delete;

  void setQueueDepth(std::size_t depth);
  void accepted();
  void completed(std::string_view jobId);
  void failed(std::string_view jobId);
  void rejected();

 private:
  struct Snapshot {
    std::uint64_t jobsAccepted{0};
    std::uint64_t jobsCompleted{0};
    std::uint64_t jobsFailed{0};
    std::uint64_t jobsRejected{0};
    std::uint64_t queueDepth{0};
  };

  void writeSnapshot(const Snapshot& snapshot) const;
  void logSnapshot(std::string_view jobId, const Snapshot& snapshot) const;
  static std::string detail(const Snapshot& snapshot);

  Logger& logger_;
  std::filesystem::path snapshotPath_;
  mutable std::mutex mutex_;
  Snapshot snapshot_;
};

}  // namespace gen
