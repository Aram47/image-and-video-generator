#include "jobs/worker.hpp"

#include "jobs/pipeline_failure.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace gen {
namespace {

PromptTarget promptTarget(const Job& job) {
  return job.type == JobType::Video ? PromptTarget::Video : PromptTarget::Image;
}

std::optional<std::string_view> requestedAction(const Job& job) {
  const auto* video = std::get_if<VideoOptions>(&job.options);
  if (video == nullptr || !video->action || video->action->empty()) {
    return std::nullopt;
  }
  return std::string_view{*video->action};
}

}  // namespace

Worker::Worker(JobQueue& queue, IJobStore& store, OutputQuota& quota, Metrics& metrics, IJobExecutor& executor,
               Logger& logger)
    : queue_(queue), store_(store), quota_(quota), metrics_(metrics), executor_(executor), logger_(logger) {}

Worker::~Worker() { stop(); }

void Worker::start() {
  if (thread_.joinable()) {
    return;
  }
  thread_ = std::thread([this] { run(); });
}

void Worker::stop() {
  queue_.stop();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Worker::run() {
  for (;;) {
    const auto jobId = queue_.pop();
    if (!jobId) {
      return;
    }
    try {
      process(*jobId);
    } catch (...) {
    }
  }
}

void Worker::process(const std::string& jobId) {
  const auto existing = store_.find(jobId);
  if (!existing) {
    return;
  }
  try {
    store_.update(jobId, [](Job& job) { job.state = JobState::Running; });
    logger_.info("job_started", existing->requestId, jobId, "state=running");

    const auto current = store_.find(jobId);
    if (!current) {
      return;
    }
    const auto parsed =
        parser_.parse(current->prompt, promptTarget(*current), requestedAction(*current), jobSeed(*current));
    if (const auto* error = std::get_if<Error>(&parsed)) {
      fail(jobId, existing->requestId, error->code, error->message, error->message);
      return;
    }

    Job prepared = *current;
    prepared.scene = std::get<SceneSpec>(parsed);
    const JobResult result = executor_.execute(prepared);
    store_.update(jobId, [&prepared, &result](Job& job) {
      job.state = JobState::Completed;
      job.errorCode.clear();
      job.errorMessage.clear();
      job.scene = prepared.scene;
      job.result = result;
    });
    quota_.add(result.bytes);
    metrics_.setQueueDepth(queue_.size());
    metrics_.completed(jobId);
    logger_.info("job_completed", existing->requestId, jobId, "state=completed");
  } catch (const PipelineFailure& failure) {
    fail(jobId, existing->requestId, failure.code(), failure.clientMessage(), failure.what());
  } catch (...) {
    fail(jobId, existing->requestId, ErrorCode::InternalError, "Job processing failed.", "Job processing failed.");
  }
}

void Worker::fail(const std::string& jobId, const std::string& requestId, ErrorCode code, const std::string& message,
                  std::string_view logDetail) {
  const auto codeText = std::string(toString(code));
  store_.update(jobId, [&codeText, &message](Job& job) {
    job.state = JobState::Failed;
    job.result.reset();
    job.scene.reset();
    job.errorCode = codeText;
    job.errorMessage = message;
  });
  metrics_.setQueueDepth(queue_.size());
  metrics_.failed(jobId);
  logger_.error("job_failed", requestId, jobId, "code=" + codeText + " " + std::string(logDetail));
}

}  // namespace gen
