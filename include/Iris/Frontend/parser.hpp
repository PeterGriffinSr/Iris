#pragma once

#include <Iris/Common/error.hpp>
#include <Iris/Frontend/ast.hpp>
#include <Iris/Frontend/token.hpp>
#include <span>

namespace Iris::Frontend {

class Parser {
public:
  explicit Parser(std::span<const Token> tokens, std::string_view filename,
                  std::string_view source, Common::DiagnosticBag &bag);

  std::vector<ExprPtr> parse();

private:
  std::span<const Token> m_tokens;
  std::string_view m_filename;
  std::string_view m_source;
  Common::DiagnosticBag &m_bag;
  size_t m_pos{0};

  [[nodiscard]] const Token &peek(size_t offset = 0) const noexcept;
  [[nodiscard]] const Token &previous() const noexcept;
  [[nodiscard]] bool atEnd() const noexcept;
  [[nodiscard]] bool check(TokenType type,
                           std::string_view value = "") const noexcept;

  const Token &advance();
  bool match(TokenType type, std::string_view value = "");
  const Token &expect(TokenType type, std::string_view value, std::string msg,
                      std::optional<Common::Error> code);

  void emitError(std::string msg, std::optional<std::string> hint,
                 Common::Span span,
                 std::optional<Common::Error> code = std::nullopt);
  void synchronize();

  [[nodiscard]] ExprPtr parseExpr();
  [[nodiscard]] ExprPtr parseLet();
  [[nodiscard]] ExprPtr parseIf();
  [[nodiscard]] ExprPtr parseBinary(int minPrec = 0);
  [[nodiscard]] ExprPtr parseUnary();
  [[nodiscard]] ExprPtr parseCall();
  [[nodiscard]] ExprPtr parsePrimary();
  [[nodiscard]] ExprPtr parsePackage();
  [[nodiscard]] ExprPtr parseImport();
  [[nodiscard]] Block parseBlock();

  struct OpInfo {
    int prec;
    bool rightAssoc;
  };

  [[nodiscard]] static std::optional<OpInfo>
  infixOpInfo(std::string_view value) noexcept;
  [[nodiscard]] static BinaryOp tokenToBinaryOp(std::string_view value);
};
} // namespace Iris::Frontend