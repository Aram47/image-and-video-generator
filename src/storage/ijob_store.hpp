#pragma once

#include "jobs/job.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gen {

struct StoreRecovery {
  std::vector<std::string> requeue;
  std::vector<std::string> interrupted;
};

class IJobStore {
 public:
  virtual ~IJobStore() = default;

  IJobStore() = default;
  IJobStore(const IJobStore&) = delete;
  IJobStore& operator=(const IJobStore&) = delete;

  virtual bool insert(const Job& job) = 0;
  virtual void erase(const std::string& id) = 0;
  virtual std::optional<Job> find(const std::string& id) const = 0;
  virtual bool update(const std::string& id, const std::function<void(Job&)>& mutate) = 0;
  virtual StoreRecovery recover() = 0;
};

}  // namespace gen
