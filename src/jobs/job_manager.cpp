#include "jobs/job_manager.hpp"

#include "common/utf8.hpp"

#include <random>
#include <stdexcept>
#include <string>

namespace gen {
namespace {

constexpr int kMaxIdAttempts = 8;

}  // namespace

JobManager::JobManager(std::size_t queueCapacity, std::unique_ptr<IJobExecutor> executor, Logger& logger,
                       IJobStore& store, OutputQuota& quota, Metrics& metrics, bool logPrompts)
    : logger_(logger),
      store_(store),
      quota_(quota),
      metrics_(metrics),
      logPrompts_(logPrompts),
      queue_(queueCapacity),
      executor_(requireExecutor(std::move(executor))),
      worker_(queue_, store_, quota_, metrics_, *executor_, logger_) {}

JobManager::~JobManager() { stop(); }

std::unique_ptr<IJobExecutor> JobManager::requireExecutor(std::unique_ptr<IJobExecutor> executor) {
  if (!executor) {
    throw std::invalid_argument("Job executor is required.");
  }
  return executor;
}

void JobManager::start() {
  const StoreRecovery recovery = store_.recover();
  for (const auto& jobId : recovery.interrupted) {
    metrics_.setQueueDepth(queue_.size());
    metrics_.failed(jobId);
  }
  for (const auto& jobId : recovery.requeue) {
    if (queue_.contains(jobId)) {
      continue;
    }
    if (!queue_.tryPush(jobId)) {
      break;
    }
  }
  worker_.start();
}

void JobManager::stop() { worker_.stop(); }

std::size_t JobManager::queuedCount() const { return queue_.size(); }

std::variant<std::string, Error> JobManager::submitImage(const ImageRequest& request) {
  Job job;
  job.state = JobState::Received;
  job.state = JobState::Validating;
  job.type = JobType::Image;
  job.requestId = request.requestId;
  job.prompt = request.prompt;
  ImageOptions options;
  options.width = request.width;
  options.height = request.height;
  options.style = request.style;
  options.seed = request.seed.value_or(generateSeed());
  job.options = std::move(options);
  job.state = JobState::Queued;
  return enqueue(std::move(job));
}

std::variant<std::string, Error> JobManager::submitVideo(const VideoRequest& request) {
  Job job;
  job.state = JobState::Received;
  job.state = JobState::Validating;
  job.type = JobType::Video;
  job.requestId = request.requestId;
  job.prompt = request.prompt;
  VideoOptions options;
  options.width = request.width;
  options.height = request.height;
  options.fps = request.fps;
  options.durationSeconds = request.durationSeconds;
  options.style = request.style;
  options.action = request.action;
  options.seed = request.seed.value_or(generateSeed());
  job.options = std::move(options);
  job.state = JobState::Queued;
  return enqueue(std::move(job));
}

std::variant<JobSnapshot, Error> JobManager::status(const std::string& jobId) const {
  const auto job = store_.find(jobId);
  if (!job) {
    return Error{ErrorCode::JobNotFound, "Job was not found."};
  }
  return toSnapshot(*job);
}

std::variant<std::string, Error> JobManager::enqueue(Job job) {
  if (queue_.size() >= queue_.capacity()) {
    return Error{ErrorCode::ResourceLimitExceeded, "queue is full"};
  }
  if (quota_.exceeded()) {
    return Error{ErrorCode::ResourceLimitExceeded, "output storage is full"};
  }

  for (int attempt = 0; attempt < kMaxIdAttempts; ++attempt) {
    job.id = generateJobId();
    try {
      if (!store_.insert(job)) {
        continue;
      }
    } catch (const std::exception&) {
      return Error{ErrorCode::InternalError, "Job processing failed."};
    }
    if (!queue_.tryPush(job.id)) {
      store_.erase(job.id);
      return Error{ErrorCode::ResourceLimitExceeded, "queue is full"};
    }
    metrics_.setQueueDepth(queue_.size());
    metrics_.accepted();
    logger_.info("job_accepted", job.requestId, job.id, acceptanceDetail(job));
    return job.id;
  }
  return Error{ErrorCode::InternalError, "Job processing failed."};
}

std::string JobManager::acceptanceDetail(const Job& job) const {
  const Utf8Length length = utf8Length(job.prompt);
  const std::size_t promptLength = length.valid ? length.codePoints : job.prompt.size();
  std::string detail = "type=" + std::string(toString(job.type)) + " prompt_length=" + std::to_string(promptLength);
  if (logPrompts_) {
    detail += " prompt=" + job.prompt;
  }
  return detail;
}

std::string JobManager::generateJobId() {
  std::random_device device;
  constexpr char kHex[] = "0123456789abcdef";
  std::string id = "job-";
  id.reserve(36);
  for (int index = 0; index < 16; ++index) {
    const auto byte = static_cast<unsigned char>(device());
    id.push_back(kHex[byte >> 4]);
    id.push_back(kHex[byte & 0x0F]);
  }
  return id;
}

std::uint32_t JobManager::generateSeed() {
  std::random_device device;
  return static_cast<std::uint32_t>(device());
}

}  // namespace gen
