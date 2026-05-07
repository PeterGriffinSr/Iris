#pragma once

#include <Iris/Common/error.hpp>
#include <Iris/Frontend/ast.hpp>
#include <Iris/Runtime/opcodes.hpp>
#include <Iris/Runtime/value.hpp>
#include <span>
#include <unordered_map>

namespace Iris::Backend {

enum class SlotKind { Local, Upvalue };
struct SlotRef {
  SlotKind kind;
  uint8_t index;
};

class Compiler {
public:
  explicit Compiler(std::string_view filename, std::string_view source,
                    Common::DiagnosticBag &bag);

  void injectExternal(uint32_t bindingIndex, const std::string &name,
                      Runtime::Value value);

  void registerBuiltin(uint32_t bindingIndex, const std::string &name);

  std::shared_ptr<Runtime::Chunk>
  compile(std::span<const Frontend::ExprPtr> program);

private:
  std::unordered_map<uint32_t, std::string> m_builtins;
  std::string_view m_filename;
  std::string_view m_source;
  Common::DiagnosticBag &m_bag;

  struct FnState {
    std::shared_ptr<Runtime::Chunk> chunk;
    uint8_t nextSlot{0};
    uint8_t captureCount{0};
    bool inTailPos{false};
    std::unordered_map<uint32_t, uint8_t> slotMap;
    std::vector<uint8_t> captureSlots;
  };

  std::vector<FnState> m_fnStack;

  std::vector<std::tuple<uint32_t, std::string, Runtime::Value>> m_externals;

  FnState &current() { return m_fnStack.back(); }

  void emit(uint8_t byte);
  void emitOp(Runtime::OpCode op);
  void emitOp(Runtime::OpCode op, uint8_t operand);
  void emitJump(Runtime::OpCode op, int16_t offset = 0);
  void patchJump(size_t jumpPos);
  uint8_t addConstant(Runtime::Value v);
  void emitConstant(Runtime::Value v);

  void emitError(std::string msg, std::optional<std::string> hint,
                 Common::Span span,
                 std::optional<Common::Error> code = std::nullopt);

  uint8_t allocSlot(uint32_t bindingIndex);
  SlotRef lookupSlot(uint32_t bindingIndex);
  uint8_t addCapture(uint8_t outerSlot);

  std::string resolveBuiltinCall(const Frontend::Call &call);

  void compileExpr(const Frontend::Expr &expr, bool tailPos = false);
  void compileLet(const Frontend::Let &let);
  void compileLambda(const Frontend::Lambda &lambda, const std::string &name);
  void compileBlock(const Frontend::Block &block, bool tailPos = false);
  void compileIf(const Frontend::If &ifExpr, bool tailPos = false);
  void compileCall(const Frontend::Call &call, bool tailPos = false);
  void compileBinary(const Frontend::Binary &binary);
  void compileUnary(const Frontend::Unary &unary);
  void compileIdentifier(const Frontend::Identifier &identifier);
  void compileFieldAccess(const Frontend::FieldAccess &fa);
};
} // namespace Iris::Backend