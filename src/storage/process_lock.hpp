#pragma once

#include <filesystem>

namespace gen {

class ProcessLock {
 public:
  explicit ProcessLock(std::filesystem::path path);
  ~ProcessLock();

  ProcessLock(const ProcessLock&) = delete;
  ProcessLock& operator=(const ProcessLock&) = delete;

 private:
  int fd_{-1};
};

}  // namespace gen
