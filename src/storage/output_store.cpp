#include "storage/output_store.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace gen {
namespace {

bool isSafeJobId(std::string_view jobId) {
  if (jobId.empty()) {
    return false;
  }
  for (const char character : jobId) {
    const bool allowed = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
                         (character >= '0' && character <= '9') || character == '_' || character == '-';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

bool isSafeFileName(std::string_view fileName) {
  if (fileName.empty() || fileName == "." || fileName == "..") {
    return false;
  }
  for (const char character : fileName) {
    const bool allowed = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
                         (character >= '0' && character <= '9') || character == '_' || character == '-' ||
                         character == '.';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

std::string prefixOf(const std::filesystem::path& root) {
  const auto normalized = root.lexically_normal();
  const auto filename = normalized.filename();
  if (!filename.empty() && filename != "." && filename != "..") {
    return filename.generic_string();
  }
  const auto parentName = normalized.parent_path().filename();
  if (!parentName.empty() && parentName != "." && parentName != "..") {
    return parentName.generic_string();
  }
  return {};
}

[[noreturn]] void failWrite(const std::filesystem::path& path) {
  throw std::runtime_error("Failed to write job output at " + path.string());
}

}  // namespace

OutputStore::OutputStore(std::filesystem::path root) : root_(std::move(root)) {
  if (root_.empty()) {
    throw std::invalid_argument("outputDirectory is required.");
  }
  publishedPrefix_ = prefixOf(root_);
  if (publishedPrefix_.empty()) {
    throw std::invalid_argument("outputDirectory is required.");
  }
}

std::filesystem::path OutputStore::jobDirectory(std::string_view jobId) const {
  if (!isSafeJobId(jobId)) {
    throw std::invalid_argument("Job id cannot be used as a path.");
  }

  std::error_code error;
  std::filesystem::create_directories(root_, error);
  if (error) {
    failWrite(root_);
  }

  const auto canonicalRoot = std::filesystem::weakly_canonical(root_, error);
  if (error) {
    failWrite(root_);
  }

  const auto jobDirectory = canonicalRoot / std::string(jobId);
  std::filesystem::create_directories(jobDirectory, error);
  if (error) {
    failWrite(jobDirectory);
  }

  const auto canonicalJob = std::filesystem::weakly_canonical(jobDirectory, error);
  if (error) {
    failWrite(jobDirectory);
  }
  const auto relativeDirectory = std::filesystem::relative(canonicalJob, canonicalRoot, error);
  if (error || relativeDirectory != std::filesystem::path(std::string(jobId))) {
    failWrite(jobDirectory);
  }
  return canonicalJob;
}

StoredFile OutputStore::allocate(std::string_view jobId, std::string_view fileName) const {
  if (!isSafeFileName(fileName)) {
    throw std::invalid_argument("File name cannot be used as a path.");
  }

  const auto canonicalJob = jobDirectory(jobId);
  const auto filePath = canonicalJob / std::string(fileName);
  const auto published = std::filesystem::path(publishedPrefix_) / std::string(jobId) / std::string(fileName);
  return StoredFile{filePath, published.generic_string()};
}

StoredFile OutputStore::allocateNested(std::string_view jobId, std::string_view directory,
                                       std::string_view fileName) const {
  if (!isSafeFileName(directory) || !isSafeFileName(fileName)) {
    throw std::invalid_argument("File name cannot be used as a path.");
  }

  const auto canonicalJob = jobDirectory(jobId);
  const auto nested = canonicalJob / std::string(directory);
  std::error_code error;
  std::filesystem::create_directories(nested, error);
  if (error) {
    failWrite(nested);
  }
  const auto canonicalNested = std::filesystem::weakly_canonical(nested, error);
  if (error) {
    failWrite(nested);
  }
  const auto relativeNested = std::filesystem::relative(canonicalNested, canonicalJob, error);
  if (error || relativeNested != std::filesystem::path(std::string(directory))) {
    failWrite(nested);
  }

  const auto filePath = canonicalNested / std::string(fileName);
  const auto published = std::filesystem::path(publishedPrefix_) / std::string(jobId) / std::string(directory) /
                         std::string(fileName);
  return StoredFile{filePath, published.generic_string()};
}

void OutputStore::removeDirectory(std::string_view jobId, std::string_view directory) const {
  if (!isSafeJobId(jobId) || !isSafeFileName(directory)) {
    throw std::invalid_argument("File name cannot be used as a path.");
  }

  std::error_code error;
  const auto canonicalRoot = std::filesystem::weakly_canonical(root_, error);
  if (error || !std::filesystem::exists(canonicalRoot)) {
    return;
  }
  const auto jobDirectory = canonicalRoot / std::string(jobId);
  if (!std::filesystem::exists(jobDirectory)) {
    return;
  }
  const auto canonicalJob = std::filesystem::weakly_canonical(jobDirectory, error);
  if (error) {
    return;
  }
  const auto nested = canonicalJob / std::string(directory);
  if (!std::filesystem::exists(nested)) {
    return;
  }
  const auto canonicalNested = std::filesystem::weakly_canonical(nested, error);
  if (error) {
    return;
  }
  const auto relativeNested = std::filesystem::relative(canonicalNested, canonicalJob, error);
  if (error || relativeNested != std::filesystem::path(std::string(directory))) {
    throw std::invalid_argument("File name cannot be used as a path.");
  }
  std::filesystem::remove_all(canonicalNested, error);
}

JobResult OutputStore::writeStub(std::string_view jobId, std::string_view mediaType) const {
  if (mediaType != "image" && mediaType != "video") {
    throw std::invalid_argument("Media type is not supported.");
  }

  const StoredFile stored = allocate(jobId, "stub.txt");
  std::ofstream output(stored.absolute, std::ios::binary | std::ios::trunc);
  if (!output) {
    failWrite(stored.absolute);
  }
  output << "stub\n";
  if (!output) {
    failWrite(stored.absolute);
  }
  JobResult result{std::string(mediaType), stored.relative};
  result.bytes = static_cast<std::uint64_t>(std::filesystem::file_size(stored.absolute));
  return result;
}

}  // namespace gen
