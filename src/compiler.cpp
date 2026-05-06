#include "src/include/compiler.hpp"
#include "src/include/ast.hpp"
#include "src/include/error.hpp"
#include "src/include/utils.hpp"
#include <cassert>

Compiler::Compiler(std::string_view filename, std::string_view source,
                   DiagnosticBag &bag)
    : m_filename(filename), m_source(source), m_bag(bag) {}

void Compiler::injectExternal(uint32_t bindingIndex, const std::string &name,
                              Value value) {
  m_externals.emplace_back(bindingIndex, name, std::move(value));
}

void Compiler::registerBuiltin(uint32_t bindingIndex, const std::string &name) {
  m_builtins[bindingIndex] = name;
}

void Compiler::emitError(std::string msg, std::optional<std::string> hint,
                         Span span, std::optional<Error> code) {
  m_bag.emit(Diagnostic{.severity = Severity::Error,
                        .code = code,
                        .hint = std::move(hint),
                        .message = std::move(msg),
                        .filename = std::string(m_filename),
                        .sourceLine = getSourceLine(m_source, span.startLine),
                        .span = span});
}

void Compiler::emit(uint8_t byte) { current().chunk->code.push_back(byte); }
void Compiler::emitOp(OpCode op) { emit(static_cast<uint8_t>(op)); }
void Compiler::emitOp(OpCode op, uint8_t operand) {
  emit(static_cast<uint8_t>(op));
  emit(operand);
}

void Compiler::emitJump(OpCode op, int16_t offset) {
  emit(static_cast<uint8_t>(op));
  emit(static_cast<uint8_t>(offset & 0xFF));
  emit(static_cast<uint8_t>((offset >> 8) & 0xFF));
}

void Compiler::patchJump(size_t jumpPos) {
  auto &code = current().chunk->code;
  int16_t offset = static_cast<int16_t>(
      static_cast<int>(code.size() - static_cast<int>(jumpPos) - 3));
  code[jumpPos + 1] = static_cast<uint8_t>(offset & 0xFF);
  code[jumpPos + 2] = static_cast<uint8_t>((offset >> 8) & 0xFF);
}

uint8_t Compiler::addConstant(Value v) {
  auto &constants = current().chunk->constants;
  for (size_t i = 0; i < constants.size(); ++i) {
    if (valuesEqual(constants[i], v))
      return static_cast<uint8_t>(i);
  }
  if (constants.size() >= 255) {
    emitError("too many constants in one function (max 255)",
              std::string("split this function into smaller pieces"),
              Span{0, 0, 0, 0}, RuntimeError::TooManyConstants);
    return 0;
  }
  constants.push_back(std::move(v));
  return static_cast<uint8_t>(constants.size() - 1);
}

void Compiler::emitConstant(Value v) {
  uint8_t idx = addConstant(std::move(v));
  emitOp(OpCode::PushConst, idx);
}

uint8_t Compiler::allocSlot(uint32_t bindingIndex) {
  auto &fn = current();
  if (fn.nextSlot == 255) {
    emitError("too many local variables in one function (max 255)",
              std::string("split this function into smaller pieces"),
              Span{0, 0, 0, 0}, RuntimeError::TooManyLocals);
    return 0;
  }
  uint8_t slot = fn.nextSlot++;
  fn.slotMap[bindingIndex] = slot;
  return slot;
}

SlotRef Compiler::lookupSlot(uint32_t bindingIndex) {
  auto &fn = current();
  auto it = fn.slotMap.find(bindingIndex);
  if (it != fn.slotMap.end())
    return {SlotKind::Local, it->second};

  for (auto rit = m_fnStack.rbegin() + 1; rit != m_fnStack.rend(); ++rit) {
    auto found = rit->slotMap.find(bindingIndex);
    if (found != rit->slotMap.end())
      return {SlotKind::Upvalue, addCapture(found->second)};
  }

  assert(false && "slot not found - resolver missed a binding.");
  return {SlotKind::Local, 0};
}

uint8_t Compiler::addCapture(uint8_t outerSlot) {
  auto &fn = current();
  for (size_t i = 0; i < fn.captureSlots.size(); ++i) {
    if (fn.captureSlots[i] == outerSlot)
      return static_cast<uint8_t>(i);
  }
  fn.captureSlots.push_back(outerSlot);
  return static_cast<uint8_t>(fn.captureSlots.size() - 1);
}

