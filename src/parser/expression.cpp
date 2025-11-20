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
  case lexer::TokenType::Percent:
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
  case lexer::TokenType::Percent:
    return ast::BinaryOperationType::Modulo;
  case lexer::TokenType::Less:
    return ast::BinaryOperationType::Less;
  case lexer::TokenType::Greater:
    return ast::BinaryOperationType::More;
  case lexer::TokenType::Equals:
    return ast::BinaryOperationType::Equal;
  default:
    emitSyntaxError("Invalid binary operator");
    return ast::BinaryOperationType::Add; // unreachable
  }
}

std::unique_ptr<ast::ExpressionNode> Parser::parsePrimaryExpression(lexer::TokenType terminator) {
  switch (lexer_->peek().type) {
  case lexer::TokenType::IntLit: {
    auto tok = expect(lexer::TokenType::IntLit);

    // check for 'u' unsigned suffix
    bool isUnsigned = false;
    std::string numStr = tok.src;
    if (!numStr.empty() && numStr.back() == 'u') {
      isUnsigned = true;
      numStr.pop_back(); // remove the 'u' suffix
    }

    int base = (numStr.size() > 2 && numStr[0] == '0' && (numStr[1] == 'x' || numStr[1] == 'X')) ? 16 : 10;
    auto node = std::make_unique<ast::IntLiteral>(std::stoi(numStr, nullptr, base), !isUnsigned);
    node->setLocation(tok.row, tok.col);
    return node;
  }

  case lexer::TokenType::StringLit: {
    auto tok = expect(lexer::TokenType::StringLit);
    auto node = std::make_unique<ast::StringLiteral>(tok.src);
    node->setLocation(tok.row, tok.col);
    return node;
  }

  case lexer::TokenType::FloatLit: {
    auto tok = expect(lexer::TokenType::FloatLit);
    auto node = std::make_unique<ast::FloatLiteral>(std::stof(tok.src));
    node->setLocation(tok.row, tok.col);
    return node;
  }

  case lexer::TokenType::Nullptr: {
    auto tok = expect(lexer::TokenType::Nullptr);
    auto node = std::make_unique<ast::NullptrLiteral>();
    node->setLocation(tok.row, tok.col);
    return node;
  }

  case lexer::TokenType::Minus:
    lexer_->consume();
    if (lexer_->peekT(lexer::TokenType::FloatLit)) {
      return std::make_unique<ast::FloatLiteral>(0 - std::stof(expect(lexer::TokenType::FloatLit).src));
    } else {
      std::string intStr = expect(lexer::TokenType::IntLit).src;

      // check for 'u' (unsigned) suffix
      bool isUnsigned = false;
      if (!intStr.empty() && intStr.back() == 'u') {
        isUnsigned = true;
        intStr.pop_back(); // remove the 'u' suffix
      }

      int base = (intStr.size() > 2 && intStr[0] == '0' && (intStr[1] == 'x' || intStr[1] == 'X')) ? 16 : 10;
      return std::make_unique<ast::IntLiteral>(0 - std::stoi(intStr, nullptr, base), !isUnsigned);
    }

  case lexer::TokenType::Ampersand:
  case lexer::TokenType::Dollar:
  case lexer::TokenType::Identifier: {
    if (lexer_->peekT(lexer::TokenType::LParen, 1)) {
      // function call or function pointer call

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

      // check for function pointer variable
      auto varType = Parser::lookupVariableType(name);
      if (varType) {
        // could be a function pointer
        auto varRef = std::make_unique<ast::VariableReference>(name, varType);
        varRef->setLocation(nameToken.row, nameToken.col);

        auto funcRef = std::make_unique<ast::FunctionReference>(std::move(varRef), varType);
        funcRef->setLocation(nameToken.row, nameToken.col);

        auto node = std::make_unique<ast::FunctionCall>(std::move(funcRef), std::move(functionArgs), varType);
        node->setLocation(nameToken.row, nameToken.col);
        return node;
      } else {
        // normal function
        auto functionReturnType = Parser::lookupFunctionReturnType(name);
        auto type = functionReturnType ? functionReturnType
                                       : std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Void, false);

        auto funcRef = std::make_unique<ast::FunctionReference>(name, Parser::lookupFunctionType(name));
        funcRef->setLocation(nameToken.row, nameToken.col);

        auto node = std::make_unique<ast::FunctionCall>(std::move(funcRef), std::move(functionArgs), type);
        node->setLocation(nameToken.row, nameToken.col);
        return node;
      }
    } else {
      if (lexer_->peekT(lexer::TokenType::Identifier)) {
        if (intDefs_.contains(lexer_->peek().src)) {
          auto token = expect(lexer::TokenType::Identifier);
          auto [value, isSigned] = intDefs_.at(token.src);
          return std::make_unique<ast::IntLiteral>(value, isSigned);
        }
      }

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
    if (type == terminator) {
      return true;
    }
    if (terminator == lexer::TokenType::Comma && type == lexer::TokenType::RParen) {
      return true;
    }
    return false;
  };

  while (!isTerminator()) {
    auto tokType = lexer_->peek().type;

    if (tokType == lexer::TokenType::Equals && !lexer_->peekT(lexer::TokenType::Equals, 1)) {
      emitSyntaxError("Variable assignment is not an expression, did you mean '=='?");
    }

    int tokPrec = getOperatorPrecedence(tokType);
    if (tokPrec < exprPrec) {
      return lhs;
    }

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

    auto binop = std::make_unique<ast::BinaryOperation>(tokenToBinaryOp(tokType), std::move(lhs), std::move(rhs),
                                                        lhs->getType());
    binop->setLocation(lexer_->peek().row, lexer_->peek().col);
    lhs = std::move(binop);
  }

  return lhs;
}

std::unique_ptr<ast::ExpressionNode> Parser::parseExpression(lexer::TokenType terminator) {
  auto lhs = parsePrimaryExpression(terminator);
  return parseBinaryOpRHS(0, std::move(lhs), terminator);
}

} // namespace axen::parser
