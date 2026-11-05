#pragma once

#include "media/video_encoder.hpp"

namespace gen {

class LibavVideoEncoder final : public IVideoEncoder {
 public:
  void encode(const std::vector<std::filesystem::path>& frames, int fps, const std::filesystem::path& output) override;
};

}  // namespace gen
