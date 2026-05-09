#include <Iris/Frontend/unifier.hpp>

#include <format>
#include <stdexcept>

namespace Iris::Frontend {

TypePtr resolveType(TypePtr ty) {
  while (true) {
    if (const auto *m = std::get_if<MetaVar>(&ty->val)) {
      if (m->solution) {
        ty = m->solution;
        continue;
      }
    }
    return ty;
  }
}

bool occursIn(uint32_t id, TypePtr ty) {
  ty = resolveType(std::move(ty));

  if (const auto *m = std::get_if<MetaVar>(&ty->val))
    return m->id == id;

  if (const auto *p = std::get_if<PiType>(&ty->val))
    return occursIn(id, p->domain) || occursIn(id, p->codomain);

  return false;
}

void unify(TypePtr a, TypePtr b) {
  a = resolveType(a);
  b = resolveType(b);

  if (auto *ma = std::get_if<MetaVar>(&a->val)) {
    if (auto *mb = std::get_if<MetaVar>(&b->val)) {
      if (ma->id == mb->id)
        return;
    }

    if (occursIn(ma->id, b)) {
      throw std::runtime_error("Infinite type detected (occurs check)");
    }
    ma->solution = b;
    return;
  }

  if (auto *mb = std::get_if<MetaVar>(&b->val)) {
    if (occursIn(mb->id, a))
      throw std::runtime_error(std::format(
          "occurs check failed: ?{} appears in its own solution", mb->id));

    mb->solution = a;
    return;
  }

  if (const auto *ba = std::get_if<BaseType>(&a->val)) {
    if (const auto *bb = std::get_if<BaseType>(&b->val)) {
      if (ba->kind == bb->kind)
        return;
      throw std::runtime_error("base type mismatch");
    }
    throw std::runtime_error("cannot unify base type with non-base type");
  }

  if (auto *pa = std::get_if<PiType>(&a->val)) {
    if (auto *pb = std::get_if<PiType>(&b->val)) {
      unify(pa->domain, pb->domain);
      unify(pa->codomain, pb->codomain);
      return;
    }
    throw std::runtime_error("cannot unify Pi type with non-Pi type");
  }

  if (std::holds_alternative<TypeOfTypes>(a->val) &&
      std::holds_alternative<TypeOfTypes>(b->val))
    return;

  throw std::runtime_error("cannot unify types");
}
} // namespace Iris::Frontend