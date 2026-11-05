#pragma once

#include "jobs/job.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace gen {

struct StoredFile {
  std::filesystem::path absolute;
  std::string relative;
};

class OutputStore {
 public:
  explicit OutputStore(std::filesystem::path root);

  StoredFile allocate(std::string_view jobId, std::string_view fileName) const;
  StoredFile allocateNested(std::string_view jobId, std::string_view directory, std::string_view fileName) const;
  void removeDirectory(std::string_view jobId, std::string_view directory) const;
  JobResult writeStub(std::string_view jobId, std::string_view mediaType) const;

 private:
  std::filesystem::path jobDirectory(std::string_view jobId) const;

  std::filesystem::path root_;
  std::string publishedPrefix_;
};

}  // namespace gen
