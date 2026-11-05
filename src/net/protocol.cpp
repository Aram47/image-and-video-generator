#include "net/protocol.hpp"

#include "common/utf8.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>

namespace gen {
namespace {

constexpr std::int64_t kMaxSeed = 4294967295LL;
constexpr int kMaxPromptLength = 1024;
constexpr int kMaxRequestIdLength = 128;
constexpr int kMaxActionLength = 64;
constexpr int kMinDimension = 16;
constexpr int kMaxDimension = 1024;
constexpr int kMinFps = 1;
constexpr int kMaxFps = 30;
constexpr int kMinDurationSeconds = 1;
constexpr int kMaxDurationSeconds = 8;
constexpr int kMaxFrameCount = 240;

ProtocolFailure fail(ErrorCode code, std::string message, std::string requestId) {
  return ProtocolFailure{Error{code, std::move(message)}, std::move(requestId)};
}

std::string required(std::string_view field) {
  return "Field '" + std::string(field) + "' is required.";
}

std::string notAllowed(std::string_view field) {
  return "Field '" + std::string(field) + "' is not allowed.";
}

std::optional<std::int64_t> asInteger(const nlohmann::json& value) {
  if (!value.is_number_integer()) {
    return std::nullopt;
  }
  try {
    return value.get<std::int64_t>();
  } catch (const nlohmann::json::exception&) {
    return std::nullopt;
  }
}

template <typename T>
std::optional<ProtocolFailure> failureOf(const std::variant<T, Error>& value, const std::string& requestId) {
  if (const auto* error = std::get_if<Error>(&value)) {
    return fail(error->code, error->message, requestId);
  }
  return std::nullopt;
}

std::variant<std::string, Error> parsePrompt(const nlohmann::json& body) {
  if (!body.contains("prompt")) {
    return Error{ErrorCode::MissingField, required("prompt")};
  }
  if (!body["prompt"].is_string()) {
    return Error{ErrorCode::InvalidFieldValue, "Field 'prompt' must be from 1 to 1024 characters."};
  }
  const auto prompt = body["prompt"].get<std::string>();
  const Utf8Length length = utf8Length(prompt);
  if (!length.valid || length.codePoints < 1 || length.codePoints > kMaxPromptLength) {
    return Error{ErrorCode::InvalidFieldValue, "Field 'prompt' must be from 1 to 1024 characters."};
  }
  return prompt;
}

std::variant<nlohmann::json, Error> parseOptions(const nlohmann::json& body,
                                                 const std::set<std::string>& allowed) {
  if (!body.contains("options")) {
    return Error{ErrorCode::MissingField, required("options")};
  }
  if (!body["options"].is_object()) {
    return Error{ErrorCode::InvalidFieldValue, "Field 'options' must be an object."};
  }
  const auto& options = body["options"];
  for (auto field = options.begin(); field != options.end(); ++field) {
    if (!allowed.contains(field.key())) {
      return Error{ErrorCode::InvalidFieldValue, notAllowed(field.key())};
    }
  }
  return options;
}

std::variant<int, Error> parseDimension(const nlohmann::json& options, const char* name) {
  if (!options.contains(name)) {
    return Error{ErrorCode::MissingField, required(name)};
  }
  const auto value = asInteger(options[name]);
  if (!value || *value < kMinDimension || *value > kMaxDimension) {
    return Error{ErrorCode::InvalidFieldValue, "Field '" + std::string(name) +
                                                    "' must be an integer from 16 to 1024."};
  }
  return static_cast<int>(*value);
}

std::variant<std::optional<std::uint32_t>, Error> parseSeed(const nlohmann::json& options) {
  if (!options.contains("seed")) {
    return std::optional<std::uint32_t>{};
  }
  const auto value = asInteger(options["seed"]);
  if (!value || *value < 0 || *value > kMaxSeed) {
    return Error{ErrorCode::InvalidFieldValue, "Field 'seed' must be an integer from 0 to 4294967295."};
  }
  return std::optional<std::uint32_t>{static_cast<std::uint32_t>(*value)};
}

std::variant<std::string, Error> parseStyle(const nlohmann::json& options) {
  if (!options.contains("style")) {
    return std::string("stylized_3d");
  }
  if (!options["style"].is_string() || options["style"].get<std::string>() != "stylized_3d") {
    return Error{ErrorCode::InvalidFieldValue, "Field 'style' must be 'stylized_3d'."};
  }
  return std::string("stylized_3d");
}

std::variant<int, Error> parseFps(const nlohmann::json& options) {
  if (!options.contains("fps")) {
    return Error{ErrorCode::MissingField, required("fps")};
  }
  const auto value = asInteger(options["fps"]);
  if (!value || *value < kMinFps || *value > kMaxFps) {
    return Error{ErrorCode::InvalidFieldValue, "Field 'fps' must be an integer from 1 to 30."};
  }
  return static_cast<int>(*value);
}

std::variant<int, Error> parseDuration(const nlohmann::json& options) {
  if (!options.contains("duration_seconds")) {
    return Error{ErrorCode::MissingField, required("duration_seconds")};
  }
  const auto value = asInteger(options["duration_seconds"]);
  if (!value || *value < kMinDurationSeconds || *value > kMaxDurationSeconds) {
    return Error{ErrorCode::InvalidFieldValue,
                 "Field 'duration_seconds' must be an integer from 1 to 8."};
  }
  return static_cast<int>(*value);
}

std::variant<std::optional<std::string>, Error> parseAction(const nlohmann::json& options) {
  if (!options.contains("action")) {
    return std::optional<std::string>{};
  }
  if (!options["action"].is_string()) {
    return Error{ErrorCode::InvalidFieldValue, "Field 'action' must be a string up to 64 characters."};
  }
  const auto action = options["action"].get<std::string>();
  const Utf8Length length = utf8Length(action);
  if (!length.valid || length.codePoints < 1 || length.codePoints > kMaxActionLength) {
    return Error{ErrorCode::InvalidFieldValue, "Field 'action' must be a string up to 64 characters."};
  }
  return std::optional<std::string>{action};
}

ProtocolResult parseImage(const nlohmann::json& body, const std::string& requestId, const std::string& authToken) {
  auto prompt = parsePrompt(body);
  if (auto error = failureOf(prompt, requestId)) {
    return *error;
  }
  auto options = parseOptions(body, {"width", "height", "seed", "style"});
  if (auto error = failureOf(options, requestId)) {
    return *error;
  }
  const auto& optionObject = std::get<nlohmann::json>(options);
  auto width = parseDimension(optionObject, "width");
  if (auto error = failureOf(width, requestId)) {
    return *error;
  }
  auto height = parseDimension(optionObject, "height");
  if (auto error = failureOf(height, requestId)) {
    return *error;
  }
  auto seed = parseSeed(optionObject);
  if (auto error = failureOf(seed, requestId)) {
    return *error;
  }
  auto style = parseStyle(optionObject);
  if (auto error = failureOf(style, requestId)) {
    return *error;
  }

  ImageRequest request;
  request.requestId = requestId;
  request.authToken = authToken;
  request.prompt = std::get<std::string>(prompt);
  request.width = std::get<int>(width);
  request.height = std::get<int>(height);
  request.seed = std::get<std::optional<std::uint32_t>>(seed);
  request.style = std::get<std::string>(style);
  return request;
}

ProtocolResult parseVideo(const nlohmann::json& body, const std::string& requestId, const std::string& authToken) {
  auto prompt = parsePrompt(body);
  if (auto error = failureOf(prompt, requestId)) {
    return *error;
  }
  auto options = parseOptions(body, {"width", "height", "fps", "duration_seconds", "seed", "style", "action"});
  if (auto error = failureOf(options, requestId)) {
    return *error;
  }
  const auto& optionObject = std::get<nlohmann::json>(options);
  auto width = parseDimension(optionObject, "width");
  if (auto error = failureOf(width, requestId)) {
    return *error;
  }
  auto height = parseDimension(optionObject, "height");
  if (auto error = failureOf(height, requestId)) {
    return *error;
  }
  auto fps = parseFps(optionObject);
  if (auto error = failureOf(fps, requestId)) {
    return *error;
  }
  auto duration = parseDuration(optionObject);
  if (auto error = failureOf(duration, requestId)) {
    return *error;
  }
  const auto frameCount = static_cast<std::int64_t>(std::get<int>(fps)) *
                          static_cast<std::int64_t>(std::get<int>(duration));
  if (frameCount > kMaxFrameCount) {
    return fail(ErrorCode::InvalidFieldValue, "Frame count must not exceed 240.", requestId);
  }
  auto seed = parseSeed(optionObject);
  if (auto error = failureOf(seed, requestId)) {
    return *error;
  }
  auto style = parseStyle(optionObject);
  if (auto error = failureOf(style, requestId)) {
    return *error;
  }
  auto action = parseAction(optionObject);
  if (auto error = failureOf(action, requestId)) {
    return *error;
  }

  VideoRequest request;
  request.requestId = requestId;
  request.authToken = authToken;
  request.prompt = std::get<std::string>(prompt);
  request.width = std::get<int>(width);
  request.height = std::get<int>(height);
  request.fps = std::get<int>(fps);
  request.durationSeconds = std::get<int>(duration);
  request.seed = std::get<std::optional<std::uint32_t>>(seed);
  request.style = std::get<std::string>(style);
  request.action = std::get<std::optional<std::string>>(action);
  return request;
}

ProtocolResult parseStatus(const nlohmann::json& body, const std::string& requestId, const std::string& authToken) {
  if (!body.contains("job_id")) {
    return fail(ErrorCode::MissingField, required("job_id"), requestId);
  }
  if (!body["job_id"].is_string() || body["job_id"].get<std::string>().empty()) {
    return fail(ErrorCode::InvalidFieldValue, "Field 'job_id' must be a non-empty string.", requestId);
  }
  StatusRequest request;
  request.requestId = requestId;
  request.authToken = authToken;
  request.jobId = body["job_id"].get<std::string>();
  return request;
}

}  // namespace

ProtocolResult Protocol::parse(std::string_view line) const {
  nlohmann::json body;
  try {
    body = nlohmann::json::parse(line.begin(), line.end());
  } catch (const nlohmann::json::exception&) {
    return fail(ErrorCode::MalformedJson, "Request body is not valid JSON.", "");
  }
  if (!body.is_object()) {
    return fail(ErrorCode::MalformedJson, "Request body is not valid JSON.", "");
  }

  std::string requestId;
  if (body.contains("request_id") && body["request_id"].is_string()) {
    requestId = body["request_id"].get<std::string>();
  }

  if (!body.contains("protocol_version")) {
    return fail(ErrorCode::MissingField, required("protocol_version"), requestId);
  }
  const auto version = asInteger(body["protocol_version"]);
  if (!version) {
    return fail(ErrorCode::InvalidFieldValue, "Field 'protocol_version' must be an integer.", requestId);
  }
  if (*version != 1) {
    return fail(ErrorCode::UnsupportedProtocolVersion, "protocol_version must be 1.", requestId);
  }

  if (!body.contains("request_id")) {
    return fail(ErrorCode::MissingField, required("request_id"), requestId);
  }
  if (!body["request_id"].is_string()) {
    return fail(ErrorCode::InvalidFieldValue,
                "Field 'request_id' must be a non-empty string up to 128 characters.", requestId);
  }
  const Utf8Length requestLength = utf8Length(requestId);
  if (!requestLength.valid || requestLength.codePoints == 0 ||
      requestLength.codePoints > kMaxRequestIdLength) {
    return fail(ErrorCode::InvalidFieldValue,
                "Field 'request_id' must be a non-empty string up to 128 characters.", requestId);
  }

  if (!body.contains("type")) {
    return fail(ErrorCode::MissingField, required("type"), requestId);
  }
  if (!body["type"].is_string()) {
    return fail(ErrorCode::InvalidFieldValue, "Field 'type' must be a string.", requestId);
  }
  const auto type = body["type"].get<std::string>();

  std::set<std::string> allowed;
  if (type == "image" || type == "video") {
    allowed = {"protocol_version", "request_id", "type", "prompt", "options", "auth_token"};
  } else if (type == "status") {
    allowed = {"protocol_version", "request_id", "type", "job_id", "auth_token"};
  } else {
    return fail(ErrorCode::InvalidRequestType, "Request type is not supported.", requestId);
  }

  for (auto field = body.begin(); field != body.end(); ++field) {
    if (!allowed.contains(field.key())) {
      return fail(ErrorCode::InvalidFieldValue, notAllowed(field.key()), requestId);
    }
  }

  std::string authToken;
  if (body.contains("auth_token")) {
    if (!body["auth_token"].is_string()) {
      return fail(ErrorCode::InvalidFieldValue, "Field 'auth_token' must be a string.", requestId);
    }
    authToken = body["auth_token"].get<std::string>();
  }

  if (type == "image") {
    return parseImage(body, requestId, authToken);
  }
  if (type == "video") {
    return parseVideo(body, requestId, authToken);
  }
  return parseStatus(body, requestId, authToken);
}

std::string Protocol::acceptedResponse(std::string_view requestId, std::string_view jobId) const {
  const nlohmann::json body = {{"protocol_version", 1},
                               {"request_id", requestId},
                               {"ok", true},
                               {"job_id", jobId},
                               {"status", "queued"}};
  return body.dump();
}

std::string Protocol::errorResponse(std::string_view requestId, const Error& error) const {
  const nlohmann::json body = {{"protocol_version", 1},
                               {"request_id", requestId},
                               {"ok", false},
                               {"error", {{"code", std::string(toString(error.code))}, {"message", error.message}}}};
  return body.dump();
}

std::string Protocol::statusResponse(std::string_view requestId, const JobSnapshot& snapshot) const {
  nlohmann::json body = {{"protocol_version", 1},
                         {"request_id", requestId},
                         {"ok", true},
                         {"job_id", snapshot.id},
                         {"status", std::string(toString(snapshot.state))},
                         {"seed", snapshot.seed}};
  if (snapshot.state == JobState::Completed && snapshot.result) {
    body["result"] = {{"media_type", snapshot.result->mediaType}, {"path", snapshot.result->relativePath}};
  }
  if (snapshot.state == JobState::Completed && snapshot.parsed) {
    nlohmann::json diagnostics = nlohmann::json::object();
    if (!snapshot.unsupportedTerms.empty()) {
      diagnostics["unsupported_terms"] = snapshot.unsupportedTerms;
    }
    diagnostics["seed"] = snapshot.seed;
    if (snapshot.frameCount) {
      diagnostics["frame_count"] = *snapshot.frameCount;
    }
    if (snapshot.actionName) {
      diagnostics["action"] = *snapshot.actionName;
    }
    body["diagnostics"] = std::move(diagnostics);
  }
  if (snapshot.state == JobState::Failed) {
    body["error"] = {{"code", snapshot.errorCode}, {"message", snapshot.errorMessage}};
  }
  return body.dump();
}

}  // namespace gen
