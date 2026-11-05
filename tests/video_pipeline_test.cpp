#include "animation/action_library.hpp"
#include "common/logger.hpp"
#include "jobs/generation_executor.hpp"
#include "jobs/job_manager.hpp"
#include "jobs/pipeline_failure.hpp"
#include "jobs/scene_composer.hpp"
#include "jobs/video_pipeline.hpp"
#include "prompt/prompt_parser.hpp"
#include "render/cpu_rasterizer.hpp"
#include "render/framebuffer.hpp"
#include "test_services.hpp"

#if GEN_HAS_FFMPEG
#include "media/libav_video_encoder.hpp"

extern "C" {
#include <libavformat/avformat.h>
}
#endif

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <variant>

namespace {

class TempDirectory {
 public:
  explicit TempDirectory(std::string name) {
    path_ = std::filesystem::temp_directory_path() /
            (name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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

class FlagEncoder final : public gen::IVideoEncoder {
 public:
  bool fail{false};
  int calls{0};

  void encode(const std::vector<std::filesystem::path>&, int, const std::filesystem::path& output) override {
    ++calls;
    if (fail) {
      throw gen::VideoEncodingError(7);
    }
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    const char header[] = {'\0', '\0', '\0', '\x18', 'f', 't', 'y', 'p', 'i', 's', 'o', 'm'};
    file.write(header, sizeof header);
  }
};

gen::SceneSpec parseScene(std::string_view prompt, std::string_view action, std::uint32_t seed) {
  const auto parsed = gen::PromptParser{}.parse(prompt, gen::PromptTarget::Video, action, seed);
  const auto* spec = std::get_if<gen::SceneSpec>(&parsed);
  EXPECT_NE(spec, nullptr);
  return spec == nullptr ? gen::SceneSpec{} : *spec;
}

gen::VideoOptions videoOptions(int fps, int duration, int size = 16) {
  gen::VideoOptions options;
  options.width = size;
  options.height = size;
  options.fps = fps;
  options.durationSeconds = duration;
  options.seed = 12345;
  options.style = "stylized_3d";
  options.action = "walk";
  return options;
}

gen::Job videoJob(std::string id, gen::VideoOptions options, gen::SceneSpec scene) {
  gen::Job job;
  job.id = std::move(id);
  job.requestId = "video-request";
  job.type = gen::JobType::Video;
  job.prompt = "a man walks";
  job.options = std::move(options);
  job.scene = std::move(scene);
  return job;
}

std::uint64_t hashRgba(const std::vector<std::uint8_t>& rgba) {
  std::uint64_t hash = 14695981039346656037ull;
  for (const std::uint8_t byte : rgba) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::uint64_t frameHash(const gen::SceneSpec& spec, int frame, int fps, int size) {
  const gen::PreparedScene prepared = gen::SceneComposer{}.prepare(spec);
  const gen::Pose pose = gen::AnimationController{}.poseAt(spec.action->value, frame, fps);
  const gen::ComposedFrame composed = gen::SceneComposer{}.compose(prepared, pose);
  gen::Framebuffer framebuffer(size, size);
  framebuffer.clear(composed.scene.background);
  gen::CpuRasterizer{}.render(composed.scene, composed.camera, framebuffer);
  return hashRgba(framebuffer.rgba());
}

std::string readText(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
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

}  // namespace

TEST(VideoPipelineTest, RejectsMoreThan240FramesBeforeEncoding) {
  TempDirectory directory("gen-video-limit");
  gen::OutputStore output(directory.outputs());
  auto encoder = std::make_unique<FlagEncoder>();
  FlagEncoder* raw = encoder.get();
  gen::VideoPipeline pipeline(output, std::make_unique<gen::CpuRasterizer>(), std::move(encoder), false);
  gen::SceneSpec scene;
  scene.action = gen::ExplicitAttribute<gen::ActionId>{gen::ActionId::Walk, true};

  try {
    pipeline.execute(videoJob("limit-job", videoOptions(241, 1), scene));
    FAIL() << "expected a frame-limit failure";
  } catch (const gen::PipelineFailure& failure) {
    EXPECT_EQ(failure.code(), gen::ErrorCode::ResourceLimitExceeded);
    EXPECT_EQ(failure.clientMessage(), "Frame count exceeds the limit of 240.");
  }
  EXPECT_EQ(raw->calls, 0);
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / "limit-job" / "video.mp4"));
}

TEST(VideoPipelineTest, Accepts240FramesAndRemovesThemAfterEncoding) {
  TempDirectory directory("gen-video-max");
  gen::OutputStore output(directory.outputs());
  auto encoder = std::make_unique<FlagEncoder>();
  gen::VideoPipeline pipeline(output, std::make_unique<gen::CpuRasterizer>(), std::move(encoder), false);
  const gen::SceneSpec scene = parseScene("a man walks", "walk", 12345);

  const gen::JobResult result = pipeline.execute(videoJob("max-job", videoOptions(30, 8), scene));
  EXPECT_EQ(result.mediaType, "video");
  EXPECT_EQ(result.relativePath, "outputs/max-job/video.mp4");
  EXPECT_EQ(result.frameCount, 240);
  EXPECT_EQ(result.actionName, "walk");
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / "max-job" / "frames"));
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / "max-job" / "video.mp4"));

  const auto metadata = nlohmann::json::parse(readText(directory.outputs() / "max-job" / "metadata.json"));
  EXPECT_EQ(metadata["seed"], 12345);
  EXPECT_EQ(metadata["frame_count"], 240);
  EXPECT_EQ(metadata["fps"], 30);
  EXPECT_EQ(metadata["width"], 16);
  EXPECT_EQ(metadata["height"], 16);
  EXPECT_EQ(metadata["format"], "mp4");
  EXPECT_EQ(metadata["action"], "walk");
  EXPECT_NE(readText(directory.outputs() / "max-job" / "video.mp4").find("ftyp"), std::string::npos);
}

