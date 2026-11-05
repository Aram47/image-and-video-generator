#pragma once

#include <string>
#include <string_view>

namespace gen {

enum class ErrorCode {
  MalformedJson,
  UnsupportedProtocolVersion,
  InvalidRequestType,
  MissingField,
  InvalidFieldValue,
  RequestTooLarge,
  UnsupportedPromptFeature,
  UnsupportedAction,
  ResourceLimitExceeded,
  JobNotFound,
  RenderFailed,
  EncodingFailed,
  InternalError,
  Unauthorized,
};

struct Error {
  ErrorCode code{};
  std::string message;
};

std::string_view toString(ErrorCode code);

}  // namespace gen
