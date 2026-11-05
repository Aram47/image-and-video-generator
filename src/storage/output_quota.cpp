#include "storage/output_quota.hpp"

namespace gen {

OutputQuota::OutputQuota(std::filesystem::path root, std::uint64_t limitBytes)
    : root_(std::move(root)), limitBytes_(limitBytes) {
  scan();
}

void OutputQuota::scan() {
  std::lock_guard lock(mutex_);
  usedBytes_ = measure();
}

bool OutputQuota::exceeded() const {
  std::lock_guard lock(mutex_);
  return usedBytes_ >= limitBytes_;
}

void OutputQuota::add(std::uint64_t bytes) {
  std::lock_guard lock(mutex_);
  usedBytes_ += bytes;
}

std::uint64_t OutputQuota::measure() const {
  std::error_code error;
  if (root_.empty() || !std::filesystem::is_directory(root_, error)) {
    return 0;
  }
  std::uint64_t total = 0;
  const std::filesystem::recursive_directory_iterator end;
  for (std::filesystem::recursive_directory_iterator entry(
           root_, std::filesystem::directory_options::skip_permission_denied, error);
       !error && entry != end; entry.increment(error)) {
    if (!entry->is_regular_file(error)) {
      continue;
    }
    const auto size = entry->file_size(error);
    if (!error) {
      total += static_cast<std::uint64_t>(size);
    }
    error.clear();
  }
  return total;
}

}  // namespace gen
