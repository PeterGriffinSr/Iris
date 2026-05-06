#include "src/include/resolver.hpp"
#include "src/include/ast.hpp"
#include "src/include/error.hpp"
#include "src/include/utils.hpp"

Resolver::Resolver(std::string_view filename, std::string_view source,
                   DiagnosticBag &bag)
    : m_filename(filename), m_source(source), m_bag(bag) {
  m_scopes.emplace_back();
}

void Resolver::pushScope() { m_scopes.emplace_back(); }
void Resolver::popScope() { m_scopes.pop_back(); }

uint32_t Resolver::declareExternal(const std::string &name) {
  ResolvedInfo info = declare(name);
  return info.bindingIndex;
}

ResolvedInfo Resolver::declare(const std::string &name) {
  uint32_t depth = static_cast<uint32_t>(m_scopes.size() - 1);
  uint32_t index = m_nextIndex++;
  m_scopes.back()[name] = Binding{depth, index};
  return ResolvedInfo{depth, index, std::nullopt};
}

std::optional<ResolvedInfo> Resolver::lookup(const std::string &name) const {
  for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end())
      return ResolvedInfo{found->second.scopeDepth, found->second.bindingIndex,
                          std::nullopt};
  }
  return std::nullopt;
}

void Resolver::emitError(std::string msg, std::optional<std::string> hint,
                         Span span, std::optional<Error> code) {
  m_bag.emit(Diagnostic{
      .severity = Severity::Error,
      .code = code,
      .hint = std::move(hint),
      .message = std::move(msg),
      .filename = std::string(m_filename),
      .sourceLine = getSourceLine(m_source, span.startLine),
      .span = span,
  });
}

void Resolver::resolve(std::span<ExprPtr> program) {
  if (program.empty()) {
    emitError("empty program", std::nullopt, Span{0, 0, 0, 0},
              ResolutionError::EmptyProgram);
    return;
  }

  collectTopLevel(program);

  for (ExprPtr &expr : program)
    resolveExpr(*expr);
}

void Resolver::collectTopLevel(std::span<ExprPtr> program) {
  for (ExprPtr &expr : program) {
    if (std::holds_alternative<Package>(expr->val))
      continue;
    if (std::holds_alternative<Import>(expr->val))
      continue;
    if (auto *let = std::get_if<Let>(&expr->val))
      if (!let->resolved)
        let->resolved = declare(let->name);
  }
}

void Resolver::resolveExpr(Expr &expr) {
  std::visit(
      [&](auto &node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, Let>)
          resolveLet(node);
        else if constexpr (std::is_same_v<T, Lambda>)
          resolveLambda(node);
        else if constexpr (std::is_same_v<T, Block>)
          resolveBlock(node);
        else if constexpr (std::is_same_v<T, If>)
          resolveIf(node);
        else if constexpr (std::is_same_v<T, Call>)
          resolveCall(node);
        else if constexpr (std::is_same_v<T, Binary>)
          resolveBinary(node);
        else if constexpr (std::is_same_v<T, Unary>)
          resolveUnary(node);
        else if constexpr (std::is_same_v<T, Identifier>)
          resolveIdentifier(node);
        else if constexpr (std::is_same_v<T, FieldAccess>)
          resolveFieldAccess(node);
      },
      expr.val);
}

void Resolver::resolveLet(Let &let) {
  resolveExpr(*let.value);
  if (!let.resolved)
    let.resolved = declare(let.name);
}

void Resolver::resolveLambda(Lambda &lambda) {
  pushScope();
  for (Param &param : lambda.params) {
    ResolvedInfo info = declare(param.name);
    param.bindingIndex = info.bindingIndex;
  }
  resolveBlock(lambda.body);
  popScope();
}

void Resolver::resolveBlock(Block &block) {
  pushScope();
  for (ExprPtr &expr : block.exprs) {
    if (auto *let = std::get_if<Let>(&expr->val))
      if (!let->resolved)
        let->resolved = declare(let->name);
  }
  for (ExprPtr &expr : block.exprs)
    resolveExpr(*expr);
  popScope();
}

void Resolver::resolveIf(If &ifExpr) {
  resolveExpr(*ifExpr.cond);
  resolveBlock(ifExpr.then);
  if (ifExpr.els)
    resolveBlock(*ifExpr.els);
}

void Resolver::resolveCall(Call &call) {
  resolveExpr(*call.callee);
  for (ExprPtr &arg : call.args)
    resolveExpr(*arg);
}

void Resolver::resolveBinary(Binary &binary) {
  resolveExpr(*binary.lhs);
  resolveExpr(*binary.rhs);
}

void Resolver::resolveUnary(Unary &unary) { resolveExpr(*unary.rhs); }

void Resolver::resolveIdentifier(Identifier &identifier) {
  auto info = lookup(identifier.name);
  if (!info) {
    emitError("undefined name '" + identifier.name + "'",
              std::string("make sure '" + identifier.name +
                          "' is defined before it is used"),
              identifier.span, ResolutionError::UndefinedName);
    return;
  }
  identifier.resolved = *info;
}

void Resolver::resolveFieldAccess(FieldAccess &fa) { resolveExpr(*fa.object); }