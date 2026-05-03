#include "src/include/lexer.hpp"
#include <unordered_map>
#include <unordered_set>

static const std::unordered_map<std::string, TokenType> s_keywords = {
    {"let", TokenType::Keyword},   {"if", TokenType::Keyword},
    {"else", TokenType::Keyword},  {"true", TokenType::Keyword},
    {"false", TokenType::Keyword},
};

static const std::unordered_set<std::string> s_openingKeywords = {"let", "if",
                                                                  "else"};

Lexer::Lexer(std::string_view source, std::string_view filename,
             DiagnosticBag &bag)
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

Token Lexer::makeToken(TokenType type, std::string value) const {
  return Token{type, std::move(value), m_line, m_col};
}

std::string Lexer::getSourceLine(uint32_t lineNum) const {
  uint32_t current = 1;
  for (size_t i = 0; i < m_source.size(); ++i) {
    if (current == lineNum) {
      size_t end = m_source.find('\n', i);
      if (end == std::string_view::npos)
        end = m_source.size();
      return std::string(m_source.substr(i, end - i));
    }
    if (m_source[i] == '\n')
      ++current;
  }
  return {};
}

[[noreturn]] void Lexer::fatalError(std::optional<Error> code, std::string msg,
                                    std::optional<std::string> hint,
                                    uint32_t line, uint32_t col) const {
  emitFatal(Diagnostic{
      .severity = Severity::Error,
      .code = code,
      .hint = std::move(hint),
      .message = std::move(msg),
      .filename = std::string(m_filename),
      .sourceLine = getSourceLine(line),
      .line = line,
      .col = col,
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
  case TokenType::Delimiter:
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
        out.push_back(makeToken(TokenType::Semicolon, ";"));
        m_last = TokenType::Semicolon;
      }
      advance();
    } else if (c == ' ' || c == '\r' || c == '\t') {
      advance();
    } else {
      break;
    }
  }
}

Token Lexer::scanNumber() {
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
                   std::string(m_source.substr(start, m_pos - start)));
}

Token Lexer::scanString() {
  uint32_t startLine = m_line;
  uint32_t startCol = m_col - 1;
  std::string value;

  while (!atEnd() && peek() != '"') {
    char c = advance();
    if (c != '\\') {
      value += c;
      continue;
    }

    if (atEnd())
      fatalError(LexerError::UnterminatedString, "unterminated string escape",
                 std::nullopt, m_line, m_col);

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
      fatalError(LexerError::UnknownEscape,
                 std::string("unknown escape sequence '\\") + esc + '\'',
                 "valid sequences: \\n, \\t, \\r, \\\", \\\\", m_line,
                 m_col - 2);
    }
  }

  if (atEnd())
    fatalError(
        LexerError::UnterminatedString, "unterminated string",
        "every string must be closed with a matching '\"' on the same line",
        startLine, startCol);

  advance();
  return makeToken(TokenType::String, std::move(value));
}

Token Lexer::scanIdentifierOrKeyword() {
  size_t start = m_pos - 1;
  while (!atEnd() && (std::isalnum(peek()) || peek() == '_'))
    advance();

  std::string word(m_source.substr(start, m_pos - start));
  auto it = s_keywords.find(word);
  TokenType type =
      (it != s_keywords.end()) ? TokenType::Keyword : TokenType::Identifier;
  return makeToken(type, std::move(word));
}

Token Lexer::scanOperatorOrDelimiter(uint32_t col) {
  char c = m_source[m_pos - 1];

  switch (c) {
  case '+':
    return makeToken(TokenType::Operator, "+");
  case '*':
    return makeToken(TokenType::Operator, "*");
  case '/':
    return makeToken(TokenType::Operator, "/");
  case '%':
    return makeToken(TokenType::Operator, "%");
  case '^':
    return makeToken(TokenType::Operator, "^");
  case '.':
    return makeToken(TokenType::Operator, ".");
  case '-':
    return makeToken(TokenType::Operator, match('>') ? "->" : "-");
  case '=':
    return makeToken(TokenType::Operator, match('=') ? "==" : "=");
  case '!':
    return makeToken(TokenType::Operator, match('=') ? "!=" : "!");
  case '<':
    return makeToken(TokenType::Operator, match('=') ? "<=" : "<");
  case '>':
    return makeToken(TokenType::Operator, match('=') ? ">=" : ">");
  case '&':
    if (match('&'))
      return makeToken(TokenType::Operator, "&&");
    m_bag.emit(Diagnostic{.severity = Severity::Error,
                          .code = std::nullopt,
                          .hint = std::string("use '&&' for logical and"),
                          .message = "bitwise '&' is not supported",
                          .filename = std::string(m_filename),
                          .sourceLine = getSourceLine(m_line),
                          .line = m_line,
                          .col = col});
    return makeToken(TokenType::Error, "&");
  case '|':
    if (match('|'))
      return makeToken(TokenType::Operator, "||");
    m_bag.emit(Diagnostic{.severity = Severity::Error,
                          .code = std::nullopt,
                          .hint = std::string("use '||' for logical or"),
                          .message = "bitwise '|' is not supported",
                          .filename = std::string(m_filename),
                          .sourceLine = getSourceLine(m_line),
                          .line = m_line,
                          .col = col});
    return makeToken(TokenType::Error, "|");
  case '(':
    return makeToken(TokenType::Delimiter, "(");
  case ')':
    return makeToken(TokenType::Delimiter, ")");
  case '{':
    return makeToken(TokenType::Delimiter, "{");
  case '}':
    return makeToken(TokenType::Delimiter, "}");
  case '[':
    return makeToken(TokenType::Delimiter, "[");
  case ']':
    return makeToken(TokenType::Delimiter, "]");
  case ',':
    return makeToken(TokenType::Delimiter, ",");
  case ':':
    return makeToken(TokenType::Delimiter, ":");
  case ';':
    return makeToken(TokenType::Semicolon, ";");
  default:
    m_bag.emit(Diagnostic{
        .severity = Severity::Error,
        .code = std::nullopt,
        .hint =
            std::string("check for stray punctuation or copy-paste artifacts"),
        .message = std::string("unexpected character '") + c + '\'',
        .filename = std::string(m_filename),
        .sourceLine = getSourceLine(m_line),
        .line = m_line,
        .col = col,
    });
    return makeToken(TokenType::Error, std::string(1, c));
  }
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  tokens.reserve(256);

  while (true) {
    skipWhitespaceAndComments(tokens);

    if (atEnd()) {
      if (shouldInsertSemicolon())
        tokens.push_back(makeToken(TokenType::Semicolon, ";"));
      tokens.push_back(makeToken(TokenType::Eof, ""));
      break;
    }

    if (needsSemicolonBefore(peek())) {
      tokens.push_back(makeToken(TokenType::Semicolon, ";"));
      m_last = TokenType::Semicolon;
    }

    uint32_t startCol = m_col;
    char c = advance();
    Token tok = [&]() -> Token {
      if (std::isdigit(c))
        return scanNumber();
      if (c == '"')
        return scanString();
      if (std::isalpha(c) || c == '_')
        return scanIdentifierOrKeyword();
      return scanOperatorOrDelimiter(startCol);
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

    tok.column = startCol;
    m_last = effectiveLastType(tok);
    tokens.push_back(std::move(tok));
  }

  return tokens;
}