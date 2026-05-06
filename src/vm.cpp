#include "src/include/vm.hpp"
#include "src/include/error.hpp"
#include "src/include/opcodes.hpp"
#include "src/include/printer.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

#define BINARY_NUM_OP(op, opcode_name, op_str)                                 \
  case OpCode::opcode_name: {                                                  \
    Value rhs = pop();                                                         \
    Value lhs = pop();                                                         \
    if (auto *a = std::get_if<double>(&lhs))                                   \
      if (auto *b = std::get_if<double>(&rhs)) {                               \
        push(*a op * b);                                                       \
        break;                                                                 \
      }                                                                        \
    emitError("'" op_str "' requires two numbers", std::nullopt,               \
              RuntimeError::TypeError);                                        \
    return std::nullopt;                                                       \
  }

VM::VM(std::string_view filename, std::string_view source, DiagnosticBag &bag)
    : m_filename(filename), m_source(source), m_bag(bag) {}

void VM::emitError(std::string msg, std::optional<std::string> hint,
                   std::optional<Error> code) {
  m_bag.emit(Diagnostic{.severity = Severity::Error,
                        .code = code,
                        .hint = std::move(hint),
                        .message = std::move(msg),
                        .filename = std::string(m_filename),
                        .sourceLine = {},
                        .span = {0, 0, 0, 0}});
}

uint8_t VM::readByte() { return chunk().code[frame().ip++]; }

int16_t VM::readInt16() {
  uint8_t lo = readByte();
  uint8_t hi = readByte();
  return static_cast<int16_t>(lo | (hi << 8));
}

void VM::push(Value v) {
  if (m_stack.size() >= kMaxStack) {
    emitError("stack overflow", std::nullopt, RuntimeError::StackOverflow);
    return;
  }
  m_stack.push_back(std::move(v));
}

Value VM::pop() {
  assert(!m_stack.empty());
  Value v = std::move(m_stack.back());
  m_stack.pop_back();
  return v;
}

Value &VM::peek(size_t offset) { return m_stack[m_stack.size() - 1 - offset]; }

bool VM::call(std::shared_ptr<Closure> closure, uint8_t argc) {
  if (argc != closure->arity) {
    emitError("arity mismatch: expected " + std::to_string(closure->arity) +
                  " argument(s), got " + std::to_string(argc),
              std::nullopt, RuntimeError::ArityMismatch);
    return false;
  }

  if (m_frames.size() >= kMaxFrames) {
    emitError("stack overflow", std::nullopt, RuntimeError::StackOverflow);
    return false;
  }

  size_t base = m_stack.size() - argc;
  size_t needed = base + closure->chunk->slotCount;

  m_frames.push_back(CallFrame{
      .closure = std::move(closure),
      .ip = 0,
      .base = base,
  });

  if (needed > m_stack.size())
    m_stack.resize(needed, Value{Unit{}});

  return true;
}

std::optional<Value> VM::run(std::shared_ptr<Chunk> chunk) {
  auto scriptClosure = std::make_shared<Closure>();
  scriptClosure->chunk = chunk;
  scriptClosure->arity = 0;

  m_stack.resize(chunk->slotCount, Value{Unit{}});

  m_frames.push_back(CallFrame{
      .closure = scriptClosure,
      .ip = 0,
      .base = 0,
  });

  auto result = execute();

  for (auto &[name, slot] : chunk->exportedSlots) {
    if (slot < m_stack.size())
      m_exports.push_back({name, m_stack[slot]});
  }

  return result;
}

