#include <memory>
#include <string>
#include <utility>

#include "nodes/function.hpp"
#include "nodes/statement.hpp"
#include "nodes/types.hpp"

#include "lexer.hpp"
#include "parser.hpp"

namespace axen::parser {

std::unique_ptr<ast::FunctionNode> Parser::parseFunction() {

  bool isDetached = currentClassName_.empty();

  // type (along with all type modifiers)
  std::shared_ptr<ast::TypeNode> type = parseType();

  // name
  auto nameToken = expect(lexer::TokenType::Identifier);
  validateIdentifier(nameToken.src);
  std::string baseName = nameToken.src;
  std::string name;
  if (isDetached) {
    name = baseName;
  } else if (!currentClassName_.empty()) {
    name = currentClassName_ + "_" + baseName;
  } else {
    // NOTE: if no class exists it will be treated at a detached function.
    name = baseName;
  }

  // left paren for params
  expect(lexer::TokenType::LParen);

  auto params = std::vector<std::pair<std::string, std::shared_ptr<ast::TypeNode>>>();

  // add 'this' parameter for non-detached member functions
  if (!isDetached && !currentClassName_.empty()) {
    auto thisType = getTypeNode(currentClassName_);
    if (thisType) {
      auto thisPtrType = std::make_shared<ast::PointerTypeNode>(thisType);
      params.emplace_back("this", thisPtrType);
    }
  }

  while (lexer_->peek().type != lexer::TokenType::RParen) {
    // type (along with all type modifiers)
    auto paramType = parseType();

    // name is consumed
    auto token = expect(lexer::TokenType::Identifier);
    validateIdentifier(token.src);

    params.emplace_back(std::string(token.src), std::move(paramType));

    if (lexer_->peekT(lexer::TokenType::Comma))
      lexer_->consume();
  }

  // closing paren for params
  expect(lexer::TokenType::RParen);

  std::optional<std::vector<std::unique_ptr<ast::StatementNode>>> body;

  // NOTE: function may be bodyless, only parse body if it exists
  if (lexer_->consume().type == lexer::TokenType::LBrace) {
    body = std::vector<std::unique_ptr<ast::StatementNode>>();

    Parser::pushScope();

    // index function parameters into scope
    for (const auto &param : params) {
      Parser::indexVariableType(param.first, const_cast<std::shared_ptr<ast::TypeNode> &>(param.second));
    }

    while (!lexer_->peekT(lexer::TokenType::RBrace)) {
      // NOTE: we don't need to keep track of braces because braces are only used for function scopes
      body->emplace_back(parseStatement());
    }
    expect(lexer::TokenType::RBrace);

    Parser::popScope();
  }

  return std::make_unique<ast::FunctionNode>(name, std::move(type), true, std::move(params), std::move(body),
                                             isDetached);
}
} // namespace axen::parser
