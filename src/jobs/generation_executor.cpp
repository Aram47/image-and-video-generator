#include "jobs/generation_executor.hpp"

#include "render/cpu_rasterizer.hpp"

#include <utility>

namespace gen {

GenerationExecutor::GenerationExecutor(OutputStore& output, bool keepFramesOnEncodingFailure)
    : GenerationExecutor(output, makeVideoEncoder(), keepFramesOnEncodingFailure) {}

GenerationExecutor::GenerationExecutor(OutputStore& output, std::unique_ptr<IVideoEncoder> encoder,
                                       bool keepFramesOnEncodingFailure)
    : image_(output, std::make_unique<CpuRasterizer>()),
      video_(output, std::make_unique<CpuRasterizer>(), std::move(encoder), keepFramesOnEncodingFailure) {}

JobResult GenerationExecutor::execute(const Job& job) {
  if (job.type == JobType::Image) {
    return image_.execute(job);
  }
  return video_.execute(job);
}

}  // namespace gen
