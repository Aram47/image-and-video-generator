#pragma once

#include "app/server_config.hpp"
#include "common/logger.hpp"
#include "net/connection.hpp"
#include "net/request_dispatcher.hpp"
#include "net/socket.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gen {

class TlsContext;

class TcpServer {
 public:
  TcpServer(const ServerConfig& config, RequestDispatcher& dispatcher, Logger& logger);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  void start();
  void stop();
  std::uint16_t port() const;
  const std::string& boundAddress() const;

 private:
  void openListenSocket();
  void acceptLoop();
  void acceptAvailable();
  void signalWake() const;

  const ServerConfig& config_;
  RequestDispatcher& dispatcher_;
  Logger& logger_;
  Socket listen_;
  std::unique_ptr<TlsContext> tls_;
  int wakeRead_{-1};
  int wakeWrite_{-1};
  std::atomic<bool> stopping_{false};
  std::atomic<bool> started_{false};
  std::uint16_t port_{0};
  std::string boundAddress_;
  std::mutex mutex_;
  std::vector<std::shared_ptr<Connection>> connections_;
  std::vector<std::thread> connectionThreads_;
  std::thread acceptThread_;
};

}  // namespace gen
