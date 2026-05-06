#pragma once

#include "src/include/resolved_info.hpp"
#include "src/include/span.hpp"
#include <memory>
#include <optional>
#include <variant>
#include <vector>

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class BinaryOp {
  Addition,
  Subtraction,
  Multiplication,
  Division,
  Modulo,
  Power,
  Equal,
  NotEqual,
  LessThan,
  LessThanEqual,
  GreaterThan,
  GreaterThanEqual,
  And,
  Or,
};

enum class UnaryOp { Negation, Not };

struct NumericLiteral {
  double value;
  Span span;
};

struct StringLiteral {
  std::string value;
  Span span;
};

struct BoolLiteral {
  bool value;
  Span span;
};

struct Identifier {
  std::string name;
  Span span;
  std::optional<ResolvedInfo> resolved = std::nullopt;
};

struct Unary {
  UnaryOp op;
  ExprPtr rhs;
  Span span;
};

struct Binary {
  ExprPtr lhs;
  BinaryOp op;
  ExprPtr rhs;
  Span span;
};

struct Block {
  std::vector<ExprPtr> exprs;
  Span span;
};

struct If {
  ExprPtr cond;
  Block then;
  std::optional<Block> els;
  Span span;
};

struct Call {
  ExprPtr callee;
  std::vector<ExprPtr> args;
  Span span;
};

struct Param {
  std::string name;
  uint32_t bindingIndex{0};
};

struct Lambda {
  std::vector<Param> params;
  Block body;
  Span span;
};

struct Let {
  std::string name;
  ExprPtr value;
  Span span;
  std::optional<ResolvedInfo> resolved = std::nullopt;
};

struct Package {
  std::string name;
  Span span;
};

struct Import {
  std::string collection;
  std::vector<std::string> pathSegments;
  std::string localName;
  Span span;
};

struct FieldAccess {
  ExprPtr object;
  std::string field;
  Span span;
};

using ExprVariant = std::variant<NumericLiteral, StringLiteral, BoolLiteral,
                                 Identifier, Unary, Binary, If, Call, Block,
                                 Let, Lambda, Package, Import, FieldAccess>;

struct Expr {
  ExprVariant val;

  template <typename T> Expr(T &&node) : val(std::forward<T>(node)) {}
};

template <typename T> ExprPtr makeExpr(T &&node) {
  return std::make_unique<Expr>(std::forward<T>(node));
}

template <typename Visitor> auto visit(Visitor &&v, const Expr &e) {
  return std::visit(std::forward<Visitor>(v), e.val);
}

template <typename Visitor> auto visit(Visitor &&v, Expr &e) {
  return std::visit(std::forward<Visitor>(v), e.val);
}

inline Span spanOf(const Expr &expr) {
  return visit([](const auto &n) { return n.span; }, expr);
}