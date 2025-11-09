#pragma once

#include <map>
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

  /// returns true if types are compatible, types may still need resizing.
  bool checkTypeCompatible(llvm::Type *t1, llvm::Type *t2) { return t1 == t2; }

  /// converts value to target type if needed
  llvm::Value *convertIfNeeded(llvm::Value *value, llvm::Type *targetType, bool isSigned) {
    if (!value || !targetType)
      return value;

    llvm::Type *valueType = value->getType();

    if (valueType == targetType)
      return value;

    if (valueType->isIntegerTy() && targetType->isIntegerTy()) {
      unsigned valueBits = valueType->getIntegerBitWidth();
      unsigned targetBits = targetType->getIntegerBitWidth();

      if (valueBits < targetBits) {
        return isSigned ? builder.CreateSExt(value, targetType, "sext") : builder.CreateZExt(value, targetType, "zext");
      } else if (valueBits > targetBits) {
        return builder.CreateTrunc(value, targetType, "trunc");
      }
    }

    return value;
  }

  bool existsInCurrentScope(const std::string &name) { return scopes.back().find(name) != scopes.back().end(); }
};

} // namespace axen::ast
