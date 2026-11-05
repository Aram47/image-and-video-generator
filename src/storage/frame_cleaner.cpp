#include "storage/frame_cleaner.hpp"

#include <chrono>
#include <vector>

namespace gen {

void FrameCleaner::purge(const std::filesystem::path& outputRoot, int retentionSeconds) {
  std::error_code error;
  if (outputRoot.empty() || !std::filesystem::is_directory(outputRoot, error)) {
    return;
  }

  const auto now = std::filesystem::file_time_type::clock::now();
  const auto maxAge = std::chrono::seconds(retentionSeconds < 0 ? 0 : retentionSeconds);
  std::vector<std::filesystem::path> stale;
  const std::filesystem::recursive_directory_iterator end;
  for (std::filesystem::recursive_directory_iterator entry(
           outputRoot, std::filesystem::directory_options::skip_permission_denied, error);
       !error && entry != end; entry.increment(error)) {
    if (!entry->is_directory(error) || entry->path().filename() != "frames") {
      error.clear();
      continue;
    }
    bool remove = retentionSeconds <= 0;
    if (!remove) {
      const auto written = entry->last_write_time(error);
      remove = !error && now - written >= maxAge;
    }
    if (remove) {
      stale.push_back(entry->path());
      entry.disable_recursion_pending();
    }
    error.clear();
  }

  for (const auto& path : stale) {
    std::filesystem::remove_all(path, error);
    error.clear();
  }
}

}  // namespace gen
