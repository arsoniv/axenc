#pragma once

#include <deque>
#include <string>
#include <unordered_map>

namespace axen::lexer {
enum class TokenType {
  // literals
  IntLit,
  FloatLit,
  StringLit,

  // keywords
  Return,
  Break,
  Continue,
  If,
  While,
  Else,
  Ptr,

  Import,
  Class,
  Typedef,

  // symbols
  LParen,
  RParen,
  LBrace,
  RBrace,
  LBracket,
  RBracket,
  Period,
  Comma,
  Semi,
  Ampersand,
  Dollar,
  Percent,
  Plus,
  Minus,
  Asterisk,
  Slash,
  Equals,
  Less,
  Greater,

  // misc
  Identifier,
  EndOfFile,
};

inline const std::unordered_map<std::string, TokenType> keywordMap = {
    {"return", TokenType::Return},   {"break", TokenType::Break},   {"continue", TokenType::Continue},
    {"if", TokenType::If},           {"else", TokenType::Else},     {"while", TokenType::While},
    {"ptr", TokenType::Ptr},         {"import", TokenType::Import}, {"class", TokenType::Class},
    {"typedef", TokenType::Typedef},
};

inline const std::unordered_map<char, TokenType> symbolMap = {
    {'(', TokenType::LParen},    {')', TokenType::RParen},   {'{', TokenType::LBrace},   {'}', TokenType::RBrace},
    {'[', TokenType::LBracket},  {']', TokenType::RBracket}, {'.', TokenType::Period},   {',', TokenType::Comma},
    {'+', TokenType::Plus},      {'-', TokenType::Minus},    {'*', TokenType::Asterisk}, {'/', TokenType::Slash},
    {'=', TokenType::Equals},    {'<', TokenType::Less},     {'>', TokenType::Greater},  {';', TokenType::Semi},
    {'&', TokenType::Ampersand}, {'$', TokenType::Dollar},   {'%', TokenType::Percent},
};

inline const std::unordered_map<TokenType, std::string> tokenToKeyword = [] {
  std::unordered_map<TokenType, std::string> rev;
  for (const auto &[key, val] : keywordMap)
    rev[val] = key;
  return rev;
}();

inline const std::unordered_map<TokenType, char> tokenToSymbol = [] {
  std::unordered_map<TokenType, char> rev;
  for (const auto &[key, val] : symbolMap)
    rev[val] = key;
  return rev;
}();

struct Token {
  TokenType type;
  std::string src;
  int row;
  int col;
};

class Lexer {
public:
  explicit Lexer(std::string src);

  [[nodiscard]] Token peek(unsigned int offset = 0);
  [[nodiscard]] bool peekT(TokenType t, unsigned int offset = 0);
  Token consume();

  struct LexerState {
    size_t srcCursor;
    std::deque<Token> lookAhead;
    size_t tokensCursor;
    int row;
    int col;
  };

  LexerState saveState() const { return {srcCursor_, lookAhead_, tokensCursor_, row_, col_}; }

  void restoreState(const LexerState &state) {
    srcCursor_ = state.srcCursor;
    lookAhead_ = state.lookAhead;
    tokensCursor_ = state.tokensCursor;
    row_ = state.row;
    col_ = state.col;
  }

private:
  Token nextToken();

  [[nodiscard]] char peekChar(unsigned int offset = 0) const;
  char consumeChar();

  const std::string src_;
  size_t srcCursor_ = 0;

  std::deque<Token> lookAhead_;
  size_t tokensCursor_ = 0;

  int row_ = 1;
  int col_ = 1;
};
} // namespace axen::lexer
