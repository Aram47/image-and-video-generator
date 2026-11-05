#include "common/logger.hpp"
#include "jobs/generation_executor.hpp"
#include "jobs/job_manager.hpp"
#include "storage/output_store.hpp"
#include "test_services.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>

namespace {

class TempDirectory {
 public:
  TempDirectory() {
    path_ = std::filesystem::temp_directory_path() /
            ("gen-image-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path_ / "outputs");
  }

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::permissions(outputs(), std::filesystem::perms::owner_all, std::filesystem::perm_options::replace,
                                 error);
    std::filesystem::remove_all(path_, error);
  }

  std::filesystem::path outputs() const { return path_ / "outputs"; }

 private:
  std::filesystem::path path_;
};

gen::ImageRequest imageRequest(std::uint32_t seed) {
  gen::ImageRequest request;
  request.requestId = "image-request";
  request.prompt = "A short man with black hair wearing a yellow shirt, standing";
  request.width = 48;
  request.height = 48;
  request.seed = seed;
  request.style = "stylized_3d";
  return request;
}

gen::JobSnapshot waitFor(gen::JobManager& jobs, const std::string& jobId, gen::JobState state) {
  for (int attempt = 0; attempt < 1000; ++attempt) {
    const auto result = jobs.status(jobId);
    if (const auto* snapshot = std::get_if<gen::JobSnapshot>(&result); snapshot && snapshot->state == state) {
      return *snapshot;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ADD_FAILURE() << "timed out waiting for job " << jobId;
  return {};
}

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

TEST(ImagePipelineTest, WritesAPngAndRepeatsItForTheSameSeed) {
  TempDirectory directory;
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::GenerationExecutor>(output), logger, services.store, services.quota,
                       services.metrics);
  jobs.start();

  const auto firstId = std::get<std::string>(jobs.submitImage(imageRequest(42)));
  const auto first = waitFor(jobs, firstId, gen::JobState::Completed);
  ASSERT_TRUE(first.result.has_value());
  EXPECT_EQ(first.result->mediaType, "image");
  EXPECT_EQ(first.result->relativePath, "outputs/" + firstId + "/image.png");
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / firstId / "image.png"));
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / firstId / "stub.txt"));

  auto secondRequest = imageRequest(42);
  secondRequest.requestId = "image-request-2";
  const auto secondId = std::get<std::string>(jobs.submitImage(secondRequest));
  const auto second = waitFor(jobs, secondId, gen::JobState::Completed);
  ASSERT_TRUE(second.result.has_value());
  EXPECT_EQ(readBytes(directory.outputs() / firstId / "image.png"),
            readBytes(directory.outputs() / secondId / "image.png"));
}

TEST(ImagePipelineTest, DifferentSeedWritesADifferentPng) {
  TempDirectory directory;
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::GenerationExecutor>(output), logger, services.store, services.quota,
                       services.metrics);
  jobs.start();

  const auto firstId = std::get<std::string>(jobs.submitImage(imageRequest(1)));
  auto secondRequest = imageRequest(2);
  secondRequest.requestId = "image-request-2";
  const auto secondId = std::get<std::string>(jobs.submitImage(secondRequest));
  ASSERT_EQ(waitFor(jobs, firstId, gen::JobState::Completed).state, gen::JobState::Completed);
  ASSERT_EQ(waitFor(jobs, secondId, gen::JobState::Completed).state, gen::JobState::Completed);
  EXPECT_NE(readBytes(directory.outputs() / firstId / "image.png"),
            readBytes(directory.outputs() / secondId / "image.png"));
}

