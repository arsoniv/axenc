#include <memory>

#include "nodes/expression.hpp"
#include "nodes/types.hpp"
#include "parser.hpp"

namespace axen::parser {

std::pair<std::unique_ptr<ast::ExpressionNode>, std::shared_ptr<ast::TypeNode>> Parser::parseValue() {

  // handle prefix dereferences
  int drefs = 0;
  while (lexer_->peekT(lexer::TokenType::Dollar)) {
    drefs++;
    lexer_->consume();
  }

  // handle address-of operator
  bool addressOf = false;
  if (lexer_->peekT(lexer::TokenType::Ampersand)) {
    addressOf = true;
    lexer_->consume();
  }

  // get lvalue name
  auto nameToken = expect(lexer::TokenType::Identifier);
  validateIdentifier(nameToken.src);
  auto name = nameToken.src;

  // find variable type
  std::shared_ptr<ast::TypeNode> derivedType = Parser::lookupVariableType(name);
  std::unique_ptr<ast::ExpressionNode> target;

  if (derivedType) {
    // must be a local (non-member) variable
    target = std::make_unique<ast::VariableReference>(name, derivedType->isSigned());
  } else {
    // ensure a member function and this is a member variable
    auto thisType = Parser::lookupVariableType("this");
    if (thisType) {
      auto thisPtrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(thisType);
      if (thisPtrType) {
        auto classRefType = std::dynamic_pointer_cast<ast::ClassReferenceNode>(thisPtrType->target());
        if (classRefType) {
          std::shared_ptr<ast::ClassNode> structDecl = classRefType->getDecl();
          auto fieldType = structDecl->lookupMemberType(name);
          if (fieldType) {
            // member variable access via implicit 'this' pointer
            auto thisRef = std::make_unique<ast::VariableReference>("this", thisType->isSigned());
            auto targetType = thisPtrType->target();
            auto derefThis =
                std::make_unique<ast::Dref>(std::move(thisRef), targetType, thisPtrType->target()->isSigned());
            target = std::make_unique<ast::StructAccess>(std::move(derefThis), name, structDecl->getName(),
                                                         fieldType->isSigned(), classRefType);
            derivedType = fieldType;
          }
        }
      }
    }

    if (!derivedType) {
      emitSemanticError("Undefined variable '" + name + "'");
    }
  }

  // apply prefix dereferences
  for (int i = 0; i < drefs; i++) {
    auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);
    if (!ptrType) {
      emitSemanticError("Cannot dereference non-pointer type");
    }
    derivedType = ptrType->target();
    target = std::make_unique<ast::Dref>(std::move(target), derivedType, derivedType->isSigned());
  }

  // handle postfix operations in loop
  while (true) {
    if (lexer_->peekT(lexer::TokenType::Period)) {
      lexer_->consume();

      auto structType = std::dynamic_pointer_cast<ast::ClassReferenceNode>(derivedType);

      // auto dereference if derivedType is a pointer to a struct
      if (!structType) {
        auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);
        if (ptrType) {
          structType = std::dynamic_pointer_cast<ast::ClassReferenceNode>(ptrType->target());
          if (structType) {
            derivedType = ptrType->target();
            target = std::make_unique<ast::Dref>(std::move(target), derivedType, derivedType->isSigned());
          }
        }
      }

      if (!structType)
        emitSemanticError("Cannot access member of non-struct type");

      // handle postfix dereferences on member
      int memDrefs = 0;
      while (lexer_->peekT(lexer::TokenType::Dollar)) {
        memDrefs++;
        lexer_->consume();
      }

      auto fieldToken = expect(lexer::TokenType::Identifier);
      validateIdentifier(fieldToken.src);
      auto fieldName = fieldToken.src;

      // check for member method call
      if (lexer_->peekT(lexer::TokenType::LParen)) {
        std::shared_ptr<ast::ClassNode> structDecl = structType->getDecl();
        std::string methodName = structDecl->getName() + "_" + fieldName;

        lexer_->consume(); // consume lParen

        auto functionArgs = std::vector<std::unique_ptr<ast::ExpressionNode>>();

        // First argument is 'this' - take address of the target
        auto thisArg = std::make_unique<ast::AddressOf>(std::move(target), derivedType->isSigned());
        functionArgs.push_back(std::move(thisArg));

        // Parse remaining arguments
        while (lexer_->peek().type != lexer::TokenType::RParen) {
          functionArgs.emplace_back(parseExpression(lexer::TokenType::Comma));
          if (lexer_->peek().type == lexer::TokenType::Comma) {
            lexer_->consume();
          }
        }
        lexer_->consume(); // consume ')'

        auto functionReturnType = Parser::lookupFunctionReturnType(methodName);

        if (!functionReturnType) {
          emitSemanticError("Call to undefined member method '" + methodName + "'");
        }

        auto call =
            std::make_unique<ast::FunctionCall>(methodName, std::move(functionArgs), functionReturnType->isSigned());
        return {std::move(call), functionReturnType};
      }

      const std::string &structName = structType->name();
      std::shared_ptr<ast::ClassNode> structDecl = structType->getDecl();

      auto fieldType = structDecl->lookupMemberType(fieldName);

      if (!fieldType) {
        emitSemanticError("Struct '" + structName + "' has no member '" + fieldName + "'");
      }

      target = std::make_unique<ast::StructAccess>(std::move(target), fieldName, structName, fieldType->isSigned(),
                                                   structType);
      derivedType = fieldType;

      // apply member dereferences
      for (int i = 0; i < memDrefs; i++) {
        auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);
        if (!ptrType)
          emitSemanticError("Cannot dereference non-pointer type");

        derivedType = ptrType->target();
        target = std::make_unique<ast::Dref>(std::move(target), derivedType, derivedType->isSigned());
      }

    } else if (lexer_->peekT(lexer::TokenType::LBracket)) {
      // handle postfix dereferences before lBracket
      int memDrefs = 0;
      while (lexer_->peekT(lexer::TokenType::Dollar)) {
        memDrefs++;
        lexer_->consume();
      }

      lexer_->consume(); // consume lBracket

      auto arrayType = std::dynamic_pointer_cast<ast::ArrayTypeNode>(derivedType);
      auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);

      if (!arrayType && !ptrType)
        emitSemanticError("Cannot apply subscript operator to non-array/non-pointer type");

      std::unique_ptr<ast::ExpressionNode> indexExpression = parseExpression(lexer::TokenType::RBracket);
      expect(lexer::TokenType::RBracket);

      if (arrayType) {
        target = std::make_unique<ast::ArrayAccess>(std::move(target), std::move(indexExpression),
                                                    arrayType->isSigned(), arrayType);
        derivedType = arrayType->target();
      } else {
        target = std::make_unique<ast::PtrIndexAccess>(std::move(target), std::move(indexExpression),
                                                       ptrType->isSigned(), ptrType);
        derivedType = ptrType->target();
      }

      // apply postfix dereferences
      for (int i = 0; i < memDrefs; i++) {
        auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);
        if (!ptrType)
          emitSemanticError("Cannot dereference non-pointer type");

        derivedType = ptrType->target();
        target = std::make_unique<ast::Dref>(std::move(target), derivedType, derivedType->isSigned());
      }
    } else {
      break;
    }
  }

  // apply address-of operator
  if (addressOf) {
    target = std::make_unique<ast::AddressOf>(std::move(target), derivedType->isSigned());
  }

  return {std::move(target), derivedType};
}

} // namespace axen::parser
