#pragma once

#include <Iris/Common/error.hpp>
#include <Iris/Runtime/value.hpp>
#include <optional>
#include <vector>

namespace Iris::Runtime {

class VM {
public:
  explicit VM(std::string_view filename, std::string_view source,
              Common::DiagnosticBag &bag);

  std::optional<Value> run(std::shared_ptr<Chunk> chunk);

  const std::vector<std::pair<std::string, Value>> &exportedBindings() const {
    return m_exports;
  }

private:
  std::string_view m_filename;
  std::string_view m_source;
  Common::DiagnosticBag &m_bag;

  struct CallFrame {
    std::shared_ptr<Closure> closure;
    size_t ip{0};
    size_t base{0};
  };

  static constexpr size_t kMaxFrames = 1024;
  static constexpr size_t kMaxStack = 1024 * 64;

  std::vector<Value> m_stack;
  std::vector<CallFrame> m_frames;
  std::vector<std::pair<std::string, Value>> m_exports;

  CallFrame &frame() { return m_frames.back(); }
  Chunk &chunk() { return *frame().closure->chunk; }

  uint8_t readByte();
  int16_t readInt16();

  void push(Value v);
  Value pop();
  Value &peek(size_t offset = 0);

  void emitError(std::string msg, std::optional<std::string> hint,
                 std::optional<Common::Error> code = std::nullopt);

  bool call(std::shared_ptr<Closure> closure, uint8_t argc);
  std::optional<Value> execute();
};
} // namespace Iris::Runtime