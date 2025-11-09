#include <memory>
#include <optional>
#include <utility>

#include "lexer.hpp"
#include "nodes/expression.hpp"
#include "nodes/statement.hpp"
#include "nodes/types.hpp"
#include "parser.hpp"

namespace axen::parser {
/// Consumes a statement and returns the resulting StatementNode
std::unique_ptr<ast::StatementNode> Parser::parseStatement() {

  /*
   * Statement types:
   * variable decl [type] [name] [semi]
   * variable decl + assignment [type] [name] [equals] [EXPR] [semi]
   * variable assignment [name] [equals] [EXPR] [semi]
   * return [return] [EXPR] [semi]
   * while [while] [lparen] [EXPR] [rparen] [STMT ... ] [else]
   * if [while] [lparen] [EXPR] [rparen] [STMT ... ] [else] *[STMT ... ]
   * expression [EXPR] [semi]
   */

  switch (lexer_->peek().type) {
  case lexer::TokenType::Return: {
    lexer_->consume();

    if (lexer_->peekT(lexer::TokenType::Semi)) {
      lexer_->consume();

      return std::make_unique<ast::Return>(nullptr);

    } else {

      std::unique_ptr<ast::ExpressionNode> returnValue = parseExpression(lexer::TokenType::Semi);
      expect(lexer::TokenType::Semi);

      return std::make_unique<ast::Return>(std::move(returnValue));
    }
  }
  case lexer::TokenType::If: {

    lexer_->consume();

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

      falseBody = std::vector<std::unique_ptr<ast::StatementNode>>();

      falseBody = std::vector<std::unique_ptr<ast::StatementNode>>();
      while (!lexer_->peekT(lexer::TokenType::RBrace)) {
        falseBody->emplace_back(parseStatement());
      }

      expect(lexer::TokenType::RBrace);
    }

    return std::make_unique<ast::If>(std::move(condition), std::move(trueBody), std::move(falseBody));
  }
  case lexer::TokenType::While: {

    lexer_->consume();

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

    return std::make_unique<ast::While>(std::move(condition), std::move(body));
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

    return std::make_unique<ast::VariableDeclaration>(type, std::move(name), std::move(initialValue));
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

      if (!functionReturnType)
        emitSemanticError("Call to undefined function '" + name + "'");

      // check if this is a member function call without an instance
      if (!currentClassName_.empty() && name.find("_") != std::string::npos) {
        std::string prefix = currentClassName_ + "_";
        if (name.find(prefix) == 0)
          emitSemanticError("Cannot call member function '" + name + "' without an instance of the class");
      }

      auto call = std::make_unique<ast::FunctionCall>(name, std::move(functionArgs), functionReturnType->isSigned());
      return std::make_unique<ast::ExpressionStatement>(std::move(call));
    }

    // parse lvalue
    auto [target, derivedType] = parseValue();

    // check if this is a method call statement
    if (dynamic_cast<ast::FunctionCall *>(target.get())) {
      expect(lexer::TokenType::Semi);
      return std::make_unique<ast::ExpressionStatement>(std::move(target));
    }

    expect(lexer::TokenType::Equals);

    std::unique_ptr<ast::ExpressionNode> newValue = parseExpression(lexer::TokenType::Semi);

    expect(lexer::TokenType::Semi);

    return std::make_unique<ast::AssignmentStatement>(std::move(target), std::move(newValue));
  }
}
} // namespace axen::parser