std::shared_ptr<Chunk> Compiler::compile(std::span<const ExprPtr> program) {
  m_fnStack.push_back(FnState{
      .chunk = std::make_shared<Chunk>(),
      .nextSlot = 0,
      .captureCount = 0,
      .inTailPos = false,
      .slotMap = {},
      .captureSlots = {},
  });
  current().chunk->name = "<script>";

  for (auto &[bindingIndex, name] : m_builtins)
    allocSlot(bindingIndex);

  for (auto &[bindingIndex, name, value] : m_externals) {
    uint8_t slot = allocSlot(bindingIndex);
    uint8_t idx = addConstant(value);
    emitOp(OpCode::PushConst, idx);
    emitOp(OpCode::Store, slot);
  }

  for (const ExprPtr &expr : program) {
    if (std::holds_alternative<Package>(expr->val))
      continue;
    if (std::holds_alternative<Import>(expr->val))
      continue;

    compileExpr(*expr);
    if (expr.get() != program.back().get())
      emitOp(OpCode::Pop);
  }

  emitOp(OpCode::Return);

  auto chunk = current().chunk;
  chunk->slotCount = current().nextSlot;

  for (const ExprPtr &expr : program) {
    if (const auto *let = std::get_if<Let>(&expr->val)) {
      if (let->resolved) {
        auto it = current().slotMap.find(let->resolved->bindingIndex);
        if (it != current().slotMap.end()) {
          chunk->exportedSlots[let->name] = it->second;
        }
      }
    }
  }

  m_fnStack.pop_back();
  return chunk;
}

void Compiler::compileExpr(const Expr &expr, bool tailPos) {
  std::visit(
      [&](const auto &node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, NumericLiteral>) {
          emitConstant(node.value);
        } else if constexpr (std::is_same_v<T, StringLiteral>) {
          emitConstant(node.value);
        } else if constexpr (std::is_same_v<T, BoolLiteral>) {
          emitOp(OpCode::PushBool, node.value ? 1 : 0);
        } else if constexpr (std::is_same_v<T, Unit>) {
          emitOp(OpCode::PushUnit);
        } else if constexpr (std::is_same_v<T, Identifier>) {
          compileIdentifier(node);
        } else if constexpr (std::is_same_v<T, Let>) {
          compileLet(node);
        } else if constexpr (std::is_same_v<T, Lambda>) {
          compileLambda(node, "<lambda>");
        } else if constexpr (std::is_same_v<T, Block>) {
          compileBlock(node, tailPos);
        } else if constexpr (std::is_same_v<T, If>) {
          compileIf(node, tailPos);
        } else if constexpr (std::is_same_v<T, Call>) {
          compileCall(node, tailPos);
        } else if constexpr (std::is_same_v<T, Binary>) {
          compileBinary(node);
        } else if constexpr (std::is_same_v<T, Unary>) {
          compileUnary(node);
        } else if constexpr (std::is_same_v<T, FieldAccess>) {
          compileFieldAccess(node);
        }
      },
      expr.val);
}

void Compiler::compileLet(const Let &let) {
  assert(let.resolved.has_value() && "Let node not resolved");

  uint8_t slot = allocSlot(let.resolved->bindingIndex);

  if (const Lambda *lam = std::get_if<Lambda>(&let.value->val)) {
    emitOp(OpCode::PushUnit);
    emitOp(OpCode::Store, slot);
    compileLambda(*lam, let.name);
    emitOp(OpCode::StoreClosure, slot);
  } else {
    compileExpr(*let.value);
    emitOp(OpCode::Store, slot);
  }

  emitOp(OpCode::PushUnit);
}

void Compiler::compileLambda(const Lambda &lambda, const std::string &name) {
  m_fnStack.push_back(FnState{
      .chunk = std::make_shared<Chunk>(),
      .nextSlot = 0,
      .captureCount = 0,
      .inTailPos = false,
      .slotMap = {},
      .captureSlots = {},
  });
  current().chunk->name = name;

  assert(lambda.params.size() <= 255 && "too many parameters");
  for (const Param &param : lambda.params) {
    uint8_t slot = current().nextSlot++;
    current().slotMap[param.bindingIndex] = slot;
  }

  compileBlock(lambda.body, true);
  emitOp(OpCode::Return);

  auto fnChunk = current().chunk;
  uint8_t arity = static_cast<uint8_t>(lambda.params.size());
  auto captureSlots = current().captureSlots;
  fnChunk->slotCount = current().nextSlot;
  m_fnStack.pop_back();

  auto closure = std::make_shared<Closure>();
  closure->chunk = fnChunk;
  closure->arity = arity;

  uint8_t idx = addConstant(Value{closure});
  emitOp(OpCode::MakeClosure, idx);
  emit(static_cast<uint8_t>(captureSlots.size()));
  for (uint8_t slot : captureSlots)
    emit(slot);
}

void Compiler::compileBlock(const Block &block, bool tailPos) {
  for (size_t i = 0; i < block.exprs.size(); ++i) {
    bool last = (i == block.exprs.size() - 1);
    compileExpr(*block.exprs[i], tailPos && last);
    if (!last)
      emitOp(OpCode::Pop);
  }
}

void Compiler::compileIf(const If &ifExpr, bool tailPos) {
  compileExpr(*ifExpr.cond);

  size_t jumpFalsePos = current().chunk->code.size();
  emitJump(OpCode::JumpFalse);

  compileBlock(ifExpr.then, tailPos);

  if (ifExpr.els) {
    size_t jumpPos = current().chunk->code.size();
    emitJump(OpCode::Jump);
    patchJump(jumpFalsePos);
    compileBlock(*ifExpr.els, tailPos);
    patchJump(jumpPos);
  } else {
    patchJump(jumpFalsePos);
    emitOp(OpCode::PushUnit);
  }
}

