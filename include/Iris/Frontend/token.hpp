#pragma once

#include <Iris/Common/span.hpp>
#include <string>

namespace Iris::Frontend {

enum class TokenType {
  Keyword,
  Number,
  String,
  Operator,
  Delimiter,
  Identifier,
  Semicolon,
  Eof,
  Error
};

struct Token {
  TokenType type;
  std::string value;
  Common::Span span;
};
} // namespace Iris::Frontend