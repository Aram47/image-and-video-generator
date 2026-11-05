#include "app/application.hpp"
#include "app/server_config.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

class TempDirectory {
 public:
  explicit TempDirectory(std::string name) {
    path_ = std::filesystem::temp_directory_path() / name;
    std::filesystem::create_directories(path_ / "outputs");
  }

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  std::filesystem::path path() const { return path_; }
  std::filesystem::path outputs() const { return path_ / "outputs"; }

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
      fd_ = -1;
      throw std::runtime_error("Failed to connect to the server.");
    }
  }

  ~TestClient() { close(); }

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  void sendRaw(std::string_view data) {
    while (!data.empty()) {
      const ssize_t sent = ::send(fd_, data.data(), data.size(), 0);
      if (sent < 0) {
        if (errno == EINTR) {
          continue;
        }
        throw std::runtime_error("Failed to send to the server.");
      }
      data.remove_prefix(static_cast<std::size_t>(sent));
    }
  }

  void sendJson(const nlohmann::json& message) { sendRaw(message.dump() + "\n"); }

  std::string readLine() {
    std::string line;
    char character = 0;
    for (;;) {
      const ssize_t count = ::recv(fd_, &character, 1, 0);
      if (count < 0) {
        if (errno == EINTR) {
          continue;
        }
        throw std::runtime_error("Failed to read from the server.");
      }
      if (count == 0) {
        throw std::runtime_error("Server closed the connection.");
      }
      if (character == '\n') {
        return line;
      }
      line.push_back(character);
    }
  }

  nlohmann::json roundTrip(const nlohmann::json& message) {
    sendJson(message);
    return nlohmann::json::parse(readLine());
  }

  nlohmann::json roundTripRaw(std::string_view payload) {
    sendRaw(payload);
    return nlohmann::json::parse(readLine());
  }

  void close() {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  std::string localAddress() const { return readAddress(true); }

  std::string peerAddress() const { return readAddress(false); }

 private:
  std::string readAddress(bool local) const {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    const int result = local ? ::getsockname(fd_, reinterpret_cast<sockaddr*>(&address), &length)
                             : ::getpeername(fd_, reinterpret_cast<sockaddr*>(&address), &length);
    if (result != 0) {
      throw std::runtime_error("Failed to read a socket address.");
    }
    char text[INET_ADDRSTRLEN] = {};
    if (::inet_ntop(AF_INET, &address.sin_addr, text, sizeof(text)) == nullptr) {
      throw std::runtime_error("Failed to format a socket address.");
    }
    return text;
  }

  int fd_{-1};
};

nlohmann::json imageRequest() {
  return {{"protocol_version", 1},
          {"request_id", "r-1"},
          {"type", "image"},
          {"prompt", "A short man with black hair wearing a yellow shirt, standing"},
          {"options", {{"width", 512}, {"height", 512}, {"seed", 42}, {"style", "stylized_3d"}}}};
}

nlohmann::json videoRequest() {
  return {{"protocol_version", 1},
          {"request_id", "client-request-002"},
          {"type", "video"},
          {"prompt", "A young man in a green jacket walks through a park and waves at the camera"},
          {"options",
           {{"width", 16},
            {"height", 16},
            {"fps", 2},
            {"duration_seconds", 1},
            {"seed", 12345},
            {"style", "stylized_3d"},
            {"action", "walk_and_wave"}}}};
}

nlohmann::json statusRequest(const std::string& jobId) {
  return {{"protocol_version", 1}, {"request_id", "status-request"}, {"type", "status"}, {"job_id", jobId}};
}

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

nlohmann::json waitUntilCompleted(TestClient& client, const std::string& jobId) {
  for (int attempt = 0; attempt < 1000; ++attempt) {
    const auto status = client.roundTrip(statusRequest(jobId));
    if (status["status"] == "completed" || status["status"] == "failed") {
      return status;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  throw std::runtime_error("Timed out waiting for job completion.");
}

class ServerFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    std::signal(SIGPIPE, SIG_IGN);
    directory_ = std::make_unique<TempDirectory>("gen-server-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) +
                                                 "-" +
                                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    gen::ServerConfig config;
    config.port = 0;
    config.outputDirectory = directory_->outputs();
    config.jobDirectory = directory_->path() / "jobs";
    application_ = std::make_unique<gen::Application>(std::move(config));
    application_->start();
  }

  void TearDown() override {
    if (application_) {
      application_->stop();
      application_.reset();
    }
    directory_.reset();
  }

  std::unique_ptr<TempDirectory> directory_;
  std::unique_ptr<gen::Application> application_;
};

}  // namespace

