#include <Iris/Frontend/scheme.hpp>
#include <Iris/Frontend/unifier.hpp>

#include <unordered_map>

namespace Iris::Frontend {

void collectFreeMetas(const TypePtr &ty, std::unordered_set<uint32_t> &out) {
  TypePtr r = resolveType(ty);

  std::visit(
      [&](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, MetaVar>) {
          out.insert(v.id);
        } else if constexpr (std::is_same_v<T, PiType>) {
          collectFreeMetas(v.domain, out);
          collectFreeMetas(v.codomain, out);
        } else if constexpr (std::is_same_v<T, BaseType> ||
                             std::is_same_v<T, TypeOfTypes> ||
                             std::is_same_v<T, TypeVar>) {
        }
      },
      r->val);
}

std::unordered_set<uint32_t> freeMetas(const TypePtr &ty) {
  std::unordered_set<uint32_t> result;
  collectFreeMetas(ty, result);
  return result;
}

static TypePtr
applyCapture(const TypePtr &ty,
             const std::unordered_map<uint32_t, uint32_t> &mapping) {
  TypePtr r = resolveType(ty);

  return std::visit(
      [&](const auto &v) -> TypePtr {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, MetaVar>) {
          if (auto it = mapping.find(v.id); it != mapping.end()) {
            return std::make_shared<Type>(TypeVar{it->second});
          }
          return r;
        } else if constexpr (std::is_same_v<T, PiType>) {
          return Type::pi(v.hint, applyCapture(v.domain, mapping),
                          applyCapture(v.codomain, mapping));
        } else {
          return r;
        }
      },
      r->val);
}

Scheme generalize(const TypePtr &ty,
                  const std::unordered_set<uint32_t> &envMetas) {
  auto tyMetas = freeMetas(ty);

  std::unordered_map<uint32_t, uint32_t> mapping;
  std::vector<uint32_t> boundVars;

  uint32_t nextVar = 0;
  for (uint32_t metaId : tyMetas) {
    if (!envMetas.contains(metaId)) {
      mapping[metaId] = nextVar;
      boundVars.push_back(nextVar);
      ++nextVar;
    }
  }

  if (boundVars.empty())
    return Scheme::mono(ty);

  TypePtr body = applyCapture(ty, mapping);
  return Scheme{.boundVars = std::move(boundVars), .body = std::move(body)};
}

static TypePtr
applyInstantiation(const TypePtr &ty,
                   const std::unordered_map<uint32_t, TypePtr> &mapping) {

  return std::visit(
      [&](const auto &v) -> TypePtr {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, TypeVar>) {
          if (auto it = mapping.find(v.bindingIndex); it != mapping.end())
            return it->second;
          return ty;

        } else if constexpr (std::is_same_v<T, PiType>) {
          return Type::pi(v.hint, applyInstantiation(v.domain, mapping),
                          applyInstantiation(v.codomain, mapping));

        } else {
          return ty;
        }
      },
      ty->val);
}

TypePtr instantiate(const Scheme &scheme, uint32_t &nextMeta) {
  if (scheme.boundVars.empty())
    return scheme.body;
  std::unordered_map<uint32_t, TypePtr> mapping;
  mapping.reserve(scheme.boundVars.size());

  for (uint32_t varId : scheme.boundVars)
    mapping[varId] = Type::meta(nextMeta++);

  return applyInstantiation(scheme.body, mapping);
}
} // namespace Iris::Frontend