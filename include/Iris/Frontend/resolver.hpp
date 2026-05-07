#pragma once

#include <Iris/Common/error.hpp>
#include <Iris/Frontend/ast.hpp>
#include <span>
#include <unordered_map>

namespace Iris::Frontend {

class Resolver {
public:
  explicit Resolver(std::string_view filename, std::string_view source,
                    Common::DiagnosticBag &bag);

  uint32_t declareExternal(const std::string &name);

  void resolve(std::span<ExprPtr> program);

private:
  std::string_view m_filename;
  std::string_view m_source;
  Common::DiagnosticBag &m_bag;

  struct Binding {
    uint32_t scopeDepth;
    uint32_t bindingIndex;
  };

  std::vector<std::unordered_map<std::string, Binding>> m_scopes;
  uint32_t m_nextIndex{0};

  void pushScope();
  void popScope();

  ResolvedInfo declare(const std::string &name);
  std::optional<ResolvedInfo> lookup(const std::string &name) const;

  void emitError(std::string msg, std::optional<std::string> hint,
                 Common::Span span,
                 std::optional<Common::Error> code = std::nullopt);

  void collectTopLevel(std::span<ExprPtr> program);

  void resolveExpr(Expr &expr);
  void resolveLet(Let &let);
  void resolveLambda(Lambda &lambda);
  void resolveBlock(Block &block);
  void resolveIf(If &ifExpr);
  void resolveCall(Call &call);
  void resolveBinary(Binary &binary);
  void resolveUnary(Unary &unary);
  void resolveIdentifier(Identifier &identifier);
  void resolveFieldAccess(FieldAccess &fa);
};
} // namespace Iris::Frontend