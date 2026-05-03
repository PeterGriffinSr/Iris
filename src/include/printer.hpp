#pragma once

#include "src/include/ast.hpp"
#include "src/include/token.hpp"
#include <span>

[[nodiscard]] std::string_view tokenTypeName(TokenType type) noexcept;
void printToken(const Token &tok);
void printTokens(std::span<const Token> tokens);

void printExpr(const Expr &expr, int indent = 0);
void printAst(std::span<const ExprPtr> exprs);