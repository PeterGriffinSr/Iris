#pragma once

#include <Iris/Frontend/ast.hpp>
#include <Iris/Frontend/token.hpp>
#include <Iris/Runtime/opcodes.hpp>
#include <Iris/Runtime/value.hpp>
#include <span>

namespace Iris::Debug {

[[nodiscard]] std::string_view tokenTypeName(Frontend::TokenType type) noexcept;
void printToken(const Frontend::Token &tok);
void printTokens(std::span<const Frontend::Token> tokens);

void printExpr(const Frontend::Expr &expr, int indent = 0);
void printAst(std::span<const Frontend::ExprPtr> exprs);

[[nodiscard]] std::string valueToString(const Runtime::Value &v) noexcept;
[[nodiscard]] std::string_view opcodeName(Runtime::OpCode op) noexcept;

void printChunk(const std::shared_ptr<Runtime::Chunk> &chunk);
} // namespace Iris::Debug