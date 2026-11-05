#pragma once

#include "jobs/ijob_executor.hpp"
#include "storage/output_store.hpp"

namespace gen {

class StubJobExecutor final : public IJobExecutor {
 public:
  explicit StubJobExecutor(OutputStore& output);

  JobResult execute(const Job& job) override;

 private:
  OutputStore& output_;
};

}  // namespace gen
