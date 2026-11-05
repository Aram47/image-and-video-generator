#pragma once

namespace gen {

class Socket {
 public:
  Socket() = default;
  explicit Socket(int fd) noexcept;
  ~Socket();

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  Socket(Socket&& other) noexcept;
  Socket& operator=(Socket&& other) noexcept;

  int fd() const noexcept;
  bool valid() const noexcept;
  void reset() noexcept;
  void setReuseAddress();
  void setNoSigPipe();
  void setCloexec();
  void setNonBlocking();
  void setBlocking();
  void setNoDelay();
  void shutdownBoth() noexcept;

 private:
  int fd_{-1};
};

}  // namespace gen
