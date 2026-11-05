#include "cli_parser.hpp"
#include "ndjson_client.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

namespace gen {
namespace {

std::string requestId() {
  std::random_device device;
  constexpr char kHex[] = "0123456789abcdef";
  std::string id = "cli-";
  for (int index = 0; index < 16; ++index) {
    const auto byte = static_cast<unsigned char>(device());
    id.push_back(kHex[byte >> 4]);
    id.push_back(kHex[byte & 0x0F]);
  }
  return id;
}

nlohmann::json buildRequest(const CliOptions& options) {
  nlohmann::json body = {{"protocol_version", 1}, {"request_id", requestId()}, {"type", options.command}};
  if (!options.token.empty()) {
    body["auth_token"] = options.token;
  }
  if (options.command == "status") {
    body["job_id"] = options.jobId;
    return body;
  }

  body["prompt"] = options.prompt;
  nlohmann::json requestOptions = {{"width", options.width}, {"height", options.height}, {"style", "stylized_3d"}};
  if (options.seed) {
    requestOptions["seed"] = *options.seed;
  }
  if (options.command == "video") {
    requestOptions["fps"] = options.fps;
    requestOptions["duration_seconds"] = options.durationSeconds;
    requestOptions["action"] = options.action;
  }
  body["options"] = std::move(requestOptions);
  return body;
}

int failCode(const nlohmann::json& body) {
  if (body.contains("error") && body["error"].is_object() && body["error"].contains("code") &&
      body["error"]["code"].is_string()) {
    std::cerr << body["error"]["code"].get<std::string>() << '\n';
    return 1;
  }
  std::cerr << "INVALID_FIELD_VALUE\n";
  return 1;
}

int interpret(const nlohmann::json& body) {
  if (!body.value("ok", false)) {
    return failCode(body);
  }
  const auto status = body.value("status", "");
  if (status == "completed") {
    return 0;
  }
  if (status == "failed") {
    return failCode(body);
  }
  return 1;
}

int runCommand(const CliOptions& options) {
  NdjsonClient client(options.host, options.port, options.caFile, options.timeoutSeconds);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                               std::chrono::duration<double>(options.timeoutSeconds));
  const std::string firstLine = client.exchange(buildRequest(options).dump());
  std::cout << firstLine << '\n';
  nlohmann::json first;
  try {
    first = nlohmann::json::parse(firstLine);
  } catch (const nlohmann::json::exception&) {
    std::cerr << "MALFORMED_JSON\n";
    return 1;
  }
  if (options.command == "status" || !first.value("ok", false) || first.value("status", "") != "queued") {
    return interpret(first);
  }

  const auto jobId = first.value("job_id", "");
  if (jobId.empty()) {
    std::cerr << "INTERNAL_ERROR\n";
    return 1;
  }
  const auto interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(options.pollIntervalSeconds));
  while (std::chrono::steady_clock::now() < deadline) {
    if (interval.count() > 0) {
      std::this_thread::sleep_for(interval);
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      break;
    }
    nlohmann::json status = {{"protocol_version", 1}, {"request_id", requestId()}, {"type", "status"}, {"job_id", jobId}};
    if (!options.token.empty()) {
      status["auth_token"] = options.token;
    }
    const std::string line = client.exchange(status.dump());
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(line);
    } catch (const nlohmann::json::exception&) {
      std::cout << line << '\n';
      std::cerr << "MALFORMED_JSON\n";
      return 1;
    }
    const auto state = body.value("status", "");
    if (!body.value("ok", false) || state == "completed" || state == "failed") {
      std::cout << line << '\n';
      return interpret(body);
    }
  }
  std::cerr << "TIMEOUT\n";
  return 1;
}

}  // namespace

int run(const CliOptions& options) { return runCommand(options); }

}  // namespace gen

int main(int argc, char** argv) {
  try {
    return gen::run(gen::CliParser(argc, argv).parse());
  } catch (const std::invalid_argument& error) {
    std::cerr << error.what() << '\n';
    return 2;
  } catch (const std::exception& error) {
    const std::string message = error.what();
    if (message == "TIMEOUT") {
      std::cerr << "TIMEOUT\n";
    } else {
      std::cerr << message << '\n';
    }
    return 1;
  }
}
