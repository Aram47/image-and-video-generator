#include "app/application.hpp"
#include "app/server_config.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <stdexcept>

TEST(ServerConfigTest, RejectsAddressesOtherThanLoopback) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("gen-config-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  gen::ServerConfig config;
  config.bindAddress = "0.0.0.0";
  config.jobDirectory = root / "jobs";
  config.outputDirectory = root / "outputs";
  EXPECT_THROW(config.validate(), std::invalid_argument);

  gen::Application application(config);
  EXPECT_THROW(application.start(), std::invalid_argument);
  EXPECT_EQ(application.port(), 0);
  std::error_code error;
  std::filesystem::remove_all(root, error);
}

TEST(ServerConfigTest, RejectsMoreThanOneWorker) {
  gen::ServerConfig config;
  config.workerCount = 2;
  EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ServerConfigTest, AcceptsTheLocalDefault) {
  gen::ServerConfig config;
  EXPECT_NO_THROW(config.validate());
}
