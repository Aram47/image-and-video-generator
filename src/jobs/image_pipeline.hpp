#pragma once

#include "jobs/job.hpp"
#include "render/irenderer.hpp"
#include "storage/output_store.hpp"

#include <memory>
#include <string>

namespace gen {

class ImagePipeline {
 public:
  ImagePipeline(OutputStore& output, std::unique_ptr<IRenderer> renderer);

  JobResult execute(const Job& job);

 private:
  JobResult render(const ImageOptions& options, const SceneSpec& scene, const std::string& jobId) const;

  OutputStore& output_;
  std::unique_ptr<IRenderer> renderer_;
};

}  // namespace gen
