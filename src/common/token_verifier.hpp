#pragma once

#include <string>
#include <string_view>

namespace gen {

class TokenVerifier {
 public:
  void require(std::string expected);
  void disable();
  bool accepts(std::string_view provided) const;

 private:
  bool required_{false};
  std::string expected_;
};

}  // namespace gen
