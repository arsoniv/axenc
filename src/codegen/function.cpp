#include <cstdio>
#include <vector>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

#include "nodes/context.hpp"
#include "nodes/function.hpp"

namespace axen::ast {

void FunctionNode::generateFunctionBody(CodegenContext &ctx, llvm::Function *function) {

  llvm::BasicBlock *entry = llvm::BasicBlock::Create(ctx.llvmContext, "entry", function);
  ctx.builder.SetInsertPoint(entry);

  // push new scope for function body
  ctx.pushScope();

  // copy parameters to stack variables to make them mutable
  auto argIt = function->arg_begin();
  for (size_t i = 0; i < params_->size(); ++i, ++argIt) {
    llvm::Argument *arg = &(*argIt);
    arg->setName(params_->at(i).first);

    llvm::AllocaInst *alloca = ctx.builder.CreateAlloca(arg->getType(), nullptr, params_->at(i).first);

    if (!alloca) {
      error::reportError(error::ErrorType::Codegen,
                         "Failed to allocate parameter '" + params_->at(i).first + "' in function '" + name_ + "'");
      ctx.popScope();
      return;
    }

    ctx.builder.CreateStore(arg, alloca);

    ctx.declareVariable(params_->at(i).first, alloca);
  }

  // generate body
  for (auto &bodyNode : *body_) {
    bodyNode->codeGen(ctx);

    // check if statement is a terminator
    llvm::BasicBlock *currentBlock = ctx.builder.GetInsertBlock();
    if (currentBlock->getTerminator())
      break;
  }

  // implicitly return return void if block has no terminator
  if (!ctx.builder.GetInsertBlock()->getTerminator())
    ctx.builder.CreateRetVoid();

  ctx.popScope();
}

llvm::Function *FunctionNode::codeGen(CodegenContext &ctx) {

  llvm::Type *type = type_->codeGen(ctx);

  auto parameters = std::vector<llvm::Type *>();

  for (auto &param : *params_) {
    parameters.emplace_back(param.second->codeGen(ctx));
  }

  auto functionType = llvm::FunctionType::get(type, parameters, false);

  if (!functionType) {
    error::reportError(error::ErrorType::Codegen, "Failed to create function type for '" + name_ + "'");
    return nullptr;
  }

  llvm::Function *function;

  if (isPublic_) {
    function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name_, ctx.module.get());
  } else {
    function = llvm::Function::Create(functionType, llvm::Function::InternalLinkage, name_, ctx.module.get());
  }

  if (!function) {
    error::reportError(error::ErrorType::Codegen, "Failed to create function '" + name_ + "'");
    return nullptr;
  }

  // generate body only if it exists, functions can be bodyless.
  if (body_.has_value()) {
    generateFunctionBody(ctx, function);
  }

  return function;
}

} // namespace axen::ast
