#include "jobs/pipeline_failure.hpp"

#include <utility>

namespace gen {

PipelineFailure::PipelineFailure(ErrorCode code, std::string clientMessage, std::string logDetail)
    : std::runtime_error(std::move(logDetail)), code_(code), clientMessage_(std::move(clientMessage)) {}

}  // namespace gen
