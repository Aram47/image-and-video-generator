#pragma once

#include "jobs/ijob_executor.hpp"
#include "jobs/image_pipeline.hpp"
#include "jobs/video_pipeline.hpp"
#include "media/video_encoder.hpp"

#include <memory>

namespace gen {

class GenerationExecutor final : public IJobExecutor {
 public:
  explicit GenerationExecutor(OutputStore& output, bool keepFramesOnEncodingFailure = false);
  GenerationExecutor(OutputStore& output, std::unique_ptr<IVideoEncoder> encoder, bool keepFramesOnEncodingFailure = false);

  JobResult execute(const Job& job) override;

 private:
  ImagePipeline image_;
  VideoPipeline video_;
};

}  // namespace gen
