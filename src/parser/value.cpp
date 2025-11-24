#include <memory>

#include "nodes/expression.hpp"
#include "nodes/type.hpp"
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

  auto span = makeSpan(nameToken);
  if (derivedType) {
    // local variable
    target = std::make_unique<ast::VariableReference>(name, derivedType, span);
  } else if (addressOf) {
    // check if this is a function reference
    auto funcReturnType = Parser::lookupFunctionReturnType(name);
    if (funcReturnType) {
      // this is a function, create a function reference
      auto funcType = Parser::lookupFunctionType(name);
      derivedType = funcType;
      target = std::make_unique<ast::FunctionReference>(name, funcType, span);
    }
  }

  if (!target) {
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
            auto thisRef = std::make_unique<ast::VariableReference>("this", thisType, span);
            auto targetType = thisPtrType->target();
            auto derefThis = std::make_unique<ast::Dref>(std::move(thisRef), targetType, span);
            target = std::make_unique<ast::StructAccess>(std::move(derefThis), name, structDecl->getName(),
                                                         classRefType, fieldType, span);
            derivedType = fieldType;
          }
        }
      }
    }

    // error will be caught in analysis
    if (!derivedType) {
      target = std::make_unique<ast::VariableReference>(name, derivedType, span);
    }
  }

  // apply prefix dereferences
  for (int i = 0; i < drefs; i++) {
    auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);
    if (ptrType) {
      derivedType = ptrType->target();
    }

    // if not pointer type, error will be caught in analysis
    target = std::make_unique<ast::Dref>(std::move(target), derivedType, span);
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
            auto derefSpan = makeSpan(lexer_->peek());
            target = std::make_unique<ast::Dref>(std::move(target), derivedType, derefSpan);
          }
        }
      }

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
        lexer_->consume(); // consume lParen

        auto functionArgs = std::vector<std::unique_ptr<ast::ExpressionNode>>();

        // use the derivedType for 'this' argument
        auto fieldSpan = makeSpan(fieldToken);
        auto thisArg =
            std::make_unique<ast::AddressOf>(std::move(target), std::make_shared<ast::PointerTypeNode>(derivedType, fieldSpan), fieldSpan);
        functionArgs.push_back(std::move(thisArg));

        // parse remaining arguments
        while (lexer_->peek().type != lexer::TokenType::RParen) {
          functionArgs.emplace_back(parseExpression(lexer::TokenType::Comma));
          if (lexer_->peek().type == lexer::TokenType::Comma) {
            lexer_->consume();
          }
        }
        lexer_->consume(); // consume ')'

        // build method name from struct name if available
        std::string methodName = fieldName;
        if (structType) {
          methodName = structType->getDecl()->getName() + "-" + fieldName;
        }

        auto functionReturnType = Parser::lookupFunctionReturnType(methodName);

        auto funcRef = std::make_unique<ast::FunctionReference>(methodName, Parser::lookupFunctionType(methodName), fieldSpan);

        // functionReturnType may be nullptr, will be caught in analysis
        auto call =
            std::make_unique<ast::FunctionCall>(std::move(funcRef), std::move(functionArgs), functionReturnType, fieldSpan);
        return {std::move(call), functionReturnType};
      }

      // find field type if the struct type is known
      std::shared_ptr<ast::TypeNode> fieldType;
      std::string structName = "";
      if (structType) {
        structName = structType->getDecl()->getName();
        std::shared_ptr<ast::ClassNode> structDecl = structType->getDecl();
        fieldType = structDecl->lookupMemberType(fieldName);
      }

      // create struct access with available info
      auto fieldSpan = makeSpan(fieldToken);
      target = std::make_unique<ast::StructAccess>(std::move(target), fieldName, structName, structType, fieldType, fieldSpan);
      derivedType = fieldType;

      // apply member dereferences
      for (int i = 0; i < memDrefs; i++) {
        auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);

        if (ptrType) {
          derivedType = ptrType->target();
        } else {
          derivedType = nullptr;
        }
        auto derefSpan = makeSpan(lexer_->peek());
        target = std::make_unique<ast::Dref>(std::move(target), derivedType, derefSpan);
      }

    } else {
      if (lexer_->peekT(lexer::TokenType::LBracket)) {
        // handle postfix dereferences before lBracket
        int memDrefs = 0;
        while (lexer_->peekT(lexer::TokenType::Dollar)) {
          memDrefs++;
          lexer_->consume();
        }

        lexer_->consume(); // consume lBracket

        auto arrayType = std::dynamic_pointer_cast<ast::ArrayTypeNode>(derivedType);
        auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);

        std::unique_ptr<ast::ExpressionNode> indexExpression = parseExpression(lexer::TokenType::RBracket);
        auto endBracket = expect(lexer::TokenType::RBracket);
        auto accessSpan = makeSpan(nameToken.row, nameToken.col, endBracket.row, static_cast<int>(endBracket.col + endBracket.src.length()));

        // Create appropriate access node based on type if known
        if (arrayType) {
          target = std::make_unique<ast::ArrayAccess>(std::move(target), std::move(indexExpression), arrayType, accessSpan);
          derivedType = arrayType->target();
        } else if (ptrType) {
          target = std::make_unique<ast::PtrIndexAccess>(std::move(target), std::move(indexExpression), ptrType, accessSpan);
          derivedType = ptrType->target();
        } else {
          // unknown type, use ArrayAccess as default, will be caught in analysis
          target = std::make_unique<ast::ArrayAccess>(std::move(target), std::move(indexExpression), nullptr, accessSpan);
        }

        // apply postfix dereferences
        for (int i = 0; i < memDrefs; i++) {
          auto ptrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(derivedType);

          if (ptrType) {
            derivedType = ptrType->target();
          } else {
            derivedType = nullptr;
          }
          auto derefSpan = makeSpan(lexer_->peek());
          target = std::make_unique<ast::Dref>(std::move(target), derivedType, derefSpan);
        }
      } else {
        break;
      }
    }
  }

  // apply address-of operator
  if (addressOf) {
    // only apply dereference to non function pointer (functions are already pointers)
    auto funcRef = dynamic_cast<ast::FunctionReference *>(target.get());
    if (!funcRef) {
      auto ptrType = std::make_shared<ast::PointerTypeNode>(derivedType, span);
      // ptrType may be nullptr, will be caught in analysis
      target = std::make_unique<ast::AddressOf>(std::move(target), ptrType, span);
    }
  }

  return {std::move(target), derivedType};
}

} // namespace axen::parser
