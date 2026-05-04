#pragma once

#include <cstdint>
#include <variant>

enum class LexerError : uint32_t {
  UnterminatedString = 2,
  UnknownEscape = 3,
};

enum class ParserError : uint32_t {
  ExpectedIdentifierAfterLet = 200,
  ExpectedParameterName = 201,
  ExpectedClosingParen = 202,
  ExpectedBlockAfterParams = 203,
  ExpectedEquals = 204,
  ExpectedIfCondition = 205,
  ExpectedThenBlock = 206,
  ExpectedElseBlock = 207,
  ExpectedRhsAfterBinaryOp = 208,
  ExpectedRhsAfterUnaryOp = 209,
  ExpectedClosingCallParen = 210,
  ExpectedClosingGroupParen = 211,
  UnexpectedToken = 212,
  ExpectedClosingBrace = 213,
  EmptyBlock = 214,
};

enum class ResolutionError : uint32_t {};
enum class TypeError : uint32_t {};
enum class RuntimeError : uint32_t {};
enum class IOError : uint32_t {
  FileNotFound = 100,
  FileUnreadable = 101,
};

using Error = std::variant<LexerError, ParserError, ResolutionError, TypeError,
                           RuntimeError, IOError>;