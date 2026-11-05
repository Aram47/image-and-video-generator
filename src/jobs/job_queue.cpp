#include "jobs/job_queue.hpp"

#include <algorithm>

namespace gen {

JobQueue::JobQueue(std::size_t capacity) : capacity_(capacity) {}

bool JobQueue::tryPush(std::string jobId) {
  std::lock_guard lock(mutex_);
  if (stopped_ || items_.size() >= capacity_) {
    return false;
  }
  items_.push_back(std::move(jobId));
  ready_.notify_one();
  return true;
}

std::optional<std::string> JobQueue::pop() {
  std::unique_lock lock(mutex_);
  ready_.wait(lock, [this] { return stopped_ || !items_.empty(); });
  if (items_.empty()) {
    return std::nullopt;
  }
  std::string jobId = std::move(items_.front());
  items_.pop_front();
  return jobId;
}

void JobQueue::stop() {
  std::lock_guard lock(mutex_);
  stopped_ = true;
  ready_.notify_all();
}

std::size_t JobQueue::size() const {
  std::lock_guard lock(mutex_);
  return items_.size();
}

std::size_t JobQueue::capacity() const { return capacity_; }

bool JobQueue::contains(const std::string& jobId) const {
  std::lock_guard lock(mutex_);
  return std::find(items_.begin(), items_.end(), jobId) != items_.end();
}

}  // namespace gen
