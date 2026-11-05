#include "app/server_config.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace gen {
namespace {

constexpr const char* kTlsRequired = "TLS certificate and key are required.";
constexpr const char* kTlsRequiredForAddress =
    "TLS certificate and key are required when bind_address is not 127.0.0.1.";
constexpr const char* kTokenFileRequired = "Authentication token file is required.";

[[noreturn]] void invalidField(const char* name) {
  throw std::invalid_argument(std::string("Server configuration field '") + name + "' is invalid.");
}

std::string requireText(const nlohmann::json& body, const char* name, const std::string& fallback) {
  if (!body.contains(name)) {
    return fallback;
  }
  if (!body[name].is_string()) {
    invalidField(name);
  }
  return body[name].get<std::string>();
}

}  // namespace

void ServerConfig::validate() const {
  if (bindAddress.empty()) {
    throw std::invalid_argument("Server bind address is invalid.");
  }
  const bool hasCert = !tlsCert.empty();
  const bool hasKey = !tlsKey.empty();
  if (hasCert != hasKey) {
    throw std::invalid_argument(kTlsRequired);
  }
  if (bindAddress != "127.0.0.1" && !hasCert) {
    throw std::invalid_argument(kTlsRequiredForAddress);
  }
  if (workerCount != 1) {
    throw std::invalid_argument("workerCount must be 1.");
  }
  if (maxMessageBytes == 0) {
    throw std::invalid_argument("maxMessageBytes must be positive.");
  }
  if (queueCapacity == 0) {
    throw std::invalid_argument("queueCapacity must be positive.");
  }
  if (outputDirectory.empty()) {
    throw std::invalid_argument("outputDirectory is required.");
  }
  if (jobDirectory.empty()) {
    throw std::invalid_argument("jobDirectory is required.");
  }
  if (frameRetentionSeconds < 0) {
    throw std::invalid_argument("frameRetentionSeconds must not be negative.");
  }
  if (authRequired && authTokenFile.empty()) {
    throw std::invalid_argument(kTokenFileRequired);
  }
}

void ServerConfig::ensureTlsMaterial() const {
  if (!tlsEnabled()) {
    return;
  }
  std::error_code error;
  if (!std::filesystem::is_regular_file(tlsCert, error) || !std::filesystem::is_regular_file(tlsKey, error)) {
    throw std::invalid_argument(kTlsRequired);
  }
}

std::string ServerConfig::readAuthToken() const {
  if (authTokenFile.empty()) {
    throw std::invalid_argument(kTokenFileRequired);
  }
  std::ifstream input(authTokenFile, std::ios::binary);
  if (!input) {
    throw std::invalid_argument(kTokenFileRequired);
  }
  std::string token((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (!token.empty() && token.back() == '\n') {
    token.pop_back();
  }
  if (!token.empty() && token.back() == '\r') {
    token.pop_back();
  }
  if (token.empty()) {
    throw std::invalid_argument(kTokenFileRequired);
  }
  return token;
}

bool ServerConfig::tlsEnabled() const { return !tlsCert.empty() && !tlsKey.empty(); }

std::filesystem::path ServerConfig::dataDirectory() const {
  const auto parent = jobDirectory.parent_path();
  if (parent.empty()) {
    return ".";
  }
  return parent;
}

ServerConfig ServerConfig::load(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::invalid_argument("Failed to read server configuration.");
  }
  nlohmann::json body;
  try {
    input >> body;
  } catch (const nlohmann::json::exception&) {
    throw std::invalid_argument("Server configuration is not valid JSON.");
  }
  if (!body.is_object()) {
    throw std::invalid_argument("Server configuration is not valid JSON.");
  }

  ServerConfig config;
  config.authRequired = true;
  config.bindAddress = requireText(body, "bind_address", config.bindAddress);
  config.authTokenFile = requireText(body, "auth_token_file", {});
  config.tlsCert = requireText(body, "tls_cert", {});
  config.tlsKey = requireText(body, "tls_key", {});
  config.outputDirectory = requireText(body, "output_dir", config.outputDirectory.string());
  config.jobDirectory = requireText(body, "job_dir", config.jobDirectory.string());

  if (body.contains("port")) {
    if (!body["port"].is_number_integer()) {
      invalidField("port");
    }
    const auto port = body["port"].get<std::int64_t>();
    if (port < 0 || port > 65535) {
      invalidField("port");
    }
    config.port = static_cast<std::uint16_t>(port);
  }
  if (body.contains("auth_required")) {
    if (!body["auth_required"].is_boolean()) {
      invalidField("auth_required");
    }
    config.authRequired = body["auth_required"].get<bool>();
  }
  if (body.contains("max_queued")) {
    if (!body["max_queued"].is_number_integer() || body["max_queued"].get<std::int64_t>() <= 0) {
      invalidField("max_queued");
    }
    config.queueCapacity = body["max_queued"].get<std::size_t>();
  }
  if (body.contains("max_output_bytes")) {
    if (!body["max_output_bytes"].is_number_integer() || body["max_output_bytes"].get<std::int64_t>() < 0) {
      invalidField("max_output_bytes");
    }
    config.maxOutputBytes = body["max_output_bytes"].get<std::uint64_t>();
  }
  if (body.contains("log_prompts")) {
    if (!body["log_prompts"].is_boolean()) {
      invalidField("log_prompts");
    }
    config.logPrompts = body["log_prompts"].get<bool>();
  }
  if (body.contains("frame_retention_seconds")) {
    if (!body["frame_retention_seconds"].is_number_integer()) {
      invalidField("frame_retention_seconds");
    }
    config.frameRetentionSeconds = body["frame_retention_seconds"].get<int>();
  }

  config.validate();
  return config;
}

}  // namespace gen
