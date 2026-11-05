#include "jobs/image_pipeline.hpp"

#include "jobs/pipeline_failure.hpp"
#include "jobs/scene_composer.hpp"
#include "media/image_writer.hpp"
#include "render/framebuffer.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace gen {

ImagePipeline::ImagePipeline(OutputStore& output, std::unique_ptr<IRenderer> renderer)
    : output_(output), renderer_(std::move(renderer)) {
  if (!renderer_) {
    throw std::invalid_argument("Renderer is required.");
  }
}

JobResult ImagePipeline::execute(const Job& job) {
  const auto* options = std::get_if<ImageOptions>(&job.options);
  if (options == nullptr || !job.scene) {
    throw PipelineFailure(ErrorCode::RenderFailed, "Failed to write the image.", "Image job is incomplete.");
  }

  try {
    return render(*options, *job.scene, job.id);
  } catch (const PipelineFailure&) {
    throw;
  } catch (const std::exception& error) {
    throw PipelineFailure(ErrorCode::RenderFailed, "Failed to write the image.", error.what());
  } catch (...) {
    throw PipelineFailure(ErrorCode::RenderFailed, "Failed to write the image.", "Image rendering failed.");
  }
}

JobResult ImagePipeline::render(const ImageOptions& options, const SceneSpec& scene, const std::string& jobId) const {
  const ComposedFrame frame = SceneComposer{}.compose(scene);

  Framebuffer framebuffer(options.width, options.height);
  framebuffer.clear(frame.scene.background);
  renderer_->render(frame.scene, frame.camera, framebuffer);

  const StoredFile stored = output_.allocate(jobId, "image.png");
  ImageWriter writer;
  writer.writePng(framebuffer, stored.absolute);
  JobResult result{"image", stored.relative};
  std::error_code error;
  const auto size = std::filesystem::file_size(stored.absolute, error);
  if (!error) {
    result.bytes = static_cast<std::uint64_t>(size);
  }
  return result;
}

}  // namespace gen