TEST(VideoPipelineTest, EncodingFailureDoesNotCompleteAndDropsFramesByDefault) {
  TempDirectory directory("gen-video-fail");
  gen::OutputStore output(directory.outputs());
  auto encoder = std::make_unique<FlagEncoder>();
  encoder->fail = true;
  gen::VideoPipeline pipeline(output, std::make_unique<gen::CpuRasterizer>(), std::move(encoder), false);
  const gen::SceneSpec scene = parseScene("a man waves", "wave", 3);

  try {
    pipeline.execute(videoJob("fail-job", videoOptions(2, 1), scene));
    FAIL() << "expected an encoding failure";
  } catch (const gen::PipelineFailure& failure) {
    EXPECT_EQ(failure.code(), gen::ErrorCode::EncodingFailed);
    EXPECT_EQ(failure.clientMessage(), "Failed to encode the video.");
    EXPECT_EQ(failure.clientMessage().find("frames"), std::string::npos);
    EXPECT_EQ(failure.clientMessage().find("ffmpeg"), std::string::npos);
    EXPECT_NE(std::string(failure.what()).find("encoder_status=7"), std::string::npos);
  }
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / "fail-job" / "frames"));
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / "fail-job" / "video.mp4"));
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / "fail-job" / "metadata.json"));
}

TEST(VideoPipelineTest, EncodingFailureKeepsFramesWhenConfigured) {
  TempDirectory directory("gen-video-keep");
  gen::OutputStore output(directory.outputs());
  auto encoder = std::make_unique<FlagEncoder>();
  encoder->fail = true;
  gen::VideoPipeline pipeline(output, std::make_unique<gen::CpuRasterizer>(), std::move(encoder), true);
  const gen::SceneSpec scene = parseScene("a man waves", "wave", 3);

  EXPECT_THROW(pipeline.execute(videoJob("keep-job", videoOptions(2, 1), scene)), gen::PipelineFailure);
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / "keep-job" / "frames" / "frame_0000.png"));
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / "keep-job" / "frames" / "frame_0001.png"));
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / "keep-job" / "video.mp4"));
}

TEST(VideoPipelineTest, ImageDoesNotEncodeAndVideoIsNotAnImage) {
  TempDirectory directory("gen-video-branch");
  gen::OutputStore output(directory.outputs());
  auto encoder = std::make_unique<FlagEncoder>();
  FlagEncoder* raw = encoder.get();
  gen::GenerationExecutor executor(output, std::move(encoder));

  gen::Job image;
  image.id = "image-job";
  image.type = gen::JobType::Image;
  image.prompt = "a man standing";
  gen::ImageOptions imageOptions;
  imageOptions.width = 16;
  imageOptions.height = 16;
  imageOptions.seed = 5;
  imageOptions.style = "stylized_3d";
  image.options = imageOptions;
  const auto parsed = gen::PromptParser{}.parse(image.prompt, gen::PromptTarget::Image, std::nullopt, 5);
  image.scene = std::get<gen::SceneSpec>(parsed);

  const gen::JobResult imageResult = executor.execute(image);
  EXPECT_EQ(raw->calls, 0);
  EXPECT_EQ(imageResult.mediaType, "image");
  EXPECT_FALSE(imageResult.frameCount.has_value());

  const gen::JobResult videoResult =
      executor.execute(videoJob("branch-job", videoOptions(2, 1), parseScene("a man walks", "walk", 5)));
  EXPECT_EQ(raw->calls, 1);
  EXPECT_EQ(videoResult.mediaType, "video");
  EXPECT_NE(videoResult.mediaType, "image");
}

