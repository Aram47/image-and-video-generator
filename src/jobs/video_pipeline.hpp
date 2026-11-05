#pragma once

#include "jobs/job.hpp"
#include "media/video_encoder.hpp"
#include "render/irenderer.hpp"
#include "storage/output_store.hpp"

#include <memory>

namespace gen {

class VideoPipeline {
 public:
  VideoPipeline(OutputStore& output, std::unique_ptr<IRenderer> renderer, std::unique_ptr<IVideoEncoder> encoder,
                bool keepFramesOnEncodingFailure);

  JobResult execute(const Job& job);

 private:
  JobResult render(const VideoOptions& options, const SceneSpec& scene, const std::string& jobId, int frameCount) const;

  OutputStore& output_;
  std::unique_ptr<IRenderer> renderer_;
  std::unique_ptr<IVideoEncoder> encoder_;
  bool keepFramesOnEncodingFailure_;
};

}  // namespace gen
