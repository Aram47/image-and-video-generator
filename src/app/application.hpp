#pragma once

#include "app/server_config.hpp"
#include "common/logger.hpp"
#include "common/metrics.hpp"
#include "common/token_verifier.hpp"
#include "jobs/ijob_executor.hpp"
#include "jobs/job_manager.hpp"
#include "net/request_dispatcher.hpp"
#include "net/tcp_server.hpp"
#include "storage/file_job_store.hpp"
#include "storage/output_quota.hpp"
#include "storage/output_store.hpp"
#include "storage/process_lock.hpp"

#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>

namespace gen {

class Application {
 public:
  explicit Application(ServerConfig config, std::unique_ptr<IJobExecutor> executor = nullptr,
                       std::ostream* log = nullptr);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  void start();
  void stop();
  std::uint16_t port() const;
  const std::string& boundAddress() const;

 private:
  ServerConfig config_;
  std::ostream* logStream_;
  Logger logger_;
  OutputStore output_;
  OutputQuota quota_;
  FileJobStore jobsOnDisk_;
  Metrics metrics_;
  TokenVerifier verifier_;
  JobManager jobs_;
  RequestDispatcher dispatcher_;
  TcpServer server_;
  std::unique_ptr<ProcessLock> lock_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopped_{false};
};

}  // namespace gen
