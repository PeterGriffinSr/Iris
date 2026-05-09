#pragma once

#include <Iris/Common/error_codes.hpp>
#include <Iris/Common/span.hpp>
#include <Iris/options.hpp>
#include <optional>
#include <string>

namespace Iris::Common {

enum class Severity { Note, Warning, Error };

struct Diagnostic {
  Severity severity;
  std::optional<Error> code;
  std::optional<std::string> hint;
  std::string message, filename, sourceLine;
  Span span;
};

class DiagnosticBag {
public:
  explicit DiagnosticBag(const CompilerOptions &opts) : m_opts(opts) {}

  void emit(const Diagnostic &diag);
  void printSummary() const;

  [[nodiscard]] bool hasErrors() const noexcept;
  [[nodiscard]] bool limitReached() const noexcept;
  [[nodiscard]] uint32_t errorCount() const noexcept { return m_errors; }
  [[nodiscard]] uint32_t warningCount() const noexcept { return m_warnings; }

private:
  const CompilerOptions &m_opts;
  uint32_t m_errors{0}, m_warnings{0};
};

[[noreturn]] void emitFatal(const Diagnostic &diag);
void emitDirect(const Diagnostic &diag);
bool explainError(uint32_t code);
} // namespace Iris::Common