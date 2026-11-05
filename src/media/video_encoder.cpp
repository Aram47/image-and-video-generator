#include "media/video_encoder.hpp"

#if GEN_HAS_FFMPEG
#include "media/libav_video_encoder.hpp"
#endif

#include <utility>

namespace gen {

VideoEncodingError::VideoEncodingError(int status) : std::runtime_error("encoder failed"), status_(status) {}

void NullVideoEncoder::encode(const std::vector<std::filesystem::path>&, int, const std::filesystem::path&) {
  throw VideoEncodingError(-1);
}

std::unique_ptr<IVideoEncoder> makeVideoEncoder() {
#if GEN_HAS_FFMPEG
  return std::make_unique<LibavVideoEncoder>();
#else
  return std::make_unique<NullVideoEncoder>();
#endif
}

}  // namespace gen
