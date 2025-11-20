#include <memory>
#include <string>
#include <unistd.h>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <utility>

#include "lexer.hpp"
#include "nodes/class.hpp"
#include "parser.hpp"

namespace axen::parser {

void Parser::parseClass() {

  auto savedState = lexer_->saveState();

  std::map<std::string, std::shared_ptr<ast::TypeNode>> members;

  // first pass, parse member variables
  while (lexer_->peek().type != lexer::TokenType::EndOfFile && lexer_->peek().type != lexer::TokenType::RBrace) {

    auto type = parseType();
    auto token = expect(lexer::TokenType::Identifier);
    validateIdentifier(token.src);

    if (!lexer_->peekT(lexer::TokenType::LParen)) {
      expect(lexer::TokenType::Semi);

      members[token.src] = type;
      continue;
    }

    // skip functions
    // type and identifier have already been consumed

    expect(lexer::TokenType::LParen);
    while (lexer_->peek().type != lexer::TokenType::RParen) {
      if (lexer_->peek().type != lexer::TokenType::RParen && lexer_->peek().type != lexer::TokenType::Comma) {
        parseType();
        auto token = expect(lexer::TokenType::Identifier);
        validateIdentifier(token.src);
      }
      if (lexer_->peekT(lexer::TokenType::Comma)) {
        lexer_->consume();
      }
    }
    expect(lexer::TokenType::RParen);

    if (lexer_->peekT(lexer::TokenType::LBrace)) {
      lexer_->consume();
      int braceDepth = 1;
      while (braceDepth > 0 && lexer_->peek().type != lexer::TokenType::EndOfFile) {
        if (lexer_->peek().type == lexer::TokenType::LBrace) {
          braceDepth++;
        } else if (lexer_->peek().type == lexer::TokenType::RBrace) {
          braceDepth--;
        }
        lexer_->consume();
      }
    } else {
      expect(lexer::TokenType::Semi);
    }
  }

  // create struct for data members
  if (!currentClassName_.empty() && !members.empty()) {
    // check if class already exists
    auto existingType = getTypeNode(currentClassName_);
    if (existingType) {
      // class exists, add members to it
      auto classRef = std::dynamic_pointer_cast<ast::ClassReferenceNode>(existingType);
      if (classRef) {
        classRef->getDecl()->addMembers(members);
      }
    } else {
      // class doesn't exist, create it
      auto classNode = std::make_shared<ast::ClassNode>(currentClassName_, std::move(members));
      classes_.push_back(classNode);
      registerStructType(currentClassName_, classNode);
    }
  }

  // second pass to parse functions.
  lexer_->restoreState(savedState);
  parseFunctions();
}
} // namespace axen::parser
