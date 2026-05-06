#include "src/include/parser.hpp"
#include "src/include/ast.hpp"
#include "src/include/error.hpp"
#include "src/include/token.hpp"
#include "src/include/utils.hpp"
#include <cassert>
#include <optional>
#include <sstream>
#include <vector>

Parser::Parser(std::span<const Token> tokens, std::string_view filename,
               std::string_view source, DiagnosticBag &bag)
    : m_tokens(tokens), m_filename(filename), m_source(source), m_bag(bag) {}

const Token &Parser::peek(size_t offset) const noexcept {
  size_t idx = m_pos + offset;
  if (idx >= m_tokens.size())
    return m_tokens.back();
  return m_tokens[idx];
}

const Token &Parser::previous() const noexcept {
  assert(m_pos > 0);
  return m_tokens[m_pos - 1];
}

bool Parser::atEnd() const noexcept { return peek().type == TokenType::Eof; }

const Token &Parser::advance() {
  if (!atEnd())
    ++m_pos;
  return previous();
}

bool Parser::check(TokenType type, std::string_view value) const noexcept {
  const Token &t = peek();
  if (t.type != type)
    return false;
  if (!value.empty() && t.value != value)
    return false;
  return true;
}

bool Parser::match(TokenType type, std::string_view value) {
  if (!check(type, value))
    return false;
  advance();
  return true;
}

const Token &Parser::expect(TokenType type, std::string_view value,
                            std::string msg, std::optional<Error> code) {
  if (check(type, value))
    return advance();

  emitError(std::move(msg), std::nullopt, peek().span, code);
  return peek();
}

