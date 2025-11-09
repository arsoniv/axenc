#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include "nodes/context.hpp"
#include "nodes/statement.hpp"
#include "nodes/types.hpp"

namespace axen::ast {

class FunctionNode {
public:
  FunctionNode(std::string name, std::shared_ptr<TypeNode> &&type, bool isPublic,
               std::optional<std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>>> &&params,
               std::optional<std::vector<std::unique_ptr<StatementNode>>> &&body, bool isDetached)
      : name_(std::move(name)), type_(type), params_(params), body_(std::move(body)), isPublic_(isPublic),
        isDetached_(isDetached) {}

  llvm::Function *codeGen(CodegenContext &ctx);
  void generateFunctionBody(CodegenContext &ctx, llvm::Function *function);

  std::string getName() { return name_; }

  std::shared_ptr<TypeNode> getReturnType() { return type_; }

private:
  std::string name_;
  std::shared_ptr<TypeNode> type_;
  bool isPublic_;
  std::optional<std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>>> params_;
  std::optional<std::vector<std::unique_ptr<StatementNode>>> body_;
  bool isDetached_;
};

} // namespace axen::ast
