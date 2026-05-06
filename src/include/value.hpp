#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct Namespace;
struct Closure;

struct Unit {
  bool operator==(const Unit &) const noexcept { return true; }
};

using Value =
    std::variant<double, bool, std::string, Unit, std::shared_ptr<Closure>,
                 std::shared_ptr<Namespace>>;

struct Namespace {
  std::shared_ptr<std::unordered_map<std::string, Value>> fields;
};

struct Chunk {
  std::string name;
  std::vector<uint8_t> code;
  std::vector<Value> constants;
  std::vector<uint32_t> lines;
  uint8_t slotCount{0};
  std::unordered_map<std::string, uint8_t> exportedSlots;
};

struct Closure {
  std::shared_ptr<Chunk> chunk;
  std::vector<Value> captures;
  std::vector<std::weak_ptr<Closure>> weakCaptures;
  uint8_t arity;
};

bool isTruthy(const Value &v) noexcept;

inline bool valuesEqual(const Value &a, const Value &b) {
  return std::visit(
      [](const auto &x, const auto &y) -> bool {
        using X = std::decay_t<decltype(x)>;
        using Y = std::decay_t<decltype(y)>;
        if constexpr (std::is_same_v<X, Y>) {
          if constexpr (std::is_same_v<X, std::shared_ptr<Closure>>)
            return false;
          else if constexpr (std::is_same_v<X, std::shared_ptr<Namespace>>)
            return false;
          else
            return x == y;
        }
        return false;
      },
      a, b);
}