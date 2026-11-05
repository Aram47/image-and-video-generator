#include "ndjson_client.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

namespace gen {
namespace {

class TimeoutError : public std::runtime_error {
 public:
  TimeoutError() : std::runtime_error("TIMEOUT") {}
};

void configureSocket(int fd, double timeoutSeconds) {
  const int enabled = 1;
#ifdef SO_NOSIGPIPE
  if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0) {
    throw std::runtime_error("Failed to configure socket.");
  }
#endif
  if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) != 0) {
    throw std::runtime_error("Failed to configure socket.");
  }
  timeval timeout{};
  const auto whole = static_cast<time_t>(timeoutSeconds);
  timeout.tv_sec = whole < 1 ? 1 : whole;
  timeout.tv_usec = 0;
  if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
      ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
    throw std::runtime_error("Failed to configure socket.");
  }
}

}  // namespace

struct NdjsonClient::Tls {
  SSL_CTX* context{nullptr};
  SSL* ssl{nullptr};

  ~Tls() {
    if (ssl != nullptr) {
      SSL_free(ssl);
    }
    if (context != nullptr) {
      SSL_CTX_free(context);
    }
  }
};

NdjsonClient::NdjsonClient(const std::string& host, std::uint16_t port, const std::filesystem::path& caFile,
                           double timeoutSeconds)
    : host_(host), port_(port), caFile_(caFile), timeoutSeconds_(timeoutSeconds) {
  try {
    if (caFile_.empty()) {
      connectPlain();
    } else {
      connectTls();
    }
  } catch (...) {
    close();
    throw;
  }
}

NdjsonClient::~NdjsonClient() { close(); }

void NdjsonClient::connectPlain() {
  fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    throw std::runtime_error("Failed to open client socket.");
  }
  configureSocket(fd_, timeoutSeconds_);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port_);
  if (::inet_pton(AF_INET, host_.c_str(), &address.sin_addr) != 1 ||
      ::connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    throw std::runtime_error("Failed to connect to the server.");
  }
}

void NdjsonClient::connectTls() {
  connectPlain();
  tls_ = new Tls();
  OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
  tls_->context = SSL_CTX_new(TLS_client_method());
  if (tls_->context == nullptr) {
    throw std::runtime_error("Failed to verify the server certificate.");
  }
  SSL_CTX_set_min_proto_version(tls_->context, TLS1_2_VERSION);
  SSL_CTX_set_verify(tls_->context, SSL_VERIFY_PEER, nullptr);
  if (SSL_CTX_load_verify_locations(tls_->context, caFile_.string().c_str(), nullptr) != 1) {
    throw std::runtime_error("Failed to verify the server certificate.");
  }
  tls_->ssl = SSL_new(tls_->context);
  if (tls_->ssl == nullptr) {
    throw std::runtime_error("Failed to verify the server certificate.");
  }
  SSL_set_fd(tls_->ssl, fd_);
  SSL_set_mode(tls_->ssl, SSL_MODE_AUTO_RETRY);
  X509_VERIFY_PARAM* parameters = SSL_get0_param(tls_->ssl);
  unsigned char address[4] = {};
  if (::inet_pton(AF_INET, host_.c_str(), address) == 1) {
    if (X509_VERIFY_PARAM_set1_ip(parameters, address, sizeof(address)) != 1) {
      throw std::runtime_error("Failed to verify the server certificate.");
    }
  } else if (X509_VERIFY_PARAM_set1_host(parameters, host_.c_str(), 0) != 1) {
    throw std::runtime_error("Failed to verify the server certificate.");
  }
  if (SSL_connect(tls_->ssl) != 1) {
    const long verify = SSL_get_verify_result(tls_->ssl);
    throw std::runtime_error("Failed to verify the server certificate (" + std::to_string(verify) + ").");
  }
}

std::string NdjsonClient::exchange(std::string_view payload) {
  std::string request(payload);
  if (request.empty() || request.back() != '\n') {
    request.push_back('\n');
  }
  sendAll(request);
  return readLine();
}

void NdjsonClient::sendAll(std::string_view data) {
  while (!data.empty()) {
    const int written = tls_ == nullptr ? static_cast<int>(::send(fd_, data.data(), data.size(), 0))
                                        : SSL_write(tls_->ssl, data.data(), static_cast<int>(data.size()));
    if (written < 0) {
      if (tls_ == nullptr && errno == EINTR) {
        continue;
      }
      if (tls_ == nullptr && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        throw TimeoutError();
      }
      throw std::runtime_error("Failed to send to the server.");
    }
    if (written == 0) {
      throw std::runtime_error("Failed to send to the server.");
    }
    data.remove_prefix(static_cast<std::size_t>(written));
  }
}

std::string NdjsonClient::readLine() {
  std::string line;
  char character = 0;
  for (;;) {
    int count = 0;
    if (tls_ == nullptr) {
      const ssize_t received = ::recv(fd_, &character, 1, 0);
      if (received < 0 && errno == EINTR) {
        continue;
      }
      if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        throw TimeoutError();
      }
      count = static_cast<int>(received);
    } else {
      count = SSL_read(tls_->ssl, &character, 1);
      if (count <= 0) {
        const int reason = SSL_get_error(tls_->ssl, count);
        if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE) {
          continue;
        }
        if (reason == SSL_ERROR_SYSCALL && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
          if (errno == EINTR) {
            continue;
          }
          throw TimeoutError();
        }
        throw std::runtime_error("Failed to read from the server.");
      }
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

void NdjsonClient::close() {
  delete tls_;
  tls_ = nullptr;
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

}  // namespace gen
