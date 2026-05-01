#pragma once

#include "error_codes.hpp"
#include <cstdint>
#include <optional>
#include <string>

enum class Severity { Note, Warning, Error };

struct Diagnostic {
  Severity severity;
  std::optional<Error> code;
  std::optional<std::string> hint;
  std::string message, filename, sourceLine;
  uint32_t line, col;
};

[[noreturn]] void emitFatal(const Diagnostic &diag);
void emit(const Diagnostic &diag);

bool explainError(uint32_t code);