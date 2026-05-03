#pragma once

#include "src/include/span.hpp"
#include <string>

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
  Span span;
};