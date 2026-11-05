#include "net/tls_stream.hpp"

#include <openssl/ssl.h>

#include <cerrno>
#include <utility>

namespace gen {
namespace {

ssize_t transfer(SSL* ssl, bool reading, char* data, std::size_t size) {
  if (size > static_cast<std::size_t>(0x7fffffff)) {
    size = 0x7fffffff;
  }
  for (;;) {
    const int count = reading ? SSL_read(ssl, data, static_cast<int>(size))
                              : SSL_write(ssl, data, static_cast<int>(size));
    if (count > 0) {
      return count;
    }
    const int reason = SSL_get_error(ssl, count);
    if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE) {
      continue;
    }
    if (reason == SSL_ERROR_ZERO_RETURN) {
      return 0;
    }
    if (reason == SSL_ERROR_SYSCALL && errno == EINTR) {
      continue;
    }
    return -1;
  }
}

}  // namespace

TlsStream::TlsStream(Socket socket, SSL* ssl) : socket_(std::move(socket)), ssl_(ssl) {}

TlsStream::~TlsStream() {
  if (ssl_ != nullptr) {
    SSL_free(ssl_);
    ssl_ = nullptr;
  }
}

ssize_t TlsStream::read(char* data, std::size_t size) { return transfer(ssl_, true, data, size); }

ssize_t TlsStream::write(const char* data, std::size_t size) {
  return transfer(ssl_, false, const_cast<char*>(data), size);
}

void TlsStream::shutdown() noexcept { socket_.shutdownBoth(); }

}  // namespace gen
