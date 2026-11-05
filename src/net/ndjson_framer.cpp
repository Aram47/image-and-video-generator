#include "net/ndjson_framer.hpp"

namespace gen {

NdjsonFramer::NdjsonFramer(std::size_t maxMessageBytes) : maxMessageBytes_(maxMessageBytes) {}

std::vector<Frame> NdjsonFramer::append(std::string_view chunk) {
  std::vector<Frame> frames;
  for (const char character : chunk) {
    if (discarding_) {
      if (character == '\n') {
        discarding_ = false;
      }
      continue;
    }
    if (character == '\n') {
      Frame frame;
      frame.kind = FrameKind::Message;
      frame.payload = std::move(buffer_);
      buffer_.clear();
      frames.push_back(std::move(frame));
      continue;
    }
    if (buffer_.size() >= maxMessageBytes_) {
      buffer_.clear();
      discarding_ = true;
      Frame frame;
      frame.kind = FrameKind::TooLarge;
      frames.push_back(std::move(frame));
      continue;
    }
    buffer_.push_back(character);
  }
  return frames;
}

}  // namespace gen
