#include "app/application.hpp"
#include "app/server_config.hpp"
#include "cli_path.hpp"
#include "common/logger.hpp"
#include "jobs/job.hpp"
#include "jobs/job_manager.hpp"
#include "jobs/stub_job_executor.hpp"
#include "storage/file_job_store.hpp"
#include "storage/output_quota.hpp"
#include "storage/output_store.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

class TempDirectory {
 public:
  explicit TempDirectory(std::string name) {
    path_ = std::filesystem::temp_directory_path() /
            (name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path_ / "outputs");
    std::filesystem::create_directories(path_ / "jobs");
  }

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  std::filesystem::path path() const { return path_; }
  std::filesystem::path outputs() const { return path_ / "outputs"; }
  std::filesystem::path jobs() const { return path_ / "jobs"; }

 private:
  std::filesystem::path path_;
};

class TestClient {
 public:
  explicit TestClient(std::uint16_t port) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
      throw std::runtime_error("Failed to open test socket.");
    }
    const int enabled = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
    timeval timeout{};
    timeout.tv_sec = 2;
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        ::connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
      ::close(fd_);
      throw std::runtime_error("Failed to connect to the server.");
    }
  }

  ~TestClient() {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }

  nlohmann::json roundTrip(const nlohmann::json& message) {
    const std::string payload = message.dump() + "\n";
    std::string_view data = payload;
    while (!data.empty()) {
      const ssize_t sent = ::send(fd_, data.data(), data.size(), 0);
      if (sent < 0) {
        throw std::runtime_error("Failed to send to the server.");
      }
      data.remove_prefix(static_cast<std::size_t>(sent));
    }
    std::string line;
    char character = 0;
    for (;;) {
      const ssize_t count = ::recv(fd_, &character, 1, 0);
      if (count <= 0) {
        throw std::runtime_error("Failed to read from the server.");
      }
      if (character == '\n') {
        return nlohmann::json::parse(line);
      }
      line.push_back(character);
    }
  }

 private:
  int fd_{-1};
};

struct CommandResult {
  int exitCode{1};
  std::string output;
};

CommandResult runCommand(const std::vector<std::string>& args, int timeoutSeconds = 30) {
  int pipes[2] = {-1, -1};
  if (::pipe(pipes) != 0) {
    throw std::runtime_error("Failed to create a pipe.");
  }
  const pid_t pid = ::fork();
  if (pid < 0) {
    ::close(pipes[0]);
    ::close(pipes[1]);
    throw std::runtime_error("Failed to start the CLI.");
  }
  if (pid == 0) {
    ::dup2(pipes[1], STDOUT_FILENO);
    ::dup2(pipes[1], STDERR_FILENO);
    ::close(pipes[0]);
    ::close(pipes[1]);
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    ::execv(argv[0], argv.data());
    _exit(127);
  }

  ::close(pipes[1]);
  CommandResult result;
  char buffer[1024];
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
  bool timedOut = false;
  for (;;) {
    pollfd ready{};
    ready.fd = pipes[0];
    ready.events = POLLIN;
    const int waitMs = 200;
    const int polled = ::poll(&ready, 1, waitMs);
    if (polled > 0) {
      const ssize_t count = ::read(pipes[0], buffer, sizeof(buffer));
      if (count > 0) {
        result.output.append(buffer, static_cast<std::size_t>(count));
      } else if (count == 0) {
        break;
      }
    }
    int status = 0;
    const pid_t done = ::waitpid(pid, &status, WNOHANG);
    if (done == pid) {
      result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
      for (;;) {
        const ssize_t count = ::read(pipes[0], buffer, sizeof(buffer));
        if (count <= 0) {
          break;
        }
        result.output.append(buffer, static_cast<std::size_t>(count));
      }
      ::close(pipes[0]);
      return result;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      timedOut = true;
      ::kill(pid, SIGKILL);
      break;
    }
  }
  ::close(pipes[0]);
  int status = 0;
  ::waitpid(pid, &status, 0);
  if (timedOut) {
    result.exitCode = 1;
    result.output += "TIMEOUT";
  } else if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  }
  return result;
}

