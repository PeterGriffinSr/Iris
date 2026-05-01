#include "src/include/error.hpp"
#include "error_docs.hpp"
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string_view severityLabel(Severity s) noexcept {
  switch (s) {
  case Severity::Note:
    return "note";
  case Severity::Warning:
    return "warning";
  case Severity::Error:
    return "error";
  }
  return "error";
}

std::string formatCode(const Error &code) {
  uint32_t value =
      std::visit([](auto e) { return static_cast<uint32_t>(e); }, code);
  std::ostringstream ss;
  ss << 'E' << std::setfill('0') << std::setw(4) << value;
  return ss.str();
}

void printDiagnostic(const Diagnostic &d) {
  std::cerr << severityLabel(d.severity);
  if (d.code)
    std::cerr << '[' << formatCode(*d.code) << ']';
  std::cerr << ": " << d.message << '\n';

  if (d.line > 0)
    std::cerr << " --> " << d.filename << ':' << d.line << ':' << d.col << '\n';

  if (!d.sourceLine.empty()) {
    std::string lineStr = std::to_string(d.line);
    size_t gutter = lineStr.size();

    std::cerr << std::string(gutter + 1, ' ') << "|\n";
    std::cerr << lineStr << " | " << d.sourceLine << '\n';
    std::cerr << std::string(gutter + 1, ' ') << "| "
              << std::string(d.col > 0 ? d.col - 1 : 0, ' ') << "^\n";
  }

  if ((d.hint || d.code) && d.line > 0) {
    std::string lineStr = std::to_string(d.line);
    std::cerr << std::string(lineStr.size() + 1, ' ') << "|\n";
  }

  if (d.hint)
    std::cerr << "= hint: " << *d.hint << '\n';

  if (d.code)
    std::cerr << "= note: run `iris --explain " << formatCode(*d.code)
              << "` for more detail\n";
}

#ifndef _WIN32
bool commandExists(const char *cmd) {
  std::string check = std::string("command -v ") + cmd + " > /dev/null 2>&1";
  return std::system(check.c_str()) == 0;
}
#endif

FILE *openPager() {
#ifdef _WIN32
  return popen("powershell -Command \"$input | Out-Host -Paging\"", "w");
#else
  const char *userPager = std::getenv("PAGER");
  if (userPager && userPager[0] != '\0')
    return popen(userPager, "w");
  if (commandExists("glow"))
    return popen("glow - | less -R", "w");
  return popen("less -R", "w");
#endif
}

} // namespace

void emit(const Diagnostic &diag) { printDiagnostic(diag); }

[[noreturn]] void emitFatal(const Diagnostic &diag) {
  printDiagnostic(diag);
  std::exit(1);
}

bool explainError(uint32_t code) {
  std::string_view text = lookupErrorDoc(code);
  if (text.empty()) {
    std::cerr << "No documentation found for E" << std::setfill('0')
              << std::setw(4) << code << '\n';
    return false;
  }

  FILE *pager = openPager();
  if (!pager) {
    std::cout << text << '\n';
    return true;
  }

  std::fwrite(text.data(), 1, text.size(), pager);
  pclose(pager);
  return true;
}