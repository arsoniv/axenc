#include <memory>
#include <utility>
#include <vector>

#include "lexer.hpp"
#include "nodes/type.hpp"
#include "parser.hpp"

namespace axen::parser {

/// consumes a type (including type mods) and returns the resulting TypeNode. Returns nullptr if no type exists.
std::shared_ptr<ast::TypeNode> Parser::parseType() {
  int ptrs = 0;

  while (lexer_->peekT(lexer::TokenType::Ptr)) {
    ptrs++;
    lexer_->consume();
  }

  std::shared_ptr<ast::TypeNode> newType = getTypeNode(lexer_->peek().src);

  if (newType) {
    lexer_->consume();
    // check for function pointer type
    if (lexer_->peekT(lexer::TokenType::LParen) && ptrs > 0) {
      expect(lexer::TokenType::LParen);

      auto params = std::vector<std::shared_ptr<ast::TypeNode>>();
      while (!lexer_->peekT(lexer::TokenType::RParen)) {

        params.emplace_back(parseType());

        if (lexer_->peekT(lexer::TokenType::Comma)) {
          lexer_->consume();
        }
      }

      expect(lexer::TokenType::RParen);
      newType = std::make_shared<ast::FunctionTypeNode>(std::move(newType), std::move(params));
    }

    // because 0 arraylen means not an array
    int arrayLen = 0;

    // parse array mod
    if (lexer_->peekT(lexer::TokenType::LBracket)) {
      lexer_->consume();
      std::string intStr = expect(lexer::TokenType::IntLit).src;

      // both base 10 and 16 can be used
      int base = (intStr.size() > 2 && intStr[0] == '0' && (intStr[1] == 'x' || intStr[1] == 'X')) ? 16 : 10;
      arrayLen = std::stoi(intStr, nullptr, base);

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
  int ptrs = 0;

  while (lexer_->peekT(lexer::TokenType::Ptr, i)) {
    ptrs++;
    i++;
  }

  if (lexer_->peekT(lexer::TokenType::Identifier, i)) {
    i++;
  }

  // check for function pointer type
  if (lexer_->peekT(lexer::TokenType::LParen, i) && ptrs > 0) {
    i++; // LParen

    while (!lexer_->peekT(lexer::TokenType::RParen, i)) {
      // skip pointer modifiers
      while (lexer_->peekT(lexer::TokenType::Ptr, i)) {
        i++;
      }

      // skip identifier
      if (lexer_->peekT(lexer::TokenType::Identifier, i)) {
        i++;
      }

      // skip array modifier if present
      if (lexer_->peekT(lexer::TokenType::LBracket, i)) {
        i++;
        if (lexer_->peekT(lexer::TokenType::IntLit, i)) {
          i++;
        }
        if (lexer_->peekT(lexer::TokenType::RBracket, i)) {
          i++;
        }
      }

      if (lexer_->peekT(lexer::TokenType::Comma, i)) {
        i++;
      }
    }

    if (lexer_->peekT(lexer::TokenType::RParen, i)) {
      i++;
    }
  }

  // parse array mod
  if (lexer_->peekT(lexer::TokenType::LBracket, i)) {
    i++;

    if (lexer_->peekT(lexer::TokenType::IntLit, i)) {
      i++;
    }

    if (lexer_->peekT(lexer::TokenType::RBracket, i)) {
      i++;
    }
  }
  return i;
}

} // namespace axen::parser
