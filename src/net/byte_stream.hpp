#pragma once

#include <sys/types.h>

#include <cstddef>

namespace gen {

class ByteStream {
 public:
  virtual ~ByteStream() = default;

  ByteStream() = default;
  ByteStream(const ByteStream&) = delete;
  ByteStream& operator=(const ByteStream&) = delete;

  virtual ssize_t read(char* data, std::size_t size) = 0;
  virtual ssize_t write(const char* data, std::size_t size) = 0;
  virtual void shutdown() noexcept = 0;
};

}  // namespace gen
