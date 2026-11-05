#include "net/tls_context.hpp"

#include "net/tls_stream.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <stdexcept>

namespace gen {
namespace {

struct SslContextDeleter {
  void operator()(SSL_CTX* context) const { SSL_CTX_free(context); }
};

[[noreturn]] void rejectTls() { throw std::invalid_argument("TLS certificate and key are required."); }

}  // namespace

struct TlsContext::Context {
  std::unique_ptr<SSL_CTX, SslContextDeleter> ssl;
};

TlsContext::TlsContext(const std::filesystem::path& certificate, const std::filesystem::path& key)
    : context_(std::make_unique<Context>()) {
  OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
  context_->ssl.reset(SSL_CTX_new(TLS_server_method()));
  if (!context_->ssl) {
    rejectTls();
  }
  SSL_CTX_set_min_proto_version(context_->ssl.get(), TLS1_2_VERSION);
  if (SSL_CTX_use_certificate_file(context_->ssl.get(), certificate.string().c_str(), SSL_FILETYPE_PEM) != 1 ||
      SSL_CTX_use_PrivateKey_file(context_->ssl.get(), key.string().c_str(), SSL_FILETYPE_PEM) != 1 ||
      SSL_CTX_check_private_key(context_->ssl.get()) != 1) {
    rejectTls();
  }
}

TlsContext::~TlsContext() = default;

std::unique_ptr<ByteStream> TlsContext::accept(Socket socket) {
  SSL* ssl = SSL_new(context_->ssl.get());
  if (ssl == nullptr) {
    return nullptr;
  }
  SSL_set_fd(ssl, socket.fd());
  SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
  if (SSL_accept(ssl) != 1) {
    SSL_free(ssl);
    return nullptr;
  }
  return std::make_unique<TlsStream>(std::move(socket), ssl);
}

}  // namespace gen
