#include <Iris/Frontend/type_env.hpp>

namespace Iris::Frontend {

void TypeEnv::bind(uint32_t index, Scheme scheme) {
  m_schemes[index] = std::move(scheme);
  m_activeKeys.push_back(index);
}

void TypeEnv::bindMono(uint32_t index, TypePtr ty) {
  bind(index, Scheme::mono(std::move(ty)));
}

const Scheme *TypeEnv::lookup(uint32_t index) const {
  if (auto it = m_schemes.find(index); it != m_schemes.end())
    return &it->second;
  return nullptr;
}

std::unordered_set<uint32_t> TypeEnv::allFreeMetas() const {
  std::unordered_set<uint32_t> result;
  for (const auto &[_, scheme] : m_schemes) {
    collectFreeMetas(scheme.body, result);
  }
  return result;
}

void TypeEnv::pushScope() { m_frames.push_back(m_activeKeys.size()); }

void TypeEnv::popScope() {
  if (m_frames.empty())
    return;
  size_t targetSize = m_frames.back();
  m_frames.pop_back();
  while (m_activeKeys.size() > targetSize) {
    uint32_t key = m_activeKeys.back();
    m_schemes.erase(key);
    m_activeKeys.pop_back();
  }
}

} // namespace Iris::Frontend