TEST_F(ServerFixture, ListensOnLoopbackAndCompletesAnImage) {
  EXPECT_EQ(application_->boundAddress(), "127.0.0.1");
  TestClient client(application_->port());
  EXPECT_EQ(client.localAddress(), "127.0.0.1");
  EXPECT_EQ(client.peerAddress(), "127.0.0.1");

  const auto accepted = client.roundTrip(imageRequest());
  EXPECT_EQ(accepted["ok"], true);
  EXPECT_EQ(accepted["status"], "queued");
  EXPECT_EQ(accepted["request_id"], "r-1");
  const auto jobId = accepted["job_id"].get<std::string>();

  const auto status = waitUntilCompleted(client, jobId);
  EXPECT_EQ(status["ok"], true);
  EXPECT_EQ(status["status"], "completed");
  EXPECT_EQ(status["request_id"], "status-request");
  EXPECT_EQ(status["seed"], 42);
  EXPECT_EQ(status["result"]["media_type"], "image");
  EXPECT_EQ(status["result"]["path"], "outputs/" + jobId + "/image.png");
  EXPECT_EQ(status["diagnostics"]["seed"], 42);
  EXPECT_FALSE(status["diagnostics"].contains("unsupported_terms"));
  EXPECT_TRUE(std::filesystem::exists(directory_->outputs() / jobId / "image.png"));
  EXPECT_FALSE(std::filesystem::exists(directory_->outputs() / jobId / "stub.txt"));
}

TEST_F(ServerFixture, WingsFailTheJobWithoutWritingAStub) {
  TestClient client(application_->port());
  auto request = imageRequest();
  request["prompt"] = "A tall woman with wings";
  const auto accepted = client.roundTrip(request);
  ASSERT_EQ(accepted["ok"], true);
  const auto jobId = accepted["job_id"].get<std::string>();

  const auto status = waitUntilCompleted(client, jobId);
  EXPECT_EQ(status["ok"], true);
  EXPECT_EQ(status["status"], "failed");
  EXPECT_EQ(status["seed"], 42);
  EXPECT_EQ(status["error"]["code"], "UNSUPPORTED_PROMPT_FEATURE");
  EXPECT_EQ(status["error"]["message"], "The requested feature 'wings' is not supported yet.");
  EXPECT_FALSE(status.contains("diagnostics"));
  EXPECT_FALSE(status.contains("result"));
  EXPECT_FALSE(std::filesystem::exists(directory_->outputs() / jobId / "stub.txt"));
  EXPECT_FALSE(std::filesystem::exists(directory_->outputs() / jobId / "image.png"));
}

TEST_F(ServerFixture, CompletesAShortVideoOnTheSameQueue) {
  TestClient client(application_->port());
  const auto accepted = client.roundTrip(videoRequest());
  EXPECT_EQ(accepted["ok"], true);
  const auto jobId = accepted["job_id"].get<std::string>();
  const auto status = waitUntilCompleted(client, jobId);
  EXPECT_EQ(status["seed"], 12345);
  EXPECT_FALSE(std::filesystem::exists(directory_->outputs() / jobId / "stub.txt"));
  EXPECT_FALSE(std::filesystem::exists(directory_->outputs() / jobId / "frames"));
#if GEN_HAS_FFMPEG
  EXPECT_EQ(status["status"], "completed");
  EXPECT_EQ(status["result"]["media_type"], "video");
  EXPECT_EQ(status["result"]["path"], "outputs/" + jobId + "/video.mp4");
  EXPECT_EQ(status["diagnostics"]["seed"], 12345);
  EXPECT_EQ(status["diagnostics"]["frame_count"], 2);
  EXPECT_EQ(status["diagnostics"]["action"], "walk_and_wave");
  EXPECT_TRUE(std::filesystem::exists(directory_->outputs() / jobId / "video.mp4"));
  EXPECT_TRUE(std::filesystem::exists(directory_->outputs() / jobId / "metadata.json"));
#else
  EXPECT_EQ(status["status"], "failed");
  EXPECT_EQ(status["error"]["code"], "ENCODING_FAILED");
  EXPECT_FALSE(status.contains("result"));
  EXPECT_EQ(status["error"]["message"], "Failed to encode the video.");
#endif
}

TEST_F(ServerFixture, RepeatedPayloadsGetDistinctJobIds) {
  TestClient client(application_->port());
  const auto first = client.roundTrip(imageRequest());
  const auto second = client.roundTrip(imageRequest());
  EXPECT_NE(first["job_id"], second["job_id"]);
  EXPECT_EQ(waitUntilCompleted(client, first["job_id"].get<std::string>())["status"], "completed");
  EXPECT_EQ(waitUntilCompleted(client, second["job_id"].get<std::string>())["status"], "completed");
}

