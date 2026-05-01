#include "src/include/printer.hpp"
#include <iomanip>
#include <iostream>

std::string_view tokenTypeName(TokenType type) noexcept {
  switch (type) {
  case TokenType::Keyword:
    return "Keyword";
  case TokenType::Number:
    return "Number";
  case TokenType::String:
    return "String";
  case TokenType::Operator:
    return "Operator";
  case TokenType::Delimiter:
    return "Delimiter";
  case TokenType::Identifier:
    return "Identifier";
  case TokenType::Semicolon:
    return "Semicolon";
  case TokenType::Eof:
    return "Eof";
  case TokenType::Error:
    return "Error";
  }
  return "Unknown";
}

void printToken(const Token &tok) {
  std::cout << std::right << std::setw(3) << tok.line << ':' << std::left
            << std::setw(4) << tok.column << std::setw(12)
            << tokenTypeName(tok.type)
            << (tok.type == TokenType::Semicolon ? "<;>" : tok.value) << '\n';
}

void printTokens(std::span<const Token> tokens) {
  std::cout << std::left << std::setw(8) << "Ln:Col" << std::setw(12) << "Type"
            << "Value\n"
            << std::string(40, '-') << '\n';

  for (const Token &tok : tokens)
    printToken(tok);
}