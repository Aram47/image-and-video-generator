#pragma once

#include <cstddef>
#include <string_view>

namespace gen {

struct Utf8Length {
  bool valid{false};
  std::size_t codePoints{0};
};

Utf8Length utf8Length(std::string_view text);

}  // namespace gen
