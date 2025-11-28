#pragma once

#include <llvm/IR/BasicBlock.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include "error.hpp"

namespace axen::ast {

class TypeNode;
class FunctionNode;
class ClassNode;

enum class SymbolType {
  Function,
  Class,
  Type,
  Variable,
};

struct AnalysisContext {
  // each scope map element is a variable with its type
  std::vector<std::map<std::string, std::shared_ptr<TypeNode>>> scopes;

  std::map<std::string, FunctionNode *> functions;
  std::map<std::string, std::shared_ptr<ClassNode>> classes;

  // current context
  std::string currentFile;
  std::string currentClass;
  FunctionNode *currentFunction = nullptr;
  int currentRow = 0;
  int currentCol = 0;

  // for lsp completions
  std::map<std::string, SymbolType> contextSymbols;
  int cRow = 0;
  int cCol = 0;

  void addContextSymbol(const std::string &name, SymbolType type) { contextSymbols[name] = type; }

  bool isSymbolAvailableAtCursor(const error::SourceSpan &span) {
    if (span.file != currentFile) {
      return true;
    }

    if (span.endRow < cRow)
      return true;
    if (span.endRow == cRow && span.endCol < cCol)
      return true;

    return false;
  }

  std::vector<error::ErrorInfo> errors;

  void pushScope() { scopes.push_back({}); }

  void popScope() {
    if (!scopes.empty())
      scopes.pop_back();
  }

  void declareVariable(const std::string &name, std::shared_ptr<TypeNode> type) { scopes.back()[name] = type; }

  std::shared_ptr<TypeNode> lookupVariable(const std::string &name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto found = it->find(name);
      if (found != it->end()) {
        return found->second;
      }
    }
    return nullptr;
  }

  bool existsInCurrentScope(const std::string &name) {
    if (scopes.empty())
      return false;
    return scopes.back().find(name) != scopes.back().end();
  }

  void registerFunction(const std::string &name, FunctionNode *func);

  FunctionNode *lookupFunction(const std::string &name) {
    auto it = functions.find(name);
    if (it != functions.end()) {
      return it->second;
    }
    return nullptr;
  }

  void registerClass(const std::string &name, std::shared_ptr<ClassNode> classNode) { classes[name] = classNode; }

  std::shared_ptr<ClassNode> lookupClass(const std::string &name) {
    auto it = classes.find(name);
    if (it != classes.end()) {
      return it->second;
    }
    return nullptr;
  }

  void emitAnalysisError(const std::string &msg, const error::SourceSpan &loc) { errors.push_back({msg, loc}); }
};

struct CodegenContext {
  llvm::LLVMContext llvmContext;
  llvm::IRBuilder<> builder;
  std::unique_ptr<llvm::Module> module;

  // each scope map element is a variable with its AllocaInst and allocated type
  std::vector<std::map<std::string, llvm::AllocaInst *>> scopes;
  std::map<std::string, std::pair<llvm::StructType *, std::vector<std::string>>> namedStructs;
  std::vector<llvm::BasicBlock *> loopExitStack;
  std::vector<llvm::BasicBlock *> loopContinueStack;

  CodegenContext(const std::string &moduleName)
      : builder(llvmContext), module(std::make_unique<llvm::Module>(moduleName, llvmContext)) {}

  void declareStruct(std::string name, llvm::StructType *type, std::vector<std::string> names) {
    namedStructs.emplace(name, std::make_pair(type, names));
  }

  void pushScope() { scopes.push_back({}); }

  void popScope() {
    if (!scopes.empty())
      scopes.pop_back();
  }

  void pushLoopExit(llvm::BasicBlock *exitBlock) { loopExitStack.push_back(exitBlock); }

  void popLoopExit() {
    if (!loopExitStack.empty())
      loopExitStack.pop_back();
  }

  llvm::BasicBlock *getCurrentLoopExit() {
    if (loopExitStack.empty())
      return nullptr;
    return loopExitStack.back();
  }

  void pushLoopContinue(llvm::BasicBlock *continueBlock) { loopContinueStack.push_back(continueBlock); }

  void popLoopContinue() {
    if (!loopContinueStack.empty())
      loopContinueStack.pop_back();
  }

  llvm::BasicBlock *getCurrentLoopContinue() {
    if (loopContinueStack.empty())
      return nullptr;
    return loopContinueStack.back();
  }

  void declareVariable(const std::string &name, llvm::AllocaInst *alloca) { scopes.back()[name] = alloca; }

  llvm::AllocaInst *lookupVariable(const std::string &name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto found = it->find(name);
      if (found != it->end()) {
        return found->second;
      }
    }
    return nullptr;
  }

  /// returns true if types are compatible
  bool checkTypeCompatible(llvm::Type *t1, llvm::Type *t2) {
    if (t1 == t2)
      return true;
    if ((t1->isIntegerTy() && t2->isIntegerTy()) || (t1->isFloatingPointTy() && t2->isFloatingPointTy()))
      return true;
    return false;
  }

  /// converts value to target type if needed
  llvm::Value *convertIfNeeded(llvm::Value *value, llvm::Type *targetType, bool isSigned) {
    if (!value || !targetType)
      return value;

    llvm::Type *valueType = value->getType();

    if (valueType == targetType)
      return value;

    // integer to integer
    if (valueType->isIntegerTy() && targetType->isIntegerTy()) {
      unsigned valueBits = valueType->getIntegerBitWidth();
      unsigned targetBits = targetType->getIntegerBitWidth();

      if (valueBits < targetBits) {
        return isSigned ? builder.CreateSExt(value, targetType, "sext") : builder.CreateZExt(value, targetType, "zext");
      } else if (valueBits > targetBits) {
        return builder.CreateTrunc(value, targetType, "trunc");
      }
    }

    // integer to float
    if (valueType->isIntegerTy() && targetType->isFloatingPointTy()) {
      return isSigned ? builder.CreateSIToFP(value, targetType, "sitofp")
                      : builder.CreateUIToFP(value, targetType, "uitofp");
    }

    // float to integer
    if (valueType->isFloatingPointTy() && targetType->isIntegerTy()) {
      return isSigned ? builder.CreateFPToSI(value, targetType, "fptosi")
                      : builder.CreateFPToUI(value, targetType, "fptoui");
    }

    // float to float
    if (valueType->isFloatingPointTy() && targetType->isFloatingPointTy()) {
      unsigned valueBits = valueType->getPrimitiveSizeInBits();
      unsigned targetBits = targetType->getPrimitiveSizeInBits();

      if (valueBits < targetBits) {
        return builder.CreateFPExt(value, targetType, "fpext");
      } else if (valueBits > targetBits) {
        return builder.CreateFPTrunc(value, targetType, "fptrunc");
      }
    }

    return value;
  }

  bool existsInCurrentScope(const std::string &name) { return scopes.back().find(name) != scopes.back().end(); }

  [[noreturn]] void emitCodegenError(const std::string &msg) { throw std::runtime_error("Codegen Error: " + msg); }

  [[noreturn]] void emitInternalError(const std::string &msg) { throw std::runtime_error("Internal Error: " + msg); }
};

} // namespace axen::ast
