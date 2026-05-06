#pragma once

#include "src/include/ast.hpp"
#include "src/include/error_codes.hpp"
#include "src/include/opcodes.hpp"
#include "src/include/value.hpp"
#include <span>
#include <unordered_map>

class DiagnosticBag;

enum class SlotKind { Local, Upvalue };
struct SlotRef {
  SlotKind kind;
  uint8_t index;
};

class Compiler {
public:
  explicit Compiler(std::string_view filename, std::string_view source,
                    DiagnosticBag &bag);

  void injectExternal(uint32_t bindingIndex, const std::string &name,
                      Value value);

  void registerBuiltin(uint32_t bindingIndex, const std::string &name);

  std::shared_ptr<Chunk> compile(std::span<const ExprPtr> program);

private:
  std::unordered_map<uint32_t, std::string> m_builtins;
  std::string_view m_filename;
  std::string_view m_source;
  DiagnosticBag &m_bag;

  struct FnState {
    std::shared_ptr<Chunk> chunk;
    uint8_t nextSlot{0};
    uint8_t captureCount{0};
    bool inTailPos{false};
    std::unordered_map<uint32_t, uint8_t> slotMap;
    std::vector<uint8_t> captureSlots;
  };

  std::vector<FnState> m_fnStack;

  std::vector<std::tuple<uint32_t, std::string, Value>> m_externals;

  FnState &current() { return m_fnStack.back(); }

  void emit(uint8_t byte);
  void emitOp(OpCode op);
  void emitOp(OpCode op, uint8_t operand);
  void emitJump(OpCode op, int16_t offset = 0);
  void patchJump(size_t jumpPos);
  uint8_t addConstant(Value v);
  void emitConstant(Value v);

  void emitError(std::string msg, std::optional<std::string> hint, Span span,
                 std::optional<Error> code = std::nullopt);

  uint8_t allocSlot(uint32_t bindingIndex);
  SlotRef lookupSlot(uint32_t bindingIndex);
  uint8_t addCapture(uint8_t outerSlot);

  std::string resolveBuiltinCall(const Call &call);

  void compileExpr(const Expr &expr, bool tailPos = false);
  void compileLet(const Let &let);
  void compileLambda(const Lambda &lambda, const std::string &name);
  void compileBlock(const Block &block, bool tailPos = false);
  void compileIf(const If &ifExpr, bool tailPos = false);
  void compileCall(const Call &call, bool tailPos = false);
  void compileBinary(const Binary &binary);
  void compileUnary(const Unary &unary);
  void compileIdentifier(const Identifier &identifier);
  void compileFieldAccess(const FieldAccess &fa);
};