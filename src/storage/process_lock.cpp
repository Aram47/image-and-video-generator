#include "storage/process_lock.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <stdexcept>

namespace gen {

ProcessLock::ProcessLock(std::filesystem::path path) {
  if (path.empty()) {
    throw std::invalid_argument("Server lock path is required.");
  }
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
      throw std::runtime_error("Failed to lock the server.");
    }
  }

  fd_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  if (fd_ < 0) {
    throw std::runtime_error("Failed to lock the server.");
  }
  if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
    const int failure = errno;
    ::close(fd_);
    fd_ = -1;
    if (failure == EWOULDBLOCK || failure == EAGAIN) {
      throw std::runtime_error("Server is already running.");
    }
    throw std::runtime_error("Failed to lock the server.");
  }
}

ProcessLock::~ProcessLock() {
  if (fd_ < 0) {
    return;
  }
  ::flock(fd_, LOCK_UN);
  ::close(fd_);
  fd_ = -1;
}

}  // namespace gen
