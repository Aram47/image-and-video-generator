#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace gen {

class NdjsonClient {
 public:
  NdjsonClient(const std::string& host, std::uint16_t port, const std::filesystem::path& caFile, double timeoutSeconds);
  ~NdjsonClient();

  NdjsonClient(const NdjsonClient&) = delete;
  NdjsonClient& operator=(const NdjsonClient&) = delete;

  std::string exchange(std::string_view payload);

 private:
  void connectPlain();
  void connectTls();
  void sendAll(std::string_view data);
  std::string readLine();
  void close();

  std::string host_;
  std::uint16_t port_;
  std::filesystem::path caFile_;
  double timeoutSeconds_;
  int fd_{-1};
  struct Tls;
  Tls* tls_{nullptr};
};

}  // namespace gen
