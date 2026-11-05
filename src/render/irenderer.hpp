#pragma once

#include "render/camera.hpp"
#include "render/framebuffer.hpp"
#include "render/scene.hpp"

namespace gen {

class IRenderer {
 public:
  virtual ~IRenderer() = default;

  IRenderer() = default;
  IRenderer(const IRenderer&) = delete;
  IRenderer& operator=(const IRenderer&) = delete;

  virtual void render(const Scene& scene, const Camera& camera, Framebuffer& framebuffer) = 0;
};

}  // namespace gen
