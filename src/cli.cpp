#include "src/include/cli.hpp"
#include "src/include/lexer.hpp"
#include "src/include/printer.hpp"
#include "version.hpp"
#include <charconv>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

namespace {

void printUsage() {
  std::cout << "Usage: iris <file>              Run an Iris source file\n"
               "       iris --explain <code>    Show documentation for an "
               "error code\n"
               "       iris --version           Print the compiler version\n"
               "       iris --help              Show this message\n"
               "\n"
               "Options:\n"
               "  --werror                      Treat all warnings as errors\n"
               "  --max-errors <n>              Stop after n errors (default: "
               "20, 0 = unlimited)\n";
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

int runFile(std::string_view path, const CompilerOptions &opts) {
  std::ifstream file{std::string(path)};
  if (!file) {
    emitFatal(Diagnostic{
        .severity = Severity::Error,
        .code = IOError::FileNotFound,
        .hint = std::string("check the path is correct and the file exists"),
        .message = "could not open '" + std::string(path) + "'",
        .filename = std::string(path),
        .sourceLine = {},
        .span = {0, 0, 0, 0},
    });
  }

  std::ostringstream buf;
  buf << file.rdbuf();
  std::string source = buf.str();

  DiagnosticBag bag(opts);

  Lexer lexer(source, path, bag);
  auto tokens = lexer.tokenize();

  bag.printSummary();

  if (bag.hasErrors())
    return 1;

  printTokens(tokens);
  return 0;
}

struct Flag {
  std::string_view description;
  int argCount;
  std::function<int(std::string_view, CompilerOptions &)> handler;
};

const std::unordered_map<std::string_view, Flag> k_flags = {
    {"--help",
     {"Show this message", 0,
      [](auto, auto &) {
        printUsage();
        return 0;
      }}},
    {"--version",
     {"Print the compiler version", 0,
      [](auto, auto &) {
        printVersion();
        return 0;
      }}},
    {"--explain",
     {"Show docs for an error code", 1,
      [](auto arg, auto &) { return runExplain(arg); }}},
    {"--werror",
     {"Treat all warnings as errors", 0,
      [](auto, CompilerOptions &opts) {
        opts.werror = true;
        return -1;
      }}},
    {"--max-errors",
     {"Stop after n errors (0 = unlimited)", 1,
      [](std::string_view arg, CompilerOptions &opts) {
        uint32_t n = 0;
        auto [ptr, ec] =
            std::from_chars(arg.data(), arg.data() + arg.size(), n);
        if (ec != std::errc{} || ptr != arg.data() + arg.size()) {
          std::cerr
              << "error: --max-errors expects a non-negative integer, got '"
              << arg << "'\n";
          return 1;
        }
        opts.maxErrors = n;
        return -1;
      }}},
};

} // namespace

int runCli(int argc, char *argv[]) {
  if (argc < 2) {
    printUsage();
    return 1;
  }

  CompilerOptions opts;
  std::string_view file;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];

    if (auto it = k_flags.find(arg); it != k_flags.end()) {
      const Flag &flag = it->second;

      if (flag.argCount > 0 && i + 1 >= argc) {
        std::cerr << "error: " << arg << " requires an argument\n"
                  << "Example: iris " << arg
                  << (arg == "--explain" ? " E0003" : " 10") << "\n";
        return 1;
      }

      std::string_view flagArg = flag.argCount > 0 ? argv[++i] : "";
      int result = flag.handler(flagArg, opts);
      if (result != -1)
        return result;
      continue;
    }

    if (arg.starts_with('-')) {
      std::cerr << "error: unknown option '" << arg << "'\n";
      printUsage();
      return 1;
    }

    if (!file.empty()) {
      std::cerr << "error: unexpected argument '" << arg << "'\n"
                << "Only one source file can be provided at a time\n";
      return 1;
    }
    file = arg;
  }

  if (file.empty()) {
    printUsage();
    return 1;
  }

  return runFile(file, opts);
}