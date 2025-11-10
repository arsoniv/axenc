#include <memory>
#include <string>
#include <utility>

#include <llvm/Support/Casting.h>

#include "lexer.hpp"
#include "nodes/expression.hpp"
#include "parser.hpp"

namespace axen::parser {

static int getOperatorPrecedence(lexer::TokenType type) {
  switch (type) {
  case lexer::TokenType::Asterisk:
  case lexer::TokenType::Slash:
    return 20;
  case lexer::TokenType::Plus:
  case lexer::TokenType::Minus:
    return 10;
  case lexer::TokenType::Less:
  case lexer::TokenType::Greater:
    return 5;
  case lexer::TokenType::Equals:
    return 3;
  default:
    return -1;
  }
}

ast::BinaryOperationType Parser::tokenToBinaryOp(lexer::TokenType type) {
  switch (type) {
  case lexer::TokenType::Plus:
    return ast::BinaryOperationType::Add;
  case lexer::TokenType::Minus:
    return ast::BinaryOperationType::Subtract;
  case lexer::TokenType::Asterisk:
    return ast::BinaryOperationType::Multiply;
  case lexer::TokenType::Slash:
    return ast::BinaryOperationType::Divide;
  case lexer::TokenType::Less:
    return ast::BinaryOperationType::Less;
  case lexer::TokenType::Greater:
    return ast::BinaryOperationType::More;
  case lexer::TokenType::Equals:
    return ast::BinaryOperationType::Equal;
  default:
    emitSemanticError("Invalid binary operator");
    return ast::BinaryOperationType::Add; // unreachable
  }
}

std::unique_ptr<ast::ExpressionNode> Parser::parsePrimaryExpression(lexer::TokenType terminator) {
  switch (lexer_->peek().type) {
  case lexer::TokenType::IntLit: {
    std::string intStr = expect(lexer::TokenType::IntLit).src;
    int base = (intStr.size() > 2 && intStr[0] == '0' && (intStr[1] == 'x' || intStr[1] == 'X')) ? 16 : 10;
    return std::make_unique<ast::IntLiteral>(std::stoi(intStr, nullptr, base));
  }

  case lexer::TokenType::StringLit:
    return std::make_unique<ast::StringLiteral>(expect(lexer::TokenType::StringLit).src);

  case lexer::TokenType::FloatLit:
    return std::make_unique<ast::FloatLiteral>(std::stof(expect(lexer::TokenType::FloatLit).src));

  case lexer::TokenType::Minus:
    lexer_->consume();
    if (lexer_->peekT(lexer::TokenType::FloatLit)) {
      return std::make_unique<ast::FloatLiteral>(0 - std::stof(expect(lexer::TokenType::FloatLit).src));
    } else {
      std::string intStr = expect(lexer::TokenType::IntLit).src;
      int base = (intStr.size() > 2 && intStr[0] == '0' && (intStr[1] == 'x' || intStr[1] == 'X')) ? 16 : 10;
      return std::make_unique<ast::IntLiteral>(0 - std::stoi(intStr, nullptr, base));
    }

  case lexer::TokenType::Ampersand:
  case lexer::TokenType::Dollar:
  case lexer::TokenType::Identifier: {
    if (lexer_->peekT(lexer::TokenType::LParen, 1)) {
      // function call

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

      auto functionReturnType = Parser::lookupFunctionReturnType(name);

      if (!functionReturnType)
        emitSemanticError("Call to undefined function '" + name + "'");

      // non-detatched member function calls must be made using a instance
      if (!currentClassName_.empty() && name.find("_") != std::string::npos) {
        std::string prefix = currentClassName_ + "_";
        if (name.find(prefix) == 0)
          emitSemanticError("Cannot call member function '" + name + "' without an instance of the class");
      }

      return std::make_unique<ast::FunctionCall>(name, std::move(functionArgs), functionReturnType->isSigned());
    } else {
      if (lexer_->peekT(lexer::TokenType::Identifier))
        if (intDefs_.contains(lexer_->peek().src))
          return std::make_unique<ast::IntLiteral>(intDefs_.at(expect(lexer::TokenType::Identifier).src));

      return parseValue().first;
    }
  }

  case axen::lexer::TokenType::LParen: {
    expect(lexer::TokenType::LParen);
    auto expr = parseExpression(lexer::TokenType::RParen);
    expect(lexer::TokenType::RParen);
    return expr;
  }

  default:
    emitSyntaxError("Unexpected token in expression");
    return nullptr; // unreachable
  }
}

std::unique_ptr<ast::ExpressionNode> Parser::parseBinaryOpRHS(int exprPrec, std::unique_ptr<ast::ExpressionNode> lhs,
                                                              lexer::TokenType terminator) {
  auto isTerminator = [&]() {
    auto type = lexer_->peek().type;
    if (type == terminator)
      return true;
    if (terminator == lexer::TokenType::Comma && type == lexer::TokenType::RParen)
      return true;
    return false;
  };

  while (!isTerminator()) {
    auto tokType = lexer_->peek().type;

    if (tokType == lexer::TokenType::Equals && !lexer_->peekT(lexer::TokenType::Equals, 1)) {
      emitSemanticError("Variable assignment is not an expression, did you mean '=='?");
    }

    int tokPrec = getOperatorPrecedence(tokType);
    if (tokPrec < exprPrec)
      return lhs;

    if (tokType == lexer::TokenType::Equals) {
      lexer_->consume();
      lexer_->consume();
    } else {
      lexer_->consume();
    }

    auto rhs = parsePrimaryExpression(terminator);

    auto nextTokType = lexer_->peek().type;
    if (!isTerminator()) {
      if (nextTokType == lexer::TokenType::Equals && !lexer_->peekT(lexer::TokenType::Equals, 1)) {
      } else {
        int nextPrec = getOperatorPrecedence(nextTokType);
        if (nextPrec > tokPrec) {
          rhs = parseBinaryOpRHS(tokPrec + 1, std::move(rhs), terminator);
        }
      }
    }

    if (lhs->isSigned() != rhs->isSigned()) {
      emitSemanticError("Cannot create binary operation with types of different signedness");
    }

    lhs = std::make_unique<ast::BinaryOperation>(tokenToBinaryOp(tokType), std::move(lhs), std::move(rhs),
                                                 lhs->isSigned());
  }

  return lhs;
}

std::unique_ptr<ast::ExpressionNode> Parser::parseExpression(lexer::TokenType terminator) {
  auto lhs = parsePrimaryExpression(terminator);
  return parseBinaryOpRHS(0, std::move(lhs), terminator);
}

} // namespace axen::parser
