#include <Iris/Runtime/value.hpp>
#include <cmath>
#include <sstream>

namespace Iris::Runtime {

bool isTruthy(const Value &v) noexcept {
  if (const bool *b = std::get_if<bool>(&v))
    return *b;
  if (std::get_if<Unit>(&v))
    return false;
  return true;
}

std::string toString(const Value &v) noexcept {
  return std::visit(
      [](const auto &val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, double>) {
          if (val == std::floor(val) && std::isfinite(val)) {
            std::ostringstream ss;
            ss << static_cast<long long>(val);
            return ss.str();
          }
          std::ostringstream ss;
          ss << val;
          return ss.str();
        } else if constexpr (std::is_same_v<T, bool>) {
          return val ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
          return val;
        } else if constexpr (std::is_same_v<T, Unit>) {
          return "unit";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Closure>>) {
          return "<closure/" + std::to_string(val->arity) + ">";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Namespace>>) {
          std::string s = "<namespace {";
          bool first = true;
          for (const auto &[name, _] : *val->fields) {
            if (!first)
              s += ", ";
            s += name;
            first = false;
          }
          s += "}>";
          return s;
        }
      },
      v);
}

} // namespace Iris::Runtime