#pragma once

#include "src/include/error_codes.hpp"
#include "src/include/options.hpp"
#include "src/include/span.hpp"
#include <optional>
#include <string>

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

  bool hasErrors() const noexcept;
  bool limitReached() const noexcept;

  uint32_t errorCount() const noexcept { return m_errors; };
  uint32_t warningCount() const noexcept { return m_warnings; };

private:
  const CompilerOptions &m_opts;
  uint32_t m_errors = 0, m_warnings = 0;
};

[[noreturn]] void emitFatal(const Diagnostic &diag);
void emitDirect(const Diagnostic &diag);

bool explainError(uint32_t code);