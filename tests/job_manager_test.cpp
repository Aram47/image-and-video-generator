#include "common/logger.hpp"
#include "jobs/ijob_executor.hpp"
#include "jobs/job_manager.hpp"
#include "jobs/stub_job_executor.hpp"
#include "storage/output_store.hpp"
#include "test_services.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

class TempDirectory {
 public:
  TempDirectory() {
    path_ = std::filesystem::temp_directory_path() /
            ("gen-jobs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path_ / "outputs");
  }

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  std::filesystem::path outputs() const { return path_ / "outputs"; }

 private:
  std::filesystem::path path_;
};

class FlakyExecutor : public gen::IJobExecutor {
 public:
  gen::JobResult execute(const gen::Job& job) override {
    if (calls_.fetch_add(1) == 0) {
      throw std::runtime_error("injected failure");
    }
    return {job.type == gen::JobType::Image ? "image" : "video", "outputs/" + job.id + "/stub.txt"};
  }

 private:
  std::atomic<int> calls_{0};
};

class RecordingExecutor : public gen::IJobExecutor {
 public:
  gen::JobResult execute(const gen::Job& job) override {
    std::lock_guard lock(mutex_);
    seen.push_back(job);
    return {job.type == gen::JobType::Image ? "image" : "video", "outputs/" + job.id + "/stub.txt"};
  }

  std::vector<gen::Job> seen;
  std::mutex mutex_;
};

gen::ImageRequest imageRequest() {
  gen::ImageRequest request;
  request.requestId = "UNIQUE_REQUEST_TOKEN_ZZZ";
  request.prompt = "UNIQUE_PROMPT_TOKEN_ZZZ";
  request.width = 32;
  request.height = 48;
  request.seed = 42;
  request.style = "stylized_3d";
  return request;
}

gen::VideoRequest videoRequest() {
  gen::VideoRequest request;
  request.requestId = "UNIQUE_REQUEST_TOKEN_ZZZ";
  request.prompt = "UNIQUE_PROMPT_TOKEN_ZZZ";
  request.width = 64;
  request.height = 64;
  request.fps = 12;
  request.durationSeconds = 2;
  request.seed = 7;
  request.style = "stylized_3d";
  request.action = "walk_and_wave";
  return request;
}

gen::JobSnapshot waitFor(gen::JobManager& jobs, const std::string& jobId, gen::JobState state) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    const auto result = jobs.status(jobId);
    if (const auto* snapshot = std::get_if<gen::JobSnapshot>(&result); snapshot && snapshot->state == state) {
      return *snapshot;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ADD_FAILURE() << "timed out waiting for job " << jobId;
  return {};
}

}  // namespace

TEST(JobManagerTest, FillsEightSlotsAndRejectsTheNinthWithoutAWorker) {
  std::ostringstream log;
  gen::Logger logger(log);
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<FlakyExecutor>(), logger, services.store, services.quota, services.metrics);

  std::string firstId;
  for (int index = 0; index < 8; ++index) {
    auto request = imageRequest();
    request.requestId = "request-" + std::to_string(index);
    const auto submitted = jobs.submitImage(request);
    const auto* jobId = std::get_if<std::string>(&submitted);
    ASSERT_NE(jobId, nullptr) << index;
    if (firstId.empty()) {
      firstId = *jobId;
    }
  }
  const auto rejected = jobs.submitImage(imageRequest());
  const auto* error = std::get_if<gen::Error>(&rejected);
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, gen::ErrorCode::ResourceLimitExceeded);
  EXPECT_EQ(jobs.queuedCount(), 8U);

  const auto status = jobs.status(firstId);
  const auto* snapshot = std::get_if<gen::JobSnapshot>(&status);
  ASSERT_NE(snapshot, nullptr);
  EXPECT_EQ(snapshot->state, gen::JobState::Queued);
  EXPECT_EQ(snapshot->seed, 42U);

  const auto missing = jobs.status("job-does-not-exist");
  const auto* missingError = std::get_if<gen::Error>(&missing);
  ASSERT_NE(missingError, nullptr);
  EXPECT_EQ(missingError->code, gen::ErrorCode::JobNotFound);
}

