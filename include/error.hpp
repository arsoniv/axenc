#pragma once

#include <string>

namespace axen::error {

struct SourceSpan {
  std::string file;
  int startRow;
  int startCol;
  int endRow;
  int endCol;
};

struct ErrorInfo {
  std::string message;
  SourceSpan span;
};

} // namespace axen::error
