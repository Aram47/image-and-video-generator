#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

namespace gen {

class VideoEncodingError : public std::runtime_error {
 public:
  explicit VideoEncodingError(int status);

  int status() const { return status_; }

 private:
  int status_;
};

class IVideoEncoder {
 public:
  virtual ~IVideoEncoder() = default;

  IVideoEncoder() = default;
  IVideoEncoder(const IVideoEncoder&) = delete;
  IVideoEncoder& operator=(const IVideoEncoder&) = delete;

  virtual void encode(const std::vector<std::filesystem::path>& frames, int fps,
                      const std::filesystem::path& output) = 0;
};

class NullVideoEncoder final : public IVideoEncoder {
 public:
  void encode(const std::vector<std::filesystem::path>& frames, int fps, const std::filesystem::path& output) override;
};

std::unique_ptr<IVideoEncoder> makeVideoEncoder();

}  // namespace gen
