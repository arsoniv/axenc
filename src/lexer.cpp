#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <utility>

#include "lexer.hpp"

namespace axen::lexer {

Lexer::Lexer(std::string src) : src_(std::move(src)) {}

Token Lexer::peek(unsigned int offset) {
  while (lookAhead_.size() <= offset) {
    lookAhead_.push_back(nextToken());
  }
  return lookAhead_[offset];
}

bool Lexer::peekT(TokenType t, unsigned int offset) { return peek(offset).type == t; }

Token Lexer::consume() {
  if (lookAhead_.empty()) {
    lookAhead_.push_back(nextToken());
  }
  Token tok = lookAhead_.front();
  lookAhead_.pop_front();
  return tok;
}

Token Lexer::nextToken() {
  while (srcCursor_ < src_.size()) {
    if (peekChar() == '\n' || std::isspace(peekChar())) {
      consumeChar();
      continue;
    }

    if (peekChar() == '/' && srcCursor_ + 1 < src_.size()) {
      if (peekChar(1) == '/') {
        consumeChar();
        consumeChar();
        while (srcCursor_ < src_.size() && peekChar() != '\n') {
          consumeChar();
        }
        continue;
      } else if (peekChar(1) == '*') {
        consumeChar();
        consumeChar();
        while (srcCursor_ + 1 < src_.size()) {
          if (peekChar() == '*' && peekChar(1) == '/') {
            consumeChar();
            consumeChar();
            break;
          }
          consumeChar();
        }
        continue;
      }
    }

    Token newToken = {.type = TokenType::EndOfFile, .src = src_.substr(srcCursor_, 1), .row = row_, .col = col_};

    char c = peekChar();
    auto it = symbolMap.find(c);
    if (it != symbolMap.end())
      newToken.type = it->second;

    if (newToken.type != TokenType::EndOfFile) {
      consumeChar(); // consume the single char token
      return newToken;
    }

    if (std::isdigit(peekChar())) {
      std::string newIntLiteral;
      while (srcCursor_ < src_.length() && std::isdigit(peekChar())) {
        newIntLiteral += consumeChar(); // int literal digits are consumed
      }

      newToken.type = TokenType::IntLit;
      if (peekChar() == '.') {
        newIntLiteral += consumeChar();
        while (srcCursor_ < src_.length() && std::isdigit(peekChar())) {
          newIntLiteral += consumeChar(); // int literal digits are consumed
          newToken.type = TokenType::FloatLit;
        }
      }
      newToken.src = std::move(newIntLiteral);
      return newToken;
    }

    if (peekChar() == '"') {
      consumeChar(); // consume opening quote
      std::string stringContent;
      while (srcCursor_ < src_.length() && peekChar() != '"') {
        if (peekChar() == '\\' && srcCursor_ + 1 < src_.length()) {
          consumeChar(); // consume backslash
          char escaped = consumeChar();
          switch (escaped) {
          case 'n':
            stringContent += '\n';
            break;
          case 't':
            stringContent += '\t';
            break;
          case '"':
            stringContent += '"';
            break;
          case '\\':
            stringContent += '\\';
            break;
          default:
            stringContent += escaped;
            break;
          }
        } else {
          stringContent += consumeChar();
        }
      }
      if (srcCursor_ >= src_.length()) {
        fprintf(stderr, "Unterminated string literal.\n");
        exit(EXIT_FAILURE);
      }
      consumeChar(); // consume closing quote
      newToken.type = TokenType::StringLit;
      newToken.src = std::move(stringContent);
      return newToken;
    }

    if (std::isalpha(peekChar()) || peekChar() == '_') {
      // can be a keyword or type
      std::string newKeyword;
      while (srcCursor_ < src_.length() && (std::isalnum(peekChar()) || peekChar() == '_')) {
        newKeyword += consumeChar(); // keyword chars are consumed
      }

      auto it2 = keywordMap.find(newKeyword);
      if (it2 != keywordMap.end())
        newToken.type = it2->second;
      else
        newToken.type = TokenType::Identifier;

      newToken.src = std::move(newKeyword); // newKeyword is not needed anymore
      return newToken;
    }
    fprintf(stderr, "Invalid character found during lexing: '%c'.\n", peekChar());
    exit(EXIT_FAILURE);
  }
  return {
      .type = TokenType::EndOfFile,
      .src = src_.substr(srcCursor_),
  };
}

char Lexer::peekChar(unsigned int offset) const { return src_.at(srcCursor_ + offset); }

char Lexer::consumeChar() {
  if (peekChar() == '\n') {
    row_++;
    col_ = 1;
  } else {
    col_++;
  }
  return src_.at(srcCursor_++);
}
} // namespace axen::lexer