TEST_F(ServerFixture, ClosingTheClientDoesNotCancelAQueuedJob) {
  TestClient client(application_->port());
  const auto accepted = client.roundTrip(imageRequest());
  const auto jobId = accepted["job_id"].get<std::string>();
  EXPECT_EQ(accepted["status"], "queued");
  client.close();

  TestClient poller(application_->port());
  const auto status = waitUntilCompleted(poller, jobId);
  EXPECT_EQ(status["status"], "completed");
  EXPECT_EQ(status["result"]["path"], "outputs/" + jobId + "/image.png");
}

TEST_F(ServerFixture, OversizedMessageDoesNotConsumeTheFollowingRequest) {
  TestClient client(application_->port());
  std::string payload(64 * 1024 + 1, 'a');
  payload.push_back('\n');
  payload += imageRequest().dump();
  payload.push_back('\n');
  client.sendRaw(payload);

  const auto error = nlohmann::json::parse(client.readLine());
  EXPECT_EQ(error["ok"], false);
  EXPECT_EQ(error["request_id"], "");
  EXPECT_EQ(error["error"]["code"], "REQUEST_TOO_LARGE");
  EXPECT_FALSE(error.contains("job_id"));

  const auto accepted = nlohmann::json::parse(client.readLine());
  EXPECT_EQ(accepted["ok"], true);
  EXPECT_EQ(accepted["status"], "queued");
}

TEST_F(ServerFixture, SamePromptAndSeedWriteIdenticalPngBytes) {
  TestClient client(application_->port());
  const auto first = client.roundTrip(imageRequest());
  const auto second = client.roundTrip(imageRequest());
  const auto firstId = first["job_id"].get<std::string>();
  const auto secondId = second["job_id"].get<std::string>();
  EXPECT_EQ(waitUntilCompleted(client, firstId)["status"], "completed");
  EXPECT_EQ(waitUntilCompleted(client, secondId)["status"], "completed");

  const auto firstBytes = readBytes(directory_->outputs() / firstId / "image.png");
  const auto secondBytes = readBytes(directory_->outputs() / secondId / "image.png");
  EXPECT_FALSE(firstBytes.empty());
  EXPECT_EQ(firstBytes, secondBytes);
}

TEST_F(ServerFixture, SittingPoseCompletesAndWritesAnImage) {
  TestClient client(application_->port());
  auto request = imageRequest();
  request["prompt"] = "a man sits";
  request["options"]["width"] = 64;
  request["options"]["height"] = 64;
  const auto accepted = client.roundTrip(request);
  ASSERT_EQ(accepted["ok"], true);
  const auto jobId = accepted["job_id"].get<std::string>();

  const auto status = waitUntilCompleted(client, jobId);
  EXPECT_EQ(status["status"], "completed");
  EXPECT_EQ(status["result"]["media_type"], "image");
  EXPECT_EQ(status["result"]["path"], "outputs/" + jobId + "/image.png");
  EXPECT_TRUE(std::filesystem::exists(directory_->outputs() / jobId / "image.png"));
}

TEST_F(ServerFixture, ReadOnlyOutputDirectoryFailsTheImage) {
  struct RestoreWriteAccess {
    explicit RestoreWriteAccess(std::filesystem::path path) : path_(std::move(path)) {
      std::filesystem::permissions(path_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                   std::filesystem::perm_options::replace);
    }

    ~RestoreWriteAccess() {
      std::error_code error;
      std::filesystem::permissions(path_, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace,
                                   error);
    }

    std::filesystem::path path_;
  };

  const RestoreWriteAccess permissions(directory_->outputs());
  TestClient client(application_->port());
  auto request = imageRequest();
  request["options"]["width"] = 32;
  request["options"]["height"] = 32;
  const auto accepted = client.roundTrip(request);
  ASSERT_EQ(accepted["ok"], true);
  const auto jobId = accepted["job_id"].get<std::string>();
  const auto status = waitUntilCompleted(client, jobId);
  EXPECT_EQ(status["status"], "failed");
  EXPECT_EQ(status["error"]["code"], "RENDER_FAILED");
  EXPECT_EQ(status["error"]["message"], "Failed to write the image.");
  EXPECT_EQ(status["error"]["message"].get<std::string>().find(directory_->outputs().string()), std::string::npos);
  EXPECT_FALSE(status.contains("result"));
  EXPECT_FALSE(std::filesystem::exists(directory_->outputs() / jobId / "image.png"));
}
