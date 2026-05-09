#pragma once

#include <Iris/Frontend/type.hpp>
#include <unordered_set>
#include <vector>

namespace Iris::Frontend {

struct Scheme {
  std::vector<uint32_t> boundVars;
  TypePtr body;
  static Scheme mono(TypePtr ty) {
    return Scheme{.boundVars = {}, .body = std::move(ty)};
  }
};

void collectFreeMetas(const TypePtr &ty, std::unordered_set<uint32_t> &out);

[[nodiscard]] std::unordered_set<uint32_t> freeMetas(const TypePtr &ty);
[[nodiscard]] Scheme generalize(const TypePtr &ty,
                                const std::unordered_set<uint32_t> &envMetas);

[[nodiscard]] TypePtr instantiate(const Scheme &scheme, uint32_t &nextMeta);
} // namespace Iris::Frontend