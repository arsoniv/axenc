#include <memory>
#include <optional>
#include <utility>

#include "lexer.hpp"
#include "nodes/expression.hpp"
#include "nodes/statement.hpp"
#include "nodes/type.hpp"
#include "parser.hpp"

namespace axen::parser {
/// consumes a statement and returns the resulting StatementNode
std::unique_ptr<ast::StatementNode> Parser::parseStatement() {

  switch (lexer_->peek().type) {
  case lexer::TokenType::Return: {
    auto tok = lexer_->peek();
    lexer_->consume();
    auto span = makeSpan(tok);

    if (lexer_->peekT(lexer::TokenType::Semi)) {
      lexer_->consume();
      return std::make_unique<ast::Return>(nullptr, span);
    } else {
      std::unique_ptr<ast::ExpressionNode> returnValue = parseExpression(lexer::TokenType::Semi);
      expect(lexer::TokenType::Semi);
      return std::make_unique<ast::Return>(std::move(returnValue), span);
    }
  }
  case lexer::TokenType::If: {
    auto tok = lexer_->peek();
    lexer_->consume();
    auto span = makeSpan(tok);

    expect(lexer::TokenType::LParen);
    std::unique_ptr<ast::ExpressionNode> condition = parseExpression(lexer::TokenType::RParen);
    expect(lexer::TokenType::RParen);

    expect(lexer::TokenType::LBrace);

    std::vector<std::unique_ptr<ast::StatementNode>> trueBody;
    std::optional<std::vector<std::unique_ptr<ast::StatementNode>>> falseBody;

    // parse the scope
    // NOTE: we should not need to keep track of braces in becuase parseStatement should consume rbrace before the loop
    // checks it.
    while (!lexer_->peekT(lexer::TokenType::Else) && !lexer_->peekT(lexer::TokenType::RBrace)) {
      trueBody.emplace_back(parseStatement());
    }
    expect(lexer::TokenType::RBrace);
    if (lexer_->peekT(lexer::TokenType::Else)) {
      // consume the else
      lexer_->consume();

      expect(lexer::TokenType::LBrace);

      falseBody = std::vector<std::unique_ptr<ast::StatementNode>>();
      while (!lexer_->peekT(lexer::TokenType::RBrace)) {
        falseBody->emplace_back(parseStatement());
      }

      expect(lexer::TokenType::RBrace);
    }

    return std::make_unique<ast::If>(std::move(condition), std::move(trueBody), std::move(falseBody), span);
  }
  case lexer::TokenType::While: {
    auto tok = lexer_->peek();
    lexer_->consume();
    auto span = makeSpan(tok);

    expect(lexer::TokenType::LParen);
    std::unique_ptr<ast::ExpressionNode> condition = parseExpression(lexer::TokenType::RParen);
    expect(lexer::TokenType::RParen);

    expect(lexer::TokenType::LBrace);

    std::vector<std::unique_ptr<ast::StatementNode>> body;

    // parse the scope
    while (!lexer_->peekT(lexer::TokenType::RBrace)) {
      body.emplace_back(parseStatement());
    }

    expect(lexer::TokenType::RBrace);

    return std::make_unique<ast::While>(std::move(condition), std::move(body), span);
  }
  default:
    break;
  }

  // consumes the type (along with any type mods)
  std::shared_ptr<ast::TypeNode> type = parseType();

  if (type) {
    // should be a variable decleration with optional assignment
    auto nameToken = expect(lexer::TokenType::Identifier);
    validateIdentifier(nameToken.src);
    std::string name = nameToken.src;

    std::unique_ptr<ast::ExpressionNode> initialValue = nullptr;

    if (lexer_->peekT(lexer::TokenType::Equals)) {
      // should be variable declearation WITH assignment
      lexer_->consume(); // consume the equals sign
      initialValue = parseExpression(lexer::TokenType::Semi);
    }

    expect(lexer::TokenType::Semi);

    Parser::indexVariableType(name, type);

    return std::make_unique<ast::VariableDeclaration>(type, std::move(name), std::move(initialValue), makeSpan(nameToken));
  } else {

    // check if it's a detatched function call first
    if (lexer_->peekT(lexer::TokenType::Identifier) && lexer_->peekT(lexer::TokenType::LParen, 1)) {
      auto nameToken = expect(lexer::TokenType::Identifier);
      validateIdentifier(nameToken.src);
      std::string name = nameToken.src;
      expect(lexer::TokenType::LParen);

      auto functionArgs = std::vector<std::unique_ptr<ast::ExpressionNode>>();
      while (lexer_->peek().type != lexer::TokenType::RParen) {
        functionArgs.emplace_back(parseExpression(lexer::TokenType::Comma));
        if (lexer_->peek().type == lexer::TokenType::Comma) {
          lexer_->consume();
        }
      }
      lexer_->consume();

      expect(lexer::TokenType::Semi);

      auto functionReturnType = Parser::lookupFunctionReturnType(name);
      auto span = makeSpan(nameToken);

      auto funcRef = std::make_unique<ast::FunctionReference>(name, Parser::lookupFunctionType(name), span);

      // functionReturnType may be nullptr, catch in analysis
      auto call = std::make_unique<ast::FunctionCall>(std::move(funcRef), std::move(functionArgs), functionReturnType, span);
      return std::make_unique<ast::ExpressionStatement>(std::move(call), span);
    }

    // parse lvalue
    auto [target, derivedType] = parseValue();

    // check if this is a method call statement
    if (dynamic_cast<ast::FunctionCall *>(target.get())) {
      expect(lexer::TokenType::Semi);
      auto span = target->getSpan();
      return std::make_unique<ast::ExpressionStatement>(std::move(target), span);
    }

    auto eqToken = expect(lexer::TokenType::Equals);

    std::unique_ptr<ast::ExpressionNode> newValue = parseExpression(lexer::TokenType::Semi);

    expect(lexer::TokenType::Semi);

    return std::make_unique<ast::AssignmentStatement>(std::move(target), std::move(newValue), makeSpan(eqToken));
  }
}
} // namespace axen::parser
