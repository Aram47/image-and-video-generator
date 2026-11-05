#include "net/socket.hpp"

#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>

namespace gen {
namespace {

void requireOption(int fd, int level, int name) {
  const int enabled = 1;
  if (::setsockopt(fd, level, name, &enabled, sizeof(enabled)) != 0) {
    throw std::runtime_error("Failed to configure socket.");
  }
}

}  // namespace

Socket::Socket(int fd) noexcept : fd_(fd) {}

Socket::~Socket() { reset(); }

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Socket& Socket::operator=(Socket&& other) noexcept {
  if (this != &other) {
    reset();
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

int Socket::fd() const noexcept { return fd_; }

bool Socket::valid() const noexcept { return fd_ >= 0; }

void Socket::reset() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

void Socket::setReuseAddress() { requireOption(fd_, SOL_SOCKET, SO_REUSEADDR); }

void Socket::setNoSigPipe() {
#ifdef SO_NOSIGPIPE
  requireOption(fd_, SOL_SOCKET, SO_NOSIGPIPE);
#else
  throw std::runtime_error("Failed to configure socket.");
#endif
}

void Socket::setCloexec() {
  if (::fcntl(fd_, F_SETFD, FD_CLOEXEC) != 0) {
    throw std::runtime_error("Failed to configure socket.");
  }
}

void Socket::setNonBlocking() {
  const int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != 0) {
    throw std::runtime_error("Failed to configure socket.");
  }
}

void Socket::setBlocking() {
  const int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK) != 0) {
    throw std::runtime_error("Failed to configure socket.");
  }
}

void Socket::setNoDelay() { requireOption(fd_, IPPROTO_TCP, TCP_NODELAY); }

void Socket::shutdownBoth() noexcept {
  if (fd_ >= 0) {
    ::shutdown(fd_, SHUT_RDWR);
  }
}

}  // namespace gen
