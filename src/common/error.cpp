#include "common/error.hpp"

namespace gen {

std::string_view toString(ErrorCode code) {
  switch (code) {
    case ErrorCode::MalformedJson:
      return "MALFORMED_JSON";
    case ErrorCode::UnsupportedProtocolVersion:
      return "UNSUPPORTED_PROTOCOL_VERSION";
    case ErrorCode::InvalidRequestType:
      return "INVALID_REQUEST_TYPE";
    case ErrorCode::MissingField:
      return "MISSING_FIELD";
    case ErrorCode::InvalidFieldValue:
      return "INVALID_FIELD_VALUE";
    case ErrorCode::RequestTooLarge:
      return "REQUEST_TOO_LARGE";
    case ErrorCode::UnsupportedPromptFeature:
      return "UNSUPPORTED_PROMPT_FEATURE";
    case ErrorCode::UnsupportedAction:
      return "UNSUPPORTED_ACTION";
    case ErrorCode::ResourceLimitExceeded:
      return "RESOURCE_LIMIT_EXCEEDED";
    case ErrorCode::JobNotFound:
      return "JOB_NOT_FOUND";
    case ErrorCode::RenderFailed:
      return "RENDER_FAILED";
    case ErrorCode::EncodingFailed:
      return "ENCODING_FAILED";
    case ErrorCode::InternalError:
      return "INTERNAL_ERROR";
    case ErrorCode::Unauthorized:
      return "UNAUTHORIZED";
  }
  return "INTERNAL_ERROR";
}

}  // namespace gen