std::optional<Value> VM::execute() {
  while (true) {
    if (m_bag.hasErrors())
      return std::nullopt;

    auto op = static_cast<OpCode>(readByte());

    switch (op) {
    case OpCode::PushConst: {
      uint8_t idx = readByte();
      push(chunk().constants[idx]);
      break;
    }
    case OpCode::PushBool: {
      uint8_t val = readByte();
      push(Value{val != 0});
      break;
    }
    case OpCode::PushUnit: {
      push(Value{Unit{}});
      break;
    }
    case OpCode::Load: {
      uint8_t slot = readByte();
      size_t idx = frame().base + slot;
      if (idx >= m_stack.size())
        m_stack.resize(idx + 1, Value{Unit{}});
      push(m_stack[idx]);
      break;
    }
    case OpCode::Store: {
      uint8_t slot = readByte();
      size_t idx = frame().base + slot;
      Value v = pop();
      if (idx >= m_stack.size())
        m_stack.resize(idx + 1);
      m_stack[idx] = std::move(v);
      break;
    }
    case OpCode::StoreClosure: {
      uint8_t slot = readByte();
      Value v = pop();

      m_stack[frame().base + slot] = v;

      if (auto *cl = std::get_if<std::shared_ptr<Closure>>(&v)) {
        (*cl)->weakCaptures.resize((*cl)->captures.size());
        for (size_t i = 0; i < (*cl)->captures.size(); ++i) {
          if (std::holds_alternative<Unit>((*cl)->captures[i])) {
            (*cl)->weakCaptures[i] = *cl;
            (*cl)->captures[i] = Value{Unit{}};
          }
        }
      }
      break;
    }
    case OpCode::LoadUpvalue: {
      uint8_t idx = readByte();
      auto &cl = frame().closure;
      if (idx < cl->weakCaptures.size() && !cl->weakCaptures[idx].expired()) {
        push(Value{cl->weakCaptures[idx].lock()});
      } else {
        push(cl->captures[idx]);
      }
      break;
    }
    case OpCode::Pop: {
      pop();
      break;
    }
    case OpCode::Jump: {
      int16_t offset = readInt16();
      frame().ip += offset;
      break;
    }
    case OpCode::JumpFalse: {
      int16_t offset = readInt16();
      Value cond = pop();
      if (!isTruthy(cond))
        frame().ip += offset;
      break;
    }
    case OpCode::MakeClosure: {
      uint8_t idx = readByte();
      uint8_t upvalCount = readByte();

      auto proto = std::get<std::shared_ptr<Closure>>(chunk().constants[idx]);
      auto closure = std::make_shared<Closure>();
      closure->chunk = proto->chunk;
      closure->arity = proto->arity;
      closure->captures.reserve(upvalCount);
      closure->weakCaptures.resize(upvalCount);

      for (uint8_t i = 0; i < upvalCount; ++i) {
        uint8_t slot = readByte();
        closure->captures.push_back(m_stack[frame().base + slot]);
      }

      push(Value{closure});
      break;
    }
    case OpCode::Call: {
      uint8_t argc = readByte();
      Value &callee = peek(argc);

      auto *cl = std::get_if<std::shared_ptr<Closure>>(&callee);
      if (!cl) {
        emitError("value is not callable", std::nullopt,
                  RuntimeError::NotCallable);
        return std::nullopt;
      }

      if (!call(*cl, argc))
        return std::nullopt;
      break;
    }
    case OpCode::TailCall: {
      uint8_t argc = readByte();
      Value &callee = peek(argc);

      auto *cl = std::get_if<std::shared_ptr<Closure>>(&callee);
      if (!cl) {
        emitError("value is not callable", std::nullopt,
                  RuntimeError::NotCallable);
        return std::nullopt;
      }

      auto closure = *cl;
      if (argc != closure->arity) {
        emitError("arity mismatch: expected " + std::to_string(closure->arity) +
                      " argument(s), got " + std::to_string(argc),
                  std::nullopt, RuntimeError::ArityMismatch);
        return std::nullopt;
      }

      std::vector<Value> args;
      args.reserve(argc);
      for (uint8_t i = 0; i < argc; ++i)
        args.push_back(std::move(m_stack[m_stack.size() - argc + i]));

      m_stack.resize(m_stack.size() - argc - 1);

      frame().closure = std::move(closure);
      frame().ip = 0;

      size_t needed = frame().base + frame().closure->chunk->slotCount;
      m_stack.resize(needed, Value{Unit{}});

      for (uint8_t i = 0; i < argc; ++i)
        m_stack[frame().base + i] = std::move(args[i]);

      break;
    }
    case OpCode::Return: {
      Value result = pop();

      if (m_frames.size() == 1) {
        m_frames.pop_back();
        return result;
      }

      size_t base = frame().base;
      m_frames.pop_back();
      m_stack.resize(base > 0 ? base - 1 : 0);
      push(std::move(result));
      break;
    }
      BINARY_NUM_OP(+, Add, "+")
      BINARY_NUM_OP(-, Sub, "-")
      BINARY_NUM_OP(*, Mul, "*")
    case OpCode::Div: {
      Value rhs = pop();
      Value lhs = pop();
      if (auto *a = std::get_if<double>(&lhs))
        if (auto *b = std::get_if<double>(&rhs)) {
          if (*b == 0.0) {
            emitError("division by zero", std::nullopt,
                      RuntimeError::DivisionByZero);
            return std::nullopt;
          }
          push(*a / *b);
          break;
        }
      emitError("'/' requires two numbers", std::nullopt,
                RuntimeError::TypeError);
      return std::nullopt;
    }
    case OpCode::Mod: {
      Value rhs = pop();
      Value lhs = pop();
      if (auto *a = std::get_if<double>(&lhs))
        if (auto *b = std::get_if<double>(&rhs)) {
          if (*b == 0.0) {
            emitError("modulo by zero", std::nullopt,
                      RuntimeError::DivisionByZero);
            return std::nullopt;
          }
          push(std::fmod(*a, *b));
          break;
        }
      emitError("'%' requires two numbers", std::nullopt,
                RuntimeError::TypeError);
      return std::nullopt;
    }
    case OpCode::Pow: {
      Value rhs = pop();
      Value lhs = pop();
      if (auto *a = std::get_if<double>(&lhs))
        if (auto *b = std::get_if<double>(&rhs)) {
          push(std::pow(*a, *b));
          break;
        }
      emitError("'^' requires two numbers", std::nullopt,
                RuntimeError::TypeError);
      return std::nullopt;
    }
    case OpCode::Neg: {
      Value v = pop();
      if (auto *a = std::get_if<double>(&v)) {
        push(-*a);
        break;
      }
      emitError("'-' requires a number", std::nullopt, RuntimeError::TypeError);
      return std::nullopt;
    }
    case OpCode::Not: {
      Value v = pop();
      push(!isTruthy(v));
      break;
    }
    case OpCode::Eq: {
      Value rhs = pop();
      Value lhs = pop();
      push(valuesEqual(lhs, rhs));
      break;
    }
    case OpCode::Neq: {
      Value rhs = pop();
      Value lhs = pop();
      push(!valuesEqual(lhs, rhs));
      break;
    }
    case OpCode::GetField: {
      uint8_t nameIdx = readByte();
      std::string &fieldName =
          std::get<std::string>(chunk().constants[nameIdx]);
      Value obj = pop();

      if (auto *ns = std::get_if<std::shared_ptr<Namespace>>(&obj)) {
        auto it = (*ns)->fields->find(fieldName);
        if (it == (*ns)->fields->end()) {
          emitError("namespace has no field '" + fieldName + "'", std::nullopt,
                    RuntimeError::TypeError);
          return std::nullopt;
        }
        push(it->second);
        break;
      }

      emitError("value does not support field access", std::nullopt,
                RuntimeError::TypeError);
      return std::nullopt;
    }
    case OpCode::Print: {
      Value v = pop();
      std::cout << valueToString(v) << '\n';
      push(Value{Unit{}});
      break;
    }
    case OpCode::Panic: {
      Value v = pop();
      emitError(valueToString(v), std::nullopt, RuntimeError::Panic);
      return std::nullopt;
    }
      BINARY_NUM_OP(<, Lt, "<")
      BINARY_NUM_OP(<=, LtEq, "<=")
      BINARY_NUM_OP(>, Gt, ">")
      BINARY_NUM_OP(>=, GtEq, ">=")
    }
  }
}