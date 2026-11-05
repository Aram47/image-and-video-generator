#include "common/token_verifier.hpp"

#include <openssl/crypto.h>

#include <algorithm>
#include <cstring>

namespace gen {

void TokenVerifier::require(std::string expected) {
  required_ = true;
  expected_ = std::move(expected);
}

void TokenVerifier::disable() {
  required_ = false;
  expected_.clear();
}

bool TokenVerifier::accepts(std::string_view provided) const {
  if (!required_) {
    return true;
  }
  if (expected_.empty()) {
    return false;
  }

  // Compare the whole expected token. A length mismatch still walks that buffer
  // so the call does not return on the first different byte.
  const std::size_t length = expected_.size();
  std::string buffer(length, '\0');
  const std::size_t copyLength = std::min(length, provided.size());
  if (copyLength > 0) {
    std::memcpy(buffer.data(), provided.data(), copyLength);
  }
  const unsigned char lengthMismatch = expected_.size() == provided.size() ? 0 : 1;
  const int difference = CRYPTO_memcmp(expected_.data(), buffer.data(), length);
  return (difference | lengthMismatch) == 0;
}

}  // namespace gen
