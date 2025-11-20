#include <cstdio>
#include <memory>
#include <vector>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Casting.h>

#include "nodes/context.hpp"
#include "nodes/function.hpp"

namespace axen::ast {

void FunctionNode::generateFunctionBody(CodegenContext &ctx, llvm::Function *function) {

  llvm::BasicBlock *entry = llvm::BasicBlock::Create(ctx.llvmContext, "entry", function);
  ctx.builder.SetInsertPoint(entry);

  // push new scope for function body
  ctx.pushScope();

  // copy parameters to stack variables to make them mutable
  auto params = getParams();
  auto argIt = function->arg_begin();
  for (size_t i = 0; i < params.size(); ++i, ++argIt) {
    llvm::Argument *arg = &(*argIt);
    arg->setName(params.at(i).first);

    llvm::AllocaInst *alloca = ctx.builder.CreateAlloca(arg->getType(), nullptr, params.at(i).first);

    if (!alloca) {
      ctx.popScope();
      ctx.emitCodegenError("Failed to allocate parameter '" + params.at(i).first + "' in function '" + name_ + "'");
    }

    ctx.builder.CreateStore(arg, alloca);

    ctx.declareVariable(params.at(i).first, alloca);
  }

  // generate body
  for (auto &bodyNode : *body_) {
    bodyNode->codeGen(ctx);

    // check if statement is a terminator
    llvm::BasicBlock *currentBlock = ctx.builder.GetInsertBlock();
    if (currentBlock->getTerminator()) {
      break;
    }
  }

  // implicitly return return void if block has no terminator
  if (!ctx.builder.GetInsertBlock()->getTerminator()) {
    ctx.builder.CreateRetVoid();
  }

  ctx.popScope();
}

llvm::Function *FunctionNode::codeGen(CodegenContext &ctx) {

  llvm::Type *type = type_->codeGen(ctx);

  auto functionType = llvm::dyn_cast<llvm::FunctionType>(type);

  if (!functionType) {
    ctx.emitCodegenError("Failed to create function type for '" + name_ + "'");
  }

  llvm::Function *function;

  function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name_, ctx.module.get());

  if (!function) {
    ctx.emitCodegenError("Failed to create function '" + name_ + "'");
  }

  // generate body only if it exists, functions can be bodyless.
  if (body_.has_value()) {
    generateFunctionBody(ctx, function);
  }

  return function;
}

} // namespace axen::ast
