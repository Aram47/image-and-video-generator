#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace gen {

enum class FrameKind { Message, TooLarge };

struct Frame {
  FrameKind kind{FrameKind::Message};
  std::string payload;
};

class NdjsonFramer {
 public:
  explicit NdjsonFramer(std::size_t maxMessageBytes);

  std::vector<Frame> append(std::string_view chunk);

 private:
  std::size_t maxMessageBytes_;
  std::string buffer_;
  bool discarding_{false};
};

}  // namespace gen
