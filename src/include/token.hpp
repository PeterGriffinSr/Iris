#pragma once

#include <cstdint>
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
  uint32_t line, column;
};