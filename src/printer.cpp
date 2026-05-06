#include "src/include/printer.hpp"
#include "src/include/opcodes.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

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

std::string resolvedAnnotation(const std::optional<ResolvedInfo> &r) {
  if (!r)
    return " @(unresolved)";
  return " @(depth=" + std::to_string(r->scopeDepth) +
         ", index=" + std::to_string(r->bindingIndex) + ")";
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
          std::cout << ind(depth) << "Identifier " << node.name
                    << resolvedAnnotation(node.resolved) << '\n';

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
          std::cout << ind(depth) << "Let " << node.name
                    << resolvedAnnotation(node.resolved) << '\n';
          printExprImpl(*node.value, depth + 1);

        } else if constexpr (std::is_same_v<T, Lambda>) {
          std::cout << ind(depth) << "Lambda [";
          for (size_t i = 0; i < node.params.size(); ++i) {
            if (i > 0)
              std::cout << ", ";
            std::cout << node.params[i].name;
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

std::string valueToString(const Value &v) noexcept {
  return std::visit(
      [](const auto &val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, double>) {
          if (val == std::floor(val) && std::isfinite(val)) {
            std::ostringstream ss;
            ss << static_cast<long long>(val);
            return ss.str();
          }
          std::ostringstream ss;
          ss << val;
          return ss.str();
        } else if constexpr (std::is_same_v<T, bool>) {
          return val ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
          return val;
        } else if constexpr (std::is_same_v<T, Unit>) {
          return "unit";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Closure>>) {
          return "<closure/" + std::to_string(val->arity) + ">";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Namespace>>) {
          std::string s = "<namespace {";
          bool first = true;
          for (const auto &[name, _] : *val->fields) {
            if (!first)
              s += ", ";
            s += name;
            first = false;
          }
          s += "}>";
          return s;
        }
      },
      v);
}

std::string_view opcodeName(OpCode op) noexcept {
  switch (op) {
  case OpCode::PushConst:
    return "PUSH_CONST";
  case OpCode::PushBool:
    return "PUSH_BOOL";
  case OpCode::PushUnit:
    return "PUSH_UNIT";
  case OpCode::Load:
    return "LOAD";
  case OpCode::Store:
    return "STORE";
  case OpCode::StoreClosure:
    return "STORE_CLOSURE";
  case OpCode::LoadUpvalue:
    return "LOAD_UPVALUE";
  case OpCode::Add:
    return "ADD";
  case OpCode::Sub:
    return "SUB";
  case OpCode::Mul:
    return "MUL";
  case OpCode::Div:
    return "DIV";
  case OpCode::Mod:
    return "MOD";
  case OpCode::Pow:
    return "POW";
  case OpCode::Neg:
    return "NEG";
  case OpCode::Not:
    return "NOT";
  case OpCode::Eq:
    return "EQ";
  case OpCode::Neq:
    return "NEQ";
  case OpCode::Lt:
    return "LT";
  case OpCode::LtEq:
    return "LTEQ";
  case OpCode::Gt:
    return "GT";
  case OpCode::GtEq:
    return "GTEQ";
  case OpCode::Jump:
    return "JUMP";
  case OpCode::JumpFalse:
    return "JUMP_FALSE";
  case OpCode::Call:
    return "CALL";
  case OpCode::TailCall:
    return "TAIL_CALL";
  case OpCode::Return:
    return "RETURN";
  case OpCode::Pop:
    return "POP";
  case OpCode::MakeClosure:
    return "MAKE_CLOSURE";
  case OpCode::GetField:
    return "GET_FIELD";
  case OpCode::Print:
    return "PRINT";
  case OpCode::Panic:
    return "PANIC";
  }
  return "UNKNOWN";
}

void printChunk(const std::shared_ptr<Chunk> &chunk) {
  const int W = 52;
  auto rule = [&](char c) { std::cout << std::string(W, c) << '\n'; };
  auto header = [&](const std::string &title) {
    rule('-');
    std::cout << "  " << title << '\n';
    rule('-');
  };

  std::cout << '\n';
  header("bytecode [" + chunk->name + "]");
  std::cout << '\n';
  std::cout << std::left << std::setw(8) << "  off" << std::setw(20) << "opcode"
            << "operand\n";
  std::cout << '\n';

  for (size_t i = 0; i < chunk->code.size(); ++i) {
    uint8_t byte = chunk->code[i];
    auto op = static_cast<OpCode>(byte);

    std::cout << "  " << std::right << std::setw(4) << i << "  " << std::left
              << std::setw(20) << opcodeName(op);

    if (op == OpCode::Jump || op == OpCode::JumpFalse) {
      uint8_t lo = chunk->code[++i];
      uint8_t hi = chunk->code[++i];
      int16_t offset = static_cast<int16_t>(lo | (hi << 8));
      std::cout << offset << "  ; -> " << (i + 1 + offset);
    } else if (op == OpCode::MakeClosure) {
      uint8_t idx = chunk->code[++i];
      uint8_t upvalCount = chunk->code[++i];
      std::cout << static_cast<int>(idx)
                << "  ; upvalues: " << static_cast<int>(upvalCount);
      for (uint8_t u = 0; u < upvalCount; ++u) {
        uint8_t slot = chunk->code[++i];
        std::cout << "  [" << static_cast<int>(u) << ": slot "
                  << static_cast<int>(slot) << "]";
      }
    } else {
      switch (op) {
      case OpCode::PushConst: {
        uint8_t idx = chunk->code[++i];
        std::cout << static_cast<int>(idx);
        if (idx < chunk->constants.size())
          std::cout << "  ; " << valueToString(chunk->constants[idx]);
        break;
      }
      case OpCode::PushBool:
      case OpCode::Load:
      case OpCode::Store:
      case OpCode::LoadUpvalue:
      case OpCode::Call:
      case OpCode::TailCall:
        if (i + 1 < chunk->code.size())
          std::cout << static_cast<int>(chunk->code[++i]);
        break;
      default:
        break;
      }
    }

    std::cout << '\n';
  }

  std::cout << '\n';
  header("constants [" + chunk->name + "]");
  std::cout << '\n';

  if (chunk->constants.empty()) {
    std::cout << "  (none)\n";
  } else {
    for (size_t i = 0; i < chunk->constants.size(); ++i) {
      std::cout << "  " << std::right << std::setw(4) << i << "  "
                << valueToString(chunk->constants[i]) << '\n';
    }
  }

  std::cout << '\n';

  for (const Value &v : chunk->constants) {
    if (const auto *cl = std::get_if<std::shared_ptr<Closure>>(&v))
      printChunk((*cl)->chunk);
  }
}