std::string Compiler::resolveBuiltinCall(const Call &call) {
  const Identifier *id = std::get_if<Identifier>(&call.callee->val);
  if (!id || !id->resolved)
    return "";
  auto it = m_builtins.find(id->resolved->bindingIndex);
  if (it == m_builtins.end())
    return "";
  return it->second;
}

void Compiler::compileCall(const Call &call, bool tailPos) {
  std::string builtin = resolveBuiltinCall(call);

  if (builtin == "print") {
    if (call.args.size() != 1) {
      emitError("'print' takes exactly 1 argument", std::nullopt,
                Span{0, 0, 0, 0}, std::nullopt);
      return;
    }
    compileExpr(*call.args[0]);
    emitOp(OpCode::Print);
    return;
  }

  if (builtin == "panic") {
    if (call.args.size() != 1) {
      emitError("'panic' takes exactly 1 argument", std::nullopt,
                Span{0, 0, 0, 0}, std::nullopt);
      return;
    }
    compileExpr(*call.args[0]);
    emitOp(OpCode::Panic);
    return;
  }

  compileExpr(*call.callee);
  for (const ExprPtr &arg : call.args)
    compileExpr(*arg);
  uint8_t argc = static_cast<uint8_t>(call.args.size());
  emitOp(tailPos ? OpCode::TailCall : OpCode::Call, argc);
}

void Compiler::compileBinary(const Binary &binary) {
  if (binary.op == BinaryOp::And) {
    compileExpr(*binary.lhs);
    size_t jumpPos = current().chunk->code.size();
    emitJump(OpCode::JumpFalse);
    emitOp(OpCode::Pop);
    compileExpr(*binary.rhs);
    patchJump(jumpPos);
    return;
  }

  if (binary.op == BinaryOp::Or) {
    compileExpr(*binary.lhs);
    size_t jumpFalsePos = current().chunk->code.size();
    emitJump(OpCode::JumpFalse);
    size_t jumpPos = current().chunk->code.size();
    emitJump(OpCode::Jump);
    patchJump(jumpFalsePos);
    emitOp(OpCode::Pop);
    compileExpr(*binary.rhs);
    patchJump(jumpPos);
    return;
  }

  compileExpr(*binary.lhs);
  compileExpr(*binary.rhs);

  switch (binary.op) {
  case BinaryOp::Addition:
    emitOp(OpCode::Add);
    break;
  case BinaryOp::Subtraction:
    emitOp(OpCode::Sub);
    break;
  case BinaryOp::Multiplication:
    emitOp(OpCode::Mul);
    break;
  case BinaryOp::Division:
    emitOp(OpCode::Div);
    break;
  case BinaryOp::Modulo:
    emitOp(OpCode::Mod);
    break;
  case BinaryOp::Power:
    emitOp(OpCode::Pow);
    break;
  case BinaryOp::Equal:
    emitOp(OpCode::Eq);
    break;
  case BinaryOp::NotEqual:
    emitOp(OpCode::Neq);
    break;
  case BinaryOp::LessThan:
    emitOp(OpCode::Lt);
    break;
  case BinaryOp::LessThanEqual:
    emitOp(OpCode::LtEq);
    break;
  case BinaryOp::GreaterThan:
    emitOp(OpCode::Gt);
    break;
  case BinaryOp::GreaterThanEqual:
    emitOp(OpCode::GtEq);
    break;
  case BinaryOp::And:
  case BinaryOp::Or:
    break;
  }
}

void Compiler::compileUnary(const Unary &unary) {
  compileExpr(*unary.rhs);
  switch (unary.op) {
  case UnaryOp::Negation:
    emitOp(OpCode::Neg);
    break;
  case UnaryOp::Not:
    emitOp(OpCode::Not);
    break;
  }
}

void Compiler::compileIdentifier(const Identifier &identifier) {
  assert(identifier.resolved.has_value() && "Identifier not resolved");

  if (m_builtins.count(identifier.resolved->bindingIndex)) {
    emitError("'" + identifier.name +
                  "' is a builtin and cannot be used as a value",
              std::string("call it directly: " + identifier.name + "(...)"),
              identifier.span, std::nullopt);
    emitOp(OpCode::PushUnit);
    return;
  }

  auto [kind, index] = lookupSlot(identifier.resolved->bindingIndex);
  if (kind == SlotKind::Local)
    emitOp(OpCode::Load, index);
  else
    emitOp(OpCode::LoadUpvalue, index);
}

void Compiler::compileFieldAccess(const FieldAccess &fa) {
  compileExpr(*fa.object);
  uint8_t nameIdx = addConstant(Value{fa.field});
  emitOp(OpCode::GetField, nameIdx);
}