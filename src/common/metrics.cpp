#include "common/metrics.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <utility>

namespace gen {
namespace {

void restrictToOwner(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, error);
}

}  // namespace

Metrics::Metrics(Logger& logger, std::filesystem::path snapshotPath)
    : logger_(logger), snapshotPath_(std::move(snapshotPath)) {}

void Metrics::setQueueDepth(std::size_t depth) {
  std::lock_guard lock(mutex_);
  snapshot_.queueDepth = depth;
}

void Metrics::accepted() {
  Snapshot copy;
  {
    std::lock_guard lock(mutex_);
    ++snapshot_.jobsAccepted;
    copy = snapshot_;
    writeSnapshot(copy);
  }
}

void Metrics::completed(std::string_view jobId) {
  Snapshot copy;
  {
    std::lock_guard lock(mutex_);
    ++snapshot_.jobsCompleted;
    copy = snapshot_;
    writeSnapshot(copy);
  }
  logSnapshot(jobId, copy);
}

void Metrics::failed(std::string_view jobId) {
  Snapshot copy;
  {
    std::lock_guard lock(mutex_);
    ++snapshot_.jobsFailed;
    copy = snapshot_;
    writeSnapshot(copy);
  }
  logSnapshot(jobId, copy);
}

void Metrics::rejected() {
  Snapshot copy;
  {
    std::lock_guard lock(mutex_);
    ++snapshot_.jobsRejected;
    copy = snapshot_;
    writeSnapshot(copy);
  }
}

void Metrics::writeSnapshot(const Snapshot& snapshot) const {
  if (snapshotPath_.empty()) {
    return;
  }
  try {
    const auto parent = snapshotPath_.parent_path();
    if (!parent.empty()) {
      std::error_code error;
      std::filesystem::create_directories(parent, error);
      if (error) {
        return;
      }
    }
    const auto temporary = snapshotPath_.string() + ".tmp";
    {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      if (!output) {
        return;
      }
      const nlohmann::json body = {{"jobs_accepted", snapshot.jobsAccepted},
                                   {"jobs_completed", snapshot.jobsCompleted},
                                   {"jobs_failed", snapshot.jobsFailed},
                                   {"jobs_rejected", snapshot.jobsRejected},
                                   {"queue_depth", snapshot.queueDepth}};
      output << body.dump() << '\n';
      if (!output) {
        return;
      }
    }
    std::error_code error;
    std::filesystem::rename(temporary, snapshotPath_, error);
    if (error) {
      std::filesystem::remove(temporary, error);
      return;
    }
    restrictToOwner(snapshotPath_);
  } catch (...) {
  }
}

void Metrics::logSnapshot(std::string_view jobId, const Snapshot& snapshot) const {
  logger_.info("metrics", "", jobId, detail(snapshot));
}

std::string Metrics::detail(const Snapshot& snapshot) {
  return "jobs_accepted=" + std::to_string(snapshot.jobsAccepted) +
         " jobs_completed=" + std::to_string(snapshot.jobsCompleted) +
         " jobs_failed=" + std::to_string(snapshot.jobsFailed) +
         " jobs_rejected=" + std::to_string(snapshot.jobsRejected) +
         " queue_depth=" + std::to_string(snapshot.queueDepth);
}

}  // namespace gen
