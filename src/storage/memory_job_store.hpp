#pragma once

#include "storage/ijob_store.hpp"

#include <mutex>
#include <unordered_map>

namespace gen {

class MemoryJobStore final : public IJobStore {
 public:
  bool insert(const Job& job) override;
  void erase(const std::string& id) override;
  std::optional<Job> find(const std::string& id) const override;
  bool update(const std::string& id, const std::function<void(Job&)>& mutate) override;
  StoreRecovery recover() override;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Job> jobs_;
};

}  // namespace gen
