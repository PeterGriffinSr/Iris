#include "src/include/printer.hpp"
#include <iomanip>
#include <iostream>

std::string_view tokenTypeName(TokenType type) noexcept {
  switch (type) {
  case TokenType::Keyword:
    return "Keyword";
  case TokenType::Number:
    return "Number";
  case TokenType::String:
    return "String";
  case TokenType::Operator:
    return "Operator";
  case TokenType::Delimiter:
    return "Delimiter";
  case TokenType::Identifier:
    return "Identifier";
  case TokenType::Semicolon:
    return "Semicolon";
  case TokenType::Eof:
    return "Eof";
  case TokenType::Error:
    return "Error";
  }
  return "Unknown";
}

void printToken(const Token &tok) {
  std::cout << std::right << std::setw(3) << tok.span.startLine << ':'
            << std::left << std::setw(4) << tok.span.startCol << std::setw(12)
            << tokenTypeName(tok.type)
            << (tok.type == TokenType::Semicolon ? "<;>" : tok.value) << '\n';
}

void printTokens(std::span<const Token> tokens) {
  std::cout << std::left << std::setw(8) << "Ln:Col" << std::setw(12) << "Type"
            << "Value\n"
            << std::string(40, '-') << '\n';

  for (const Token &tok : tokens)
    printToken(tok);
}

namespace {
const std::string INDENT_UNIT = "  ";

std::string ind(int depth) {
  std::string s;
  s.reserve(depth * 2);
  for (int i = 0; i < depth; ++i)
    s += INDENT_UNIT;
  return s;
}

std::string_view binaryOpName(BinaryOp op) noexcept {
  switch (op) {
  case BinaryOp::Addition:
    return "+";
  case BinaryOp::Subtraction:
    return "-";
  case BinaryOp::Multiplication:
    return "*";
  case BinaryOp::Division:
    return "/";
  case BinaryOp::Modulo:
    return "%";
  case BinaryOp::Power:
    return "^";
  case BinaryOp::Equal:
    return "==";
  case BinaryOp::NotEqual:
    return "!=";
  case BinaryOp::LessThan:
    return "<";
  case BinaryOp::LessThanEqual:
    return "<=";
  case BinaryOp::GreaterThan:
    return ">";
  case BinaryOp::GreaterThanEqual:
    return ">=";
  case BinaryOp::And:
    return "&&";
  case BinaryOp::Or:
    return "||";
  }
  return "?";
}

std::string_view unaryOpName(UnaryOp op) noexcept {
  switch (op) {
  case UnaryOp::Negation:
    return "-";
  case UnaryOp::Not:
    return "!";
  }
  return "?";
}

void printExprImpl(const Expr &expr, int depth);

void printBlock(const Block &block, int depth) {
  std::cout << ind(depth) << "Block\n";
  for (const ExprPtr &e : block.exprs)
    printExprImpl(*e, depth + 1);
}

void printExprImpl(const Expr &expr, int depth) {
  visit(
      [&](const auto &node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, NumericLiteral>) {
          std::cout << ind(depth) << "NumericLiteral " << node.value << '\n';
        } else if constexpr (std::is_same_v<T, StringLiteral>) {
          std::cout << ind(depth) << "StringLiteral \"" << node.value << "\"\n";
        } else if constexpr (std::is_same_v<T, BoolLiteral>) {
          std::cout << ind(depth) << "BoolLiteral "
                    << (node.value ? "true" : "false") << '\n';
        } else if constexpr (std::is_same_v<T, Identifier>) {
          std::cout << ind(depth) << "Identifier " << node.name << '\n';
        } else if constexpr (std::is_same_v<T, Unary>) {
          std::cout << ind(depth) << "Unary " << unaryOpName(node.op) << '\n';
          printExprImpl(*node.rhs, depth + 1);
        } else if constexpr (std::is_same_v<T, Binary>) {
          std::cout << ind(depth) << "Binary " << binaryOpName(node.op) << '\n';
          printExprImpl(*node.lhs, depth + 1);
          printExprImpl(*node.rhs, depth + 1);
        } else if constexpr (std::is_same_v<T, If>) {
          std::cout << ind(depth) << "If\n";
          std::cout << ind(depth + 1) << "Cond\n";
          printExprImpl(*node.cond, depth + 2);
          std::cout << ind(depth + 1) << "Then\n";
          printBlock(node.then, depth + 2);
          if (node.els) {
            std::cout << ind(depth + 1) << "Else\n";
            printBlock(*node.els, depth + 2);
          }
        } else if constexpr (std::is_same_v<T, Call>) {
          std::cout << ind(depth) << "Call\n";
          std::cout << ind(depth + 1) << "Callee\n";
          printExprImpl(*node.callee, depth + 2);
          if (!node.args.empty()) {
            std::cout << ind(depth + 1) << "Args\n";
            for (const ExprPtr &arg : node.args)
              printExprImpl(*arg, depth + 2);
          }
        } else if constexpr (std::is_same_v<T, Block>) {
          printBlock(node, depth);
        } else if constexpr (std::is_same_v<T, Let>) {
          std::cout << ind(depth) << "Let " << node.name << '\n';
          printExprImpl(*node.value, depth + 1);
        } else if constexpr (std::is_same_v<T, Lambda>) {
          std::cout << ind(depth) << "Lambda [";
          for (size_t i = 0; i < node.params.size(); ++i) {
            if (i > 0)
              std::cout << ", ";
            std::cout << node.params[i];
          }
          std::cout << "]\n";
          printBlock(node.body, depth + 1);
        }
      },
      expr);
}

} // namespace

void printExpr(const Expr &expr, int indent) { printExprImpl(expr, indent); }

void printAst(std::span<const ExprPtr> exprs) {
  for (const ExprPtr &e : exprs)
    printExprImpl(*e, 0);
}