TEST(ImagePipelineTest, ExamplePromptsProduceDifferentPictures) {
  TempDirectory directory;
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::GenerationExecutor>(output), logger, services.store, services.quota,
                       services.metrics);
  jobs.start();

  auto coat = imageRequest(12345);
  coat.prompt = "A tall woman with curly red hair wearing a blue coat, standing on a city street at sunset";
  auto shirt = imageRequest(12345);
  shirt.requestId = "image-request-2";
  shirt.prompt = "A short man with black hair wearing a yellow shirt, standing";
  const auto coatId = std::get<std::string>(jobs.submitImage(coat));
  const auto shirtId = std::get<std::string>(jobs.submitImage(shirt));
  ASSERT_EQ(waitFor(jobs, coatId, gen::JobState::Completed).state, gen::JobState::Completed);
  ASSERT_EQ(waitFor(jobs, shirtId, gen::JobState::Completed).state, gen::JobState::Completed);
  EXPECT_NE(readBytes(directory.outputs() / coatId / "image.png"),
            readBytes(directory.outputs() / shirtId / "image.png"));
}

TEST(ImagePipelineTest, SittingCompletesAndWritesAnImage) {
  TempDirectory directory;
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::GenerationExecutor>(output), logger, services.store, services.quota,
                       services.metrics);
  jobs.start();

  auto request = imageRequest(7);
  request.prompt = "a man sits";
  const auto jobId = std::get<std::string>(jobs.submitImage(request));
  const auto completed = waitFor(jobs, jobId, gen::JobState::Completed);
  ASSERT_TRUE(completed.result.has_value());
  EXPECT_EQ(completed.result->mediaType, "image");
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / jobId / "image.png"));
}

TEST(ImagePipelineTest, WalkingAndWavingWriteAnImage) {
  TempDirectory directory;
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::GenerationExecutor>(output), logger, services.store, services.quota,
                       services.metrics);
  jobs.start();

  auto walking = imageRequest(7);
  walking.prompt = "a man walking";
  const auto walkingId = std::get<std::string>(jobs.submitImage(walking));
  const auto walkingDone = waitFor(jobs, walkingId, gen::JobState::Completed);
  ASSERT_TRUE(walkingDone.result.has_value());
  EXPECT_EQ(walkingDone.result->mediaType, "image");
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / walkingId / "image.png"));
  EXPECT_FALSE(walkingDone.result->frameCount.has_value());

  auto waving = imageRequest(7);
  waving.requestId = "image-request-2";
  waving.prompt = "a man waves";
  const auto wavingId = std::get<std::string>(jobs.submitImage(waving));
  const auto wavingDone = waitFor(jobs, wavingId, gen::JobState::Completed);
  ASSERT_TRUE(wavingDone.result.has_value());
  EXPECT_EQ(wavingDone.result->mediaType, "image");
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / wavingId / "image.png"));

  auto both = imageRequest(7);
  both.requestId = "image-request-3";
  both.prompt = "a man walks and waves";
  const auto bothId = std::get<std::string>(jobs.submitImage(both));
  const auto bothDone = waitFor(jobs, bothId, gen::JobState::Completed);
  ASSERT_TRUE(bothDone.result.has_value());
  EXPECT_EQ(bothDone.result->mediaType, "image");
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / bothId / "image.png"));
}

TEST(ImagePipelineTest, ReadOnlyDirectoryFailsWithoutCompleting) {
  TempDirectory directory;
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::GenerationExecutor>(output), logger, services.store, services.quota,
                       services.metrics);
  jobs.start();

  std::filesystem::permissions(directory.outputs(),
                               std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::replace);

  const auto jobId = std::get<std::string>(jobs.submitImage(imageRequest(9)));
  const auto failed = waitFor(jobs, jobId, gen::JobState::Failed);
  EXPECT_EQ(failed.errorCode, "RENDER_FAILED");
  EXPECT_EQ(failed.errorMessage, "Failed to write the image.");
  EXPECT_EQ(failed.errorMessage.find(directory.outputs().string()), std::string::npos);
  EXPECT_FALSE(failed.result.has_value());
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / jobId / "image.png"));
  EXPECT_NE(log.str().find(directory.outputs().string()), std::string::npos);
  EXPECT_NE(log.str().find("code=RENDER_FAILED"), std::string::npos);
}
