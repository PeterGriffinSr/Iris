#include <Iris/Debug/printer.hpp>
#include <Iris/Runtime/opcodes.hpp>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Iris::Debug {

std::string_view tokenTypeName(Frontend::TokenType type) noexcept {
  switch (type) {
  case Frontend::TokenType::Keyword:
    return "Keyword";
  case Frontend::TokenType::Number:
    return "Number";
  case Frontend::TokenType::String:
    return "String";
  case Frontend::TokenType::Operator:
    return "Operator";
  case Frontend::TokenType::Delimiter:
    return "Delimiter";
  case Frontend::TokenType::Identifier:
    return "Identifier";
  case Frontend::TokenType::Semicolon:
    return "Semicolon";
  case Frontend::TokenType::Eof:
    return "Eof";
  case Frontend::TokenType::Error:
    return "Error";
  }
  return "Unknown";
}

void printToken(const Frontend::Token &tok) {
  std::cout << std::right << std::setw(3) << tok.span.startLine << ':'
            << std::left << std::setw(4) << tok.span.startCol << std::setw(12)
            << tokenTypeName(tok.type)
            << (tok.type == Frontend::TokenType::Semicolon ? "<;>" : tok.value)
            << '\n';
}

void printTokens(std::span<const Frontend::Token> tokens) {
  std::cout << std::left << std::setw(8) << "Ln:Col" << std::setw(12) << "Type"
            << "Value\n"
            << std::string(40, '-') << '\n';

  for (const Frontend::Token &tok : tokens)
    printToken(tok);
}

const std::string INDENT_UNIT = "  ";

std::string ind(int depth) {
  std::string s;
  s.reserve(depth * 2);
  for (int i = 0; i < depth; ++i)
    s += INDENT_UNIT;
  return s;
}

std::string_view binaryOpName(Frontend::BinaryOp op) noexcept {
  switch (op) {
  case Frontend::BinaryOp::Addition:
    return "+";
  case Frontend::BinaryOp::Subtraction:
    return "-";
  case Frontend::BinaryOp::Multiplication:
    return "*";
  case Frontend::BinaryOp::Division:
    return "/";
  case Frontend::BinaryOp::Modulo:
    return "%";
  case Frontend::BinaryOp::Power:
    return "^";
  case Frontend::BinaryOp::Equal:
    return "==";
  case Frontend::BinaryOp::NotEqual:
    return "!=";
  case Frontend::BinaryOp::LessThan:
    return "<";
  case Frontend::BinaryOp::LessThanEqual:
    return "<=";
  case Frontend::BinaryOp::GreaterThan:
    return ">";
  case Frontend::BinaryOp::GreaterThanEqual:
    return ">=";
  case Frontend::BinaryOp::And:
    return "&&";
  case Frontend::BinaryOp::Or:
    return "||";
  }
  return "?";
}

