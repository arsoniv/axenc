#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "error.hpp"
#include "lexer.hpp"
#include "nodes/class.hpp"
#include "nodes/expression.hpp"
#include "nodes/function.hpp"
#include "nodes/statement.hpp"
#include "nodes/type.hpp"

namespace axen::parser {

class Parser {
public:
  Parser(std::string &&sourceCode, std::string filePath = "")
      : sourceCode_(std::move(sourceCode)), rootFilePath_(std::move(filePath)) {

    registerPrimitiveType("bool", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Bool, false));

    registerPrimitiveType("void", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Void, true));

    registerPrimitiveType("char", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Char, true));
    registerPrimitiveType("uchar", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Char, false));

    registerPrimitiveType("short", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Short, true));
    registerPrimitiveType("ushort", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Short, false));

    registerPrimitiveType("int", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Int, true));
    registerPrimitiveType("uint", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Int, false));

    registerPrimitiveType("long", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Long, true));
    registerPrimitiveType("ulong", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Long, false));

    // fp types are always signed
    registerPrimitiveType("half", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Half, true));
    registerPrimitiveType("float", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Float, true));
    registerPrimitiveType("double", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Double, true));
    registerPrimitiveType("quad", std::make_shared<ast::PrimitiveTypeNode>(ast::PrimitiveType::Quad, true));
  }

  void parse();
  const std::vector<std::unique_ptr<ast::FunctionNode>> *getFunctions() const { return &functions_; }
  std::vector<std::unique_ptr<ast::FunctionNode>> &getFunctionsMut() { return functions_; }
  const std::vector<std::shared_ptr<ast::ClassNode>> *getClasses() const { return &classes_; }
  const std::vector<std::map<std::string, std::shared_ptr<ast::TypeNode>>> &getScopes() const { return scopes; }

  std::shared_ptr<ast::TypeNode> lookupVariableType(const std::string &name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto found = it->find(name);
      if (found != it->end()) {
        return found->second;
      }
    }
    return nullptr;
  }

private:
  void parseClass();
  void parseFile();
  void parseFunctions();
  void processImports();
  std::unique_ptr<ast::FunctionNode> parseFunction();
  std::unique_ptr<ast::ExpressionNode> parseExpression(lexer::TokenType terminator);
  std::unique_ptr<ast::ExpressionNode> parsePrimaryExpression(lexer::TokenType terminator);

  ast::BinaryOperationType tokenToBinaryOp(lexer::TokenType type);
  std::unique_ptr<ast::ExpressionNode> parseBinaryOpRHS(int exprPrec, std::unique_ptr<ast::ExpressionNode> lhs,
                                                        lexer::TokenType terminator);
  std::unique_ptr<ast::StatementNode> parseStatement();
  std::shared_ptr<ast::TypeNode> parseType();
  int getNextTypeLength();
  std::pair<std::unique_ptr<ast::ExpressionNode>, std::shared_ptr<ast::TypeNode>> parseValue();

  inline void emitSyntaxError(const std::string &msg) {
    error::SourceLocation loc(currentFileName_, currentClassName_, lexer_->peek().row, lexer_->peek().col,
                              lexer_->peek().src);
    error::reportError(error::ErrorType::Syntax, msg, &loc);
  }

  inline void emitSemanticError(const std::string &msg) {
    error::SourceLocation loc(currentFileName_, currentClassName_, lexer_->peek().row, lexer_->peek().col,
                              lexer_->peek().src);
    error::reportError(error::ErrorType::Semantic, msg, &loc);
  }

  // parsing utils
  inline lexer::Token expect(lexer::TokenType t) {
    if (lexer_->peek().type != t) {

      std::string expectedString = "";

      if (auto it = lexer::tokenToKeyword.find(t); it != lexer::tokenToKeyword.end())
        expectedString = it->second;
      else if (auto it2 = lexer::tokenToSymbol.find(t); it2 != lexer::tokenToSymbol.end())
        expectedString = it2->second;

      if (t == lexer::TokenType::Identifier) {
        expectedString = "<identifier>";
      }

      error::SourceLocation loc(currentFileName_, currentClassName_, lexer_->peek().row, lexer_->peek().col,
                                lexer_->peek().src);
      error::reportError(error::ErrorType::Syntax, "Expected token: '" + expectedString + "'", &loc);
    }
    return lexer_->consume();
  }

  void validateIdentifier(const std::string &id) {
    if (id.find('-') != std::string::npos) {
      emitSyntaxError("Invalid identifier '" + id + "': hyphens are not allowed in identifiers");
    }
  }

  // variable utils
  void pushScope() { scopes.push_back({}); }
  void popScope() {
    if (!scopes.empty())
      scopes.pop_back();
  }
  void indexVariableType(const std::string &name, std::shared_ptr<ast::TypeNode> &type) { scopes.back()[name] = type; }
  bool variableExistsInCurrentScope(const std::string &name) { return scopes.back().find(name) != scopes.back().end(); }

  // type utils
  void registerPrimitiveType(const std::string &name, std::shared_ptr<ast::PrimitiveTypeNode> type) {
    types_.insert({name, type});
  }
  void registerStructType(const std::string &name, const std::shared_ptr<ast::ClassNode> &structDeclNode) {
    types_.insert({name, std::make_shared<ast::ClassReferenceNode>(structDeclNode)});
  }

  std::shared_ptr<ast::TypeNode> getTypeNode(const std::string &name) const {
    auto it = types_.find(name);
    if (it == types_.end()) {
      return nullptr;
    }
    return it->second;
  }

  std::shared_ptr<ast::TypeNode> lookupFunctionReturnType(const std::string &name) {
    for (auto &func : functions_)
      if (func->getName() == name)
        return func->getReturnType();

    return nullptr;
  }

  std::shared_ptr<ast::FunctionTypeNode> lookupFunctionType(const std::string &name) {
    for (auto &func : functions_) {
      if (func->getName() == name) {
        auto params = func->getParams();
        std::vector<std::shared_ptr<ast::TypeNode>> paramTypes;
        for (const auto &param : params) {
          paramTypes.push_back(param.second);
        }
        return std::make_shared<ast::FunctionTypeNode>(func->getReturnType(), paramTypes);
      }
    }
    return nullptr;
  }

  void insertTypeDef(std::string &alias, std::shared_ptr<ast::TypeNode> targetType) {
    if (!targetType) {
      emitSyntaxError("Invalid target type in typedef");
      return;
    }

    types_.insert({alias, targetType});
  }

  void insertIntDef(std::string &alias, int targetInt, bool isSigned) { intDefs_[alias] = {targetInt, isSigned}; }

  std::string currentClassName_;
  std::string currentFileName_;

  std::string sourceCode_;
  std::string rootFilePath_;
  std::shared_ptr<lexer::Lexer> lexer_;

  std::vector<std::unique_ptr<ast::FunctionNode>> functions_;
  std::vector<std::shared_ptr<ast::ClassNode>> classes_;

  // for variables
  std::vector<std::map<std::string, std::shared_ptr<ast::TypeNode>>> scopes;

  // for types
  std::map<std::string, std::shared_ptr<ast::TypeNode>> types_;

  // for int defs
  std::map<std::string, std::pair<int, bool>> intDefs_; // value, isSigned

  // for tracking imports
  std::set<std::string> importedFiles_;
};
} // namespace axen::parser
