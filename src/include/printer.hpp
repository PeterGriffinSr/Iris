#pragma once

#include "src/include/ast.hpp"
#include "src/include/opcodes.hpp"
#include "src/include/token.hpp"
#include "src/include/value.hpp"
#include <span>

[[nodiscard]] std::string_view tokenTypeName(TokenType type) noexcept;
void printToken(const Token &tok);
void printTokens(std::span<const Token> tokens);

void printExpr(const Expr &expr, int indent = 0);
void printAst(std::span<const ExprPtr> exprs);

[[nodiscard]] std::string valueToString(const Value &v) noexcept;
[[nodiscard]] std::string_view opcodeName(OpCode op) noexcept;

void printChunk(const std::shared_ptr<Chunk> &chunk);