#include <Iris/Common/error.hpp>
#include <error_docs.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Iris::Common {

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

  const Span &sp = d.span;
  if (sp.startLine == 0)
    goto hints;

  std::cerr << " --> " << d.filename << ':' << sp.startLine << ':'
            << sp.startCol << '\n';

  if (!d.sourceLine.empty()) {
    std::string lineStr = std::to_string(sp.startLine);
    size_t gutter = lineStr.size();
    std::string gutterPad(gutter + 1, ' ');

    std::cerr << gutterPad << "|\n";
    std::cerr << lineStr << " | " << d.sourceLine << '\n';
    std::cerr << gutterPad << "| ";

    uint32_t offset = sp.startCol > 0 ? sp.startCol - 1 : 0;
    std::cerr << std::string(offset, ' ');

    uint32_t underlineWidth = sp.isMultiLine() ? 1 : std::max(sp.width(), 1u);
    std::cerr << std::string(underlineWidth, '^') << '\n';
  }

  if ((d.hint || d.code) && sp.startLine > 0) {
    std::string lineStr = std::to_string(sp.startLine);
    std::cerr << std::string(lineStr.size() + 1, ' ') << "|\n";
  }

hints:
  if (d.hint)
    std::cerr << "= hint: " << *d.hint << '\n';

  if (d.code)
    std::cerr << "= note: run `iris --explain " << formatCode(*d.code)
              << "` for more detail\n";
}

std::string plural(uint32_t n, std::string_view word) {
  return std::to_string(n) + ' ' + std::string(word) + (n == 1 ? "" : "s");
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
        .span = {0, 0, 0, 0},
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
} // namespace Iris::Common