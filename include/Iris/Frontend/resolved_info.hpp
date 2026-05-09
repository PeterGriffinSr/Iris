#pragma once

#include <cstdint>
#include <optional>

namespace Iris::Frontend {

struct ResolvedInfo {
  uint32_t scopeDepth;
  uint32_t bindingIndex;
  std::optional<uint8_t> slot;
};
} // namespace Iris::Frontend