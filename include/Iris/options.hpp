#pragma once

#include <cstdint>

namespace Iris {

struct CompilerOptions {
  uint32_t maxErrors = 20;
  bool werror = false;
};
} // namespace Iris