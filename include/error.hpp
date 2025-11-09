#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

namespace axen::error {

enum class ErrorType {
  Syntax,
  Semantic,
  Codegen,
  Internal,
};

struct SourceLocation {
  std::string fileName;
  std::string className;
  int row = 0;
  int col = 0;
  std::string tokenText;

  SourceLocation() = default;
  SourceLocation(const std::string &file, const std::string &cls, int r, int c, const std::string &token)
      : fileName(file), className(cls), row(r), col(c), tokenText(token) {}
};

inline void reportError(ErrorType type, const std::string &message, const SourceLocation *loc = nullptr) {
  const char *typeStr = "";
  switch (type) {
  case ErrorType::Syntax:
    typeStr = "Syntax Error";
    break;
  case ErrorType::Semantic:
    typeStr = "Semantic Error";
    break;
  case ErrorType::Codegen:
    typeStr = "Code Generation Error";
    break;
  case ErrorType::Internal:
    typeStr = "Internal Compiler Error";
    break;
  }

  fprintf(stderr, "%s: %s\n", typeStr, message.c_str());

  if (loc) {
    if (loc->row > 0 && loc->col > 0) {
      fprintf(stderr, "  at line %d, column %d", loc->row, loc->col);
      if (!loc->tokenText.empty()) {
        fprintf(stderr, " (token: '%s')", loc->tokenText.c_str());
      }
      fprintf(stderr, "\n");
    }
    if (!loc->className.empty()) {
      fprintf(stderr, "  in class '%s'\n", loc->className.c_str());
    }
    if (!loc->fileName.empty()) {
      fprintf(stderr, "  in file '%s'\n", loc->fileName.c_str());
    }
  }

  exit(EXIT_FAILURE);
}

} // namespace axen::error
