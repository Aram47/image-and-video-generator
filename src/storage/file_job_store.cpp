#include "storage/file_job_store.hpp"

#include "common/error.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace gen {
namespace {

constexpr const char* kInterruptedMessage = "Job was interrupted by a restart.";

bool isSafeId(std::string_view jobId) {
  if (jobId.empty()) {
    return false;
  }
  for (const char character : jobId) {
    const bool allowed = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
                         (character >= '0' && character <= '9') || character == '_' || character == '-';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

void restrictOwner(const std::filesystem::path& path, bool directory) {
  const auto permissions = directory ? std::filesystem::perms::owner_all
                                     : (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
  std::error_code error;
  std::filesystem::permissions(path, permissions, std::filesystem::perm_options::replace, error);
  if (error) {
    throw std::runtime_error("Failed to store the job.");
  }
}

std::int64_t nowMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

nlohmann::json optionsJson(const Job& job) {
  return std::visit(
      [](const auto& options) {
        nlohmann::json body = {{"width", options.width},
                               {"height", options.height},
                               {"seed", options.seed},
                               {"style", options.style}};
        if constexpr (std::is_same_v<std::decay_t<decltype(options)>, VideoOptions>) {
          body["fps"] = options.fps;
          body["duration_seconds"] = options.durationSeconds;
          if (options.action) {
            body["action"] = *options.action;
          }
        }
        return body;
      },
      job.options);
}

nlohmann::json resultJson(const Job& job) {
  if (!job.result) {
    return nullptr;
  }
  nlohmann::json body = {{"media_type", job.result->mediaType},
                         {"path", job.result->relativePath},
                         {"bytes", job.result->bytes}};
  if (job.result->frameCount) {
    body["frame_count"] = *job.result->frameCount;
  }
  if (job.result->actionName) {
    body["action"] = *job.result->actionName;
  }
  return body;
}

JobState parseState(const std::string& text) {
  if (text == "received") {
    return JobState::Received;
  }
  if (text == "validating") {
    return JobState::Validating;
  }
  if (text == "queued") {
    return JobState::Queued;
  }
  if (text == "running") {
    return JobState::Running;
  }
  if (text == "completed") {
    return JobState::Completed;
  }
  return JobState::Failed;
}

std::optional<Job> parseJob(const nlohmann::json& body) {
  if (!body.is_object() || !body.contains("id") || !body["id"].is_string() || !body.contains("type") ||
      !body["type"].is_string() || !body.contains("state") || !body["state"].is_string() || !body.contains("prompt") ||
      !body["prompt"].is_string() || !body.contains("request_id") || !body["request_id"].is_string() ||
      !body.contains("options") || !body["options"].is_object()) {
    return std::nullopt;
  }

  Job job;
  job.id = body["id"].get<std::string>();
  if (!isSafeId(job.id)) {
    return std::nullopt;
  }
  job.requestId = body["request_id"].get<std::string>();
  job.prompt = body["prompt"].get<std::string>();
  job.state = parseState(body["state"].get<std::string>());
  const auto type = body["type"].get<std::string>();
  const auto& options = body["options"];
  if (!options.contains("width") || !options["width"].is_number_integer() || !options.contains("height") ||
      !options["height"].is_number_integer() || !options.contains("seed") || !options["seed"].is_number_integer() ||
      !options.contains("style") || !options["style"].is_string()) {
    return std::nullopt;
  }
  const auto seed = options["seed"].get<std::int64_t>();
  if (seed < 0 || seed > 4294967295LL) {
    return std::nullopt;
  }

  if (type == "image") {
    job.type = JobType::Image;
    ImageOptions image;
    image.width = options["width"].get<int>();
    image.height = options["height"].get<int>();
    image.seed = static_cast<std::uint32_t>(seed);
    image.style = options["style"].get<std::string>();
    job.options = image;
  } else if (type == "video") {
    if (!options.contains("fps") || !options["fps"].is_number_integer() || !options.contains("duration_seconds") ||
        !options["duration_seconds"].is_number_integer()) {
      return std::nullopt;
    }
    job.type = JobType::Video;
    VideoOptions video;
    video.width = options["width"].get<int>();
    video.height = options["height"].get<int>();
    video.fps = options["fps"].get<int>();
    video.durationSeconds = options["duration_seconds"].get<int>();
    video.seed = static_cast<std::uint32_t>(seed);
    video.style = options["style"].get<std::string>();
    if (options.contains("action")) {
      if (!options["action"].is_string()) {
        return std::nullopt;
      }
      video.action = options["action"].get<std::string>();
    }
    job.options = video;
  } else {
    return std::nullopt;
  }

  if (body.contains("error_code") && body["error_code"].is_string()) {
    job.errorCode = body["error_code"].get<std::string>();
  }
  if (body.contains("error_message") && body["error_message"].is_string()) {
    job.errorMessage = body["error_message"].get<std::string>();
  }
  if (body.contains("result") && body["result"].is_object()) {
    const auto& result = body["result"];
    if (!result.contains("media_type") || !result["media_type"].is_string() || !result.contains("path") ||
        !result["path"].is_string()) {
      return std::nullopt;
    }
    JobResult stored;
    stored.mediaType = result["media_type"].get<std::string>();
    stored.relativePath = result["path"].get<std::string>();
    if (result.contains("bytes") && result["bytes"].is_number_integer() && result["bytes"].get<std::int64_t>() >= 0) {
      stored.bytes = result["bytes"].get<std::uint64_t>();
    }
    if (result.contains("frame_count") && result["frame_count"].is_number_integer()) {
      stored.frameCount = result["frame_count"].get<int>();
    }
    if (result.contains("action") && result["action"].is_string()) {
      stored.actionName = result["action"].get<std::string>();
    }
    job.result = std::move(stored);
  }
  return job;
}

}  // namespace

FileJobStore::FileJobStore(std::filesystem::path directory, std::filesystem::path outputDirectory)
    : directory_(std::move(directory)), outputDirectory_(std::move(outputDirectory)) {
  if (directory_.empty()) {
    throw std::invalid_argument("jobDirectory is required.");
  }
  std::error_code error;
  std::filesystem::create_directories(directory_, error);
  if (error) {
    throw std::runtime_error("Failed to store the job.");
  }
  restrictOwner(directory_, true);
  load();
}

bool FileJobStore::insert(const Job& job) {
  if (!isSafeId(job.id)) {
    throw std::invalid_argument("Job id cannot be used as a path.");
  }
  std::lock_guard lock(mutex_);
  if (jobs_.contains(job.id)) {
    return false;
  }
  persist(job);
  jobs_.emplace(job.id, job);
  return true;
}

void FileJobStore::erase(const std::string& id) {
  std::lock_guard lock(mutex_);
  jobs_.erase(id);
  if (!isSafeId(id)) {
    return;
  }
  std::error_code error;
  std::filesystem::remove(directory_ / (id + ".json"), error);
}

std::optional<Job> FileJobStore::find(const std::string& id) const {
  std::lock_guard lock(mutex_);
  const auto found = jobs_.find(id);
  if (found == jobs_.end()) {
    return std::nullopt;
  }
  return found->second;
}

bool FileJobStore::update(const std::string& id, const std::function<void(Job&)>& mutate) {
  std::lock_guard lock(mutex_);
  const auto found = jobs_.find(id);
  if (found == jobs_.end()) {
    return false;
  }
  Job next = found->second;
  mutate(next);
  persist(next);
  found->second = std::move(next);
  return true;
}

StoreRecovery FileJobStore::recover() {
  std::lock_guard lock(mutex_);
  StoreRecovery recovery;
  if (recovered_) {
    return recovery;
  }
  recovered_ = true;

  for (auto& entry : jobs_) {
    Job& job = entry.second;
    if (job.state == JobState::Running) {
      job.state = JobState::Failed;
      job.result.reset();
      job.scene.reset();
      job.errorCode = std::string(toString(ErrorCode::InternalError));
      job.errorMessage = kInterruptedMessage;
      persist(job);
      recovery.interrupted.push_back(job.id);
      continue;
    }
    if (job.state == JobState::Queued && !outputExists(job)) {
      recovery.requeue.push_back(job.id);
    }
  }
  return recovery;
}

void FileJobStore::load() {
  std::error_code error;
  if (!std::filesystem::is_directory(directory_, error)) {
    return;
  }
  for (const auto& entry : std::filesystem::directory_iterator(directory_, error)) {
    if (error || !entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() != ".json") {
      continue;
    }
    const auto stem = entry.path().stem().string();
    if (!isSafeId(stem)) {
      continue;
    }
    std::ifstream input(entry.path(), std::ios::binary);
    if (!input) {
      continue;
    }
    nlohmann::json body;
    try {
      input >> body;
    } catch (const nlohmann::json::exception&) {
      continue;
    }
    auto job = parseJob(body);
    if (!job || job->id != stem) {
      continue;
    }
    jobs_.emplace(job->id, std::move(*job));
  }
}

void FileJobStore::persist(const Job& job) const {
  if (!isSafeId(job.id)) {
    throw std::invalid_argument("Job id cannot be used as a path.");
  }
  const nlohmann::json body = {{"id", job.id},
                               {"request_id", job.requestId},
                               {"type", std::string(toString(job.type))},
                               {"state", std::string(toString(job.state))},
                               {"prompt", job.prompt},
                               {"options", optionsJson(job)},
                               {"error_code", job.errorCode},
                               {"error_message", job.errorMessage},
                               {"result", resultJson(job)},
                               {"updated_at", nowMillis()}};
  const auto finalPath = directory_ / (job.id + ".json");
  const auto temporary = directory_ / (job.id + ".json.tmp");
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
      throw std::runtime_error("Failed to store the job.");
    }
    output << body.dump() << '\n';
    output.flush();
    if (!output) {
      std::error_code error;
      std::filesystem::remove(temporary, error);
      throw std::runtime_error("Failed to store the job.");
    }
  }
  std::error_code error;
  std::filesystem::rename(temporary, finalPath, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    throw std::runtime_error("Failed to store the job.");
  }
  restrictOwner(finalPath, false);
}

bool FileJobStore::outputExists(const Job& job) const {
  std::error_code error;
  const auto jobDirectory = outputDirectory_ / job.id;
  if (job.result) {
    const auto name = std::filesystem::path(job.result->relativePath).filename();
    if (!name.empty() && std::filesystem::is_regular_file(jobDirectory / name, error)) {
      return true;
    }
  }
  const char* fallback = job.type == JobType::Image ? "image.png" : "video.mp4";
  if (std::filesystem::is_regular_file(jobDirectory / fallback, error)) {
    return true;
  }
  return std::filesystem::is_regular_file(jobDirectory / "stub.txt", error);
}

}  // namespace gen
