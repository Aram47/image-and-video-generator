#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace gen {

class JobQueue {
 public:
  explicit JobQueue(std::size_t capacity);

  JobQueue(const JobQueue&) = delete;
  JobQueue& operator=(const JobQueue&) = delete;

  bool tryPush(std::string jobId);
  std::optional<std::string> pop();
  void stop();
  std::size_t size() const;
  std::size_t capacity() const;
  bool contains(const std::string& jobId) const;

 private:
  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<std::string> items_;
  bool stopped_{false};
};

}  // namespace gen
