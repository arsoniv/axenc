#pragma once

#include <exception>
#include <string>

namespace axen::error {

struct SourceSpan {
  std::string file;
  int startRow;
  int startCol;
  int endRow;
  int endCol;
};

enum class ErrorType {
  Syntax,
  Semantic,
  Codegen,
  Internal,
};

class CompilerException : public std::exception {
public:
  CompilerException(ErrorType type, const std::string &message, const SourceSpan &loc)
      : type_(type), message_(message), location_(loc) {
    buildFullMessage();
  }

  CompilerException(ErrorType type, const std::string &message) : type_(type), message_(message), location_() {
    buildFullMessage();
  }

  const char *what() const noexcept override { return fullMessage_.c_str(); }

  ErrorType getType() const { return type_; }
  const std::string &getMessage() const { return message_; }
  const SourceSpan &getLocation() const { return location_; }

private:
  void buildFullMessage() {
    const char *typeStr = "";
    switch (type_) {
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

    fullMessage_ = std::string(typeStr) + ": " + message_;
  }

  ErrorType type_;
  std::string message_;
  SourceSpan location_;
  std::string fullMessage_;
};

[[noreturn]] inline void reportError(ErrorType type, const std::string &message, const SourceSpan &loc) {
  throw CompilerException(type, message, loc);
}

} // namespace axen::error
