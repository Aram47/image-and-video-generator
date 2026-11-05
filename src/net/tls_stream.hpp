#pragma once

#include "net/byte_stream.hpp"
#include "net/socket.hpp"

#include <openssl/ssl.h>

namespace gen {

class TlsStream final : public ByteStream {
 public:
  TlsStream(Socket socket, SSL* ssl);
  ~TlsStream() override;

  ssize_t read(char* data, std::size_t size) override;
  ssize_t write(const char* data, std::size_t size) override;
  void shutdown() noexcept override;

 private:
  Socket socket_;
  SSL* ssl_;
};

}  // namespace gen
