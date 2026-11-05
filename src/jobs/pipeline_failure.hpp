#pragma once

#include "common/error.hpp"

#include <stdexcept>
#include <string>

namespace gen {

class PipelineFailure : public std::runtime_error {
 public:
  PipelineFailure(ErrorCode code, std::string clientMessage, std::string logDetail);

  ErrorCode code() const { return code_; }
  const std::string& clientMessage() const { return clientMessage_; }

 private:
  ErrorCode code_;
  std::string clientMessage_;
};

}  // namespace gen