TEST(JobManagerTest, FailureOfOneJobDoesNotStopTheNext) {
  std::ostringstream log;
  gen::Logger logger(log);
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<FlakyExecutor>(), logger, services.store, services.quota, services.metrics);
  jobs.start();

  const auto first = std::get<std::string>(jobs.submitImage(imageRequest()));
  const auto failed = waitFor(jobs, first, gen::JobState::Failed);
  EXPECT_EQ(failed.errorCode, "INTERNAL_ERROR");
  EXPECT_EQ(failed.errorMessage, "Job processing failed.");
  EXPECT_FALSE(failed.result.has_value());

  auto secondRequest = imageRequest();
  secondRequest.requestId = "second-request";
  const auto second = std::get<std::string>(jobs.submitImage(secondRequest));
  const auto completed = waitFor(jobs, second, gen::JobState::Completed);
  EXPECT_EQ(completed.result->mediaType, "image");
  EXPECT_NE(log.str().find("event=job_failed"), std::string::npos);
  EXPECT_NE(log.str().find("request_id=UNIQUE_REQUEST_TOKEN_ZZZ"), std::string::npos);
  EXPECT_NE(log.str().find("job_id=" + first), std::string::npos);
}

TEST(JobManagerTest, StubCompletesImageAndVideoWithoutUsingCallerTextInThePath) {
  TempDirectory directory;
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::StubJobExecutor>(output), logger, services.store, services.quota,
                       services.metrics);
  jobs.start();

  const auto imageId = std::get<std::string>(jobs.submitImage(imageRequest()));
  const auto image = waitFor(jobs, imageId, gen::JobState::Completed);
  ASSERT_TRUE(image.result.has_value());
  EXPECT_EQ(image.result->mediaType, "image");
  EXPECT_EQ(image.result->relativePath, "outputs/" + imageId + "/stub.txt");
  EXPECT_EQ(image.result->relativePath.find("UNIQUE_PROMPT_TOKEN_ZZZ"), std::string::npos);
  EXPECT_EQ(image.result->relativePath.find("UNIQUE_REQUEST_TOKEN_ZZZ"), std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / imageId / "stub.txt"));
  EXPECT_EQ((directory.outputs() / imageId).filename(), imageId);

  const auto videoId = std::get<std::string>(jobs.submitVideo(videoRequest()));
  const auto video = waitFor(jobs, videoId, gen::JobState::Completed);
  ASSERT_TRUE(video.result.has_value());
  EXPECT_EQ(video.result->mediaType, "video");
  EXPECT_EQ(video.result->relativePath, "outputs/" + videoId + "/stub.txt");
  EXPECT_NE(imageId, videoId);

  const auto& text = log.str();
  EXPECT_NE(text.find("event=job_accepted"), std::string::npos);
  EXPECT_NE(text.find("event=job_started"), std::string::npos);
  EXPECT_NE(text.find("event=job_completed"), std::string::npos);
  EXPECT_NE(text.find("request_id=UNIQUE_REQUEST_TOKEN_ZZZ"), std::string::npos);
  EXPECT_NE(text.find("job_id=" + imageId), std::string::npos);
  EXPECT_NE(text.find("prompt_length="), std::string::npos);
  EXPECT_EQ(text.find("UNIQUE_PROMPT_TOKEN_ZZZ"), std::string::npos);
}

TEST(JobManagerTest, StoresValidatedOptionsForTheWorker) {
  std::ostringstream log;
  gen::Logger logger(log);
  auto recorder = std::make_unique<RecordingExecutor>();
  auto* recorded = recorder.get();
  TestServices services(logger);
  gen::JobManager jobs(8, std::move(recorder), logger, services.store, services.quota, services.metrics);
  jobs.start();

  const auto imageId = std::get<std::string>(jobs.submitImage(imageRequest()));
  waitFor(jobs, imageId, gen::JobState::Completed);
  const auto videoId = std::get<std::string>(jobs.submitVideo(videoRequest()));
  waitFor(jobs, videoId, gen::JobState::Completed);

  ASSERT_EQ(recorded->seen.size(), 2U);
  const auto& image = std::get<gen::ImageOptions>(recorded->seen[0].options);
  EXPECT_EQ(image.width, 32);
  EXPECT_EQ(image.height, 48);
  EXPECT_EQ(image.seed, 42U);
  EXPECT_EQ(image.style, "stylized_3d");
  EXPECT_EQ(recorded->seen[0].prompt, "UNIQUE_PROMPT_TOKEN_ZZZ");

  const auto& video = std::get<gen::VideoOptions>(recorded->seen[1].options);
  EXPECT_EQ(video.fps, 12);
  EXPECT_EQ(video.durationSeconds, 2);
  EXPECT_EQ(video.seed, 7U);
  ASSERT_TRUE(video.action.has_value());
  EXPECT_EQ(*video.action, "walk_and_wave");
}

TEST(OutputStoreTest, RejectsIdsThatAreNotPathSafe) {
  TempDirectory directory;
  gen::OutputStore output(directory.outputs());
  EXPECT_THROW(output.writeStub("../etc", "image"), std::invalid_argument);
  EXPECT_THROW(output.writeStub("job/id", "image"), std::invalid_argument);
  EXPECT_THROW(output.writeStub("job-1", "png"), std::invalid_argument);
}
