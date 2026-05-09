#include <Iris/Backend/compiler.hpp>
#include <Iris/Common/utils.hpp>
#include <cassert>

namespace Iris::Backend {

Compiler::Compiler(std::string_view filename, std::string_view source,
                   Common::DiagnosticBag &bag)
    : m_filename(filename), m_source(source), m_bag(bag) {}

void Compiler::injectExternal(uint32_t bindingIndex, const std::string &name,
                              Runtime::Value value) {
  m_externals.emplace_back(bindingIndex, name, std::move(value));
}

void Compiler::registerBuiltin(uint32_t bindingIndex, const std::string &name) {
  m_builtins[bindingIndex] = name;
}

void Compiler::emitError(std::string msg, std::optional<std::string> hint,
                         Common::Span span, std::optional<Common::Error> code) {
  m_bag.emit(Common::Diagnostic{
      .severity = Common::Severity::Error,
      .code = code,
      .hint = std::move(hint),
      .message = std::move(msg),
      .filename = std::string(m_filename),
      .sourceLine = Common::getSourceLine(m_source, span.startLine),
      .span = span});
}

void Compiler::emit(uint8_t byte) { current().chunk->code.push_back(byte); }
void Compiler::emitOp(Runtime::OpCode op) { emit(static_cast<uint8_t>(op)); }
void Compiler::emitOp(Runtime::OpCode op, uint8_t operand) {
  emit(static_cast<uint8_t>(op));
  emit(operand);
}

void Compiler::emitJump(Runtime::OpCode op, int16_t offset) {
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

uint8_t Compiler::addConstant(Runtime::Value v) {
  auto &constants = current().chunk->constants;
  for (size_t i = 0; i < constants.size(); ++i) {
    if (valuesEqual(constants[i], v))
      return static_cast<uint8_t>(i);
  }
  if (constants.size() >= 255) {
    emitError("too many constants in one function (max 255)",
              std::string("split this function into smaller pieces"),
              Common::Span{0, 0, 0, 0}, Common::RuntimeError::TooManyConstants);
    return 0;
  }
  constants.push_back(std::move(v));
  return static_cast<uint8_t>(constants.size() - 1);
}

void Compiler::emitConstant(Runtime::Value v) {
  uint8_t idx = addConstant(std::move(v));
  emitOp(Runtime::OpCode::PushConst, idx);
}

uint8_t Compiler::allocSlot(uint32_t bindingIndex) {
  auto &fn = current();
  if (fn.nextSlot == 255) {
    emitError("too many local variables in one function (max 255)",
              std::string("split this function into smaller pieces"),
              Common::Span{0, 0, 0, 0}, Common::RuntimeError::TooManyLocals);
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

std::shared_ptr<Runtime::Chunk>
Compiler::compile(std::span<const Frontend::ExprPtr> program) {
  m_fnStack.push_back(FnState{
      .chunk = std::make_shared<Runtime::Chunk>(),
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
    emitOp(Runtime::OpCode::PushConst, idx);
    emitOp(Runtime::OpCode::Store, slot);
  }

  for (const Frontend::ExprPtr &expr : program) {
    if (std::holds_alternative<Frontend::Package>(expr->val))
      continue;
    if (std::holds_alternative<Frontend::Import>(expr->val))
      continue;

    compileExpr(*expr);
    if (expr.get() != program.back().get())
      emitOp(Runtime::OpCode::Pop);
  }

  emitOp(Runtime::OpCode::Return);

  auto chunk = current().chunk;
  chunk->slotCount = current().nextSlot;

  for (const Frontend::ExprPtr &expr : program) {
    if (const auto *let = std::get_if<Frontend::Let>(&expr->val)) {
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

void Compiler::compileExpr(const Frontend::Expr &expr, bool tailPos) {
  std::visit(
      [&](const auto &node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, Frontend::NumericLiteral>) {
          emitConstant(node.value);
        } else if constexpr (std::is_same_v<T, Frontend::StringLiteral>) {
          emitConstant(node.value);
        } else if constexpr (std::is_same_v<T, Frontend::BoolLiteral>) {
          emitOp(Runtime::OpCode::PushBool, node.value ? 1 : 0);
        } else if constexpr (std::is_same_v<T, Runtime::Unit>) {
          emitOp(Runtime::OpCode::PushUnit);
        } else if constexpr (std::is_same_v<T, Frontend::Identifier>) {
          compileIdentifier(node);
        } else if constexpr (std::is_same_v<T, Frontend::Let>) {
          compileLet(node);
        } else if constexpr (std::is_same_v<T, Frontend::Lambda>) {
          compileLambda(node, "<lambda>");
        } else if constexpr (std::is_same_v<T, Frontend::Block>) {
          compileBlock(node, tailPos);
        } else if constexpr (std::is_same_v<T, Frontend::If>) {
          compileIf(node, tailPos);
        } else if constexpr (std::is_same_v<T, Frontend::Call>) {
          compileCall(node, tailPos);
        } else if constexpr (std::is_same_v<T, Frontend::Binary>) {
          compileBinary(node);
        } else if constexpr (std::is_same_v<T, Frontend::Unary>) {
          compileUnary(node);
        } else if constexpr (std::is_same_v<T, Frontend::FieldAccess>) {
          compileFieldAccess(node);
        }
      },
      expr.val);
}

void Compiler::compileLet(const Frontend::Let &let) {
  assert(let.resolved.has_value() && "Let node not resolved");

  uint8_t slot = allocSlot(let.resolved->bindingIndex);

  if (const Frontend::Lambda *lam =
          std::get_if<Frontend::Lambda>(&let.value->val)) {
    emitOp(Runtime::OpCode::PushUnit);
    emitOp(Runtime::OpCode::Store, slot);
    compileLambda(*lam, let.name);
    emitOp(Runtime::OpCode::StoreClosure, slot);
  } else {
    compileExpr(*let.value);
    emitOp(Runtime::OpCode::Store, slot);
  }

  emitOp(Runtime::OpCode::PushUnit);
}

void Compiler::compileLambda(const Frontend::Lambda &lambda,
                             const std::string &name) {
  m_fnStack.push_back(FnState{
      .chunk = std::make_shared<Runtime::Chunk>(),
      .nextSlot = 0,
      .captureCount = 0,
      .inTailPos = false,
      .slotMap = {},
      .captureSlots = {},
  });
  current().chunk->name = name;

  assert(lambda.params.size() <= 255 && "too many parameters");
  for (const Frontend::Param &param : lambda.params) {
    uint8_t slot = current().nextSlot++;
    current().slotMap[param.bindingIndex] = slot;
  }

  compileBlock(lambda.body, true);
  emitOp(Runtime::OpCode::Return);

  auto fnChunk = current().chunk;
  uint8_t arity = static_cast<uint8_t>(lambda.params.size());
  auto captureSlots = current().captureSlots;
  fnChunk->slotCount = current().nextSlot;
  m_fnStack.pop_back();

  auto closure = std::make_shared<Runtime::Closure>();
  closure->chunk = fnChunk;
  closure->arity = arity;

  uint8_t idx = addConstant(Runtime::Value{closure});
  emitOp(Runtime::OpCode::MakeClosure, idx);
  emit(static_cast<uint8_t>(captureSlots.size()));
  for (uint8_t slot : captureSlots)
    emit(slot);
}

void Compiler::compileBlock(const Frontend::Block &block, bool tailPos) {
  for (size_t i = 0; i < block.exprs.size(); ++i) {
    bool last = (i == block.exprs.size() - 1);
    compileExpr(*block.exprs[i], tailPos && last);
    if (!last)
      emitOp(Runtime::OpCode::Pop);
  }
}

void Compiler::compileIf(const Frontend::If &ifExpr, bool tailPos) {
  compileExpr(*ifExpr.cond);

  size_t jumpFalsePos = current().chunk->code.size();
  emitJump(Runtime::OpCode::JumpFalse);

  compileBlock(ifExpr.then, tailPos);

  if (ifExpr.els) {
    size_t jumpPos = current().chunk->code.size();
    emitJump(Runtime::OpCode::Jump);
    patchJump(jumpFalsePos);
    compileBlock(*ifExpr.els, tailPos);
    patchJump(jumpPos);
  } else {
    patchJump(jumpFalsePos);
    emitOp(Runtime::OpCode::PushUnit);
  }
}

std::string Compiler::resolveBuiltinCall(const Frontend::Call &call) {
  const Frontend::Identifier *id =
      std::get_if<Frontend::Identifier>(&call.callee->val);
  if (!id || !id->resolved)
    return "";
  auto it = m_builtins.find(id->resolved->bindingIndex);
  if (it == m_builtins.end())
    return "";
  return it->second;
}

void Compiler::compileCall(const Frontend::Call &call, bool tailPos) {
  std::string builtin = resolveBuiltinCall(call);

  if (builtin == "print") {
    if (call.args.size() != 1) {
      emitError("'print' takes exactly 1 argument", std::nullopt,
                Common::Span{0, 0, 0, 0}, std::nullopt);
      return;
    }
    compileExpr(*call.args[0]);
    emitOp(Runtime::OpCode::Print);
    return;
  }

  if (builtin == "panic") {
    if (call.args.size() != 1) {
      emitError("'panic' takes exactly 1 argument", std::nullopt,
                Common::Span{0, 0, 0, 0}, std::nullopt);
      return;
    }
    compileExpr(*call.args[0]);
    emitOp(Runtime::OpCode::Panic);
    return;
  }

  compileExpr(*call.callee);
  for (const Frontend::ExprPtr &arg : call.args)
    compileExpr(*arg);
  uint8_t argc = static_cast<uint8_t>(call.args.size());
  emitOp(tailPos ? Runtime::OpCode::TailCall : Runtime::OpCode::Call, argc);
}

void Compiler::compileBinary(const Frontend::Binary &binary) {
  if (binary.op == Frontend::BinaryOp::And) {
    compileExpr(*binary.lhs);
    size_t jumpPos = current().chunk->code.size();
    emitJump(Runtime::OpCode::JumpFalse);
    emitOp(Runtime::OpCode::Pop);
    compileExpr(*binary.rhs);
    patchJump(jumpPos);
    return;
  }

  if (binary.op == Frontend::BinaryOp::Or) {
    compileExpr(*binary.lhs);
    size_t jumpFalsePos = current().chunk->code.size();
    emitJump(Runtime::OpCode::JumpFalse);
    size_t jumpPos = current().chunk->code.size();
    emitJump(Runtime::OpCode::Jump);
    patchJump(jumpFalsePos);
    emitOp(Runtime::OpCode::Pop);
    compileExpr(*binary.rhs);
    patchJump(jumpPos);
    return;
  }

  compileExpr(*binary.lhs);
  compileExpr(*binary.rhs);

  switch (binary.op) {
  case Frontend::BinaryOp::Addition:
    emitOp(Runtime::OpCode::Add);
    break;
  case Frontend::BinaryOp::Subtraction:
    emitOp(Runtime::OpCode::Sub);
    break;
  case Frontend::BinaryOp::Multiplication:
    emitOp(Runtime::OpCode::Mul);
    break;
  case Frontend::BinaryOp::Division:
    emitOp(Runtime::OpCode::Div);
    break;
  case Frontend::BinaryOp::Modulo:
    emitOp(Runtime::OpCode::Mod);
    break;
  case Frontend::BinaryOp::Power:
    emitOp(Runtime::OpCode::Pow);
    break;
  case Frontend::BinaryOp::Equal:
    emitOp(Runtime::OpCode::Eq);
    break;
  case Frontend::BinaryOp::NotEqual:
    emitOp(Runtime::OpCode::Neq);
    break;
  case Frontend::BinaryOp::LessThan:
    emitOp(Runtime::OpCode::Lt);
    break;
  case Frontend::BinaryOp::LessThanEqual:
    emitOp(Runtime::OpCode::LtEq);
    break;
  case Frontend::BinaryOp::GreaterThan:
    emitOp(Runtime::OpCode::Gt);
    break;
  case Frontend::BinaryOp::GreaterThanEqual:
    emitOp(Runtime::OpCode::GtEq);
    break;
  case Frontend::BinaryOp::And:
  case Frontend::BinaryOp::Or:
    break;
  }
}

void Compiler::compileUnary(const Frontend::Unary &unary) {
  compileExpr(*unary.rhs);
  switch (unary.op) {
  case Frontend::UnaryOp::Negation:
    emitOp(Runtime::OpCode::Neg);
    break;
  case Frontend::UnaryOp::Not:
    emitOp(Runtime::OpCode::Not);
    break;
  }
}

void Compiler::compileIdentifier(const Frontend::Identifier &identifier) {
  assert(identifier.resolved.has_value() && "Identifier not resolved");

  if (m_builtins.count(identifier.resolved->bindingIndex)) {
    emitError("'" + identifier.name +
                  "' is a builtin and cannot be used as a value",
              std::string("call it directly: " + identifier.name + "(...)"),
              identifier.span, std::nullopt);
    emitOp(Runtime::OpCode::PushUnit);
    return;
  }

  auto [kind, index] = lookupSlot(identifier.resolved->bindingIndex);
  if (kind == SlotKind::Local)
    emitOp(Runtime::OpCode::Load, index);
  else
    emitOp(Runtime::OpCode::LoadUpvalue, index);
}

void Compiler::compileFieldAccess(const Frontend::FieldAccess &fa) {
  compileExpr(*fa.object);
  uint8_t nameIdx = addConstant(Runtime::Value{fa.field});
  emitOp(Runtime::OpCode::GetField, nameIdx);
}
} // namespace Iris::Backend