TEST(VideoPipelineTest, UnknownActionFailsBeforeTheEncoder) {
  TempDirectory directory("gen-video-action");
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  auto encoder = std::make_unique<FlagEncoder>();
  FlagEncoder* raw = encoder.get();
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::GenerationExecutor>(output, std::move(encoder)), logger, services.store,
                       services.quota, services.metrics);
  jobs.start();

  gen::VideoRequest request;
  request.requestId = "video-request";
  request.prompt = "a man walks";
  request.width = 16;
  request.height = 16;
  request.fps = 2;
  request.durationSeconds = 1;
  request.seed = 9;
  request.style = "stylized_3d";
  request.action = "fly";
  const auto jobId = std::get<std::string>(jobs.submitVideo(request));
  const auto failed = waitFor(jobs, jobId, gen::JobState::Failed);
  EXPECT_EQ(failed.errorCode, "UNSUPPORTED_ACTION");
  EXPECT_EQ(failed.state, gen::JobState::Failed);
  EXPECT_FALSE(failed.result.has_value());
  EXPECT_EQ(raw->calls, 0);
}

TEST(VideoPipelineTest, FailedEncoderMarksTheJobFailed) {
  TempDirectory directory("gen-video-status");
  std::ostringstream log;
  gen::Logger logger(log);
  gen::OutputStore output(directory.outputs());
  auto encoder = std::make_unique<FlagEncoder>();
  encoder->fail = true;
  TestServices services(logger);
  gen::JobManager jobs(8, std::make_unique<gen::GenerationExecutor>(output, std::move(encoder)), logger, services.store,
                       services.quota, services.metrics);
  jobs.start();

  gen::VideoRequest request;
  request.requestId = "video-request";
  request.prompt = "a man walks";
  request.width = 16;
  request.height = 16;
  request.fps = 2;
  request.durationSeconds = 1;
  request.seed = 9;
  request.style = "stylized_3d";
  request.action = "walk";
  const auto jobId = std::get<std::string>(jobs.submitVideo(request));
  const auto failed = waitFor(jobs, jobId, gen::JobState::Failed);
  EXPECT_EQ(failed.state, gen::JobState::Failed);
  EXPECT_EQ(failed.errorCode, "ENCODING_FAILED");
  EXPECT_EQ(failed.errorMessage, "Failed to encode the video.");
  EXPECT_FALSE(failed.result.has_value());
  EXPECT_FALSE(std::filesystem::exists(directory.outputs() / jobId / "frames"));
}

TEST(VideoPipelineTest, RepeatedFramesMatchAndAWalkChangesThePark) {
  const gen::SceneSpec example = parseScene(
      "A young man in a green jacket walks through a park and waves at the camera", "walk_and_wave", 12345);
  constexpr int fps = 24;
  const int frames[] = {0, 48, 95};
  for (const int frame : frames) {
    EXPECT_EQ(frameHash(example, frame, fps, 32), frameHash(example, frame, fps, 32)) << frame;
  }

  const gen::SceneSpec park = parseScene("a man walks through a park", "walk", 12345);
  EXPECT_NE(frameHash(park, 0, 1, 64), frameHash(park, 4, 1, 64));
}

#if GEN_HAS_FFMPEG
TEST(VideoPipelineTest, LibavContainerDurationMatchesTheFrameCount) {
  TempDirectory directory("gen-video-libav");
  gen::OutputStore output(directory.outputs());
  gen::VideoPipeline pipeline(output, std::make_unique<gen::CpuRasterizer>(), std::make_unique<gen::LibavVideoEncoder>(),
                              false);
  const gen::SceneSpec scene = parseScene("a man walks and waves", "walk_and_wave", 12345);
  const gen::JobResult result = pipeline.execute(videoJob("libav-job", videoOptions(4, 1), scene));
  const auto path = directory.outputs() / "libav-job" / "video.mp4";
  EXPECT_EQ(result.frameCount, 4);
  EXPECT_NE(readText(path).find("ftyp"), std::string::npos);

  AVFormatContext* format = nullptr;
  ASSERT_EQ(avformat_open_input(&format, path.string().c_str(), nullptr, nullptr), 0);
  ASSERT_GE(avformat_find_stream_info(format, nullptr), 0);
  const int streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  ASSERT_GE(streamIndex, 0);
  const AVStream* stream = format->streams[streamIndex];
  double duration = 0.0;
  if (stream->duration > 0) {
    duration = stream->duration * av_q2d(stream->time_base);
  } else if (format->duration > 0) {
    duration = static_cast<double>(format->duration) / AV_TIME_BASE;
  }
  avformat_close_input(&format);
  EXPECT_NEAR(duration, 4.0 / 4.0, 1.0 / 4.0);
}
#endif
