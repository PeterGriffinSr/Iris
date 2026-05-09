#include <Iris/Frontend/type.hpp>

namespace Iris::Frontend {

TypePtr Type::meta(uint32_t id) {
  return std::make_shared<Type>(MetaVar{id, nullptr});
}

TypePtr Type::num() { return std::make_shared<Type>(BaseType{BaseKind::Num}); }

TypePtr Type::bool_() {
  return std::make_shared<Type>(BaseType{BaseKind::Bool});
}

TypePtr Type::str() {
  return std::make_shared<Type>(BaseType{BaseKind::String});
}

TypePtr Type::unit() {
  return std::make_shared<Type>(BaseType{BaseKind::Unit});
}

TypePtr Type::pi(std::string hint, TypePtr dom, TypePtr cod) {
  return std::make_shared<Type>(
      PiType{std::move(hint), std::move(dom), std::move(cod)});
}

TypePtr Type::typeOfTypes() { return std::make_shared<Type>(TypeOfTypes{}); }
} // namespace Iris::Frontend