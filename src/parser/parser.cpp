#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <utility>

#include "lexer.hpp"
#include "parser.hpp"

namespace axen::parser {

void Parser::parse() {
  lexer_ = std::make_shared<lexer::Lexer>(sourceCode_);
  currentFileName_ = rootFilePath_;

  if (!rootFilePath_.empty()) {
    importedFiles_.insert(std::filesystem::canonical(rootFilePath_).string());
  }

  processImports();
  parseFile();
}

void Parser::processImports() {
  auto savedLexer = lexer_;
  auto savedFileName = currentFileName_;

  while (!lexer_->peekT(lexer::TokenType::EndOfFile)) {
    if (lexer_->peekT(lexer::TokenType::Import)) {
      lexer_->consume();

      std::string importFile = expect(lexer::TokenType::StringLit).src;
      expect(lexer::TokenType::Semi);

      std::filesystem::path importPath = std::filesystem::path(importFile);

      if (!importPath.is_absolute() && !savedFileName.empty()) {
        std::filesystem::path currentDir = std::filesystem::path(savedFileName).parent_path();
        importPath = currentDir / importPath;
      }

      if (!std::filesystem::exists(importPath))
        emitSemanticError("Cannot import nonexistent file: '" + importFile + "'");

      std::string canonicalPath = std::filesystem::canonical(importPath).string();

      if (importedFiles_.find(canonicalPath) != importedFiles_.end())
        continue;

      importedFiles_.insert(canonicalPath);

      std::ifstream in(importPath);
      std::ostringstream ss;
      ss << in.rdbuf();
      std::string sourceCode = ss.str();

      lexer_ = std::make_shared<lexer::Lexer>(sourceCode);
      currentFileName_ = canonicalPath;

      processImports();
      parseFile();

      lexer_ = savedLexer;
      currentFileName_ = savedFileName;
    } else {
      break;
    }
  }
}

void Parser::parseFile() {

  while (!lexer_->peekT(lexer::TokenType::EndOfFile)) {

    switch (lexer_->peek().type) {
    case lexer::TokenType::Import:
      lexer_->consume();
      expect(lexer::TokenType::StringLit);
      expect(lexer::TokenType::Semi);
      break;
    case lexer::TokenType::Typedef: {
      expect(lexer::TokenType::Typedef);
      std::string alias = expect(lexer::TokenType::Identifier).src;
      std::string targetType = expect(lexer::TokenType::Identifier).src;

      insertTypeDef(alias, std::move(targetType));

      expect(lexer::TokenType::Semi);
      break;
    }
    case lexer::TokenType::Intdef: {
      expect(lexer::TokenType::Intdef);
      std::string alias = expect(lexer::TokenType::Identifier).src;
      std::string intStr = expect(lexer::TokenType::IntLit).src;

      int base = (intStr.size() > 2 && intStr[0] == '0' && (intStr[1] == 'x' || intStr[1] == 'X')) ? 16 : 10;
      int targetInt = std::stoi(intStr, nullptr, base);

      insertIntDef(alias, targetInt);

      expect(lexer::TokenType::Semi);
      break;
    }

    case lexer::TokenType::Class: {
      // parse class
      lexer_->consume();
      auto classNameToken = expect(lexer::TokenType::Identifier);
      validateIdentifier(classNameToken.src);
      currentClassName_ = classNameToken.src;
      expect(lexer::TokenType::LBrace);
      parseClass();
      expect(lexer::TokenType::RBrace);
      currentClassName_.clear();
      break;
    }
    default:
      // parse detached function (top-level function outside any class)
      functions_.push_back(parseFunction());
    }
  }
}

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
      if (lexer_->peekT(lexer::TokenType::Comma))
        lexer_->consume();
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

// NOTE: this function only parses class functions. it only parses within the class
void Parser::parseFunctions() {

  while (lexer_->peek().type != lexer::TokenType::EndOfFile && lexer_->peek().type != lexer::TokenType::RBrace) {
    switch (lexer_->peek().type) {

    case lexer::TokenType::Typedef:
      lexer_->consume();

    default:
      if (lexer_->peekT(lexer::TokenType::LParen, getNextTypeLength() + 1)) {
        functions_.push_back(parseFunction());
        continue;
      }

      // must be a class data member. skip it
      parseType();
      expect(lexer::TokenType::Identifier);
      expect(lexer::TokenType::Semi);
    }
  }
}

} // namespace axen::parser