nlohmann::json imageRequest(const std::string& token = {}) {
  nlohmann::json body = {{"protocol_version", 1},
                         {"request_id", "r-1"},
                         {"type", "image"},
                         {"prompt", "A short man with black hair wearing a yellow shirt, standing"},
                         {"options", {{"width", 16}, {"height", 16}, {"seed", 42}, {"style", "stylized_3d"}}}};
  if (!token.empty()) {
    body["auth_token"] = token;
  }
  return body;
}

void writeSecret(const std::filesystem::path& path, const std::string& secret) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << secret << '\n';
}

std::size_t countJson(const std::filesystem::path& directory) {
  std::size_t count = 0;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (!error && entry.path().extension() == ".json") {
      ++count;
    }
  }
  return count;
}

gen::ServerConfig testConfig(const TempDirectory& directory) {
  gen::ServerConfig config;
  config.port = 0;
  config.outputDirectory = directory.outputs();
  config.jobDirectory = directory.jobs();
  return config;
}

void writeCertificate(const std::filesystem::path& directory, std::filesystem::path& certificatePath,
                      std::filesystem::path& keyPath) {
  EVP_PKEY_CTX* context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  if (context == nullptr || EVP_PKEY_keygen_init(context) <= 0 || EVP_PKEY_CTX_set_rsa_keygen_bits(context, 2048) <= 0) {
    EVP_PKEY_CTX_free(context);
    throw std::runtime_error("Failed to create a test key.");
  }
  EVP_PKEY* key = nullptr;
  if (EVP_PKEY_keygen(context, &key) <= 0) {
    EVP_PKEY_CTX_free(context);
    throw std::runtime_error("Failed to create a test key.");
  }
  EVP_PKEY_CTX_free(context);

  X509* certificate = X509_new();
  ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  X509_gmtime_adj(X509_getm_notBefore(certificate), 0);
  X509_gmtime_adj(X509_getm_notAfter(certificate), 31536000L);
#else
  X509_gmtime_adj(X509_get_notBefore(certificate), 0);
  X509_gmtime_adj(X509_get_notAfter(certificate), 31536000L);
#endif
  X509_set_pubkey(certificate, key);
  X509_NAME* name = X509_NAME_new();
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("localhost"), -1, -1, 0);
  X509_set_subject_name(certificate, name);
  X509_set_issuer_name(certificate, name);
  X509_NAME_free(name);

  X509V3_CTX extensionContext;
  X509V3_set_ctx_nodb(&extensionContext);
  X509V3_set_ctx(&extensionContext, certificate, certificate, nullptr, nullptr, 0);
  X509_EXTENSION* altName =
      X509V3_EXT_conf_nid(nullptr, &extensionContext, NID_subject_alt_name, "DNS:localhost,IP:127.0.0.1");
  X509_EXTENSION* usage = X509V3_EXT_conf_nid(nullptr, &extensionContext, NID_ext_key_usage, "serverAuth");
  if (altName == nullptr || usage == nullptr || X509_add_ext(certificate, altName, -1) != 1 ||
      X509_add_ext(certificate, usage, -1) != 1 || X509_sign(certificate, key, EVP_sha256()) == 0) {
    X509_EXTENSION_free(altName);
    X509_EXTENSION_free(usage);
    X509_free(certificate);
    EVP_PKEY_free(key);
    throw std::runtime_error("Failed to create a test certificate.");
  }
  X509_EXTENSION_free(altName);
  X509_EXTENSION_free(usage);

  certificatePath = directory / "cert.pem";
  keyPath = directory / "key.pem";
  FILE* certificateFile = std::fopen(certificatePath.c_str(), "w");
  FILE* keyFile = std::fopen(keyPath.c_str(), "w");
  const bool wrote = certificateFile != nullptr && keyFile != nullptr && PEM_write_X509(certificateFile, certificate) == 1 &&
                     PEM_write_PrivateKey(keyFile, key, nullptr, nullptr, 0, nullptr, nullptr) == 1;
  if (certificateFile != nullptr) {
    std::fclose(certificateFile);
  }
  if (keyFile != nullptr) {
    std::fclose(keyFile);
  }
  X509_free(certificate);
  EVP_PKEY_free(key);
  if (!wrote) {
    throw std::runtime_error("Failed to write a test certificate.");
  }
}

