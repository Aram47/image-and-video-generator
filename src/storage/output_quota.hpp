#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>

namespace gen {

class OutputQuota {
 public:
  OutputQuota(std::filesystem::path root, std::uint64_t limitBytes);

  OutputQuota(const OutputQuota&) = delete;
  OutputQuota& operator=(const OutputQuota&) = delete;

  void scan();
  bool exceeded() const;
  void add(std::uint64_t bytes);

 private:
  std::uint64_t measure() const;

  std::filesystem::path root_;
  std::uint64_t limitBytes_;
  mutable std::mutex mutex_;
  std::uint64_t usedBytes_{0};
};

}  // namespace gen
