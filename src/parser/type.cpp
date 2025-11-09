#include <cstdlib>
#include <memory>

#include "lexer.hpp"
#include "nodes/types.hpp"
#include "parser.hpp"

namespace axen::parser {

/// Consumes a type (including type mods) and returns the resulting TypeNode. Returns nullptr if no type exists.
std::shared_ptr<ast::TypeNode> Parser::parseType() {
  int ptrs = 0;

  while (lexer_->peekT(lexer::TokenType::Ptr)) {
    ptrs++;
    lexer_->consume();
  }

  std::shared_ptr<ast::TypeNode> newType = getTypeNode(lexer_->peek().src);

  if (newType) {
    lexer_->consume();

    // because 0 arraylen means not an array
    int arrayLen = 0;

    // parse array mod
    if (lexer_->peekT(lexer::TokenType::LBracket)) {
      lexer_->consume();
      arrayLen = atoi(expect(lexer::TokenType::IntLit).src.data());
      expect(lexer::TokenType::RBracket);
    }

    for (int i = 0; i < ptrs; i++) {
      newType = std::make_shared<ast::PointerTypeNode>(newType);
    }

    if (arrayLen) {
      newType = std::make_shared<ast::ArrayTypeNode>(newType, arrayLen);
    }

    return newType;
  } else {
    return nullptr;
  }
}

/// peeks tokens to find the length of the next type, no tokens are consumed
int Parser::getNextTypeLength() {

  int i = 0;

  while (lexer_->peekT(lexer::TokenType::Ptr, i))
    i++;

  if (lexer_->peekT(lexer::TokenType::Identifier, i))
    i++;

  // parse array mod
  if (lexer_->peekT(lexer::TokenType::LBracket, i)) {
    i++;

    if (lexer_->peekT(lexer::TokenType::IntLit, i))
      i++;

    if (lexer_->peekT(lexer::TokenType::RBracket, i))
      i++;
  }
  return i;
}

} // namespace axen::parser
