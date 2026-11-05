#pragma once

#include "common/error.hpp"
#include "jobs/job.hpp"
#include "jobs/job_request.hpp"

#include <string>
#include <string_view>
#include <variant>

namespace gen {

struct ProtocolFailure {
  Error error;
  std::string requestId;
};

using ProtocolResult = std::variant<ImageRequest, VideoRequest, StatusRequest, ProtocolFailure>;

class Protocol {
 public:
  ProtocolResult parse(std::string_view line) const;
  std::string acceptedResponse(std::string_view requestId, std::string_view jobId) const;
  std::string errorResponse(std::string_view requestId, const Error& error) const;
  std::string statusResponse(std::string_view requestId, const JobSnapshot& snapshot) const;
};

}  // namespace gen
