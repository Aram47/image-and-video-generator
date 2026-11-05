#pragma once

#include "prompt/scene_spec.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace gen {

enum class JobType { Image, Video };

enum class JobState { Received, Validating, Queued, Running, Completed, Failed };

struct ImageOptions {
  int width{0};
  int height{0};
  std::uint32_t seed{0};
  std::string style;
};

struct VideoOptions {
  int width{0};
  int height{0};
  int fps{0};
  int durationSeconds{0};
  std::uint32_t seed{0};
  std::string style;
  std::optional<std::string> action;
};

struct JobResult {
  JobResult() = default;
  JobResult(std::string media, std::string path) : mediaType(std::move(media)), relativePath(std::move(path)) {}

  std::string mediaType;
  std::string relativePath;
  std::optional<int> frameCount;
  std::optional<std::string> actionName;
  std::uint64_t bytes{0};
};

struct Job {
  std::string id;
  std::string requestId;
  JobType type{JobType::Image};
  std::string prompt;
  std::variant<ImageOptions, VideoOptions> options;
  JobState state{JobState::Received};
  std::string errorCode;
  std::string errorMessage;
  std::optional<JobResult> result;
  std::optional<SceneSpec> scene;
};

struct JobSnapshot {
  std::string id;
  JobType type{JobType::Image};
  JobState state{JobState::Queued};
  std::uint32_t seed{0};
  std::string errorCode;
  std::string errorMessage;
  std::optional<JobResult> result;
  bool parsed{false};
  std::vector<std::string> unsupportedTerms;
  std::optional<int> frameCount;
  std::optional<std::string> actionName;
};

inline std::string_view toString(JobType type) {
  return type == JobType::Image ? "image" : "video";
}

inline std::string_view toString(JobState state) {
  switch (state) {
    case JobState::Received:
      return "received";
    case JobState::Validating:
      return "validating";
    case JobState::Queued:
      return "queued";
    case JobState::Running:
      return "running";
    case JobState::Completed:
      return "completed";
    case JobState::Failed:
      return "failed";
  }
  return "failed";
}

inline std::uint32_t jobSeed(const Job& job) {
  return std::visit([](const auto& options) { return options.seed; }, job.options);
}

inline JobSnapshot toSnapshot(const Job& job) {
  JobSnapshot snapshot;
  snapshot.id = job.id;
  snapshot.type = job.type;
  snapshot.state = job.state;
  snapshot.seed = jobSeed(job);
  snapshot.errorCode = job.errorCode;
  snapshot.errorMessage = job.errorMessage;
  snapshot.result = job.result;
  if (job.result) {
    snapshot.frameCount = job.result->frameCount;
    snapshot.actionName = job.result->actionName;
  }
  if (job.scene) {
    snapshot.parsed = true;
    snapshot.unsupportedTerms = job.scene->unsupportedTerms;
  }
  return snapshot;
}

}  // namespace gen
