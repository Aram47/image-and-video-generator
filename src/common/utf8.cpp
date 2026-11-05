#include "common/utf8.hpp"

namespace gen {

Utf8Length utf8Length(std::string_view text) {
  Utf8Length result;
  result.valid = true;
  for (std::size_t index = 0; index < text.size();) {
    const auto lead = static_cast<unsigned char>(text[index]);
    std::size_t width = 1;
    if (lead <= 0x7F) {
      width = 1;
    } else if ((lead & 0xE0) == 0xC0) {
      width = 2;
    } else if ((lead & 0xF0) == 0xE0) {
      width = 3;
    } else if ((lead & 0xF8) == 0xF0) {
      width = 4;
    } else {
      result.valid = false;
      result.codePoints = 0;
      return result;
    }
    if (index + width > text.size()) {
      result.valid = false;
      result.codePoints = 0;
      return result;
    }
    for (std::size_t offset = 1; offset < width; ++offset) {
      const auto continuation = static_cast<unsigned char>(text[index + offset]);
      if ((continuation & 0xC0) != 0x80) {
        result.valid = false;
        result.codePoints = 0;
        return result;
      }
    }
    index += width;
    ++result.codePoints;
  }
  return result;
}

}  // namespace gen
