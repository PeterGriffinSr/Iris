#pragma once

#include <Iris/Frontend/type.hpp>

namespace Iris::Frontend {

[[nodiscard]] TypePtr resolveType(TypePtr ty);

[[nodiscard]] bool occursIn(uint32_t id, TypePtr ty);

void unify(TypePtr a, TypePtr b);
} // namespace Iris::Frontend