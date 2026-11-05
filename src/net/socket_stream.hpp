#pragma once

#include "net/byte_stream.hpp"
#include "net/socket.hpp"

namespace gen {

class SocketStream final : public ByteStream {
 public:
  explicit SocketStream(Socket socket);

  ssize_t read(char* data, std::size_t size) override;
  ssize_t write(const char* data, std::size_t size) override;
  void shutdown() noexcept override;

 private:
  Socket socket_;
};

}  // namespace gen
