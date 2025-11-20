#pragma once

#include <exception>
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

class CompilerException : public std::exception {
public:
  CompilerException(ErrorType type, const std::string &message, const SourceLocation &loc)
      : type_(type), message_(message), location_(loc) {
    buildFullMessage();
  }

  CompilerException(ErrorType type, const std::string &message) : type_(type), message_(message), location_() {
    buildFullMessage();
  }

  const char *what() const noexcept override { return fullMessage_.c_str(); }

  ErrorType getType() const { return type_; }
  const std::string &getMessage() const { return message_; }
  const SourceLocation &getLocation() const { return location_; }
  int getRow() const { return location_.row; }
  int getCol() const { return location_.col; }

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
  SourceLocation location_;
  std::string fullMessage_;
};

[[noreturn]] inline void reportError(ErrorType type, const std::string &message, const SourceLocation *loc = nullptr) {
  if (loc) {
    throw CompilerException(type, message, *loc);
  } else {
    throw CompilerException(type, message);
  }
}

} // namespace axen::error
