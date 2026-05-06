#pragma once

#include "src/include/ast.hpp"
#include "src/include/error_codes.hpp"
#include <span>
#include <unordered_map>

class DiagnosticBag;

class Resolver {
public:
  explicit Resolver(std::string_view filename, std::string_view source,
                    DiagnosticBag &bag);

  uint32_t declareExternal(const std::string &name);

  void resolve(std::span<ExprPtr> program);

private:
  std::string_view m_filename;
  std::string_view m_source;
  DiagnosticBag &m_bag;

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

  void emitError(std::string msg, std::optional<std::string> hint, Span span,
                 std::optional<Error> code = std::nullopt);

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