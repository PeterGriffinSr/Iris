#pragma once

#include "src/include/error_codes.hpp"
#include "src/include/token.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Lexer {
public:
  explicit Lexer(std::string_view source,
                 std::string_view filename = "<input>");

  std::vector<Token> tokenize();

private:
  std::string_view m_source, m_filename;
  size_t m_pos{0};
  uint32_t m_line{1}, m_col{1};
  TokenType m_last{TokenType::Eof};

  [[nodiscard]] bool atEnd() const noexcept;
  [[nodiscard]] char peek(size_t offset = 0) const noexcept;
  char advance() noexcept;
  bool match(char expected) noexcept;
  [[nodiscard]] bool shouldInsertSemicolon() const noexcept;
  [[nodiscard]] static TokenType effectiveLastType(const Token &tok) noexcept;
  [[nodiscard]] std::string getSourceLine(uint32_t lineNum) const;

  [[noreturn]] void fatalError(std::optional<Error> code, std::string msg,
                               std::optional<std::string> hint, uint32_t line,
                               uint32_t col) const;

  void skipWhitespaceAndComments(std::vector<Token> &out);
  [[nodiscard]] Token makeToken(TokenType type, std::string value) const;
  [[nodiscard]] Token scanNumber();
  [[nodiscard]] Token scanString();
  [[nodiscard]] Token scanIdentifierOrKeyword();
  [[nodiscard]] Token scanOperatorOrDelimiter(uint32_t startCol);
};