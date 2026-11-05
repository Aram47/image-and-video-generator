#include "jobs/video_pipeline.hpp"

#include "animation/action_library.hpp"
#include "jobs/pipeline_failure.hpp"
#include "jobs/scene_composer.hpp"
#include "media/image_writer.hpp"
#include "render/framebuffer.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace gen {
namespace {

constexpr int kMaxFrames = 240;

std::string frameName(int index) {
  std::ostringstream name;
  name << "frame_" << std::setw(4) << std::setfill('0') << index << ".png";
  return name.str();
}

void discardFrames(const OutputStore& output, const std::string& jobId) {
  try {
    output.removeDirectory(jobId, "frames");
  } catch (...) {
  }
}

void discardFile(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

}  // namespace

VideoPipeline::VideoPipeline(OutputStore& output, std::unique_ptr<IRenderer> renderer,
                             std::unique_ptr<IVideoEncoder> encoder, bool keepFramesOnEncodingFailure)
    : output_(output),
      renderer_(std::move(renderer)),
      encoder_(std::move(encoder)),
      keepFramesOnEncodingFailure_(keepFramesOnEncodingFailure) {
  if (!renderer_) {
    throw std::invalid_argument("Renderer is required.");
  }
  if (!encoder_) {
    throw std::invalid_argument("Video encoder is required.");
  }
}

JobResult VideoPipeline::execute(const Job& job) {
  const auto* options = std::get_if<VideoOptions>(&job.options);
  if (options == nullptr || !job.scene || !job.scene->action) {
    throw PipelineFailure(ErrorCode::RenderFailed, "Failed to render the video.", "Video job is incomplete.");
  }

  const auto frameCount64 =
      static_cast<long long>(options->fps) * static_cast<long long>(options->durationSeconds);
  if (frameCount64 <= 0 || frameCount64 > kMaxFrames) {
    throw PipelineFailure(ErrorCode::ResourceLimitExceeded, "Frame count exceeds the limit of 240.",
                          "frame_count=" + std::to_string(frameCount64));
  }
  return render(*options, *job.scene, job.id, static_cast<int>(frameCount64));
}

JobResult VideoPipeline::render(const VideoOptions& options, const SceneSpec& scene, const std::string& jobId,
                                int frameCount) const {
  const PreparedScene prepared = SceneComposer{}.prepare(scene);
  const ActionId action = scene.action->value;
  AnimationController animation;
  std::vector<std::filesystem::path> frames;
  frames.reserve(static_cast<std::size_t>(frameCount));

  try {
    for (int index = 0; index < frameCount; ++index) {
      const Pose pose = animation.poseAt(action, index, options.fps);
      const ComposedFrame composed = SceneComposer{}.compose(prepared, pose);
      Framebuffer framebuffer(options.width, options.height);
      framebuffer.clear(composed.scene.background);
      renderer_->render(composed.scene, composed.camera, framebuffer);
      const StoredFile stored = output_.allocateNested(jobId, "frames", frameName(index));
      ImageWriter{}.writePng(framebuffer, stored.absolute);
      frames.push_back(stored.absolute);
    }
  } catch (const PipelineFailure&) {
    discardFrames(output_, jobId);
    throw;
  } catch (const std::exception& error) {
    discardFrames(output_, jobId);
    throw PipelineFailure(ErrorCode::RenderFailed, "Failed to render the video.", error.what());
  } catch (...) {
    discardFrames(output_, jobId);
    throw PipelineFailure(ErrorCode::RenderFailed, "Failed to render the video.", "Video rendering failed.");
  }

  const StoredFile video = output_.allocate(jobId, "video.mp4");
  try {
    encoder_->encode(frames, options.fps, video.absolute);
  } catch (const VideoEncodingError& error) {
    discardFile(video.absolute);
    if (!keepFramesOnEncodingFailure_) {
      discardFrames(output_, jobId);
    }
    throw PipelineFailure(ErrorCode::EncodingFailed, "Failed to encode the video.",
                          "encoder_status=" + std::to_string(error.status()));
  } catch (...) {
    discardFile(video.absolute);
    if (!keepFramesOnEncodingFailure_) {
      discardFrames(output_, jobId);
    }
    throw PipelineFailure(ErrorCode::EncodingFailed, "Failed to encode the video.", "encoder_status=unknown");
  }

  const StoredFile metadataFile = output_.allocate(jobId, "metadata.json");
  const nlohmann::json metadata = {{"seed", options.seed},
                                   {"frame_count", frameCount},
                                   {"fps", options.fps},
                                   {"width", options.width},
                                   {"height", options.height},
                                   {"format", "mp4"},
                                   {"action", std::string(toString(action))}};
  {
    std::ofstream output(metadataFile.absolute, std::ios::binary | std::ios::trunc);
    if (!output) {
      discardFile(video.absolute);
      discardFrames(output_, jobId);
      throw PipelineFailure(ErrorCode::RenderFailed, "Failed to render the video.", "Failed to write metadata.");
    }
    output << metadata.dump(2) << '\n';
    if (!output) {
      discardFile(video.absolute);
      discardFrames(output_, jobId);
      throw PipelineFailure(ErrorCode::RenderFailed, "Failed to render the video.", "Failed to write metadata.");
    }
  }

  discardFrames(output_, jobId);
  JobResult result;
  result.mediaType = "video";
  result.relativePath = video.relative;
  result.frameCount = frameCount;
  result.actionName = std::string(toString(action));
  std::error_code error;
  const auto size = std::filesystem::file_size(video.absolute, error);
  if (!error) {
    result.bytes = static_cast<std::uint64_t>(size);
  }
  return result;
}

}  // namespace gen
