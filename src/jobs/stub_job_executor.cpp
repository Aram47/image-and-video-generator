#include "jobs/stub_job_executor.hpp"

namespace gen {

StubJobExecutor::StubJobExecutor(OutputStore& output) : output_(output) {}

JobResult StubJobExecutor::execute(const Job& job) {
  const std::string mediaType = job.type == JobType::Image ? "image" : "video";
  return output_.writeStub(job.id, mediaType);
}

}  // namespace gen
