#include "render/camera.hpp"

#include "character/body_metrics.hpp"
#include "character/proportions.hpp"

#include <algorithm>
#include <cmath>

namespace gen {

Camera Camera::frameCharacter(Height height) {
  constexpr float kFovDegrees = 40.0f;
  constexpr float kMargin = 1.2f;
  constexpr float kDegreesToRadians = 0.017453292519943295f;
  const float scale = heightScale(height);
  const float fov = kFovDegrees * kDegreesToRadians;
  const float figureHeight = BodyMetrics::height * scale;
  const float torsoCenterY = BodyMetrics::torsoCenterY * scale;
  const float maxOffset = std::max(torsoCenterY, figureHeight - torsoCenterY);
  const float distance = (maxOffset * kMargin) / std::tan(fov * 0.5f);

  Camera camera;
  camera.position = Vec3{0.0f, torsoCenterY, distance};
  camera.target = Vec3{0.0f, torsoCenterY, 0.0f};
  camera.verticalFovRadians = fov;
  return camera;
}

Camera Camera::defaultPortrait() { return frameCharacter(Height::Average); }

Camera Camera::follow(const Camera& base, const Vec3& root) {
  Camera camera = base;
  camera.position.x += root.x;
  camera.position.z += root.z;
  camera.target.x += root.x;
  camera.target.z += root.z;
  return camera;
}

}  // namespace gen
