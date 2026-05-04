#pragma once

#include "src/include/error_codes.hpp"
#include "src/include/token.hpp"
#include <optional>
#include <vector>

class DiagnosticBag;

class Lexer {
public:
  explicit Lexer(std::string_view source, std::string_view filename,
                 DiagnosticBag &bag);

  std::vector<Token> tokenize();

private:
  std::string m_lastValue;
  std::string_view m_source, m_filename;
  size_t m_pos{0};
  uint32_t m_line{1}, m_col{1};
  TokenType m_last{TokenType::Eof};
  int m_parenDepth{0}, m_braceDepth{0};
  DiagnosticBag &m_bag;

  [[nodiscard]] bool atEnd() const noexcept;
  [[nodiscard]] char peek(size_t offset = 0) const noexcept;
  char advance() noexcept;
  bool match(char expected) noexcept;

  [[nodiscard]] Token makeToken(TokenType type, std::string value,
                                Span span) const;
  [[nodiscard]] Span makeSpan(uint32_t startLine,
                              uint32_t startCol) const noexcept;
  [[noreturn]] void fatalError(std::optional<Error> code, std::string msg,
                               std::optional<std::string> hint,
                               Span span) const;
  [[nodiscard]] bool shouldInsertSemicolon() const noexcept;
  [[nodiscard]] bool needsSemicolonBefore(char next) const noexcept;
  [[nodiscard]] static TokenType effectiveLastType(const Token &tok) noexcept;
  void skipWhitespaceAndComments(std::vector<Token> &out);
  [[nodiscard]] Token scanNumber(uint32_t startLine, uint32_t startCol);
  [[nodiscard]] Token scanString(uint32_t startLine, uint32_t startCol);
  [[nodiscard]] Token scanIdentifierOrKeyword(uint32_t startLine,
                                              uint32_t startCol);
  [[nodiscard]] Token scanOperatorOrDelimiter(uint32_t startLine,
                                              uint32_t startCol);
};