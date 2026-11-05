#pragma once

#include "common/error.hpp"
#include "common/logger.hpp"
#include "common/metrics.hpp"
#include "jobs/ijob_executor.hpp"
#include "jobs/job_queue.hpp"
#include "jobs/job_request.hpp"
#include "jobs/worker.hpp"
#include "storage/ijob_store.hpp"
#include "storage/output_quota.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <variant>

namespace gen {

class JobManager {
 public:
  JobManager(std::size_t queueCapacity, std::unique_ptr<IJobExecutor> executor, Logger& logger, IJobStore& store,
             OutputQuota& quota, Metrics& metrics, bool logPrompts = false);
  ~JobManager();

  JobManager(const JobManager&) = delete;
  JobManager& operator=(const JobManager&) = delete;

  std::variant<std::string, Error> submitImage(const ImageRequest& request);
  std::variant<std::string, Error> submitVideo(const VideoRequest& request);
  std::variant<JobSnapshot, Error> status(const std::string& jobId) const;

  void start();
  void stop();
  std::size_t queuedCount() const;

 private:
  static std::unique_ptr<IJobExecutor> requireExecutor(std::unique_ptr<IJobExecutor> executor);
  static std::string generateJobId();
  static std::uint32_t generateSeed();
  std::string acceptanceDetail(const Job& job) const;

  std::variant<std::string, Error> enqueue(Job job);

  Logger& logger_;
  IJobStore& store_;
  OutputQuota& quota_;
  Metrics& metrics_;
  bool logPrompts_;
  JobQueue queue_;
  std::unique_ptr<IJobExecutor> executor_;
  Worker worker_;
};

}  // namespace gen
