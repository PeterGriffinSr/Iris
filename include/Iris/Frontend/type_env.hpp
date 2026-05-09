#pragma once

#include <Iris/Frontend/scheme.hpp>
#include <Iris/Frontend/type.hpp>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Iris::Frontend {

class TypeEnv {
public:
  void bind(uint32_t index, Scheme scheme);
  void bindMono(uint32_t index, TypePtr ty);
  [[nodiscard]] const Scheme *lookup(uint32_t index) const;
  [[nodiscard]] std::unordered_set<uint32_t> allFreeMetas() const;

  void pushScope();
  void popScope();

private:
  std::unordered_map<uint32_t, Scheme> m_schemes;
  std::vector<uint32_t> m_activeKeys;
  std::vector<std::size_t> m_frames;
};
} // namespace Iris::Frontend