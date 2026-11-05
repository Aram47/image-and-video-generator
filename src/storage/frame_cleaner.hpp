#pragma once

#include <filesystem>

namespace gen {

class FrameCleaner {
 public:
  static void purge(const std::filesystem::path& outputRoot, int retentionSeconds);
};

}  // namespace gen
