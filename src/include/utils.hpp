#pragma once

#include <cstdint>
#include <string>
#include <string_view>

inline std::string getSourceLine(std::string_view source, uint32_t lineNum) {
  uint32_t current = 1;
  for (size_t i = 0; i < source.size(); ++i) {
    if (current == lineNum) {
      size_t end = source.find('\n', i);
      if (end == std::string_view::npos)
        end = source.size();
      return std::string(source.substr(i, end - i));
    }
    if (source[i] == '\n')
      ++current;
  }
  return {};
}