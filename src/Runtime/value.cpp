#include <Iris/Runtime/value.hpp>

namespace Iris::Runtime {

bool isTruthy(const Value &v) noexcept {
  if (const bool *b = std::get_if<bool>(&v))
    return *b;
  if (std::get_if<Unit>(&v))
    return false;
  return true;
}
} // namespace Iris::Runtime