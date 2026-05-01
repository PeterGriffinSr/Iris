#pragma once

#include <cstdint>
#include <variant>

enum class LexerError : uint32_t {
  UnterminatedString = 2,
  UnknownEscape = 3,
};

enum class ParserError : uint32_t {};
enum class ResolutionError : uint32_t {};
enum class TypeError : uint32_t {};
enum class RuntimeError : uint32_t {};
enum class IOError : uint32_t {
  FileNotFound = 100,
  FileUnreadable = 101,
};

using Error = std::variant<LexerError, ParserError, ResolutionError, TypeError,
                           RuntimeError, IOError>;