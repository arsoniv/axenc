#pragma once

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

// symbol information for LSP
struct SymbolInfo {
  enum class Kind { Variable, Function, Parameter, Member, Class, Type };

  std::string name;
  Kind kind;
  std::shared_ptr<TypeNode> type;
  int scopeLevel;

  std::string fileName;
  int line;
  int column;

  SymbolInfo(std::string n, Kind k, std::shared_ptr<TypeNode> t = nullptr, int scope = 0, std::string file = "",
             int l = 0, int c = 0)
      : name(std::move(n)), kind(k), type(t), scopeLevel(scope), fileName(std::move(file)), line(l), column(c) {}
};

struct AnalysisContext {
  // each scope map element is a variable with its type
  std::vector<std::map<std::string, std::shared_ptr<TypeNode>>> scopes;

  std::map<std::string, FunctionNode *> functions;
  std::map<std::string, std::shared_ptr<ClassNode>> classes;
  std::map<std::string, std::shared_ptr<TypeNode>> typeAliases;

  // current context
  std::string currentFile;
  std::string currentClass;
  FunctionNode *currentFunction = nullptr;
  int currentRow = 0;
  int currentCol = 0;

  struct ErrorInfo {
    std::string message;
    std::string location;
    int row;
    int col;
  };
  std::vector<ErrorInfo> errors;
  bool collectErrors = false;

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

  void registerTypeAlias(const std::string &alias, std::shared_ptr<TypeNode> type) { typeAliases[alias] = type; }

  std::shared_ptr<TypeNode> lookupTypeAlias(const std::string &alias) {
    auto it = typeAliases.find(alias);
    if (it != typeAliases.end()) {
      return it->second;
    }
    return nullptr;
  }

  void emitAnalysisError(const std::string &msg) {
    if (collectErrors) {
      std::string location = currentFile;
      if (!currentClass.empty()) {
        location += "::" + currentClass;
      }
      errors.push_back({msg, location, currentRow, currentCol});
      // continue collecting errors, don't exit
      return;
    }
    error::reportError(error::ErrorType::Semantic, msg);
  }

  void reportError(const std::string &msg) {
    if (collectErrors) {
      std::string location = currentFile;
      if (!currentClass.empty()) {
        location += "::" + currentClass;
      }
      errors.push_back({msg, location, currentRow, currentCol});
      // continue collecting errors, don't exit
      return;
    }
    error::reportError(error::ErrorType::Semantic, msg);
  }
};

struct CodegenContext {
  llvm::LLVMContext llvmContext;
  llvm::IRBuilder<> builder;
  std::unique_ptr<llvm::Module> module;

  // each scope map element is a variable with its AllocaInst and allocated type
  std::vector<std::map<std::string, llvm::AllocaInst *>> scopes;
  std::map<std::string, std::pair<llvm::StructType *, std::vector<std::string>>> namedStructs;

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

  [[noreturn]] void emitCodegenError(const std::string &msg) { error::reportError(error::ErrorType::Codegen, msg); }

  [[noreturn]] void emitInternalError(const std::string &msg) { error::reportError(error::ErrorType::Internal, msg); }
};

} // namespace axen::ast
