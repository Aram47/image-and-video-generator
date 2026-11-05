#pragma once

#include "net/byte_stream.hpp"
#include "net/socket.hpp"

#include <filesystem>
#include <memory>

namespace gen {

class TlsContext {
 public:
  TlsContext(const std::filesystem::path& certificate, const std::filesystem::path& key);
  ~TlsContext();

  TlsContext(const TlsContext&) = delete;
  TlsContext& operator=(const TlsContext&) = delete;

  std::unique_ptr<ByteStream> accept(Socket socket);

 private:
  struct Context;
  std::unique_ptr<Context> context_;
};

}  // namespace gen
