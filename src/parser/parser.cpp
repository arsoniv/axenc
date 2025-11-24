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

#include "lexer.hpp"
#include "parser.hpp"

namespace axen::parser {

void Parser::parse() {
  lexer_ = std::make_shared<lexer::Lexer>(sourceCode_);
  currentFileName_ = rootFilePath_;

  if (!rootFilePath_.empty()) {
    // the file might not exist on disk (lsp)
    try {
      if (std::filesystem::exists(rootFilePath_)) {
        importedFiles_.insert(std::filesystem::canonical(rootFilePath_).string());
      } else {
        // file doesn't exist, use path as-is
        importedFiles_.insert(rootFilePath_);
      }
    } catch (const std::filesystem::filesystem_error &) {
      // if canonical fails, use path as-is
      importedFiles_.insert(rootFilePath_);
    }
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
        emitSyntaxError("Cannot import nonexistent file: '" + importFile + "'");

      std::string canonicalPath = std::filesystem::canonical(importPath).string();

      if (importedFiles_.find(canonicalPath) != importedFiles_.end()) {
        continue;
      }

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
      auto targetType = parseType();

      if (!targetType) {
        emitSyntaxError("Invalid target type in typedef");
      }

      insertTypeDef(alias, targetType);

      expect(lexer::TokenType::Semi);
      break;
    }
    case lexer::TokenType::Intdef: {
      expect(lexer::TokenType::Intdef);
      std::string alias = expect(lexer::TokenType::Identifier).src;
      std::string intStr = expect(lexer::TokenType::IntLit).src;

      // check for 'u' (unsigned) suffix
      bool isSigned = true;
      if (!intStr.empty() && intStr.back() == 'u') {
        isSigned = false;
        intStr.pop_back(); // remove suffix
      }

      int base = (intStr.size() > 2 && intStr[0] == '0' && (intStr[1] == 'x' || intStr[1] == 'X')) ? 16 : 10;
      int targetInt = std::stoi(intStr, nullptr, base);

      insertIntDef(alias, targetInt, isSigned);

      expect(lexer::TokenType::Semi);
      break;
    }

    case lexer::TokenType::Class: {
      // parse class
      auto classToken = lexer_->consume();
      auto classNameToken = expect(lexer::TokenType::Identifier);
      validateIdentifier(classNameToken.src);
      currentClassName_ = classNameToken.src;
      expect(lexer::TokenType::LBrace);
      auto classSpan = makeSpan(classToken);
      parseClass(classSpan);
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

// NOTE: this function only parses class functions (parses within a class)
void Parser::parseFunctions() {

  while (lexer_->peek().type != lexer::TokenType::EndOfFile && lexer_->peek().type != lexer::TokenType::RBrace) {
    switch (lexer_->peek().type) {

    case lexer::TokenType::Typedef:
      lexer_->consume();
      break;

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
