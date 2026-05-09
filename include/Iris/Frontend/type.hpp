#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace Iris::Frontend {

struct Type;
using TypePtr = std::shared_ptr<Type>;

struct MetaVar {
  uint32_t id;
  mutable TypePtr solution;
};

struct TypeVar {
  uint32_t bindingIndex;
};

struct TypeOfTypes {};

struct PiType {
  std::string hint;
  TypePtr domain;
  TypePtr codomain;
};

enum class BaseKind : uint8_t { Num, Bool, String, Unit };

struct BaseType {
  BaseKind kind;
};

using TypeVariant =
    std::variant<MetaVar, TypeVar, TypeOfTypes, PiType, BaseType>;

struct Type {
  TypeVariant val;

  template <typename T> explicit Type(T &&v) : val(std::forward<T>(v)) {}

  [[nodiscard]] static TypePtr meta(uint32_t id);
  [[nodiscard]] static TypePtr num();
  [[nodiscard]] static TypePtr bool_();
  [[nodiscard]] static TypePtr str();
  [[nodiscard]] static TypePtr unit();
  [[nodiscard]] static TypePtr pi(std::string hint, TypePtr dom, TypePtr cod);
  [[nodiscard]] static TypePtr typeOfTypes();
};
} // namespace Iris::Frontend