#pragma once

#include "storage/ijob_store.hpp"

#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace gen {

class FileJobStore final : public IJobStore {
 public:
  FileJobStore(std::filesystem::path directory, std::filesystem::path outputDirectory);

  bool insert(const Job& job) override;
  void erase(const std::string& id) override;
  std::optional<Job> find(const std::string& id) const override;
  bool update(const std::string& id, const std::function<void(Job&)>& mutate) override;
  StoreRecovery recover() override;

 private:
  void load();
  void persist(const Job& job) const;
  bool outputExists(const Job& job) const;

  std::filesystem::path directory_;
  std::filesystem::path outputDirectory_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Job> jobs_;
  bool recovered_{false};
};

}  // namespace gen
