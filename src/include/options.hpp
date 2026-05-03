#pragma once

#include <cstdint>

struct CompilerOptions {
  uint32_t maxErrors = 20;
  bool werror = false;
};