#include "src/include/error.hpp"
#include "error_docs.hpp"
#include <iomanip>
#include <iostream>

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

std::string plural(uint32_t n, std::string_view word) {
  return std::to_string(n) + ' ' + std::string(word) + (n == 1 ? "" : "s");
}

} // namespace

void DiagnosticBag::emit(const Diagnostic &diag) {
  printDiagnostic(diag);

  const bool countsAsError =
      diag.severity == Severity::Error ||
      (diag.severity == Severity::Warning && m_opts.werror);

  if (countsAsError)
    ++m_errors;
  else if (diag.severity == Severity::Warning)
    ++m_warnings;

  if (m_opts.maxErrors > 0 && m_errors >= m_opts.maxErrors) {
    emitFatal(Diagnostic{
        .severity = Severity::Error,
        .code = std::nullopt,
        .hint = std::string("use --max-errors to raise the limit, "
                            "or 0 to disable it entirely"),
        .message = "too many errors (" + std::to_string(m_errors) +
                   "); stopping compilation",
        .filename = diag.filename,
        .sourceLine = {},
        .line = 0,
        .col = 0,
    });
  }
}

void DiagnosticBag::printSummary() const {
  if (m_errors == 0 && m_warnings == 0)
    return;

  std::cerr << '\n';

  if (m_errors > 0 && m_warnings > 0)
    std::cerr << plural(m_errors, "error") << ", "
              << plural(m_warnings, "warning") << " generated.\n";
  else if (m_errors > 0)
    std::cerr << plural(m_errors, "error") << " generated.\n";
  else
    std::cerr << plural(m_warnings, "warning") << " generated.\n";
}

bool DiagnosticBag::hasErrors() const noexcept { return m_errors > 0; }

bool DiagnosticBag::limitReached() const noexcept {
  return m_opts.maxErrors > 0 && m_errors >= m_opts.maxErrors;
}

[[noreturn]] void emitFatal(const Diagnostic &diag) {
  printDiagnostic(diag);
  std::exit(1);
}

void emitDirect(const Diagnostic &diag) { printDiagnostic(diag); }

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