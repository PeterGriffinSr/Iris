#pragma once

#include <Iris/Common/error.hpp>
#include <Iris/Common/span.hpp>
#include <Iris/Frontend/ast.hpp>
#include <Iris/Frontend/scheme.hpp>
#include <Iris/Frontend/type.hpp>
#include <Iris/Frontend/type_env.hpp>
#include <span>
#include <string_view>

namespace Iris::Frontend {

class TypeChecker {
public:
  explicit TypeChecker(std::string_view filename, std::string_view source,
                       Common::DiagnosticBag &bag);

  void check(std::span<ExprPtr> program);
  TypePtr infer(Expr &expr);

private:
  std::string_view m_filename;
  std::string_view m_source;
  Common::DiagnosticBag &m_bag;
  TypeEnv m_env;
  uint32_t m_nextMeta{0};

  [[nodiscard]] TypePtr freshMeta();
  [[nodiscard]] TypePtr inferNode(NumericLiteral &node);
  [[nodiscard]] TypePtr inferNode(StringLiteral &node);
  [[nodiscard]] TypePtr inferNode(BoolLiteral &node);
  [[nodiscard]] TypePtr inferNode(Package &node);
  [[nodiscard]] TypePtr inferNode(Import &node);
  [[nodiscard]] TypePtr inferNode(Identifier &node);
  [[nodiscard]] TypePtr inferNode(Let &node);
  [[nodiscard]] TypePtr inferNode(Lambda &node);
  [[nodiscard]] TypePtr inferNode(Block &node);
  [[nodiscard]] TypePtr inferNode(Call &node);
  [[nodiscard]] TypePtr inferNode(Binary &node);
  [[nodiscard]] TypePtr inferNode(Unary &node);
  [[nodiscard]] TypePtr inferNode(If &node);
  [[nodiscard]] TypePtr inferNode(FieldAccess &node);

  [[nodiscard]] TypePtr inferBlock(Block &block);

  void emitError(std::string msg, std::optional<std::string> hint,
                 Common::Span span, Common::TypeError code);
};

} // namespace Iris::Frontend