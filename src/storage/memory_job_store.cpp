#include "storage/memory_job_store.hpp"

namespace gen {

bool MemoryJobStore::insert(const Job& job) {
  std::lock_guard lock(mutex_);
  return jobs_.emplace(job.id, job).second;
}

void MemoryJobStore::erase(const std::string& id) {
  std::lock_guard lock(mutex_);
  jobs_.erase(id);
}

std::optional<Job> MemoryJobStore::find(const std::string& id) const {
  std::lock_guard lock(mutex_);
  const auto found = jobs_.find(id);
  if (found == jobs_.end()) {
    return std::nullopt;
  }
  return found->second;
}

bool MemoryJobStore::update(const std::string& id, const std::function<void(Job&)>& mutate) {
  std::lock_guard lock(mutex_);
  const auto found = jobs_.find(id);
  if (found == jobs_.end()) {
    return false;
  }
  mutate(found->second);
  return true;
}

StoreRecovery MemoryJobStore::recover() { return {}; }

}  // namespace gen
