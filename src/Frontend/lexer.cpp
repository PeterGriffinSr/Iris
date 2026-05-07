#include <Iris/Common/error.hpp>
#include <Iris/Common/utils.hpp>
#include <Iris/Frontend/lexer.hpp>
#include <Iris/Frontend/token.hpp>
#include <unordered_map>
#include <unordered_set>

namespace Iris::Frontend {

static const std::unordered_map<std::string, TokenType> s_keywords = {
    {"let", TokenType::Keyword},   {"if", TokenType::Keyword},
    {"else", TokenType::Keyword},  {"true", TokenType::Keyword},
    {"false", TokenType::Keyword}, {"package", TokenType::Keyword},
    {"import", TokenType::Keyword}};

static const std::unordered_set<std::string> s_openingKeywords = {
    "let", "if", "else", "package", "import"};

Lexer::Lexer(std::string_view source, std::string_view filename,
             Common::DiagnosticBag &bag)
    : m_source(source), m_filename(filename), m_bag(bag) {
  if (m_source.size() >= 3 && (unsigned char)m_source[0] == 0xEF &&
      (unsigned char)m_source[1] == 0xBB &&
      (unsigned char)m_source[2] == 0xBF) {
    m_source.remove_prefix(3);
  }
}

bool Lexer::atEnd() const noexcept { return m_pos >= m_source.size(); }

char Lexer::peek(size_t offset) const noexcept {
  size_t idx = m_pos + offset;
  return idx < m_source.size() ? m_source[idx] : '\0';
}

char Lexer::advance() noexcept {
  char c = m_source[m_pos++];
  if (c == '\n') {
    ++m_line;
    m_col = 1;
  } else {
    ++m_col;
  }
  return c;
}

bool Lexer::match(char expected) noexcept {
  if (atEnd() || m_source[m_pos] != expected)
    return false;
  advance();
  return true;
}

Common::Span Lexer::makeSpan(uint32_t startLine,
                             uint32_t startCol) const noexcept {
  return Common::Span{startLine, startCol, m_line, m_col};
}

Token Lexer::makeToken(TokenType type, std::string value,
                       Common::Span span) const {
  return Token{type, std::move(value), span};
}

[[noreturn]] void Lexer::fatalError(std::optional<Common::Error> code,
                                    std::string msg,
                                    std::optional<std::string> hint,
                                    Common::Span span) const {
  emitFatal(Common::Diagnostic{
      .severity = Common::Severity::Error,
      .code = code,
      .hint = std::move(hint),
      .message = std::move(msg),
      .filename = std::string(m_filename),
      .sourceLine = Common::getSourceLine(m_source, span.startLine),
      .span = span,
  });
}

bool Lexer::shouldInsertSemicolon() const noexcept {
  switch (m_last) {
  case TokenType::Identifier:
  case TokenType::Number:
  case TokenType::String:
  case TokenType::Keyword:
  case TokenType::Delimiter:
    return true;
  default:
    return false;
  }
}

bool Lexer::needsSemicolonBefore(char next) const noexcept {
  switch (m_last) {
  case TokenType::Identifier:
  case TokenType::Number:
  case TokenType::String:
    break;
  case TokenType::Delimiter:

    if (m_lastValue != ")" && m_lastValue != "]")
      return false;
    break;
  default:
    return false;
  }

  if (next == '}') {
    return m_parenDepth == 0 &&
           (m_last == TokenType::Identifier || m_last == TokenType::Number ||
            m_last == TokenType::String);
  }

  if (!std::isalpha(next) && next != '_' && !std::isdigit(next) && next != '"')
    return false;

  if (std::isalpha(next) || next == '_') {
    size_t start = m_pos;
    size_t i = start;
    while (i < m_source.size() &&
           (std::isalnum(m_source[i]) || m_source[i] == '_'))
      ++i;
    if (m_source.substr(start, i - start) == "else")
      return false;
  }

  return true;
}

TokenType Lexer::effectiveLastType(const Token &tok) noexcept {
  if (tok.type == TokenType::Delimiter) {
    char v = tok.value[0];
    if (v == '(' || v == '{' || v == '[')
      return TokenType::Operator;
  }
  if (tok.type == TokenType::Keyword && s_openingKeywords.count(tok.value))
    return TokenType::Operator;
  return tok.type;
}

void Lexer::skipWhitespaceAndComments(std::vector<Token> &out) {
  while (!atEnd()) {
    char c = peek();
    if (c == '\n') {
      if (shouldInsertSemicolon()) {
        Common::Span sp{m_line, m_col, m_line, m_col};
        out.push_back(makeToken(TokenType::Semicolon, ";", sp));
        m_last = TokenType::Semicolon;
        m_lastValue = ";";
      }
      advance();
    } else if (c == ' ' || c == '\r' || c == '\t') {
      advance();
    } else {
      break;
    }
  }
}

Token Lexer::scanNumber(uint32_t startLine, uint32_t startCol) {
  size_t start = m_pos - 1;

  while (!atEnd() && std::isdigit(peek()))
    advance();

  if (peek() == '.' && std::isdigit(peek(1))) {
    advance();
    while (!atEnd() && std::isdigit(peek()))
      advance();
  }

  if (peek() == 'e' || peek() == 'E') {
    advance();
    if (peek() == '+' || peek() == '-')
      advance();
    while (!atEnd() && std::isdigit(peek()))
      advance();
  }

  return makeToken(TokenType::Number,
                   std::string(m_source.substr(start, m_pos - start)),
                   makeSpan(startLine, startCol));
}

Token Lexer::scanString(uint32_t startLine, uint32_t startCol) {
  std::string value;

  while (!atEnd() && peek() != '"') {
    char c = advance();
    if (c != '\\') {
      value += c;
      continue;
    }

    if (atEnd())
      fatalError(Common::LexerError::UnterminatedString,
                 "unterminated string escape", std::nullopt,
                 makeSpan(startLine, startCol));

    char esc = advance();
    switch (esc) {
    case 'n':
      value += '\n';
      break;
    case 't':
      value += '\t';
      break;
    case 'r':
      value += '\r';
      break;
    case '"':
      value += '"';
      break;
    case '\\':
      value += '\\';
      break;
    default:
      fatalError(Common::LexerError::UnknownEscape,
                 std::string("unknown escape sequence '\\") + esc + '\'',
                 "valid sequences: \\n, \\t, \\r, \\\", \\\\",
                 makeSpan(m_line, m_col - 2));
    }
  }

  if (atEnd())
    fatalError(
        Common::LexerError::UnterminatedString, "unterminated string",
        "every string must be closed with a matching '\"' on the same line",
        makeSpan(startLine, startCol));

  advance();
  return makeToken(TokenType::String, std::move(value),
                   makeSpan(startLine, startCol));
}

Token Lexer::scanIdentifierOrKeyword(uint32_t startLine, uint32_t startCol) {
  size_t start = m_pos - 1;
  while (!atEnd() && (std::isalnum(peek()) || peek() == '_'))
    advance();

  std::string word(m_source.substr(start, m_pos - start));
  auto it = s_keywords.find(word);
  TokenType type =
      (it != s_keywords.end()) ? TokenType::Keyword : TokenType::Identifier;
  return makeToken(type, std::move(word), makeSpan(startLine, startCol));
}

Token Lexer::scanOperatorOrDelimiter(uint32_t startLine, uint32_t startCol) {
  char c = m_source[m_pos - 1];

  switch (c) {
  case '+':
    return makeToken(TokenType::Operator, "+", makeSpan(startLine, startCol));
  case '*':
    return makeToken(TokenType::Operator, "*", makeSpan(startLine, startCol));
  case '/':
    return makeToken(TokenType::Operator, "/", makeSpan(startLine, startCol));
  case '%':
    return makeToken(TokenType::Operator, "%", makeSpan(startLine, startCol));
  case '^':
    return makeToken(TokenType::Operator, "^", makeSpan(startLine, startCol));
  case '.':
    return makeToken(TokenType::Operator, ".", makeSpan(startLine, startCol));
  case '-':
    return makeToken(TokenType::Operator, match('>') ? "->" : "-",
                     makeSpan(startLine, startCol));
  case '=':
    return makeToken(TokenType::Operator, match('=') ? "==" : "=",
                     makeSpan(startLine, startCol));
  case '!':
    return makeToken(TokenType::Operator, match('=') ? "!=" : "!",
                     makeSpan(startLine, startCol));
  case '<':
    return makeToken(TokenType::Operator, match('=') ? "<=" : "<",
                     makeSpan(startLine, startCol));
  case '>':
    return makeToken(TokenType::Operator, match('=') ? ">=" : ">",
                     makeSpan(startLine, startCol));

  case '&':
    if (match('&'))
      return makeToken(TokenType::Operator, "&&",
                       makeSpan(startLine, startCol));
    m_bag.emit(Common::Diagnostic{
        .severity = Common::Severity::Error,
        .code = std::nullopt,
        .hint = std::string("use '&&' for logical and"),
        .message = "bitwise '&' is not supported",
        .filename = std::string(m_filename),
        .sourceLine = Common::getSourceLine(m_source, m_line),
        .span = makeSpan(startLine, startCol),
    });
    return makeToken(TokenType::Error, "&", makeSpan(startLine, startCol));

  case '|':
    if (match('|'))
      return makeToken(TokenType::Operator, "||",
                       makeSpan(startLine, startCol));
    m_bag.emit(Common::Diagnostic{
        .severity = Common::Severity::Error,
        .code = std::nullopt,
        .hint = std::string("use '||' for logical or"),
        .message = "bitwise '|' is not supported",
        .filename = std::string(m_filename),
        .sourceLine = Common::getSourceLine(m_source, m_line),
        .span = makeSpan(startLine, startCol),
    });
    return makeToken(TokenType::Error, "|", makeSpan(startLine, startCol));

  case '(':
    return makeToken(TokenType::Delimiter, "(", makeSpan(startLine, startCol));
  case ')':
    return makeToken(TokenType::Delimiter, ")", makeSpan(startLine, startCol));
  case '{':
    return makeToken(TokenType::Delimiter, "{", makeSpan(startLine, startCol));
  case '}':
    return makeToken(TokenType::Delimiter, "}", makeSpan(startLine, startCol));
  case '[':
    return makeToken(TokenType::Delimiter, "[", makeSpan(startLine, startCol));
  case ']':
    return makeToken(TokenType::Delimiter, "]", makeSpan(startLine, startCol));
  case ',':
    return makeToken(TokenType::Delimiter, ",", makeSpan(startLine, startCol));
  case ':':
    return makeToken(TokenType::Delimiter, ":", makeSpan(startLine, startCol));
  case ';':
    return makeToken(TokenType::Semicolon, ";", makeSpan(startLine, startCol));

  default:
    m_bag.emit(Common::Diagnostic{
        .severity = Common::Severity::Error,
        .code = std::nullopt,
        .hint =
            std::string("check for stray punctuation or copy-paste artifacts"),
        .message = std::string("unexpected character '") + c + '\'',
        .filename = std::string(m_filename),
        .sourceLine = Common::getSourceLine(m_source, m_line),
        .span = makeSpan(startLine, startCol),
    });
    return makeToken(TokenType::Error, std::string(1, c),
                     makeSpan(startLine, startCol));
  }
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  tokens.reserve(256);

  while (true) {
    skipWhitespaceAndComments(tokens);

    if (atEnd()) {
      if (shouldInsertSemicolon()) {
        Common::Span sp{m_line, m_col, m_line, m_col};
        tokens.push_back(makeToken(TokenType::Semicolon, ";", sp));
      }
      Common::Span sp{m_line, m_col, m_line, m_col};
      tokens.push_back(makeToken(TokenType::Eof, "", sp));
      break;
    }

    if (needsSemicolonBefore(peek())) {
      Common::Span sp{m_line, m_col, m_line, m_col};
      tokens.push_back(makeToken(TokenType::Semicolon, ";", sp));
      m_last = TokenType::Semicolon;
    }

    uint32_t startLine = m_line;
    uint32_t startCol = m_col;
    char c = advance();

    Token tok = [&]() -> Token {
      if (std::isdigit(c))
        return scanNumber(startLine, startCol);
      if (c == '"')
        return scanString(startLine, startCol);
      if (std::isalpha(c) || c == '_')
        return scanIdentifierOrKeyword(startLine, startCol);
      return scanOperatorOrDelimiter(startLine, startCol);
    }();

    if (tok.type == TokenType::Delimiter) {
      char v = tok.value[0];
      if (v == '(')
        ++m_parenDepth;
      else if (v == ')')
        --m_parenDepth;
      else if (v == '{')
        ++m_braceDepth;
      else if (v == '}')
        --m_braceDepth;
    }

    m_last = effectiveLastType(tok);
    m_lastValue = tok.value;
    tokens.push_back(std::move(tok));
  }

  return tokens;
}
} // namespace Iris::Frontend