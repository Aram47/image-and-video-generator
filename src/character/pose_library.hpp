#pragma once

#include "common/geometry.hpp"

namespace gen {

struct Pose {
  Vec3 root{};
  float spine{0};
  float shoulderLeft{0};
  float elbowLeft{0};
  float shoulderRight{0};
  float elbowRight{0};
  float hipLeft{0};
  float kneeLeft{0};
  float hipRight{0};
  float kneeRight{0};

  bool operator==(const Pose&) const = default;
};

class PoseLibrary {
 public:
  static Pose stand();
  static Pose sit();
  static Pose walkContact();
  static Pose walkPassing();
  static Pose waveLowered();
  static Pose waveRaised();
};

}  // namespace gen
