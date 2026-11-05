#pragma once

#include "common/error.hpp"
#include "common/logger.hpp"
#include "common/metrics.hpp"
#include "common/token_verifier.hpp"
#include "jobs/job_manager.hpp"
#include "jobs/job_request.hpp"
#include "net/protocol.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

namespace gen {

class RequestDispatcher {
 public:
  RequestDispatcher(JobManager& jobs, Logger& logger, std::size_t maxMessageBytes, Metrics& metrics,
                    TokenVerifier& verifier);

  std::string handleLine(std::string_view line);
  std::string handleTooLarge();

 private:
  void logRejection(std::string_view requestId, const Error& error);
  std::string finishSubmit(const std::string& requestId, const std::variant<std::string, Error>& submitted);
  std::string finishStatus(const StatusRequest& status);

  JobManager& jobs_;
  Logger& logger_;
  std::size_t maxMessageBytes_;
  Metrics& metrics_;
  TokenVerifier& verifier_;
  Protocol protocol_;
};

}  // namespace gen
