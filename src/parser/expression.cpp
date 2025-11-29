#include <memory>
#include <string>
#include <utility>

#include <llvm/Support/Casting.h>

#include "lexer.hpp"
#include "nodes/expression.hpp"
#include "parser.hpp"

namespace axen::parser {

static int getOperatorPrecedence(ast::BinaryOperationType type) {
  switch (type) {
  case ast::BinaryOperationType::Multiply:
  case ast::BinaryOperationType::Divide:
  case ast::BinaryOperationType::Modulo:
    return 4;
  case ast::BinaryOperationType::Add:
  case ast::BinaryOperationType::Subtract:
    return 3;
  case ast::BinaryOperationType::Less:
  case ast::BinaryOperationType::More:
  case ast::BinaryOperationType::Equal:
  case ast::BinaryOperationType::NotEqual:
  case ast::BinaryOperationType::LessEqual:
  case ast::BinaryOperationType::MoreEqual:
    return 2;
  case ast::BinaryOperationType::And:
    return 1;
  case ast::BinaryOperationType::Or:
    return 0;
  default:
    return -1;
  }
}

ast::BinaryOperationType Parser::parseBinaryOp() {
  switch (lexer_->peek().type) {
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
    switch (lexer_->peek(1).type) {
    case lexer::TokenType::Equals:
      return ast::BinaryOperationType::LessEqual;
    case lexer::TokenType::Exclamation:
      return ast::BinaryOperationType::MoreEqual;
    default:
      return ast::BinaryOperationType::Less;
    }
  case lexer::TokenType::Greater:
    switch (lexer_->peek(1).type) {
    case lexer::TokenType::Equals:
      return ast::BinaryOperationType::MoreEqual;
    case lexer::TokenType::Exclamation:
      return ast::BinaryOperationType::LessEqual;
    default:
      return ast::BinaryOperationType::More;
    }
  case lexer::TokenType::Equals:
    switch (lexer_->peek(1).type) {
    case lexer::TokenType::Equals:
      return ast::BinaryOperationType::Equal;
    case lexer::TokenType::Exclamation:
      return ast::BinaryOperationType::NotEqual;
    case lexer::TokenType::Less:
      return ast::BinaryOperationType::LessEqual;
    case lexer::TokenType::Greater:
      return ast::BinaryOperationType::MoreEqual;
    default:
      break;
    }
  case lexer::TokenType::Exclamation:
    switch (lexer_->peek(1).type) {
    case lexer::TokenType::Equals:
      return ast::BinaryOperationType::NotEqual;
    case lexer::TokenType::Less:
      return ast::BinaryOperationType::MoreEqual;
    case lexer::TokenType::Greater:
      return ast::BinaryOperationType::LessEqual;
    default:
      return ast::BinaryOperationType::Not;
      break;
    }
  case lexer::TokenType::Ampersand:
    switch (lexer_->peek(1).type) {
    case lexer::TokenType::Ampersand:
      return ast::BinaryOperationType::And;
    default:
      break;
    }
  case lexer::TokenType::Pipe:
    switch (lexer_->peek(1).type) {
    case lexer::TokenType::Pipe:
      return ast::BinaryOperationType::Or;
    default:
      break;
    }
  default:
    break;
  }
  emitSyntaxError("Invalid binary operator");
  return ast::BinaryOperationType::Add; // unreachable
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
    return std::make_unique<ast::IntLiteral>(std::stoi(numStr, nullptr, base), !isUnsigned, makeSpan(tok));
  }

  case lexer::TokenType::StringLit: {
    auto tok = expect(lexer::TokenType::StringLit);
    return std::make_unique<ast::StringLiteral>(tok.src, makeSpan(tok));
  }

  case lexer::TokenType::FloatLit: {
    auto tok = expect(lexer::TokenType::FloatLit);
    return std::make_unique<ast::FloatLiteral>(std::stof(tok.src), makeSpan(tok));
  }

  case lexer::TokenType::Nullptr: {
    auto tok = expect(lexer::TokenType::Nullptr);
    return std::make_unique<ast::NullptrLiteral>(makeSpan(tok));
  }

  case lexer::TokenType::SizeOf: {
    auto tok = expect(lexer::TokenType::SizeOf);
    return std::make_unique<ast::SizeOf>(parseType(), makeSpan(tok));
  }

