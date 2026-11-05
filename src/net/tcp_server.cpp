#include "net/tcp_server.hpp"

#include "net/socket_stream.hpp"
#include "net/tls_context.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <stdexcept>
#include <string>

namespace gen {

TcpServer::TcpServer(const ServerConfig& config, RequestDispatcher& dispatcher, Logger& logger)
    : config_(config), dispatcher_(dispatcher), logger_(logger) {
  int fds[2] = {-1, -1};
  if (::pipe(fds) != 0) {
    throw std::runtime_error("Failed to create wake pipe.");
  }
  wakeRead_ = fds[0];
  wakeWrite_ = fds[1];
  if (::fcntl(wakeRead_, F_SETFD, FD_CLOEXEC) != 0 || ::fcntl(wakeWrite_, F_SETFD, FD_CLOEXEC) != 0) {
    ::close(wakeRead_);
    ::close(wakeWrite_);
    wakeRead_ = -1;
    wakeWrite_ = -1;
    throw std::runtime_error("Failed to create wake pipe.");
  }
}

TcpServer::~TcpServer() {
  try {
    stop();
  } catch (...) {
  }
  if (wakeRead_ >= 0) {
    ::close(wakeRead_);
    wakeRead_ = -1;
  }
  if (wakeWrite_ >= 0) {
    ::close(wakeWrite_);
    wakeWrite_ = -1;
  }
}

void TcpServer::start() {
  if (stopping_.load()) {
    throw std::logic_error("Server is stopped.");
  }
  bool expected = false;
  if (!started_.compare_exchange_strong(expected, true)) {
    throw std::logic_error("Server is already running.");
  }
  try {
    if (config_.tlsEnabled()) {
      tls_ = std::make_unique<TlsContext>(config_.tlsCert, config_.tlsKey);
    }
    openListenSocket();
    acceptThread_ = std::thread([this] { acceptLoop(); });
  } catch (...) {
    started_ = false;
    listen_.reset();
    tls_.reset();
    throw;
  }
}

void TcpServer::stop() {
  stopping_.store(true);
  signalWake();
  {
    std::lock_guard lock(mutex_);
    for (const auto& connection : connections_) {
      connection->requestStop();
    }
  }
  if (acceptThread_.joinable()) {
    acceptThread_.join();
  }
  std::vector<std::thread> threads;
  {
    std::lock_guard lock(mutex_);
    threads.swap(connectionThreads_);
  }
  for (auto& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  {
    std::lock_guard lock(mutex_);
    connections_.clear();
  }
  listen_.reset();
  tls_.reset();
}

std::uint16_t TcpServer::port() const { return port_; }

const std::string& TcpServer::boundAddress() const { return boundAddress_; }

void TcpServer::signalWake() const {
  if (wakeWrite_ < 0) {
    return;
  }
  const char byte = 1;
  const ssize_t written = ::write(wakeWrite_, &byte, 1);
  if (written < 0 && errno == EINTR) {
    ::write(wakeWrite_, &byte, 1);
  }
}

void TcpServer::openListenSocket() {
  const int rawFd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (rawFd < 0) {
    throw std::runtime_error("Failed to open server socket.");
  }
  Socket socket(rawFd);
  socket.setCloexec();
  socket.setReuseAddress();
  socket.setNoSigPipe();
  socket.setNonBlocking();

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(config_.port);
  if (::inet_pton(AF_INET, config_.bindAddress.c_str(), &address.sin_addr) != 1) {
    throw std::invalid_argument("Server bind address is invalid.");
  }
  if (::bind(socket.fd(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    throw std::runtime_error("Failed to bind server socket.");
  }
  if (::listen(socket.fd(), 128) != 0) {
    throw std::runtime_error("Failed to listen on server socket.");
  }

  sockaddr_in bound{};
  socklen_t length = sizeof(bound);
  if (::getsockname(socket.fd(), reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
    throw std::runtime_error("Failed to read bound server address.");
  }
  char text[INET_ADDRSTRLEN] = {};
  if (::inet_ntop(AF_INET, &bound.sin_addr, text, sizeof(text)) == nullptr) {
    throw std::runtime_error("Failed to read bound server address.");
  }
  boundAddress_ = text;
  port_ = ntohs(bound.sin_port);
  listen_ = std::move(socket);
  logger_.info("server_started", "", "",
               "address=" + boundAddress_ + " port=" + std::to_string(port_) +
                   " tls=" + (config_.tlsEnabled() ? "on" : "off"));
}

void TcpServer::acceptLoop() {
  while (!stopping_.load()) {
    pollfd fds[2];
    fds[0].fd = listen_.fd();
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = wakeRead_;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    const int ready = ::poll(fds, 2, -1);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (stopping_.load() || (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
      break;
    }
    if ((fds[0].revents & POLLIN) == 0) {
      continue;
    }
    acceptAvailable();
  }
}

void TcpServer::acceptAvailable() {
  while (!stopping_.load()) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    const int rawFd = ::accept(listen_.fd(), reinterpret_cast<sockaddr*>(&address), &length);
    if (rawFd < 0) {
      if (errno == EINTR) {
        continue;
      }
      return;
    }
    Socket socket(rawFd);
    try {
      socket.setCloexec();
      socket.setNoSigPipe();
      socket.setBlocking();
      socket.setNoDelay();
    } catch (...) {
      return;
    }
    if (stopping_.load()) {
      return;
    }
    std::unique_ptr<ByteStream> stream;
    try {
      if (tls_) {
        stream = tls_->accept(std::move(socket));
      } else {
        stream = std::make_unique<SocketStream>(std::move(socket));
      }
    } catch (...) {
      return;
    }
    if (!stream) {
      continue;
    }
    auto connection = std::make_shared<Connection>(std::move(stream), dispatcher_, config_.maxMessageBytes);
    std::lock_guard lock(mutex_);
    if (stopping_.load()) {
      return;
    }
    connections_.push_back(connection);
    connectionThreads_.emplace_back([connection] { connection->run(); });
  }
}

}  // namespace gen