std::string_view unaryOpName(Frontend::UnaryOp op) noexcept {
  switch (op) {
  case Frontend::UnaryOp::Negation:
    return "-";
  case Frontend::UnaryOp::Not:
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

void printExprImpl(const Frontend::Expr &expr, int depth);

void printBlock(const Frontend::Block &block, int depth) {
  std::cout << ind(depth) << "Block\n";
  for (const Frontend::ExprPtr &e : block.exprs)
    printExprImpl(*e, depth + 1);
}

void printExprImpl(const Frontend::Expr &expr, int depth) {
  visit(
      [&](const auto &node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, Frontend::NumericLiteral>) {
          std::cout << ind(depth) << "NumericLiteral " << node.value << '\n';

        } else if constexpr (std::is_same_v<T, Frontend::StringLiteral>) {
          std::cout << ind(depth) << "StringLiteral \"" << node.value << "\"\n";

        } else if constexpr (std::is_same_v<T, Frontend::BoolLiteral>) {
          std::cout << ind(depth) << "BoolLiteral "
                    << (node.value ? "true" : "false") << '\n';

        } else if constexpr (std::is_same_v<T, Frontend::Identifier>) {
          std::cout << ind(depth) << "Identifier " << node.name
                    << resolvedAnnotation(node.resolved) << '\n';

        } else if constexpr (std::is_same_v<T, Frontend::Unary>) {
          std::cout << ind(depth) << "Unary " << unaryOpName(node.op) << '\n';
          printExprImpl(*node.rhs, depth + 1);

        } else if constexpr (std::is_same_v<T, Frontend::Binary>) {
          std::cout << ind(depth) << "Binary " << binaryOpName(node.op) << '\n';
          printExprImpl(*node.lhs, depth + 1);
          printExprImpl(*node.rhs, depth + 1);

        } else if constexpr (std::is_same_v<T, Frontend::If>) {
          std::cout << ind(depth) << "If\n";
          std::cout << ind(depth + 1) << "Cond\n";
          printExprImpl(*node.cond, depth + 2);
          std::cout << ind(depth + 1) << "Then\n";
          printBlock(node.then, depth + 2);
          if (node.els) {
            std::cout << ind(depth + 1) << "Else\n";
            printBlock(*node.els, depth + 2);
          }

        } else if constexpr (std::is_same_v<T, Frontend::Call>) {
          std::cout << ind(depth) << "Call\n";
          std::cout << ind(depth + 1) << "Callee\n";
          printExprImpl(*node.callee, depth + 2);
          if (!node.args.empty()) {
            std::cout << ind(depth + 1) << "Args\n";
            for (const Frontend::ExprPtr &arg : node.args)
              printExprImpl(*arg, depth + 2);
          }

        } else if constexpr (std::is_same_v<T, Frontend::Block>) {
          printBlock(node, depth);

        } else if constexpr (std::is_same_v<T, Frontend::Let>) {
          std::cout << ind(depth) << "Let " << node.name
                    << resolvedAnnotation(node.resolved) << '\n';
          printExprImpl(*node.value, depth + 1);

        } else if constexpr (std::is_same_v<T, Frontend::Lambda>) {
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

void printExpr(const Frontend::Expr &expr, int indent) {
  printExprImpl(expr, indent);
}

void printAst(std::span<const Frontend::ExprPtr> exprs) {
  for (const Frontend::ExprPtr &e : exprs)
    printExprImpl(*e, 0);
}

std::string valueToString(const Runtime::Value &v) noexcept {
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
        } else if constexpr (std::is_same_v<T, Runtime::Unit>) {
          return "unit";
        } else if constexpr (std::is_same_v<
                                 T, std::shared_ptr<Runtime::Closure>>) {
          return "<closure/" + std::to_string(val->arity) + ">";
        } else if constexpr (std::is_same_v<
                                 T, std::shared_ptr<Runtime::Namespace>>) {
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

std::string_view opcodeName(Runtime::OpCode op) noexcept {
  switch (op) {
  case Runtime::OpCode::PushConst:
    return "PUSH_CONST";
  case Runtime::OpCode::PushBool:
    return "PUSH_BOOL";
  case Runtime::OpCode::PushUnit:
    return "PUSH_UNIT";
  case Runtime::OpCode::Load:
    return "LOAD";
  case Runtime::OpCode::Store:
    return "STORE";
  case Runtime::OpCode::StoreClosure:
    return "STORE_CLOSURE";
  case Runtime::OpCode::LoadUpvalue:
    return "LOAD_UPVALUE";
  case Runtime::OpCode::Add:
    return "ADD";
  case Runtime::OpCode::Sub:
    return "SUB";
  case Runtime::OpCode::Mul:
    return "MUL";
  case Runtime::OpCode::Div:
    return "DIV";
  case Runtime::OpCode::Mod:
    return "MOD";
  case Runtime::OpCode::Pow:
    return "POW";
  case Runtime::OpCode::Neg:
    return "NEG";
  case Runtime::OpCode::Not:
    return "NOT";
  case Runtime::OpCode::Eq:
    return "EQ";
  case Runtime::OpCode::Neq:
    return "NEQ";
  case Runtime::OpCode::Lt:
    return "LT";
  case Runtime::OpCode::LtEq:
    return "LTEQ";
  case Runtime::OpCode::Gt:
    return "GT";
  case Runtime::OpCode::GtEq:
    return "GTEQ";
  case Runtime::OpCode::Jump:
    return "JUMP";
  case Runtime::OpCode::JumpFalse:
    return "JUMP_FALSE";
  case Runtime::OpCode::Call:
    return "CALL";
  case Runtime::OpCode::TailCall:
    return "TAIL_CALL";
  case Runtime::OpCode::Return:
    return "RETURN";
  case Runtime::OpCode::Pop:
    return "POP";
  case Runtime::OpCode::MakeClosure:
    return "MAKE_CLOSURE";
  case Runtime::OpCode::GetField:
    return "GET_FIELD";
  case Runtime::OpCode::Print:
    return "PRINT";
  case Runtime::OpCode::Panic:
    return "PANIC";
  }
  return "UNKNOWN";
}

void printChunk(const std::shared_ptr<Runtime::Chunk> &chunk) {
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
    auto op = static_cast<Runtime::OpCode>(byte);

    std::cout << "  " << std::right << std::setw(4) << i << "  " << std::left
              << std::setw(20) << opcodeName(op);

    if (op == Runtime::OpCode::Jump || op == Runtime::OpCode::JumpFalse) {
      uint8_t lo = chunk->code[++i];
      uint8_t hi = chunk->code[++i];
      int16_t offset = static_cast<int16_t>(lo | (hi << 8));
      std::cout << offset << "  ; -> " << (i + 1 + offset);
    } else if (op == Runtime::OpCode::MakeClosure) {
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
      case Runtime::OpCode::PushConst: {
        uint8_t idx = chunk->code[++i];
        std::cout << static_cast<int>(idx);
        if (idx < chunk->constants.size())
          std::cout << "  ; " << valueToString(chunk->constants[idx]);
        break;
      }
      case Runtime::OpCode::PushBool:
      case Runtime::OpCode::Load:
      case Runtime::OpCode::Store:
      case Runtime::OpCode::LoadUpvalue:
      case Runtime::OpCode::Call:
      case Runtime::OpCode::TailCall:
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

  for (const Runtime::Value &v : chunk->constants) {
    if (const auto *cl = std::get_if<std::shared_ptr<Runtime::Closure>>(&v))
      printChunk((*cl)->chunk);
  }
}
} // namespace Iris::Debug