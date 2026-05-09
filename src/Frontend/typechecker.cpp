#include <Iris/Common/utils.hpp>
#include <Iris/Frontend/scheme.hpp>
#include <Iris/Frontend/typechecker.hpp>
#include <Iris/Frontend/unifier.hpp>

#include <format>

namespace Iris::Frontend {

TypeChecker::TypeChecker(std::string_view filename, std::string_view source,
                         Common::DiagnosticBag &bag)
    : m_filename(filename), m_source(source), m_bag(bag) {}

void TypeChecker::check(std::span<ExprPtr> program) {
  for (auto &expr : program)
    infer(*expr);
}

TypePtr TypeChecker::infer(Expr &expr) {
  return std::visit([&](auto &node) -> TypePtr { return inferNode(node); },
                    expr.val);
}

TypePtr TypeChecker::freshMeta() { return Type::meta(m_nextMeta++); }

void TypeChecker::emitError(std::string msg, std::optional<std::string> hint,
                            Common::Span span, Common::TypeError code) {
  m_bag.emit(Common::Diagnostic{
      .severity = Common::Severity::Error,
      .code = code,
      .hint = std::move(hint),
      .message = std::move(msg),
      .filename = std::string(m_filename),
      .sourceLine = Common::getSourceLine(m_source, span.startLine),
      .span = span,
  });
}

TypePtr TypeChecker::inferNode(NumericLiteral &) { return Type::num(); }
TypePtr TypeChecker::inferNode(StringLiteral &) { return Type::str(); }
TypePtr TypeChecker::inferNode(BoolLiteral &) { return Type::bool_(); }
TypePtr TypeChecker::inferNode(Package &) { return Type::unit(); }
TypePtr TypeChecker::inferNode(Import &) { return Type::unit(); }

TypePtr TypeChecker::inferNode(Identifier &id) {
  if (!id.resolved) {
    emitError(
        std::format("unresolved identifier '{}'", id.name),
        std::format("make sure '{}' is defined before it is used", id.name),
        id.span, Common::TypeError::UnresolvedIdentifier);
    return freshMeta();
  }

  const Scheme *scheme = m_env.lookup(id.resolved->bindingIndex);
  if (!scheme) {
    TypePtr ty = freshMeta();
    m_env.bindMono(id.resolved->bindingIndex, ty);
    return ty;
  }

  return instantiate(*scheme, m_nextMeta);
}

TypePtr TypeChecker::inferNode(Let &let) {
  TypePtr valTy = infer(*let.value);

  if (let.resolved) {
    TypePtr resolved = resolveType(valTy);
    auto envMetas = m_env.allFreeMetas();
    Scheme scheme = generalize(resolved, envMetas);
    m_env.bind(let.resolved->bindingIndex, std::move(scheme));
  }

  return valTy;
}

TypePtr TypeChecker::inferNode(Lambda &lam) {
  m_env.pushScope();

  std::vector<TypePtr> paramTypes;
  paramTypes.reserve(lam.params.size());

  for (auto &param : lam.params) {
    TypePtr paramTy = freshMeta();
    m_env.bindMono(param.bindingIndex, paramTy);
    paramTypes.push_back(paramTy);
  }

  TypePtr bodyTy = inferBlock(lam.body);
  m_env.popScope();

  TypePtr result = bodyTy;
  for (int i = static_cast<int>(lam.params.size()) - 1; i >= 0; --i)
    result = Type::pi(lam.params[i].name, paramTypes[i], result);

  return result;
}

TypePtr TypeChecker::inferNode(Block &block) { return inferBlock(block); }

TypePtr TypeChecker::inferBlock(Block &block) {
  m_env.pushScope();

  TypePtr last = Type::unit();
  for (auto &expr : block.exprs)
    last = infer(*expr);

  m_env.popScope();
  return last;
}

TypePtr TypeChecker::inferNode(Call &call) {
  TypePtr calleeTy = resolveType(infer(*call.callee));
  TypePtr resultTy = freshMeta();

  TypePtr expected = resultTy;
  for (int i = static_cast<int>(call.args.size()) - 1; i >= 0; --i) {
    TypePtr argTy = infer(*call.args[i]);
    expected = Type::pi("_", argTy, expected);
  }

  try {
    unify(calleeTy, expected);
  } catch (const std::exception &e) {
    TypePtr resolved = resolveType(calleeTy);
    if (!std::holds_alternative<PiType>(resolved->val)) {
      emitError("expression is not callable",
                "only functions can be called with '(...)'",
                spanOf(*call.callee), Common::TypeError::NotCallable);
    } else {
      emitError(
          std::format("type error in function call: {}", e.what()),
          "check that argument types match the function's parameter types",
          spanOf(*call.callee), Common::TypeError::TypeMismatch);
    }
  }

  return resolveType(resultTy);
}

TypePtr TypeChecker::inferNode(Binary &bin) {
  TypePtr lhsTy = infer(*bin.lhs);
  TypePtr rhsTy = infer(*bin.rhs);

  const auto requireNum = [&](TypePtr ty, const Expr &side) {
    try {
      unify(ty, Type::num());
    } catch (const std::exception &) {
      emitError(
          "expected Num",
          "only numeric values support arithmetic and comparison operators",
          spanOf(side), Common::TypeError::ArithmeticNonNum);
    }
  };

  const auto requireBool = [&](TypePtr ty, const Expr &side) {
    try {
      unify(ty, Type::bool_());
    } catch (const std::exception &) {
      emitError("expected Bool",
                "only boolean values are valid here — use '&&' or '||'",
                spanOf(side), Common::TypeError::LogicNonBool);
    }
  };

  switch (bin.op) {
  case BinaryOp::Addition:
  case BinaryOp::Subtraction:
  case BinaryOp::Multiplication:
  case BinaryOp::Division:
  case BinaryOp::Modulo:
  case BinaryOp::Power:
    requireNum(lhsTy, *bin.lhs);
    requireNum(rhsTy, *bin.rhs);
    return Type::num();

  case BinaryOp::LessThan:
  case BinaryOp::LessThanEqual:
  case BinaryOp::GreaterThan:
  case BinaryOp::GreaterThanEqual:
    requireNum(lhsTy, *bin.lhs);
    requireNum(rhsTy, *bin.rhs);
    return Type::bool_();

  case BinaryOp::Equal:
  case BinaryOp::NotEqual:
    try {
      unify(lhsTy, rhsTy);
    } catch (const std::exception &) {
      emitError("cannot compare values of different types",
                "both sides of '==' or '!=' must have the same type", bin.span,
                Common::TypeError::TypeMismatch);
    }
    return Type::bool_();

  case BinaryOp::And:
  case BinaryOp::Or:
    requireBool(lhsTy, *bin.lhs);
    requireBool(rhsTy, *bin.rhs);
    return Type::bool_();
  }

  __builtin_unreachable();
}

TypePtr TypeChecker::inferNode(Unary &un) {
  TypePtr ty = infer(*un.rhs);

  switch (un.op) {
  case UnaryOp::Negation:
    try {
      unify(ty, Type::num());
    } catch (const std::exception &) {
      emitError("unary '-' requires a Num",
                "only numeric values can be negated", spanOf(*un.rhs),
                Common::TypeError::ArithmeticNonNum);
    }
    return Type::num();

  case UnaryOp::Not:
    try {
      unify(ty, Type::bool_());
    } catch (const std::exception &) {
      emitError("unary '!' requires a Bool",
                "only boolean values can be logically negated", spanOf(*un.rhs),
                Common::TypeError::LogicNonBool);
    }
    return Type::bool_();
  }

  __builtin_unreachable();
}

TypePtr TypeChecker::inferNode(If &ifExpr) {
  TypePtr condTy = infer(*ifExpr.cond);

  try {
    unify(condTy, Type::bool_());
  } catch (const std::exception &) {
    emitError("if condition must be Bool",
              "the condition between 'if' and '{' must evaluate to a boolean",
              spanOf(*ifExpr.cond), Common::TypeError::ConditionNotBool);
  }

  TypePtr thenTy = inferBlock(ifExpr.then);

  if (ifExpr.els) {
    TypePtr elsTy = inferBlock(*ifExpr.els);
    try {
      unify(thenTy, elsTy);
    } catch (const std::exception &) {
      emitError("if branches have different types",
                "both branches of an if expression must produce the same type",
                ifExpr.span, Common::TypeError::BranchMismatch);
    }
    return resolveType(thenTy);
  }

  return Type::unit();
}

TypePtr TypeChecker::inferNode(FieldAccess &fa) {
  infer(*fa.object);
  return freshMeta();
}
} // namespace Iris::Frontend