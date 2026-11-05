#pragma once

#include "render/irenderer.hpp"

namespace gen {

class CpuRasterizer final : public IRenderer {
 public:
  void render(const Scene& scene, const Camera& camera, Framebuffer& framebuffer) override;
};

}  // namespace gen
