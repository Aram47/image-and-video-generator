#include "net/request_dispatcher.hpp"

namespace gen {
namespace {

std::string_view requestIdOf(const ProtocolResult& result) {
  if (const auto* image = std::get_if<ImageRequest>(&result)) {
    return image->requestId;
  }
  if (const auto* video = std::get_if<VideoRequest>(&result)) {
    return video->requestId;
  }
  if (const auto* status = std::get_if<StatusRequest>(&result)) {
    return status->requestId;
  }
  return {};
}

std::string_view tokenOf(const ProtocolResult& result) {
  if (const auto* image = std::get_if<ImageRequest>(&result)) {
    return image->authToken;
  }
  if (const auto* video = std::get_if<VideoRequest>(&result)) {
    return video->authToken;
  }
  if (const auto* status = std::get_if<StatusRequest>(&result)) {
    return status->authToken;
  }
  return {};
}

}  // namespace

RequestDispatcher::RequestDispatcher(JobManager& jobs, Logger& logger, std::size_t maxMessageBytes, Metrics& metrics,
                                     TokenVerifier& verifier)
    : jobs_(jobs), logger_(logger), maxMessageBytes_(maxMessageBytes), metrics_(metrics), verifier_(verifier) {}

std::string RequestDispatcher::handleLine(std::string_view line) {
  const ProtocolResult result = protocol_.parse(line);
  if (const auto* failure = std::get_if<ProtocolFailure>(&result)) {
    logRejection(failure->requestId, failure->error);
    return protocol_.errorResponse(failure->requestId, failure->error);
  }
  if (!verifier_.accepts(tokenOf(result))) {
    const Error error{ErrorCode::Unauthorized, "Authentication is required."};
    logRejection(requestIdOf(result), error);
    return protocol_.errorResponse(requestIdOf(result), error);
  }
  if (const auto* image = std::get_if<ImageRequest>(&result)) {
    return finishSubmit(image->requestId, jobs_.submitImage(*image));
  }
  if (const auto* video = std::get_if<VideoRequest>(&result)) {
    return finishSubmit(video->requestId, jobs_.submitVideo(*video));
  }
  return finishStatus(std::get<StatusRequest>(result));
}

std::string RequestDispatcher::handleTooLarge() {
  const Error error{ErrorCode::RequestTooLarge,
                    "Request exceeds the " + std::to_string(maxMessageBytes_) + " byte limit."};
  logRejection("", error);
  return protocol_.errorResponse("", error);
}

void RequestDispatcher::logRejection(std::string_view requestId, const Error& error) {
  logger_.info("request_rejected", requestId, "", toString(error.code));
  metrics_.setQueueDepth(jobs_.queuedCount());
  metrics_.rejected();
}

std::string RequestDispatcher::finishSubmit(const std::string& requestId,
                                            const std::variant<std::string, Error>& submitted) {
  if (const auto* error = std::get_if<Error>(&submitted)) {
    logRejection(requestId, *error);
    return protocol_.errorResponse(requestId, *error);
  }
  return protocol_.acceptedResponse(requestId, std::get<std::string>(submitted));
}

std::string RequestDispatcher::finishStatus(const StatusRequest& status) {
  const auto snapshot = jobs_.status(status.jobId);
  if (const auto* error = std::get_if<Error>(&snapshot)) {
    logRejection(status.requestId, *error);
    return protocol_.errorResponse(status.requestId, *error);
  }
  return protocol_.statusResponse(status.requestId, std::get<JobSnapshot>(snapshot));
}

}  // namespace gen
