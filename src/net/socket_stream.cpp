#include "net/socket_stream.hpp"

#include <sys/socket.h>

#include <cerrno>
#include <utility>

namespace gen {

SocketStream::SocketStream(Socket socket) : socket_(std::move(socket)) {}

ssize_t SocketStream::read(char* data, std::size_t size) {
  for (;;) {
    const ssize_t count = ::recv(socket_.fd(), data, size, 0);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    return count;
  }
}

ssize_t SocketStream::write(const char* data, std::size_t size) {
  for (;;) {
    const ssize_t sent = ::send(socket_.fd(), data, size, 0);
    if (sent < 0 && errno == EINTR) {
      continue;
    }
    return sent;
  }
}

void SocketStream::shutdown() noexcept { socket_.shutdownBoth(); }

}  // namespace gen