class NoopExecutor : public gen::IJobExecutor {
 public:
  gen::JobResult execute(const gen::Job& job) override {
    return {job.type == gen::JobType::Image ? "image" : "video", "outputs/" + job.id + "/stub.txt"};
  }
};

gen::Job queuedImage(const std::string& id) {
  gen::Job job;
  job.id = id;
  job.requestId = "stored-request";
  job.type = gen::JobType::Image;
  job.prompt = "person";
  job.state = gen::JobState::Queued;
  gen::ImageOptions options;
  options.width = 16;
  options.height = 16;
  options.seed = 5;
  options.style = "stylized_3d";
  job.options = options;
  return job;
}

}  // namespace

TEST(DeploymentTest, ExampleConfigRequiresAuthAndLoopback) {
  const auto config = gen::ServerConfig::load(std::filesystem::path(GEN_SOURCE_DIR) / "server_config.example.json");
  EXPECT_TRUE(config.authRequired);
  EXPECT_EQ(config.bindAddress, "127.0.0.1");
  EXPECT_FALSE(config.logPrompts);
  EXPECT_EQ(config.queueCapacity, 8U);
  EXPECT_EQ(config.maxOutputBytes, 2147483648ULL);
  EXPECT_NO_THROW(config.validate());
}

TEST(DeploymentTest, RejectsMissingAndWrongTokensBeforeCreatingAJob) {
  TempDirectory directory("gen-auth");
  const auto secret = "super-secret-token-value";
  writeSecret(directory.path() / "token", secret);
  auto config = testConfig(directory);
  config.authRequired = true;
  config.authTokenFile = directory.path() / "token";
  std::ostringstream log;
  gen::Application application(config, nullptr, &log);
  application.start();
  TestClient client(application.port());

  const auto missing = client.roundTrip(imageRequest());
  EXPECT_EQ(missing["ok"], false);
  EXPECT_EQ(missing["error"]["code"], "UNAUTHORIZED");
  EXPECT_EQ(missing["error"]["message"], "Authentication is required.");
  EXPECT_FALSE(missing.contains("job_id"));

  auto wrong = imageRequest("different-token");
  wrong["request_id"] = "wrong-token";
  const auto rejected = client.roundTrip(wrong);
  EXPECT_EQ(rejected["error"]["code"], "UNAUTHORIZED");
  EXPECT_EQ(rejected["error"]["message"], "Authentication is required.");
  EXPECT_EQ(countJson(directory.jobs()), 0U);

  auto acceptedRequest = imageRequest(secret);
  acceptedRequest["request_id"] = "accepted-token";
  acceptedRequest["prompt"] = "UNIQUE_PROMPT_TOKEN_ZZZ";
  const auto accepted = client.roundTrip(acceptedRequest);
  EXPECT_EQ(accepted["ok"], true);
  EXPECT_EQ(accepted["status"], "queued");
  EXPECT_EQ(log.str().find(secret), std::string::npos);
  EXPECT_EQ(log.str().find("UNIQUE_PROMPT_TOKEN_ZZZ"), std::string::npos);
}

TEST(DeploymentTest, PhaseOneRequestStillWorksWithoutAuthOnLoopback) {
  TempDirectory directory("gen-dev-auth");
  auto config = testConfig(directory);
  config.authRequired = false;
  gen::Application application(config);
  application.start();
  TestClient client(application.port());
  const auto accepted = client.roundTrip(imageRequest());
  EXPECT_EQ(accepted["ok"], true);
  EXPECT_EQ(accepted["status"], "queued");
  EXPECT_TRUE(accepted.contains("job_id"));
}