  case lexer::TokenType::Minus:
    lexer_->consume();
    if (lexer_->peekT(lexer::TokenType::FloatLit)) {
      auto tok = expect(lexer::TokenType::FloatLit);
      return std::make_unique<ast::FloatLiteral>(0 - std::stof(tok.src), makeSpan(tok));
    } else {
      auto tok = expect(lexer::TokenType::IntLit);
      std::string intStr = tok.src;

      // check for 'u' (unsigned) suffix
      bool isUnsigned = false;
      if (!intStr.empty() && intStr.back() == 'u') {
        isUnsigned = true;
        intStr.pop_back(); // remove the 'u' suffix
      }

      int base = (intStr.size() > 2 && intStr[0] == '0' && (intStr[1] == 'x' || intStr[1] == 'X')) ? 16 : 10;
      return std::make_unique<ast::IntLiteral>(0 - std::stoi(intStr, nullptr, base), !isUnsigned, makeSpan(tok));
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
      while (!lexer_->peekT(lexer::TokenType::RParen) && !lexer_->peekT(lexer::TokenType::EndOfFile)) {
        auto newExpression = parseExpression(lexer::TokenType::Comma);
        if (!newExpression) {
          emitSyntaxError("Invalid expression in function call arguments");
          break;
        }
        functionArgs.emplace_back(std::move(newExpression));
        if (lexer_->peek().type == lexer::TokenType::Comma) {
          lexer_->consume();
        }
      }
      expect(lexer::TokenType::RParen);

      // check for function pointer variable
      auto varType = Parser::lookupVariableType(name);
      auto span = makeSpan(nameToken);
      if (varType) {
        // could be a function pointer
        auto varRef = std::make_unique<ast::VariableReference>(name, varType, span);
        auto funcRef = std::make_unique<ast::FunctionReference>(std::move(varRef), varType, span);
        return std::make_unique<ast::FunctionCall>(std::move(funcRef), std::move(functionArgs), varType, span);
      } else {
        // normal function
        auto functionReturnType = Parser::lookupFunctionReturnType(name);
        auto type = functionReturnType
                        ? functionReturnType
                        : std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Void, false, span);

        auto funcRef = std::make_unique<ast::FunctionReference>(name, Parser::lookupFunctionType(name), span);
        return std::make_unique<ast::FunctionCall>(std::move(funcRef), std::move(functionArgs), type, span);
      }
    } else {
      if (lexer_->peekT(lexer::TokenType::Identifier)) {
        if (intDefs_.contains(lexer_->peek().src)) {
          auto token = expect(lexer::TokenType::Identifier);
          auto [value, isSigned] = intDefs_.at(token.src);
          return std::make_unique<ast::IntLiteral>(value, isSigned, makeSpan(token));
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
    lexer_->consume();
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

  while (!isTerminator() && !lexer_->peekT(lexer::TokenType::EndOfFile)) {
    auto tokType = lexer_->peek().type;

    ast::BinaryOperationType type = parseBinaryOp();

    int tokPrec = getOperatorPrecedence(type);
    if (tokPrec < exprPrec) {
      return lhs;
    }

    switch (type) {
    case ast::BinaryOperationType::And:
    case ast::BinaryOperationType::Equal:
    case ast::BinaryOperationType::NotEqual:
    case ast::BinaryOperationType::LessEqual:
    case ast::BinaryOperationType::MoreEqual:
    case ast::BinaryOperationType::Or:
      lexer_->consume();
    case ast::BinaryOperationType::Add:
    case ast::BinaryOperationType::Subtract:
    case ast::BinaryOperationType::Multiply:
    case ast::BinaryOperationType::Divide:
    case ast::BinaryOperationType::Modulo:
    case ast::BinaryOperationType::Less:
    case ast::BinaryOperationType::More:
    case ast::BinaryOperationType::Not:
      lexer_->consume();
    }

    auto rhs = parsePrimaryExpression(terminator);
    if (!rhs) {
      return nullptr;
    }

    auto tok = lexer_->peek();
    lhs = std::make_unique<ast::BinaryOperation>(type, std::move(lhs), std::move(rhs), lhs->getType(), makeSpan(tok));
  }

  return lhs;
}

std::unique_ptr<ast::ExpressionNode> Parser::parseExpression(lexer::TokenType terminator) {
  auto lhs = parsePrimaryExpression(terminator);
  if (!lhs) {
    return nullptr;
  }
  return parseBinaryOpRHS(0, std::move(lhs), terminator);
}

} // namespace axen::parser