void Parser::emitError(std::string msg, std::optional<std::string> hint,
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

void Parser::synchronize() {
  while (!atEnd()) {
    if (match(TokenType::Semicolon))
      return;

    const Token &t = peek();
    if (t.type == TokenType::Keyword && (t.value == "let" || t.value == "if"))
      return;
    if (t.type == TokenType::Delimiter && t.value == "}")
      return;

    advance();
  }
}

std::optional<Parser::OpInfo>
Parser::infixOpInfo(std::string_view value) noexcept {
  if (value == "||")
    return OpInfo{1, false};
  if (value == "&&")
    return OpInfo{2, false};
  if (value == "==" || value == "!=")
    return OpInfo{3, false};
  if (value == "<" || value == "<=" || value == ">" || value == ">=")
    return OpInfo{4, false};
  if (value == "+" || value == "-")
    return OpInfo{5, false};
  if (value == "*" || value == "/" || value == "%")
    return OpInfo{6, false};
  if (value == "^")
    return OpInfo{7, true};
  return std::nullopt;
}

BinaryOp Parser::tokenToBinaryOp(std::string_view value) {
  if (value == "+")
    return BinaryOp::Addition;
  if (value == "-")
    return BinaryOp::Subtraction;
  if (value == "*")
    return BinaryOp::Multiplication;
  if (value == "/")
    return BinaryOp::Division;
  if (value == "%")
    return BinaryOp::Modulo;
  if (value == "^")
    return BinaryOp::Power;
  if (value == "==")
    return BinaryOp::Equal;
  if (value == "!=")
    return BinaryOp::NotEqual;
  if (value == "<")
    return BinaryOp::LessThan;
  if (value == "<=")
    return BinaryOp::LessThanEqual;
  if (value == ">")
    return BinaryOp::GreaterThan;
  if (value == ">=")
    return BinaryOp::GreaterThanEqual;
  if (value == "&&")
    return BinaryOp::And;
  if (value == "||")
    return BinaryOp::Or;
  assert(false && "unknown binary operator");
  return BinaryOp::Addition;
}

std::vector<ExprPtr> Parser::parse() {
  std::vector<ExprPtr> exprs;

  if (check(TokenType::Keyword, "package")) {
    ExprPtr pkg = parsePackage();
    if (pkg)
      exprs.push_back(std::move(pkg));
  }

  while (check(TokenType::Keyword, "import")) {
    ExprPtr import = parseImport();
    if (import)
      exprs.push_back(std::move(import));
  }

  while (!atEnd()) {
    if (match(TokenType::Semicolon))
      continue;

    if (check(TokenType::Keyword, "package")) {
      emitError("'package' must appear at the top of the file", std::nullopt,
                peek().span, std::nullopt);
      advance();
      synchronize();
      continue;
    }
    if (check(TokenType::Keyword, "import")) {
      emitError(
          "'import' declarations must appear before any other expressions",
          std::nullopt, peek().span, std::nullopt);
      advance();
      synchronize();
      continue;
    }

    ExprPtr e = parseExpr();
    if (e)
      exprs.push_back(std::move(e));
    else
      synchronize();
  }

  return exprs;
}

ExprPtr Parser::parseExpr() {
  if (check(TokenType::Keyword, "let"))
    return parseLet();
  if (check(TokenType::Keyword, "if"))
    return parseIf();
  return parseBinary();
}

ExprPtr Parser::parseLet() {
  const Token &letTok = advance();
  Span letSpan = letTok.span;

  if (!check(TokenType::Identifier)) {
    emitError(
        "expected identifier after 'let'",
        std::string("write: let <name> = <expr>  or  let <name>(...) { ... }"),
        peek().span, ParserError::ExpectedIdentifierAfterLet);
    return nullptr;
  }

  const Token &nameTok = advance();
  std::string name = nameTok.value;

  if (match(TokenType::Delimiter, "(")) {
    std::vector<std::string> params;
    Span paramStart = previous().span;

    while (!check(TokenType::Delimiter, ")") && !atEnd()) {
      if (!check(TokenType::Identifier)) {
        emitError("expected parameter name",
                  std::string("function parameters must be plain identifiers"),
                  peek().span, ParserError::ExpectedParameterName);
        synchronize();
        return nullptr;
      }
      params.push_back(advance().value);
      if (!match(TokenType::Delimiter, ","))
        break;
    }

    expect(TokenType::Delimiter, ")", "expected ')' after parameter list",
           ParserError::ExpectedClosingParen);

    if (!check(TokenType::Delimiter, "{")) {
      emitError("expected '{' after parameter list",
                std::string("function body must be a block: let " + name +
                            "(...) { ... }"),
                peek().span, ParserError::ExpectedBlockAfterParams);
      return nullptr;
    }

    Block body = parseBlock();
    Span lambdaSpan = Span::merge(paramStart, body.span);

    std::vector<Param> paramList;
    paramList.reserve(params.size());
    for (std::string &p : params)
      paramList.push_back(Param{.name = std::move(p)});

    auto lambda = makeExpr(Lambda{
        .params = std::move(paramList),
        .body = std::move(body),
        .span = lambdaSpan,
    });

    return makeExpr(Let{.name = std::move(name),
                        .value = std::move(lambda),
                        .span = Span::merge(letSpan, lambdaSpan)});
  }

  expect(TokenType::Operator, "=", "expected '=' after name in let binding",
         ParserError::ExpectedEquals);

  ExprPtr value = parseExpr();
  if (!value)
    return nullptr;

  match(TokenType::Semicolon);
  Span fullSpan = Span::merge(letSpan, spanOf(*value));

  return makeExpr(Let{
      .name = std::move(name),
      .value = std::move(value),
      .span = fullSpan,
  });
}

ExprPtr Parser::parseIf() {
  const Token &ifTok = advance();

  ExprPtr cond = parseBinary();
  if (!cond) {
    emitError("expected condition after 'if'", std::nullopt, peek().span,
              ParserError::ExpectedIfCondition);
    return nullptr;
  }

  if (!check(TokenType::Delimiter, "{")) {
    emitError("expected '{' after if condition",
              std::string("wrap the then-branch in braces: if <cond> { ... }"),
              peek().span, ParserError::ExpectedThenBlock);
    return nullptr;
  }
  Block thenBlock = parseBlock();

  std::optional<Block> elsBlock;
  if (match(TokenType::Keyword, "else")) {
    if (!check(TokenType::Delimiter, "{")) {
      emitError("expected '{' after 'else'",
                std::string("wrap the else-branch in braces: else { ... }"),
                peek().span, ParserError::ExpectedElseBlock);
      return nullptr;
    }
    elsBlock = parseBlock();
  }

  Span fullSpan = ifTok.span;
  if (elsBlock)
    fullSpan = Span::merge(fullSpan, elsBlock->span);
  else
    fullSpan = Span::merge(fullSpan, thenBlock.span);

  return makeExpr(If{
      .cond = std::move(cond),
      .then = std::move(thenBlock),
      .els = std::move(elsBlock),
      .span = fullSpan,
  });
}

ExprPtr Parser::parseBinary(int minPrec) {
  ExprPtr lhs = parseUnary();
  if (!lhs)
    return nullptr;

  while (true) {
    const Token &op = peek();
    if (op.type != TokenType::Operator)
      break;

    auto info = infixOpInfo(op.value);
    if (!info || info->prec < minPrec)
      break;

    advance();

    int nextPrec = info->rightAssoc ? info->prec : info->prec + 1;
    ExprPtr rhs = parseBinary(nextPrec);
    if (!rhs) {
      emitError("expected expression after '" + std::string(op.value) + "'",
                std::nullopt, peek().span,
                ParserError::ExpectedRhsAfterBinaryOp);
      return nullptr;
    }

    Span span = Span::merge(spanOf(*lhs), spanOf(*rhs));
    lhs = makeExpr(Binary{
        .lhs = std::move(lhs),
        .op = tokenToBinaryOp(op.value),
        .rhs = std::move(rhs),
        .span = span,
    });
  }

  return lhs;
}

ExprPtr Parser::parseUnary() {
  if (check(TokenType::Operator, "-") || check(TokenType::Operator, "!")) {
    const Token &opTok = advance();
    UnaryOp op = opTok.value == "-" ? UnaryOp::Negation : UnaryOp::Not;

    ExprPtr rhs = parseUnary();
    if (!rhs) {
      emitError("expected expression after unary '" + opTok.value + "'",
                std::nullopt, peek().span,
                ParserError::ExpectedRhsAfterUnaryOp);
      return nullptr;
    }

    Span span = Span::merge(opTok.span, spanOf(*rhs));
    return makeExpr(Unary{
        .op = op,
        .rhs = std::move(rhs),
        .span = span,
    });
  }

  return parseCall();
}

ExprPtr Parser::parseCall() {
  ExprPtr expr = parsePrimary();
  if (!expr)
    return nullptr;

  while (true) {
    if (match(TokenType::Delimiter, "(")) {
      std::vector<ExprPtr> args;
      if (!check(TokenType::Delimiter, ")")) {
        do {
          ExprPtr arg = parseExpr();
          if (!arg) {
            synchronize();
            return nullptr;
          }
          args.push_back(std::move(arg));
        } while (match(TokenType::Delimiter, ","));
      }
      expect(TokenType::Delimiter, ")", "expected ')' after argument list",
             ParserError::ExpectedClosingCallParen);
      Span callSpan = Span::merge(spanOf(*expr), previous().span);
      expr = makeExpr(Call{
          .callee = std::move(expr),
          .args = std::move(args),
          .span = callSpan,
      });
    } else if (check(TokenType::Operator, ".")) {
      advance();
      if (!check(TokenType::Identifier)) {
        emitError("expected field name after '.'",
                  std::string("write: <expr>.<name>"), peek().span,
                  std::nullopt);
        return nullptr;
      }
      const Token &field = advance();
      Span faSpan = Span::merge(spanOf(*expr), field.span);
      expr = makeExpr(FieldAccess{
          .object = std::move(expr),
          .field = field.value,
          .span = faSpan,
      });
    } else {
      break;
    }
  }

  return expr;
}

ExprPtr Parser::parsePrimary() {
  const Token &tok = peek();

  if (tok.type == TokenType::Number) {
    advance();
    double value = 0.0;
    value = std::strtod(tok.value.data(), nullptr);
    return makeExpr(NumericLiteral{.value = value, .span = tok.span});
  }
  if (tok.type == TokenType::String) {
    advance();
    return makeExpr(StringLiteral{.value = tok.value, .span = tok.span});
  }
  if (tok.type == TokenType::Keyword && tok.value == "true") {
    advance();
    return makeExpr(BoolLiteral{.value = true, .span = tok.span});
  }
  if (tok.type == TokenType::Keyword && tok.value == "false") {
    advance();
    return makeExpr(BoolLiteral{.value = false, .span = tok.span});
  }
  if (tok.type == TokenType::Identifier) {
    advance();
    return makeExpr(Identifier{.name = tok.value, .span = tok.span});
  }
  if (tok.type == TokenType::Delimiter && tok.value == "(") {
    advance();
    ExprPtr inner = parseExpr();
    if (!inner)
      return nullptr;
    expect(TokenType::Delimiter, ")",
           "expected ')' to close grouped expression",
           ParserError::ExpectedClosingGroupParen);
    return inner;
  }
  if (tok.type != TokenType::Eof) {
    emitError(
        "unexpected token '" + tok.value + "'",
        std::string(
            "expected a value: number, string, true, false, or identifier"),
        tok.span, ParserError::UnexpectedToken);
    advance();
  }
  return nullptr;
}

Block Parser::parseBlock() {
  const Token &open = advance();
  Span blockSpan = open.span;
  std::vector<ExprPtr> exprs;

  while (!check(TokenType::Delimiter, "}") && !atEnd()) {
    if (match(TokenType::Semicolon))
      continue;

    ExprPtr e = parseExpr();
    if (e) {
      exprs.push_back(std::move(e));
      match(TokenType::Semicolon);
    } else {
      synchronize();
    }
  }

  const Token &close = peek();
  if (close.type == TokenType::Delimiter && close.value == "}") {
    blockSpan = Span::merge(blockSpan, close.span);
    advance();
  } else {
    emitError("expected '}' to close block", std::nullopt, blockSpan,
              ParserError::ExpectedClosingBrace);
  }

  if (exprs.empty()) {
    emitError("empty block is not allowed",
              std::string("a block must contain at least one expression"),
              blockSpan, ParserError::EmptyBlock);
  }

  return Block{.exprs = std::move(exprs), .span = blockSpan};
}

ExprPtr Parser::parsePackage() {
  const Token &kwTok = advance();
  if (!check(TokenType::Identifier)) {
    emitError("expected package name after 'package'",
              std::string("write: package <name>"), peek().span,
              ParserError::ExpectedIdentifierAfterLet);
    return nullptr;
  }

  const Token &nameTok = advance();
  match(TokenType::Semicolon);

  return makeExpr(Package{
      .name = nameTok.value,
      .span = Span::merge(kwTok.span, nameTok.span),
  });
}

ExprPtr Parser::parseImport() {
  const Token &kwTok = advance();

  if (!check(TokenType::String)) {
    emitError("expected string after 'import'",
              std::string("write: import \"collection:path\""), peek().span,
              std::nullopt);
    return nullptr;
  }

  const Token &strTok = advance();
  std::string raw = strTok.value;

  auto colonPos = raw.find(':');
  if (colonPos == std::string::npos) {
    emitError(
        "invalid import path '" + raw + "'",
        std::string("import paths must have the form \"collection:path\""),
        strTok.span, std::nullopt);
    return nullptr;
  }

  std::string collection = raw.substr(0, colonPos);
  std::string pathStr = raw.substr(colonPos + 1);

  if (collection.empty() || pathStr.empty()) {
    emitError("invalid import path '" + raw + "'",
              std::string("collection and path must both be non-empty"),
              strTok.span, std::nullopt);
    return nullptr;
  }

  std::vector<std::string> segments;
  std::istringstream ss(pathStr);
  std::string seg;
  while (std::getline(ss, seg, '/')) {
    if (seg.empty()) {
      emitError("invalid import path '" + raw + "'",
                std::string("path segments must not be empty"), strTok.span,
                std::nullopt);
      return nullptr;
    }
    segments.push_back(seg);
  }

  std::string localName = segments.back();
  match(TokenType::Semicolon);

  return makeExpr(Import{
      .collection = std::move(collection),
      .pathSegments = std::move(segments),
      .localName = std::move(localName),
      .span = Span::merge(kwTok.span, strTok.span),
  });
}