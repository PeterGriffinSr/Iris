#include "src/include/cli.hpp"
#include "src/include/error.hpp"
#include "src/include/lexer.hpp"
#include "src/include/printer.hpp"
#include "version.hpp"
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace {

void printUsage() {
  std::cout << "Usage: iris <file>              Run an Iris source file\n"
               "       iris --explain <code>    Show documentation for an "
               "error code\n"
               "       iris --version           Print the compiler version\n"
               "       iris --help              Show this message\n";
}

void printVersion() { std::cout << "iris " << version << '\n'; }

int runExplain(std::string_view code) {
  if (!code.empty() && (code[0] == 'E' || code[0] == 'e'))
    code.remove_prefix(1);

  if (code.empty() ||
      code.find_first_not_of("0123456789") != std::string_view::npos) {
    std::cerr << "error: invalid error code '" << code << "'\n"
              << "Expected a number or E-prefixed code, e.g. E0003 or 3\n";
    return 1;
  }

  return explainError(static_cast<uint32_t>(std::stoul(std::string(code)))) ? 0
                                                                            : 1;
}

int runFile(std::string_view path) {
  std::ifstream file{std::string(path)};
  if (!file) {
    emitFatal(Diagnostic{
        .severity = Severity::Error,
        .code = IOError::FileNotFound,
        .hint = "check the path is correct and the file exists",
        .message = "could not open '" + std::string(path) + "'",
        .filename = std::string(path),
        .line = 0,
        .col = 0,
    });
  }

  std::ostringstream buf;
  buf << file.rdbuf();

  Lexer lexer(buf.str(), path);
  printTokens(lexer.tokenize());

  return 0;
}

struct Flag {
  std::string_view description;
  int argCount;
  std::function<int(std::string_view)> handler;
};

const std::unordered_map<std::string_view, Flag> k_flags = {
    {"--help",
     {"Show this message", 0,
      [](auto) {
        printUsage();
        return 0;
      }}},
    {"--version",
     {"Print the compiler version", 0,
      [](auto) {
        printVersion();
        return 0;
      }}},
    {"--explain",
     {"Show docs for an error code", 1,
      [](auto arg) { return runExplain(arg); }}},
};

} // namespace

int runCli(int argc, char *argv[]) {
  if (argc < 2) {
    printUsage();
    return 1;
  }

  std::string_view arg = argv[1];

  if (auto it = k_flags.find(arg); it != k_flags.end()) {
    const Flag &flag = it->second;
    if (flag.argCount > 0 && argc < 3) {
      std::cerr << "error: " << arg << " requires an argument\n"
                << "Example: iris " << arg << " E0003\n";
      return 1;
    }
    return flag.handler(flag.argCount > 0 ? argv[2] : "");
  }

  if (arg.starts_with('-')) {
    std::cerr << "error: unknown option '" << arg << "'\n";
    printUsage();
    return 1;
  }

  return runFile(arg);
}