TEST(DeploymentTest, RestartMarksRunningJobFailedAndRequeuesQueuedJob) {
  TempDirectory directory("gen-recover");
  {
    gen::FileJobStore store(directory.jobs(), directory.outputs());
    auto running = queuedImage("job-running");
    running.state = gen::JobState::Running;
    ASSERT_TRUE(store.insert(running));
    ASSERT_TRUE(store.insert(queuedImage("job-queued")));
  }

  std::ostringstream log;
  gen::Logger logger(log);
  gen::FileJobStore store(directory.jobs(), directory.outputs());
  gen::OutputStore output(directory.outputs());
  gen::OutputQuota quota(directory.outputs(), 1024 * 1024);
  gen::Metrics metrics(logger, directory.path() / "metrics.json");
  gen::JobManager jobs(8, std::make_unique<gen::StubJobExecutor>(output), logger, store, quota, metrics);
  jobs.start();

  const auto interrupted = jobs.status("job-running");
  const auto* failed = std::get_if<gen::JobSnapshot>(&interrupted);
  ASSERT_NE(failed, nullptr);
  EXPECT_EQ(failed->state, gen::JobState::Failed);
  EXPECT_EQ(failed->errorCode, "INTERNAL_ERROR");
  EXPECT_EQ(failed->errorMessage, "Job was interrupted by a restart.");
  EXPECT_TRUE(std::filesystem::is_regular_file(directory.jobs() / "job-running.json"));

  const auto requeuedId = std::string("job-queued");
  for (int attempt = 0; attempt < 200 && !std::holds_alternative<gen::JobSnapshot>(jobs.status(requeuedId)); ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  gen::JobSnapshot completed;
  for (int attempt = 0; attempt < 200; ++attempt) {
    const auto status = jobs.status(requeuedId);
    if (const auto* snapshot = std::get_if<gen::JobSnapshot>(&status); snapshot && snapshot->state == gen::JobState::Completed) {
      completed = *snapshot;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_EQ(completed.state, gen::JobState::Completed);
  EXPECT_TRUE(std::filesystem::exists(directory.outputs() / requeuedId / "stub.txt"));
}

TEST(DeploymentTest, NinthJobAndFullOutputAreRejectedBeforeAJobIsStored) {
  TempDirectory directory("gen-quota");
  std::ostringstream log;
  gen::Logger logger(log);
  gen::FileJobStore store(directory.jobs(), directory.outputs());
  gen::OutputQuota quota(directory.outputs(), 1024 * 1024);
  gen::Metrics metrics(logger, {});
  gen::JobManager jobs(8, std::make_unique<NoopExecutor>(), logger, store, quota, metrics);

  gen::ImageRequest request;
  request.requestId = "queue";
  request.prompt = "person";
  request.width = 16;
  request.height = 16;
  request.seed = 1;
  request.style = "stylized_3d";
  for (int index = 0; index < 8; ++index) {
    request.requestId = "queue-" + std::to_string(index);
    ASSERT_TRUE(std::holds_alternative<std::string>(jobs.submitImage(request))) << index;
  }
  request.requestId = "queue-9";
  const auto rejected = jobs.submitImage(request);
  const auto* error = std::get_if<gen::Error>(&rejected);
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, gen::ErrorCode::ResourceLimitExceeded);
  EXPECT_EQ(error->message, "queue is full");
  EXPECT_EQ(countJson(directory.jobs()), 8U);

  TempDirectory full("gen-disk");
  {
    std::ofstream seed(full.outputs() / "existing.bin", std::ios::binary);
    seed << std::string(64, 'x');
  }
  auto config = testConfig(full);
  config.maxOutputBytes = 32;
  gen::Application application(config);
  application.start();
  TestClient client(application.port());
  auto body = imageRequest();
  body["options"]["width"] = 64;
  body["options"]["height"] = 64;
  const auto response = client.roundTrip(body);
  EXPECT_EQ(response["ok"], false);
  EXPECT_EQ(response["error"]["code"], "RESOURCE_LIMIT_EXCEEDED");
  EXPECT_EQ(response["error"]["message"], "output storage is full");
  EXPECT_FALSE(response.contains("job_id"));
  EXPECT_EQ(countJson(full.jobs()), 0U);
}

TEST(DeploymentTest, MetricsRecordAcceptedAndRejectedJobs) {
  TempDirectory directory("gen-metrics");
  auto config = testConfig(directory);
  std::ostringstream log;
  gen::Application application(config, nullptr, &log);
  application.start();
  TestClient client(application.port());
  const auto accepted = client.roundTrip(imageRequest());
  ASSERT_EQ(accepted["ok"], true);
  const auto jobId = accepted["job_id"].get<std::string>();
  for (int attempt = 0; attempt < 200; ++attempt) {
    const auto status = client.roundTrip(
        {{"protocol_version", 1}, {"request_id", "status"}, {"type", "status"}, {"job_id", jobId}});
    if (status.value("status", "") == "completed" || status.value("status", "") == "failed") {
      EXPECT_EQ(status["status"], "completed");
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  const auto rejected = client.roundTrip({{"request_id", "bad"}});
  EXPECT_EQ(rejected["error"]["code"], "MISSING_FIELD");

  std::ifstream input(directory.path() / "metrics.json");
  ASSERT_TRUE(input);
  const auto metrics = nlohmann::json::parse(input);
  EXPECT_GT(metrics["jobs_completed"].get<int>(), 0);
  EXPECT_GT(metrics["jobs_rejected"].get<int>(), 0);
  EXPECT_EQ(metrics.dump().find("super-secret"), std::string::npos);
  EXPECT_EQ(metrics.dump().find("prompt"), std::string::npos);
}

TEST(DeploymentTest, StartupRemovesAbandonedFrames) {
  TempDirectory directory("gen-frames");
  const auto frames = directory.outputs() / "job-old" / "frames";
  std::filesystem::create_directories(frames);
  std::ofstream(frames / "frame_0000.png") << "frame";
  auto config = testConfig(directory);
  config.frameRetentionSeconds = 0;
  gen::Application application(config);
  application.start();
  EXPECT_FALSE(std::filesystem::exists(frames));
}

TEST(DeploymentTest, SecondProcessDoesNotListen) {
  TempDirectory directory("gen-lock");
  auto config = testConfig(directory);
  gen::Application first(config);
  first.start();

  const auto configPath = directory.path() / "server_config.json";
  {
    std::ofstream output(configPath);
    const nlohmann::json body = {{"bind_address", "127.0.0.1"},
                                 {"port", 0},
                                 {"auth_required", false},
                                 {"output_dir", directory.outputs().string()},
                                 {"job_dir", directory.jobs().string()},
                                 {"max_queued", 8},
                                 {"max_output_bytes", 1000000},
                                 {"log_prompts", false},
                                 {"frame_retention_seconds", 0}};
    output << body.dump() << '\n';
  }

  const auto second = runCommand({GEN_SERVER_PATH, configPath.string()}, 3);
  EXPECT_NE(second.exitCode, 0) << second.output;
  EXPECT_NE(second.output.find("Server is already running."), std::string::npos) << second.output;
  EXPECT_EQ(first.boundAddress(), "127.0.0.1");
}

TEST(DeploymentTest, TlsRejectsAClientWithoutTheCertificateAndAcceptsOneWithIt) {
  TempDirectory directory("gen-tls");
  std::filesystem::path certificate;
  std::filesystem::path key;
  writeCertificate(directory.path(), certificate, key);
  auto config = testConfig(directory);
  config.tlsCert = certificate;
  config.tlsKey = key;
  gen::Application application(config);
  application.start();

  const auto missing = runCommand({GEN_CLI_PATH, "status", "--host", "127.0.0.1", "--port", std::to_string(application.port()),
                                   "--job-id", "job-missing", "--timeout", "5"});
  EXPECT_NE(missing.exitCode, 0);
  EXPECT_EQ(missing.output.find("JOB_NOT_FOUND"), std::string::npos);

  const auto verified = runCommand({GEN_CLI_PATH, "status", "--host", "127.0.0.1", "--port",
                                    std::to_string(application.port()), "--ca-file", certificate.string(), "--job-id",
                                    "job-missing", "--timeout", "5"});
  EXPECT_NE(verified.exitCode, 0) << verified.output;
  EXPECT_NE(verified.output.find("JOB_NOT_FOUND"), std::string::npos) << verified.output;
}

TEST(DeploymentTest, CMakeDependenciesDoNotReferenceGenerationApis) {
  std::ifstream input(std::filesystem::path(GEN_SOURCE_DIR) / "CMakeLists.txt");
  ASSERT_TRUE(input);
  const std::string cmake((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  const char* forbidden[] = {"api.openai.com",     "stability.ai",
                             "replicate.com",      "api.anthropic.com",
                             "generativelanguage", "midjourney",
                             "fal.ai",             "api.together.xyz",
                             "huggingface.co"};
  for (const char* needle : forbidden) {
    EXPECT_EQ(cmake.find(needle), std::string::npos) << needle;
  }
}

TEST(DeploymentTest, CliImageCompletesAndPrintsAnExistingPng) {
  TempDirectory directory("gen-cli-image");
  const auto secret = "cli-image-token";
  writeSecret(directory.path() / "token", secret);
  auto config = testConfig(directory);
  config.authRequired = true;
  config.authTokenFile = directory.path() / "token";
  std::ostringstream log;
  gen::Application application(config, nullptr, &log);
  application.start();

  const auto result = runCommand({GEN_CLI_PATH,
                                  "image",
                                  "--host",
                                  "127.0.0.1",
                                  "--port",
                                  std::to_string(application.port()),
                                  "--token",
                                  secret,
                                  "--prompt",
                                  "A short man with black hair wearing a yellow shirt, standing",
                                  "--width",
                                  "64",
                                  "--height",
                                  "64",
                                  "--seed",
                                  "42",
                                  "--poll-interval",
                                  "0.05",
                                  "--timeout",
                                  "20"});
  EXPECT_EQ(result.exitCode, 0) << result.output;
  EXPECT_NE(result.output.find("\"status\":\"completed\""), std::string::npos) << result.output;
  EXPECT_EQ(log.str().find(secret), std::string::npos);

  bool found = false;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(directory.outputs(), error)) {
    if (!error && std::filesystem::is_regular_file(entry.path() / "image.png")) {
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

#if GEN_HAS_FFMPEG
TEST(DeploymentTest, CliVideoCompletesOnLibav) {
  TempDirectory directory("gen-cli-video");
  const auto secret = "cli-video-token";
  writeSecret(directory.path() / "token", secret);
  auto config = testConfig(directory);
  config.authRequired = true;
  config.authTokenFile = directory.path() / "token";
  gen::Application application(config);
  application.start();
  const auto result = runCommand({GEN_CLI_PATH,
                                  "video",
                                  "--host",
                                  "127.0.0.1",
                                  "--port",
                                  std::to_string(application.port()),
                                  "--token",
                                  secret,
                                  "--prompt",
                                  "A young man in a green jacket walks through a park and waves at the camera",
                                  "--width",
                                  "16",
                                  "--height",
                                  "16",
                                  "--fps",
                                  "2",
                                  "--duration",
                                  "1",
                                  "--action",
                                  "walk_and_wave",
                                  "--seed",
                                  "42",
                                  "--poll-interval",
                                  "0.05",
                                  "--timeout",
                                  "20"});
  EXPECT_EQ(result.exitCode, 0) << result.output;
  EXPECT_NE(result.output.find("\"status\":\"completed\""), std::string::npos) << result.output;
}
#endif
