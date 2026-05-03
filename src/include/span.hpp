#pragma once

#include <cstdint>

struct Span {
  uint32_t startLine, startCol;
  uint32_t endLine, endCol;

  uint32_t width() const noexcept {
    return startLine == endLine ? endCol - startCol : 1;
  }

  bool isMultiLine() const noexcept { return startLine != endLine; }

  static Span merge(const Span &a, const Span &b) noexcept {
    Span s;
    if (a.startLine < b.startLine ||
        (a.startLine == b.startLine && a.startCol <= b.startCol)) {
      s.startLine = a.startLine;
      s.startCol = a.startCol;
    } else {
      s.startLine = b.startLine;
      s.startCol = b.startCol;
    }
    if (a.endLine > b.endLine ||
        (a.endLine == b.endLine && a.endCol >= b.endCol)) {
      s.endLine = a.endLine;
      s.endCol = a.endCol;
    } else {
      s.endLine = b.endLine;
      s.endCol = b.endCol;
    }
    return s;
  }
};
