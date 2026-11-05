#pragma once

#include "common/error.hpp"
#include "common/logger.hpp"
#include "common/metrics.hpp"
#include "jobs/ijob_executor.hpp"
#include "jobs/job_queue.hpp"
#include "prompt/prompt_parser.hpp"
#include "storage/ijob_store.hpp"
#include "storage/output_quota.hpp"

#include <string>
#include <string_view>
#include <thread>

namespace gen {

class Worker {
 public:
  Worker(JobQueue& queue, IJobStore& store, OutputQuota& quota, Metrics& metrics, IJobExecutor& executor,
         Logger& logger);
  ~Worker();

  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  void start();
  void stop();

 private:
  void run();
  void process(const std::string& jobId);
  void fail(const std::string& jobId, const std::string& requestId, ErrorCode code, const std::string& message,
            std::string_view logDetail);

  JobQueue& queue_;
  IJobStore& store_;
  OutputQuota& quota_;
  Metrics& metrics_;
  IJobExecutor& executor_;
  Logger& logger_;
  PromptParser parser_;
  std::thread thread_;
};

}  // namespace gen
