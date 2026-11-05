#pragma once

#include "jobs/job.hpp"

namespace gen {

class IJobExecutor {
 public:
  virtual ~IJobExecutor() = default;

  IJobExecutor() = default;
  IJobExecutor(const IJobExecutor&) = delete;
  IJobExecutor& operator=(const IJobExecutor&) = delete;

  virtual JobResult execute(const Job& job) = 0;
};

}  // namespace gen
