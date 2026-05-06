#include "src/include/value.hpp"

bool isTruthy(const Value &v) noexcept {
  if (const bool *b = std::get_if<bool>(&v))
    return *b;
  if (std::get_if<Unit>(&v))
    return false;
  return true;
}