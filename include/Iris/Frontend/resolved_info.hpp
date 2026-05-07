#pragma once

#include <cstdint>
#include <optional>

struct ResolvedInfo {
  uint32_t scopeDepth;
  uint32_t bindingIndex;
  std::optional<uint8_t> slot;
  // std::optional<TypeId> type;
};
